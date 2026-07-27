#include <stdint.h>

int64_t runes_linux_x86_64_syscall6(uint64_t number, uint64_t argument1,
                                    uint64_t argument2, uint64_t argument3,
                                    uint64_t argument4, uint64_t argument5,
                                    uint64_t argument6) {
  (void)number;
  (void)argument1;
  (void)argument2;
  (void)argument3;
  (void)argument4;
  (void)argument5;
  (void)argument6;
  return -38; /* ENOSYS */
}
