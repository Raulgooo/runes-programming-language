#include "target.h"
#include <string.h>

const char *runes_target_host_triple(void) {
#if defined(__x86_64__) && defined(__linux__)
  return "x86_64-unknown-linux-gnu";
#else
  return NULL;
#endif
}

bool runes_target_parse(const char *triple, RunesTarget *target) {
  if (!triple || !target)
    return false;

  RunesTarget parsed = {
      .triple = triple,
      .arch = RUNES_ARCH_X86_64,
      .pointer_width = 64,
      .little_endian = true,
  };

  if (strcmp(triple, "x86_64-unknown-linux-gnu") == 0) {
    parsed.os = RUNES_OS_LINUX;
    parsed.env = RUNES_ENV_GNU;
    parsed.hosted = true;
  } else if (strcmp(triple, "x86_64-unknown-linux-none") == 0) {
    parsed.os = RUNES_OS_LINUX;
    parsed.env = RUNES_ENV_NONE;
    parsed.hosted = false;
  } else if (strcmp(triple, "x86_64-unknown-runes-none") == 0) {
    parsed.os = RUNES_OS_RUNES;
    parsed.env = RUNES_ENV_NONE;
    parsed.hosted = false;
  } else {
    return false;
  }

  *target = parsed;
  return true;
}

const char *runes_target_arch_name(const RunesTarget *target) {
  (void)target;
  return "x86_64";
}

const char *runes_target_os_name(const RunesTarget *target) {
  return target->os == RUNES_OS_LINUX ? "linux" : "runes";
}

const char *runes_target_env_name(const RunesTarget *target) {
  return target->env == RUNES_ENV_GNU ? "gnu" : "none";
}
