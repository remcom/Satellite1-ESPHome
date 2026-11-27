#include "audio_quality_sensor.h"
#include "esphome/core/log.h"

#include <cmath>

namespace esphome::satellite1 {

static const char *const TAG = "satellite1.audio_quality";

void Satellite1AudioQualitySensor::publish_unavailable_() {
  PUBLISH_STATE(this->echo_reduction_sensor_, NAN);
  PUBLISH_STATE(this->echo_reduction_worst_sensor_, NAN);
}

void Satellite1AudioQualitySensor::update() {
  if (this->parent_->get_state() != SAT_XMOS_CONNECTED_STATE) {
    this->have_sequence_ = false;
    return;
  }
  AudioQuality q{};
  if (!this->parent_->read_audio_quality(q)) {
    // Fails on every read with XMOS firmware without the telemetry, so only log it once.
    if (!this->read_failed_logged_) {
      ESP_LOGW(TAG, "Reading the audio quality telemetry from the XMOS failed");
      this->read_failed_logged_ = true;
    }
    return;
  }
  this->read_failed_logged_ = false;
  if (!(q.flags & AUDIO_QUALITY_FLAG_READY)) {
    this->publish_unavailable_();
    return;
  }

  // An unchanged frame sequence means the audio pipeline stalled: the ERLE values are stale.
  const bool stalled = this->have_sequence_ && q.sequence == this->last_sequence_;
  this->last_sequence_ = q.sequence;
  this->have_sequence_ = true;
  if (stalled) {
    ESP_LOGW(TAG, "XMOS audio pipeline made no progress since the previous read");
    this->publish_unavailable_();
  } else {
    float sum = 0.0f;
    float worst = NAN;
    unsigned count = 0;
    for (uint8_t ch = 0; ch < q.aec_channels; ch++) {
      if (q.erle_cdb[ch] == AUDIO_QUALITY_ERLE_UNKNOWN) {
        continue;
      }
      const float db = q.erle_cdb[ch] / 100.0f;
      sum += db;
      worst = std::isnan(worst) ? db : std::min(worst, db);
      count++;
    }
    PUBLISH_STATE(this->echo_reduction_sensor_, count ? sum / count : NAN);
    PUBLISH_STATE(this->echo_reduction_worst_sensor_, worst);
  }

  if (q.flags & AUDIO_QUALITY_FLAG_REFERENCE) {
    // Saturating counters; the sum of two can exceed 2^32 only after ~2 years of constant drops.
    PUBLISH_STATE(this->reference_drops_sensor_,
                  static_cast<float>(static_cast<uint64_t>(q.ref_drops_oldest) + q.ref_drops_newest));
  }
  if (q.output_channels >= 2) {
    PUBLISH_STATE(this->clipped_samples_ch0_sensor_, static_cast<float>(q.clips[0]));
    PUBLISH_STATE(this->clipped_samples_ch1_sensor_, static_cast<float>(q.clips[1]));
  }
  ESP_LOGV(TAG, "seq %u, AEC channels %u, drops %u/%u, clips %u/%u", (unsigned) q.sequence, q.aec_channels,
           (unsigned) q.ref_drops_oldest, (unsigned) q.ref_drops_newest, (unsigned) q.clips[0],
           (unsigned) q.clips[1]);
}

void Satellite1AudioQualitySensor::dump_config() {
  ESP_LOGCONFIG(TAG, "Satellite1 audio quality:");
  LOG_UPDATE_INTERVAL(this);
  LOG_SENSOR("  ", "Echo reduction", this->echo_reduction_sensor_);
  LOG_SENSOR("  ", "Echo reduction (worst mic)", this->echo_reduction_worst_sensor_);
  LOG_SENSOR("  ", "Reference drops", this->reference_drops_sensor_);
  LOG_SENSOR("  ", "Clipped samples channel 0", this->clipped_samples_ch0_sensor_);
  LOG_SENSOR("  ", "Clipped samples channel 1", this->clipped_samples_ch1_sensor_);
}

}  // namespace esphome::satellite1
