#ifndef RUNES_TARGET_H
#define RUNES_TARGET_H

#include <stdbool.h>

typedef enum {
  RUNES_ARCH_X86_64,
} RunesTargetArch;

typedef enum {
  RUNES_OS_LINUX,
  RUNES_OS_RUNES,
} RunesTargetOs;

typedef enum {
  RUNES_ENV_GNU,
  RUNES_ENV_NONE,
} RunesTargetEnv;

typedef struct {
  const char *triple;
  RunesTargetArch arch;
  RunesTargetOs os;
  RunesTargetEnv env;
  unsigned pointer_width;
  bool little_endian;
  bool hosted;
} RunesTarget;

const char *runes_target_host_triple(void);
bool runes_target_parse(const char *triple, RunesTarget *target);
const char *runes_target_arch_name(const RunesTarget *target);
const char *runes_target_os_name(const RunesTarget *target);
const char *runes_target_env_name(const RunesTarget *target);

#endif
