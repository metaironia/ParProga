mpic++ -O3 -o transport_mpi transport_mpi.c
mpirun -np 2 ./transport_mpi 10000 10000
