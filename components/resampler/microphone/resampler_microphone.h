#pragma once

#ifdef USE_ESP32

#include "esphome/components/audio/audio.h"
#include "esphome/components/audio/audio_transfer_buffer.h"
#include "esphome/components/microphone/microphone_source.h"

#include "esphome/core/component.h"

#include <memory>

#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/semphr.h>

namespace esphome {
namespace resampler {

class ResamplerMicrophone : public Component, public microphone::Microphone {
 public:
  float get_setup_priority() const override { return esphome::setup_priority::DATA; }
  void setup() override;
  void loop() override;

  void start() override;
  void stop() override;

  void set_microphone_source(microphone::MicrophoneSource *microphone_source) {
    this->microphone_source_ = microphone_source;
  }
  void set_task_stack_in_psram(bool task_stack_in_psram) { this->task_stack_in_psram_ = task_stack_in_psram; }

  void set_target_sample_rate(uint32_t target_sample_rate) { this->target_sample_rate_ = target_sample_rate; }

  void set_filters(uint16_t filters) { this->filters_ = filters; }
  void set_taps(uint16_t taps) { this->taps_ = taps; }

  void set_buffer_duration(uint32_t buffer_duration_ms) { this->buffer_duration_ms_ = buffer_duration_ms; }

 protected:
  /// @brief Starts the resampler task after allocating the task stack
  /// @return ESP_OK if successful,
  ///         ESP_ERR_NO_MEM if the task stack couldn't be allocated
  ///         ESP_ERR_INVALID_STATE if the task wasn't created
  esp_err_t start_task_();

  void deallocate_task_stack_();

  inline bool requires_resampling_() const;
  static void resample_task(void *params);

  /// @brief Sets the Microphone ``audio_stream_info_`` member variable to the configured I2S settings.
  void configure_stream_settings_();

  uint32_t target_sample_rate_;
  uint32_t buffer_duration_ms_;
  uint16_t taps_;
  uint16_t filters_;
  bool task_stack_in_psram_{false};

  EventGroupHandle_t event_group_{nullptr};
  SemaphoreHandle_t active_listeners_semaphore_{nullptr};

  microphone::MicrophoneSource *microphone_source_{nullptr};

  TaskHandle_t task_handle_{nullptr};
  StackType_t *task_stack_buffer_{nullptr};
  StaticTask_t task_stack_;

  std::weak_ptr<RingBuffer> ring_buffer_;

  audio::AudioStreamInfo source_stream_info_;
};

}  // namespace resampler
}  // namespace esphome

#endif
