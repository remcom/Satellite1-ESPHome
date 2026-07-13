#include "fusb302b.h"

#ifdef USE_ESP_IDF

#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#include <driver/gpio.h>

#include "fusb302_defines.h"

namespace esphome::fusb302b {

static const char *const TAG = "fusb302b";

static constexpr uint32_t CC_SETTLE_MS = 5;            // settle time after switching the measured CC line
static constexpr uint32_t CC_RETRY_INTERVAL_MS = 500;  // backoff between CC measurement attempts

enum Fusb302TxToken : uint8_t {
  TX_TOKEN_TXON = 0xA1,
  TX_TOKEN_SOP1 = 0x12,
  TX_TOKEN_SOP2 = 0x13,
  TX_TOKEN_SOP3 = 0x1B,
  TX_TOKEN_RESET1 = 0x15,
  TX_TOKEN_RESET2 = 0x16,
  TX_TOKEN_PACKSYM = 0x80,
  TX_TOKEN_JAM_CRC = 0xFF,
  TX_TOKEN_EOP = 0x14,
  TX_TOKEN_TXOFF = 0xFE,
};

// ISR handler — runs in ISR context, must be in IRAM
void IRAM_ATTR fusb302b_isr_handler(void *arg) {
  FUSB302B *fusb302b = static_cast<FUSB302B *>(arg);
  TaskHandle_t reader_task = fusb302b->reader_task_handle_;
  if (reader_task == nullptr) {
    return;  // reader task not created yet or already deleted
  }
  BaseType_t higher_priority_task_woken = pdFALSE;
  xTaskNotifyFromISR(reader_task, 0x01, eSetBits, &higher_priority_task_woken);
  if (higher_priority_task_woken == pdTRUE) {
    portYIELD_FROM_ISR();
  }
}

void msg_reader_task(void *params) {
  FUSB302B *fusb302b = static_cast<FUSB302B *>(params);
  uint32_t notification_value;
  FusbStatus regs;
  PDEventInfo event_info;
  PDMsg &msg = event_info.msg;

  fusb302b->enable_auto_crc();
  fusb302b->fusb_reset_();

  while (true) {
    xTaskNotifyWait(0x00, 0xFFFFFFFF, &notification_value, portMAX_DELAY);
    if (!fusb302b->read_status(regs)) {
      continue;
    }
    if ((regs.interrupt & FUSB_INTERRUPT_I_VBUSOK) && !(regs.status0 & FUSB_STATUS0_VBUSOK)) {
      // VBUS dropped: source detached. Flag it for loop() to update state and publish.
      fusb302b->vbus_lost_ = true;
    }
    if (regs.interruptb & FUSB_INTERRUPTB_I_GCRCSENT) {
      event_info.event = PD_EVENT_RECEIVED_MSG;
      while (!(regs.status1 & FUSB_STATUS1_RX_EMPTY)) {
        if (fusb302b->read_message(msg)) {
          xQueueSend(fusb302b->message_queue_, &event_info, 0);
        } else {
          ESP_LOGV(TAG, "Reading message failed");
        }
        fusb302b->read_status_register(FUSB_STATUS1, regs.status1);
      }
    }
    // These branches only differ by ESP_LOGV message; at lower LOG_LOCAL_LEVEL the macro compiles
    // away entirely, making the branches look identical.
    // NOLINTNEXTLINE(bugprone-branch-clone)
    if (regs.interrupta & FUSB_INTERRUPTA_I_HARDRST) {
      ESP_LOGV(TAG, "Hard reset received");
    } else if (regs.interrupta & FUSB_INTERRUPTA_I_SOFTRST) {
      ESP_LOGV(TAG, "Soft reset request received");
    } else if (regs.interrupta & FUSB_INTERRUPTA_I_RETRYFAIL) {
      ESP_LOGV(TAG, "Message not acknowledged (retry failed)");
    }
  }
}

void trigger_task(void *params) {
  FUSB302B *fusb302b = static_cast<FUSB302B *>(params);

  fusb302b->message_queue_ = xQueueCreate(5, sizeof(PDEventInfo));
  if (fusb302b->message_queue_ == nullptr) {
    ESP_LOGE(TAG, "Failed to create message queue");
    fusb302b->mark_failed();
    vTaskDelete(nullptr);
    return;
  }

  gpio_num_t irq_gpio_pin = static_cast<gpio_num_t>(fusb302b->irq_pin_);

  gpio_config_t io_conf = {};
  io_conf.pin_bit_mask = (1ULL << irq_gpio_pin);
  io_conf.mode = GPIO_MODE_INPUT;
  io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
  io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
  io_conf.intr_type = GPIO_INTR_NEGEDGE;
  gpio_config(&io_conf);

  gpio_set_intr_type(irq_gpio_pin, GPIO_INTR_NEGEDGE);
  esp_err_t err = gpio_install_isr_service(0);
  if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
    ESP_LOGE(TAG, "Failed to install GPIO ISR service: %s", esp_err_to_name(err));
    fusb302b->mark_failed();
    vTaskDelete(nullptr);
    return;
  }
  // Create the reader task before registering the ISR: an INT_N edge arriving in the
  // window between the two would otherwise notify a not-yet-existing task.
  BaseType_t ret = xTaskCreatePinnedToCore(msg_reader_task, "fusb302b_read", 4096, fusb302b, configMAX_PRIORITIES / 2,
                                           &fusb302b->reader_task_handle_, 1);
  if (ret != pdPASS) {
    ESP_LOGE(TAG, "Failed to create reader task");
    fusb302b->mark_failed();
    vQueueDelete(fusb302b->message_queue_);
    fusb302b->message_queue_ = nullptr;
    vTaskDelete(nullptr);
    return;
  }

