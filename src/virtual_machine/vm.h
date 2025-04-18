/* Defines the Virtual Machine and Opcode function on execution */
#ifndef PRESEQL_VM_H
#define PRESEQL_VM_H

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include "opcodes.h"
#include "../status/step.h"

/* Forward declarations */
typedef struct PSqlCursor Cursor;
typedef int PSqlRegister;  // Using int as the register type for simplicity

// The VM state is effectively stored into PSqlStatement
// The statement is the VM state in and of itself
// Rather than just a bundle of instructions
typedef struct PSqlStatement {
    PSqlInstruction *program;     // Compiled bytecode / VM instructions
    int pc;                   // Program counter (current instruction index)

    PSqlRegister *registers;         // Virtual registers (used for everything from scratch registers to returning values)
    int register_count;

    int row_base;             // Base register index for current row
    int row_count;            // Number of columns in the row
    bool has_row;             // Is a row currently ready?

    int txn_id;               // Transaction ID, if applicable (BEGIN TRANSACTION)
    
    // Pointers to open tables / cursors
    Cursor **open_cursors;
    int cursor_count;

    // Result status (SQLITE_ROW, SQLITE_DONE, SQLITE_ERROR, etc.)
    PsqlStepStatus result_code;

    // Optional: error info, current database, etc.
    // Error messages will be passed to psql_err
    char *error_msg;
} PSqlStatement;

/* Function pointer type for opcode handlers */
typedef void (*PSqlOpFunc)(PSqlStatement *vm, int a, int b, int c);

/* OPCodes -> VM Operations as functions - each OpCode has a relevant callback function defined */
void psql_op_open_table(PSqlStatement *vm, int a, int b, int c) {
    // example
    // a = register to write cursor handle
    // b = table id
    // c = unused
    printf("Open table %d into register %d\n", b, a);
}

void psql_op_return_row(PSqlStatement *vm, int a, int b, int c) {
    // Usually flushes values in registers a..b out to client
    //
    // TODO: bind to file
    printf("Return row: registers %d to %d\n", a, b);
}

/* Additional opcode handlers according to your opcodes.h file */
void psql_op_create_table(PSqlStatement *vm, int a, int b, int c) {
    printf("Create table, schema in register %d\n", a);
}

void psql_op_define_schema(PSqlStatement *vm, int a, int b, int c) {
    printf("Define schema for table at cursor %d\n", a);
}

void psql_op_insert(PSqlStatement *vm, int a, int b, int c) {
    printf("Insert row from registers %d to %d into table at cursor %d\n", b, c, a);
}

void psql_op_search(PSqlStatement *vm, int a, int b, int c) {
    printf("Search for row with key in register %d in table at cursor %d\n", b, a);
}

void psql_op_compare(PSqlStatement *vm, int a, int b, int c) {
    printf("Compare value in register %d with register %d, store result in register %d\n", b, c, a);
}

void psql_op_jump_if_false(PSqlStatement *vm, int a, int b, int c) {
    if (vm->registers[a] == 0) {
        vm->pc = b;  // Jump to instruction at index b
    }
    printf("Jump to instruction %d if register %d is false\n", b, a);
}

void psql_op_halt(PSqlStatement *vm, int a, int b, int c) {
    printf("Halt execution\n");
    vm->result_code = a;  // Set result code to a
}

// Jump table
PSqlOpFunc psql_jump_table[MAX_OPCODES] = {
    [OP_CREATE_TABLE] = psql_op_create_table,
    [OP_OPEN_TABLE] = psql_op_open_table,
    [OP_DEFINE_SCHEMA] = psql_op_define_schema,
    [OP_RETURN_ROW] = psql_op_return_row,
    [OP_INSERT] = psql_op_insert,
    [OP_SEARCH] = psql_op_search,
    [OP_COMPARE] = psql_op_compare,
    [OP_JUMP_IF_FALSE] = psql_op_jump_if_false,
    [OP_HALT] = psql_op_halt,
    // TODO: Bind more ops to the jump table
};

// Execute each instruction - psql_step()
void psql_vm_step(PSqlStatement *vm) {
    PSqlInstruction *inst = &vm->program[vm->pc++];
    if (inst->opcode >= MAX_OPCODES || psql_jump_table[inst->opcode] == NULL) {
        vm->result_code = PSQL_ERROR;  // Assuming you have an error code defined
        vm->error_msg = "Invalid opcode";
        return;
    }
    psql_jump_table[inst->opcode](vm, inst->a, inst->b, inst->c);
}

/* Initialize a VM with allocated memory */
void psql_vm_init(PSqlStatement *vm, int reg_count) {
    vm->pc = 0;
    vm->registers = (PSqlRegister*)calloc(reg_count, sizeof(PSqlRegister));
    vm->register_count = reg_count;
    vm->row_base = 0;
    vm->row_count = 0;
    vm->has_row = false;
    vm->open_cursors = NULL;
    vm->cursor_count = 0;
    vm->result_code = 0;  // Success
    vm->error_msg = NULL;
}

/* Clean up VM resources */
void psql_vm_finalize(PSqlStatement *vm) {
    free(vm->registers);
    // Free other allocated resources like cursors
    for (int i = 0; i < vm->cursor_count; i++) {
        // Assuming you have a function to close cursors
        // psql_cursor_close(vm->open_cursors[i]);
    }
    free(vm->open_cursors);
    free(vm->error_msg);
}

#endif
