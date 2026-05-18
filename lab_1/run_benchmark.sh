mpic++ -O3 -o transport_mpi transport_mpi.c

M=10000
K=10000

echo "=== Бенчмарк ускорения ==="
echo "M=$M, K=$K"
echo ""

# Запускаем с разным числом процессов
for np in 1 2 4 8 16; do
    echo "--- np = $np ---"
    mpirun -np $np ./transport_mpi $M $K
    echo ""
done