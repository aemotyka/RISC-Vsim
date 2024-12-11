#ifndef PIPELINE_H
#define PIPELINE_H

#include <stdio.h>

void IF();
void IS();
void ID();
void RF();
void EX();
void DF();
void DS();
void WB();

void initialize();
void handle_forward();

void simulate(FILE* input_file, FILE* output_file, int trace_start, int trace_end);

#endif