#include "sat_gpio.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome::satellite1 {

static const char *const TAG = "Satellite1-GPIOs";

void Satellite1GPIOPin::digital_write(bool value) {
  if (this->parent_ == nullptr) {
    ESP_LOGE(TAG, "Parent not set for GPIO pin");
    return;
  }
  if (this->port_ != XMOSPort::OUTPUT_A) {
    ESP_LOGE(TAG, "Trying writing to read only port.");
    return;
  }
  uint8_t payload[2] = {this->pin_, static_cast<uint8_t>(value != this->inverted_)};
  this->parent_->transfer(DC_RESOURCE::GPIO_PORT_OUT_A, GPIO_SERVICER_CMD_SET_PIN, payload, 2);
}

bool Satellite1GPIOPin::digital_read() {
  if (this->parent_ == nullptr) {
    ESP_LOGE(TAG, "Parent not set for GPIO pin");
    return false;
  }
  DC_STATUS_REGISTER::RegisterId port_register;
  switch (this->port_) {
    case XMOSPort::INPUT_A:
      port_register = DC_STATUS_REGISTER::GPIO_PORT_IN_A;
      break;
    case XMOSPort::INPUT_B:
      port_register = DC_STATUS_REGISTER::GPIO_PORT_IN_B;
      break;
    case XMOSPort::OUTPUT_A:
      port_register = DC_STATUS_REGISTER::GPIO_PORT_OUT_A;
      break;
    default:
      ESP_LOGE(TAG, "Invalid port set.");
      return false;
      break;
  }
  this->parent_->request_status_register_update();
  uint8_t port_value = this->parent_->get_dc_status(port_register);
  bool bit_value = !!(port_value & (1 << this->pin_));
  return bit_value != this->inverted_;
}

static const char *port_str(XMOSPort port) {
  switch (port) {
    case XMOSPort::INPUT_A:
      return "INPUT_A";
    case XMOSPort::INPUT_B:
      return "INPUT_B";
    case XMOSPort::OUTPUT_A:
      return "OUTPUT_A";
    default:
      return "UNKNOWN";
  }
}

size_t Satellite1GPIOPin::dump_summary(char *buffer, size_t len) const {
  return buf_append_printf(buffer, len, 0, "%s pin %u via XMOS", port_str(this->port_), this->pin_);
}

}  // namespace esphome::satellite1
