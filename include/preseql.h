/* Main file included into client apps to embed database */
#ifndef PRESEQL_H
#define PRESEQL_H

#include "opcodes.h"
#include "status.h"
#include "sqltypes.h"

/* SQL Database Connection */
typedef struct {
    int status;
} PSql;

PSqlStatus psql_create(PSql* db);
PSqlStatus psql_destroy(PSql* db);

/* SQL Statement */
typedef struct {
} PSqlStmt;

PSqlStatus psql_prepare(PSqlStmt* stmt, char*);
PSqlStatus psql_finalize(PSqlStmt* stmt);
PSqlStatus psql_step(PSqlStmt* stmt);

/* SQL Statement but returns a value 
 * A function is written for each SQL datatype since it affects the return value signature
 * */
PSqlDataTypes psql_column_type(PSqlStmt* stmt);   /* Query the type to be returned by statement - so know which column function to use */
PSqlStatus psql_column_text(PSqlStmt* stmt, char* output);
PSqlStatus psql_column_int(PSqlStmt* stmt, int* output);


/* Parameter/String subsitution via literals/variables binding */
PSqlStatus psql_column_text(PSqlStmt* stmt, int index, char *value);
PSqlStatus psql_column_int(PSqlStmt* stmt, int index, int value);

/* Optionals */
PSqlStatus psql_exec(char* sql_stmt, char* output);  /* Combination of prepare(), step(), column(), and finalize() on one or more SQL statement */

#endif
