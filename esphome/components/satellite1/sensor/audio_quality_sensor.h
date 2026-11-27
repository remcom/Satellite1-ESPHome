#pragma once

#include "esphome/components/satellite1/satellite1.h"

#include "esphome/components/sensor/sensor.h"
#include "esphome/core/component.h"

namespace esphome::satellite1 {

/// Polls the audio quality telemetry of the XMOS stats servicer.
class Satellite1AudioQualitySensor : public PollingComponent, public Parented<Satellite1> {
 public:
  void update() override;
  void dump_config() override;

  /// Mean echo reduction (ERLE) over the AEC mics, dB
  SUB_SENSOR(echo_reduction)
  /// Echo reduction of the AEC mic that removes the least echo, dB
  SUB_SENSOR(echo_reduction_worst)
  /// Speaker reference frames dropped since the XMOS booted (the AEC fell behind)
  SUB_SENSOR(reference_drops)
  /// Clipped output samples since the XMOS booted, channel 0 (to Home Assistant)
  SUB_SENSOR(clipped_samples_ch0)
  /// Clipped output samples since the XMOS booted, channel 1 (wake word)
  SUB_SENSOR(clipped_samples_ch1)

 protected:
  void publish_unavailable_();

  uint32_t last_sequence_{0};
  bool have_sequence_{false};
  bool read_failed_logged_{false};
};

}  // namespace esphome::satellite1
