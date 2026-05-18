#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <unistd.h>
#include <stdbool.h>

const double BASE_TOL = 1e-8;
const double MIN_H = 1e-10;
const int MAX_THREADS = 64;

typedef struct {
    double a, b, tol;
} Task;

typedef struct Node {
    Task data;
    struct Node* next;
} Node;

typedef struct {
    Node* head;
    Node* tail;
    int   count;
    pthread_mutex_t mtx;
    pthread_cond_t  cond;
} TaskQueue;

TaskQueue task_queue;
pthread_mutex_t acc_mtx = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t work_mtx = PTHREAD_MUTEX_INITIALIZER;
double total_integral = 0.0;
int active_tasks = 0;
bool finished = false;
int num_threads;

static void queue_push(Task t) {
    Node* node = (Node*)malloc(sizeof(Node));
    if (!node) {
        perror("malloc failed");
        exit(EXIT_FAILURE);
    }
    node->data = t;
    node->next = NULL;

    pthread_mutex_lock(&task_queue.mtx);
    if (task_queue.tail)
        task_queue.tail->next = node;
    else
        task_queue.head = node;
    task_queue.tail = node;
    task_queue.count++;
    pthread_cond_signal(&task_queue.cond);
    pthread_mutex_unlock(&task_queue.mtx);
}

static void* worker(void* arg) {
    (void)arg;
    while (1) {
        Task t;

        pthread_mutex_lock(&task_queue.mtx);
        while (task_queue.count == 0 && !finished) {
            pthread_cond_wait(&task_queue.cond, &task_queue.mtx);
        }
        if (task_queue.count == 0) {
            pthread_mutex_unlock(&task_queue.mtx);
            break;
        }

        t = task_queue.head->data;
        Node* tmp = task_queue.head;
        task_queue.head = tmp->next;
        if (!task_queue.head) task_queue.tail = NULL;
        task_queue.count--;
        free(tmp);
        pthread_mutex_unlock(&task_queue.mtx);

        double a = t.a, b = t.b, tol = t.tol;
        double h = b - a;
        double m = (a + b) * 0.5;

        double fa = sin(1.0 / a);
        double fb = sin(1.0 / b);
        double fm = sin(1.0 / m);

        double I1 = h * (fa + fb) * 0.5;
        double I2 = (h * 0.5) * (fa + 2.0 * fm + fb) * 0.5;
        double err = fabs(I2 - I1) / 15.0;

        bool converged = (err < tol) || (h < MIN_H);

        if (converged) {
            double res = I2 + (I2 - I1) / 15.0;
            pthread_mutex_lock(&acc_mtx);
            total_integral += res;
            pthread_mutex_unlock(&acc_mtx);

            pthread_mutex_lock(&work_mtx);
            active_tasks--;
            if (active_tasks == 0) {
                finished = true;
                pthread_cond_broadcast(&task_queue.cond);
            }
            pthread_mutex_unlock(&work_mtx);
        } else {
            Task t1 = {a, m, tol * 0.5};
            Task t2 = {m, b, tol * 0.5};
            queue_push(t1);
            queue_push(t2);

            pthread_mutex_lock(&work_mtx);
            active_tasks++;
            pthread_mutex_unlock(&work_mtx);
        }
    }
    return NULL;
}

static void queue_init(void) {
    task_queue.head = task_queue.tail = NULL;
    task_queue.count = 0;
    pthread_mutex_init(&task_queue.mtx, NULL);
    pthread_cond_init(&task_queue.cond, NULL);
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <a> <b> [threads]\n", argv[0]);
        return EXIT_FAILURE;
    }

    double a = atof(argv[1]);
    double b = atof(argv[2]);
    if (a <= 0.0 || b <= 0.0 || a >= b) {
        fprintf(stderr, "Error: 0 < a < b required\n");
        return EXIT_FAILURE;
    }

    num_threads = (argc > 3) ? atoi(argv[3]) : (int)sysconf(_SC_NPROCESSORS_ONLN);
    if (num_threads < 1) num_threads = 1;
    if (num_threads > MAX_THREADS) num_threads = MAX_THREADS;

    queue_init();
    pthread_t* threads = (pthread_t*)malloc(num_threads * sizeof(pthread_t));
    if (!threads) { perror("malloc"); return EXIT_FAILURE; }

    pthread_mutex_lock(&work_mtx);
    active_tasks = 1;
    pthread_mutex_unlock(&work_mtx);

    queue_push((Task){a, b, BASE_TOL * (b - a)});

    for (int i = 0; i < num_threads; i++) {
        if (pthread_create(&threads[i], NULL, worker, NULL) != 0) {
            perror("pthread_create");
            return EXIT_FAILURE;
        }
    }

    struct timespec ts_start, ts_end;
    clock_gettime(CLOCK_MONOTONIC, &ts_start);

    for (int i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL);
    }

    clock_gettime(CLOCK_MONOTONIC, &ts_end);
    double elapsed = (ts_end.tv_sec - ts_start.tv_sec) + (ts_end.tv_nsec - ts_start.tv_nsec) / 1e9;

    printf("Result: %.12f\n", total_integral);
    printf("Time: %.6f sec\n", elapsed);
    printf("Threads: %d\n", num_threads);

    free(threads);
    pthread_mutex_destroy(&task_queue.mtx);
    pthread_cond_destroy(&task_queue.cond);
    pthread_mutex_destroy(&acc_mtx);
    pthread_mutex_destroy(&work_mtx);

    return EXIT_SUCCESS;
}