#pragma once
#include "satellite1.h"

#include "esphome/core/automation.h"

namespace esphome::satellite1 {

template<typename... Ts> class XMOSHardwareResetAction : public Action<Ts...>, public Parented<Satellite1> {
 public:
  void play_complex(const Ts &...x) override {
    this->num_running_++;
    // Continue once the XMOS was re-detected (or detection gave up), so following actions can talk to it.
    // `mutable`: see DelayAction, captured copies are passed on to play_next_(const Ts &...).
    this->parent_->xmos_hardware_reset([this, x...]() mutable { this->play_next_(x...); });
  }

  void play(const Ts &...x) override { /* ignore - see play_complex */ }
};

}  // namespace esphome::satellite1
