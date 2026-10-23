#pragma once

#include "esphome/core/gpio.h"

#include "satellite1.h"

namespace esphome::satellite1 {

/// A pin on one of the XMOS GPIO ports, see XMOS_GPIO_PINS_PER_PORT for the pin numbering.
class Satellite1GPIOPin : public GPIOPin, public Parented<Satellite1> {
 public:
  void setup() override {}
  void pin_mode(gpio::Flags flags) override {}
  bool digital_read() override { return this->parent_->digital_read(this->pin_) != this->inverted_; }
  void digital_write(bool value) override { this->parent_->digital_write(this->pin_, value != this->inverted_); }
  size_t dump_summary(char *buffer, size_t len) const override;

  void set_pin(uint8_t pin) { this->pin_ = pin; }
  void set_inverted(bool inverted) { this->inverted_ = inverted; }
  void set_flags(gpio::Flags flags) { this->flags_ = flags; }
  gpio::Flags get_flags() const override { return this->flags_; }

 protected:
  uint8_t pin_{0};
  bool inverted_{false};
  gpio::Flags flags_{};
};

}  // namespace esphome::satellite1
