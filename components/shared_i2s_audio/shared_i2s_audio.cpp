#include "shared_i2s_audio.h"

#ifdef USE_ESP32

#include "esphome/core/log.h"

namespace esphome {
namespace shared_i2s_audio {

static const char *const TAG = "shared_i2s_audio";

void SharedI2SAudioComponent::setup() {
  ESP_LOGCONFIG(TAG, "Setting up Shared I2S Audio...");

  // Initial setup just validates pins, actual I2S init happens when first device configures
  if (this->bclk_pin_ == (uint8_t) -1 || this->lrclk_pin_ == (uint8_t) -1) {
    ESP_LOGE(TAG, "BCLK and LRCLK pins must be configured");
    this->mark_failed();
    return;
  }

  ESP_LOGCONFIG(TAG, "Shared I2S Audio initialized (port: %d)", this->port_);
}

void SharedI2SAudioComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "Shared I2S Audio:");
  ESP_LOGCONFIG(TAG, "  Port: %d", this->port_);
  ESP_LOGCONFIG(TAG, "  BCLK Pin: %d", this->bclk_pin_);
  ESP_LOGCONFIG(TAG, "  LRCLK Pin: %d", this->lrclk_pin_);
  if (this->mclk_pin_ != -1) {
    ESP_LOGCONFIG(TAG, "  MCLK Pin: %d", this->mclk_pin_);
  }
  ESP_LOGCONFIG(TAG, "  Sample Rate: %d Hz", this->sample_rate_);
  ESP_LOGCONFIG(TAG, "  Bits per Sample: %d", this->bits_per_sample_);
  ESP_LOGCONFIG(TAG, "  DMA Buffers: %d x %d bytes", this->dma_buf_count_, this->dma_buf_len_);
}

bool SharedI2SAudioComponent::stop_i2s() {
  if (!this->initialized_) {
    return true;
  }

  ESP_LOGD(TAG, "Stopping I2S port %d", this->port_);

  esp_err_t err = i2s_stop(this->port_);
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "Failed to stop I2S: %s", esp_err_to_name(err));
    return false;
  }

  err = i2s_driver_uninstall(this->port_);
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "Failed to uninstall I2S driver: %s", esp_err_to_name(err));
    return false;
  }

  this->initialized_ = false;
  return true;
}

bool SharedI2SAudioComponent::reconfigure_for_speaker(uint8_t dout_pin) {
  ESP_LOGD(TAG, "Reconfiguring I2S for speaker (DOUT: %d)", dout_pin);

  // Stop current I2S if running
  if (this->initialized_) {
    if (!this->stop_i2s()) {
      ESP_LOGE(TAG, "Failed to stop I2S before reconfiguration");
      return false;
    }
  }

  // Configure I2S for TX (speaker output)
  i2s_config_t i2s_config = {
      .mode = (i2s_mode_t) (I2S_MODE_MASTER | I2S_MODE_TX),
      .sample_rate = this->sample_rate_,
      .bits_per_sample = this->bits_per_sample_,
      .channel_format = this->channel_format_,
      .communication_format = I2S_COMM_FORMAT_STAND_I2S,
      .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
      .dma_buf_count = this->dma_buf_count_,
      .dma_buf_len = this->dma_buf_len_,
      .use_apll = this->use_apll_,
      .tx_desc_auto_clear = true,
      .fixed_mclk = 0,
      .mclk_multiple = I2S_MCLK_MULTIPLE_256,
      .bits_per_chan = I2S_BITS_PER_CHAN_DEFAULT,
  };

  esp_err_t err = i2s_driver_install(this->port_, &i2s_config, 0, nullptr);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to install I2S driver: %s", esp_err_to_name(err));
    return false;
  }

  i2s_pin_config_t pin_config = {
      .bck_io_num = this->bclk_pin_,
      .ws_io_num = this->lrclk_pin_,
      .data_out_num = (int) dout_pin,
      .data_in_num = I2S_PIN_NO_CHANGE,
  };

  if (this->mclk_pin_ != -1) {
    pin_config.mck_io_num = this->mclk_pin_;
  } else {
    pin_config.mck_io_num = I2S_PIN_NO_CHANGE;
  }

  err = i2s_set_pin(this->port_, &pin_config);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to set I2S pins: %s", esp_err_to_name(err));
    i2s_driver_uninstall(this->port_);
    return false;
  }

  this->initialized_ = true;
  this->current_mode_ = (i2s_mode_t) (I2S_MODE_MASTER | I2S_MODE_TX);

  ESP_LOGD(TAG, "I2S configured for speaker");
  return true;
}

bool SharedI2SAudioComponent::reconfigure_for_microphone(uint8_t din_pin) {
  ESP_LOGD(TAG, "Reconfiguring I2S for microphone (DIN: %d)", din_pin);

  // Stop current I2S if running
  if (this->initialized_) {
    if (!this->stop_i2s()) {
      ESP_LOGE(TAG, "Failed to stop I2S before reconfiguration");
      return false;
    }
  }

  // Configure I2S for RX (microphone input)
  i2s_config_t i2s_config = {
      .mode = (i2s_mode_t) (I2S_MODE_MASTER | I2S_MODE_RX),
      .sample_rate = this->sample_rate_,
      .bits_per_sample = this->bits_per_sample_,
      .channel_format = this->channel_format_,
      .communication_format = I2S_COMM_FORMAT_STAND_I2S,
      .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
      .dma_buf_count = this->dma_buf_count_,
      .dma_buf_len = this->dma_buf_len_,
      .use_apll = this->use_apll_,
      .tx_desc_auto_clear = false,
      .fixed_mclk = 0,
      .mclk_multiple = I2S_MCLK_MULTIPLE_256,
      .bits_per_chan = I2S_BITS_PER_CHAN_DEFAULT,
  };

  esp_err_t err = i2s_driver_install(this->port_, &i2s_config, 0, nullptr);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to install I2S driver: %s", esp_err_to_name(err));
    return false;
  }

  i2s_pin_config_t pin_config = {
      .bck_io_num = this->bclk_pin_,
      .ws_io_num = this->lrclk_pin_,
      .data_out_num = I2S_PIN_NO_CHANGE,
      .data_in_num = (int) din_pin,
  };

  if (this->mclk_pin_ != -1) {
    pin_config.mck_io_num = this->mclk_pin_;
  } else {
    pin_config.mck_io_num = I2S_PIN_NO_CHANGE;
  }

  err = i2s_set_pin(this->port_, &pin_config);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to set I2S pins: %s", esp_err_to_name(err));
    i2s_driver_uninstall(this->port_);
    return false;
  }

  this->initialized_ = true;
  this->current_mode_ = (i2s_mode_t) (I2S_MODE_MASTER | I2S_MODE_RX);

  ESP_LOGD(TAG, "I2S configured for microphone");
  return true;
}

}  // namespace shared_i2s_audio
}  // namespace esphome

#endif  // USE_ESP32
