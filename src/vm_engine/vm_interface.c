/* Implementation of VM backend entry points */

#include "../preseql.h"
#include "vm.h"

PSqlStatus psql_step(PSqlStatement* stmt) {
    if (!stmt) {
        return PSQL_MISUSE;
    }

    // Execute one VM instruction
    psql_vm_step(stmt);

    // Convert VM result code to PSqlStatus
    switch (stmt->result_code) {
        case PSQL_STEP_ROW:
            return PSQL_OK;  // We have a row
        case PSQL_STEP_DONE:
            return PSQL_OK;  // We're done, but it's still a success
        case PSQL_STEP_ERROR:
            return PSQL_ERROR;
        case PSQL_STEP_BUSY:
            return PSQL_BUSY;
        case PSQL_STEP_MISUSE:
            return PSQL_MISUSE;
        default:
            return PSQL_INTERNAL;
    }
}

PSqlStatus psql_finalize(PSqlStatement* stmt) {
    if (!stmt) {
        return PSQL_MISUSE;
    }
    
    // Clean up VM resources
    psql_vm_finalize(stmt);
    
    // Free the statement itself
    free(stmt->program);
    free(stmt);
    
    return PSQL_OK;
}
