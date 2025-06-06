# Pager subsystem
Pager subsystem handles a bulk of the I/O handling and file operations on the database. Given how much of the database is always meant to be disk-backed, this contains a bulk of the code on the low-level specifics of dealing with the serialization of the file.

Pager handles two different dynamically growing files:
1) Main `.pseql` database file - contains the data.
2) Journal `.pseql-journal` file - contains a copy of the original pages of the database whenever TCL instructions like `BEGIN TRANSACTION`, `COMMIT` and `ROLLBACK` is handled. This allows rollback to transaction ID.

File layout in this subsystem is that each folder contains the implementation of one `page` type and their necessary page related functions.

# Database initialization flow
Database is opened by the virtual machine - it calls functions from the pager subsystem. PreSeQL like SQLite does not have a `CREATE DATABASE` command, but rather its via `psql_open()` which returns a handle to the database.

When a new database is opened,
1) If file exists and is_valid (via CRC32 checksum): `mmap()` into a specific memory location in virtual memory. This gives us a `(void *)` ptr to the "start" of the file. Can immediately do operations on DB.
2) If the file is empty: Then follow following steps:
3) Create Page 0 Base Header - This is one page only
4) Bootstrap and Create Page 1-3 Special Catalog Tables (Table, Column, Foreign Key) - These are B+ Tree Indexes with their own hardcoded data sorted by `id` as primary key
5) Create secondary indexes for the speical catalog tables 
    - Table Catalog: `table_name` - for name resolution
    - Column Catalog: `column_name` - for reolving columns IDs by name
    - Foreign Key Catalog: 
        - `from_table` - When inserting/updating a row in a referencing table, you check if the FK constraint is satisfied.
        - `to_table` - When deleting/updating a referenced row, you look up what child tables are affected.
6) Hand over control to VM engine


If Journal is implemented, on database opening:
1) `mmap()` file - Create a blank journal file with only the header metadata. This serves as a blank canvas for any transactions later on.
2) If transaction happens via `BEGIN TRANSACTIONS`: Create the necessary copies of the pages into Journal. This is handled by VM.
3) If transaction number returned by the VM is not continguous with the highest transaction number seen by the journal - this invalidate the current journalentries. Journal is cleared and reinitialized (either immediately or batched).

# Database - Disk and in-memory representations differences

When dealing with database files, it is important to distinguish between on-disk and in-memory representations.

For the most part, due to `mmap()` the on-disk representations are exactly the same as the in-memory mapped version of the file.

When dealing with disk representations of a database, there are some good rules to follow in the code:
1) it is important to ensure that all data are statically sized (e.g arrays have a fixed upper bound on sizes set in DEFINE). This is used a lot when defining header sizes.
2) It is also important to be careful to sync the changes to the disk using `fsync()` and `msync()`, failure to do so might result in unsaved changes to the file.
3) Be conscious of page alignment which is determined by the OS. It is generally 4096 bytes. This is as not aligning to 4KB will result in unnecessary I/O even if it exceeds by 1 byte. Hence, a lot of parts of the code, intentionally pad the headers until they fit in 4KB even if it appears to be wasted space. This is also limits how much stuff we can store in the header in practice.

Due to the 3rd limitation, header metadata in disk usually has to try to keep within the 4KB limit. Even if it means that the representation is not as optimal for searching or querying. Hence, usually when the VM boots up, it loads in the compact disk format (e.g a free page list) and puts it into memory where it can be as large as it wants without I/O penalty (e.g a Radix Tree which might be 10-20MB in practice, but up to a few GB in worst case).

# Database file disk file format

The pages at the back of the database file can grow dynamically in count.

A page is determined by the OS kernel, but is normally 4096 bytes or 8192 bytes. We keep within this page limit as it minimizes I/O (1 I/O operation per page retrieval) since the page is the smallest unit that can be retrieved from disk. 

Breaking the page alignment will cause unnecessary I/O operations, as well as write amplification (possibly leading to unnecessary disk wear). Hence all pages are intentionally padded to 4096 bytes even if its not used. A page has to be strictly aligned.

