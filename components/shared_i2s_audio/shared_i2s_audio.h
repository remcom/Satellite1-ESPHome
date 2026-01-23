#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

#ifdef USE_ESP32
#include <driver/i2s.h>

namespace esphome {
namespace shared_i2s_audio {

class SharedI2SAudioComponent : public Component {
 public:
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::HARDWARE - 1.0f; }

  // Configuration setters
  void set_i2s_port(i2s_port_t port) { this->port_ = port; }
  void set_bclk_pin(uint8_t pin) { this->bclk_pin_ = pin; }
  void set_lrclk_pin(uint8_t pin) { this->lrclk_pin_ = pin; }
  void set_mclk_pin(int pin) { this->mclk_pin_ = pin; }

  // Audio configuration
  void set_sample_rate(uint32_t sample_rate) { this->sample_rate_ = sample_rate; }
  void set_bits_per_sample(i2s_bits_per_sample_t bits) { this->bits_per_sample_ = bits; }
  void set_channel_format(i2s_channel_fmt_t format) { this->channel_format_ = format; }
  void set_use_apll(bool use_apll) { this->use_apll_ = use_apll; }
  void set_buffer_count(int count) { this->dma_buf_count_ = count; }
  void set_buffer_length(int length) { this->dma_buf_len_ = length; }

  // Getters
  i2s_port_t get_port() const { return this->port_; }
  bool is_initialized() const { return this->initialized_; }

  // Control methods
  bool reconfigure_for_speaker(uint8_t dout_pin);
  bool reconfigure_for_microphone(uint8_t din_pin);
  bool stop_i2s();

 protected:
  i2s_port_t port_{I2S_NUM_0};
  uint8_t bclk_pin_{-1};
  uint8_t lrclk_pin_{-1};
  int mclk_pin_{-1};

  uint32_t sample_rate_{16000};
  i2s_bits_per_sample_t bits_per_sample_{I2S_BITS_PER_SAMPLE_16BIT};
  i2s_channel_fmt_t channel_format_{I2S_CHANNEL_FMT_ONLY_RIGHT};
  bool use_apll_{false};
  int dma_buf_count_{8};
  int dma_buf_len_{256};

  bool initialized_{false};
  i2s_mode_t current_mode_{I2S_MODE_MASTER};
};

}  // namespace shared_i2s_audio
}  // namespace esphome

#endif  // USE_ESP32
