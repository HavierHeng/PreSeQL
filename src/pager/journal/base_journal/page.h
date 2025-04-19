/*
[ Page 0 ] Header Metadata - Stores magic number, versioning and roots of various tables - acts as a boot sector


| Field                   | Purpose                                 |
| ----------------------- | --------------------------------------- |
| Magic number            | File format/version check               |
| Page size               | 4KB or 8KB, etc                         |
| DB version              | Schema versioning or migrations         |
| Root pointers           | To tables/indexes (e.g., page numbers)  |
| Number of tables        | For iterating                           |
| Free page list          | Space management                        |
| highest_page            | Highest page allocated                  |
| Transaction state       | For WAL / crash recovery                |
| Optional flags          | E.g., read-only mode, corruption checks |
| Checksum                | Data corruption and recovery (CRC-32)   |
*/

// TODO: Build this for page 0 
