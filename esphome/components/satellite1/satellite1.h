#pragma once

#include "esphome/components/gpio_expander/cached_gpio.h"
#include "esphome/components/gpio_expander/expander_gpio_pin.h"
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

static const uint8_t AUDIO_SERVICER_RESID = 230;

static const uint8_t DFU_CONTROLLER_SERVICER_RESID = 240;

static const uint8_t STATS_SERVICER_RESID = 250;

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
  AUDIO = AUDIO_SERVICER_RESID,
  DFU_CONTROLLER = DFU_CONTROLLER_SERVICER_RESID,
  STATS = STATS_SERVICER_RESID,
  GPIO_PORT_IN_A = GPIO_SERVICER_RESID_PORT_IN_A,
  GPIO_PORT_IN_B = GPIO_SERVICER_RESID_PORT_IN_B,
  GPIO_PORT_OUT_A = GPIO_SERVICER_RESID_PORT_OUT_A
};
}  // namespace DC_RESOURCE

namespace DC_RET_STATUS {
enum DcRetStatusEnum { CMD_SUCCESS = 0, STATUS_ONLY_FRAME = 1, PAYLOAD_AVAILABLE = 23 };
}  // namespace DC_RET_STATUS

namespace DC_STATUS_REGISTER {
enum RegisterId {
  DEVICE_STATUS = 0,
  GPIO_PORT_IN_A = 1,
  GPIO_PORT_IN_B = 2,
  GPIO_PORT_OUT_A = 3,

  REGISTER_LEN = 4
};
}  // namespace DC_STATUS_REGISTER

namespace DC_DFU_CMD {
enum DcDfuCmdId {
  GET_VERSION = (88 | CONTROL_CMD_READ_BIT),
  // From XMOS firmware v2.0.0-beta.2. Older firmware rejects it, which means fixed_delay.
  GET_VARIANT = (92 | CONTROL_CMD_READ_BIT),
};
}  // namespace DC_DFU_CMD

/// Audio servicer commands, available from the XMOS firmware with 4-mic front end and DOA.
namespace DC_AUDIO_CMD {
enum DcAudioCmdId {
  SET_MIC_GAIN = 0,
  SET_MIC_MODE = 1,
  GET_DOA = (2 | CONTROL_CMD_READ_BIT),
  GET_BEAM = (4 | CONTROL_CMD_READ_BIT),
  // From the XMOS firmware with wake word steering. Older firmware ignores them.
  SET_CH1_TAP = 5,
  SET_IC_FAST_HOLD = 6,
  SET_HPF = 7,
  SET_STEER_POLICY = 8,
  SET_WAKE_EVENT = 9,
  // From the XMOS firmware with a mono echo reference in both variants. Older firmware ignores it.
  SET_REF_MIX = 10,
  // From the XMOS firmware that measures the wake word report latency. Older firmware rejects it.
  GET_WAKE_TIMING = (11 | CONTROL_CMD_READ_BIT),
  // From the XMOS firmware with a level control on the wake word channel. Older firmware rejects it.
  SET_CH1_AGC = 12,
};
}  // namespace DC_AUDIO_CMD

static const uint8_t XMOS_MIC_GAIN_DB_MAX = 30;

static const uint8_t AUDIO_DOA_PAYLOAD_LEN = 4;
static const uint8_t AUDIO_DOA_FLAG_VALID = 1 << 0;
static const uint8_t AUDIO_DOA_FLAG_UPDATED = 1 << 1;
static const uint8_t AUDIO_BEAM_PAYLOAD_LEN = 2;
static const uint8_t AUDIO_WAKE_TIMING_PAYLOAD_LEN = 8;
static const uint8_t AUDIO_WAKE_TIMING_VERSION = 1;
static const uint8_t AUDIO_WAKE_TIMING_FLAG_HANDLED = 1 << 0;  ///< a wake word was handled since the XMOS started
static const uint8_t AUDIO_WAKE_TIMING_FLAG_LOCATED = 1 << 1;  ///< and its direction was found
/// Wake timing age when there was no DOA result at all.
static const uint16_t AUDIO_WAKE_TIMING_NO_AGE = 0xFFFF;
/// Length of an XMOS audio frame, the unit of the wake timing.
static const uint8_t XMOS_FRAME_MS = 15;
/// Beam azimuth reported outside the beam mic mode.
static const uint16_t XMOS_NO_BEAM = 0xFFFF;

/// Which mics feed the two channels of the XMOS audio pipeline.
enum XMOSMicMode : uint8_t {
  MIC_MODE_PAIR_EW = 0,
  MIC_MODE_PAIR_NS = 1,
  MIC_MODE_AVERAGE = 2,
  MIC_MODE_AUTO_PAIR = 3,
  MIC_MODE_BEAM = 4,
  MIC_MODE_LAST = MIC_MODE_BEAM,
};

