#include "pager/db/catalog/catalog.h"
#include "pager/db/index/index_page.h"
#include "pager/db/free_space.h"
#include "pager/pager.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// Catalog table IDs
#define TABLE_CATALOG_ID 1
#define COLUMN_CATALOG_ID 2
#define FK_CATALOG_ID 3

// Flag values
#define FLAG_HIDDEN 0x01
#define FLAG_SYSTEM 0x02
#define FLAG_TEMPORARY 0x04
#define FLAG_WITHOUT_ROWID 0x08
#define FLAG_VIRTUAL 0x10
#define FLAG_AUTOINCREMENT 0x20

// Secondary index root pages
static uint16_t table_name_index_root = 0;
static uint16_t column_name_index_root = 0;
static uint16_t fk_from_table_index_root = 0;
static uint16_t fk_to_table_index_root = 0;

// Helper function to get current timestamp
static uint32_t get_current_timestamp() {
    return (uint32_t)time(NULL);
}

// Initialize the table catalog (Page 1)
PSqlStatus init_table_catalog(DBPage* page) {
    if (!page || page->header.page_id != 1) return PSQL_MISUSE;

    Pager* pager = NULL;
    if (!init_index_leaf_page(pager, 1)) return PSQL_IOERR;

    uint8_t table_key[sizeof(uint16_t)];
    uint8_t table_value[256];
    uint16_t value_size;

    // Table 1: Table Catalog
    *(uint16_t*)table_key = TABLE_CATALOG_ID;
    value_size = 0;
    const char* table_name = "__table_catalog__";
    uint16_t name_len = strlen(table_name) + 1;
    *(uint16_t*)(table_value + value_size) = name_len;
    value_size += sizeof(uint16_t);
    memcpy(table_value + value_size, table_name, name_len);
    value_size += name_len;
    *(uint16_t*)(table_value + value_size) = 1; // root_page
    value_size += sizeof(uint16_t);
    table_value[value_size++] = 0; // index_type
    table_value[value_size++] = FLAG_SYSTEM | FLAG_HIDDEN; // flags
    *(uint16_t*)(table_value + value_size) = 1; // schema_version
    value_size += sizeof(uint16_t);
    *(uint32_t*)(table_value + value_size) = get_current_timestamp();
    value_size += sizeof(uint32_t);
    *(uint32_t*)(table_value + value_size) = get_current_timestamp();
    value_size += sizeof(uint32_t);

    PSqlStatus status = btree_insert_value(pager, 1, table_key, sizeof(table_key), table_value, value_size);
    if (status != PSQL_OK) return status;

    // Table 2: Column Catalog
    *(uint16_t*)table_key = COLUMN_CATALOG_ID;
    value_size = 0;
    table_name = "__column_catalog__";
    name_len = strlen(table_name) + 1;
    *(uint16_t*)(table_value + value_size) = name_len;
    value_size += sizeof(uint16_t);
    memcpy(table_value + value_size, table_name, name_len);
    value_size += name_len;
    *(uint16_t*)(table_value + value_size) = 2;
    value_size += sizeof(uint16_t);
    table_value[value_size++] = 0;
    table_value[value_size++] = FLAG_SYSTEM | FLAG_HIDDEN;
    *(uint16_t*)(table_value + value_size) = 1;
    value_size += sizeof(uint16_t);
    *(uint32_t*)(table_value + value_size) = get_current_timestamp();
    value_size += sizeof(uint32_t);
    *(uint32_t*)(table_value + value_size) = get_current_timestamp();
    value_size += sizeof(uint32_t);

    status = btree_insert_value(pager, 1, table_key, sizeof(table_key), table_value, value_size);
    if (status != PSQL_OK) return status;

    // Table 3: Foreign Key Catalog
    *(uint16_t*)table_key = FK_CATALOG_ID;
    value_size = 0;
    table_name = "__fk_catalog__";
    name_len = strlen(table_name) + 1;
    *(uint16_t*)(table_value + value_size) = name_len;
    value_size += sizeof(uint16_t);
    memcpy(table_value + value_size, table_name, name_len);
    value_size += name_len;
    *(uint16_t*)(table_value + value_size) = 3;
    value_size += sizeof(uint16_t);
    table_value[value_size++] = 0;
    table_value[value_size++] = FLAG_SYSTEM | FLAG_HIDDEN;
    *(uint16_t*)(table_value + value_size) = 1;
    value_size += sizeof(uint16_t);
    *(uint32_t*)(table_value + value_size) = get_current_timestamp();
    value_size += sizeof(uint32_t);
    *(uint32_t*)(table_value + value_size) = get_current_timestamp();
    value_size += sizeof(uint32_t);

    status = btree_insert_value(pager, 1, table_key, sizeof(table_key), table_value, value_size);
    return status;
}

