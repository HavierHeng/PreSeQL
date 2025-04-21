#include "pager/db/overflow/overflow_page.h"
#include "pager/pager.h"
#include "status/db.h"
#include <string.h>
#include <stdlib.h>

// Size of a Chunk structure (data + OverflowPointer)
#define CHUNK_OVERHEAD (sizeof(uint16_t) + sizeof(uint16_t)) // OverflowPointer: next_page_id + next_chunk_id
#define MAX_CHUNK_DATA_SIZE (MAX_USABLE_PAGE_SIZE - CHUNK_OVERHEAD)

// Find an empty slot in an overflow page that can fit the data
uint8_t find_empty_overflow_slot(Pager* pager, uint16_t page_id, size_t data_size) {
    DBPage* page = pager_get_page(pager, page_id);
    if (!page) return 0;

    // Check existing free slots
    if (page->header.free_slot_count > 0) {
        uint8_t slot_id = page->header.free_slot_list[page->header.free_slot_count - 1];
        SlotEntry* entries = (SlotEntry*)(page->data + page->header.free_start);
        for (uint8_t i = 0; i < page->header.total_slots; i++) {
            if (entries[i].slot_id == slot_id) {
                if (entries[i].size >= data_size + CHUNK_OVERHEAD + SLOT_ENTRY_SIZE) {
                    page->header.free_slot_count--;
                    pager_write_page(pager, page);
                    return slot_id;
                }
                break;
            }
        }
    }

    // Check if there's enough space for a new slot
    size_t required_size = data_size + CHUNK_OVERHEAD + SLOT_ENTRY_SIZE;
    if (page->header.free_total >= required_size && page->header.highest_slot < 255) {
        page->header.highest_slot++;
        page->header.total_slots++;
        pager_write_page(pager, page);
        return page->header.highest_slot;
    }

    return 0;
}

// Read a chunk from an overflow page
void read_overflow_slot(Pager* pager, uint16_t page_id, uint8_t slot_id, Chunk* chunk) {
    DBPage* page = pager_get_page(pager, page_id);
    if (!page || slot_id > page->header.highest_slot) return;

    SlotEntry* entries = (SlotEntry*)(page->data + page->header.free_start);
    for (uint8_t i = 0; i < page->header.total_slots; i++) {
        if (entries[i].slot_id == slot_id) {
            uint16_t offset = entries[i].offset;
            memcpy(chunk->data, page->data + offset, entries[i].size - CHUNK_OVERHEAD);
            offset += entries[i].size - CHUNK_OVERHEAD;
            chunk->overflow.next_page_id = *(uint16_t*)(page->data + offset);
            offset += sizeof(uint16_t);
            chunk->overflow.next_chunk_id = *(uint16_t*)(page->data + offset);
            break;
        }
    }
}

// Write a chunk to an overflow page
PSqlStatus write_overflow_slot(Pager* pager, uint16_t page_id, uint8_t slot_id, const uint8_t* data, size_t data_size, OverflowPointer* overflow) {
    DBPage* page = pager_get_page(pager, page_id);
    if (!page) return PSQL_IOERR;

    // Find insertion point for slot entry (unordered, append to end)
    SlotEntry* entries = (SlotEntry*)(page->data + page->header.free_start);
    uint8_t insert_pos = page->header.total_slots - 1;

    // Calculate space needed
    size_t slot_data_size = data_size + CHUNK_OVERHEAD;
    uint16_t offset = page->header.free_end - slot_data_size;

    // Write chunk data
    memcpy(page->data + offset, data, data_size);
    offset += data_size;
    *(uint16_t*)(page->data + offset) = overflow->next_page_id;
    offset += sizeof(uint16_t);
    *(uint16_t*)(page->data + offset) = overflow->next_chunk_id;

    // Update slot entry
    entries[insert_pos].slot_id = slot_id;
    entries[insert_pos].offset = page->header.free_end - slot_data_size;
    entries[insert_pos].size = slot_data_size;

    // Update header
    page->header.free_end = entries[insert_pos].offset;
    page->header.free_total -= (slot_data_size + SLOT_ENTRY_SIZE);

    pager_write_page(pager, page);
    return PSQL_OK;
}

