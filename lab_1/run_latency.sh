mpic++ -O3 -o mpi_latency mpi_latency.c
mpirun -np 2 ./mpi_latency
