#pragma once

#include "esphome/components/memory_flasher/memory_flasher.h"
#include "esphome/components/update/update_entity.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"

#include "../satellite1.h"

#include <string>
#include <utility>

namespace esphome::satellite1 {

/**
 * @brief Update entity for the XMOS firmware.
 *
 * Shows the running XMOS release and variant as installed version, and the release of this build in the
 * selected variant as latest version, e.g. "v2.0.0-beta.2 (aec4)". While the XMOS is flashed it reports
 * the progress, and after a failed flash the reason as release summary.
 *
 * Installing only calls the on_install automation: it picks the image for the selected variant (embedded
 * or downloaded) and handles the fallback to the embedded image.
 */
class XMOSUpdate final : public update::UpdateEntity, public Component, public Parented<Satellite1> {
 public:
  void setup() override;
  void dump_config() override;

  void perform(bool force) override;
  void check() override { this->refresh_(); }

  void set_flasher(memory_flasher::MemoryFlasher *flasher) { this->flasher_ = flasher; }
  void set_target_version(const char *version) { this->target_version_ = version; }
  void set_release_url(const char *url) { this->update_info_.release_url = url; }

  /// The variant installing should end up with, i.e. the user's selection.
  void set_target_variant(XMOSFwVariant variant);
  XMOSFwVariant get_target_variant() const { return this->target_variant_; }

  template<typename F> void add_on_install_callback(F &&callback) {
    this->install_callback_.add(std::forward<F>(callback));
  }

 protected:
  /// Recomputes the installed and latest version and the update state, then publishes.
  void refresh_();
  void on_flasher_state_(memory_flasher::FlasherState state, memory_flasher::FlasherAction action);
  void set_summary_(const char *error);

  CallbackManager<void()> install_callback_{};
  memory_flasher::MemoryFlasher *flasher_{nullptr};
  const char *target_version_{""};
  XMOSFwVariant target_variant_{XMOS_FW_VARIANT_FIXED_DELAY};
  /// Why the last install failed, empty if it didn't. Kept until the next install or until the XMOS
  /// runs the selected firmware.
  const char *last_error_{nullptr};
  bool flashing_{false};
};

}  // namespace esphome::satellite1