```
[ Page 0 ] Header Metadata - Stores magic number, versioning and roots of various tables - acts as a boot sector

[ Page 1 ] Table Catalog (B+ Tree Root Page) - track each Index Page for each table, its names and its root page and Index type

[ Page 2 ] Column Catalog (B+ Tree Root Page) - track column schema for each table

[ Page 3 ] Foreign Key Catalog - track Foreign Key Constraints between table to table
  
[ Page 4..X ]
  └─ Table Data Pages (slotted page implementation) - stores the actual data
  └─ Index Pages (B+ tree nodes) - Internal nodes, and leaf nodes which point to data (or other nodes)
  └─ Overflow Pages - for big INTEGER and TEXT that do not fit within a page
  └─ Free Pages - Pages marked as freed in database file, these are holes created after deletion of pages (a bitmap in the Page 0 header on disk keeps track of what pages are free for reuse - to fill in holes)
```

## Page 0: Header of database
Stores metadata about the database. These are the choosen things to be stored inside. It is stored in fixed position of Page 0 - almost like a boot sector for the database.

| Field               | Type     | Purpose |
|---------------------|----------|-------------|
| `magic`             | `CHAR[8]`| Fixed file signature — `"SQLSHITE"` (ASCII, 8 bytes). |
| `page_size`         | `UINT16` | Page size in bytes (usually 4096 or 8192) - used as debug info to know how to load the file in Pager |
| `db_version`        | `UINT16` | Global schema version (for migrations or format changes). |
| `root_table_catalog`| `UINT16` | Root page number of the system table catalog |
| `root_index_catalog`| `UINT16` | Root page number of the system index catalog |
| `root_fk_catalog`   | `UINT16` | Root page number of the system foreign key catalog |
| `free_page_list`    | `UINT16[256]` | Fixed-size inline free page list (up to 256 entries). |
| `transaction_state` | `UINT16` | For rollback journal / crash recovery tracking. |
| `flags`             | `UINT16` | Optional database-level flags (e.g read-only, corruption, compression). |
| `checksum`          | `UINT32` | CRC-32 checksum of header (excluding this field) |

## Special Catalog Tables (Page 1-3)

These are tables with their own schema and can be queried like SQL tables. However, they are implemented in C, since otherwise, we have a chicken or egg problem, since these catalog tables do store metadata (including about themselves). 

On the VM start, these pages are immediately initialized with values as well. The tables are first bootstrapped via their primary key - `id` column. Then the secondary keys are indexed to speed up the operations.

Each of these tables are to fit in a 4KB page as well. They themselves are B+ Tree Root nodes, which link to other data pages at the back of the database file.

Below are their schema (yes as an PreSeQL table).

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

# Page types

## Index Page

An Index Page used for B+ Tree Nodes. 1 B+ Tree Node is in an Index Page.

There are 3 types of B+ Tree Nodes:
1) Root Node
2) Internal Nodes - Does not store values, only does routing to leaf nodes
3) Leaf Nodes - Points to the data page where the data lives.

## Data Page
For data pages, it is implemented by means of a slotted page system.

In a slotted page system, a slot directory after the page header will maintain a set of offset pointers to actual records. This slot directory grows downwards. Meanwhile, record data grows upwards from the back of the page. 

This allows them to grow into the middle free space. In practice, this looks like this:

```
┌──────────────────────────────────────────────┐
│ Page Header                                  │
│ - Page ID                                    │
│ - Number of records                          │
│ - Pointer to free space                      │
└──────────────────────────────────────────────┘
│ Slot Directory (grows downward)              │
│ [0] → offset 300                             │
│ [1] → offset 270                             │
│ [2] → offset 240                             │
│ [3] → offset 210                             │
└──────────────────────────────────────────────┘
│ Free space (in middle)                       │
└──────────────────────────────────────────────┘
│ Record Data (grows upward)                   │
│ Offset 210: id=4, name="Adam"                │
│ Offset 240: id=3, name="Charlie"             │
│ Offset 270: id=2, name="Bob"                 │
│ Offset 300: id=1, name="Alice"               │
└──────────────────────────────────────────────┘
```

Slots are where rows of data live. In the above example, we only have 2 rows per slot based on the schema:
- (INTEGER) id
- (TEXT) name

