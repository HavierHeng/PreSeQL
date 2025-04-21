/* Status codes for database operations */
#ifndef PRESEQL_STATUS_DB_H
#define PRESEQL_STATUS_DB_H

typedef enum {
    PSQL_OK,       /* Operation successful */
    PSQL_ERROR,    /* Generic error */
    PSQL_BUSY,     /* Database file is locked */
    PSQL_NOMEM,    /* Out of memory */
    PSQL_READONLY, /* Attempt to write to readonly database */
    PSQL_IOERR,    /* Some kind of disk I/O error */
    PSQL_CORRUPT,  /* Database disk image is malformed */
    PSQL_NOTFOUND, /* Table or record not found */
    PSQL_FULL,     /* Database or disk is full */
    PSQL_MISUSE,   /* API used incorrectly - psql_step() after PSQL_STEP_DONE */
    PSQL_INTERNAL  /* Internal logic error */
} PSqlStatus;

#endif