// Initialize the column catalog (Page 2)
PSqlStatus init_column_catalog(DBPage* page) {
    if (!page || page->header.page_id != 2) return PSQL_MISUSE;

    Pager* pager = NULL; // TODO: Initialize properly
    if (!init_index_leaf_page(pager, 2)) return PSQL_IOERR;

    uint8_t column_key[sizeof(uint16_t) * 2];
    uint8_t column_value[256];
    uint16_t value_size;

    // Columns for Table Catalog (table_id = 1)
    const char* column_names[] = {
        "table_id", "table_name", "root_page", "index_type",
        "flags", "schema_version", "created_on", "last_modified"
    };
    uint8_t column_types[] = {1, 2, 1, 1, 1, 1, 1, 1}; // INT=1, TEXT=2
    uint8_t column_nullable[] = {0, 1, 0, 0, 0, 0, 0, 0};
    uint8_t column_is_pk[] = {1, 0, 0, 0, 0, 0, 0, 0};

    PSqlStatus status;
    for (int i = 0; i < 8; i++) {
        *(uint16_t*)column_key = TABLE_CATALOG_ID;
        *(uint16_t*)(column_key + sizeof(uint16_t)) = i;
        value_size = 0;
        uint16_t name_len = strlen(column_names[i]) + 1;
        *(uint16_t*)(column_value + value_size) = name_len;
        value_size += sizeof(uint16_t);
        memcpy(column_value + value_size, column_names[i], name_len);
        value_size += name_len;
        column_value[value_size++] = column_types[i];
        column_value[value_size++] = column_nullable[i];
        column_value[value_size++] = column_is_pk[i];
        status = btree_insert_value(pager, 2, column_key, sizeof(column_key), column_value, value_size);
        if (status != PSQL_OK) return status;
    }

    // Columns for Column Catalog (table_id = 2)
    const char* col_column_names[] = {
        "table_id", "column_index", "column_name", "column_type",
        "nullable", "is_pk"
    };
    uint8_t col_column_types[] = {1, 1, 2, 1, 1, 1};
    uint8_t col_column_nullable[] = {0, 0, 0, 0, 0, 0};
    uint8_t col_column_is_pk[] = {1, 1, 0, 0, 0, 0};

    for (int i = 0; i < 6; i++) {
        *(uint16_t*)column_key = COLUMN_CATALOG_ID;
        *(uint16_t*)(column_key + sizeof(uint16_t)) = i;
        value_size = 0;
        uint16_t name_len = strlen(col_column_names[i]) + 1;
        *(uint16_t*)(column_value + value_size) = name_len;
        value_size += sizeof(uint16_t);
        memcpy(column_value + value_size, col_column_names[i], name_len);
        value_size += name_len;
        column_value[value_size++] = col_column_types[i];
        column_value[value_size++] = col_column_nullable[i];
        column_value[value_size++] = col_column_is_pk[i];
        status = btree_insert_value(pager, 2, column_key, sizeof(column_key), column_value, value_size);
        if (status != PSQL_OK) return status;
    }

    // Columns for FK Catalog (table_id = 3)
    const char* fk_column_names[] = {
        "fk_id", "from_table", "from_column", "to_table",
        "to_column", "on_delete", "on_update"
    };
    uint8_t fk_column_types[] = {1, 1, 1, 1, 1, 1, 1};
    uint8_t fk_column_nullable[] = {0, 0, 0, 0, 0, 0, 0};
    uint8_t fk_column_is_pk[] = {1, 0, 0, 0, 0, 0, 0};

    for (int i = 0; i < 7; i++) {
        *(uint16_t*)column_key = FK_CATALOG_ID;
        *(uint16_t*)(column_key + sizeof(uint16_t)) = i;
        value_size = 0;
        uint16_t name_len = strlen(fk_column_names[i]) + 1;
        *(uint16_t*)(column_value + value_size) = name_len;
        value_size += sizeof(uint16_t);
        memcpy(column_value + value_size, fk_column_names[i], name_len);
        value_size += name_len;
        column_value[value_size++] = fk_column_types[i];
        column_value[value_size++] = fk_column_nullable[i];
        column_value[value_size++] = fk_column_is_pk[i];
        status = btree_insert_value(pager, 2, column_key, sizeof(column_key), column_value, value_size);
        if (status != PSQL_OK) return status;
    }

    return PSQL_OK;
}

