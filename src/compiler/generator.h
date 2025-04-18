#ifndef PRESEQL_GENERATOR_H
#define PRESEQL_GENERATOR_H

#include "parser.h"
#include "../vm_engine/opcodes.h"

// Structure to represent a single VM instruction
typedef struct
{
    PSqlOpcode opcode;
    int param1;         // First numeric parameter
    int param2;         // Second numeric parameter
    char *string_param; // For storing string parameters (table names, column names, etc.)
} Instruction;

// Structure to represent a compiled program
typedef struct
{
    Instruction *instructions;
    size_t count;    // Number of instructions
    size_t capacity; // Allocated capacity
} CompiledProgram;

// Initialize a new compiled program
CompiledProgram *init_program();

// Add an instruction to the program
void emit_instruction(CompiledProgram *program, PSqlOpcode opcode,
                      int param1, int param2, const char *string_param);

// Generate code for different statement types
void generate_select(CompiledProgram *program, SelectStatement *stmt);
void generate_insert(CompiledProgram *program, InsertStatement *stmt);
void generate_create(CompiledProgram *program, CreateStatement *stmt);
void generate_control(CompiledProgram *program, ControlStatement *stmt);

// Main code generation function
CompiledProgram *generate_code(void *stmt, int stmt_type);

// Free resources associated with a compiled program
void free_program(CompiledProgram *program);

// Debug function to print the generated program
void print_program(CompiledProgram *program);

#endif