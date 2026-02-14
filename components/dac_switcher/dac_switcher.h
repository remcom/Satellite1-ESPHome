#pragma once

#include "esphome/components/audio_dac/audio_dac.h"
#include "esphome/core/component.h"

namespace esphome {
namespace dac_switcher {

enum DacSelection : uint8_t {
  DAC_TAS2780 = 0,
  DAC_PCM5122 = 1,
};

class DacSwitcher : public audio_dac::AudioDac, public Component {
 public:
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

  // AudioDac interface - pure pass-through to active DAC
  bool set_mute_off() override;
  bool set_mute_on() override;
  bool set_volume(float volume) override;
  bool is_muted() override { return this->is_muted_; }
  float volume() override { return this->volume_; }

  // Configuration methods
  void set_tas2780(audio_dac::AudioDac *dac) { this->tas2780_ = dac; }
  void set_pcm5122(audio_dac::AudioDac *dac) { this->pcm5122_ = dac; }

  // Switching methods
  void select_tas2780();
  void select_pcm5122();

  DacSelection get_active_dac() const { return this->active_dac_; }

 protected:
  audio_dac::AudioDac *tas2780_{nullptr};
  audio_dac::AudioDac *pcm5122_{nullptr};
  DacSelection active_dac_{DAC_TAS2780};  // Default to TAS2780
  float volume_{0.0f};
  bool is_muted_{false};

  audio_dac::AudioDac *get_active_dac_();
};

}  // namespace dac_switcher
}  // namespace esphome
