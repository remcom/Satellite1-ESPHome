#include "xmos_flashing.h"
#include "esphome/core/application.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#include <algorithm>
#include <cinttypes>
#include <endian.h>

namespace esphome::satellite1 {
using namespace memory_flasher;

static const char *const TAG = "xmos_flasher";

static const size_t FLASH_PAGE_SIZE = 256;
static const size_t FLASH_SECTOR_SIZE = 4096;
constexpr size_t FLASH_TOTAL_NUMBER_OF_SECTORS = 8388608 / FLASH_SECTOR_SIZE;

void XMOSFlasher::loop() {
  switch (this->state_) {
    case FLASHER_IDLE:
      // Nothing to do until an action is requested, see start_action_() / fail_action_().
      this->disable_loop();
      break;

    case FLASHER_INITIALIZING:
      if (this->init_flashing_()) {
        this->state_ = FLASHER_ERASING;
      } else {
        this->deinit_flashing_();
        this->state_ = FLASHER_ERROR_STATE;
      }
      break;

    case FLASHER_ERASING: {
      int remaining = this->erasing_step_();
      this->publish_progress();
      if (remaining == 0 && this->requested_action_ == ACTION_FULL_ERASE) {
        this->deinit_flashing_();
        this->state_ = FLASHER_SUCCESS_STATE;
      } else if (remaining == 0) {
        this->state_ = FLASHER_FLASHING;
      } else if (remaining < 0) {
        // Release the flash again, otherwise the XMOS stays in reset and all XMOS
        // communication (buttons, status) is blocked until reboot.
        this->deinit_flashing_();
        this->state_ = FLASHER_ERROR_STATE;
      }
      break;
    }

    case FLASHER_FLASHING: {
      int remaining = this->flashing_step_();
      this->publish_progress();
      if (remaining == 0) {
        ESP_LOGI(TAG, "Flashing finished in %" PRIu32 " s", (millis() - this->flashing_start_time_) / 1000);
        this->deinit_flashing_();
        this->state_ = FLASHER_SUCCESS_STATE;
      } else if (remaining < 0) {
        this->deinit_flashing_();
        this->state_ = FLASHER_ERROR_STATE;
      }
      break;
    }

    case FLASHER_SUCCESS_STATE:
    case FLASHER_ERROR_STATE:
      if (this->state_ == FLASHER_ERROR_STATE) {
        ESP_LOGE(TAG, "Failed with error code %u", this->error_code_);
      }
      this->publish();
      this->state_ = FLASHER_IDLE;
      this->high_freq_.stop();
      break;

    default:
      break;
  }
}

void XMOSFlasher::start_action_(FlasherAction action) {
  this->requested_action_ = action;
  this->error_code_ = FLASHER_OK;
  this->state_ = FLASHER_INITIALIZING;
  this->high_freq_.start();
  this->enable_loop();
}

void XMOSFlasher::fail_action_(FlasherAction action, FlasherError error) {
  this->requested_action_ = action;
  this->error_code_ = error;
  this->state_ = FLASHER_ERROR_STATE;
  this->enable_loop();
}

void XMOSFlasher::publish_progress() {
  const uint32_t now = App.get_loop_component_start_time();
  if ((now - this->last_published_) <= 1000) {
    return;
  }

  size_t sectors_done = this->current_sector_ < 0 ? 0 : static_cast<size_t>(this->current_sector_);
  size_t done;
  size_t total;
  if (this->requested_action_ == ACTION_FULL_ERASE) {
    done = sectors_done;
    total = this->total_sectors_to_erase_;
  } else {
    done = sectors_done + this->total_number_of_bytes_ - this->bytes_remaining_;
    total = this->total_number_of_bytes_ + this->total_sectors_to_erase_;
  }
  uint32_t progress = total == 0 ? 0 : done * 100 / total;
  progress = progress > 100 ? 100 : progress;
  // on_progress_update fires for every publish while flashing, only report changes
  if (progress == this->flashing_progress_ && this->state_ == this->last_published_state_) {
    return;
  }
  this->flashing_progress_ = progress;

  this->last_published_ = now;
  ESP_LOGD(TAG, "Progress: %u%%", this->flashing_progress_);
  this->publish();
}

bool XMOSFlasher::init_flasher() {
  ESP_LOGD(TAG, "Setting up XMOS flasher...");
  this->parent_->set_spi_flash_direct_access_mode(true);
  if (!this->read_jedec_id_()) {
    ESP_LOGE(TAG, "Couldn't read JEDEC ID, XMOS flash not accessible");
    return false;
  }
  this->dump_flash_info();
  return true;
}

bool XMOSFlasher::deinit_flasher() {
  ESP_LOGD(TAG, "Stopping XMOS flasher...");
  this->parent_->set_spi_flash_direct_access_mode(false);
  return true;
}

void XMOSFlasher::dump_flash_info() {
  ESP_LOGCONFIG(TAG,
                "Satellite1-Flasher:\n"
                "  JEDEC-manufacturer_id: 0x%02X\n"
                "  JEDEC-memory_type_id: 0x%02X\n"
                "  JEDEC-capacity_id: 0x%02X",
                this->manufacturer_id_, this->memory_type_id_, this->capacity_id_);
  if (this->capacity_id_ < 32) {
    ESP_LOGCONFIG(TAG, "  JEDEC-capacity: %" PRIu32 " bytes", static_cast<uint32_t>(1) << this->capacity_id_);
  }
}

bool XMOSFlasher::flash_accessible() {
  if (this->state_ != FLASHER_IDLE) {
    // Toggling the direct access mode would release the XMOS reset in the middle of an operation.
    ESP_LOGW(TAG, "XMOS flasher is busy, not probing the flash");
    return false;
  }
  this->parent_->set_spi_flash_direct_access_mode(true);
  bool got_id = this->read_jedec_id_();
  this->parent_->set_spi_flash_direct_access_mode(false);
  return got_id;
}

void XMOSFlasher::erase_memory() {
  if (this->state_ != FLASHER_IDLE) {
    ESP_LOGE(TAG, "XMOS flasher is busy, can't initiate erasing");
    return;
  }

  this->start_action_(ACTION_FULL_ERASE);
}

void XMOSFlasher::flash_remote_image(const std::string &url, const std::string &md5, const std::string &md5_url) {
  // Nothing may be stored before this check, it would change the url / checksum of the running action.
  if (this->state_ != FLASHER_IDLE) {
    ESP_LOGE(TAG, "XMOS flasher is busy, can't initiate new flash");
    return;
  }

  if (!this->validate_url_(url)) {
    this->fail_action_(ACTION_FLASH_REMOTE_IMAGE, BAD_URL);
    return;
  }

  std::string expected_md5 = md5;
  if (expected_md5.empty() && (!this->validate_url_(md5_url) || !this->http_get_md5_(md5_url, expected_md5))) {
    ESP_LOGE(TAG, "Couldn't receive expected md5 sum.");
    this->fail_action_(ACTION_FLASH_REMOTE_IMAGE, MD5_INVALID);
    return;
  }

  // Check before erasing anything: a bad checksum would only be detected after the flash was overwritten.
  if (!is_valid_md5_(expected_md5)) {
    ESP_LOGE(TAG, "Invalid expected md5 sum: '%s'", expected_md5.c_str());
    this->fail_action_(ACTION_FLASH_REMOTE_IMAGE, MD5_INVALID);
    return;
  }

  this->url_ = url;
  this->md5_expected_ = std::move(expected_md5);
  this->start_action_(ACTION_FLASH_REMOTE_IMAGE);
}

void XMOSFlasher::flash_embedded_image() {
  if (this->state_ != FLASHER_IDLE) {
    ESP_LOGE(TAG, "XMOS flasher is busy, can't initiate new flash");
    return;
  }

  if (this->embedded_image_.length == 0) {
    ESP_LOGE(TAG, "Didn't find embedded image!");
    this->fail_action_(ACTION_FLASH_EMBEDDED_IMAGE, NO_EMBEDDED_IMAGE_ERROR);
    return;
  }

  this->md5_expected_ = this->embedded_image_.md5;
  this->start_action_(ACTION_FLASH_EMBEDDED_IMAGE);
}

bool XMOSFlasher::read_jedec_id_() {
  this->enable_();
  this->transfer_byte_(0x9F);
  uint8_t manufacturer = this->transfer_byte_(0);
  uint8_t memory_type = this->transfer_byte_(0);
  uint8_t capacity = this->transfer_byte_(0);
  this->disable_();

  if (manufacturer && memory_type && capacity) {
    this->manufacturer_id_ = manufacturer;
    this->memory_type_id_ = memory_type;
    this->capacity_id_ = capacity;
    return true;
  }
  return false;
}

bool XMOSFlasher::wait_while_flash_busy_(uint32_t timeout_ms) {
  const uint32_t timeout_invoke = millis();
  const uint8_t busy = 1;

  while ((millis() - timeout_invoke) < timeout_ms) {
    this->enable_();
    this->transfer_byte_(0x05);
    uint8_t status = this->transfer_byte_(0x00);
    this->disable_();
    if ((status & busy) == 0) {
      return true;
    }
  }
  return false;
}

bool XMOSFlasher::enable_writing_() {
  // enable writing
  this->enable_();
  this->transfer_byte_(0x06);
  this->disable_();

  this->enable_();
  this->transfer_byte_(0x05);
  uint8_t status = this->transfer_byte_(0x00);
  this->disable_();
  const uint8_t wel = 2;
  return (status & wel) != 0;
}

bool XMOSFlasher::disable_writing_() {
  // disable writing
  this->enable_();
  this->transfer_byte_(0x04);
  this->disable_();
  return true;
}

bool XMOSFlasher::erase_sector_(int sector) {
  // erase 4kB sector
  assert(FLASH_SECTOR_SIZE == 4096);
  uint32_t u32 = htole32(sector * FLASH_SECTOR_SIZE);
  uint8_t *u8_ptr = (uint8_t *) &u32;

  if (!this->enable_writing_()) {
    return false;
  }

  this->enable_();
  this->transfer_byte_(0x20);
  this->transfer_byte_(*(u8_ptr + 2));
  this->transfer_byte_(*(u8_ptr + 1));
  this->transfer_byte_(*(u8_ptr));
  this->disable_();

  // this->disable_writing_();
  return true;
}

bool XMOSFlasher::write_page_(uint32_t byte_addr, const uint8_t *buffer) {
  if ((byte_addr & (FLASH_PAGE_SIZE - 1)) != 0) {
    ESP_LOGE(TAG, "Address needs to be page aligned (%zu).", FLASH_PAGE_SIZE);
    return false;
  }
  if (!this->enable_writing_()) {
    ESP_LOGE(TAG, "Couldn't enable writing");
    return false;
  }

  uint32_t u32 = htole32(byte_addr);
  uint8_t *u8_ptr = (uint8_t *) &u32;
  this->enable_();
  this->transfer_byte_(0x02);
  this->transfer_byte_(*(u8_ptr + 2));
  this->transfer_byte_(*(u8_ptr + 1));
  this->transfer_byte_(*(u8_ptr));
  for (size_t pos = 0; pos < FLASH_PAGE_SIZE; pos++) {
    this->transfer_byte_(*(buffer + pos));
  }
  this->disable_();

  if (!this->wait_while_flash_busy_(15)) {
    ESP_LOGE(TAG, "Writing page timeout");
    return false;
  }
  this->disable_writing_();
  return true;
}

bool XMOSFlasher::read_page_(uint32_t byte_addr, uint8_t *buffer) {
  if ((byte_addr & (FLASH_PAGE_SIZE - 1)) != 0) {
    return false;
  }
  uint32_t u32 = htole32(byte_addr);
  uint8_t *u8_ptr = (uint8_t *) &u32;
  this->enable_();
  this->transfer_byte_(0x0B);
  this->transfer_byte_(*(u8_ptr + 2));
  this->transfer_byte_(*(u8_ptr + 1));
  this->transfer_byte_(*(u8_ptr));
  this->transfer_byte_(0x00);
  for (size_t pos = 0; pos < FLASH_PAGE_SIZE; pos++) {
    *(buffer + pos) = this->transfer_byte_(0x00);
  }
  this->disable_();
  return true;
}

bool XMOSFlasher::init_flashing_() {
  if (!this->init_flasher()) {
    this->error_code_ = INIT_FLASH_ERROR;
    return false;
  }

  this->flashing_start_time_ = millis();

  switch (this->requested_action_) {
    case ACTION_FLASH_EMBEDDED_IMAGE:
      this->reader_ = new EmbeddedImageReader(this->embedded_image_);
      break;
    case ACTION_FLASH_REMOTE_IMAGE:
      this->reader_ = new HttpImageReader(this->http_request_, this->url_);
      break;
    case ACTION_FULL_ERASE:
      this->total_sectors_to_erase_ = FLASH_TOTAL_NUMBER_OF_SECTORS;
      this->current_sector_ = -1;
      this->flashing_progress_ = 0;
      return true;
  }

  if (!this->reader_->init_reader()) {
    this->error_code_ = INIT_READER_ERROR;
    return false;
  }

  // NOLINTBEGIN(cppcoreguidelines-no-malloc): exceptions are disabled in this build
  // (CONFIG_COMPILER_CXX_EXCEPTIONS=n), so a std::vector allocation failure would abort()
  // instead of letting us report INIT_FLASH_ERROR and bail out gracefully.
  this->reader_buffer_ = (uint8_t *) malloc(FLASH_PAGE_SIZE);
  if (this->reader_buffer_ == nullptr) {
    ESP_LOGE(TAG, "Couldn't allocate memory");
    this->error_code_ = INIT_FLASH_ERROR;
    return false;
  }

  this->compare_buffer_ = (uint8_t *) malloc(FLASH_PAGE_SIZE);
  if (this->compare_buffer_ == nullptr) {
    ESP_LOGE(TAG, "Couldn't allocate memory");
    this->error_code_ = INIT_FLASH_ERROR;
    return false;
  }
  // NOLINTEND(cppcoreguidelines-no-malloc)

  ESP_LOGD(TAG, "MD5 expected: %s", this->md5_expected_.c_str());

  this->flashing_progress_ = 0;
  this->md5_receive_.init();

  size_t size_in_bytes = this->reader_->get_image_size();
  if (size_in_bytes == 0) {
    // e.g. HTTP response without Content-Length; the image size is needed to know what to erase
    ESP_LOGE(TAG, "Image size unknown or empty");
    this->error_code_ = INIT_READER_ERROR;
    return false;
  }
  if (size_in_bytes > FLASH_TOTAL_NUMBER_OF_SECTORS * FLASH_SECTOR_SIZE) {
    ESP_LOGE(TAG, "Image size %zu exceeds flash size", size_in_bytes);
    this->error_code_ = INIT_READER_ERROR;
    return false;
  }
  size_t size_in_pages = ((size_in_bytes + FLASH_PAGE_SIZE - 1) / FLASH_PAGE_SIZE);
  size_t size_in_sectors = ((size_in_pages * FLASH_PAGE_SIZE + FLASH_SECTOR_SIZE - 1) / FLASH_SECTOR_SIZE);

  this->total_number_of_bytes_ = size_in_bytes;
  this->bytes_remaining_ = size_in_bytes;
  this->page_pos_ = 0;

  this->total_sectors_to_erase_ = size_in_sectors;
  this->current_sector_ = -1;

  return true;
}

void XMOSFlasher::deinit_flashing_() {
  // NOLINTBEGIN(cppcoreguidelines-no-malloc): pairs with the malloc() in init_flashing_().
  if (this->reader_buffer_) {
    free(this->reader_buffer_);
    this->reader_buffer_ = nullptr;
  }

  if (this->compare_buffer_) {
    free(this->compare_buffer_);
    this->compare_buffer_ = nullptr;
  }
  // NOLINTEND(cppcoreguidelines-no-malloc)

  if (this->reader_) {
    this->reader_->deinit_reader();
    delete this->reader_;
    this->reader_ = nullptr;
  }

  this->md5_expected_.clear();

  delay(5);
  this->deinit_flasher();
}

int XMOSFlasher::erasing_step_() {
  if (!this->wait_while_flash_busy_(1)) {
    return this->total_sectors_to_erase_ - this->current_sector_;
  }

  this->current_sector_++;
  if (this->current_sector_ < this->total_sectors_to_erase_) {
    if (!this->erase_sector_(this->current_sector_)) {
      this->error_code_ = WRITE_TO_FLASH_ERROR;
      return -1;
    }
  }

  return this->total_sectors_to_erase_ - this->current_sector_;
}

int XMOSFlasher::flashing_step_() {
  // Never read past the announced image size, bytes_remaining_ must not underflow.
  size_t to_read = std::min(FLASH_PAGE_SIZE, this->bytes_remaining_);
  int bytes_read = this->reader_->read_image_block(this->reader_buffer_, to_read);
  if (bytes_read < 0) {
    ESP_LOGE(TAG, "Stream closed");
    this->error_code_ = CONNECTION_ERROR;
    return -1;
  }
  if (static_cast<size_t>(bytes_read) != to_read) {
    ESP_LOGE(TAG, "Image ended early, %zu bytes missing", this->bytes_remaining_ - bytes_read);
    this->error_code_ = CONNECTION_ERROR;
    return -1;
  }

  this->md5_receive_.add(this->reader_buffer_, bytes_read);
  this->bytes_remaining_ -= bytes_read;
  if (static_cast<size_t>(bytes_read) != FLASH_PAGE_SIZE) {
    // it's the last page to flash
    // fill it up with zeros
    memset(this->reader_buffer_ + bytes_read, 0, FLASH_PAGE_SIZE - bytes_read);
  }

  uint32_t page_pos = this->page_pos_;
  if (!this->write_page_(page_pos, this->reader_buffer_)) {
    ESP_LOGE(TAG, "Error while writing page %" PRIu32 ", retrying...", page_pos);
  }

  // read back the page that has just been written
  this->read_page_(page_pos, this->compare_buffer_);

  if (memcmp(this->reader_buffer_, this->compare_buffer_, FLASH_PAGE_SIZE) != 0) {
    // not equal, give it a second try
    if (!this->write_page_(page_pos, this->reader_buffer_)) {
      ESP_LOGE(TAG, "Error while writing page %" PRIu32 ", giving up...", page_pos);
      this->error_code_ = WRITE_TO_FLASH_ERROR;
      return -1;
    }

    this->read_page_(page_pos, this->compare_buffer_);
    if (memcmp(this->reader_buffer_, this->compare_buffer_, FLASH_PAGE_SIZE) != 0) {
      ESP_LOGE(TAG, "Read page mismatch, page addr: %" PRIu32, page_pos);
      this->error_code_ = WRITE_TO_FLASH_ERROR;
      return -1;
    }
  }

  this->page_pos_ += FLASH_PAGE_SIZE;

  if (this->bytes_remaining_ == 0) {
    char md5_receive_str[MD5_SIZE + 1];
    this->md5_receive_.calculate();
    this->md5_receive_.get_hex(md5_receive_str);

    if (!this->md5_receive_.equals_hex(this->md5_expected_.c_str())) {
      ESP_LOGE(TAG, "MD5 computed: %s - Aborting due to MD5 mismatch", md5_receive_str);
      this->error_code_ = MD5_MISMATCH_ERROR;
      return -1;
    }
    ESP_LOGD(TAG, "MD5 computed: %s - Matches!", md5_receive_str);
  }

  return this->bytes_remaining_;
}

}  // namespace esphome::satellite1