#pragma once

#ifdef USE_ESP32

#include "../i2s_audio.h"

#include <freertos/event_groups.h>
#include <freertos/queue.h>
#include <freertos/FreeRTOS.h>

#include "esphome/components/audio/audio.h"
#include "esphome/components/speaker/speaker.h"

#include "esphome/core/component.h"
#include "esphome/core/gpio.h"
#include "esphome/core/helpers.h"
#include "esphome/core/ring_buffer.h"

namespace esphome::i2s_audio {

// Shared constants for I2S audio speaker implementations
static constexpr uint32_t DMA_BUFFER_DURATION_MS = 15;
static constexpr size_t TASK_STACK_SIZE = 4096;
static constexpr ssize_t TASK_PRIORITY = 19;

enum SpeakerEventGroupBits : uint32_t {
  COMMAND_START = (1 << 0),            // indicates loop should start speaker task
  COMMAND_STOP = (1 << 1),             // stops the speaker task
  COMMAND_STOP_GRACEFULLY = (1 << 2),  // stops the speaker task once all data has been written

  TASK_STARTING = (1 << 10),
  TASK_RUNNING = (1 << 11),
  TASK_STOPPING = (1 << 12),
  TASK_STOPPED = (1 << 13),

  ERR_ESP_NO_MEM = (1 << 19),

  WARN_DROPPED_EVENT = (1 << 20),

  ALL_BITS = 0x00FFFFFF,  // All valid FreeRTOS event group bits
};

/// @brief Abstract base class for I2S audio speaker implementations.
/// Provides shared infrastructure: event groups, ring buffer, software volume control,
/// task lifecycle, and common setup()/loop()/start()/stop()/play() logic.
/// Derived classes implement run_speaker_task() and start_i2s_driver_().
class I2SAudioSpeakerBase : public I2SAudioOut, public speaker::Speaker, public Component {
 public:
  float get_setup_priority() const override { return esphome::setup_priority::PROCESSOR; }

  void setup() override;
  void loop() override;
  void dump_config() override;

  void set_buffer_duration(uint32_t buffer_duration_ms) { this->buffer_duration_ms_ = buffer_duration_ms; }
  void set_timeout(uint32_t ms) { this->timeout_ = ms; }

  void start() override;
  void stop() override;
  void finish() override;

  void set_pause_state(bool pause_state) override { this->pause_state_ = pause_state; }
  bool get_pause_state() const override { return this->pause_state_; }

  size_t play(const uint8_t *data, size_t length, TickType_t ticks_to_wait) override;
  size_t play(const uint8_t *data, size_t length) override { return play(data, length, 0); }

  bool has_buffered_data() const override;

  void set_volume(float volume) override;
  void set_mute_state(bool mute_state) override;

 protected:
  /// @brief FreeRTOS task entry point. Casts params and calls run_speaker_task().
  static void speaker_task(void *params);

  /// @brief Main speaker task loop. Implemented by derived classes.
  virtual void run_speaker_task() = 0;

  /// @brief Starts the I2S driver for the given stream. Called from loop() before task spawn.
  /// Uses the shared I2S bus via start_i2s_channel_() / stop_i2s_channel_().
  virtual esp_err_t start_i2s_driver_(audio::AudioStreamInfo &audio_stream_info) = 0;

  /// @brief Called in loop() when the task has stopped. Override for mode-specific cleanup.
  virtual void on_task_stopped() {}

  void stop_(bool wait_on_empty);

  static bool IRAM_ATTR i2s_on_sent_cb(i2s_chan_handle_t handle, i2s_event_data_t *event, void *user_ctx);

  void apply_software_volume_(uint8_t *data, size_t bytes_read);
  void swap_esp32_mono_samples_(uint8_t *data, size_t bytes_read);

  TaskHandle_t speaker_task_handle_{nullptr};
  EventGroupHandle_t event_group_{nullptr};

  // Weak pointer: task owns the shared_ptr; base holds a weak ref for play() / has_buffered_data()
  std::weak_ptr<RingBuffer> audio_ring_buffer_;

  uint32_t buffer_duration_ms_;
  optional<uint32_t> timeout_;
  bool pause_state_{false};
  int32_t q31_volume_factor_{INT32_MAX};

  QueueHandle_t i2s_event_queue_{nullptr};

  audio::AudioStreamInfo current_stream_info_;
};

}  // namespace esphome::i2s_audio

#endif  // USE_ESP32
