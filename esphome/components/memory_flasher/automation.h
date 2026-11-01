#pragma once
#include "memory_flasher.h"

#include "esphome/core/automation.h"

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

template<typename... Ts> class InProgressCondition : public Condition<Ts...>, public Parented<MemoryFlasher> {
 public:
  bool check(const Ts &...x) override { return this->parent_->get_state() != FLASHER_IDLE; }
};

}  // namespace esphome::memory_flasher