  if (gpio_isr_handler_add(irq_gpio_pin, fusb302b_isr_handler, static_cast<void *>(fusb302b)) != ESP_OK) {
    ESP_LOGE(TAG, "Failed to add GPIO ISR handler");
    fusb302b->mark_failed();
    // Hold the I2C lock so the reader task isn't deleted mid-transaction
    bool locked = xSemaphoreTake(fusb302b->i2c_lock_, pdMS_TO_TICKS(100)) == pdTRUE;
    vTaskDelete(fusb302b->reader_task_handle_);
    fusb302b->reader_task_handle_ = nullptr;
    if (locked) {
      xSemaphoreGive(fusb302b->i2c_lock_);
    }
    vQueueDelete(fusb302b->message_queue_);
    fusb302b->message_queue_ = nullptr;
    vTaskDelete(nullptr);
    return;
  }

  PDEventInfo event_info;
  while (true) {
    if (xQueueReceive(fusb302b->message_queue_, &event_info, portMAX_DELAY) == pdTRUE) {
      PDMsg &msg = event_info.msg;
      fusb302b->handle_message(msg);
    }
  }
}

void FUSB302B::setup() {
  // Mutex (not binary semaphore) so the I2C lock gets priority inheritance
  this->i2c_lock_ = xSemaphoreCreateMutex();
  if (this->i2c_lock_ == nullptr) {
    ESP_LOGE(TAG, "Failed to create semaphore");
    this->mark_failed();
    return;
  }

  if (!this->check_chip_id()) {
    ESP_LOGE(TAG, "FUSB302B not found at I2C address 0x%02X", this->address_);
    this->set_state_(PD_STATE_ERROR);
    this->publish();
    this->mark_failed();
    return;
  }
  ESP_LOGD(TAG, "FUSB302B found, initializing...");

  if (!this->init_fusb_settings_()) {
    ESP_LOGE(TAG, "Failed to initialize FUSB302B");
    this->set_state_(PD_STATE_ERROR);
    this->publish();
    this->mark_failed();
    return;
  }
  this->startup_delay_ = millis();
}

void FUSB302B::dump_config() {
  ESP_LOGCONFIG(TAG, "FUSB302B USB-PD Controller:");
  LOG_I2C_DEVICE(this);
  ESP_LOGCONFIG(TAG, "  IRQ Pin: %d", this->irq_pin_);
  ESP_LOGCONFIG(TAG, "  Request Voltage: %dV", this->request_voltage_);
}

