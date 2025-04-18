# Database file disk file format

The pages at the back of the database file can grow dynamically in count.

A page is determined by the OS kernel, but is normally 4096 bytes or 8192 bytes. We keep within this page limit as it minimizes I/O (1 I/O operation per page retrieval) since the page is the smallest unit that can be retrieved from disk. 

Breaking the page alignment will cause unnecessary I/O operations. Hence all pages are intentionally padded to 4096 bytes even if its not used. A page has to be strictly aligned.

```
[ Page 0 ]
  └─ Magic number, Page size, Version
  └─ Roots of special system catalogs tables (page 1-4 )

[ Page 1 ] - track each table, its names and its root page
  └─ Table catalog (B+ Tree Root page): table_id, table name, root page, schema ID, flags (hidden/system)

[ Page 2 ] - track columns for each table
  └─ Column catalog (B+ Tree Root Page): table_id, column_name, type, order

[ Page 3 ] - track FK relations
  └─ Foreign Key catalog (B+ Tree Root Page): table_id, column_name, type, order
  
[ Page 4..X ]
  └─ Table Data Pages (slotted page implementation) - stores the actual data
  └─ Index Pages (B+ trees) - Internal nodes, and leaf nodes which point to data (or other nodes)
  └─ Overflow Pages - for big INTEGER and TEXT that do not fit
  └─ Free Pages - holes in pages created after deletion of pages (a bitmap/radix tree in the Page 0 header keeps track of what pages are free for reuse - to fill in holes)
```

# Header of database
Stores metadata about the database. These are the choosen things to be stored inside. It is stored in fixed position of Page 0 - almost like a boot sector for the database.

| Field                   | Purpose                                                                           |
| ----------------------- | --------------------------------------------------------------------------------- |
| Magic number            | File format/version check (`SQLShite` in ASCII)                                                         |
| Page size               | 4KB or 8KB, etc - used as debug info to know how to load the files later in pager |
| DB version              | Schema versioning or migrations                                                   |
| Root pointers           | To special system catalog tables/indexes                                          |
| Number of tables        | For iterating                                                                     |
| Free page list | Space management for reusing freed pages |
| Transaction state       | For WAL / crash recovery                                                          |
| Optional flags          | E.g., read-only mode, corruption checks, compression                              |
| Checksum                | Data corruption and recovery                                                      |

# Special Catalog Tables 

These are tables with their own schema and can be queried like SQL tables. However, they are implemented in C, since otherwise, we have a chicken or egg problem, since these catalog tables do store metadata (including about themselves).

Each of these tables are to fit in a 4KB page as well. They themselves are B+ Tree Root nodes, which link to other data pages at the back of the database file.

## Page 1: Table Catalog

## Page 2: Column Catalog

## Page 3: Foreign Key Catalog

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

