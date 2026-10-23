#pragma once
#include "memory_flasher.h"

#include "esphome/core/automation.h"

#include <type_traits>

namespace esphome::memory_flasher {

template<typename... Ts> class FlashAction : public Action<Ts...> {
 public:
  explicit FlashAction(MemoryFlasher *parent) : parent_(parent) {}
  TEMPLATABLE_VALUE(std::string, md5_url)
  TEMPLATABLE_VALUE(std::string, md5)
  TEMPLATABLE_VALUE(std::string, url)

  void play(const Ts &...x) override {
    this->parent_->flash_remote_image(this->url_.value(x...), this->md5_.value_or(x..., ""),
                                      this->md5_url_.value_or(x..., ""));
  }

 protected:
  MemoryFlasher *parent_;
};

template<typename... Ts> class FlashEmbeddedAction : public Action<Ts...>, public Parented<MemoryFlasher> {
 public:
  void play(const Ts &...x) override { this->parent_->flash_embedded_image(); }
};

template<typename... Ts> class EraseMemoryAction : public Action<Ts...>, public Parented<MemoryFlasher> {
 public:
  void play(const Ts &...x) override { this->parent_->erase_memory(); }
};

/// Callback forwarders for build_callback_automation(), must stay pointer-sized (see TriggerForwarder).
template<FlasherState State> struct FlasherStateForwarder {
  Automation<> *automation;
  void operator()(FlasherState state, FlasherState previous_state, FlasherAction action) const {
    if (state == State)
      this->automation->trigger();
  }
};

/// Fires once when erasing starts, not for the progress updates published while erasing.
struct FlashingStartedForwarder {
  Automation<> *automation;
  void operator()(FlasherState state, FlasherState previous_state, FlasherAction action) const {
    if (state == FLASHER_ERASING && previous_state != FLASHER_ERASING)
      this->automation->trigger();
  }
};

struct ErasingDoneForwarder {
  Automation<> *automation;
  void operator()(FlasherState state, FlasherState previous_state, FlasherAction action) const {
    if (state == FLASHER_SUCCESS_STATE && action == ACTION_FULL_ERASE)
      this->automation->trigger();
  }
};

static_assert(sizeof(FlasherStateForwarder<FLASHER_FLASHING>) <= sizeof(void *));
static_assert(std::is_trivially_copyable_v<FlasherStateForwarder<FLASHER_FLASHING>>);
static_assert(sizeof(FlashingStartedForwarder) <= sizeof(void *));
static_assert(std::is_trivially_copyable_v<FlashingStartedForwarder>);
static_assert(sizeof(ErasingDoneForwarder) <= sizeof(void *));
static_assert(std::is_trivially_copyable_v<ErasingDoneForwarder>);

template<typename... Ts> class InProgressCondition : public Condition<Ts...>, public Parented<MemoryFlasher> {
 public:
  bool check(const Ts &...x) override { return this->parent_->get_state() != FLASHER_IDLE; }
};

}  // namespace esphome::memory_flasher
