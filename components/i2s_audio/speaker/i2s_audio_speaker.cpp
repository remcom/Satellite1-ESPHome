#include "i2s_audio_speaker.h"

#ifdef USE_ESP32

#include "esphome/components/audio/audio.h"

#include "esphome/core/hal.h"
#include "esphome/core/log.h"

#include "esp_timer.h"

#include <algorithm>
#include <cmath>

// esp-audio-libs
#include <gain.h>

namespace esphome::i2s_audio {

static const char *const TAG = "i2s_audio.speaker";

// Software volume control maps the user-facing [0.0, 1.0] range to a Q31 scale factor.
// Volumes in (0.0, 1.0) map linearly to a dB reduction in [-49.0, 0.0] dB.
static constexpr float SOFTWARE_VOLUME_MIN_DB = -49.0f;

void I2SAudioSpeakerBase::setup() {
  this->event_group_ = xEventGroupCreate();
  if (this->event_group_ == nullptr) {
    ESP_LOGE(TAG, "Event group creation failed");
    this->mark_failed();
    return;
  }

  this->set_volume(this->volume_);
}

void I2SAudioSpeakerBase::dump_config() {
  this->dump_i2s_settings();
  ESP_LOGCONFIG(TAG,
                "Speaker:\n"
                "  Buffer duration: %" PRIu32 " ms",
                this->buffer_duration_ms_);
  if (this->timeout_.has_value()) {
    ESP_LOGCONFIG(TAG, "  Timeout: %" PRIu32 " ms", this->timeout_.value());
  }
}

void I2SAudioSpeakerBase::loop() {
  uint32_t event_group_bits = xEventGroupGetBits(this->event_group_);

  if (event_group_bits & SpeakerEventGroupBits::TASK_STARTING) {
    xEventGroupClearBits(this->event_group_, SpeakerEventGroupBits::TASK_STARTING);
  }
  if (event_group_bits & SpeakerEventGroupBits::TASK_RUNNING) {
    ESP_LOGV(TAG, "Started");
    xEventGroupClearBits(this->event_group_, SpeakerEventGroupBits::TASK_RUNNING);
    this->state_ = speaker::STATE_RUNNING;
  }
  if (event_group_bits & SpeakerEventGroupBits::TASK_STOPPING) {
    ESP_LOGV(TAG, "Stopping");
    // Lockstep-breaking error bits are latched by the task and cleared along with all other bits
    // when TASK_STOPPED is processed; log them here, exactly once, as the task winds down.
    if (event_group_bits & SpeakerEventGroupBits::ERR_DROPPED_EVENT) {
      ESP_LOGE(TAG, "ISR event queue overflow, restarting speaker task to recover timestamp sync");
    }
    if (event_group_bits & SpeakerEventGroupBits::ERR_PARTIAL_WRITE) {
      ESP_LOGE(TAG, "Partial DMA write broke buffer alignment, restarting speaker task");
    }
    if (event_group_bits & SpeakerEventGroupBits::ERR_LOCKSTEP_DESYNC) {
      ESP_LOGE(TAG, "Event/record queues desynced, restarting speaker task");
    }
    xEventGroupClearBits(this->event_group_, SpeakerEventGroupBits::TASK_STOPPING);
    this->state_ = speaker::STATE_STOPPING;
  }
  if (event_group_bits & SpeakerEventGroupBits::TASK_STOPPED) {
    if (this->speaker_task_handle_ != nullptr) {
      vTaskDelete(this->speaker_task_handle_);
      this->speaker_task_handle_ = nullptr;
    }

    this->stop_i2s_channel();
    this->on_task_stopped();

    xEventGroupClearBits(this->event_group_, SpeakerEventGroupBits::ALL_BITS);
    this->status_clear_error();
    this->state_ = speaker::STATE_STOPPED;
  }

  if (event_group_bits & SpeakerEventGroupBits::ERR_ESP_NO_MEM) {
    ESP_LOGE(TAG, "Not enough memory");
    xEventGroupClearBits(this->event_group_, SpeakerEventGroupBits::ERR_ESP_NO_MEM);
  }

  // Spawn task when COMMAND_START is received and speaker is starting
  if ((event_group_bits & SpeakerEventGroupBits::COMMAND_START) && (this->state_ == speaker::STATE_STARTING)) {
    xEventGroupClearBits(this->event_group_, SpeakerEventGroupBits::COMMAND_START);

    if (this->start_i2s_driver(this->audio_stream_info_) != ESP_OK) {
      ESP_LOGE(TAG, "Driver failed to start; retrying in 1 second");
      this->state_ = speaker::STATE_STOPPED;
      this->status_momentary_error("driver-failure", 1000);
    } else if (this->speaker_task_handle_ == nullptr) {
      xTaskCreate(I2SAudioSpeakerBase::speaker_task, "speaker_task", TASK_STACK_SIZE, (void *) this, TASK_PRIORITY,
                  &this->speaker_task_handle_);

      if (this->speaker_task_handle_ == nullptr) {
        ESP_LOGE(TAG, "Failed to create speaker task");
        this->status_set_error(LOG_STR("Failed to create speaker task"));
        this->stop_i2s_channel();
        this->state_ = speaker::STATE_STOPPED;
      }
    }
  }
}

void I2SAudioSpeakerBase::set_volume(float volume) {
  this->volume_ = volume;
#ifdef USE_AUDIO_DAC
  if (this->audio_dac_ != nullptr) {
    this->audio_dac_->set_volume(volume);
  } else
#endif  // USE_AUDIO_DAC
  {
    if (volume >= 1.0f) {
      this->q31_volume_factor_ = INT32_MAX;
    } else if (volume <= 0.0f) {
      this->q31_volume_factor_ = 0;
    } else {
      this->q31_volume_factor_ =
          esp_audio_libs::gain::db_to_q31(remap<float, float>(volume, 0.0f, 1.0f, SOFTWARE_VOLUME_MIN_DB, 0.0f));
    }
  }
}

void I2SAudioSpeakerBase::set_mute_state(bool mute_state) {
  this->mute_state_ = mute_state;
#ifdef USE_AUDIO_DAC
  if (this->audio_dac_) {
    if (mute_state) {
      this->audio_dac_->set_mute_on();
    } else {
      this->audio_dac_->set_mute_off();
    }
  } else
#endif  // USE_AUDIO_DAC
  {
    if (mute_state) {
      this->q31_volume_factor_ = 0;
    } else {
      this->set_volume(this->volume_);
    }
  }
}

size_t I2SAudioSpeakerBase::play(const uint8_t *data, size_t length, TickType_t ticks_to_wait) {
  if (this->is_failed()) {
    ESP_LOGE(TAG, "Setup failed; cannot play audio");
    return 0;
  }

  if (this->state_ != speaker::STATE_RUNNING && this->state_ != speaker::STATE_STARTING) {
    this->start();
  }

  size_t bytes_written = 0;
  if (this->state_ == speaker::STATE_RUNNING) {
    std::shared_ptr<ring_buffer::RingBuffer> temp_ring_buffer = this->audio_ring_buffer_.lock();
    if (temp_ring_buffer != nullptr) {
      bytes_written = temp_ring_buffer->write_without_replacement((void *) data, length, ticks_to_wait);
    }
  }

  return bytes_written;
}

bool I2SAudioSpeakerBase::has_buffered_data() const {
  if (this->audio_ring_buffer_.use_count() > 0) {
    std::shared_ptr<ring_buffer::RingBuffer> temp_ring_buffer = this->audio_ring_buffer_.lock();
    return temp_ring_buffer->available() > 0;
  }
  return false;
}

void I2SAudioSpeakerBase::speaker_task(void *params) {
  I2SAudioSpeakerBase *this_speaker = (I2SAudioSpeakerBase *) params;
  this_speaker->run_speaker_task();
}

void I2SAudioSpeakerBase::start() {
  if (!this->is_ready() || this->is_failed() || this->status_has_error())
    return;
  if ((this->state_ == speaker::STATE_STARTING) || (this->state_ == speaker::STATE_RUNNING))
    return;

  this->state_ = speaker::STATE_STARTING;
  xEventGroupSetBits(this->event_group_, SpeakerEventGroupBits::COMMAND_START);
}

void I2SAudioSpeakerBase::stop() { this->stop_(false); }

void I2SAudioSpeakerBase::finish() { this->stop_(true); }

void I2SAudioSpeakerBase::stop_(bool wait_on_empty) {
  if (this->is_failed())
    return;
  if (this->state_ == speaker::STATE_STOPPED)
    return;

  if (wait_on_empty) {
    xEventGroupSetBits(this->event_group_, SpeakerEventGroupBits::COMMAND_STOP_GRACEFULLY);
  } else {
    xEventGroupSetBits(this->event_group_, SpeakerEventGroupBits::COMMAND_STOP);
  }
}

bool IRAM_ATTR I2SAudioSpeakerBase::i2s_on_sent_cb(i2s_chan_handle_t handle, i2s_event_data_t *event, void *user_ctx) {
  int64_t now = esp_timer_get_time();

  BaseType_t need_yield1 = pdFALSE;
  BaseType_t need_yield2 = pdFALSE;
  BaseType_t need_yield3 = pdFALSE;

  I2SAudioSpeakerBase *this_speaker = (I2SAudioSpeakerBase *) user_ctx;

  if (xQueueIsQueueFullFromISR(this_speaker->i2s_event_queue_)) {
    // Queue is full, so discard the oldest event. Once we drop a completion event, i2s_event_queue_
    // and any per-buffer record queue maintained by the task are permanently desynced, so the task
    // must restart to recover. Set both ERR_DROPPED_EVENT (so loop() can log it) and COMMAND_STOP
    // (so the task bails immediately, closing the race where loop() could clear the error bit
    // before the task observes it).
    int64_t dummy;
    xQueueReceiveFromISR(this_speaker->i2s_event_queue_, &dummy, &need_yield1);
    xEventGroupSetBitsFromISR(this_speaker->event_group_,
                              SpeakerEventGroupBits::ERR_DROPPED_EVENT | SpeakerEventGroupBits::COMMAND_STOP,
                              &need_yield2);
  }
  xQueueSendToBackFromISR(this_speaker->i2s_event_queue_, &now, &need_yield3);

  return need_yield1 | need_yield2 | need_yield3;
}

void I2SAudioSpeakerBase::apply_software_volume_(uint8_t *data, size_t bytes_read) {
  if (this->q31_volume_factor_ == INT32_MAX) {
    return;  // Max volume, no processing needed
  }

  const size_t bytes_per_sample = this->current_stream_info_.samples_to_bytes(1);
  const uint32_t len = bytes_read / bytes_per_sample;

  esp_audio_libs::gain::apply(data, data, this->q31_volume_factor_, len, bytes_per_sample);
}

float I2SAudioSpeakerBase::get_audio_level_(const std::atomic<float> &level) const {
  // A DMA-sized chunk covers ~15 ms; treat anything older than a few chunks as silence
  static constexpr uint32_t AUDIO_LEVEL_STALE_MS = 150;
  if (millis() - this->audio_level_updated_ms_.load(std::memory_order_relaxed) > AUDIO_LEVEL_STALE_MS) {
    return 0.0f;
  }
  return level.load(std::memory_order_relaxed);
}

void I2SAudioSpeakerBase::update_audio_level_(const uint8_t *data, size_t bytes_read) {
  static constexpr float BASS_CUTOFF_HZ = 250.0f;
  static constexpr float MID_CUTOFF_HZ = 2500.0f;

  const uint32_t sample_rate = this->current_stream_info_.get_sample_rate();
  const uint8_t channels = this->current_stream_info_.get_channels();
  const bool is_16_bit = this->current_stream_info_.get_bits_per_sample() <= 16;
  const size_t frames = bytes_read / this->current_stream_info_.frames_to_bytes(1);

  // One-pole low-pass coefficients; the meter only needs a rough band split
  const float alpha_bass = 1.0f - expf(-2.0f * M_PI * BASS_CUTOFF_HZ / sample_rate);
  const float alpha_mid = 1.0f - expf(-2.0f * M_PI * MID_CUTOFF_HZ / sample_rate);

  float lp_bass = this->band_filter_bass_;
  float lp_mid = this->band_filter_mid_;
  float peak = 0.0f;
  float peak_bass = 0.0f;
  float peak_mid = 0.0f;
  float peak_treble = 0.0f;

  // Filters run on the first channel only; a mono view is plenty for a visualizer
  for (size_t f = 0; f < frames; f++) {
    float sample;
    if (is_16_bit) {
      sample = static_cast<float>(reinterpret_cast<const int16_t *>(data)[f * channels]) / 32768.0f;
    } else {
      sample = static_cast<float>(reinterpret_cast<const int32_t *>(data)[f * channels]) / 2147483648.0f;
    }
    peak = std::max(peak, fabsf(sample));

    lp_bass += alpha_bass * (sample - lp_bass);
    lp_mid += alpha_mid * (sample - lp_mid);
    peak_bass = std::max(peak_bass, fabsf(lp_bass));
    peak_mid = std::max(peak_mid, fabsf(lp_mid - lp_bass));
    peak_treble = std::max(peak_treble, fabsf(sample - lp_mid));
  }

  this->band_filter_bass_ = lp_bass;
  this->band_filter_mid_ = lp_mid;

  this->audio_level_.store(peak, std::memory_order_relaxed);
  this->audio_level_bass_.store(peak_bass, std::memory_order_relaxed);
  this->audio_level_mid_.store(peak_mid, std::memory_order_relaxed);
  this->audio_level_treble_.store(peak_treble, std::memory_order_relaxed);
  this->audio_level_updated_ms_.store(millis(), std::memory_order_relaxed);
}

void I2SAudioSpeakerBase::reset_audio_level_() {
  this->audio_level_.store(0.0f, std::memory_order_relaxed);
  this->audio_level_bass_.store(0.0f, std::memory_order_relaxed);
  this->audio_level_mid_.store(0.0f, std::memory_order_relaxed);
  this->audio_level_treble_.store(0.0f, std::memory_order_relaxed);
  this->audio_level_updated_ms_.store(0, std::memory_order_relaxed);
  this->band_filter_bass_ = 0.0f;
  this->band_filter_mid_ = 0.0f;
}

void I2SAudioSpeakerBase::swap_esp32_mono_samples_(uint8_t *data, size_t bytes_read) {
#ifdef USE_ESP32_VARIANT_ESP32
  if (this->current_stream_info_.get_channels() == 1 && this->current_stream_info_.get_bits_per_sample() == 16) {
    int16_t *samples = reinterpret_cast<int16_t *>(data);
    size_t sample_count = bytes_read / sizeof(int16_t);
    for (size_t i = 0; i + 1 < sample_count; i += 2) {
      int16_t tmp = samples[i];
      samples[i] = samples[i + 1];
      samples[i + 1] = tmp;
    }
  }
#endif  // USE_ESP32_VARIANT_ESP32
}

}  // namespace esphome::i2s_audio

#endif  // USE_ESP32
