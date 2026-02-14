#include "dac_switcher.h"
#include "esphome/core/log.h"

namespace esphome {
namespace dac_switcher {

static const char *const TAG = "dac_switcher";

void DacSwitcher::setup() {
  ESP_LOGCONFIG(TAG, "Setting up DAC Switcher...");

  if (this->tas2780_ == nullptr && this->pcm5122_ == nullptr) {
    ESP_LOGE(TAG, "No DACs configured!");
    this->mark_failed();
    return;
  }

  // Default to TAS2780 if available, else PCM5122
  if (this->tas2780_ != nullptr) {
    this->active_dac_ = DAC_TAS2780;
  } else {
    this->active_dac_ = DAC_PCM5122;
  }
}

void DacSwitcher::dump_config() {
  ESP_LOGCONFIG(TAG, "DAC Switcher:");
  ESP_LOGCONFIG(TAG, "  TAS2780: %s", this->tas2780_ != nullptr ? "configured" : "not configured");
  ESP_LOGCONFIG(TAG, "  PCM5122: %s", this->pcm5122_ != nullptr ? "configured" : "not configured");
  ESP_LOGCONFIG(TAG, "  Active DAC: %s", this->active_dac_ == DAC_TAS2780 ? "TAS2780" : "PCM5122");
}

audio_dac::AudioDac *DacSwitcher::get_active_dac_() {
  if (this->active_dac_ == DAC_TAS2780) {
    return this->tas2780_;
  }
  return this->pcm5122_;
}

bool DacSwitcher::set_mute_off() {
  this->is_muted_ = false;
  auto *dac = this->get_active_dac_();
  if (dac == nullptr) {
    return false;
  }
  return dac->set_mute_off();
}

bool DacSwitcher::set_mute_on() {
  this->is_muted_ = true;
  auto *dac = this->get_active_dac_();
  if (dac == nullptr) {
    return false;
  }
  return dac->set_mute_on();
}

bool DacSwitcher::set_volume(float volume) {
  this->volume_ = volume;
  auto *dac = this->get_active_dac_();
  if (dac == nullptr) {
    return false;
  }
  return dac->set_volume(volume);
}

void DacSwitcher::select_tas2780() {
  if (this->tas2780_ == nullptr) {
    ESP_LOGW(TAG, "Cannot select TAS2780: not configured");
    return;
  }

  if (this->active_dac_ == DAC_TAS2780) {
    ESP_LOGD(TAG, "TAS2780 already active");
    return;  // Already active
  }

  // Mute old DAC (PCM5122)
  if (this->pcm5122_ != nullptr) {
    this->pcm5122_->set_mute_on();
    ESP_LOGD(TAG, "Muted PCM5122");
  }

  // Switch to new DAC
  this->active_dac_ = DAC_TAS2780;
  ESP_LOGI(TAG, "Switched to TAS2780");

  // Apply current volume to new DAC
  this->tas2780_->set_volume(this->volume_);

  // Unmute if switcher isn't muted
  if (!this->is_muted_) {
    this->tas2780_->set_mute_off();
  } else {
    this->tas2780_->set_mute_on();
  }
}

void DacSwitcher::select_pcm5122() {
  if (this->pcm5122_ == nullptr) {
    ESP_LOGW(TAG, "Cannot select PCM5122: not configured");
    return;
  }

  if (this->active_dac_ == DAC_PCM5122) {
    ESP_LOGD(TAG, "PCM5122 already active");
    return;  // Already active
  }

  // Mute old DAC (TAS2780)
  if (this->tas2780_ != nullptr) {
    this->tas2780_->set_mute_on();
    ESP_LOGD(TAG, "Muted TAS2780");
  }

  // Switch to new DAC
  this->active_dac_ = DAC_PCM5122;
  ESP_LOGI(TAG, "Switched to PCM5122");

  // Apply current volume to new DAC
  this->pcm5122_->set_volume(this->volume_);

  // Unmute if switcher isn't muted
  if (!this->is_muted_) {
    this->pcm5122_->set_mute_off();
  } else {
    this->pcm5122_->set_mute_on();
  }
}

}  // namespace dac_switcher
}  // namespace esphome
