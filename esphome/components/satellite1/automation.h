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

template<Satellite1State State> class Satellite1StateTrigger : public Trigger<> {
 public:
  explicit Satellite1StateTrigger(Satellite1 *sat1) {
    sat1->add_on_state_callback([this, sat1]() {
      if (sat1->get_state() == State)
        this->trigger();
    });
  }
};

using XMOSConnectedStateTrigger = Satellite1StateTrigger<SAT_XMOS_CONNECTED_STATE>;
using FlashConnectedStateTrigger = Satellite1StateTrigger<SAT_FLASH_CONNECTED_STATE>;

/// Fires when the XMOS dropped off (reset, leaving flash access mode) and detection restarts.
class XMOSDetachedStateTrigger : public Trigger<> {
 public:
  explicit XMOSDetachedStateTrigger(Satellite1 *sat1) {
    sat1->add_on_state_callback([this, sat1]() {
      if (sat1->get_state() == SAT_DETACHED_STATE && sat1->get_connection_attempts() == 0)
        this->trigger();
    });
  }
};

class XMOSNoResponseStateTrigger : public Trigger<> {
 public:
  explicit XMOSNoResponseStateTrigger(Satellite1 *sat1) {
    sat1->add_on_state_callback([this, sat1]() {
      if (sat1->get_state() == SAT_DETACHED_STATE && sat1->get_connection_attempts() == MAX_CONNECTION_ATTEMPTS)
        this->trigger();
    });
  }
};

}  // namespace esphome::satellite1