A slot might contain:
- id=4, name="Adam"
- id=1, name="Alice"

When a certain row is desired, then it will be linearly searched from the data page.

> This process can be thought of as - Index Page (B+ Tree) gives approx data page that has the rows matching the Primary key condition. Then Data page is searched linearly for the exact row values.

Record data can also end up being a pointer to an overflow page in the cases where its too big to be stored within the data page itself, we prioritise storing the smaller metadata (e.g page pointers and chunk pointers), rather than the full data (e.g large `TEXT` or large `INTEGER`)

## Overflow Page
Allows storing arbitrary length INTEGER and TEXT, or any that exceeds the page size of 4096 bytes (including any overhead from headers). 

This is in essence a linked list but implemented as pages per node. If the data manages to overflow multiple overflow pages, we can follow a next pointer until we get all the data.

To prevent wasting of half filled overflow pages, pages have a chunk system, which allows for different overflows from different data pages to fill up the same overflow page.

# Some other design choices

## Tracking free pages and slots

The choice for how to do this is largely due to:
1) Page size limits - All metadata has to fit in 4KB. In terms of size, Radix Tree > Bitmap > Free Space List. 
2) Whether the free page/slots are dynamic in count (pages are dynamic up to 65535) or fixed in count (slots per pages are fixed to 255 default)
3) What is the max possible number of free elements that are being tracked. 
    - Slots per page is small at 255 max - can be linearly searched
    - Pages is large at 65535 max - need a better way to search
4) Is this being tracked on disk or in memory. Ironically, disk is more limited due to the page size limits of 4KB, while memory can exceed 4KB. Hence the disk representation is always going to favor the smallest space complexity, while the memory representation is going to favor the lowest time complexity (without caring about space).


Hence for tracking free pages:
- Disk: Free Space List with a max size of 255 - sparse
- In memory: Radix Tree
- These are converted between forms

For tracking free slots per page:
- Disk: Bitmap - Bitmap is medium sized. 1 bit per free slot - so 255 bits for max of 255 slots per page
- In memory: Bitmap - Linear scan does not take too long with so little entries

## Primary and secondary indexes

While building a table, if the Primary key is not specified, an implicit ID field is added to sort the data by. This is not optimal for retrieving data fast as it involves a full walk of the B+ Tree.

It is recommended to always specify a PRIMARY KEY. 
- For INTEGER types as PRIMARY KEY, this is sorted by its value 
- For TEXT types as PRIMARY KEY, this is sorted by lexicographic order (`strcmp()`)

There are ways to make secondary indexes:
- Built explicitly via `CREATE INDEX idx_users_email on users(email);`
- Built via `UNIQUE` keyword during `CREATE TABLE` : e.g ` CREATE TABLE Persons ( PersonID INTEGER, Email TEXT UNIQUE);`

Making an secondary index basically initializes a whole new B+ Tree on the disk which costs more memory space, but buys back time in the long run. However, interally, as data is added, then its effectively inserted into all B-Tree indexes that are affected by the added data (its more costly on time).
- Internally building a secondary index off the primary index will cause it to point to the same data pages. The data page is not marked as freed until all indexes stop referencing it (tracked by a reference counter in the header)


## Why is Journal separated?

Journal file is made separately as having two dynamically growing regions in one db file is too messy. Journal file entries can be invalidated (if transaction numbers are distinuous), and it would be easier to just make it separate to clear all journal entries at one go.

Main db file already has to store pages for the indexes, data, and overflow pages which can grow. 

If you wanna have another part that grows, either you:
1) preallocate a large amount of empty space then put the journal behind 
    - this is okay for filesystems with sparse file support, but not APFS+ where sparse file support is not available
2) You can also truncate dynamic regions if they are not used 
    - e.g if we specified to have 65535 journal entries - we can just shrink the unused section 
    - This is messy as you need to track more things until `COMMIT` is done
3) Block compress the journal pages on write - such as using ZSTD compression
    - Downside is speed to compress and decompress
4) Simply use a separate journal file 
    - easier to manage
    - Separation of concerns
    - Append and track as much as I want
