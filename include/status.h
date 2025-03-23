/* Statuses for operations */
#ifndef PRESEQL_STATUS_H
#define PRESEQL_STATUS_H

/* Function/Operation Status */
typedef enum {
    PSQL_SUCCESS,
    PSQL_ERROR,
} PSqlStatus;

/* Connection Status */
typedef enum {
    PSQL_CONNECTED,
    PSQL_CONN_FAILURE,
} PSqlConnStatus;

#endif 