// Initialize the foreign key catalog (Page 3)
PSqlStatus init_fk_catalog(DBPage* page) {
    if (!page || page->header.page_id != 3) return PSQL_MISUSE;

    Pager* pager = NULL; // TODO: Initialize properly
    if (!init_index_leaf_page(pager, 3)) return PSQL_IOERR;

    uint8_t fk_key[sizeof(uint16_t)];
    uint8_t fk_value[256];
    uint16_t value_size = 0;

    *(uint16_t*)fk_key = 1;
    *(uint16_t*)(fk_value + value_size) = COLUMN_CATALOG_ID; // from_table
    value_size += sizeof(uint16_t);
    *(uint16_t*)(fk_value + value_size) = 0; // from_column (table_id)
    value_size += sizeof(uint16_t);
    *(uint16_t*)(fk_value + value_size) = TABLE_CATALOG_ID; // to_table
    value_size += sizeof(uint16_t);
    *(uint16_t*)(fk_value + value_size) = 0; // to_column (table_id)
    value_size += sizeof(uint16_t);
    fk_value[value_size++] = 1; // on_delete (RESTRICT)
    fk_value[value_size++] = 1; // on_update (RESTRICT)

    return btree_insert_value(pager, 3, fk_key, sizeof(fk_key), fk_value, value_size);
}

// Helper function to build secondary indexes
PSqlStatus build_secondary_indexes(Pager* pager) {
    if (!pager) return PSQL_MISUSE;

    table_name_index_root = get_free_page(pager);
    column_name_index_root = get_free_page(pager);
    fk_from_table_index_root = get_free_page(pager);
    fk_to_table_index_root = get_free_page(pager);

    if (!table_name_index_root || !column_name_index_root ||
        !fk_from_table_index_root || !fk_to_table_index_root) {
        return PSQL_FULL;
    }

    if (!init_index_leaf_page(pager, table_name_index_root) ||
        !init_index_leaf_page(pager, column_name_index_root) ||
        !init_index_leaf_page(pager, fk_from_table_index_root) ||
        !init_index_leaf_page(pager, fk_to_table_index_root)) {
        return PSQL_IOERR;
    }

    uint8_t key[256];
    uint16_t key_size;
    uint8_t value[sizeof(uint16_t) * 2];  // 4 bytes to match column_key
    uint8_t table_value[256];
    uint16_t table_value_size;
    PSqlStatus status;

    for (uint16_t table_id = 1; table_id <= 3; table_id++) {
        status = btree_find_row(pager, 1, (uint8_t*)&table_id, sizeof(table_id), table_value, &table_value_size);
        if (status == PSQL_OK) {
            uint16_t name_len = *(uint16_t*)table_value;
            memcpy(key, table_value + sizeof(uint16_t), name_len);
            key_size = name_len;
            *(uint16_t*)value = table_id;
            status = btree_insert_value(pager, table_name_index_root, key, key_size, value, sizeof(value));
            if (status != PSQL_OK) return status;
        }
    }

    uint8_t column_key[sizeof(uint16_t) * 2];
    for (uint16_t table_id = 1; table_id <= 3; table_id++) {
        for (uint16_t col_idx = 0; col_idx < 8; col_idx++) {
            *(uint16_t*)column_key = table_id;
            *(uint16_t*)(column_key + sizeof(uint16_t)) = col_idx;
            status = btree_find_row(pager, 2, column_key, sizeof(column_key), table_value, &table_value_size);
            if (status == PSQL_OK) {
                uint16_t name_len = *(uint16_t*)table_value;
                memcpy(key, table_value + sizeof(uint16_t), name_len);
                key_size = name_len;
                memcpy(value, column_key, sizeof(column_key));
                status = btree_insert_value(pager, column_name_index_root, key, key_size, value, sizeof(value));
                if (status != PSQL_OK) return status;
            }
        }
    }

    uint8_t fk_key[sizeof(uint16_t)];
    for (uint16_t fk_id = 1; fk_id <= 1; fk_id++) {
        *(uint16_t*)fk_key = fk_id;
        status = btree_find_row(pager, 3, fk_key, sizeof(fk_key), table_value, &table_value_size);
        if (status == PSQL_OK) {
            uint16_t from_table = *(uint16_t*)table_value;
            uint16_t to_table = *(uint16_t*)(table_value + sizeof(uint16_t) * 2);
            *(uint16_t*)value = fk_id;
            status = btree_insert_value(pager, fk_from_table_index_root, (uint8_t*)&from_table, sizeof(from_table), value, sizeof(value));
            if (status != PSQL_OK) return status;
            status = btree_insert_value(pager, fk_to_table_index_root, (uint8_t*)&to_table, sizeof(to_table), value, sizeof(value));
            if (status != PSQL_OK) return status;
        }
    }

    return PSQL_OK;
}

