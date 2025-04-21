/* Main file included into client apps to embed database */
#ifndef PRESEQL_H
#define PRESEQL_H

#include "status/db.h"
#include "status/step.h"
#include "types/psql_types.h"
#include "compiler/parser.h"
#include "compiler/tokenizer.h"
#include "compiler/generator.h"
#include "pager/pager.h"
#include "vm_engine/vm.h"
#include "vm_engine/opcodes.h"


/* Open/Close DB file, as a read/write database */
PSqlStatus psql_open(PSql* db);
PSqlStatus psql_close(PSql* db);

/* Frontend to PreSeQL: Lexer, Parser, Code Generator steps 
 * Returns either PSQL_OK or PSQL_ERROR (if invalid)
 * */
PSqlStatus psql_prepare(PSql* db, char* sql_query, PSqlStatement* stmt);

/* Clean up VM state, but do not clear instructions inside it - i.e rewind 
 * Very useful for re-evaluating the same statement without needing to psql_prepare() again.
 * */
PSqlStatus psql_reset(PSqlStatement* stmt);

/* Clean up VM state and all instructions inside it */
PSqlStatus psql_finalize(PSqlStatement* stmt);

/* Backend to PreSeQL: VM engine, Pager and Index manipulation via cursors */
PSqlStatus psql_step(PSqlStatement* stmt);

/* Extracts the return values from the VM engine state (in Statement).
 * A function is written for each SQL datatype since it affects the return value signature
 * */
PSqlDataTypes psql_column_type(PSqlStatement* stmt, int column);   /* Query the type to be returned by statement - so know which column function to use */
PSqlStatus psql_column_text(PSqlStatement* stmt, int column, char** output);
PSqlStatus psql_column_int(PSqlStatement* stmt, int column, int* output);


/* Parameter/String subsitution via literals/variables binding */
// PSqlStatus psql_bind_text(PSqlStatement* stmt, int index, const char *value, int length);
// PSqlStatus psql_bind_text(PSqlStatement* stmt, int index, int value);

/* SQL error handling - for more info when PSQL_ERROR is returned */
const char* psql_errmsg(PSql* db);

/* Optionals */
PSqlStatus psql_exec(char* sql_stmt, char* output);  /* Combination of prepare(), step(), column(), and finalize() on one or more SQL statement */

#endif
