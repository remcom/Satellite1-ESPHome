#pragma once

#include "esphome/components/micro_wake_word/micro_wake_word.h"
#include "esphome/components/update/update_entity.h"
#include "esphome/core/component.h"

namespace esphome::satellite1 {

// Keep the upstream manifest, download and signature verification paths, but let
// wake-word inference exit cooperatively before OTA_STARTED suspends its task.
class Satellite1Update : public update::UpdateEntity, public Component {
 public:
  void set_source(update::UpdateEntity *source) { this->source_ = source; }
  void set_micro_wake_word(micro_wake_word::MicroWakeWord *mww) { this->mww_ = mww; }

  void setup() override;
  void loop() override;
  void perform(bool force) override;
  void check() override;

 protected:
  void sync_state_(bool reset_progress = false);
  void restore_wake_word_();

  update::UpdateEntity *source_{nullptr};
  micro_wake_word::MicroWakeWord *mww_{nullptr};
  bool preparing_{false};
  bool force_{false};
  bool restart_wake_word_{false};
};

}  // namespace esphome::satellite1
