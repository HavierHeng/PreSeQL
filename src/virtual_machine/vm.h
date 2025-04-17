/* Defines the Virtual Machine and Opcode function on execution */
#ifndef PRESEQL_VM_H
#define PRESEQL_VM_H

#include "opcodes.h"

// The VM state is effectively stored into PSqlStatement
// The statement is the VM state in and of itself
// Rather than just a bundle of instructions
typedef struct PSqlStatement {
    VMInstruction *program;     // Compiled bytecode / VM instructions
    int pc;                   // Program counter (current instruction index)

    VMRegister *registers;         // Virtual registers (used for everything)
    int register_count;

    int row_base;             // Base register index for current row
    int row_count;            // Number of columns in the row
    bool has_row;             // Is a row currently ready?

    int txn_id;               // Transaction context (if applicable)
    
    // Pointers to open tables / cursors
    Cursor **open_cursors;
    int cursor_count;

    // Result status (SQLITE_ROW, SQLITE_DONE, SQLITE_ERROR, etc.)
    int result_code;

    // Optional: error info, current database, etc.
    // Error messages will be passed to psql_err
    char *error_msg;
} PSqlStatement;

/* OPCodes -> VM Operations as functions - each OpCode has a relevant callback function defined */

#endif
