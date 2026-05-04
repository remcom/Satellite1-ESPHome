#include "i2s_audio_speaker_standard.h"

#ifdef USE_ESP32

#include "esphome/components/audio/audio.h"
#include "esphome/components/audio/audio_transfer_buffer.h"

#include "esphome/core/hal.h"
#include "esphome/core/log.h"

#include "esp_timer.h"

namespace esphome::i2s_audio {

static const char *const TAG = "i2s_audio.speaker.std";

static constexpr size_t DMA_BUFFERS_COUNT = 4;
static constexpr size_t I2S_EVENT_QUEUE_COUNT = DMA_BUFFERS_COUNT + 1;

void I2SAudioSpeaker::dump_config() {
  I2SAudioSpeakerBase::dump_config();
  const char *fmt_str;
  switch (this->i2s_comm_fmt_) {
    case I2SCommFmt::PCM:
      fmt_str = "pcm";
      break;
    case I2SCommFmt::MSB:
      fmt_str = "msb";
      break;
    default:
      fmt_str = "std";
      break;
  }
  ESP_LOGCONFIG(TAG, "  Communication format: %s", fmt_str);
}

void I2SAudioSpeaker::set_i2s_comm_fmt(I2SCommFmt fmt) {
  this->i2s_comm_fmt_ = fmt;
  // Propagate to parent's string field, which start_i2s_channel_() reads.
  switch (fmt) {
    case I2SCommFmt::PCM:
      I2SAudioBase::set_i2s_comm_fmt("pcm");
      break;
    case I2SCommFmt::MSB:
      I2SAudioBase::set_i2s_comm_fmt("msb");
      break;
    default:
      I2SAudioBase::set_i2s_comm_fmt("std");
      break;
  }
}

void I2SAudioSpeaker::run_speaker_task() {
  xEventGroupSetBits(this->event_group_, SpeakerEventGroupBits::TASK_STARTING);

  // Use the hardware DMA buffer length (frames) directly. Computing from DMA_BUFFER_DURATION_MS
  // only works when sample_rate == 16kHz (240 frames == 15ms); at 48kHz it gives 720 frames
  // which is 3× the actual buffer size, causing constant underflow and dropped samples.
  const uint32_t frames_to_fill_single_dma_buffer = this->get_dma_buffer_length();
  const size_t bytes_to_fill_single_dma_buffer =
      this->current_stream_info_.frames_to_bytes(frames_to_fill_single_dma_buffer);
  const uint32_t actual_dma_buffer_ms =
      this->current_stream_info_.frames_to_microseconds(frames_to_fill_single_dma_buffer) / 1000;

  const uint32_t dma_buffers_duration_ms = actual_dma_buffer_ms * DMA_BUFFERS_COUNT;
  const uint32_t ring_buffer_duration = std::max(dma_buffers_duration_ms, this->buffer_duration_ms_);

  const size_t ring_buffer_size = this->current_stream_info_.ms_to_bytes(ring_buffer_duration);

  // When the I2S slot width exceeds the stream bit depth (e.g., 32-bit I2S with 16-bit
  // audio), each sample must be expanded before writing so the DMA sees the correct number
  // of bytes per frame.  Only the 2× case (16-bit → 32-bit) is supported; other ratios
  // are rejected by start_i2s_driver_() before the task reaches this point.
  const uint8_t expand_factor = this->i2s_bits_per_sample() / this->current_stream_info_.get_bits_per_sample();

  bool successful_setup = false;
  std::unique_ptr<audio::AudioSourceTransferBuffer> transfer_buffer =
      audio::AudioSourceTransferBuffer::create(bytes_to_fill_single_dma_buffer);
  std::unique_ptr<uint8_t[]> expansion_buffer;
  if (expand_factor > 1) {
    expansion_buffer = std::make_unique<uint8_t[]>(bytes_to_fill_single_dma_buffer * expand_factor);
  }

  if (transfer_buffer != nullptr && (expand_factor == 1 || expansion_buffer != nullptr)) {
    std::shared_ptr<RingBuffer> temp_ring_buffer = RingBuffer::create(ring_buffer_size);
    if (temp_ring_buffer.use_count() == 1) {
      transfer_buffer->set_source(temp_ring_buffer);
      this->audio_ring_buffer_ = temp_ring_buffer;  // assign to base weak_ptr
      successful_setup = true;
    }
  }

  if (!successful_setup) {
    xEventGroupSetBits(this->event_group_, SpeakerEventGroupBits::ERR_ESP_NO_MEM);
  } else {
    bool stop_gracefully = false;
    bool tx_dma_underflow = true;

    uint32_t frames_written = 0;
    uint32_t last_data_received_time = millis();

    xEventGroupSetBits(this->event_group_, SpeakerEventGroupBits::TASK_RUNNING);

    while (this->pause_state_ || !this->timeout_.has_value() ||
           (millis() - last_data_received_time) <= this->timeout_.value()) {
      uint32_t event_group_bits = xEventGroupGetBits(this->event_group_);

      if (event_group_bits & SpeakerEventGroupBits::COMMAND_STOP) {
        xEventGroupClearBits(this->event_group_, SpeakerEventGroupBits::COMMAND_STOP);
        ESP_LOGV(TAG, "Exiting: COMMAND_STOP received");
        break;
      }
      if (event_group_bits & SpeakerEventGroupBits::COMMAND_STOP_GRACEFULLY) {
        xEventGroupClearBits(this->event_group_, SpeakerEventGroupBits::COMMAND_STOP_GRACEFULLY);
        stop_gracefully = true;
      }

      if (this->audio_stream_info_ != this->current_stream_info_) {
        ESP_LOGV(TAG, "Exiting: stream info changed");
        break;
      }

      int64_t write_timestamp;
      while (xQueueReceive(this->i2s_event_queue_, &write_timestamp, 0)) {
        uint32_t frames_sent = frames_to_fill_single_dma_buffer;
        if (frames_to_fill_single_dma_buffer > frames_written) {
          tx_dma_underflow = true;
          frames_sent = frames_written;
          const uint32_t frames_zeroed = frames_to_fill_single_dma_buffer - frames_written;
          write_timestamp -= this->current_stream_info_.frames_to_microseconds(frames_zeroed);
        } else {
          tx_dma_underflow = false;
        }
        frames_written -= frames_sent;
        if (frames_sent > 0) {
          this->audio_output_callback_(frames_sent, write_timestamp);
        }
      }

      if (this->pause_state_) {
        vTaskDelay(pdMS_TO_TICKS(actual_dma_buffer_ms));
        continue;
      }

      const uint32_t read_delay =
          (this->current_stream_info_.frames_to_microseconds(frames_written) / 1000) / 2;

      size_t bytes_read = transfer_buffer->transfer_data_from_source(pdMS_TO_TICKS(read_delay));
      uint8_t *new_data = transfer_buffer->get_buffer_end() - bytes_read;

      if (bytes_read > 0) {
        this->apply_software_volume_(new_data, bytes_read);
        this->swap_esp32_mono_samples_(new_data, bytes_read);
      }

      if (transfer_buffer->available() == 0) {
        if (stop_gracefully && tx_dma_underflow) {
          break;
        }
        vTaskDelay(pdMS_TO_TICKS(actual_dma_buffer_ms / 2 + 1));
      } else {
        size_t bytes_written = 0;
        i2s_chan_handle_t handle = this->parent_->get_tx_handle();

        const void *write_ptr = transfer_buffer->get_buffer_start();
        size_t write_size = transfer_buffer->available();

        if (expand_factor == 2) {
          const int16_t *in = reinterpret_cast<const int16_t *>(write_ptr);
          int32_t *out = reinterpret_cast<int32_t *>(expansion_buffer.get());
          const size_t samples = write_size / sizeof(int16_t);
          for (size_t s = 0; s < samples; ++s) {
            out[s] = static_cast<int32_t>(in[s]) << 16;
          }
          write_ptr = expansion_buffer.get();
          write_size = samples * sizeof(int32_t);
        }

        if (tx_dma_underflow) {
          // Disable channel and clear callback to reset the DMA buffer queue,
          // then preload data so timing callbacks are accurate when re-enabled.
          i2s_channel_disable(handle);
          const i2s_event_callbacks_t null_callbacks = {.on_sent = nullptr};
          i2s_channel_register_event_callback(handle, &null_callbacks, this);
          i2s_channel_preload_data(handle, write_ptr, write_size, &bytes_written);
        } else {
          i2s_channel_write(handle, write_ptr, write_size, &bytes_written, actual_dma_buffer_ms);
        }

        bytes_written /= expand_factor;  // normalize back to stream-byte units

        if (bytes_written > 0) {
          last_data_received_time = millis();
          frames_written += this->current_stream_info_.bytes_to_frames(bytes_written);
          transfer_buffer->decrease_buffer_length(bytes_written);

          if (tx_dma_underflow) {
            tx_dma_underflow = false;
            xQueueReset(this->i2s_event_queue_);
            const i2s_event_callbacks_t callbacks = {.on_sent = i2s_on_sent_cb};
            i2s_channel_register_event_callback(handle, &callbacks, this);
            i2s_channel_enable(handle);
          }
        }
      }
    }
  }

  xEventGroupSetBits(this->event_group_, SpeakerEventGroupBits::TASK_STOPPING);

  if (transfer_buffer != nullptr) {
    transfer_buffer.reset();
  }

  xEventGroupSetBits(this->event_group_, SpeakerEventGroupBits::TASK_STOPPED);

  while (true) {
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

esp_err_t I2SAudioSpeaker::start_i2s_driver_(audio::AudioStreamInfo &audio_stream_info) {
  this->current_stream_info_ = audio_stream_info;

  if (this->has_fixed_i2s_rate() && (this->sample_rate_ != audio_stream_info.get_sample_rate())) {
    ESP_LOGE(TAG, "Incompatible stream settings");
    return ESP_ERR_NOT_SUPPORTED;
  }
  if (this->has_fixed_i2s_bitdepth() && this->i2s_bits_per_sample() != audio_stream_info.get_bits_per_sample() &&
      this->i2s_bits_per_sample() != 2 * audio_stream_info.get_bits_per_sample()) {
    ESP_LOGE(TAG, "Stream bits per sample must be <= speaker configuration");
    return ESP_ERR_NOT_SUPPORTED;
  }

  // Create or reset event queue before starting channel so ISR callback is safe
  if (this->i2s_event_queue_ == nullptr) {
    this->i2s_event_queue_ = xQueueCreate(I2S_EVENT_QUEUE_COUNT, sizeof(int64_t));
  } else {
    xQueueReset(this->i2s_event_queue_);
  }

  // Start with no on_sent callback; the task registers it after the first preload
  const i2s_event_callbacks_t no_callbacks = {.on_sent = nullptr};
  if (!this->start_i2s_channel_(no_callbacks)) {
    return ESP_ERR_INVALID_STATE;
  }
  return ESP_OK;
}

}  // namespace esphome::i2s_audio

#endif  // USE_ESP32
