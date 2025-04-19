/* 
 * Collate page structures and headers format 
 * */

#ifndef PRESEQL_PAGER_DB_FORMAT_H
#define PRESEQL_PAGER_DB_FORMAT_H

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "config.h"


// Forward declarations
typedef struct BTreePageHeader BTreePageHeader;
typedef struct DataPageHeader DataPageHeader;
typedef struct OverflowPageHeader OverflowPageHeader;

#include "pager/db/index/page_format.h"
#include "pager/db/data/page_format.h"
#include "pager/db/overflow/page_format.h"

typedef enum {
    DATA,
    INDEX,
    OVERFLOW
} PageType;

// Header different for each of 3 Page types 
typedef union {
    BTreePageHeader btree;
    DataPageHeader data;
    OverflowPageHeader overflow;
} PageTypeHeader;

// Generic Page Header
typedef struct {
    uint16_t page_id;  // Max of 2^16 = 65535 pages
    uint8_t dirty;
    uint8_t free;
    PageType type;
    PageTypeHeader type_specific;  // These have to be padded to same size in each implementation 
} PageHeader;

// Page Memory, aligned to PAGE_SIZE (4KB usually) 
// The usable space is further capped to prevent journal from exceeding PAGE_SIZE
typedef struct {
    PageHeader header;
    uint8_t payload[MAX_DATA_BYTES];  // The data depends on the page type 
} DBPage;

#endif /* PRESEQL_PAGER_DB_FORMAT_H */

