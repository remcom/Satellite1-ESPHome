#pragma once

#include "esphome/components/gpio_expander/cached_gpio.h"
#include "esphome/components/spi/spi.h"
#include "esphome/core/component.h"
#include "esphome/core/gpio.h"
#include "esphome/core/helpers.h"

#include <cassert>
#include <functional>
#include <string>

namespace esphome::satellite1 {

static const uint8_t CONTROL_CMD_READ_BIT = 0x80;

static const uint8_t CONTROL_COMMAND_IGNORED_IN_DEVICE = 7;

static const uint8_t GPIO_SERVICER_RESID_PORT_IN_A = 211;
static const uint8_t GPIO_SERVICER_RESID_PORT_IN_B = 212;
static const uint8_t GPIO_SERVICER_RESID_PORT_OUT_A = 221;

static const uint8_t DFU_CONTROLLER_SERVICER_RESID = 240;

static const uint8_t MAX_CONNECTION_ATTEMPTS = 3;

static const uint8_t XMOS_FW_VERSION_LEN = 5;

static const uint8_t GPIO_SERVICER_CMD_SET_PIN = 0x02;

/// GPIO pin numbers: 8 pins per XMOS port, in the order INPUT_A, INPUT_B, OUTPUT_A.
static const uint8_t XMOS_GPIO_PINS_PER_PORT = 8;
static const uint8_t XMOS_GPIO_NUM_PINS = 3 * XMOS_GPIO_PINS_PER_PORT;
static const uint8_t XMOS_GPIO_FIRST_OUTPUT_PIN = 2 * XMOS_GPIO_PINS_PER_PORT;

namespace DC_RESOURCE {
enum DcResourceEnum {
  CNTRL_ID = 1,
  DFU_CONTROLLER = DFU_CONTROLLER_SERVICER_RESID,
  GPIO_PORT_IN_A = GPIO_SERVICER_RESID_PORT_IN_A,
  GPIO_PORT_IN_B = GPIO_SERVICER_RESID_PORT_IN_B,
  GPIO_PORT_OUT_A = GPIO_SERVICER_RESID_PORT_OUT_A
};
}

namespace DC_RET_STATUS {
enum DcRetStatusEnum { CMD_SUCCESS = 0, PAYLOAD_AVAILABLE = 23 };
}

namespace DC_STATUS_REGISTER {
enum RegisterId {
  DEVICE_STATUS = 0,
  GPIO_PORT_IN_A = 1,
  GPIO_PORT_IN_B = 2,
  GPIO_PORT_OUT_A = 3,

  REGISTER_LEN = 4
};
}

namespace DC_DFU_CMD {
enum DcDfuCmdId {
  GET_VERSION = (88 | CONTROL_CMD_READ_BIT),
};
}

enum Satellite1State : uint8_t {
  SAT_DETACHED_STATE,
  SAT_XMOS_CONNECTED_STATE,
  SAT_FLASH_CONNECTED_STATE,
};

class Satellite1 : public Component,
                   public spi::SPIDevice<spi::BIT_ORDER_MSB_FIRST, spi::CLOCK_POLARITY_LOW, spi::CLOCK_PHASE_LEADING,
                                         spi::DATA_RATE_1KHZ>,
                   // One bank per XMOS GPIO port, all banks are read with a single status register update.
                   public gpio_expander::CachedGpioExpander<uint8_t, XMOS_GPIO_NUM_PINS> {
 public:
  std::string status_string();

  Satellite1State get_state() const { return this->state_; }
  uint8_t get_connection_attempts() const { return this->connection_attempts_; }
  const uint8_t *get_xmos_fw_version() const { return this->xmos_fw_version_; }

  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::IO; }
  void loop() override;

  /**
   * @brief Communicates with the XMOS device controller for resource-specific operations.
   *
   * This function facilitates communication with the XMOS device controller, which exposes
   * various resources that can be accessed via SPI commands. The function supports both
   * write and read operations:
   * - **Write Commands:** Data in the `payload` buffer is sent to the specified resource.
   * - **Read Commands:** The function expects a response from the resource, which is written
   *   back into the `payload` buffer.
   *
   * The behavior is determined by the `command` parameter:
   * - Commands with the highest bit set (`0x80`) indicate a **read operation**, and the
   *   device's response will overwrite the `payload` buffer.
   * - Commands without the `0x80` bit perform a **write operation**, sending the `payload`
   *   data to the device without expecting a response in the `payload` buffer.
   *
   * The function also processes status reports from the device and updates the internal
   * status register if applicable.
   *
   * @param resource_id  Identifier for the target resource within the XMOS device controller.
   * @param command      Command specifying the operation (read or write) to be executed. The
   *                     highest bit (`0x80`) determines whether a response is expected.
   * @param payload      Pointer to the buffer containing the data to be sent. For read commands,
   *                     this buffer is updated with the response from the device.
   * @param payload_len  Length of the payload buffer in bytes.
   *
   * @return             A boolean value indicating the success or failure of the operation:
   *                     - `true`: The operation was successful. The payload buffer and/or status
   *                       register have been updated as needed.
   *                     - `false`: The operation failed, possibly due to ignored commands, lack
   *                       of response from the device, or disabled SPI communication.
   *
   * @details
   * - The XMOS device may return status reports during communication. If the response indicates
   *   a status report (resource ID matches `DC_RESOURCE::CNTRL_ID`), the internal status register
   *   is updated.
   */
  bool transfer(uint8_t resource_id, uint8_t command, uint8_t *payload, uint8_t payload_len);

