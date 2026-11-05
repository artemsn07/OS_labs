#include <pthread.h>
#include "batcher.h"

static void compare(int a[], size_t i, size_t j, size_t n) {
    if (j < n && a[i] > a[j]) {
        int t = a[i]; a[i] = a[j]; a[j] = t;
    }
}

static void merge(int a[], size_t start, size_t len, size_t step, size_t n) {
    size_t k = step * 2;
    if (k < len) {
        merge(a, start, len, k, n);
        merge(a, start + step, len, k, n);
        for (size_t i = start + step; i + step < start + len; i += k)
            compare(a, i, i + step, n);
    } else {
        compare(a, start, start + step, n);
    }
}

static void sort(int a[], size_t start, size_t len, size_t n) {
    if (len <= 1 || start >= n) return;
    size_t half = len / 2;
    sort(a, start, half, n);
    sort(a, start + half, half, n);
    merge(a, start, len, 1, n);
}

static size_t power2(size_t n) {
    size_t p = 1;
    while (p < n) {
        if (p > (size_t)-1 / 2) return 0;
        p *= 2;
    }
    return p;
}

void batcher_sort(int a[], size_t n) {
    size_t p = power2(n);
    if (p) sort(a, 0, p, n);
}

typedef struct {
    int *a;
    size_t start, len, n;
    unsigned threads;
} Task;

static void parallel(int a[], size_t start, size_t len, size_t n, unsigned threads);

static void *worker(void *arg) {
    Task *t = arg;
    parallel(t->a, t->start, t->len, t->n, t->threads);
    return NULL;
}

static void parallel(int a[], size_t start, size_t len, size_t n, unsigned threads) {
    if (len <= 1 || start >= n) return;
    if (threads <= 1) { sort(a, start, len, n); return; }

    size_t half = len / 2;
    Task left = {a, start, half, n, threads / 2};
    pthread_t tid;
    if (pthread_create(&tid, NULL, worker, &left) == 0) {
        parallel(a, start + half, half, n, threads - left.threads);
        pthread_join(tid, NULL);
    } else {
        sort(a, start, half, n);
        parallel(a, start + half, half, n, threads - left.threads);
    }
    merge(a, start, len, 1, n);
}

void parallel_batcher_sort(int a[], size_t n, unsigned threads) {
    size_t p = power2(n);
    if (p) parallel(a, 0, p, n, threads);
}