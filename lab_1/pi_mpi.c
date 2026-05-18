#include <stdio.h>
#include <mpi.h>

int main(int argc, char** argv) {
    const long N = 10000000000L;

    MPI_Init(&argc, &argv);

    int rank = 0;
    int size = 0;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    double h = 1.0 / (double)N;

    double start_time, end_time;

    if (rank == 0) {
        start_time = MPI_Wtime();
    }

    double local_sum = 0.0;

    for (long long i = rank + 1; i <= N; i += size) {
        double x = h * ((double)i - 0.5);
        local_sum += 4.0 / (1.0 + x * x);
    }

    double local_pi = h * local_sum;

    double global_sum = 0.0;
    MPI_Reduce(&local_pi, &global_sum, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);

    if (rank == 0) {
        end_time = MPI_Wtime();

        printf("pi = %.16g\n", global_sum);
        printf("time = %g s\n", end_time - start_time);
    }

    MPI_Finalize();
    return 0;
}