void FUSB302B::cleanup_() {
  // Remove the ISR handler first so a late interrupt can't notify a deleted task
  gpio_isr_handler_remove(static_cast<gpio_num_t>(this->irq_pin_));

  // Take the I2C lock so no task is deleted while it holds the bus mutex
  bool locked = (this->i2c_lock_ != nullptr) && (xSemaphoreTake(this->i2c_lock_, pdMS_TO_TICKS(100)) == pdTRUE);

  if (this->reader_task_handle_ != nullptr) {
    vTaskDelete(this->reader_task_handle_);
    this->reader_task_handle_ = nullptr;
  }
  if (this->process_task_handle_ != nullptr) {
    vTaskDelete(this->process_task_handle_);
    this->process_task_handle_ = nullptr;
  }
  if (this->message_queue_ != nullptr) {
    vQueueDelete(this->message_queue_);
    this->message_queue_ = nullptr;
  }
  if (locked) {
    xSemaphoreGive(this->i2c_lock_);
  }
  // Intentionally keep i2c_lock_ alive: deleting a semaphore another task may still be
  // blocked on is undefined behavior, and this only runs at shutdown.
}

void FUSB302B::on_shutdown() { this->cleanup_(); }

void FUSB302B::loop() {
  if (this->vbus_lost_.exchange(false) && this->hw_state_ == Fusb302State::ATTACHED) {
    ESP_LOGD(TAG, "USB-C detached (VBUS lost)");
    this->hw_state_ = Fusb302State::UNATTACHED;
    this->cc_phase_ = CcMeasurePhase::IDLE;
    this->cc_retry_after_ = millis();
    this->wait_src_cap_ = false;
    this->active_ams_ = false;
    this->fusb_reset_();
    this->set_state_(PD_STATE_DISCONNECTED);
    this->publish();
  }
  this->check_status_();
  if (this->contract_timer_ && (uint32_t) (millis() - this->contract_timer_) > 1000) {
    this->publish();
    this->contract_timer_ = 0;
  }
}

/* Reads BC_LVL several times to confirm a stable measurement — caller must hold i2c_lock_ */
bool FUSB302B::cc_read_bc_lvl_(uint8_t &lvl) {
  lvl = this->reg(FUSB_STATUS0).get() & FUSB_STATUS0_BC_LVL;
  for (uint8_t i = 0; i < 5; i++) {
    uint8_t tmp = this->reg(FUSB_STATUS0).get() & FUSB_STATUS0_BC_LVL;
    if (lvl != tmp) {
      return false;
    }
  }
  return true;
}

void FUSB302B::cc_measurement_failed_() {
  // No stable/valid CC orientation (usually nothing attached): stay UNATTACHED and
  // retry after a short backoff instead of latching a terminal failure state.
  this->cc_phase_ = CcMeasurePhase::IDLE;
  this->cc_retry_after_ = millis();
}

void FUSB302B::fusb_reset_unlocked_() {
  /* Flush TX/RX buffers and reset PD logic — caller must hold i2c_lock_ */
  this->reg(FUSB_CONTROL0) = FUSB_CONTROL0_TX_FLUSH;
  this->reg(FUSB_CONTROL1) = FUSB_CONTROL1_RX_FLUSH;
  this->reg(FUSB_RESET) = FUSB_RESET_PD_RESET;
  this->last_received_msg_id_ = 255;
  this->msg_counter_ = 0;
}

void FUSB302B::fusb_reset_() {
  if (xSemaphoreTake(this->i2c_lock_, pdMS_TO_TICKS(100)) != pdTRUE) {
    return;
  }
  this->fusb_reset_unlocked_();
  xSemaphoreGive(this->i2c_lock_);
}

bool FUSB302B::read_status(FusbStatus &status) {
  if (xSemaphoreTake(this->i2c_lock_, pdMS_TO_TICKS(100)) != pdTRUE) {
    return false;
  }
  int err = this->read_register(FUSB_STATUS0A, status.bytes, 7);
  xSemaphoreGive(this->i2c_lock_);
  return err == 0;
}

