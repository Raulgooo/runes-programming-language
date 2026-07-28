#include <stdint.h>

static int stdout_attempts;
static int stderr_attempts;
static int stdin_attempts;

int64_t runes_linux_x86_64_syscall6(
    uint64_t number,
    uint64_t argument1,
    uint64_t argument2,
    uint64_t argument3,
    uint64_t argument4,
    uint64_t argument5,
    uint64_t argument6
) {
    (void)argument2;
    (void)argument4;
    (void)argument5;
    (void)argument6;

    if (number == 1 && argument1 == 1) {
        stdout_attempts++;
        if (stdout_attempts == 1) {
            return -4;
        }
        if (stdout_attempts == 2) {
            return 1;
        }
        if (stdout_attempts == 5) {
            return -32;
        }
        return (int64_t)argument3;
    }

    if (number == 1 && argument1 == 2) {
        stderr_attempts++;
        switch (stderr_attempts) {
        case 1:
            return 0;
        case 2:
            return -32;
        case 3:
            return -11;
        case 4:
            return -13;
        case 5:
            return -2;
        case 6:
            return -9;
        case 7:
            return -38;
        default:
            return -123;
        }
    }

    if (number == 0 && argument1 == 0) {
        stdin_attempts++;
        if (stdin_attempts <= 2) {
            return -4;
        }
        if (stdin_attempts == 3) {
            uint8_t *buffer = (uint8_t *)(uintptr_t)argument2;
            if (argument3 > 0) {
                buffer[0] = 90;
                return 1;
            }
        }
        return 0;
    }

    return -38;
}

int32_t runes_test_stdout_attempts(void) {
    return stdout_attempts;
}

int32_t runes_test_stderr_attempts(void) {
    return stderr_attempts;
}

int32_t runes_test_stdin_attempts(void) {
    return stdin_attempts;
}
