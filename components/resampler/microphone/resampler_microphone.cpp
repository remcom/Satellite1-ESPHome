#include "resampler_microphone.h"

#ifdef USE_ESP32

#include "esphome/components/audio/audio_resampler.h"

#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#include <algorithm>
#include <cstring>

namespace esphome {
namespace resampler {

static const UBaseType_t RESAMPLER_TASK_PRIORITY = 1;
static const UBaseType_t MAX_LISTENERS = 16;

static const uint32_t TRANSFER_BUFFER_DURATION_MS = 16;

static const uint32_t TASK_STACK_SIZE = 3072;

static const char *const TAG = "resampler_microphone";

enum ResamplingEventGroupBits : uint32_t {
  COMMAND_STOP = (1 << 0),  // stops the resampler task
  TASK_STARTING = (1 << 10),
  TASK_RUNNING = (1 << 11),
  TASK_STOPPING = (1 << 12),
  TASK_STOPPED = (1 << 13),
  WARNING_FULL_RING_BUFFER = (1 << 17),
  ERR_ESP_NO_MEM = (1 << 19),
  ERR_ESP_NOT_SUPPORTED = (1 << 20),
  ERR_ESP_FAIL = (1 << 21),
  ALL_BITS = 0x00FFFFFF,  // All valid FreeRTOS event group bits
};

void ResamplerMicrophone::setup() {
  this->event_group_ = xEventGroupCreate();
  if (this->event_group_ == nullptr) {
    ESP_LOGE(TAG, "Failed to create event group");
    this->mark_failed();
    return;
  }

  this->active_listeners_semaphore_ = xSemaphoreCreateCounting(MAX_LISTENERS, MAX_LISTENERS);
  if (this->active_listeners_semaphore_ == nullptr) {
    ESP_LOGE(TAG, "Creating semaphore failed");
    this->mark_failed();
    return;
  }

  this->microphone_source_->add_data_callback([this](const std::vector<uint8_t> &data) {
    if (this->state_ == microphone::STATE_STOPPED) {
      return;
    }
    if (this->requires_resampling_()) {
      std::shared_ptr<RingBuffer> temp_ring_buffer = this->ring_buffer_.lock();
      if (this->ring_buffer_.use_count() > 1) {
        size_t bytes_free = temp_ring_buffer->free();

        if (bytes_free < data.size()) {
          xEventGroupSetBits(this->event_group_, ResamplingEventGroupBits::WARNING_FULL_RING_BUFFER);
          temp_ring_buffer->reset();
        }
        temp_ring_buffer->write((void *) data.data(), data.size());
      }
    } else if (this->data_callbacks_.size() > 0) {
      // No resampling required, just pass through the audio
      this->data_callbacks_.call(data);
    }
  });

  this->configure_stream_settings_();
}

void ResamplerMicrophone::loop() {
  uint32_t event_group_bits = xEventGroupGetBits(this->event_group_);

  if (event_group_bits & ResamplingEventGroupBits::TASK_STARTING) {
    ESP_LOGD(TAG, "Starting");
    xEventGroupClearBits(this->event_group_, ResamplingEventGroupBits::TASK_STARTING);
  }

  if (event_group_bits & ResamplingEventGroupBits::TASK_RUNNING) {
    ESP_LOGD(TAG, "Started");

    xEventGroupClearBits(this->event_group_, ResamplingEventGroupBits::TASK_RUNNING);
    this->state_ = microphone::STATE_RUNNING;
  }

  if (event_group_bits & ResamplingEventGroupBits::TASK_STOPPING) {
    ESP_LOGD(TAG, "Stopping");
    xEventGroupClearBits(this->event_group_, ResamplingEventGroupBits::TASK_STOPPING);
  }

  if ((event_group_bits & ResamplingEventGroupBits::TASK_STOPPED)) {
    ESP_LOGD(TAG, "Stoppped");

    vTaskDelete(this->task_handle_);
    this->task_handle_ = nullptr;
    this->deallocate_task_stack_();

    xEventGroupClearBits(this->event_group_, ALL_BITS);
    this->status_clear_error();

    this->state_ = microphone::STATE_STOPPED;
  }

  if (event_group_bits & ResamplingEventGroupBits::WARNING_FULL_RING_BUFFER) {
    xEventGroupClearBits(this->event_group_, ResamplingEventGroupBits::WARNING_FULL_RING_BUFFER);
    ESP_LOGW(TAG, "Ring buffer full, resetting it.");
  }

  // Start the microphone if any semaphores are taken
  if ((uxSemaphoreGetCount(this->active_listeners_semaphore_) < MAX_LISTENERS) &&
      (this->state_ == microphone::STATE_STOPPED)) {
    this->state_ = microphone::STATE_STARTING;
  }

  // Stop the microphone if all semaphores are returned
  if ((uxSemaphoreGetCount(this->active_listeners_semaphore_) == MAX_LISTENERS) &&
      (this->state_ == microphone::STATE_RUNNING)) {
    this->state_ = microphone::STATE_STOPPING;
  }

  switch (this->state_) {
    case microphone::STATE_STARTING:
      if (this->status_has_error()) {
        break;
      }

      this->configure_stream_settings_();

      if (this->requires_resampling_()) {
        if (this->task_handle_ == nullptr) {
          if (this->start_task_() != ESP_OK) {
            ESP_LOGE(TAG, "Task failed to start, retrying in 1 second");
            this->status_momentary_error("task_fail", 1000);
          } else {
            this->microphone_source_->start();
          }
        }
      } else {
        // No task needed, just start the source mic and update state
        this->microphone_source_->start();
        this->state_ = microphone::STATE_RUNNING;
      }

      break;
    case microphone::STATE_RUNNING:
      break;
    case microphone::STATE_STOPPING:
      this->microphone_source_->stop();  // stop source mic
      if (this->requires_resampling_()) {
        xEventGroupSetBits(this->event_group_, ResamplingEventGroupBits::COMMAND_STOP);
      } else {
        // No task needed, just update state directly
        this->state_ = microphone::STATE_STOPPED;
      }
      break;
    case microphone::STATE_STOPPED:
      break;
  }
}

void ResamplerMicrophone::start() {
  if (this->is_failed())
    return;

  xSemaphoreTake(this->active_listeners_semaphore_, 0);
}

esp_err_t ResamplerMicrophone::start_task_() {
  if (this->task_stack_buffer_ == nullptr) {
    if (this->task_stack_in_psram_) {
      RAMAllocator<StackType_t> stack_allocator(RAMAllocator<StackType_t>::ALLOC_EXTERNAL);
      this->task_stack_buffer_ = stack_allocator.allocate(TASK_STACK_SIZE);
    } else {
      RAMAllocator<StackType_t> stack_allocator(RAMAllocator<StackType_t>::ALLOC_INTERNAL);
      this->task_stack_buffer_ = stack_allocator.allocate(TASK_STACK_SIZE);
    }
  }

  if (this->task_stack_buffer_ == nullptr) {
    return ESP_ERR_NO_MEM;
  }

  if (this->task_handle_ == nullptr) {
    this->task_handle_ = xTaskCreateStatic(resample_task, "resample_mic", TASK_STACK_SIZE, (void *) this,
                                           RESAMPLER_TASK_PRIORITY, this->task_stack_buffer_, &this->task_stack_);
  }

  if (this->task_handle_ == nullptr) {
    this->deallocate_task_stack_();
    return ESP_ERR_INVALID_STATE;
  }

  return ESP_OK;
}

void ResamplerMicrophone::stop() {
  if (this->state_ == microphone::STATE_STOPPED || this->is_failed())
    return;

  xSemaphoreGive(this->active_listeners_semaphore_);
}

void ResamplerMicrophone::deallocate_task_stack_() {
  if (this->task_stack_buffer_ != nullptr) {
    if (this->task_stack_in_psram_) {
      RAMAllocator<StackType_t> stack_allocator(RAMAllocator<StackType_t>::ALLOC_EXTERNAL);
      stack_allocator.deallocate(this->task_stack_buffer_, TASK_STACK_SIZE);
    } else {
      RAMAllocator<StackType_t> stack_allocator(RAMAllocator<StackType_t>::ALLOC_INTERNAL);
      stack_allocator.deallocate(this->task_stack_buffer_, TASK_STACK_SIZE);
    }

    this->task_stack_buffer_ = nullptr;
  }
}

void ResamplerMicrophone::configure_stream_settings_() {
  if (this->requires_resampling_()) {
    // Resampler outputs 32 bits per sample and lets downstream MicrophoneSource handle conversions
    this->audio_stream_info_ = audio::AudioStreamInfo(
        32, this->microphone_source_->get_audio_stream_info().get_channels(), this->target_sample_rate_);
  } else {
    // No resampling needed, so just pass through the source mic's stream info
    this->audio_stream_info_ = this->microphone_source_->get_audio_stream_info();
  }
}

bool ResamplerMicrophone::requires_resampling_() const {
  return (this->microphone_source_->get_audio_stream_info().get_sample_rate() != this->target_sample_rate_);
}

void ResamplerMicrophone::resample_task(void *params) {
  ResamplerMicrophone *this_resampler = (ResamplerMicrophone *) params;

  xEventGroupSetBits(this_resampler->event_group_, ResamplingEventGroupBits::TASK_STARTING);

  audio::AudioStreamInfo source_stream_info = this_resampler->microphone_source_->get_audio_stream_info();

  std::unique_ptr<audio::AudioResampler> resampler =
      make_unique<audio::AudioResampler>(source_stream_info.ms_to_bytes(TRANSFER_BUFFER_DURATION_MS),
                                         this_resampler->audio_stream_info_.ms_to_bytes(TRANSFER_BUFFER_DURATION_MS));

  esp_err_t err = resampler->start(source_stream_info, this_resampler->audio_stream_info_, this_resampler->taps_,
                                   this_resampler->filters_);

  std::shared_ptr<RingBuffer> output_ring_buffer;
  if (err == ESP_OK) {
    std::shared_ptr<RingBuffer> temp_ring_buffer =
        RingBuffer::create(source_stream_info.ms_to_bytes(this_resampler->buffer_duration_ms_));

    if (temp_ring_buffer.use_count() == 0) {
      err = ESP_ERR_NO_MEM;
    } else {
      this_resampler->ring_buffer_ = temp_ring_buffer;
      resampler->add_source(this_resampler->ring_buffer_);

      // Create output ring buffer for resampled audio
      output_ring_buffer =
          RingBuffer::create(this_resampler->audio_stream_info_.ms_to_bytes(this_resampler->buffer_duration_ms_));
      if (output_ring_buffer.use_count() == 0) {
        err = ESP_ERR_NO_MEM;
      } else {
        std::weak_ptr<RingBuffer> output_ring_buffer_weak = output_ring_buffer;
        resampler->add_sink(output_ring_buffer_weak);
      }
    }
  }

  if (err == ESP_OK) {
    xEventGroupSetBits(this_resampler->event_group_, ResamplingEventGroupBits::TASK_RUNNING);
  } else if (err == ESP_ERR_NO_MEM) {
    xEventGroupSetBits(this_resampler->event_group_, ResamplingEventGroupBits::ERR_ESP_NO_MEM);
  } else if (err == ESP_ERR_NOT_SUPPORTED) {
    xEventGroupSetBits(this_resampler->event_group_, ResamplingEventGroupBits::ERR_ESP_NOT_SUPPORTED);
  }

  // Buffer for reading resampled audio from output ring buffer
  std::vector<uint8_t> output_data;
  const size_t output_chunk_size = this_resampler->audio_stream_info_.ms_to_bytes(TRANSFER_BUFFER_DURATION_MS);
  output_data.resize(output_chunk_size);

  while (err == ESP_OK) {
    if (xEventGroupGetBits(this_resampler->event_group_) & ResamplingEventGroupBits::COMMAND_STOP) {
      break;
    }

    int32_t ms_differential = 0;
    audio::AudioResamplerState resampler_state = resampler->resample(false, &ms_differential);

    // Read resampled audio from output ring buffer and send via callbacks
    if (output_ring_buffer != nullptr) {
      size_t available = output_ring_buffer->available();
      while (available > 0) {
        size_t to_read = std::min(available, output_chunk_size);
        size_t bytes_read = output_ring_buffer->read((void *) output_data.data(), to_read, 0);
        if (bytes_read > 0) {
          output_data.resize(bytes_read);
          this_resampler->data_callbacks_.call(output_data);
          output_data.resize(output_chunk_size);
          available = output_ring_buffer->available();
        } else {
          break;
        }
      }
    }

    if (resampler_state == audio::AudioResamplerState::FINISHED) {
      break;
    } else if (resampler_state == audio::AudioResamplerState::FAILED) {
      xEventGroupSetBits(this_resampler->event_group_, ResamplingEventGroupBits::ERR_ESP_FAIL);
      break;
    }
  }

  xEventGroupSetBits(this_resampler->event_group_, ResamplingEventGroupBits::TASK_STOPPING);
  resampler.reset();
  xEventGroupSetBits(this_resampler->event_group_, ResamplingEventGroupBits::TASK_STOPPED);

  while (true) {
    // Continuously delay until the loop method deletes the task
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

}  // namespace resampler
}  // namespace esphome

#endif
