#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>
#include <string.h>

const int MIN_CHUNK_SIZE = 50000;
const int MAX_DEPTH = 4;
const int NUM_RUNS = 5;

static int compare_int(const void *a, const void *b) {
    int va = *(const int *)a;
    int vb = *(const int *)b;
    return (va > vb) - (va < vb);
}

static void merge(int *arr, int *temp, int left, int mid, int right) {
    int i = left, j = mid + 1, k = left;
    while (i <= mid && j <= right) {
        if (arr[i] <= arr[j]) temp[k++] = arr[i++];
        else                temp[k++] = arr[j++];
    }
    while (i <= mid) temp[k++] = arr[i++];
    while (j <= right) temp[k++] = arr[j++];
    memcpy(&arr[left], &temp[left], (right - left + 1) * sizeof(int));
}

typedef struct {
    int *arr;
    int *temp;
    int left;
    int right;
    int depth;
} SortArgs;

static void* parallel_merge_sort(void *arg) {
    SortArgs *args = (SortArgs *)arg;
    int n = args->right - args->left + 1;

    if (n <= MIN_CHUNK_SIZE || args->depth <= 0) {
        qsort(&args->arr[args->left], n, sizeof(int), compare_int);
        return NULL;
    }

    int mid = (args->left + args->right) / 2;
    pthread_t t1, t2;
    
    SortArgs a1 = {args->arr, args->temp, args->left, mid, args->depth - 1};
    SortArgs a2 = {args->arr, args->temp, mid + 1, args->right, args->depth - 1};

    pthread_create(&t1, NULL, parallel_merge_sort, &a1);
    pthread_create(&t2, NULL, parallel_merge_sort, &a2);

    pthread_join(t1, NULL);
    pthread_join(t2, NULL);

    merge(args->arr, args->temp, args->left, mid, args->right);
    return NULL;
}

static double get_time_monotonic(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec * 1e-9;
}

int main(void) {
    srand(42);

    int sizes[] = {5000, 10000, 50000, 100000, 500000, 1000000, 2000000, 5000000, 10000000, 20000000};
    int n_sizes = sizeof(sizes) / sizeof(sizes[0]);

    printf("Size,qsort_time_sec,parallel_time_sec\n");

    for (int s = 0; s < n_sizes; s++) {
        int N = sizes[s];
        int *arr_q = (int *)malloc(N * sizeof(int));
        int *arr_p = (int *)malloc(N * sizeof(int));
        int *temp  = (int *)malloc(N * sizeof(int));

        if (!arr_q || !arr_p || !temp) {
            fprintf(stderr, "Ошибка выделения памяти для N=%d\n", N);
            return EXIT_FAILURE;
        }

        double total_q = 0.0, total_p = 0.0;
        fprintf(stderr, "Бенчмарк: N = %d ... ", N);

        for (int run = 0; run < NUM_RUNS; run++) {
            for (int i = 0; i < N; i++) {
                arr_q[i] = arr_p[i] = rand();
            }

            double t0 = get_time_monotonic();
            qsort(arr_q, N, sizeof(int), compare_int);
            total_q += get_time_monotonic() - t0;

            t0 = get_time_monotonic();
            SortArgs pargs = {arr_p, temp, 0, N - 1, MAX_DEPTH};
            pthread_t tid;
            pthread_create(&tid, NULL, parallel_merge_sort, &pargs);
            pthread_join(tid, NULL);
            total_p += get_time_monotonic() - t0;
        }

        printf("%d,%.6f,%.6f\n", N, total_q / NUM_RUNS, total_p / NUM_RUNS);
        fprintf(stderr, "Готово\n");

        free(arr_q);
        free(arr_p);
        free(temp);
    }

    return EXIT_SUCCESS;
}