/// Signal on the right channel (channel 1) of the XMOS output. None has the AGC of channel 0.
enum XMOSCh1Tap : uint8_t {
  CH1_TAP_NS = 0,   ///< AEC+IC+NS (XMOS default)
  CH1_TAP_IC = 1,   ///< AEC+IC, before the noise suppression
  CH1_TAP_AEC = 2,  ///< AEC only
  CH1_TAP_LAST = CH1_TAP_AEC,
};

/// What may move the XMOS beam (and the automatic mic pair).
enum XMOSSteerPolicy : uint8_t {
  STEER_POLICY_FOLLOW_SPEECH = 0,  ///< any speech, and wake words at once (XMOS default)
  STEER_POLICY_WAKE_WORD = 1,      ///< only wake words, so a TV can't pull the beam
  STEER_POLICY_FIXED = 2,          ///< nothing
  STEER_POLICY_LAST = STEER_POLICY_FIXED,
};

/// What the XMOS mixes its (mono) echo canceller reference from. Must match what the TAS2780 plays.
enum XMOSRefMix : uint8_t {
  REF_MIX_DOWNMIX = 0,  ///< (L+R)/2, the TAS2780 "Mono (Left + Right)" mode (XMOS default)
  REF_MIX_LEFT = 1,
  REF_MIX_RIGHT = 2,
  REF_MIX_LAST = REF_MIX_RIGHT,
};

/// Stats servicer commands (read only).
namespace DC_STATS_CMD {
enum DcStatsCmdId {
  GET_AUDIO_QUALITY = (3 | CONTROL_CMD_READ_BIT),
};
}  // namespace DC_STATS_CMD

static const uint8_t AUDIO_QUALITY_PAYLOAD_LEN = 32;
static const uint8_t AUDIO_QUALITY_VERSION = 1;
static const uint8_t AUDIO_QUALITY_MAX_AEC_CHANNELS = 4;
static const uint8_t AUDIO_QUALITY_FLAG_REFERENCE = 1 << 0;  ///< reference drop counters are available
static const uint8_t AUDIO_QUALITY_FLAG_READY = 1 << 1;      ///< at least one pipeline frame was published
/// ERLE value for a channel without a measurement
static const int16_t AUDIO_QUALITY_ERLE_UNKNOWN = INT16_MIN;

/**
 * Audio quality telemetry of the XMOS stats servicer (payload version 1, see
 * docs/device-control-spi-protocol.md in Satellite1-XMOS). Counters run since the XMOS booted and
 * saturate; reads don't clear them.
 */
struct AudioQuality {
  uint8_t aec_channels;       ///< 0 (no AEC), 2 (fixed_delay) or 4 (aec4)
  uint8_t output_channels;    ///< 2 with I2S output, else 0 (no clip counts)
  uint8_t flags;              ///< AUDIO_QUALITY_FLAG_*
  uint32_t sequence;          ///< completed pipeline frames, wraps; unchanged = the pipeline stalled
  uint32_t ref_drops_oldest;  ///< speaker reference frames dropped from a full queue
  uint32_t ref_drops_newest;  ///< new reference frames that could not be queued
  int16_t erle_cdb[AUDIO_QUALITY_MAX_AEC_CHANNELS];  ///< echo reduction per AEC mic, 0.01 dB
  uint32_t clips[2];  ///< output samples at or above 0.99 full scale: left (channel 0), right (channel 1)
};

/// Direction of arrival as reported by the XMOS audio servicer.
struct DoaResult {
  uint16_t azimuth;    ///< 0..359 degrees, 0 = North mic, 90 = East mic
  uint8_t confidence;  ///< 0..255
  bool valid;          ///< a result was produced within the last ~0.5 s
  bool updated;        ///< a result was produced in the most recent audio frame
};

/// How long the last wake word took to reach the XMOS, see DC_AUDIO_CMD::GET_WAKE_TIMING.
struct WakeTiming {
  bool handled;            ///< a wake word was handled since the XMOS started
  bool located;            ///< its direction was found
  uint16_t sequence;       ///< number of that wake word since the XMOS started (wraps)
  uint16_t newest_age;     ///< frames from the newest DOA result to the wake event, AUDIO_WAKE_TIMING_NO_AGE if none
  uint8_t window_results;  ///< DOA results in the wake word window
  uint8_t latency_frames;  ///< frames the window leaves out before the wake event
};

/// XMOS firmware build variant, as reported by DC_DFU_CMD::GET_VARIANT. All variants of a release share
/// one version string. The values are fixed by the XMOS firmware, new variants take the next free number.
enum XMOSFwVariant : uint8_t {
  XMOS_FW_VARIANT_FIXED_DELAY = 0,  ///< default: AEC on the selected mic pair
  XMOS_FW_VARIANT_BYPASS = 1,       ///< not released
  XMOS_FW_VARIANT_AEC4 = 2,         ///< experimental: AEC on all four mics before the beamformer
};

