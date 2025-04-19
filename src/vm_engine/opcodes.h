/* Defines most of the OPCodes for the Virtual Machine to function */
#ifndef PRESEQL_OPCODE_H
#define PRESEQL_OPCODE_H

#define MAX_OPCODES 50
/* Note - each OPCode also has its set of valid parameters behind which is not clear here */
typedef enum
{
    /* Placeholder for unknown/unimplemented operations */
    OP_NOP = 0,

    /* Table Operations */
    OP_CREATE_TABLE,  /* New B-Tree representing Table */
    OP_OPEN_TABLE,    /* Open existing table */
    OP_DEFINE_SCHEMA, /* Define a schema - used in OP_CREATE_TABLE */
    OP_DROP_TABLE,    /* Delete a table */

    /* Data Operations - ACID */
    OP_INSERT, /* Insert row into B-Tree given a key and value */
    OP_SEARCH, /* Find a row matching the condition */
    OP_DELETE, /* Delete current columns row's value */
    OP_UPDATE, /* Update a column row with a new value */

    /* Row Data Operations */
    OP_DELETE_ROW, /* Delete entire row */

    /* Query Execution */
    OP_RETURN_ROW,    /* Return the result of a query */
    OP_COMPARE,       /* Compare a column's row with a condition such as =, <, > */
    OP_JUMP_IF_FALSE, /* Jump to label if condition is false */
    OP_HALT,          /* Stop execution */

    /* Transaction Management */
    OP_BEGIN_TXN, /* Start a transaction */
    OP_COMMIT,    /* Commit all changes - also flushes to disk */
    OP_ROLLBACK,  /* Undo changes */

    /* Join operations for doing primary key-foreign key table relations */
    OP_OUTER_JOIN, /* Join between two tables on column 1 to column 2, can either be a left join or a right join */
    OP_INNER_JOIN, /* Join between two tables on column 1 to column 2, can either be a left or right join */

    /* Sorting e.g for ORDER BY */
    OP_SORT,      /* Sort by order or ascending or descending */
    OP_SWAP_ROWS, /* Swap two rows and renumber index */

    /* Virtual Table OPCodes - Debug info for nerds
     * V-Table OPCodes are fake SQL tables emulated by the VM but not stored anywhere on disk.
     * */
    OP_GET_SCHEMA,
    OP_GET_LOGS,
    OP_GET_BTREEINFO,
    OP_GET_DBPAGE,
    OP_GET_MEMSET
} PSqlOpcode;

typedef struct
{
    PSqlOpcode opcode;
    int a, b, c;
} PSqlInstruction;

#endif
