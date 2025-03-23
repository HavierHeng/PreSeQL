/* Aka the serializer, writes to disk */
#ifndef PRESEQL_PAGER_H
#define PRESEQL_PAGER_H

#include <stdio.h>
#include "btree.h"
#include "status.h"

#define PSQL_MAGIC "SQLshite"

typedef struct {
    char *magic;
    PSqlTable *tables;  /* Each B-Tree table */
    /* TODO: Add more lol, there no way the disk format is this small - plus add paging */
} PSqlFileFormat;


static PSqlStatus psql_open_db();  /* Opens file on disk */
static PSqlStatus psql_serialize_db();  /* Converts in-memory representation of database to a format to be saved on disk */
static PSqlStatus psql_close_db();  /* Writes file onto disk */

static PSqlStatus psql_open_table();  /* Gets necessary pages for table by index */
static PSqlStatus psql_close_table();  /* Gets necessary pages for table by index */

#endif
