#include "doa_sensor.h"
#include "esphome/core/log.h"

namespace esphome::satellite1 {

static const char *const TAG = "satellite1.doa";

bool Satellite1DoaSensor::read_direction() {
  this->last_result_.valid = false;
  if (this->parent_->get_state() != SAT_XMOS_CONNECTED_STATE) {
    return false;
  }

  DoaResult result{};
  if (!this->parent_->read_doa(result)) {
    // Fails on every read with XMOS firmware that has no audio servicer, so only log it once.
    if (!this->read_failed_logged_) {
      ESP_LOGW(TAG, "Reading the direction of arrival from the XMOS failed");
      this->read_failed_logged_ = true;
    }
    return false;
  }
  this->read_failed_logged_ = false;
  this->last_result_ = result;
  return true;
}

void Satellite1DoaSensor::update() {
  if (!this->read_direction() || !this->last_result_.valid) {
    return;
  }
  const DoaResult &result = this->last_result_;
  ESP_LOGV(TAG, "Direction %u deg, confidence %u", result.azimuth, result.confidence);
  if (this->direction_sensor_ != nullptr) {
    this->direction_sensor_->publish_state(result.azimuth);
  }
  if (this->confidence_sensor_ != nullptr) {
    this->confidence_sensor_->publish_state(result.confidence * 100.0f / 255.0f);
  }
}

void Satellite1DoaSensor::dump_config() {
  ESP_LOGCONFIG(TAG, "Satellite1 DOA:");
  LOG_UPDATE_INTERVAL(this);
  LOG_SENSOR("  ", "Direction", this->direction_sensor_);
  LOG_SENSOR("  ", "Confidence", this->confidence_sensor_);
}

}  // namespace esphome::satellite1