void FUSB302B::check_status_() {
  switch (this->hw_state_) {
    case Fusb302State::UNATTACHED: {
      if (this->startup_delay_ && (uint32_t) (millis() - this->startup_delay_) < 2000) {
        return;
      }
      if (this->cc_retry_after_ && (uint32_t) (millis() - this->cc_retry_after_) < CC_RETRY_INTERVAL_MS) {
        return;
      }

      // Non-blocking CC line selection: each measurement needs ~5 ms to settle, so the
      // settle time is spent across loop() iterations instead of delay()ing with the
      // I2C lock held.
      switch (this->cc_phase_) {
        case CcMeasurePhase::IDLE: {
          if (xSemaphoreTake(this->i2c_lock_, pdMS_TO_TICKS(100)) != pdTRUE) {
            return;
          }
          this->reg(FUSB_POWER) = PWR_BANDGAP | PWR_RECEIVER | PWR_MEASURE | PWR_INT_OSC;
          /* Measure CC1 */
          this->reg(FUSB_SWITCHES0) = FUSB_SWITCHES0_PDWN_1 | FUSB_SWITCHES0_PDWN_2 | FUSB_SWITCHES0_MEAS_CC1;
          this->reg(FUSB_SWITCHES1) = 0x01 << FUSB_SWITCHES1_SPECREV_SHIFT;
          this->reg(FUSB_MEASURE) = 49;
          xSemaphoreGive(this->i2c_lock_);
          this->cc_phase_ = CcMeasurePhase::SETTLE_CC1;
          this->cc_phase_start_ = millis();
          return;
        }
        case CcMeasurePhase::SETTLE_CC1: {
          if ((uint32_t) (millis() - this->cc_phase_start_) < CC_SETTLE_MS) {
            return;
          }
          if (xSemaphoreTake(this->i2c_lock_, pdMS_TO_TICKS(100)) != pdTRUE) {
            return;
          }
          bool stable = this->cc_read_bc_lvl_(this->cc1_lvl_);
          if (stable) {
            /* Measure CC2 */
            this->reg(FUSB_SWITCHES0) = FUSB_SWITCHES0_PDWN_1 | FUSB_SWITCHES0_PDWN_2 | FUSB_SWITCHES0_MEAS_CC2;
          }
          xSemaphoreGive(this->i2c_lock_);
          if (!stable) {
            this->cc_measurement_failed_();
            return;
          }
          this->cc_phase_ = CcMeasurePhase::SETTLE_CC2;
          this->cc_phase_start_ = millis();
          return;
        }
        case CcMeasurePhase::SETTLE_CC2: {
          if ((uint32_t) (millis() - this->cc_phase_start_) < CC_SETTLE_MS) {
            return;
          }
          if (xSemaphoreTake(this->i2c_lock_, pdMS_TO_TICKS(100)) != pdTRUE) {
            return;
          }
          uint8_t cc2 = 0;
          bool connected = this->cc_read_bc_lvl_(cc2);
          if (connected) {
            /* Select the correct CC line for BMC signaling */
            if (this->cc1_lvl_ > 0 && cc2 == 0) {
              ESP_LOGD(TAG, "CC select: 1");
              this->reg(FUSB_SWITCHES0) = FUSB_SWITCHES0_PDWN_1 | FUSB_SWITCHES0_PDWN_2 | FUSB_SWITCHES0_MEAS_CC1;
              this->reg(FUSB_SWITCHES1) = FUSB_SWITCHES1_TXCC1 | (0x01 << FUSB_SWITCHES1_SPECREV_SHIFT);
            } else if (this->cc1_lvl_ == 0 && cc2 > 0) {
              ESP_LOGD(TAG, "CC select: 2");
              this->reg(FUSB_SWITCHES0) = FUSB_SWITCHES0_PDWN_1 | FUSB_SWITCHES0_PDWN_2 | FUSB_SWITCHES0_MEAS_CC2;
              this->reg(FUSB_SWITCHES1) = FUSB_SWITCHES1_TXCC2 | (0x01 << FUSB_SWITCHES1_SPECREV_SHIFT);
            } else {
              connected = false;
            }
          }
          xSemaphoreGive(this->i2c_lock_);
          this->cc_phase_ = CcMeasurePhase::IDLE;
          if (!connected) {
            this->cc_measurement_failed_();
            return;
          }
          break;  // CC line selected, proceed with attach below
        }
      }

      if (this->startup_delay_) {
        ESP_LOGD(TAG, "Startup delay reached, spawning tasks");
        this->startup_delay_ = 0;

        BaseType_t ret =
            xTaskCreatePinnedToCore(trigger_task, "fusb302b_trigger", 4096, this, 18, &this->process_task_handle_, 1);
        if (ret != pdPASS) {
          ESP_LOGE(TAG, "Failed to create process task");
          this->mark_failed();
          this->hw_state_ = Fusb302State::FAILED;
          this->set_state_(PD_STATE_ERROR);
          this->publish();
          return;
        }
        delay(1);
      } else {
        this->enable_auto_crc();
        this->fusb_reset_();
      }

      this->get_src_cap_time_stamp_ = millis();
      this->get_src_cap_retry_count_ = 0;
      this->wait_src_cap_ = true;
      this->tried_soft_reset_ = false;

      this->hw_state_ = Fusb302State::ATTACHED;
      this->set_state_(PD_STATE_DEFAULT_CONTRACT);
      this->publish();  // fires the on_connect trigger (prev state was DISCONNECTED)
      ESP_LOGD(TAG, "USB-C attached");
      break;
    }
    case Fusb302State::ATTACHED:
      if (this->check_ams()) {
        return;
      }

      if (this->wait_src_cap_) {
        if (this->get_src_cap_retry_count_ && (uint32_t) (millis() - this->get_src_cap_time_stamp_) < 5000) {
          return;
        }
        if (!this->get_src_cap_retry_count_) {
          this->get_src_cap_retry_count_++;
          this->get_src_cap_time_stamp_ = millis();
          return;
        }
        this->get_src_cap_retry_count_++;
        this->get_src_cap_time_stamp_ = millis();
        if (this->get_src_cap_retry_count_ < 2) {
          // Interrupt registers are read (and thereby cleared) exclusively by the reader
          // task; reading them here would race with it and could swallow events.
          this->send_message(PDMsg(this, PD_CNTRL_GET_SOURCE_CAP));
        } else {
          ESP_LOGD(TAG, "GET_SOURCE_CAP reached max retries");
          if (!this->tried_soft_reset_) {
            this->fusb_reset_();
            this->send_message(PDMsg(this, PD_CNTRL_SOFT_RESET));
            this->get_src_cap_retry_count_ = 2;
            this->tried_soft_reset_ = true;
          } else {
            ESP_LOGD(TAG, "PD negotiation failed, staying at 5V");
            this->wait_src_cap_ = false;
            this->active_ams_ = false;
            this->set_state_(PD_STATE_PD_TIMEOUT);
            this->publish();
          }
        }
      }
      break;

    case Fusb302State::FAILED:
      break;
  }
}

