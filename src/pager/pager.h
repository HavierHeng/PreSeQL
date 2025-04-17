/* Aka the serializer, but also handles read/writes to disk 
* This is also where the B-Tree is handled - since its nodes are tied to pages on disk

[ Page 0 ]
  └─ Magic number, Page size, Version
  └─ Roots of special system catalogs tables (page 1-4 )

[ Page 1 ] - track each table, its names and its root page
  └─ Table catalog (B+ Tree Root page): table_id, table name, root page, schema ID, flags (hidden/system)

[ Page 2 ] - track columns for each table
  └─ Column catalog (B+ Tree Root Page): table_id, column_name, type, order

[ Page 3 ] - track FK relations
  └─ Foreign Key catalog (B+ Tree Root Page): table_id, column_name, type, order
  
[ Page 4 ] - support multiple index types, and track root pages for each table
  └─ Index catalog (B+ Tree Index Roots): table_id, index_name, root_page, type (B+ or hash)

[ Page 5..X ]
  └─ Table Data Pages (slotted page implementation) - stores the actual data
  └─ Index Pages (B+ trees) - Internal nodes, and leaf nodes which point to data (or other nodes)
  └─ Overflow Pages - for big INTEGER and TEXT that do not fit
  └─ Free Pages - holes in pages created after deletion of pages (a bitmap/radix tree in the Page 0 header keeps track of what pages are free for reuse - to fill in holes)
 * */

#ifndef PRESEQL_PAGER_H
#define PRESEQL_PAGER_H

#include <cstdint>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>  // For specifying mmap() flags
#include <sys/stat.h>  // Handle structs of data by stat() functions
#include "page_format.h"
#include "../algorithm/radix_tree.h"  // For efficiently finding free pages
#include "status.h"


// Max keys per B+ Tree node - i.e max number of children a node can have - i.e fanout
// Max keys is (ORDER-1) for internal nodes, ORDER for leaf nodes
#define ORDER 256

/* Manage Free Pages via Radix Tree */


#endif