// Free an overflow slot
void free_overflow_slot(Pager* pager, uint16_t page_id, uint8_t slot_id) {
    DBPage* page = pager_get_page(pager, page_id);
    if (!page || slot_id > page->header.highest_slot) return;

    SlotEntry* entries = (SlotEntry*)(page->data + page->header.free_start);
    for (uint8_t i = 0; i < page->header.total_slots; i++) {
        if (entries[i].slot_id == slot_id) {
            // Add to free slot list
            page->header.free_slot_list[page->header.free_slot_count++] = slot_id;
            page->header.free_total += entries[i].size + SLOT_ENTRY_SIZE;
            // Shift entries
            memmove(&entries[i], &entries[i + 1], (page->header.total_slots - i - 1) * sizeof(SlotEntry));
            page->header.total_slots--;
            pager_write_page(pager, page);
            break;
        }
    }
}

// Write data to an overflow page, splitting across multiple pages if necessary
PSqlStatus write_overflow_data(Pager* pager, uint16_t page_id, const uint8_t* data, size_t size, uint16_t* chunk_id) {
    if (!pager || !data || size == 0) return PSQL_MISUSE;

    DBPage* page = pager_get_page(pager, page_id);
    if (!page || page->header.flag != PAGE_OVERFLOW) return PSQL_IOERR;

    size_t remaining_size = size;
    const uint8_t* data_ptr = data;
    uint16_t first_chunk_id = 0;
    uint16_t current_page_id = page_id;
    OverflowPointer next_overflow = {0, 0};

    while (remaining_size > 0) {
        // Determine how much data can fit in this page
        size_t chunk_size = (remaining_size > MAX_CHUNK_DATA_SIZE) ? MAX_CHUNK_DATA_SIZE : remaining_size;
        uint8_t slot_id = find_empty_overflow_slot(pager, page_id, chunk_size);
        if (slot_id == 0) {
            // No space in current page, allocate a new one
            current_page_id = get_free_page(pager);
            if (current_page_id == 0) return PSQL_NOMEM;
            if (!init_overflow_page(pager, current_page_id)) {
                mark_page_free(pager, current_page_id);
                return PSQL_IOERR;
            }
            slot_id = find_empty_overflow_slot(pager, page_id, chunk_size);
            if (slot_id == 0) {
                mark_page_free(pager, current_page_id);
                return PSQL_NOMEM;
            }
            next_overflow.next_page_id = current_page_id;
            next_overflow.next_chunk_id = slot_id;
        } else {
            next_overflow.next_page_id = 0;
            next_overflow.next_chunk_id = 0;
        }

        // Write the chunk
        PSqlStatus status = write_overflow_slot(pager, current_page_id, slot_id, data_ptr, chunk_size, &next_overflow);
        if (status != PSQL_OK) return status;

        if (first_chunk_id == 0) {
            first_chunk_id = slot_id;
        }

        // Update pointers
        remaining_size -= chunk_size;
        data_ptr += chunk_size;
        if (remaining_size > 0) {
            current_page_id = next_overflow.next_page_id;
        }
    }

    *chunk_id = first_chunk_id;
    return PSQL_OK;
}

// Read data from an overflow page chain
PSqlStatus read_overflow_data(Pager* pager, uint16_t page_id, uint16_t chunk_id, uint8_t* data, uint16_t* size) {
    if (!pager || !data || !size) return PSQL_MISUSE;

    size_t total_size = 0;
    uint16_t current_page_id = page_id;
    uint16_t current_chunk_id = chunk_id;
    uint8_t* data_ptr = data;

    while (current_page_id != 0) {
        DBPage* page = pager_get_page(pager, current_page_id);
        if (!page || page->header.flag != PAGE_OVERFLOW) return PSQL_IOERR;

        Chunk chunk;
        read_overflow_slot(pager, current_page_id, current_chunk_id, &chunk);

        // Find slot size
        SlotEntry* entries = (SlotEntry*)(page->data + page->header.free_start);
        size_t chunk_data_size = 0;
        for (uint8_t i = 0; i < page->header.total_slots; i++) {
            if (entries[i].slot_id == current_chunk_id) {
                chunk_data_size = entries[i].size - CHUNK_OVERHEAD;
                break;
            }
        }

        // Copy data
        if (total_size + chunk_data_size > *size) {
            return PSQL_OK;
        }
        memcpy(data_ptr, chunk.data, chunk_data_size);
        data_ptr += chunk_data_size;
        total_size += chunk_data_size;

        // Move to next chunk
        current_page_id = chunk.overflow.next_page_id;
        current_chunk_id = chunk.overflow.next_chunk_id;
    }

    *size = total_size;
    return PSQL_OK;
}
