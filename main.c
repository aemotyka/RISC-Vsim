#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "disassembler.h"

void print_usage() {
    printf("Usage: RISC-Vsim <inputfilename> <outputfilename> dis\n");
}

int main(int argc, char *argv[]) {
    // Check that the correct number of arguments is provided
    if (argc != 4) {
        fprintf(stderr, "Error: Invalid number of arguments.\n");
        print_usage();
        return 1;
    }

    // Extract the arguments
    const char* input_filename = argv[1];
    const char* output_filename = argv[2];
    const char* operation = argv[3];

    // Try opening the input file for reading
    FILE *input_file = fopen(input_filename, "rb");
    if (input_file == NULL) {
        fprintf(stderr, "Error: Could not open input file '%s'.\n", input_filename);
        return 1;
    }

    // Try opening the output file for writing
    FILE *output_file = fopen(output_filename, "w");
    if (output_file == NULL) {
        fprintf(stderr, "Error: Could not open output file '%s'.\n", output_filename);
        return 1;
    }

    // Check if the operation is "dis"
    if (strcmp(operation, "dis") != 0) {
        fprintf(stderr, "Error: Invalid operation. Only 'dis' is supported for now.\n");
        print_usage();
        return 1;
    }

    // Call the disassemble function with the open input and output files
    disassemble(input_file, output_file);

    // Close the files when done
    fclose(input_file);
    fclose(output_file);

    return 0;
}