// Table catalog operations
PSqlStatus catalog_add_table(const char* table_name, uint16_t root_page, uint8_t index_type,
                            uint8_t flags, uint16_t* out_table_id) {
    if (!table_name || !out_table_id) return PSQL_MISUSE;

    Pager* pager = NULL; // TODO: Initialize properly
    uint16_t table_id = 0;
    uint8_t key[sizeof(uint16_t)];
    uint8_t value[256];
    uint16_t value_size = 0;

    for (uint16_t id = 4; id < MAX_PAGES; id++) {
        *(uint16_t*)key = id;
        uint8_t dummy[256];
        uint16_t dummy_size;
        if (btree_find_row(pager, 1, key, sizeof(key), dummy, &dummy_size) == PSQL_NOTFOUND) {
            table_id = id;
            break;
        }
    }
    if (!table_id) return PSQL_FULL;

    *(uint16_t*)key = table_id;
    uint16_t name_len = strlen(table_name) + 1;
    *(uint16_t*)(value + value_size) = name_len;
    value_size += sizeof(uint16_t);
    memcpy(value + value_size, table_name, name_len);
    value_size += name_len;
    *(uint16_t*)(value + value_size) = root_page;
    value_size += sizeof(uint16_t);
    value[value_size++] = index_type;
    value[value_size++] = flags;
    *(uint16_t*)(value + value_size) = 1;
    value_size += sizeof(uint16_t);
    *(uint32_t*)(value + value_size) = get_current_timestamp();
    value_size += sizeof(uint32_t);
    *(uint32_t*)(value + value_size) = get_current_timestamp();
    value_size += sizeof(uint32_t);

    PSqlStatus status = btree_insert_value(pager, 1, key, sizeof(key), value, value_size);
    if (status != PSQL_OK) return status;

    if (table_name_index_root) {
        uint8_t sec_key[256];
        uint16_t sec_key_size = name_len;
        memcpy(sec_key, table_name, name_len);
        *(uint16_t*)value = table_id;
        status = btree_insert_value(pager, table_name_index_root, sec_key, sec_key_size, value, sizeof(uint16_t));
        if (status != PSQL_OK) return status;
    }

    *out_table_id = table_id;
    return PSQL_OK;
}

PSqlStatus catalog_get_table_by_name(const char* table_name, uint16_t* out_table_id, uint16_t* out_root_page) {
    if (!table_name || !out_table_id || !out_root_page) return PSQL_MISUSE;

    Pager* pager = NULL; // TODO: Initialize properly
    if (!table_name_index_root) return PSQL_NOTFOUND;

    uint8_t key[256];
    uint16_t key_size = strlen(table_name) + 1;
    memcpy(key, table_name, key_size);
    uint8_t value[sizeof(uint16_t)];
    uint16_t value_size;

    PSqlStatus status = btree_find_row(pager, table_name_index_root, key, key_size, value, &value_size);
    if (status != PSQL_OK) return status;

    uint16_t table_id = *(uint16_t*)value;
    uint8_t table_value[256];
    uint16_t table_value_size;

    status = btree_find_row(pager, 1, (uint8_t*)&table_id, sizeof(table_id), table_value, &table_value_size);
    if (status != PSQL_OK) return status;

    *out_table_id = table_id;
    *out_root_page = *(uint16_t*)(table_value + sizeof(uint16_t) + *(uint16_t*)table_value);
    return PSQL_OK;
}

