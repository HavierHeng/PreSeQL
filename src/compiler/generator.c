#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "generator.h"

// Initialize a new compiled program
CompiledProgram *init_program()
{
    CompiledProgram *program = malloc(sizeof(CompiledProgram));
    if (!program)
        return NULL;

    program->capacity = 16; // Start with some capacity
    program->count = 0;
    program->instructions = malloc(sizeof(Instruction) * program->capacity);

    if (!program->instructions)
    {
        free(program);
        return NULL;
    }

    return program;
}

// Add an instruction to the program
void emit_instruction(CompiledProgram *program, PSqlOpcode opcode,
                      int param1, int param2, const char *string_param)
{
    // Resize the instructions array if needed
    if (program->count >= program->capacity)
    {
        program->capacity *= 2;
        program->instructions = realloc(program->instructions,
                                        sizeof(Instruction) * program->capacity);
        if (!program->instructions)
        {
            fprintf(stderr, "Memory allocation failed during code generation.\n");
            exit(EXIT_FAILURE);
        }
    }

    // Add the new instruction
    program->instructions[program->count].opcode = opcode;
    program->instructions[program->count].param1 = param1;
    program->instructions[program->count].param2 = param2;

    if (string_param)
    {
        program->instructions[program->count].string_param = strdup(string_param);
    }
    else
    {
        program->instructions[program->count].string_param = NULL;
    }

    program->count++;
}

// Generate code for SELECT statements
void generate_select(CompiledProgram *program, SelectStatement *stmt)
{
    // Open the table
    emit_instruction(program, OP_OPEN_TABLE, 0, 0, stmt->table_name);

    // For each column, add a search operation
    for (size_t i = 0; i < stmt->column_count; i++)
    {
        // If column is '*', we want all columns
        if (strcmp(stmt->columns[i], "*") == 0)
        {
            // Special case - get all columns
            emit_instruction(program, OP_SEARCH, -1, 0, NULL); // -1 indicates all columns
        }
        else
        {
            // Search for specific column
            emit_instruction(program, OP_SEARCH, i, 0, stmt->columns[i]);
        }
    }

    // Return results
    emit_instruction(program, OP_RETURN_ROW, 0, 0, NULL);

    // Halt execution
    emit_instruction(program, OP_HALT, 0, 0, NULL);
}

// Generate code for control statements (BEGIN, COMMIT)
void generate_control(CompiledProgram *program, ControlStatement *stmt)
{
    if (stmt->type == STMT_BEGIN)
    {
        emit_instruction(program, OP_BEGIN_TXN, 0, 0, NULL);
    }
    else if (stmt->type == STMT_COMMIT)
    {
        emit_instruction(program, OP_COMMIT, 0, 0, NULL);
    }
    else if (stmt->type == STMT_ROLLBACK)
    {
        emit_instruction(program, OP_ROLLBACK, 0, 0, NULL);
    }

    // Halt execution
    emit_instruction(program, OP_HALT, 0, 0, NULL);
}

// Generate code for INSERT statements
void generate_insert(CompiledProgram *program, InsertStatement *stmt)
{
    // Begin transaction
    emit_instruction(program, OP_BEGIN_TXN, 0, 0, NULL);

    // Open the table
    emit_instruction(program, OP_OPEN_TABLE, 0, 0, stmt->table_name);

    // Add INSERT operation for each column/value pair
    for (size_t i = 0; i < stmt->column_count; i++)
    {
        if (stmt->values[i].type == VALUE_INTEGER)
        {
            emit_instruction(program, OP_INSERT, i, stmt->values[i].int_value, stmt->columns[i]);
        }
        else if (stmt->values[i].type == VALUE_TEXT)
        {
            // For text values, we store the column name as param1 and use string_param for the value
            emit_instruction(program, OP_INSERT, i, 0, stmt->values[i].text_value);
        }
    }

    // Commit transaction
    emit_instruction(program, OP_COMMIT, 0, 0, NULL);

    // Halt execution
    emit_instruction(program, OP_HALT, 0, 0, NULL);
}
// Generate code for CREATE TABLE statements
void generate_create(CompiledProgram *program, CreateStatement *stmt)
{
    // Create the table
    emit_instruction(program, OP_CREATE_TABLE, 0, 0, stmt->table_name);

    // Define schema
    for (size_t i = 0; i < stmt->column_count; i++)
    {
        int type = (stmt->columns[i].type == COLTYPE_INT) ? 1 : 2; // 1 for INT, 2 for VARCHAR
        int length = stmt->columns[i].type_length;

        emit_instruction(program, OP_DEFINE_SCHEMA, type, length, stmt->columns[i].name);
    }

    // Commit creation
    emit_instruction(program, OP_COMMIT, 0, 0, NULL);

    // Halt execution
    emit_instruction(program, OP_HALT, 0, 0, NULL);
}

// Main generator function that takes a parsed statement and generates code
CompiledProgram *generate_code(void *stmt, int stmt_type)
{
    CompiledProgram *program = init_program();
    if (!program)
        return NULL;

    switch (stmt_type)
    {
    case STMT_SELECT:
        generate_select(program, (SelectStatement *)stmt);
        break;
    case STMT_INSERT:
        generate_insert(program, (InsertStatement *)stmt);
        break;
    case STMT_CREATE:
        generate_create(program, (CreateStatement *)stmt);
        break;
    case STMT_BEGIN:
    case STMT_COMMIT:
    case STMT_ROLLBACK:
        generate_control(program, (ControlStatement *)stmt);
        break;
    default:
        fprintf(stderr, "Unsupported statement type for code generation.\n");
        free_program(program);
        return NULL;
    }

    return program;
}

// Free resources associated with a compiled program
void free_program(CompiledProgram *program)
{
    if (!program)
        return;

    // Free string parameters in instructions
    for (size_t i = 0; i < program->count; i++)
    {
        free(program->instructions[i].string_param);
    }

    free(program->instructions);
    free(program);
}

// Debug function to print the generated program
void print_program(CompiledProgram *program)
{
    printf("Generated program with %zu instructions:\n", program->count);

    // Map opcodes to their string representation for readability
    const char *opcode_names[] = {
        "OP_CREATE_TABLE", "OP_OPEN_TABLE", "OP_DEFINE_SCHEMA", "OP_DROP_TABLE",
        "OP_INSERT", "OP_SEARCH", "OP_DELETE", "OP_UPDATE",
        "OP_DELETE_ROW", "OP_RETURN_ROW", "OP_COMPARE", "OP_JUMP_IF_FALSE", "OP_HALT",
        "OP_BEGIN_TXN", "OP_COMMIT", "OP_ROLLBACK",
        "OP_OUTER_JOIN", "OP_INNER_JOIN", "OP_SORT", "OP_SWAP_ROWS",
        "OP_GET_SCHEMA", "OP_GET_LOGS", "OP_GET_BTREEINFO", "OP_GET_DBPAGE", "OP_GET_MEMSET",
        "OP_NOP"};

    for (size_t i = 0; i < program->count; i++)
    {
        PSqlOpcode op = program->instructions[i].opcode;
        printf("  %zu: %s, P1=%d, P2=%d, String=%s\n",
               i,
               op < sizeof(opcode_names) / sizeof(opcode_names[0]) ? opcode_names[op] : "UNKNOWN",
               program->instructions[i].param1,
               program->instructions[i].param2,
               program->instructions[i].string_param ? program->instructions[i].string_param : "NULL");
    }
}