// file_test.c

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define FILE_PATH "ipc_test_file.txt"
#define LARGE_DATA_SIZE 10485760  // 10MB

// Функція для вимірювання часу
double get_time_diff(struct timespec start, struct timespec end) {
    return (end.tv_sec - start.tv_sec) * 1e9 + (end.tv_nsec - start.tv_nsec);
}

// Функція для роботи з файлами: відкриття, запис і читання
void test_file(char *data, size_t data_size, int use_sync) {
    // Відкриття файлу для запису
    int fd = open(FILE_PATH, O_CREAT | O_RDWR, 0666);
    if (fd == -1) {
        perror("open");
        exit(1);
    }

    // Запис даних у файл
    if (write(fd, data, data_size) == -1) {
        perror("write");
        close(fd);
        exit(1);
    }

    // Синхронізація з диском (опціонально)
    if (use_sync) {
        if (fsync(fd) == -1) {
            perror("fsync");
        }
    }

    // Переміщення в початок файлу
    if (lseek(fd, 0, SEEK_SET) == -1) {
        perror("lseek");
        close(fd);
        exit(1);
    }

    // Виділення пам'яті для читання
    char *buffer = malloc(data_size);
    if (!buffer) {
        perror("malloc");
        close(fd);
        exit(1);
    }

    // Читання даних із файлу
    if (read(fd, buffer, data_size) == -1) {
        perror("read");
        free(buffer);
        close(fd);
        exit(1);
    }

    // Звільнення буфера та закриття файлу
    free(buffer);
    close(fd);

    // Видалення файлу після тесту
    if (unlink(FILE_PATH) == -1) {
        perror("unlink");
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
        test_function(&small_data, 1, 0);  // Виконати тест без синхронізації
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

    // Тестування file open/read/write для латентності
    printf("Тестування file open/read/write для латентності:\n");
    benchmark_ipc_latency("file open/read/write", test_file);

    // Тестування file open/read/write для пропускної здатності
    printf("\nТестування file open/read/write для пропускної здатності:\n");
    benchmark_ipc_throughput("file open/read/write", test_file, large_data,
                             data_size);

    free(large_data);
    return 0;
}
