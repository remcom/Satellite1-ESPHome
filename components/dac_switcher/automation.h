#pragma once

#include "esphome/core/automation.h"
#include "dac_switcher.h"

namespace esphome {
namespace dac_switcher {

template<typename... Ts> class SelectTas2780Action : public Action<Ts...>, public Parented<DacSwitcher> {
 public:
  void play(Ts... x) override { this->parent_->select_tas2780(); }
};

template<typename... Ts> class SelectPcm5122Action : public Action<Ts...>, public Parented<DacSwitcher> {
 public:
  void play(Ts... x) override { this->parent_->select_pcm5122(); }
};

}  // namespace dac_switcher
}  // namespace esphome
