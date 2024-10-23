// shared_memory_test.c

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/shm.h>  // для shared memory
#include <sys/stat.h>  // для режимів доступу (S_IRWXU і т.д.)
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#define SHM_NAME "/ipc_test_shared_memory"
#define LARGE_DATA_SIZE 10485760  // 10MB

// Функція для вимірювання часу
double get_time_diff(struct timespec start, struct timespec end) {
    return (end.tv_sec - start.tv_sec) * 1e9 + (end.tv_nsec - start.tv_nsec);
}

// Функція для shared memory
void test_shared_memory(char *data, size_t data_size, int use_sync) {
    // Створення або відкриття сегмента shared memory
    int shm_fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
    if (shm_fd == -1) {
        perror("shm_open");
        exit(1);
    }

    // Зміна розміру shared memory до data_size
    if (ftruncate(shm_fd, data_size) == -1) {
        perror("ftruncate");
        close(shm_fd);
        exit(1);
    }

    // Мапування shared memory
    char *shm_ptr =
        mmap(0, data_size, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (shm_ptr == MAP_FAILED) {
        perror("mmap");
        close(shm_fd);
        exit(1);
    }

    // Запис даних у розділювану пам'ять
    memcpy(shm_ptr, data, data_size);

    // Синхронізація (опціонально)
    if (use_sync) {
        if (msync(shm_ptr, data_size, MS_SYNC) == -1) {
            perror("msync");
        }
    }

    // Відключення мапування
    if (munmap(shm_ptr, data_size) == -1) {
        perror("munmap");
    }

    // Закриття дескриптора розділюваної пам'яті
    close(shm_fd);

    // Видалення розділюваної пам'яті після тесту (якщо необхідно)
    if (shm_unlink(SHM_NAME) == -1) {
        perror("shm_unlink");
    }
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
        test_function(&small_data, 1,
                      0);  // Виконати тест без синхронізації
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
        test_function(data, data_size, 1);  // Виконати тест із синхронізацією
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

    // Тестування shared memory для латентності
    printf("Тестування shared memory для латентності:\n");
    benchmark_ipc_latency("shared memory", test_shared_memory);

    // Тестування shared memory для пропускної здатності
    printf("\nТестування shared memory для пропускної здатності:\n");
    benchmark_ipc_throughput("shared memory", test_shared_memory, large_data,
                             data_size);

    free(large_data);
    return 0;
}
