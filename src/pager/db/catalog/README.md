# Catalog tables

## Description 
Catalog pages are special pages implemented as real B+ Tree backed Index tables.

The only difference is that their initialization is hardcoded into the database engine - such that whenever it boots it calls all the 3 initialization methods to immediately populate the database with indexes based on their primary `id` keys. 

After this bootstrap initialization, secondary keys for the catalog tables are also created to speed up certain types of searches. 

All other non-system tables relying on these catalog indexes can then be created.


## Schemas from Pager README.md for Reference

### Page 1: Table Catalog
Indexes are built for the keys:
- Primary: `table_id` - for FK searches
- Secondary: `table_name` - for name resolution

| Column         | Type  | Description |
|----------------|-------|-------------|
| `table_id`     | `INT` | Unique internal ID for the table. |
| `table_name`   | `TEXT` | Optional table name (can be `NULL` for internal/anonymous tables). |
| `root_page`    | `INT` | Page number of the B+Tree or Hash root node. |
| `index_type`   | `INT` | Type of structure: `0` = B+ Tree, `1` = Hash Table. |
| `flags`        | `INT` | Bitmask representing table options (see below). |
| `schema_version` | `INT` | Schema version specific to this table. |
| `created_on`   | `INT` | Unix timestamp (seconds since epoch) of table creation. |
| `last_modified`| `INT` | Unix timestamp of the last schema change. |


`flags` bitmask reference (normal table with nothing special is `0x00`:
| Bit Position | Hex Value | Flag Name           | Description | Example Use |
|--------------|-----------|---------------------|-------------|-------------|
| 0            | `0x01`    | `HIDDEN`            | Table should not be listed to clients (internal use only). | Internal system table |
| 1            | `0x02`    | `SYSTEM`            | Denotes a system-defined table (e.g., catalog or journal). | `__table_catalog__` |
| 2            | `0x04`    | `TEMPORARY`         | Table is temporary and will be dropped when session ends. | Session-local scratch table |
| 3            | `0x08`    | `WITHOUT_ROWID`     | Table doesn’t use implicit rowid; uses PRIMARY KEY instead. | Optimized lookup table |
| 4            | `0x10`    | `VIRTUAL`           | Table is backed by a virtual mechanism, not physical pages. | FTS table, JSON view |
| 5            | `0x20`    | `AUTOINCREMENT`     | Table contains an autoincrement column. | `id INTEGER AUTOINCREMENT` |


Example of mixing bitmasks:
| Meaning                             | Flags Value | Explanation |
|-------------------------------------|-------------|-------------|
| Hidden and system table             | `0x03`      | `HIDDEN | SYSTEM` |
| Temporary autoincrement table       | `0x24`      | `TEMPORARY | AUTOINCREMENT` |
| Regular table with WITHOUT_ROWID    | `0x08`      | Just `WITHOUT_ROWID` |
| Fully featured system table         | `0x3B`      | All flags except `VIRTUAL` |

### Page 2: Column Catalog
Indexes are built for the keys:
- Primary: `table_id` - for listing columns of table
- Secondary: `column_name` - for reolving columns IDs by name

Note: `table_id` here is a foreign key to the Table catalog - so it should be stored as the first entry in the Foreign Key catalog.

| Column      | Type   | Purpose                          |
| ----------- | ------ | -------------------------------- |
| `table_id`  | `INT`  | Foreign key to `table_catalog`   |
| `column_index` | `INT`  | Position in row layout (0-based) |
| `column_name`  | `TEXT` | Name of the column               |
| `column_type`  | `INT` | 0 - "NULL", 1 - "INT", 2 - "TEXT"      |
| `nullable`  | `INT`  | 0 - NULL not allowed, 1 - NULL allowed |
| `is_pk`     | `INT`  | 0 - is Foreign Key, 1 is Primary Key       |

### Page 3: Foreign Key Catalog
Indexes are built for the keys:
- Primary: `fk_id` - For bootstrapping the catalog
- Secondary: `from_table` - When inserting/updating a row in a referencing table, you check if the FK constraint is satisfied.
- Secondary: `to_table` - When deleting/updating a referenced row, you look up what child tables are affected.

| Column               | Type   | Purpose                                                                                                                                             |
| -------------------- | ------ | --------------------------------------------------------------------------------------------------------------------------------------------------- |
| `fk_id`              | `INT`  | Unique ID for the foreign key relationship                                                                                                          |
| `from_table`  | `INT`  | Referencing table                                                                      |
| `from_column` | `INT` | Position of the column in the referencing table|
| `to_table`   | `INT`  | Referenced table |
| `to_column`  | `INT` | Postion of the column in the referenced table |
| `on_delete`   | `INT` | Behaviour on deletion (0-CASCADE, 1-RESTRICT, 2-SET NULL)|
| `on_update`   | `INT` | Behaviour on update (0-CASCADE, 1-RESTRICT, 2-SET NULL)|


Actions are mapped as such:
- (0) CASCADE - Propagate change (DELETE or UPDATE) to child rows,- dependent records to follow the parent
- (1) RESTRICT - Block if child rows exists - For strict integrity, no orphaned records or accidental deletes
- (2) SET NULL - Set referencing columns in child rows to NULL when parent row is deleted or updated - preserves child record as NULL but parent is gone

