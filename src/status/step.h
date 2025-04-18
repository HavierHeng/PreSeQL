/* Step execution result codes */
#ifndef PRESEQL_STATUS_STEP_H
#define PRESEQL_STATUS_STEP_H

// Error status for psql_step()
typedef enum {
    PSQL_STEP_BUSY,  /* Database engine was unable acquire the locks to do its job.
                If statement is a COMMIT or occurs outside of an explicit transaction,
                retry the statement.
                If the statement is not a COMMIT and occurs within an explicit transaction, 
                rollback transaction before retrying */

    PSQL_STEP_DONE,  /* VM engine has finished execution.
                Do not call psql_step() again before psql_reset() is used to reset the engine state */

    PSQL_STEP_ROW,  /* VM engine detects that there is a return value in its register, 
                Return the value by calling the appropriate psql_column_*() function */

    PSQL_STEP_ERROR,  /* Run-time error, e.g constraint violation has occured
                More information can be obtained by calling psql_errmsg()
                */

    PSQL_STEP_MISUSE  /* VM engine was not reset 
    and instead psql_reset() was called again after PSQL_DONE was returned */
} PsqlStepStatus;


#endif
