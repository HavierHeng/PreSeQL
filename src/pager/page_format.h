/* 
 * Collate page structures and headers format 
 * */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "data/page.h"
#include "overflow/page.h"
#include "index/page.h"


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
    PageTypeHeader type_specific;  // TODO: These have to be padded to same size in each implementation 
} PageHeader;

// Page Memory, aligned to PAGE_SIZE (4KB usually) 
// The usable space is further capped to prevent journal from exceeding PAGE_SIZE
// TODO: This is given me a warning that header is variable sized. Though i thought it would be aligned to the largest member
typedef struct {
    PageHeader header;
    uint8_t data[MAX_DATA_BYTES];  // The things is data is depending on the page type 
} Page;

