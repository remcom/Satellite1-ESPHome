#pragma once

#include "esphome/components/i2c/i2c.h"
#include "esphome/core/component.h"
#include "esphome/core/hal.h"

#include "pd.h"

#ifdef USE_ESP_IDF

#ifdef USE_TEXT_SENSOR
#include "esphome/components/text_sensor/text_sensor.h"
#endif

namespace esphome::fusb302b {

enum class Fusb302State { UNATTACHED, ATTACHED, FAILED };

union FusbStatus {
  uint8_t bytes[7];
  struct {
    uint8_t status0a;
    uint8_t status1a;
    uint8_t interrupta;
    uint8_t interruptb;
    uint8_t status0;
    uint8_t status1;
    uint8_t interrupt;
  };
};

class FUSB302B : public PowerDelivery, public Component, public i2c::I2CDevice {
  friend void msg_reader_task(void *params);
  friend void trigger_task(void *params);
  friend void fusb302b_isr_handler(void *arg);

 public:
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::HARDWARE; }
  void loop() override;
  void on_shutdown() override;

  bool send_message(const PDMsg &msg) override;
  bool read_message(PDMsg &msg) override;
  bool read_status(FusbStatus &status);
  bool read_status_register(uint8_t reg, uint8_t &value);

  void set_irq_pin(int irq_pin) { this->irq_pin_ = irq_pin; }

  bool check_chip_id();
  bool enable_auto_crc();
  bool disable_auto_crc();

 protected:
  // CC line measurement runs as a non-blocking state machine across loop() iterations:
  // each measurement needs ~5 ms to settle, which must not block the main loop.
  enum class CcMeasurePhase : uint8_t { IDLE, SETTLE_CC1, SETTLE_CC2 };

  bool cc_read_bc_lvl_(uint8_t &lvl);
  void cc_measurement_failed_();
  void fusb_reset_unlocked_();
  void fusb_reset_();
  void check_status_();
  void publish() override {
    this->defer([this]() {
      this->state_callback_.call();
#ifdef USE_TEXT_SENSOR
      if (this->contract_sensor_ != nullptr) {
        // get_contract() copies the string under the contract lock (written from trigger task)
        std::string val = (this->state_ == PD_STATE_DISCONNECTED) ? "Detached" : this->get_contract();
        this->contract_sensor_->publish_state(val);
      }
#endif
    });
  }
  bool init_fusb_settings_();
  void cleanup_();

  Fusb302State hw_state_{Fusb302State::UNATTACHED};
  CcMeasurePhase cc_phase_{CcMeasurePhase::IDLE};
  uint32_t cc_phase_start_{0};
  uint32_t cc_retry_after_{0};
  uint8_t cc1_lvl_{0};
  // Set by the reader task when a VBUSOK interrupt reports VBUS gone; handled in loop()
  std::atomic<bool> vbus_lost_{false};
  uint32_t startup_delay_{0};
  int irq_pin_{0};

  SemaphoreHandle_t i2c_lock_{nullptr};
  TaskHandle_t reader_task_handle_{nullptr};
  TaskHandle_t process_task_handle_{nullptr};
  QueueHandle_t message_queue_{nullptr};
};

}  // namespace esphome::fusb302b

#endif  // USE_ESP_IDF
