#include "sat_gpio.h"
#include "esphome/core/helpers.h"

namespace esphome::satellite1 {

// Indexed by pin / XMOS_GPIO_PINS_PER_PORT
static const char *const PORT_NAMES[] = {"INPUT_A", "INPUT_B", "OUTPUT_A"};

size_t Satellite1GPIOPin::dump_summary(char *buffer, size_t len) const {
  return buf_append_printf(buffer, len, 0, "%s pin %u via XMOS", PORT_NAMES[this->pin_ / XMOS_GPIO_PINS_PER_PORT],
                           this->pin_ % XMOS_GPIO_PINS_PER_PORT);
}

}  // namespace esphome::satellite1
