# PreSeQL - SQLite but way worse

PreSeQL (`.pseql`) is an embedded database based on a B+ Tree, inspired by SQLite. It has pretty poor support for the full SQL language spec (not even SQL89) and we'll proudly admit so. Written in C so you know its good.

Simplifications are made for the interest of time. Such as supporting way less types in the database (minimum `TEXT` type), a smaller subset of the SQL language of our choosing, and only supporting UNIX and UNIX-like platforms. 

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
    - Writes to disk from memory in a format of our choosing
    - In the simplest implementation, writes to disk only on commit/transaction end
    - In a more complex implementation, disk-backed and page management for data that may be stored either on disk or in memory
- (Backend) OS Interface
    - For the reason of simplicity, only POSIX and UNIX-like OS abstractions and syscalls are used, i.e Linux and MacOS, Windows with its Win32 interface is ignored
    - Assume no file system issues given locking may differ from in different platform filesystem

### Optional Architecture
For some quality of life features that might be explored: 
- (Frontend) Bind operations
    - Replace literals/variables in SQL statements with parameters - like a string format
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


