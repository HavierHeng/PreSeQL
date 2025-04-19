/* Defines the Virtual Machine and Opcode function on execution */
#ifndef PRESEQL_VM_H
#define PRESEQL_VM_H

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include "opcodes.h"
#include "status/step.h"

/* Forward declarations */
typedef struct PSqlCursor Cursor;
typedef int PSqlRegister;  // Using int as the register type for simplicity

//TODO: PSqlStatement to be arena allocated as well - just like radix tree


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
    PSqlStepStatus result_code;

    // Optional: error info, current database, etc.
    // Error messages will be passed to psql_err
    char *error_msg;
} PSqlStatement;

/* Function pointer type for opcode handlers */
typedef void (*PSqlOpFunc)(PSqlStatement *vm, int a, int b, int c);
#endif
