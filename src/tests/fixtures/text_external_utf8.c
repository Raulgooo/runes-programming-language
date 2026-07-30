#include "runtime.h"

RunesStr runes_test_external_text(uint32_t kind) {
  static const uint8_t overlong[] = {0xc0, 0x80};
  static const uint8_t surrogate[] = {0xed, 0xa0, 0x80};
  static const uint8_t truncated[] = {0xf0, 0x9f, 0x8c};
  static const uint8_t continuation[] = {0x80};
  static const uint8_t valid[] = {'a', 0, 0xc3, 0xa9};

  switch (kind) {
  case 0:
    return (RunesStr){.ptr = overlong, .len = sizeof(overlong)};
  case 1:
    return (RunesStr){.ptr = surrogate, .len = sizeof(surrogate)};
  case 2:
    return (RunesStr){.ptr = truncated, .len = sizeof(truncated)};
  case 3:
    return (RunesStr){.ptr = continuation, .len = sizeof(continuation)};
  default:
    return (RunesStr){.ptr = valid, .len = sizeof(valid)};
  }
}
