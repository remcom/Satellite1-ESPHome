#pragma once

#include "esphome/components/http_request/http_request.h"
#include "esphome/components/spi/spi.h"
#include "esphome/components/md5/md5.h"

#include "esphome/components/memory_flasher/memory_flasher.h"
#include "esphome/components/satellite1/satellite1.h"

namespace esphome::satellite1 {

class XMOSFlasher : public memory_flasher::MemoryFlasher, public Satellite1SPIService {
 public:
  void loop() override;

  bool init_flasher() override;
  bool deinit_flasher() override;
  void dump_flash_info() override;

  void erase_memory() override;
  void flash_remote_image(const std::string &url, const std::string &md5, const std::string &md5_url) override;
  void flash_embedded_image() override;

  bool flash_accessible() override;

 protected:
  /// Starts the requested action from the loop.
  void start_action_(memory_flasher::FlasherAction action);
  /// Reports an error that occurred before an action could be started.
  void fail_action_(memory_flasher::FlasherAction action, memory_flasher::FlasherError error);

  bool read_jedec_id_();
  bool enable_writing_();
  bool disable_writing_();
  bool chip_erase_();
  bool erase_sector_(int sector);
  bool wait_while_flash_busy_(uint32_t timeout_ms);
  bool read_page_(uint32_t byte_addr, uint8_t *buffer);
  bool write_page_(uint32_t byte_addr, const uint8_t *buffer);

  uint8_t manufacturer_id_{0};
  uint8_t memory_type_id_{0};
  uint8_t capacity_id_{0};
  size_t total_number_of_sectors_{0};

  bool init_flashing_();
  void deinit_flashing_();
  int flashing_step_();
  int erasing_step_();
  void publish_progress() override;

  memory_flasher::FlashImageReader *reader_{nullptr};
  md5::MD5Digest md5_receive_;

  uint32_t flashing_start_time_{0};
  uint32_t last_published_{0};
  size_t total_sectors_to_erase_{0};
  int current_sector_{-1};
  size_t total_number_of_bytes_{0};
  size_t bytes_remaining_{0};
  uint32_t page_pos_{0};

  uint8_t *reader_buffer_{nullptr};
  uint8_t *compare_buffer_{nullptr};

  /// Keeps the loop running without delay while an action is in progress, every loop call handles one page/sector.
  HighFrequencyLoopRequester high_freq_;
};

}  // namespace esphome::satellite1
