/* Defines the B-Tree that represents a table and stores entries, as well as its operations */
#ifndef PRESEQL_BTREE_H
#define PRESEQL_BTREE_H

/* Actual B+ Tree that stores table entries */
typedef struct {
    /* TODO: Add more */
} PSqlBTree;

/* Metadata table that stores info of datatypes of each table */
typedef struct {
    /* TODO: Decide what to store as metadata for datatypes and how */
} PSqlMeta;

/* Table instance */
typedef struct {
    PSqlMeta meta;
    PSqlBTree btree;
    /* TODO: Add more */
} PSqlTable;


#endif
