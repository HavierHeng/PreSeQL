/*
[ Page 2 ] Column Catalog (B+ Tree Root Page) - track column schema for each table
*/

#include "../../types/psql_types.h"
#include <stdint.h>

#define MAX_COLUMN_NAME 255
#define MAX_CATALOG_ENTRIES 255  // TODO: Placeholder value - I need to calculate

typedef struct {
    uint32_t table_id;  // Table ID - Foreign key to the Table catalog
    uint32_t column_index;  // 0-indexed columns
    char column_name[255];
    PSqlDataTypes type;  // Based on enum 0 - NULL, 1 - INT, 2 - TEXT
    uint32_t nullable;  // 0 - Null not allowed, 1 - Null allowed
    uint32_t is_pk;  // 0 - foreign key, 1 - primary key
} PSqlColumnCatalogEntry;

typedef struct {
    PSqlColumnCatalogEntry entries[MAX_CATALOG_ENTRIES];
} PSqlColumnCatalogPage;
