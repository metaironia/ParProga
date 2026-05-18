#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <time.h>

#ifndef M_PI
const double M_PI = 3.14159265358979323846;
#endif

const double A_COEFF = 1.0;
const double X_MAX = 1.0;
const double T_MAX = 1.0;

double get_time_sec(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}

double phi(double x) { return sin(2.0 * M_PI * x); }
double psi(double t) { return -sin(2.0 * M_PI * A_COEFF * t); }
double f_rhs(double t, double x) { return 0.0; }
double exact_solution(double t, double x) { return sin(2.0 * M_PI * (x - A_COEFF * t)); }

void scheme_left_corner(int M, int K)
{
    double h = X_MAX / M;
    double tau = T_MAX / K;
    double sigma = A_COEFF * tau / h;

    printf("Схема 1: Явный левый уголок\n");
 
    if (sigma > 1.0 || sigma < 0.0) {
        printf("ПРЕДУПРЕЖДЕНИЕ: условие устойчивости CFL нарушено (sigma = %f)\n", sigma);
    }

    double *u_curr = (double *)malloc((M + 1) * sizeof(double));
    double *u_next = (double *)malloc((M + 1) * sizeof(double));

    for (int m = 0; m <= M; m++) u_curr[m] = phi(m * h);

    double t_start = get_time_sec();

    for (int k = 0; k < K; k++) {
        double t_k = k * tau;
        u_next[0] = psi((k + 1) * tau);

        for (int m = 1; m <= M; m++) {
            u_next[m] = u_curr[m] - sigma * (u_curr[m] - u_curr[m - 1]) + tau * f_rhs(t_k, m * h);
        }

        double *tmp = u_curr; u_curr = u_next; u_next = tmp;
    }

    double t_end = get_time_sec();
    double elapsed = t_end - t_start;

    double max_err = 0.0, l2_err = 0.0;
    for (int m = 0; m <= M; m++) {
        double exact = exact_solution(T_MAX, m * h);
        double err = fabs(u_curr[m] - exact);
        if (err > max_err) max_err = err;
        l2_err += err * err;
    }
    l2_err = sqrt(l2_err * h);

    printf("Ошибка (max-норма): %e\n", max_err);
    printf("Ошибка (L2-норма):  %e\n", l2_err);
    printf("Время вычислений:   %.6f сек\n", elapsed);

    free(u_curr);
    free(u_next);
}

int main(int argc, char *argv[])
{
    if (argc < 3) {
        fprintf(stderr, "Использование: ./transport_seq <M> <K>\n");
        fprintf(stderr, "  M: число узлов по x\n");
        fprintf(stderr, "  K: число узлов по t\n");
        return 1;
    }

    int M = atoi(argv[1]);
    int K = atoi(argv[2]);

    scheme_left_corner(M, K);

    return 0;
}