#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

static void *checked_malloc(size_t bytes) {
    void *memory = malloc(bytes);
    if (memory == NULL) {
        fprintf(stderr, "Error: Memory allocation failed.\n");
        exit(EXIT_FAILURE);
    }
    return memory;
}

void print_usage() {
    printf("Usage: RISC-Vsim <inputfilename> <outputfilename> dis\n");
}

void to_binary_string(uint32_t value, char *buffer, int bits) {
    buffer[bits] = '\0'; // Null-terminate the string
    for (int i = bits - 1; i >= 0; --i) {
        buffer[i] = (value & 1) ? '1' : '0'; // Set '1' or '0' based on the least significant bit
        value >>= 1; // Shift right
    }
}

char *processSlice(const char *array, int start, int end, int *sliceLength) {
    *sliceLength = end - start;
    
    // Allocate memory for the new slice (+1 for the null terminator)
    char *newSlice = checked_malloc((*sliceLength + 1) * sizeof(char));

    // Copy the selected slice into the new array
    for (int i = 0; i < *sliceLength; i++) {
        newSlice[i] = array[start + i];
    }
    
    // Null-terminate the new slice
    newSlice[*sliceLength] = '\0';
    
    return newSlice;
}

char *combineSlices(const char *slices[], int numSlices) {
    // Calculate total length of the combined string
    int totalLength = 0;
    for (int i = 0; i < numSlices; i++) {
        totalLength += strlen(slices[i]);
    }

    // Allocate memory for the combined string (+1 for null terminator)
    char *combined = checked_malloc((totalLength + 1) * sizeof(char));

    // Initialize the combined string
    combined[0] = '\0';

    // Append each slice to the combined string
    for (int i = 0; i < numSlices; i++) {
        strcat(combined, slices[i]);
    }

    return combined;
}

char *shiftLeft(const char *binary) {
    int length = strlen(binary);

    // Allocate memory for the shifted result (+1 for null terminator and 1 extra bit)
    char *shiftedStr = checked_malloc((length + 2) * sizeof(char));

    // Keep the highest bit (it stays in position)
    shiftedStr[0] = binary[0];

    // Shift all remaining bits left by 1 and insert '0' at the end
    for (int i = 1; i < length; i++) {
        shiftedStr[i] = binary[i];  // Shift left by one position
    }

    // Append '0' at the rightmost bit
    shiftedStr[length] = '0';

    // Null-terminate the string
    shiftedStr[length + 1] = '\0';

    return shiftedStr;
}

uint32_t readSpecificLine(FILE* input_file, int lineNumber) {
    if (lineNumber < 496 || (lineNumber - 496) % 4 != 0) {
        fprintf(stderr, "Error: Invalid input address %d.\n", lineNumber);
        exit(EXIT_FAILURE);
    }

    rewind(input_file);
    char buffer[1024];
    uint64_t address = 496;

    while (fgets(buffer, sizeof(buffer), input_file)) {
        if (address == (uint32_t)lineNumber) {
            return (uint32_t)strtoul(buffer, NULL, 2);
        }
        address += 4;
    }
    if (ferror(input_file)) {
        fprintf(stderr, "Error: Could not read input.\n");
    } else {
        fprintf(stderr, "Error: Input address %d is absent.\n", lineNumber);
    }
    exit(EXIT_FAILURE);
}