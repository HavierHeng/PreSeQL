/* Catalog pages are special pages that are implemented in C
 * These have a similar schema to real Index tables

[ Page 1 ] - track each table, its names and its root page
  └─ Table catalog (B+ Tree Root page): table_id, table name, root page, schema ID, flags (hidden/system)

[ Page 2 ] - track columns for each table
  └─ Column catalog (B+ Tree Root Page): table_id, column_name, type, order

[ Page 3 ] - track FK relations
  └─ Foreign Key catalog (B+ Tree Root Page): table_id, column_name, type, order
  
[ Page 4 ] - support multiple index types, and track root pages for each table
  └─ Index catalog (B+ Tree Index Roots): table_id, index_name, root_page, type (B+ or hash)

*
*/
