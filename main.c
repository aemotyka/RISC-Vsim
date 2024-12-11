#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "disassembler.h"
#include "utilities.h"
#include "pipeline.h"

int main(int argc, char *argv[]) {
    // Check that the correct number of arguments is provided
    if (argc < 4 || argc > 5) {
        fprintf(stderr, "Error: Invalid number of arguments.\n");
        print_usage();
        return 1;
    }

    // Extract the arguments
    const char* input_filename = argv[1];
    const char* output_filename = argv[2];
    const char* operation = argv[3];
    const char* trace = argv[4];

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

    // Variables to hold trace values
    int trace_start;
    int trace_end;

    if (trace != NULL) {
        // Parse the Tn:m format
        if (sscanf(trace, "T%d:%d", &trace_start, &trace_end) != 2) {
            fprintf(stderr, "Error: Invalid trace format. Expected Tn:m.\n");
            fclose(input_file);
            fclose(output_file);
            return 1;
        }
    } else {
        // No trace provided; calculate m as the total number of lines in the input file
        trace_start = 0;
        trace_end = 250;
        rewind(input_file); // Reset file pointer after counting lines
    }

    // printf("Trace values: n = %d, m = %d\n", trace_start, trace_end);

    // Check operation
    if (strcmp(operation, "dis") == 0) {
        disassemble(input_file, output_file);
    } else if (strcmp(operation, "sim") == 0) {
        simulate(input_file, output_file, trace_start, trace_end);
    } else {
        fprintf(stderr, "Error: Unsuported operation.\n");
        print_usage();
        return 1;
    }

    // Close the files when done
    fclose(input_file);
    fclose(output_file);

    return 0;
}