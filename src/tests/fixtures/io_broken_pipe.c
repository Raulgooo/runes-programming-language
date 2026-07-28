#include <stdlib.h>
#include <unistd.h>

__attribute__((constructor))
static void install_closed_stdout_pipe(void) {
    int descriptors[2];
    if (pipe(descriptors) != 0) {
        _exit(120);
    }
    if (close(descriptors[0]) != 0) {
        _exit(121);
    }
    if (dup2(descriptors[1], STDOUT_FILENO) < 0) {
        _exit(122);
    }
    if (close(descriptors[1]) != 0) {
        _exit(123);
    }
}