bool FUSB302B::check_chip_id() {
  if (xSemaphoreTake(this->i2c_lock_, pdMS_TO_TICKS(1000)) != pdTRUE) {
    return false;
  }
  uint8_t dev_id = this->reg(FUSB_DEVICE_ID).get();
  xSemaphoreGive(this->i2c_lock_);
  ESP_LOGD(TAG, "FUSB302B device ID: 0x%02X", dev_id);
  return (dev_id == 0x81) || (dev_id == 0x91);
}

bool FUSB302B::enable_auto_crc() {
  if (xSemaphoreTake(this->i2c_lock_, pdMS_TO_TICKS(100)) != pdTRUE) {
    return false;
  }
  uint8_t sw1 = this->reg(FUSB_SWITCHES1).get();
  this->reg(FUSB_SWITCHES1) = sw1 | FUSB_SWITCHES1_AUTO_CRC;
  xSemaphoreGive(this->i2c_lock_);
  return true;
}

bool FUSB302B::disable_auto_crc() {
  if (xSemaphoreTake(this->i2c_lock_, pdMS_TO_TICKS(100)) != pdTRUE) {
    return false;
  }
  uint8_t sw1 = this->reg(FUSB_SWITCHES1).get();
  this->reg(FUSB_SWITCHES1) = sw1 & ~FUSB_SWITCHES1_AUTO_CRC;
  xSemaphoreGive(this->i2c_lock_);
  return true;
}

bool FUSB302B::read_status_register(uint8_t reg_addr, uint8_t &value) {
  if (xSemaphoreTake(this->i2c_lock_, pdMS_TO_TICKS(100)) != pdTRUE) {
    return false;
  }
  int err = this->read_register(reg_addr, &value, 1);
  xSemaphoreGive(this->i2c_lock_);
  return err == 0;
}

