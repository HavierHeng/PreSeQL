// Internal data types for PreSeqL. 
// The more that is defined, the more handlers has to be made (and also sorting in B+-Tree changes)
#ifndef PRESEQL_TYPES_H
#define PRESEQL_TYPES_H
typedef enum {
    PSQL_NULL,
    PSQL_INT,
    PSQL_TEXT
} PSqlDataTypes;

#endif
