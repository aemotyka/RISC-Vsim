#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "disassembler.h"
#include "utilities.h"
#include "pipeline.h"

static int validate_input(FILE *input) {
    size_t line = 1;
    unsigned bits = 0;
    bool has_word = false;
    int c;

    while ((c = fgetc(input)) != EOF) {
        if (c == '\r') {
            if (fgetc(input) != '\n') goto invalid;
            c = '\n';
        }
        if (c == '\n') {
            if (bits != 32) goto invalid;
            has_word = true;
            bits = 0;
            line++;
        } else if ((c == '0' || c == '1') && bits < 32) {
            bits++;
        } else {
            goto invalid;
        }
    }
    if (ferror(input)) {
        fprintf(stderr, "Error: Could not read input.\n");
        return 0;
    }
    if ((bits != 0 && bits != 32) || (!has_word && bits == 0)) goto invalid;
    rewind(input);
    return 1;

invalid:
    fprintf(stderr, "Error: Input line %zu must contain 32 binary digits.\n", line);
    return 0;
}

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

    if (!validate_input(input_file)) {
        fclose(input_file);
        return 1;
    }

    // Try opening the output file for writing
    FILE *output_file = fopen(output_filename, "w");
    if (output_file == NULL) {
        fprintf(stderr, "Error: Could not open output file '%s'.\n", output_filename);
        fclose(input_file);
        return 1;
    }

    // Variables to hold trace values
    int trace_start;
    int trace_end;

    if (trace != NULL) {
        // Parse the Tn:m format
        char trailing;
        if (sscanf(trace, "T%d:%d%c", &trace_start, &trace_end, &trailing) != 2
            || trace_start < 0 || trace_end < trace_start) {
            fprintf(stderr, "Error: Invalid trace format. Expected Tn:m with 0 <= n <= m.\n");
            fclose(input_file);
            fclose(output_file);
            return 1;
        }
    } else {
        trace_start = 0;
        trace_end = 250;
    }

    // Check operation
    if (strcmp(operation, "dis") == 0) {
        disassemble(input_file, output_file);
    } else if (strcmp(operation, "sim") == 0) {
        load_input(input_file);
        simulate(input_file, output_file, trace_start, trace_end);
        free_input();
    } else {
        fprintf(stderr, "Error: Unsupported operation.\n");
        print_usage();
        fclose(input_file);
        fclose(output_file);
        return 1;
    }

    // Close the files when done
    fclose(input_file);
    fclose(output_file);

    return 0;
}