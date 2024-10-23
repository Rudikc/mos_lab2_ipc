#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>

#define FILE_PATH "ipc_test_file.txt"
#define LARGE_DATA_SIZE 10485760  // 10MB

// Функція для зміни розміру файлу
void resize_file(int fd, size_t size) {
    if (ftruncate(fd, size) == -1) {
        perror("ftruncate");
        exit(1);
    }
}

// Функція для вимірювання часу
double get_time_diff(struct timespec start, struct timespec end) {
    return (end.tv_sec - start.tv_sec) * 1e9 + (end.tv_nsec - start.tv_nsec);
}

// Функція для mmap
void test_mmap(char *data, size_t data_size, int use_msync) {
    int fd = open(FILE_PATH, O_RDWR | O_CREAT, 0666);
    if (fd == -1) {
        perror("open");
        exit(1);
    }

    // Зміна розміру файлу до data_size
    resize_file(fd, data_size);

    // Мапінг файлу
    char *map =
        mmap(NULL, data_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (map == MAP_FAILED) {
        perror("mmap");
        close(fd);
        exit(1);
    }

    // Запис даних у маповану пам'ять
    memcpy(map, data, data_size);

    // Синхронізація з диском (опціонально)
    if (use_msync) {
        if (msync(map, data_size, MS_SYNC) == -1) {
            perror("msync");
        }
    }

    munmap(map, data_size);
    close(fd);
}

// Функція для бенчмаркінгу латентності
void benchmark_ipc_latency(const char *ipc_type,
                           void (*test_function)(char *, size_t, int)) {
    struct timespec start, end;
    char small_data = 'A';  // 1 байт даних

    int iterations = 100000;
    double total_latency = 0;

    for (int i = 0; i < iterations; ++i) {
        clock_gettime(CLOCK_MONOTONIC, &start);
        test_function(&small_data, 1, 0);  // Виконати тест без msync
        clock_gettime(CLOCK_MONOTONIC, &end);
        total_latency += get_time_diff(start, end);
    }

    double average_latency = total_latency / iterations;
    printf("%s: Середня латентність: %.2f наносекунд\n", ipc_type,
           average_latency);
}

// Функція для бенчмаркінгу пропускної здатності
void benchmark_ipc_throughput(const char *ipc_type,
                              void (*test_function)(char *, size_t, int),
                              char *data, size_t data_size) {
    struct timespec start, end;

    int iterations = 10;

    clock_gettime(CLOCK_MONOTONIC, &start);
    for (int i = 0; i < iterations; ++i) {
        test_function(data, data_size, 1);  // Виконати тест з msync
    }
    clock_gettime(CLOCK_MONOTONIC, &end);

    double time_spent = get_time_diff(start, end);
    double throughput =
        (data_size * iterations) / (time_spent / 1e9);  // байт в секунду
    printf("%s: Пропускна здатність: %.2f MB/s\n", ipc_type,
           throughput / (1024 * 1024));
}

int main() {
    // Виділення буфера даних для пропускної здатності
    size_t data_size = LARGE_DATA_SIZE;
    char *large_data = malloc(data_size);
    if (large_data == NULL) {
        perror("malloc");
        exit(1);
    }
    memset(large_data, 'A', data_size);  // Заповнення даними

    // Тестування mmap для латентності
    printf("Тестування mmap для латентності:\n");
    benchmark_ipc_latency("mmap", test_mmap);

    // Тестування mmap для пропускної здатності
    printf("\nТестування mmap для пропускної здатності:\n");
    benchmark_ipc_throughput("mmap", test_mmap, large_data, data_size);

    free(large_data);
    return 0;
}
