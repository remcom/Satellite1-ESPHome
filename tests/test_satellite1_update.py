"""Compile the actual update coordinator against small host-side ESPHome fakes.

Run with: python3 -m unittest discover -s tests
"""

from pathlib import Path
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]
COMPONENT = ROOT / "esphome/components/satellite1/update"

STUBS = r"""
#pragma once
#include <cstdint>
#include <functional>
#include <string>
#include <vector>
#define LOG_STR(s) s
#define ESP_LOGI(...) ((void) 0)
#define ESP_LOGE(...) ((void) 0)
namespace esphome {
class Component {
 public:
  virtual ~Component() = default;
  virtual void setup() {}
  virtual void loop() {}
  void disable_loop() { loop_enabled = false; }
  void enable_loop() { loop_enabled = true; }
  void status_set_error(const char *) { error = true; }
  void status_clear_error() { error = false; }
  void set_timeout(uint32_t, uint32_t, std::function<void()> cb) { timeout = cb; }
  void cancel_timeout(uint32_t) { timeout = nullptr; }
  bool loop_enabled{true};
  bool error{false};
  std::function<void()> timeout;
};
namespace update {
enum UpdateState { UPDATE_STATE_UNKNOWN, UPDATE_STATE_NO_UPDATE,
                   UPDATE_STATE_AVAILABLE, UPDATE_STATE_INSTALLING };
struct UpdateInfo {
  std::string firmware_url;
  std::string md5;
  std::string latest_version;
  bool has_progress{false};
  float progress{0};
};
struct AvailableTrigger {
  int count{0};
  void trigger(const UpdateInfo &) { count++; }
};
class UpdateEntity {
 public:
  virtual ~UpdateEntity() = default;
  virtual void perform(bool) = 0;
  virtual void check() = 0;
  void publish_state() {
    has_state_ = true;
    for (const auto &callback : callbacks) callback();
  }
  bool has_state() const { return has_state_; }
  void add_on_state_callback(std::function<void()> cb) { callbacks.push_back(cb); }
  AvailableTrigger *get_update_available_trigger() { return &trigger; }
  const UpdateState &state = state_;
  const UpdateInfo &update_info = update_info_;
 protected:
  UpdateState state_{UPDATE_STATE_UNKNOWN};
  UpdateInfo update_info_;
 private:
  bool has_state_{false};
  std::vector<std::function<void()>> callbacks;
  AvailableTrigger trigger;
};
}
namespace http_request {
class HttpRequestUpdate : public update::UpdateEntity {
 public:
  void perform(bool force) override {
    if (state_ != update::UPDATE_STATE_AVAILABLE && !force) return;
    installs++;
    forced = force;
    state_ = update::UPDATE_STATE_INSTALLING;
    publish_state();
  }
  void check() override { checks++; }
  void report(update::UpdateState state, float progress = -1) {
    state_ = state;
    // Like the upstream transport, errors retain the last progress value.
    if (progress >= 0) {
      update_info_.has_progress = true;
      update_info_.progress = progress;
    }
    publish_state();
  }
  void manifest() {
    update_info_ = {};
    update_info_.firmware_url = "https://example.test/satellite1.ota.bin";
    update_info_.md5 = "0123456789abcdef0123456789abcdef";
    update_info_.latest_version = "v2.0.0";
    report(update::UPDATE_STATE_AVAILABLE);
  }
  void clear_manifest() {
    update_info_ = {};
    report(update::UPDATE_STATE_UNKNOWN);
  }
  int installs{0};
  int checks{0};
  bool forced{false};
};
}
namespace micro_wake_word {
class MicroWakeWord {
 public:
  bool is_running() const { return running; }
  void stop() { stops++; }
  void start() { starts++; running = true; }
  bool running{true};
  int stops{0};
  int starts{0};
};
}
}
"""

