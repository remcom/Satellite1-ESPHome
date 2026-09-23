#include "i2s_audio_speaker.h"

#ifdef USE_ESP32

#include "esphome/components/audio/audio.h"

#include "esphome/core/hal.h"
#include "esphome/core/log.h"

#include "esp_timer.h"

#include <cmath>

namespace esphome::i2s_audio {

static const char *const TAG = "i2s_audio.speaker";

// Software volume control maps the user-facing (0.0, 1.0) range linearly to a dB reduction in
// [-49.0, 0.0] dB; 0.0 is silence.
static constexpr float SOFTWARE_VOLUME_MIN_DB = -49.0f;

// Rate at which the software gain moves toward a new target.
static constexpr uint32_t GAIN_RAMP_MS_PER_DB = 1;

void I2SAudioSpeakerBase::setup() {
  this->event_group_ = xEventGroupCreate();
  if (this->event_group_ == nullptr) {
    ESP_LOGE(TAG, "Event group creation failed");
    this->mark_failed();
    return;
  }

  // Initialize volume control. When audio_dac is configured, this sets the DAC volume and mute state.
  // When no audio_dac is configured, this initializes software volume control.
  this->set_volume(this->volume_);
  this->set_mute_state(this->mute_state_);
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

  // A stop that arrives before loop() has spawned the task cancels the pending start outright,
  // instead of starting the driver (and taking the shared bus lock) only to tear it down again.
  constexpr uint32_t stop_bits = SpeakerEventGroupBits::COMMAND_STOP | SpeakerEventGroupBits::COMMAND_STOP_GRACEFULLY;
  if ((event_group_bits & stop_bits) && (this->state_ == speaker::STATE_STARTING) &&
      (this->speaker_task_handle_ == nullptr)) {
    xEventGroupClearBits(this->event_group_, stop_bits | SpeakerEventGroupBits::COMMAND_START);
    event_group_bits &= ~(stop_bits | SpeakerEventGroupBits::COMMAND_START);
    this->state_ = speaker::STATE_STOPPED;
  }

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

    // ALL_BITS includes COMMAND_START. Take the bits from the clear itself, not from the snapshot at
    // the top of loop(): the audio source's task can raise a start at any point above, including
    // during stop_i2s_channel(), and nothing would ever re-issue it.
    const EventBits_t bits_before_clear = xEventGroupClearBits(this->event_group_, SpeakerEventGroupBits::ALL_BITS);
    this->status_clear_error();
    if (bits_before_clear & SpeakerEventGroupBits::COMMAND_START) {
      ESP_LOGD(TAG, "Start requested while stopping; keeping the request");
      xEventGroupSetBits(this->event_group_, SpeakerEventGroupBits::COMMAND_START);
      this->state_ = speaker::STATE_STARTING;
    } else {
      this->state_ = speaker::STATE_STOPPED;
    }
  }

  if (event_group_bits & SpeakerEventGroupBits::ERR_ESP_NO_MEM) {
    ESP_LOGE(TAG, "Not enough memory");
    xEventGroupClearBits(this->event_group_, SpeakerEventGroupBits::ERR_ESP_NO_MEM);
  }

  // Spawn task when COMMAND_START is received and speaker is starting
  // The task handle is only cleared once TASK_STOPPED has been processed above; starting the driver
  // while a previous run is still winding down fails spuriously, so keep the request pending instead.
  if ((event_group_bits & SpeakerEventGroupBits::COMMAND_START) && (this->state_ == speaker::STATE_STARTING) &&
      (this->speaker_task_handle_ == nullptr)) {
    xEventGroupClearBits(this->event_group_, SpeakerEventGroupBits::COMMAND_START);

    if (this->start_i2s_driver(this->audio_stream_info_) != ESP_OK) {
      ESP_LOGE(TAG, "Driver failed to start; retrying in 1 second");
      this->state_ = speaker::STATE_STOPPED;
      this->status_momentary_error("driver-failure", 1000);
    } else {
      // Seed the ramp at the live target so this run adopts it instantly rather than fading to it
      // from wherever the previous run left off. Posted here, not in the task: the ramp's mailbox
      // allows one writer, and that is the main loop.
      this->post_software_gain_(0);
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
  speaker::Speaker::set_volume(volume);
  this->post_software_gain_(this->audio_stream_info_.ms_to_samples(GAIN_RAMP_MS_PER_DB));
}

void I2SAudioSpeakerBase::set_mute_state(bool mute_state) {
  speaker::Speaker::set_mute_state(mute_state);
  this->post_software_gain_(this->audio_stream_info_.ms_to_samples(GAIN_RAMP_MS_PER_DB));
}

void I2SAudioSpeakerBase::post_software_gain_(uint32_t rate_samples) {
#ifdef USE_AUDIO_DAC
  if (this->audio_dac_ != nullptr) {
    return;  // Hardware volume; the ramp stays at unity
  }
#endif  // USE_AUDIO_DAC
  // Software volume control. The ramp treats 0 dB as unity and skips processing there.
  float target_db;
  if (this->is_silent_()) {
    target_db = -INFINITY;
  } else if (this->volume_ >= 1.0f) {
    target_db = 0.0f;
  } else {
    target_db = remap<float, float>(this->volume_, 0.0f, 1.0f, SOFTWARE_VOLUME_MIN_DB, 0.0f);
  }
  this->gain_ramp_.set_target_db_at_rate(target_db, rate_samples);
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
  std::shared_ptr<ring_buffer::RingBuffer> temp_ring_buffer = this->audio_ring_buffer_.lock();
  if (temp_ring_buffer != nullptr) {
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
  if (!this->is_ready() || this->is_failed())
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
    // Queue is full, so discard the oldest event. The task's frame accounting is now off; it resyncs.
    int64_t dummy;
    xQueueReceiveFromISR(this_speaker->i2s_event_queue_, &dummy, &need_yield1);
    xEventGroupSetBitsFromISR(this_speaker->event_group_, SpeakerEventGroupBits::ERR_DROPPED_EVENT, &need_yield2);
  }
  xQueueSendToBackFromISR(this_speaker->i2s_event_queue_, &now, &need_yield3);

  return need_yield1 | need_yield2 | need_yield3;
}

void I2SAudioSpeakerBase::apply_software_volume_(uint8_t *data, size_t bytes_read) {
#ifdef USE_AUDIO_DAC
  if (this->audio_dac_ != nullptr) {
    return;  // Hardware volume; the ramp is never targeted
  }
#endif  // USE_AUDIO_DAC
  const size_t bytes_per_sample = this->current_stream_info_.samples_to_bytes(1);
  this->gain_ramp_.process(data, static_cast<uint8_t>(bytes_per_sample),
                           this->current_stream_info_.bytes_to_samples(bytes_read));
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
