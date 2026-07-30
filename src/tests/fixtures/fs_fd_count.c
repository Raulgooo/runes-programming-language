#include <dirent.h>
#include <stdint.h>

int32_t runes_test_open_fd_count(void) {
  DIR *directory = opendir("/proc/self/fd");
  if (!directory)
    return -1;
  int32_t count = -1; /* Do not count the descriptor used by opendir itself. */
  while (readdir(directory))
    count++;
  closedir(directory);
  return count;
}
