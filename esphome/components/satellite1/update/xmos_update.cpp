#include "xmos_update.h"

#include "esphome/core/log.h"

namespace esphome::satellite1 {

static const char *const TAG = "satellite1.update";

static constexpr size_t VERSION_STR_MAX_LEN = 48;

static const char *const INSTALL_HINT =
    "Installing reflashes the XMOS audio chip. It takes about a minute, the LED ring shows the progress, "
    "and voice and audio are unavailable until it is done. Don't unplug the device meanwhile.";

static const char *flasher_error_str(memory_flasher::FlasherError error) {
  switch (error) {
    case memory_flasher::INIT_READER_ERROR:
      return "the firmware image couldn't be downloaded";
    case memory_flasher::INIT_FLASH_ERROR:
      return "the XMOS flash wasn't accessible";
    case memory_flasher::WRITE_TO_FLASH_ERROR:
      return "writing the XMOS flash failed";
    case memory_flasher::MD5_MISMATCH_ERROR:
      return "the downloaded image was damaged (checksum mismatch)";
    case memory_flasher::MD5_INVALID:
      return "the checksum file couldn't be downloaded";
    case memory_flasher::BAD_URL:
      return "the image URL is invalid";
    case memory_flasher::CONNECTION_ERROR:
      return "the download broke off";
    case memory_flasher::NO_EMBEDDED_IMAGE_ERROR:
      return "this build has no embedded XMOS image";
    default:
      return "unknown error";
  }
}

void XMOSUpdate::setup() {
  this->update_info_.title = "XMOS audio firmware";
  this->parent_->add_on_state_callback([this](Satellite1State /*state*/, uint8_t /*attempts*/) { this->refresh_(); });
  if (this->flasher_ != nullptr) {
    this->flasher_->add_on_state_callback(
        [this](memory_flasher::FlasherState state, memory_flasher::FlasherState /*previous*/,
               memory_flasher::FlasherAction action) { this->on_flasher_state_(state, action); });
  }
  this->refresh_();
}

void XMOSUpdate::dump_config() {
  ESP_LOGCONFIG(TAG,
                "XMOS Update:\n"
                "  Target: %s (%s)\n"
                "  Installed: %s",
                this->target_version_, xmos_fw_variant_to_str(this->target_variant_),
                this->update_info_.current_version.c_str());
}

void XMOSUpdate::set_target_variant(XMOSFwVariant variant) {
  this->target_variant_ = variant;
  this->refresh_();
}

void XMOSUpdate::perform(bool force) {
  if (this->flashing_ || (this->flasher_ != nullptr && this->flasher_->get_state() != memory_flasher::FLASHER_IDLE)) {
    ESP_LOGW(TAG, "XMOS is already being flashed");
    return;
  }
  if (!force && this->state_ == update::UPDATE_STATE_NO_UPDATE) {
    ESP_LOGI(TAG, "XMOS already runs %s, not reflashing", this->update_info_.latest_version.c_str());
    return;
  }
  ESP_LOGI(TAG, "Installing XMOS firmware %s", this->update_info_.latest_version.c_str());
  this->last_error_ = nullptr;
  this->install_callback_.call();
}

void XMOSUpdate::refresh_() {
  // While flashing the flasher callback owns the state, and the XMOS (held in reset) reports nothing useful.
  if (this->flashing_ || this->parent_->get_state() == SAT_FLASH_CONNECTED_STATE) {
    return;
  }

  char buffer[VERSION_STR_MAX_LEN];
  buf_append_printf(buffer, sizeof(buffer), 0, "%s (%s)", this->target_version_,
                    xmos_fw_variant_to_str(this->target_variant_));
  this->update_info_.latest_version = buffer;

  if (this->parent_->get_state() == SAT_XMOS_CONNECTED_STATE) {
    char version[VERSION_STR_MAX_LEN];
    this->parent_->format_fw_version(version, sizeof(version));
    buf_append_printf(buffer, sizeof(buffer), 0, "%s (%s)", version,
                      xmos_fw_variant_to_str(this->parent_->get_xmos_fw_variant()));
    this->update_info_.current_version = buffer;
  } else if (this->parent_->get_connection_attempts() >= MAX_CONNECTION_ATTEMPTS) {
    // Installing is how a broken XMOS firmware is repaired, so offer it
    this->update_info_.current_version = "not responding";
  } else {
    // Still detecting, nothing to report yet
    return;
  }

  if (this->update_info_.current_version == this->update_info_.latest_version) {
    this->state_ = update::UPDATE_STATE_NO_UPDATE;
    this->last_error_ = nullptr;
  } else {
    this->state_ = update::UPDATE_STATE_AVAILABLE;
  }
  this->update_info_.has_progress = false;
  this->update_info_.progress = 0.0f;
  this->set_summary_(this->last_error_);
  this->publish_state();
}

void XMOSUpdate::on_flasher_state_(memory_flasher::FlasherState state, memory_flasher::FlasherAction action) {
  switch (state) {
    case memory_flasher::FLASHER_INITIALIZING:
    case memory_flasher::FLASHER_ERASING:
    case memory_flasher::FLASHER_FLASHING:
      this->flashing_ = true;
      this->state_ = update::UPDATE_STATE_INSTALLING;
      this->update_info_.has_progress = true;
      this->update_info_.progress = this->flasher_->get_flashing_progress();
      this->publish_state();
      break;

    case memory_flasher::FLASHER_ERROR_STATE:
      this->last_error_ = flasher_error_str(this->flasher_->get_error_code());
      ESP_LOGW(TAG, "Installing XMOS firmware failed: %s", this->last_error_);
      this->flashing_ = false;
      this->refresh_();
      break;

    case memory_flasher::FLASHER_SUCCESS_STATE:
      // The installed version is updated once the XMOS is detected again
      this->flashing_ = false;
      this->refresh_();
      break;

    default:
      break;
  }
}

void XMOSUpdate::set_summary_(const char *error) {
  if (error == nullptr) {
    this->update_info_.summary = this->state_ == update::UPDATE_STATE_AVAILABLE ? INSTALL_HINT : "";
    return;
  }
  std::string summary = "**The last install failed: ";
  summary += error;
  summary += ".** If a standard firmware was installed instead, the device keeps working. Install again to retry.\n\n";
  summary += INSTALL_HINT;
  this->update_info_.summary = std::move(summary);
}

}  // namespace esphome::satellite1