PSqlStatus catalog_get_table_by_id(uint16_t table_id, char** out_table_name, uint16_t* out_root_page) {
    if (!out_table_name || !out_root_page) return PSQL_MISUSE;

    Pager* pager = NULL; // TODO: Initialize properly
    uint8_t key[sizeof(uint16_t)];
    uint8_t value[256];
    uint16_t value_size;

    *(uint16_t*)key = table_id;
    PSqlStatus status = btree_find_row(pager, 1, key, sizeof(key), value, &value_size);
    if (status != PSQL_OK) return status;

    uint16_t name_len = *(uint16_t*)value;
    *out_table_name = malloc(name_len);
    if (!*out_table_name) return PSQL_NOMEM;
    memcpy(*out_table_name, value + sizeof(uint16_t), name_len);
    *out_root_page = *(uint16_t*)(value + sizeof(uint16_t) + name_len);
    return PSQL_OK;
}

PSqlStatus catalog_delete_table(uint16_t table_id) {
    Pager* pager = NULL; // TODO: Initialize properly
    uint8_t key[sizeof(uint16_t)];
    *(uint16_t*)key = table_id;

    DBPage* page = pager_get_page(pager, 1);
    if (!page) return PSQL_IOERR;
    PSqlStatus status = btree_delete(pager, 1, key, sizeof(key));
    if (status != PSQL_OK) return status;
    vacuum_page(page);
    pager_write_page(pager, page);

    if (table_name_index_root) {
        uint8_t table_value[256];
        uint16_t table_value_size;
        status = btree_find_row(pager, 1, key, sizeof(key), table_value, &table_value_size);
        if (status == PSQL_OK) {
            uint16_t name_len = *(uint16_t*)table_value;
            uint8_t sec_key[256];
            memcpy(sec_key, table_value + sizeof(uint16_t), name_len);
            page = pager_get_page(pager, table_name_index_root);
            if (!page) return PSQL_IOERR;
            status = btree_delete(pager, table_name_index_root, sec_key, name_len);
            if (status == PSQL_OK) {
                vacuum_page(page);
                pager_write_page(pager, page);
            }
        }
    }

    uint8_t column_key[sizeof(uint16_t) * 2];
    *(uint16_t*)column_key = table_id;
    for (uint16_t col_idx = 0; col_idx < MAX_PAGES; col_idx++) {
        *(uint16_t*)(column_key + sizeof(uint16_t)) = col_idx;
        page = pager_get_page(pager, 2);
        if (!page) return PSQL_IOERR;
        status = btree_delete(pager, 2, column_key, sizeof(column_key));
        if (status == PSQL_OK) {
            vacuum_page(page);
            pager_write_page(pager, page);
        } else if (status == PSQL_NOTFOUND) {
            break;
        } else {
            return status;
        }
    }

    uint8_t fk_key[sizeof(uint16_t)];
    for (uint16_t fk_id = 1; fk_id < MAX_PAGES; fk_id++) {
        *(uint16_t*)fk_key = fk_id;
        uint8_t fk_value[256];
        uint16_t fk_value_size;
        status = btree_find_row(pager, 3, fk_key, sizeof(fk_key), fk_value, &fk_value_size);
        if (status == PSQL_OK) {
            uint16_t from_table = *(uint16_t*)fk_value;
            uint16_t to_table = *(uint16_t*)(fk_value + sizeof(uint16_t) * 2);
            if (from_table == table_id || to_table == table_id) {
                page = pager_get_page(pager, 3);
                if (!page) return PSQL_IOERR;
                status = btree_delete(pager, 3, fk_key, sizeof(fk_key));
                if (status == PSQL_OK) {
                    vacuum_page(page);
                    pager_write_page(pager, page);
                }
            }
        } else if (status == PSQL_NOTFOUND) {
            break;
        } else {
            return status;
        }
    }

    return PSQL_OK;
}

