/* Catalog pages are special pages that are implemented in C
 * These are real Index tables but their initialization is hardcoded into the database engine - such that whenever it boots it calls all the 3 initialization methods 

[ Page 1 ] Table Catalog (B+ Tree Root Page) - track each Index Page for each table, its names and its root page and Index type

[ Page 2 ] Column Catalog (B+ Tree Root Page) - track column schema for each table

[ Page 3 ] Foreign Key Catalog - track Foreign Key Constraints between table to table

*/

#include "table.h"  // Table catalog - For tracking each table root page, names and type
#include "column.h"  // Column catalog - For tracking Columns for each table and their schema
#include "foreign.h"  // Foreign key catalog - For tracking foreign key constraints


// TODO: Define helper functions to initialize catalog pages here - they are fundamentally B+ Tree Nodes - these are initialized right away once a new database is open with some initial values
