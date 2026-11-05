#ifndef BATCHER_H
#define BATCHER_H

#include <stddef.h>

void batcher_sort(int a[], size_t n);
void parallel_batcher_sort(int a[], size_t n, unsigned threads);

#endif