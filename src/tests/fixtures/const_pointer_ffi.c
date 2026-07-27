#include <stddef.h>
#include <stdint.h>

size_t runes_test_const_sum(const uint8_t *data, size_t length) {
  size_t total = 0;
  for (size_t index = 0; index < length; index++)
    total += data[index];
  return total;
}

uint8_t runes_test_const_first(const uint8_t *data) {
  return data[0];
}
