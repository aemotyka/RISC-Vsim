#ifndef UTILITIES_H
#define UTILITIES_H

#include <stdio.h>

void print_usage();
void to_binary_string(uint32_t value, char *buffer, int bits);
char *processSlice(const char *array, int start, int end, int *sliceLength);
char *combineSlices(const char *slices[], int numSlices);
char *shiftLeft(const char *binary);
uint32_t readSpecificLine(FILE* input_file, int lineNumber);

#endif