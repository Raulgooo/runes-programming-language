#include <stdint.h>
#include <string.h>
#include <unistd.h>

typedef struct {
  uint64_t device;
  uint64_t inode;
  uint64_t link_count;
  uint32_t mode;
  uint32_t user;
  uint32_t group;
  int32_t padding;
  uint64_t special_device;
  int64_t size;
  int64_t block_size;
  int64_t blocks;
  int64_t accessed_seconds;
  int64_t accessed_nanoseconds;
  int64_t modified_seconds;
  int64_t modified_nanoseconds;
  int64_t changed_seconds;
  int64_t changed_nanoseconds;
  int64_t reserved[3];
} LinuxX86_64Stat;

_Static_assert(sizeof(LinuxX86_64Stat) == 144,
               "Linux x86-64 stat layout changed");

static int32_t open_attempts;
static int32_t close_attempts;
static int32_t metadata_attempts;
static int32_t read_attempts;
static int32_t write_attempts;

int64_t runes_linux_x86_64_syscall6(
    uint64_t number,
    uint64_t argument1,
    uint64_t argument2,
    uint64_t argument3,
    uint64_t argument4,
    uint64_t argument5,
    uint64_t argument6
) {
  (void)argument4;
  (void)argument5;
  (void)argument6;

  if (number == 1 && argument1 == 1)
    return write(1, (const void *)(uintptr_t)argument2, (size_t)argument3);

  if (number == 257) {
    open_attempts++;
    if (open_attempts == 1)
      return 40;
    if (open_attempts == 2)
      return 41;
    if (open_attempts == 3)
      return -13;
    return -17;
  }

  if (number == 5 && argument1 == 40) {
    metadata_attempts++;
    LinuxX86_64Stat value;
    memset(&value, 0, sizeof value);
    value.mode = 0100640;
    value.size = 123;
    value.modified_seconds = 99;
    memcpy((void *)(uintptr_t)argument2, &value, sizeof value);
    return 0;
  }

  if (number == 75 && argument1 == 40)
    return -95;

  if (number == 0 && argument1 == 41) {
    read_attempts++;
    if (read_attempts == 1)
      return -4;
    uint8_t *bytes = (uint8_t *)(uintptr_t)argument2;
    if (read_attempts == 2) {
      bytes[0] = 0x61;
      return 1;
    }
    bytes[0] = 0x62;
    bytes[1] = 0x63;
    return 2;
  }

  if (number == 1 && argument1 == 41) {
    write_attempts++;
    if (write_attempts == 1)
      return -4;
    if (write_attempts == 2)
      return 1;
    return (int64_t)argument3;
  }

  if (number == 3 && argument1 == 40) {
    close_attempts++;
    return -4;
  }
  if (number == 3 && argument1 == 41) {
    close_attempts++;
    return 0;
  }

  return -38;
}

int32_t runes_test_fs_open_attempts(void) {
  return open_attempts;
}

int32_t runes_test_fs_close_attempts(void) {
  return close_attempts;
}

int32_t runes_test_fs_metadata_attempts(void) {
  return metadata_attempts;
}

int32_t runes_test_fs_read_attempts(void) {
  return read_attempts;
}

int32_t runes_test_fs_write_attempts(void) {
  return write_attempts;
}
