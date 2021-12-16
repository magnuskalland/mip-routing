#ifndef UTILS_H
#define UTILS_H

#include <stddef.h>
#include <time.h>

/**
 * Wrapper function for calloc().
 * @param size  Number of bytes to allocate.
 * @return      A pointer to the allocated memory. NULL if error.
 * */
void *allocate_memory(size_t size);

/**
 * Checks if a is in range from x up to y (excluding y).
 * @param a     The number to check.
 * @param x     The lower number.
 * @param y     The upper number.
 * */
int in_range(int a, int x, int y);

/**
 * Function to print a string as binary numbers.
 * @param string    The string to print.
 * */
void print_string_as_binary(char* string);

unsigned int get_8bit_number();

/**
 * Reference:
    https://github.com/kr1stj0n/plenaries-in3230-h21/blob/main/p6_04-10-2021/fsm/common.c 
 * */
double diff_time_ms(struct timespec start, struct timespec end);

#endif

