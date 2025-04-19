#ifndef PRESEQL_PAGER_CONSTANTS_H
#define PRESEQL_PAGER_CONSTANTS_H

#include "config.h"

// Define constants for the pager subsystem
#define JOURNAL_HEADER_SIZE 32  // Size of journal header in bytes

// Page status flags
#define PAGE_DIRTY 0x01
#define PAGE_FREE  0x02

// Database flags
#define DB_READONLY 0x01
#define DB_JOURNAL_ENABLED 0x02
#define DB_CORRUPT 0x04

#endif /* PRESEQL_PAGER_CONSTANTS_H */