/// Short name of a firmware variant as used in the release assets, e.g. "aec4".
const char *xmos_fw_variant_to_str(XMOSFwVariant variant);

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
  /// Variant of the running XMOS firmware, read together with the version.
  XMOSFwVariant get_xmos_fw_variant() const { return this->xmos_fw_variant_; }
  /// Writes the running XMOS firmware version as "vX.Y.Z[-pre][.N]" into buffer.
  void format_fw_version(char *buffer, size_t len) const { this->format_fw_version_(buffer, len); }

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

  /**
   * @brief Reads the latest direction of arrival from the XMOS audio servicer.
   *
   * The XMOS only updates the DOA during speech while no audio is played back.
   *
   * @return  false if the transfer failed or the XMOS firmware has no audio servicer. A result with an
   *          out of range azimuth is returned as not valid.
   */
  bool read_doa(DoaResult &result);

  /// Sets the XMOS mic gain in dB (0..XMOS_MIC_GAIN_DB_MAX). Not persisted by the XMOS.
  bool set_mic_gain(uint8_t gain_db);

  /// Selects the mics feeding the XMOS audio pipeline. Not persisted by the XMOS.
  bool set_mic_mode(XMOSMicMode mode);

  /**
   * @brief Reads the XMOS beam steering.
   *
   * @param azimuth  0..359 degrees (0 = North mic, 90 = East mic), or XMOS_NO_BEAM outside the beam mic mode
   * @return         false if the transfer failed or the XMOS firmware has no beam mode
   */
  bool read_beam_azimuth(uint16_t &azimuth);

  /**
   * @brief Reads the audio quality telemetry from the XMOS stats servicer.
   *
   * @return false if the transfer failed, the XMOS firmware has no telemetry or the payload version is unknown
   */
  bool read_audio_quality(AudioQuality &quality);

  /// Selects the signal on the XMOS right output channel. Not persisted by the XMOS.
  bool set_ch1_tap(XMOSCh1Tap tap);

  /// Whether the XMOS interference canceller stops adapting within ~2 frames of speech onset. Not persisted.
  bool set_ic_fast_hold(bool enable);

  /// Whether the XMOS high-passes the mics at 100 Hz before the echo canceller. Not persisted.
  bool set_hpf(bool enable);

  /// Whether the XMOS levels the wake word channel with its own AGC (XMOS default on). Not persisted.
  bool set_ch1_agc(bool enable);

  /// Selects what may steer the XMOS beam. Not persisted by the XMOS.
  bool set_steer_policy(XMOSSteerPolicy policy);

  /// Tells the XMOS which speaker channel(s) the TAS2780 plays, for its echo reference. Not persisted.
  bool set_ref_mix(XMOSRefMix mix);

  /**
   * @brief Reports a wake word, or the end of the conversation it started, to the XMOS.
   *
   * On a wake word the XMOS steers the beam to where the wake word came from within one
   * audio frame (~15 ms), and holds it there until the conversation ends.
   */
  bool notify_wake_word(bool detected);

  /**
   * @brief Reads how long the last wake word took to reach the XMOS.
   *
   * @return false if the transfer failed, the XMOS firmware can't measure it or the payload version is unknown
   */
  bool read_wake_timing(WakeTiming &timing);

  /**
   * @brief Logs the timing of the wake word just reported, for tuning the XMOS wake word window.
   *
   * Call ~30 ms after notify_wake_word(true). Logs at INFO once per wake word; firmware without
   * the measurement is noted once at DEBUG.
   */
  void log_wake_timing();

  void set_spi_flash_direct_access_mode(bool enable);

  /// The XMOS fixes the direction of its GPIO ports, so there is no mode to set.
  void pin_mode(uint8_t pin, gpio::Flags flags) {}

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
  /// Reads the firmware variant. Firmware without the command (before v2.0.0-beta.2) is fixed_delay.
  void dfu_get_fw_variant_();
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
  XMOSFwVariant xmos_fw_variant_{XMOS_FW_VARIANT_FIXED_DELAY};
  uint8_t connection_attempts_{0};

  uint32_t last_attempt_timestamp_{0};

  uint8_t dc_status_register_[DC_STATUS_REGISTER::REGISTER_LEN]{};
  bool spi_flash_direct_access_enabled_{false};

  uint8_t spi_transfer_buffer_[256 + 3];  // Reusable buffer for SPI transfers

  GPIOPin *xmos_rst_pin_{nullptr};
  bool xmos_reset_in_progress_{false};

  // log_wake_timing()
  bool wake_timing_seen_{false};
  uint16_t wake_timing_sequence_{0};
  bool wake_timing_unsupported_logged_{false};
  std::function<void()> xmos_reset_done_callback_{};
};

class Satellite1SPIService : public Parented<Satellite1> {
 protected:
  uint8_t transfer_byte_(uint8_t byte) { return this->parent_->transfer_byte(byte); }
  void enable_() { this->parent_->enable(); }
  void disable_() { this->parent_->disable(); }
};

/// A pin on one of the XMOS GPIO ports, see XMOS_GPIO_PINS_PER_PORT for the pin numbering.
using Satellite1GPIOPin = gpio_expander::ExpanderGPIOPin<Satellite1>;

}  // namespace esphome::satellite1

ESPHOME_EXPANDER_PIN_SUMMARY_DECL(esphome::satellite1::Satellite1);
