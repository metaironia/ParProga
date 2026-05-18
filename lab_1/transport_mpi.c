#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <mpi.h>

#ifndef M_PI
const double M_PI = 3.14159265358979323846;
#endif

const double A_COEFF = 1.0;
const double X_MAX = 1.0;
const double T_MAX = 1.0;

double f_rhs(double t, double x) { 
    return x + A_COEFF * t;
}

double exact_solution(double t, double x) { 
    return x * t;
}

double phi(double x) { return exact_solution(0.0, x); }
double psi(double t) { return exact_solution(t, 0.0); }

int main(int argc, char *argv[])
{
    int rank, size;
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    if (argc < 3) {
        if (rank == 0) {
            fprintf(stderr, "Использование: mpirun -n <np> ./transport_mpi <M> <K>\n");
        }
        MPI_Finalize();
        return 1;
    }

    int M = atoi(argv[1]);
    int K = atoi(argv[2]);

    double h = X_MAX / M;
    double tau = T_MAX / K;
    double sigma = A_COEFF * tau / h;

    if (rank == 0) {
        printf("=== Параллельное решение уравнения переноса (MPI) ===\n");
        printf("Схема: Явный левый уголок\n");
        printf("M = %d, K = %d, h = %e, tau = %e, sigma = %f\n", M, K, h, tau, sigma);
        printf("Число процессов: %d\n\n", size);
        if (sigma > 1.0) {
            printf("ПРЕДУПРЕЖДЕНИЕ: CFL нарушено!\n");
        }
    }

    int total_points = M + 1;
    int base_count = total_points / size;
    int remainder = total_points % size;

    int local_count, local_start;
    if (rank < remainder) {
        local_count = base_count + 1;
        local_start = rank * (base_count + 1);
    } else {
        local_count = base_count;
        local_start = remainder * (base_count + 1) + (rank - remainder) * base_count;
    }
    int local_end = local_start + local_count - 1;

    int has_left_ghost = (rank > 0) ? 1 : 0;
    int local_size = local_count + has_left_ghost;

    double *u_curr = (double *)calloc(local_size, sizeof(double));
    double *u_next = (double *)calloc(local_size, sizeof(double));

    int i;
    for (i = 0; i < local_count; i++) {
        int global_m = local_start + i;
        u_curr[has_left_ghost + i] = phi(global_m * h);
    }

    int left_neighbor = (rank > 0) ? rank - 1 : MPI_PROC_NULL;
    int right_neighbor = (rank < size - 1) ? rank + 1 : MPI_PROC_NULL;

    double start_time = MPI_Wtime();

    int k;
    MPI_Status status_mpi;
    for (k = 0; k < K; k++) {
        double t_k = k * tau;

        double send_val = u_curr[has_left_ghost];
        double recv_val = 0.0;


        double send_to_right = u_curr[has_left_ghost + local_count - 1];

        MPI_Sendrecv(
            &send_to_right, 1, MPI_DOUBLE, right_neighbor, 0,
            &recv_val, 1, MPI_DOUBLE, left_neighbor, 0,
            MPI_COMM_WORLD, &status_mpi
        );

        if (has_left_ghost) {
            u_curr[0] = recv_val;
        }

        for (i = 0; i < local_count; i++) {
            int global_m = local_start + i;
            int local_idx = has_left_ghost + i;

            if (global_m == 0) {
                u_next[local_idx] = psi((k + 1) * tau);
            } else {
                u_next[local_idx] = u_curr[local_idx]
                    - sigma * (u_curr[local_idx] - u_curr[local_idx - 1])
                    + tau * f_rhs(t_k, global_m * h);
            }
        }

        double *tmp = u_curr;
        u_curr = u_next;
        u_next = tmp;
    }

    double end_time = MPI_Wtime();
    double local_time = end_time - start_time;

    double local_max_err = 0.0;
    double local_l2_err = 0.0;
    for (i = 0; i < local_count; i++) {
        int global_m = local_start + i;
        double exact = exact_solution(T_MAX, global_m * h);
        double err = fabs(u_curr[has_left_ghost + i] - exact);
        if (err > local_max_err) local_max_err = err;
        local_l2_err += err * err;
    }

    double global_max_err, global_l2_err, max_time;
    MPI_Reduce(&local_max_err, &global_max_err, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
    MPI_Reduce(&local_l2_err, &global_l2_err, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
    MPI_Reduce(&local_time, &max_time, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);

    if (rank == 0) {
        global_l2_err = sqrt(global_l2_err * h);
        printf("Результаты:\n");
        printf("  Ошибка (max-норма): %e\n", global_max_err);
        printf("  Ошибка (L2-норма):  %e\n", global_l2_err);
        printf("  Время вычисления:   %f сек\n", max_time);
        printf("  Процессов:          %d\n", size);
    }

    free(u_curr);
    free(u_next);
    MPI_Finalize();
    return 0;
}