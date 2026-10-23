#include "memory_flasher.h"

#include "esphome/core/application.h"
#include "esphome/core/log.h"

namespace esphome::memory_flasher {

static const char *const TAG = "memory_flasher";

void MemoryFlasher::dump_config() {
  if (this->has_image_embedded()) {
    ESP_LOGCONFIG(TAG, "Embedded Image:");
    ImageVersion v = this->embedded_image_.version;
    ESP_LOGCONFIG(TAG, "    Version: %d.%d.%d", v.major, v.minor, v.patch);
  }
}

bool MemoryFlasher::is_valid_md5_(const std::string &md5) {
  uint8_t digest[MD5_SIZE / 2];
  return md5.size() == MD5_SIZE && parse_hex(md5, digest, sizeof(digest));
}

bool MemoryFlasher::http_get_md5_(const std::string &md5_url, std::string &md5) {
  if (md5_url.empty() || this->http_request_ == nullptr) {
    return false;
  }

  ESP_LOGI(TAG, "Connecting to: %s", md5_url.c_str());
  auto container = this->http_request_->get(md5_url);
  if (container == nullptr) {
    ESP_LOGE(TAG, "Failed to connect to MD5 URL");
    return false;
  }
  if (!http_request::is_success(container->status_code)) {
    ESP_LOGE(TAG, "Failed to get MD5 file, HTTP status: %d", container->status_code);
    container->end();
    return false;
  }
  size_t length = container->content_length;
  if (length > 0 && length < MD5_SIZE) {
    ESP_LOGE(TAG, "MD5 file must be at least %u bytes; %zu bytes reported by HTTP server. Aborting", MD5_SIZE, length);
    container->end();
    return false;
  }

  char md5_buf[MD5_SIZE];
  auto result = http_request::http_read_fully(container.get(), reinterpret_cast<uint8_t *>(md5_buf), MD5_SIZE, MD5_SIZE,
                                              this->http_request_->get_timeout());
  size_t read_len = container->get_bytes_read();
  container->end();

  if (result.status != http_request::HttpReadStatus::OK || read_len < MD5_SIZE) {
    ESP_LOGE(TAG, "Failed to read MD5 file (read %zu of %u bytes)", read_len, MD5_SIZE);
    return false;
  }

  std::string received(md5_buf, MD5_SIZE);
  if (!is_valid_md5_(received)) {
    ESP_LOGE(TAG, "MD5 file doesn't start with a valid MD5 sum");
    return false;
  }
  md5 = std::move(received);
  ESP_LOGV(TAG, "MD5 expected: %s", md5.c_str());
  return true;
}

bool MemoryFlasher::validate_url_(const std::string &url) {
  if (!str_startswith(url, "http://") && !str_startswith(url, "https://")) {
    ESP_LOGE(TAG, "URL must start with 'http://' or 'https://'");
    return false;
  }
  return true;
}

bool HttpImageReader::init_reader() {
  if (this->url_.empty() || this->http_request_ == nullptr) {
    return false;
  }

  ESP_LOGI(TAG, "Connecting to: %s", this->url_.c_str());

  this->container_ = this->http_request_->get(this->url_);
  if (this->container_ == nullptr) {
    ESP_LOGE(TAG, "Failed to connect to image URL");
    return false;
  }
  if (!http_request::is_success(this->container_->status_code)) {
    ESP_LOGE(TAG, "Failed to get image, HTTP status: %d", this->container_->status_code);
    return false;
  }
  return true;
}

bool HttpImageReader::deinit_reader() {
  if (this->container_ != nullptr) {
    this->container_->end();
    this->container_ = nullptr;
  }
  return true;
}

int HttpImageReader::read_image_block(uint8_t *buffer, size_t block_size) {
  if (this->container_ == nullptr) {
    return -1;
  }
  // A single read() may return less than requested, keep reading until the block is full or the transfer is done.
  size_t bytes_read_before = this->container_->get_bytes_read();
  auto result = http_request::http_read_fully(this->container_.get(), buffer, block_size, block_size,
                                              this->http_request_->get_timeout());
  if (result.status != http_request::HttpReadStatus::OK) {
    ESP_LOGE(
        TAG, "Reading image failed (%s, error %d)",
        result.status == http_request::HttpReadStatus::TIMEOUT ? LOG_STR_LITERAL("timeout") : LOG_STR_LITERAL("error"),
        result.error_code);
    return -1;
  }
  size_t bytes_read = this->container_->get_bytes_read() - bytes_read_before;
  ESP_LOGVV(TAG, "bytes_read_ = %zu, body_length_ = %zu, block = %zu", this->container_->get_bytes_read(),
            this->container_->content_length, bytes_read);
  return static_cast<int>(bytes_read);
}

}  // namespace esphome::memory_flasher
