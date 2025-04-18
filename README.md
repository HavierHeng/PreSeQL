# PreSeQL - SQLite but way worse

PreSeQL (`.pseql`) is an embedded database based on a B+ Tree, inspired by SQLite. It has pretty poor support for the full SQL language spec (not even SQL89) and we'll proudly admit so. Written in C so you know its good.

Simplifications are made for the interest of time. Such as supporting way less types in the database (`TEXT` and `INT` type), a smaller subset of the SQL language of our choosing, and only supporting UNIX and UNIX-like platforms. 

Relations between tables are also optional features as that requires `JOIN` via a Join table with Primary and Foreign keys.

## Architecture

In summary the project consists of the following parts:
- (Usage) ClI Tool/Program using PreSeQL 
    - Allowing the writing and execution of SQL commands from stdin and reading from txt files 
    - Embeds the PreSeQL database by statically linking
- (Frontend) Header and object files for static linking in programs
    - Defines key objects like database objects and database statements (in terms of the custom bytecode) to be executed as well as functions to step through operations
- (Frontend) Tokenizer
    - Lexes SQL statements to syntax tree
- (Frontend) Parser
    - Syntax Tree to custom Opcodes defining simpler operations to be executed by the virtual machine
    - User facing SQL statements (e.g for Data Query and Data Manipulation) are fundamentally composite statements comprised of multiple small operations
- (Backend) Virtual Machine
    - Reads and executes bytecode
    - Understands the custom OPCodes
    - A state machine that interacts with the actual backing data structure
- (Backend) Data Structure - B+ Tree
    - Where entries are actually stored for quick access
- (Backend) Pager/Serializer
    - Writes to disk in pages in a format of our choosing
    - In a more complex implementation, disk-backed and page management for data that may be stored either on disk or in memory
- (Backend) Journal
    - Logs original data of pages in a separate journal file on a transaction entry.
    - Allows for rollback by copying original pages back over the modified dirty pages.
- (Backend) OS Interface
    - For the reason of simplicity, only POSIX and UNIX-like OS abstractions and syscalls are used, i.e Linux and MacOS, Windows with its Win32 interface is ignored
    - Assume no file system issues given locking may differ from in different platform filesystem

### Optional Architecture
For some quality of life features that might be explored: 
- (Frontend) Bind operations
    - Replace literals/variables in SQL statements with parameters - like a string format
- (Frontend/Backend) `JOIN` for relationships between tables
- (Backend) Write-Ahead Logging
    - Journal that records transactions that have been committed but not yet applied to database
    - This might require a main recording thread and a secondary processing thread - therefore more complex
- (Backend) Virtual Tables
    - Fake tables that "store" metadata but are not actually directly stored in the database, but rather either generated or obtained and exposed as an SQL table
    - Namely debugging tables:
        - `schema` - schema for databases for debugging purposes
        - `logs` - logging statements executed and return values
        - `btreeinfo` - B-tree status, such as depth, pages and entries
        - `dbpage` - Page number in which each entry is stored in
        - `memset` - Database status such as memory use
- (Backend) Universal OS interface 
    - Bridging OS specific behaviours

## Support for SQLite?

Of course not. Nothing is compatible, from the client down to the OPCodes and the file format. We implement only a subset of the project in any matter we deem fit.


## Why not just use a flat 2D table - e.g CSV?
There are several problems of a flat table backend:
1) Page Alignment for disk is not respected - extra I/O operations and write amplification; kernel can only modify by pages as the smallest disk unit
    - B+ Tree: All nodes in a B+ Tree are page aligned, therefore, accessing a node (which contains rows of data) is only one I/O operation. Inserting and deleting a record only affects at most 1 leaf data page at a time on average (if no rebalancing or splitting occurs).
    - Flat table: Read the whole file into RAM, then do slow repeated linear scans. Once finished, copy back into RAM. This causes a lot of write amplification on the physical disk, as having a huge sequential data array means a lot of pages will be affected at once.
2) Efficiency issues in lookup 
    - B+ Tree: always a logarithmic time lookup O(logn), as its a tree, its only affected by its height. Linear scans are even faster due to a right sibling pointer between links, which lets us pull off range scans quickly.
    - Flat table: Full scan everytime. Cannot jump to right record without an index (e.g hash table). 
3) Efficiency issues in insertion
    - B+ Tree: Acts a bit like a linked list, it doesn't matter which page number you put nodes since the B+ Tree points to each other anyways. This insertion always results in a sorted order.
    - Flat table: If any data is inserted, this would mean shifting all rows back to slot in data. 
4) Extensibility reasons - Secondary indexes (and even non B+ Tree indexes)
    - B+ Tree: Build another tree for secondary index based on the column you want to sort by. Use the same data, so you can just point to the page where the data resides. Say if I even want to add a hash-table index, I can since the B+ Tree Nodes are just pages with metadata, these metadata can work across Index types or with minor modifications work with them.
    - Flat table: Nah, no easy means of fast secondary access other than a hash table

