mpic++ -O3 -o pi_mpi pi_mpi.c
mpirun -np 2 ./pi_mpi
