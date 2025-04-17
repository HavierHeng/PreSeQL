/* Defines the B-Tree that represents a table and stores entries, as well as its operations */
#ifndef PRESEQL_BTREE_H
#define PRESEQL_BTREE_H

/*
B+ Tree structure

Given an order of m - i.e m children per node

- Internal nodes can have between m//2 and m children
- Leaf nodes have between m/2 and m-1 data records - these point to actual locations

B+ trees are balanced - all leaf nodes are at same level, ensures consistent search performance and minimal I/O.
 */

// Max keys per node - i.e max number of children a node can have
// Max keys is (ORDER-1) for internal nodes, ORDER for leaf nodes
#define ORDER 256

// B+ Tree Node types
typedef enum {
    BTREE_INTERNAL,
    BTREE_LEAF 
} PSqlBTreeNodeType;

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
