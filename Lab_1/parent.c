#define _POSIX_C_SOURCE 200809L

#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>
#include <stddef.h>

#define CHUNK_SIZE 4096
#define PATH_BUFFER 4096

extern char **environ;

static int send_all(int fd, const char *data, size_t length)
{
    size_t sent = 0;

    while (sent < length) {
        ssize_t count = write(fd, data + sent, length - sent);
        if (count < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        if (count == 0) return -1;
        sent += (size_t)count;
    }
    return 0;
}

static int read_output_path(char *path, size_t capacity)
{
    size_t length = 0;
    char symbol;

    for (;;) {
        ssize_t count = read(STDIN_FILENO, &symbol, 1);
        if (count < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        if (count == 0 || symbol == '\n') break;
        if (symbol == '\0' || length + 1 >= capacity) return -1;
        path[length++] = symbol;
    }

    if (length == 0) return -1;
    path[length] = '\0';
    return 0;
}

static int print_answer(int channel)
{
    char response[128];

    for (;;) {
        ssize_t received = read(channel, response, sizeof(response));
        if (received < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        if (received == 0) return -1;

        if (send_all(STDOUT_FILENO, response, (size_t)received) < 0)
            return -1;

        if (response[received - 1] == '\n') return 0;
    }
}

int main(void)
{
    char output_path[PATH_BUFFER];
    int request_pipe[2];
    int reply_pipe[2];

    if (read_output_path(output_path, sizeof(output_path)) < 0) return 1;

    if (pipe(request_pipe) < 0) return 1;
    if (pipe(reply_pipe) < 0) {
        close(request_pipe[0]);
        close(request_pipe[1]);
        return 1;
    }

    pid_t child_pid = fork();
    if (child_pid < 0) {
        close(request_pipe[0]);
        close(request_pipe[1]);
        close(reply_pipe[0]);
        close(reply_pipe[1]);
        return 1;
    }

    if (child_pid == 0) {
        if (close(request_pipe[1]) < 0 || close(reply_pipe[0]) < 0)
            _exit(1);

        if (dup2(request_pipe[0], STDIN_FILENO) < 0) _exit(1);
        if (dup2(reply_pipe[1], STDOUT_FILENO) < 0) _exit(1);
        if (close(request_pipe[0]) < 0 || close(reply_pipe[1]) < 0)
            _exit(1);

        char *const arguments[] = {"./child", output_path, NULL};
        execve("./child", arguments, environ);
        _exit(1);
    }

    int error_found = 0;
    if (close(request_pipe[0]) < 0) error_found = 1;
    if (close(reply_pipe[1]) < 0) error_found = 1;

    struct sigaction ignore_pipe = {0};
    ignore_pipe.sa_handler = SIG_IGN;
    if (sigaction(SIGPIPE, &ignore_pipe, NULL) < 0) {
        error_found = 1;
    }

    char input[CHUNK_SIZE];
    char pending[CHUNK_SIZE];
    size_t pending_size = 0;
    int unfinished_line = 0;

    while (!error_found) {
        ssize_t received = read(STDIN_FILENO, input, sizeof(input));
        if (received < 0) {
            if (errno == EINTR) continue;
            error_found = 1;
            break;
        }
        if (received == 0) break;

        for (ssize_t i = 0; i < received; ++i) {
            char symbol = input[i];
            pending[pending_size++] = symbol;
            if (symbol != '\n') unfinished_line = 1;

            if (pending_size == sizeof(pending) || symbol == '\n') {
                if (send_all(request_pipe[1], pending, pending_size) < 0) {
                    error_found = 1;
                    break;
                }
                pending_size = 0;
            }

            if (symbol == '\n') {
                if (print_answer(reply_pipe[0]) < 0) {
                    error_found = 1;
                    break;
                }
                unfinished_line = 0;
            }
        }
    }

    if (!error_found && unfinished_line) {
        if ((pending_size != 0 &&
             send_all(request_pipe[1], pending, pending_size) < 0) ||
            send_all(request_pipe[1], "\n", 1) < 0 ||
            print_answer(reply_pipe[0]) < 0) {
            error_found = 1;
        }
    }

    if (close(request_pipe[1]) < 0) error_found = 1;
    if (close(reply_pipe[0]) < 0) error_found = 1;

    int child_status = 0;
    while (waitpid(child_pid, &child_status, 0) < 0) {
        if (errno == EINTR) continue;
        return 1;
    }

    if (error_found || !WIFEXITED(child_status) ||
        WEXITSTATUS(child_status) != 0) return 1;

    return 0;
}