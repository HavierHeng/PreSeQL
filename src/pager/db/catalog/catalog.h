/* Catalog pages are special pages that are implemented in C
 * These are real Index tables but their initialization is hardcoded into the database engine - such that whenever it boots it calls all the 3 initialization methods 

[ Page 1 ] Table Catalog (B+ Tree Root Page) - track each Index Page for each table, its names and its root page and Index type

[ Page 2 ] Column Catalog (B+ Tree Root Page) - track column schema for each table

[ Page 3 ] Foreign Key Catalog - track Foreign Key Constraints between table to table

*/

#ifndef PRESEQL_PAGER_DB_CATALOG_H
#define PRESEQL_PAGER_DB_CATALOG_H

#include <stdint.h>
#include <string.h>
#include "pager/types.h"
#include "pager/constants.h"
#include "status/db.h"

// Catalog initialization functions - These are called when new DB file is created
// The primary keys are always going to be the first ID for each catalog table
PSqlStatus init_table_catalog(DBPage* page);
PSqlStatus init_column_catalog(DBPage* page);
PSqlStatus init_fk_catalog(DBPage* page);

// Table catalog operations
PSqlStatus catalog_add_table(const char* table_name, uint16_t root_page, uint8_t index_type, uint8_t flags, uint16_t* out_table_id);
PSqlStatus catalog_get_table_by_name(const char* table_name, uint16_t* out_table_id, uint16_t* out_root_page);
PSqlStatus catalog_get_table_by_id(uint16_t table_id, char** out_table_name, uint16_t* out_root_page);
PSqlStatus catalog_delete_table(uint16_t table_id);

// Column catalog operations
PSqlStatus catalog_add_column(uint16_t table_id, uint16_t column_index, const char* column_name, 
                             uint8_t column_type, uint8_t nullable, uint8_t is_pk);
PSqlStatus catalog_get_column_by_name(uint16_t table_id, const char* column_name, uint16_t* out_column_index, 
                                     uint8_t* out_column_type, uint8_t* out_nullable, uint8_t* out_is_pk);
PSqlStatus catalog_get_column_by_index(uint16_t table_id, uint16_t column_index, char** out_column_name, 
                                      uint8_t* out_column_type, uint8_t* out_nullable, uint8_t* out_is_pk);
PSqlStatus catalog_get_all_columns(uint16_t table_id, uint16_t* out_column_count);

// Foreign key catalog operations
PSqlStatus catalog_add_foreign_key(uint16_t from_table, uint16_t from_column, uint16_t to_table, 
                                  uint16_t to_column, uint8_t on_delete, uint8_t on_update, uint16_t* out_fk_id);
PSqlStatus catalog_get_foreign_keys_for_table(uint16_t table_id, uint16_t** out_fk_ids, uint16_t* out_count);
PSqlStatus catalog_get_foreign_key_details(uint16_t fk_id, uint16_t* out_from_table, uint16_t* out_from_column,
                                         uint16_t* out_to_table, uint16_t* out_to_column,
                                         uint8_t* out_on_delete, uint8_t* out_on_update);

#endif
