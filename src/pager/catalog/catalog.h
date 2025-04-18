/* Catalog pages are special pages that are implemented in C
 * These have a similar schema to real Index tables

[ Page 1 ] - track each table, its names and its root page
  └─ Table catalog (B+ Tree Root page): table_id, table name, root page, schema ID, type (B+/hash), flags (hidden/system)

[ Page 2 ] - track columns for each table
  └─ Column catalog (B+ Tree Root Page): table_id, column_name, type, order

[ Page 3 ] - track FK relations
  └─ Foreign Key catalog (B+ Tree Root Page): table_id, column_name, type, order

*
*/

#include "table.h"  // Table catalog - For tracking each table root page, names and type
#include "column.h"  // Column catalog - For tracking Columns for each table and their schema
#include "foreign.h"  // Foreign key catalog - For tracking foreign key constraints