  /**
   * @brief Requests an update to the XMOS device controller's status registers.
   *
   * This function sends a zero-byte command to the XMOS device controller to trigger
   * a status register update. The device responds by sending its current status
   * registers.
   *
   * @return             A boolean value indicating the success or failure of the operation:
   *                     - `true`: The status registers were successfully updated.
   *                     - `false`: The update request failed, potentially due to communication
   *                       issues or ignored commands.
   */
  bool request_status_register_update();

  /**
   * @brief Retrieves the cached value of a specific status register.
   *
   * This function returns the current value of a specific status register from the
   * locally cached `dc_status_register_` buffer. It does not trigger a status
   * update from the XMOS device controller. To ensure the cached values are up to
   * date, call `request_status_register_update` before using this function.
   *
   * @param reg          The identifier of the status register to query as defined in
   *                     `DC_STATUS_REGISTER`.
   *
   * @return             The cached value of the requested status register as an 8-bit
   *                     unsigned integer.
   */
  uint8_t get_dc_status(DC_STATUS_REGISTER::RegisterId reg) const {
    assert(reg < DC_STATUS_REGISTER::REGISTER_LEN);
    return this->dc_status_register_[reg];
  }

  void set_spi_flash_direct_access_mode(bool enable);

  void set_xmos_rst_pin(GPIOPin *xmos_rst_pin) { this->xmos_rst_pin_ = xmos_rst_pin; }

  /// The callback receives the new state and the number of connection attempts made so far.
  template<typename F> void add_on_state_callback(F &&callback) {
    this->state_callback_.add(std::forward<F>(callback));
  }

  /**
   * @brief Pulses the XMOS reset line and re-detects the XMOS afterwards.
   *
   * Non-blocking: the reset line is released from a scheduler timeout. Refused while the
   * XMOS flash is being accessed directly, since that holds the XMOS in reset itself, and
   * while a previous reset is still in progress.
   *
   * @param on_done  Called once detection after the reset has finished (XMOS connected or
   *                 not responding), when the reset is aborted by entering flash access mode,
   *                 or right away if the reset is refused.
   */
  void xmos_hardware_reset(std::function<void()> &&on_done = nullptr);

 protected:
  bool dfu_get_fw_version_();
  bool check_for_xmos_();
  /// Restart XMOS detection from scratch (used after leaving flash mode or after a reset).
  void restart_detection_();
  /// Ends a pending XMOS reset and calls its on_done callback.
  void finish_xmos_reset_();
  /// Writes the cached XMOS firmware version as "vX.Y.Z[-pre][.N]" into buffer.
  void format_fw_version_(char *buffer, size_t len) const;
  void notify_state_() { this->state_callback_.call(this->state_, this->connection_attempts_); }

  // CachedGpioExpander
  bool digital_read_hw(uint8_t pin) override;
  bool digital_read_cache(uint8_t pin) override;
  void digital_write_hw(uint8_t pin, bool value) override;

  CallbackManager<void(Satellite1State, uint8_t)> state_callback_{};

  Satellite1State state_{SAT_DETACHED_STATE};
  uint8_t xmos_fw_version_[XMOS_FW_VERSION_LEN]{};
  uint8_t connection_attempts_{0};

  uint32_t last_attempt_timestamp_{0};

  uint8_t dc_status_register_[DC_STATUS_REGISTER::REGISTER_LEN]{};
  bool spi_flash_direct_access_enabled_{false};

  uint8_t spi_transfer_buffer_[256 + 3];  // Reusable buffer for SPI transfers

  GPIOPin *xmos_rst_pin_{nullptr};
  bool xmos_reset_in_progress_{false};
  std::function<void()> xmos_reset_done_callback_{};
};

class Satellite1SPIService : public Parented<Satellite1> {
 protected:
  uint8_t transfer_byte_(uint8_t byte) { return this->parent_->transfer_byte(byte); }
  void enable_() { this->parent_->enable(); }
  void disable_() { this->parent_->disable(); }
};

}  // namespace esphome::satellite1
