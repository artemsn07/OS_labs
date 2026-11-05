#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <limits.h>
#include <time.h>
#include "batcher.h"

static double now_ms(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec * 1000.0 + t.tv_nsec / 1e6;
}

static int *read_array(const char *name, size_t *n) {
    FILE *f = fopen(name, "r");
    if (!f) { perror(name); return NULL; }
    size_t cap = 1024;
    int *a = malloc(cap * sizeof(int)), value, status;
    if (!a) { fclose(f); return NULL; }

    while ((status = fscanf(f, "%d", &value)) == 1) {
        if (*n == cap) {
            if (cap > SIZE_MAX / (2 * sizeof(int))) goto error;
            cap *= 2;
            int *tmp = realloc(a, cap * sizeof(int));
            if (!tmp) goto error;
            a = tmp;
        }
        a[(*n)++] = value;
    }
    if (status != EOF || ferror(f) || !*n) goto error;
    fclose(f);
    return a;

error:
    fprintf(stderr, "Ошибка чтения массива\n");
    free(a);
    fclose(f);
    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc != 4 || strcmp(argv[2], "-t")) {
        fprintf(stderr, "Использование: %s input.txt -t ЧИСЛО\n", argv[0]);
        return 1;
    }
    char *end;
    unsigned long count = strtoul(argv[3], &end, 10);
    if (!argv[3][0] || argv[3][0] == '-' || *end || !count || count > UINT_MAX) {
        fprintf(stderr, "Неверное количество потоков\n");
        return 1;
    }

    size_t n = 0;
    int *a = read_array(argv[1], &n);
    if (!a) return 1;
    int *copy = malloc(n * sizeof(int));
    if (!copy) { free(a); return 1; }
    memcpy(copy, a, n * sizeof(int));

    double start = now_ms();
    batcher_sort(copy, n);
    double sequential = now_ms() - start;

    start = now_ms();
    parallel_batcher_sort(a, n, (unsigned)count);
    double parallel = now_ms() - start;

    if (memcmp(a, copy, n * sizeof(int)) != 0) {
        fprintf(stderr, "Результаты сортировок различаются\n");
        free(a); free(copy);
        return 1;
    }
    for (size_t i = 1; i < n; i++) {
        if (a[i - 1] > a[i]) {
            fprintf(stderr, "Ошибка сортировки\n");
            free(a); free(copy);
            return 1;
        }
    }

    printf("Элементов: %zu\n", n);
    if (n <= 100) {
        printf("Отсортированный массив:");
        for (size_t i = 0; i < n; i++) printf(" %d", a[i]);
        printf("\n");
    }
    double speedup = parallel > 0 ? sequential / parallel : 0;
    double efficiency = speedup / count * 100;
    printf("Последовательная сортировка: %.3f мс\n", sequential);
    printf("Потоки | Время (мс) | Ускорение | Эффективность\n");
    printf("%6lu | %10.3f | %9.3f | %11.2f%%\n",
           count, parallel, speedup, efficiency);

    free(a);
    free(copy);
    return 0;
}