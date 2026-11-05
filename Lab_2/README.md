# Operating Systems - Lab 2

### Variant 5 - Batcher's Odd-Even Merge Sort

## Compiling

```bash
gcc -std=c11 -Wall -Wextra -Werror -pedantic -O2 -pthread lab2.c batcher.c -o lab2
```

## Running

```bash
./lab2 test1.txt -t 4
```

The `-t` flag specifies the maximum number of threads, including the main thread.

## Example

Create `test.txt` with the following integers:

```text
9 4 1 -2 8 0 5 3 7 6
```

Run:

```bash
./lab2 test.txt -t 4
```

Expected sorted array:

```text
-2 0 1 3 4 5 6 7 8 9
```

The program also displays sequential and parallel execution times, speedup, and efficiency.

Execution times may vary between runs.

## Performance Testing

Three input arrays are used:

- `test1.txt` - 1,048,577 random integers.
- `test_10000_reverse.txt` - 10,000 integers in reverse order.
- `test_100000_nearly_sorted.txt` - 100,000 nearly sorted integers.

Example:

```bash
./lab2 test1.txt -t 1
./lab2 test1.txt -t 2
./lab2 test1.txt -t 4
./lab2 test1.txt -t 8
./lab2 test1.txt -t 16
```

## Invalid Thread Count

Example:

```bash
./lab2 test.txt -t 0
```

Output:

```text
Неверное количество потоков
```

The program terminates with an error.

## Important

- `lab2.c`, `batcher.c`, and `batcher.h` must be in the same directory for the compilation command above.
- Input files must contain integers separated by spaces or newlines, without an array size at the beginning.
- The `-t` flag sets the maximum number of simultaneous threads, not an exact thread count.
- Both sequential and parallel sorting are implemented using Batcher's odd-even merge sort.
- The program verifies that both sorting results match and that the array is sorted.
- The sorted array is printed only if it contains 100 elements or fewer.
- Only Linux/POSIX interfaces are used for threading and time measurements.
