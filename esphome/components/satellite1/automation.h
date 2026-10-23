#pragma once
#include "satellite1.h"

#include "esphome/core/automation.h"

#include <type_traits>

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

/// Callback forwarders for build_callback_automation(), must stay pointer-sized (see TriggerForwarder).
template<Satellite1State State> struct Satellite1StateForwarder {
  Automation<> *automation;
  void operator()(Satellite1State state, uint8_t connection_attempts) const {
    if (state == State)
      this->automation->trigger();
  }
};

/// Fires when the XMOS dropped off (reset, leaving flash access mode) and detection restarts.
struct XMOSDetachedForwarder {
  Automation<> *automation;
  void operator()(Satellite1State state, uint8_t connection_attempts) const {
    if (state == SAT_DETACHED_STATE && connection_attempts == 0)
      this->automation->trigger();
  }
};

/// Fires when detection gave up.
struct XMOSNoResponseForwarder {
  Automation<> *automation;
  void operator()(Satellite1State state, uint8_t connection_attempts) const {
    if (state == SAT_DETACHED_STATE && connection_attempts == MAX_CONNECTION_ATTEMPTS)
      this->automation->trigger();
  }
};

static_assert(sizeof(Satellite1StateForwarder<SAT_XMOS_CONNECTED_STATE>) <= sizeof(void *));
static_assert(std::is_trivially_copyable_v<Satellite1StateForwarder<SAT_XMOS_CONNECTED_STATE>>);
static_assert(sizeof(XMOSDetachedForwarder) <= sizeof(void *));
static_assert(std::is_trivially_copyable_v<XMOSDetachedForwarder>);
static_assert(sizeof(XMOSNoResponseForwarder) <= sizeof(void *));
static_assert(std::is_trivially_copyable_v<XMOSNoResponseForwarder>);

}  // namespace esphome::satellite1