// Column catalog operations
PSqlStatus catalog_add_column(uint16_t table_id, uint16_t column_index, const char* column_name,
                             uint8_t column_type, uint8_t nullable, uint8_t is_pk) {
    if (!column_name) return PSQL_MISUSE;

    Pager* pager = NULL; // TODO: Initialize properly
    uint8_t key[sizeof(uint16_t) * 2];
    uint8_t value[256];
    uint16_t value_size = 0;

    *(uint16_t*)key = table_id;
    *(uint16_t*)(key + sizeof(uint16_t)) = column_index;
    uint16_t name_len = strlen(column_name) + 1;
    *(uint16_t*)(value + value_size) = name_len;
    value_size += sizeof(uint16_t);
    memcpy(value + value_size, column_name, name_len);
    value_size += name_len;
    value[value_size++] = column_type;
    value[value_size++] = nullable;
    value[value_size++] = is_pk;

    PSqlStatus status = btree_insert_value(pager, 2, key, sizeof(key), value, value_size);
    if (status != PSQL_OK) return status;

    if (column_name_index_root) {
        uint8_t sec_key[256];
        uint16_t sec_key_size = name_len;
        memcpy(sec_key, column_name, name_len);
        memcpy(value, key, sizeof(key));
        status = btree_insert_value(pager, column_name_index_root, sec_key, sec_key_size, value, sizeof(key));
    }

    return status;
}

PSqlStatus catalog_get_column_by_name(uint16_t table_id, const char* column_name, uint16_t* out_column_index,
                                     uint8_t* out_column_type, uint8_t* out_nullable, uint8_t* out_is_pk) {
    if (!column_name || !out_column_index || !out_column_type || !out_nullable || !out_is_pk)
        return PSQL_MISUSE;

    Pager* pager = NULL; // TODO: Initialize properly
    if (!column_name_index_root) return PSQL_NOTFOUND;

    uint8_t key[256];
    uint16_t key_size = strlen(column_name) + 1;
    memcpy(key, column_name, key_size);
    uint8_t value[sizeof(uint16_t) * 2];
    uint16_t value_size;

    PSqlStatus status = btree_find_row(pager, column_name_index_root, key, key_size, value, &value_size);
    if (status != PSQL_OK) return status;

    uint16_t found_table_id = *(uint16_t*)value;
    if (found_table_id != table_id) return PSQL_NOTFOUND;

    *out_column_index = *(uint16_t*)(value + sizeof(uint16_t));
    uint8_t column_value[256];
    status = btree_find_row(pager, 2, value, value_size, column_value, &value_size);
    if (status != PSQL_OK) return status;

    uint16_t name_len = *(uint16_t*)column_value;
    *out_column_type = column_value[sizeof(uint16_t) + name_len];
    *out_nullable = column_value[sizeof(uint16_t) + name_len + 1];
    *out_is_pk = column_value[sizeof(uint16_t) + name_len + 2];
    return PSQL_OK;
}

PSqlStatus catalog_get_column_by_index(uint16_t table_id, uint16_t column_index, char** out_column_name,
                                      uint8_t* out_column_type, uint8_t* out_nullable, uint8_t* out_is_pk) {
    if (!out_column_name || !out_column_type || !out_nullable || !out_is_pk)
        return PSQL_MISUSE;

    Pager* pager = NULL; // TODO: Initialize properly
    uint8_t key[sizeof(uint16_t) * 2];
    uint8_t value[256];
    uint16_t value_size;

    *(uint16_t*)key = table_id;
    *(uint16_t*)(key + sizeof(uint16_t)) = column_index;
    PSqlStatus status = btree_find_row(pager, 2, key, sizeof(key), value, &value_size);
    if (status != PSQL_OK) return status;

    uint16_t name_len = *(uint16_t*)value;
    *out_column_name = malloc(name_len);
    if (!*out_column_name) return PSQL_NOMEM;
    memcpy(*out_column_name, value + sizeof(uint16_t), name_len);
    *out_column_type = value[sizeof(uint16_t) + name_len];
    *out_nullable = value[sizeof(uint16_t) + name_len + 1];
    *out_is_pk = value[sizeof(uint16_t) + name_len + 2];
    return PSQL_OK;
}

PSqlStatus catalog_get_all_columns(uint16_t table_id, uint16_t* out_column_count) {
    if (!out_column_count) return PSQL_MISUSE;

    Pager* pager = NULL; // TODO: Initialize properly
    uint16_t count = 0;

    BTreeIterator* iter = btree_iterator_create(pager, 2);
    if (!iter) return PSQL_NOMEM;

    uint16_t data_page_id;
    uint8_t data_slot_id;
    while (btree_iterator_next(iter, &data_page_id, &data_slot_id)) {
        IndexSlotData slot;
        read_index_slot(pager, data_page_id, data_slot_id, &slot);
        uint16_t slot_table_id = *(uint16_t*)slot.key;
        if (slot_table_id == table_id) count++;
    }

    btree_iterator_destroy(iter);
    *out_column_count = count;
    return PSQL_OK;
}
