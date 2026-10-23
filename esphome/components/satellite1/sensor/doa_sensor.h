#pragma once

#include "esphome/components/satellite1/satellite1.h"

#include "esphome/components/sensor/sensor.h"
#include "esphome/core/component.h"

namespace esphome::satellite1 {

/// Polls the direction of arrival from the XMOS audio servicer.
class Satellite1DoaSensor : public PollingComponent, public Parented<Satellite1> {
 public:
  void update() override;
  void dump_config() override;

  /// Reads the direction from the XMOS without publishing it. Returns false if the read failed.
  bool read_direction();

  void set_direction_sensor(sensor::Sensor *sensor) { this->direction_sensor_ = sensor; }
  void set_confidence_sensor(sensor::Sensor *sensor) { this->confidence_sensor_ = sensor; }

  /// True if the last read returned a direction, i.e. speech was located within the last ~0.5 s.
  bool is_valid() const { return this->last_result_.valid; }
  /// Azimuth of the last read in degrees, 0 = North mic, 90 = East mic. Only meaningful if is_valid().
  uint16_t get_azimuth() const { return this->last_result_.azimuth; }
  /// Confidence of the last read, 0..255.
  uint8_t get_confidence() const { return this->last_result_.confidence; }

 protected:
  sensor::Sensor *direction_sensor_{nullptr};
  sensor::Sensor *confidence_sensor_{nullptr};
  DoaResult last_result_{};
  bool read_failed_logged_{false};
};

}  // namespace esphome::satellite1
