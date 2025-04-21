#include "pager/db/page_format.h"  // DBPage, DBPageHeader, SlotEntry
#include "pager/constants.h"
#include "pager/pager.h"
#include "pager/db/free_space.h"

// Manipulating slots - Data
typedef struct {
    PSqlDataTypes type;
    uint8_t* data;
    uint64_t size;
    OverflowPointer* overflow;  // NULL if no overflow
} Cell;  // Single column value in a returned row

typedef struct {
    uint32_t num_columns;
    Cell* cells;
} Row;  // A whole row of data

/* For Data Page */
typedef struct {
    uint8_t data[MAX_DATA_PER_DATA_SLOT];  // Up to MAX_DATA_PER_DATA_SLOT (255) bytes of truncated data - the name is different to reflect that values are stored as is
    OverflowPointer overflow;  // Overflow pointer - if row cannot fit within the data given
} DataSlotData;

uint8_t find_empty_data_slot(Pager* pager, uint64_t value_size);  // Finds a slot in data pages that can fit the value - i.e a row of data via searching the radix buckets
// Data slots do not need to be ordered in any way, so any candidate with space will do
void read_data_slot(Pager* pager, uint16_t page_id, uint8_t slot_id, Row *row);
void write_data_slot(Pager* pager, Row *row);  // Pager doesn't need to slot in any particular data slot - only condition is that the page has enough free space - yes this will lead to inefficiency of page accesses since i mix data but screw it
void free_data_slot(Pager* pager, uint16_t page_id, uint8_t slot_id);

DBPage* init_data_page(Pager* pager, uint16_t page_no);
void vaccum_data_pages(FreeSpaceTracker* tracker);  // Vaccum only on 4-bucket radix tree with tracker - this is done when the database is online - i.e only data and overflow pages get this treatment
