#include "satellite1_update.h"

#include "esphome/core/log.h"

namespace esphome::satellite1 {

static const char *const TAG = "satellite1.update";
static constexpr uint32_t STOP_TIMEOUT_MS = 5000;
static constexpr uint32_t STOP_TIMEOUT_ID = 0;

void Satellite1Update::setup() {
  this->source_->add_on_state_callback([this]() { this->sync_state_(); });
  if (this->source_->has_state())
    this->sync_state_();
  this->disable_loop();
}

void Satellite1Update::perform(bool force) {
  if (this->preparing_ || this->state_ == update::UPDATE_STATE_INSTALLING)
    return;
  if (this->source_->state != update::UPDATE_STATE_AVAILABLE && !force)
    return;
  if (this->source_->update_info.firmware_url.empty() || this->source_->update_info.md5.empty()) {
    this->status_set_error(LOG_STR("No firmware manifest available"));
    return;
  }

  ESP_LOGI(TAG, "Stopping wake-word detection before firmware update");
  this->status_clear_error();
  this->preparing_ = true;
  this->force_ = force;
  this->restart_wake_word_ = this->mww_->is_running();
  this->state_ = update::UPDATE_STATE_INSTALLING;
  this->update_info_.has_progress = true;
  this->update_info_.progress = 0.0f;
  this->publish_state();
  this->mww_->stop();
  this->enable_loop();
  this->set_timeout(STOP_TIMEOUT_ID, STOP_TIMEOUT_MS, [this]() {
    if (!this->preparing_)
      return;
    ESP_LOGE(TAG, "Wake-word detection did not stop; cancelling firmware update");
    this->preparing_ = false;
    this->sync_state_();
    this->status_set_error(LOG_STR("Timed out stopping wake-word detection"));
  });
}

void Satellite1Update::loop() {
  if (this->mww_->is_running())
    return;
  if (!this->preparing_) {
    this->restore_wake_word_();
    this->disable_loop();
    return;
  }
  this->cancel_timeout(STOP_TIMEOUT_ID);
  // Ignore the transport's synchronous installing callback, which may still
  // contain progress from a previous failed attempt. Publish a fresh 0% below.
  this->source_->perform(this->force_);
  this->preparing_ = false;
  this->disable_loop();
  this->sync_state_(true);
}

void Satellite1Update::check() {
  if (!this->preparing_ && this->state_ != update::UPDATE_STATE_INSTALLING)
    this->source_->check();
}

void Satellite1Update::sync_state_(bool reset_progress) {
  // A manifest check already in flight must not hide the preparation state.
  if (this->preparing_)
    return;
  const bool available = this->source_->state == update::UPDATE_STATE_AVAILABLE &&
                         this->state_ != update::UPDATE_STATE_AVAILABLE &&
                         this->state_ != update::UPDATE_STATE_INSTALLING;
  const bool failed =
      this->state_ == update::UPDATE_STATE_INSTALLING && this->source_->state != update::UPDATE_STATE_INSTALLING;
  this->state_ = this->source_->state;
  this->update_info_ = this->source_->update_info;
  if (this->state_ == update::UPDATE_STATE_INSTALLING && (reset_progress || !this->update_info_.has_progress)) {
    this->update_info_.has_progress = true;
    this->update_info_.progress = 0.0f;
  } else if (this->state_ != update::UPDATE_STATE_INSTALLING) {
    this->update_info_.has_progress = false;
    this->update_info_.progress = 0.0f;
    this->restore_wake_word_();
  }
  if (failed)
    this->status_set_error(LOG_STR("Firmware update did not complete"));
  this->publish_state();
  if (available)
    this->get_update_available_trigger()->trigger(this->update_info_);
}

void Satellite1Update::restore_wake_word_() {
  if (!this->restart_wake_word_)
    return;
  // A timed-out cooperative stop can still finish on a later main-loop tick.
  if (this->mww_->is_running()) {
    this->enable_loop();
    return;
  }
  this->restart_wake_word_ = false;
  this->mww_->start();
}

}  // namespace esphome::satellite1
