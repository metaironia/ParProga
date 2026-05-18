#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>
#include <string.h>
#include <errno.h>

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

typedef struct {
    pthread_mutex_t mtx;
    pthread_cond_t  cond;
    int state;
    char data[BUF_SIZE];
    char response[BUF_SIZE];
} shared_ctx_t;

void* worker_thread(void* arg) {
    shared_ctx_t* ctx = (shared_ctx_t*)arg;

    pthread_mutex_lock(&ctx->mtx);
    while (ctx->state != 1)
        pthread_cond_wait(&ctx->cond, &ctx->mtx);
    memcpy(ctx->response, ctx->data, BUF_SIZE);
    ctx->state = 2;
    pthread_cond_broadcast(&ctx->cond);
    pthread_mutex_unlock(&ctx->mtx);

    for (int i = 0; i < ITERATIONS; ++i) {
        pthread_mutex_lock(&ctx->mtx);
        while (ctx->state != 1)
            pthread_cond_wait(&ctx->cond, &ctx->mtx);

        memcpy(ctx->response, ctx->data, BUF_SIZE);
        ctx->state = 2;

        pthread_cond_broadcast(&ctx->cond);
        pthread_mutex_unlock(&ctx->mtx);
    }
    return NULL;
}

int main(void) {
    shared_ctx_t ctx = {0};
    
    if (pthread_mutex_init(&ctx.mtx, NULL) != 0) {
        perror("pthread_mutex_init");
        return 1;
    }
    if (pthread_cond_init(&ctx.cond, NULL) != 0) {
        perror("pthread_cond_init");
        pthread_mutex_destroy(&ctx.mtx);
        return 1;
    }

    pthread_t tid;
    if (pthread_create(&tid, NULL, worker_thread, &ctx) != 0) {
        perror("pthread_create");
        pthread_cond_destroy(&ctx.cond);
        pthread_mutex_destroy(&ctx.mtx);
        return 1;
    }

    pthread_mutex_lock(&ctx.mtx);
    ctx.data[0] = (char)0xAA;
    ctx.state = 1;
    pthread_cond_broadcast(&ctx.cond);
    while (ctx.state != 2)
        pthread_cond_wait(&ctx.cond, &ctx.mtx);
    ctx.state = 0;
    pthread_mutex_unlock(&ctx.mtx);

    struct timespec start, end, diff;
    double min_ns = 1e18, max_ns = 0.0, sum_ns = 0.0;
    int success = 0;

    for (int i = 0; i < ITERATIONS; ++i) {
        pthread_mutex_lock(&ctx.mtx);
        ctx.data[0] = (char)(i % 256);
        ctx.state = 1;

        clock_gettime(CLOCK_MONOTONIC, &start);
        pthread_cond_broadcast(&ctx.cond);

        while (ctx.state != 2)
            pthread_cond_wait(&ctx.cond, &ctx.mtx);
        clock_gettime(CLOCK_MONOTONIC, &end);

        pthread_mutex_unlock(&ctx.mtx);

        timespec_diff(&diff, &start, &end);
        double ns = timespec_to_ns(diff);
        if (ns < min_ns) min_ns = ns;
        if (ns > max_ns) max_ns = ns;
        sum_ns += ns;
        success++;
    }

    pthread_join(tid, NULL);
    pthread_cond_destroy(&ctx.cond);
    pthread_mutex_destroy(&ctx.mtx);

    printf("=== IPC через общую память (потоки) ===\n");
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