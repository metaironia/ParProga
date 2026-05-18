#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <time.h>
#include <string.h>
#include <errno.h>
#include <signal.h>

const int ITERATIONS = 10000;
const int BUF_SIZE = 64;

static inline double timespec_to_ns(struct timespec t) {
    return t.tv_sec * 1e9 + t.tv_nsec;
}

static void timespec_diff(struct timespec *res, struct timespec *start, struct timespec *end) {
    res->tv_sec = end->tv_sec - start->tv_sec;
    res->tv_nsec = end->tv_nsec - start->tv_nsec;
    if (res->tv_nsec < 0) {
        res->tv_sec--;
        res->tv_nsec += 1000000000L;
    }
}

static ssize_t robust_read(int fd, void *buf, size_t count) {
    size_t done = 0;
    while (done < count) {
        ssize_t ret = read(fd, (char *)buf + done, count - done);
        if (ret < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        if (ret == 0) return (ssize_t)done;
        done += ret;
    }
    return (ssize_t)count;
}

static ssize_t robust_write(int fd, const void *buf, size_t count) {
    size_t done = 0;
    while (done < count) {
        ssize_t ret = write(fd, (const char *)buf + done, count - done);
        if (ret < 0) {
            if (errno == EINTR) continue;
            if (errno == EPIPE) return -1;
            return -1;
        }
        done += ret;
    }
    return (ssize_t)count;
}

int main(void) {
    signal(SIGPIPE, SIG_IGN);

    int p2c[2], c2p[2];
    if (pipe(p2c) < 0 || pipe(c2p) < 0) {
        perror("pipe"); return 1;
    }

    pid_t pid = fork();
    if (pid < 0) { perror("fork"); return 1; }

    if (pid == 0) {
        close(p2c[1]); close(c2p[0]);
        char buf[BUF_SIZE];
        for (int i = 0; i < ITERATIONS; ++i) {
            if (robust_read(p2c[0], buf, BUF_SIZE) != BUF_SIZE) break;
            if (robust_write(c2p[1], buf, BUF_SIZE) != BUF_SIZE) break;
        }
        close(p2c[0]); close(c2p[1]);
        _exit(0);
    }

    close(p2c[0]); close(c2p[1]);
    char buf[BUF_SIZE];
    memset(buf, 'A', BUF_SIZE);

    struct timespec start, end, diff;
    double min_ns = 1e18, max_ns = 0.0, sum_ns = 0.0;
    int success = 0;

    for (int i = 0; i < ITERATIONS; ++i) {
        clock_gettime(CLOCK_MONOTONIC, &start);
        if (robust_write(p2c[1], buf, BUF_SIZE) != BUF_SIZE) break;
        if (robust_read(c2p[0], buf, BUF_SIZE) != BUF_SIZE) break;
        clock_gettime(CLOCK_MONOTONIC, &end);

        timespec_diff(&diff, &start, &end);
        double ns = timespec_to_ns(diff);
        if (ns < min_ns) min_ns = ns;
        if (ns > max_ns) max_ns = ns;
        sum_ns += ns;
        success++;
    }

    close(p2c[1]); close(c2p[0]);
    waitpid(pid, NULL, 0);

    printf("=== IPC через pipe (процессы) ===\n");
    printf("Итераций: %d | Размер буфера: %d B\n", success, BUF_SIZE);
    if (success > 0) {
        printf("Среднее: %.2f ns\n", sum_ns / success);
        printf("Мин:     %.2f ns\n", min_ns);
        printf("Макс:    %.2f ns\n", max_ns);
    } else {
        fprintf(stderr, "Ошибка: не выполнено ни одной итерации.\n");
    }

    return 0;
}