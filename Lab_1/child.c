#include <unistd.h>
#include <fcntl.h>
#include <limits.h>
#include <errno.h>
#include <stddef.h>

#define BUFFER_SIZE 4096

typedef struct {
    long long magnitude;
    int sign;
    int token_state;
    int value_present;
    int quotient;
    int pending_line;
} DivisionState;

static int send_all(int fd, const char *buffer, size_t length)
{
    size_t sent = 0;
    while (sent < length) {
        ssize_t count = write(fd, buffer + sent, length - sent);
        if (count < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        if (count == 0) return -1;
        sent += (size_t)count;
    }
    return 0;
}

static int format_integer(int number, char *buffer)
{
    long long value = number;
    int length = 0;
    char digits[32];
    int count = 0;

    if (value < 0) {
        buffer[length++] = '-';
        value = -value;
    }

    do {
        digits[count++] = (char)('0' + value % 10);
        value /= 10;
    } while (value != 0);

    while (count > 0) buffer[length++] = digits[--count];
    buffer[length++] = '\n';
    return length;
}

static int finish_integer(DivisionState *state)
{
    if (state->token_state == 0) return 0;
    if (state->token_state != 2) return -1;

    int value = (int)(state->sign * state->magnitude);
    if (!state->value_present) {
        state->quotient = value;
        state->value_present = 1;
    } else {
        if (value == 0) {
            const char message[] = "Division by zero\n";
            send_all(STDERR_FILENO, message, sizeof(message) - 1);
            return -1;
        }
        if (state->quotient == INT_MIN && value == -1) return -1;
        state->quotient /= value;
    }

    state->token_state = 0;
    state->magnitude = 0;
    state->sign = 1;
    return 0;
}

static int finish_line(DivisionState *state, int output_fd)
{
    if (finish_integer(state) < 0 || !state->value_present) return -1;

    char answer[32];
    int length = format_integer(state->quotient, answer);

    if (send_all(output_fd, answer, (size_t)length) < 0 ||
        send_all(STDOUT_FILENO, answer, (size_t)length) < 0) return -1;

    state->magnitude = 0;
    state->sign = 1;
    state->token_state = 0;
    state->value_present = 0;
    state->quotient = 0;
    state->pending_line = 0;
    return 0;
}

static int process_character(DivisionState *state, char ch, int output_fd)
{
    if (ch == '\n') return finish_line(state, output_fd);

    state->pending_line = 1;
    if (ch == ' ' || ch == '\t' || ch == '\r') return finish_integer(state);

    if (ch == '+' || ch == '-') {
        if (state->token_state != 0) return -1;
        state->sign = ch == '-' ? -1 : 1;
        state->magnitude = 0;
        state->token_state = 1;
        return 0;
    }

    if (ch < '0' || ch > '9') return -1;

    if (state->token_state == 0) {
        state->sign = 1;
        state->magnitude = 0;
    }

    int digit = ch - '0';
    long long limit = state->sign < 0 ? (long long)INT_MAX + 1 : INT_MAX;
    if (state->magnitude > (limit - digit) / 10) return -1;

    state->magnitude = state->magnitude * 10 + digit;
    state->token_state = 2;
    return 0;
}

int main(int argc, char *argv[])
{
    if (argc < 2) return 1;

    int output_fd = open(argv[1], O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (output_fd < 0) return 1;

    DivisionState state = {0};
    char buffer[BUFFER_SIZE];
    int failed = 0;

    for (;;) {
        ssize_t count = read(STDIN_FILENO, buffer, sizeof(buffer));
        if (count < 0) {
            if (errno == EINTR) continue;
            failed = 1;
            break;
        }
        if (count == 0) break;

        for (ssize_t i = 0; i < count; ++i) {
            if (process_character(&state, buffer[i], output_fd) < 0) {
                failed = 1;
                break;
            }
        }
        if (failed) break;
    }

    if (!failed && state.pending_line && finish_line(&state, output_fd) < 0)
        failed = 1;

    if (close(output_fd) < 0) failed = 1;
    return failed ? 1 : 0;
}