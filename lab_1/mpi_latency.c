#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mpi.h>

const int NUM_ITERATIONS = 10000;
const int MAX_MSG_SIZE = (1 << 20);

int main(int argc, char *argv[])
{
    int rank, size;
    MPI_Status status;

    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    if (size != 2) {
        if (rank == 0) {
            fprintf(stderr, "Ошибка: требуется ровно 2 процесса.\n");
        }
        MPI_Finalize();
        return 1;
    }

    char *buffer = (char *)malloc(MAX_MSG_SIZE);
    memset(buffer, 'A', MAX_MSG_SIZE);

    if (rank == 0) {
        printf("%-20s %-20s %-20s\n", "Размер (байт)", "Задержка (мкс)", "Пропускная способность (МБ/с)");
        printf("%-20s %-20s %-20s\n", "==============", "==============", "============================");
    }

    int msg_size;
    for (msg_size = 0; msg_size <= MAX_MSG_SIZE; msg_size = (msg_size == 0) ? 1 : msg_size * 2) {

        MPI_Barrier(MPI_COMM_WORLD);

        double start_time, end_time, total_time;
        int iter;

        if (rank == 0) {
            start_time = MPI_Wtime();

            for (iter = 0; iter < NUM_ITERATIONS; iter++) {
                MPI_Send(buffer, msg_size, MPI_CHAR, 1, 0, MPI_COMM_WORLD);
                MPI_Recv(buffer, msg_size, MPI_CHAR, 1, 0, MPI_COMM_WORLD, &status);
            }

            end_time = MPI_Wtime();
            total_time = end_time - start_time;

            double latency_us = (total_time / NUM_ITERATIONS / 2.0) * 1e6;
            double bandwidth_mbps = (msg_size > 0) ?
                (2.0 * msg_size * NUM_ITERATIONS) / total_time / 1e6 : 0.0;

            printf("%-20d %-20.3f %-20.3f\n", msg_size, latency_us, bandwidth_mbps);

        } else {
            for (iter = 0; iter < NUM_ITERATIONS; iter++) {
                MPI_Recv(buffer, msg_size, MPI_CHAR, 0, 0, MPI_COMM_WORLD, &status);
                MPI_Send(buffer, msg_size, MPI_CHAR, 0, 0, MPI_COMM_WORLD);
            }
        }
    }

    free(buffer);
    MPI_Finalize();
    return 0;
}