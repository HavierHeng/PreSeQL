/* Defines the Virtual Machine and Opcode function on execution */
#ifndef PRESEQL_VM_H
#define PRESEQL_VM_H

#include "opcodes.h"

typedef struct {
    /* TODO: State Machine needs more states to track its manipulation of B-Tree */
} PsqlVirtualMachine;

/* Function to parse OPCodes - big switch statement */
PSqlStatus psql_exec_opcode(PsqlVirtualMachine *vm, PsqlOpCode op, int argc, char** argv);  /* Takes in VM, Opcode, number of arguments and args as a char arr */


/* OPCodes -> VM Operations as functions - each OpCode has a relevant callback function defined */

#endif
