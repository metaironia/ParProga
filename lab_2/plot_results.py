import sys
import matplotlib.pyplot as plt

def main():
    lines = [l.strip() for l in sys.stdin if l.strip()]
    if not lines:
        print("Нет данных. Запустите: ./benchmark > results.csv && python3 plot_results.py < results.csv")
        return

    sizes, q_times, p_times = [], [], []
    for line in lines[1:]:
        parts = list(map(float, line.split(',')))
        sizes.append(parts[0])
        q_times.append(parts[1])
        p_times.append(parts[2])

    plt.figure(figsize=(10, 6))
    plt.plot(sizes, q_times, 'o-', label='qsort (1 поток)', linewidth=2, markersize=6)
    plt.plot(sizes, p_times, 's--', label='Parallel Merge Sort (POSIX)', linewidth=2, markersize=6)

    plt.xscale('log')
    plt.yscale('log')
    plt.xlabel('Количество элементов (логарифмическая шкала)')
    plt.ylabel('Время выполнения, сек (логарифмическая шкала)')
    plt.title('Сравнение производительности: qsort vs Параллельная сортировка')
    plt.legend()
    plt.grid(True, which="both", ls="--", alpha=0.7)
    plt.tight_layout()
    
    plt.savefig('sort_benchmark.png', dpi=150)
    print("График сохранён в sort_benchmark.png")
    plt.show()

if __name__ == "__main__":
    main()