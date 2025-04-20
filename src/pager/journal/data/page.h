#include <stdint.h>
#include <stdlib.h>
#include "pager/constants.h"
#include "status/db.h"

typedef struct {
    // Page related
    uint16_t page_id;  // Journal page ID - Max of 2^16 = 65535 pages 
    uint64_t txn_id;        // Transaction this journal entry belongs to
    uint16_t original_page_id; // The page number being backed up
    uint8_t flag;  // See above for PAGE flags

    // Pad to MAX_PAGE_HEADER_SIZE = 32 in bytes
    uint8_t reserved[23];
} JournalDataPageHeader;

// Page Memory, aligned to PAGE_SIZE (4096 Bytes usually) 
// The usable space is further capped to prevent journal from exceeding PAGE_SIZE
// data includes Slot directory + slot data entries + free space
typedef struct {
    JournalDataPageHeader header;
    uint8_t data[MAX_USABLE_PAGE_SIZE + MAX_PAGE_HEADER_SIZE];  // 4032 bytes - The data depends on the page type - its filled with SlotEntry and Index/Data/OverflowSlotData types
} JournalDataPage;