bool FUSB302B::init_fusb_settings_() {
  if (xSemaphoreTake(this->i2c_lock_, pdMS_TO_TICKS(100)) != pdTRUE) {
    return false;
  }

  /* Software reset to restore all registers to default */
  this->reg(FUSB_RESET) = FUSB_RESET_SW_RES;

  /* Flush buffers and reset PD logic (lock already held) */
  this->fusb_reset_unlocked_();

  /* Set interrupt masks */
  this->reg(FUSB_MASK1) = 0x51;
  this->reg(FUSB_MASKA) = 0;
  this->reg(FUSB_MASKB) = 0;

  /* Disable global interrupt masking */
  uint8_t cntrl0 = this->reg(FUSB_CONTROL0).get();
  this->reg(FUSB_CONTROL0) = cntrl0 & ~FUSB_CONTROL0_INT_MASK;

  /* Enable automatic retransmission (3 retries) */
  uint8_t cntrl3 = this->reg(FUSB_CONTROL3).get();
  cntrl3 &= ~FUSB_CONTROL3_N_RETRIES_MASK;
  cntrl3 |= (0x03 << FUSB_CONTROL3_N_RETRIES_SHIFT) | FUSB_CONTROL3_AUTO_RETRY;
  this->reg(FUSB_CONTROL3) = cntrl3;

  /* Power on all blocks */
  this->reg(FUSB_POWER) = 0x0F;

  /* Flush again after full power-on */
  this->fusb_reset_unlocked_();

  xSemaphoreGive(this->i2c_lock_);
  return true;
}

bool FUSB302B::read_message(PDMsg &msg) {
  if (xSemaphoreTake(this->i2c_lock_, pdMS_TO_TICKS(100)) != pdTRUE) {
    return false;
  }

  uint8_t fifo_byte = this->reg(FUSB_FIFOS).get();
  bool ok = (fifo_byte & FUSB_FIFO_RX_TOKEN_BITS) == FUSB_FIFO_RX_SOP;

  uint16_t header = 0;
  ok &= (this->read_register(FUSB_FIFOS, reinterpret_cast<uint8_t *>(&header), 2) == 0);
  msg.set_header(header);

  if (msg.num_of_obj > 7) {
    xSemaphoreGive(this->i2c_lock_);
    return false;
  }
  if (msg.num_of_obj > 0) {
    ok &= (this->read_register(FUSB_FIFOS, reinterpret_cast<uint8_t *>(msg.data_objects),
                               msg.num_of_obj * sizeof(uint32_t)) == 0);
  }

  /* Read CRC32 to advance FIFO pointer (PHY already verified it) */
  uint8_t dummy[4];
  ok &= (this->read_register(FUSB_FIFOS, dummy, 4) == 0);

  xSemaphoreGive(this->i2c_lock_);
  return ok;
}

bool FUSB302B::send_message(const PDMsg &msg) {
  if (xSemaphoreTake(this->i2c_lock_, pdMS_TO_TICKS(100)) != pdTRUE) {
    return false;
  }

  uint8_t buf[40];
  uint8_t *pbuf = buf;

  uint16_t header = msg.get_coded_header();
  uint8_t obj_count = msg.num_of_obj;

  *pbuf++ = TX_TOKEN_SOP1;
  *pbuf++ = TX_TOKEN_SOP1;
  *pbuf++ = TX_TOKEN_SOP1;
  *pbuf++ = TX_TOKEN_SOP2;
  *pbuf++ = TX_TOKEN_PACKSYM | ((obj_count << 2) + 2);
  *pbuf++ = header & 0xFF;
  *pbuf++ = (header >> 8) & 0xFF;
  for (uint8_t i = 0; i < obj_count; i++) {
    uint32_t d = msg.data_objects[i];
    *pbuf++ = d & 0xFF;
    *pbuf++ = (d >> 8) & 0xFF;
    *pbuf++ = (d >> 16) & 0xFF;
    *pbuf++ = (d >> 24) & 0xFF;
  }
  *pbuf++ = TX_TOKEN_JAM_CRC;
  *pbuf++ = TX_TOKEN_EOP;
  *pbuf++ = TX_TOKEN_TXOFF;
  *pbuf++ = TX_TOKEN_TXON;

  int err = this->write_register(FUSB_FIFOS, buf, pbuf - buf);
  if (err != i2c::ERROR_OK) {
    ESP_LOGW(TAG, "Sending message type %d failed (err=%d)", static_cast<int>(msg.type), err);
  }

  xSemaphoreGive(this->i2c_lock_);
  return err == i2c::ERROR_OK;
}

}  // namespace esphome::fusb302b

#endif  // USE_ESP_IDF