SCENARIOS = r"""
#include "satellite1_update.h"
#include <cassert>
using namespace esphome;
using namespace esphome::update;
struct Device {
  http_request::HttpRequestUpdate source;
  micro_wake_word::MicroWakeWord mww;
  satellite1::Satellite1Update firmware;
  Device() {
    source.manifest();
    firmware.set_source(&source);
    firmware.set_micro_wake_word(&mww);
    firmware.setup();
  }
  void finish_stop() { mww.running = false; firmware.loop(); }
};
int main() {
  {
    // An update after normal use waits for inference to release its resources.
    Device d;
    assert(d.firmware.state == UPDATE_STATE_AVAILABLE);
    assert(d.firmware.get_update_available_trigger()->count == 1);
    assert(!d.firmware.loop_enabled);
    d.firmware.perform(false);
    assert(d.firmware.state == UPDATE_STATE_INSTALLING);
    assert(d.firmware.update_info.has_progress);
    assert(d.firmware.update_info.progress == 0);
    d.firmware.loop();
    assert(d.source.installs == 0);
    d.firmware.perform(false);  // Repeated clicks must not start another update.
    assert(d.mww.stops == 1);
    d.source.manifest();  // A concurrent manifest check cannot hide preparation.
    assert(d.firmware.state == UPDATE_STATE_INSTALLING);
    d.firmware.check();
    assert(d.source.checks == 0);
    d.finish_stop();
    assert(d.source.installs == 1);
    assert(d.firmware.update_info.has_progress);
    assert(d.firmware.update_info.progress == 0);
    assert(!d.firmware.timeout);
    d.source.report(UPDATE_STATE_INSTALLING, 42);
    assert(d.firmware.update_info.progress == 42);
    d.source.report(UPDATE_STATE_AVAILABLE);  // Failed download/signature check.
    assert(d.mww.starts == 1);
    assert(d.firmware.error);
    assert(!d.firmware.update_info.has_progress);
    assert(d.firmware.get_update_available_trigger()->count == 1);
    d.firmware.perform(false);  // A failure remains retryable.
    assert(!d.firmware.error);
    d.finish_stop();
    assert(d.source.installs == 2);
    assert(d.firmware.update_info.progress == 0);
  }
  {
    // Immediately after reboot, stopped inference needs no extra waiting.
    Device d;
    d.mww.running = false;
    d.firmware.perform(false);
    d.firmware.loop();
    assert(d.source.installs == 1);
    d.source.report(UPDATE_STATE_AVAILABLE);
    assert(d.mww.starts == 0);  // Preserve deliberately stopped detection.
  }
  {
    // A stuck stop times out without flashing or permanently disabling voice.
    Device d;
    d.firmware.perform(false);
    d.firmware.timeout();
    assert(d.source.installs == 0);
    assert(d.firmware.state == UPDATE_STATE_AVAILABLE);
    assert(d.firmware.error);
    assert(d.firmware.loop_enabled);
    d.finish_stop();
    assert(d.mww.starts == 1);
    assert(!d.firmware.loop_enabled);
    assert(d.source.installs == 0);
  }
  {
    // Force is forwarded, but never permits flashing without a valid manifest.
    Device d;
    d.source.report(UPDATE_STATE_NO_UPDATE);
    d.firmware.perform(false);
    assert(d.mww.stops == 0);
    d.firmware.perform(true);
    d.finish_stop();
    assert(d.source.installs == 1 && d.source.forced);
    Device empty;
    empty.source.clear_manifest();
    empty.firmware.perform(true);
    assert(empty.source.installs == 0);
    assert(empty.mww.stops == 0);
    assert(empty.firmware.error);
  }
  {
    // A channel refresh that removes the update during preparation cancels it.
    Device d;
    d.firmware.perform(false);
    d.source.report(UPDATE_STATE_NO_UPDATE);
    d.finish_stop();
    assert(d.source.installs == 0);
    assert(d.firmware.state == UPDATE_STATE_NO_UPDATE);
    assert(d.mww.starts == 1);
    d.firmware.check();
    assert(d.source.checks == 1);
  }
}
"""


class Satellite1UpdateTest(unittest.TestCase):
    def test_install_lifecycle(self):
        scratch = ROOT / ".temp"
        scratch.mkdir(exist_ok=True)
        with tempfile.TemporaryDirectory(dir=scratch) as directory:
            work = Path(directory)
            (work / "stubs.h").write_text(STUBS)
            for name in (
                "core/component.h",
                "core/log.h",
                "components/http_request/update/http_request_update.h",
                "components/micro_wake_word/micro_wake_word.h",
                "components/update/update_entity.h",
            ):
                header = work / "esphome" / name
                header.parent.mkdir(parents=True, exist_ok=True)
                header.write_text('#include "stubs.h"\n')
            (work / "scenarios.cpp").write_text(SCENARIOS)
            binary = work / "test-update"
            subprocess.run(
                [
                    "g++", "-std=c++20", "-Wall", "-Wextra", "-Werror",
                    "-Wno-unused-variable", f"-I{work}", f"-I{COMPONENT}",
                    str(COMPONENT / "satellite1_update.cpp"),
                    str(work / "scenarios.cpp"), "-o", str(binary),
                ],
                check=True,
            )
            subprocess.run([str(binary)], check=True)


if __name__ == "__main__":
    unittest.main()
