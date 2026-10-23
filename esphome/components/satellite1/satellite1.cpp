#include "satellite1.h"
#include "esphome/core/application.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

#include <cstring>

namespace esphome::satellite1 {

static const char *const TAG = "Satellite1";

static constexpr uint32_t CONNECTION_RETRY_INTERVAL_MS = 1000;
static constexpr uint32_t XMOS_RESET_PULSE_MS = 100;
static constexpr uint32_t XMOS_RESET_TIMEOUT_ID = 0;
static constexpr size_t FW_VERSION_STR_MAX_LEN = 32;

void Satellite1::setup() {
  this->spi_setup();
  this->enable();
  this->transfer_byte(0);
  this->disable();

  if (this->xmos_rst_pin_ != nullptr) {
    this->xmos_rst_pin_->setup();
  }

  memset(this->xmos_fw_version_, 0, sizeof(this->xmos_fw_version_));
  this->dfu_get_fw_version_();
}

void Satellite1::dump_config() {
  ESP_LOGCONFIG(TAG, "Satellite1:");
  LOG_PIN("  XMOS Reset Pin: ", this->xmos_rst_pin_);
  ESP_LOGCONFIG(TAG, "  XMOS Status: %s", this->status_string().c_str());
}

void Satellite1::loop() {
  // Only the detection phase needs the loop; it is re-enabled by restart_detection_().
  if (this->state_ != SAT_DETACHED_STATE) {
    this->disable_loop();
    return;
  }

  const uint32_t now = App.get_loop_component_start_time();
  if (now - this->last_attempt_timestamp_ <= CONNECTION_RETRY_INTERVAL_MS) {
    return;
  }
  this->last_attempt_timestamp_ = now;

  if (this->connection_attempts_ >= MAX_CONNECTION_ATTEMPTS) {
    // Give up. connection_attempts_ stays at MAX_CONNECTION_ATTEMPTS, which is what
    // XMOSNoResponseStateTrigger checks for. Disable the loop before notifying so a callback
    // that restarts detection synchronously isn't overridden.
    ESP_LOGW(TAG, "XMOS not responding after %u attempts", MAX_CONNECTION_ATTEMPTS);
    this->disable_loop();
    this->notify_state_();
    this->finish_xmos_reset_();
    return;
  }

  this->connection_attempts_++;
  if (this->check_for_xmos_()) {
    this->state_ = SAT_XMOS_CONNECTED_STATE;
    this->connection_attempts_ = 0;
    this->disable_loop();
    this->notify_state_();
    this->finish_xmos_reset_();
  }
}

static const char *prerelease_str(uint8_t pre_idx) {
  switch (pre_idx) {
    case 1:
      return "alpha";
    case 2:
      return "beta";
    case 3:
      return "rc";
    case 4:
      return "dev";
    case 0:  // fallthrough
    default:
      return "";
  }
}

void Satellite1::format_fw_version_(char *buffer, size_t len) const {
  const uint8_t *v = this->xmos_fw_version_;
  size_t pos = buf_append_printf(buffer, len, 0, "v%u.%u.%u", v[0], v[1], v[2]);
  const char *pre_release = prerelease_str(v[3]);
  if (pre_release[0] != '\0') {
    pos = buf_append_printf(buffer, len, pos, "-%s", pre_release);
  }
  if (v[4]) {
    buf_append_printf(buffer, len, pos, ".%u", v[4]);
  }
}

std::string Satellite1::status_string() {
  switch (this->state_) {
    case SAT_XMOS_CONNECTED_STATE: {
      char buffer[FW_VERSION_STR_MAX_LEN];
      this->format_fw_version_(buffer, sizeof(buffer));
      return buffer;
    }
    case SAT_FLASH_CONNECTED_STATE:
      return "Flashing Mode";
    case SAT_DETACHED_STATE:
    default:
      if (this->connection_attempts_ < MAX_CONNECTION_ATTEMPTS) {
        return "Detecting XMOS";
      }
      return "XMOS not responding";
  }
}

bool Satellite1::request_status_register_update() {
  bool ret = this->transfer(0, 0, nullptr, 0);
  return ret;
}

bool Satellite1::transfer(uint8_t resource_id, uint8_t command, uint8_t *payload, uint8_t payload_len) {
  if (this->spi_flash_direct_access_enabled_) {
    return false;
  }
  if (payload == nullptr && payload_len > 0) {
    return false;
  }

  // Use class member buffer instead of stack allocation
  uint8_t *send_recv_buf = this->spi_transfer_buffer_;
  memset(send_recv_buf, 0, sizeof(this->spi_transfer_buffer_));

  int status_report_dummies = std::max<int>(0, DC_STATUS_REGISTER::REGISTER_LEN - payload_len - 1);

  int attempts = 3;
  do {
    send_recv_buf[0] = resource_id;
    send_recv_buf[1] = command;
    send_recv_buf[2] = payload_len + !!(command & CONTROL_CMD_READ_BIT);
    if (payload_len > 0) {
      memcpy(&send_recv_buf[3], payload, payload_len);
    }
    this->enable();
    this->transfer_array(&send_recv_buf[0], payload_len + 3 + status_report_dummies);
    this->disable();
    vTaskDelay(1);
  } while (send_recv_buf[0] == CONTROL_COMMAND_IGNORED_IN_DEVICE && attempts-- > 0);

  if (send_recv_buf[0] == CONTROL_COMMAND_IGNORED_IN_DEVICE) {
    return false;
  }

  // XMOS not responding at all
  if ((send_recv_buf[0] + send_recv_buf[1] + send_recv_buf[2]) == 0) {
    return false;
  }

  // Got status register report
  if (send_recv_buf[0] == DC_RESOURCE::CNTRL_ID && send_recv_buf[1] != DC_RET_STATUS::PAYLOAD_AVAILABLE) {
    memcpy(this->dc_status_register_, &send_recv_buf[2], DC_STATUS_REGISTER::REGISTER_LEN);
  }

  if (command & CONTROL_CMD_READ_BIT) {
    attempts = 3;
    do {
      memset(send_recv_buf, 0, payload_len + 3);
      this->enable();
      this->transfer_array(&send_recv_buf[0], payload_len + 3);
      this->disable();
      vTaskDelay(1);
    } while (send_recv_buf[0] == CONTROL_COMMAND_IGNORED_IN_DEVICE && attempts-- > 0);

    if (send_recv_buf[0] == CONTROL_COMMAND_IGNORED_IN_DEVICE) {
      return false;
    }

    if (payload_len > 0) {
      memcpy(payload, &send_recv_buf[1], payload_len);
    }
  }

  return true;
}

bool Satellite1::digital_read_hw(uint8_t pin) {
  // A failed update keeps the last known port values, which digital_read_cache() returns. Reporting
  // a failure instead would read as low, i.e. as pressed for the inverted buttons.
  this->request_status_register_update();
  return true;
}

bool Satellite1::digital_read_cache(uint8_t pin) {
  // The GPIO_PORT_* status registers follow the pin banks: INPUT_A, INPUT_B, OUTPUT_A
  const uint8_t port_value =
      this->dc_status_register_[DC_STATUS_REGISTER::GPIO_PORT_IN_A + pin / XMOS_GPIO_PINS_PER_PORT];
  return (port_value & (1 << (pin % XMOS_GPIO_PINS_PER_PORT))) != 0;
}

void Satellite1::digital_write_hw(uint8_t pin, bool value) {
  if (pin < XMOS_GPIO_FIRST_OUTPUT_PIN) {
    ESP_LOGE(TAG, "Pin %u is on a read only port", pin);
    return;
  }
  uint8_t payload[2] = {static_cast<uint8_t>(pin % XMOS_GPIO_PINS_PER_PORT), value};
  this->transfer(DC_RESOURCE::GPIO_PORT_OUT_A, GPIO_SERVICER_CMD_SET_PIN, payload, sizeof(payload));
}

void Satellite1::set_spi_flash_direct_access_mode(bool enable) {
  if (this->xmos_rst_pin_ == nullptr) {
    ESP_LOGE(TAG, "XMOS reset pin not set, can't switch flash access mode");
    return;
  }
  // Holding the XMOS in reset hands the SPI flash over to the ESP32.
  this->cancel_timeout(XMOS_RESET_TIMEOUT_ID);
  this->xmos_rst_pin_->digital_write(enable);
  const bool was_enabled = this->spi_flash_direct_access_enabled_;
  this->spi_flash_direct_access_enabled_ = enable;
  if (enable) {
    this->state_ = SAT_FLASH_CONNECTED_STATE;
    this->notify_state_();
    // The XMOS is held in reset for flash access, a pending reset won't be followed by a detection.
    this->finish_xmos_reset_();
  } else if (was_enabled) {
    this->restart_detection_();
  }
}

void Satellite1::restart_detection_() {
  this->state_ = SAT_DETACHED_STATE;
  this->connection_attempts_ = 0;
  // Give the XMOS time to boot before the first probe.
  this->last_attempt_timestamp_ = millis();
  this->enable_loop();
  this->notify_state_();
}

void Satellite1::finish_xmos_reset_() {
  this->xmos_reset_in_progress_ = false;
  if (this->xmos_reset_done_callback_) {
    auto on_done = std::move(this->xmos_reset_done_callback_);
    this->xmos_reset_done_callback_ = nullptr;
    on_done();
  }
}

bool Satellite1::dfu_get_fw_version_() {
  // Read command: the payload is only used as receive buffer, but it is clocked out
  // first, so don't send uninitialized stack memory.
  uint8_t version_resp[XMOS_FW_VERSION_LEN]{};
  if (!this->transfer(DC_RESOURCE::DFU_CONTROLLER, DC_DFU_CMD::GET_VERSION, version_resp, sizeof(version_resp))) {
    ESP_LOGW(TAG, "Requesting XMOS version failed");
    return false;
  }

  memcpy(this->xmos_fw_version_, version_resp, sizeof(this->xmos_fw_version_));
  char buffer[FW_VERSION_STR_MAX_LEN];
  this->format_fw_version_(buffer, sizeof(buffer));
  ESP_LOGI(TAG, "XMOS Firmware Version: %s", buffer);

  return true;
}

bool Satellite1::check_for_xmos_() {
  if (!this->dfu_get_fw_version_()) {
    return false;
  }
  const uint8_t compare_zeros[XMOS_FW_VERSION_LEN] = {0};
  return (memcmp(this->xmos_fw_version_, compare_zeros, sizeof(compare_zeros)) != 0);
}

void Satellite1::xmos_hardware_reset(std::function<void()> &&on_done) {
  if (this->xmos_rst_pin_ == nullptr) {
    ESP_LOGE(TAG, "XMOS reset pin not set, can't reset XMOS");
  } else if (this->spi_flash_direct_access_enabled_) {
    ESP_LOGW(TAG, "XMOS flash is being accessed, refusing to reset XMOS");
  } else if (this->xmos_reset_in_progress_) {
    ESP_LOGW(TAG, "XMOS reset already in progress");
  } else {
    ESP_LOGI(TAG, "Resetting XMOS");
    this->xmos_reset_in_progress_ = true;
    this->xmos_reset_done_callback_ = std::move(on_done);
    this->xmos_rst_pin_->digital_write(true);
    // Report the XMOS as detached right away. The first probe is only sent
    // CONNECTION_RETRY_INTERVAL_MS after the reset line was released.
    this->restart_detection_();
    this->set_timeout(XMOS_RESET_TIMEOUT_ID, XMOS_RESET_PULSE_MS, [this]() {
      this->xmos_rst_pin_->digital_write(false);
      this->last_attempt_timestamp_ = millis();
    });
    return;
  }
  if (on_done) {
    on_done();
  }
}

}  // namespace esphome::satellite1
