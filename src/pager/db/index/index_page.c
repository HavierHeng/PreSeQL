#include "index_page.h"
#include <string.h>
#include <stdlib.h>

// Helper macros - some are for measuring when to split
// B+ tree has no fixed order - its an effective order based on size of slot data
#define IS_LEAF(page) ((page)->header.flag & PAGE_INDEX_LEAF)
#define IS_INTERNAL(page) ((page)->header.flag & PAGE_INDEX_INTERNAL)
#define USED_SPACE(page) (MAX_USABLE_PAGE_SIZE - (page)->header.free_total)
#define FULL_THRESHOLD (MAX_USABLE_PAGE_SIZE * INDEX_FULL_OCCUPANCY) // 80% of 4032 bytes
#define MIN_THRESHOLD (MAX_USABLE_PAGE_SIZE * INDEX_MIN_OCCUPANCY)   // 40% of 4032 bytes


// Encode INTEGER key for lexicographic comparison
// Uses XOR trick to offset 2^63
uint64_t encode_int_key(int64_t value) {
    return (uint64_t)value ^ 0x8000000000000000ULL;
}

// Compare two keys lexicographically
int compare_keys(const uint8_t* key1, const uint8_t* key2, size_t key_size) {
    return memcmp(key1, key2, key_size);
}

// Initialize an internal index page
DBPage* init_index_internal_page(Pager* pager, uint16_t page_no) {
    DBPage* page = pager_get_page(pager, page_no);
    if (!page) return NULL;
    
    memset(&page->header, 0, sizeof(DBPageHeader));
    page->header.page_id = page_no;
    page->header.flag = PAGE_INDEX_INTERNAL;
    page->header.free_start = sizeof(DBPageHeader);
    page->header.free_end = MAX_USABLE_PAGE_SIZE;
    page->header.free_total = MAX_USABLE_PAGE_SIZE - sizeof(DBPageHeader);
    page->header.right_sibling_page_id = 0;
    
    pager_write_page(pager, page);
    return page;
}

// Initialize a leaf index page
DBPage* init_index_leaf_page(Pager* pager, uint16_t page_no) {
    DBPage* page = pager_get_page(pager, page_no);
    if (!page) return NULL;
    
    memset(&page->header, 0, sizeof(DBPageHeader));
    page->header.page_id = page_no;
    page->header.flag = PAGE_INDEX_LEAF;
    page->header.free_start = sizeof(DBPageHeader);
    page->header.free_end = MAX_USABLE_PAGE_SIZE;
    page->header.free_total = MAX_USABLE_PAGE_SIZE - sizeof(DBPageHeader);
    page->header.right_sibling_page_id = 0;
    
    pager_write_page(pager, page);
    return page;
}

// Find an empty slot in an index page
uint8_t find_empty_index_slot(Pager* pager, uint16_t page_id, uint64_t key_size) {
    DBPage* page = pager_get_page(pager, page_id);
    if (!page || key_size > MAX_DATA_PER_INDEX_SLOT) return 0;
    
    if (page->header.free_slot_count > 0) {
        uint8_t slot_id = page->header.free_slot_list[page->header.free_slot_count - 1];
        page->header.free_slot_count--;
        return slot_id;
    }
    
    // Check if there's enough space for a new slot
    if (USED_SPACE(page) < FULL_THRESHOLD && page->header.free_total >= (INDEX_SLOT_DATA_SIZE + SLOT_ENTRY_SIZE)) {
        page->header.highest_slot++;
        return page->header.highest_slot;
    }
    
    return 0;
}

// Read an index slot
void read_index_slot(Pager* pager, uint16_t page_id, uint8_t slot_id, IndexSlotData* slot) {
    DBPage* page = pager_get_page(pager, page_id);
    if (!page || slot_id >= page->header.total_slots) return;
    
    SlotEntry* entry = (SlotEntry*)(page->data + page->header.free_start);
    for (uint8_t i = 0; i < page->header.total_slots; i++) {
        if (entry[i].slot_id == slot_id) {
            memcpy(slot->key, page->data + entry[i].offset, entry[i].size);
            slot->next_page_id = *(uint16_t*)(page->data + entry[i].offset + entry[i].size);
            slot->next_slot_id = *(uint8_t*)(page->data + entry[i].offset + entry[i].size + 2);
            slot->overflow.next_page_id = *(uint16_t*)(page->data + entry[i].offset + entry[i].size + 3);
            slot->overflow.next_chunk_id = *(uint16_t*)(page->data + entry[i].offset + entry[i].size + 5);
            break;
        }
    }
}

// Write an index slot
void write_index_slot(Pager* pager, uint16_t page_id, IndexSlotData* slot) {
    DBPage* page = pager_get_page(pager, page_id);
    if (!page || USED_SPACE(page) >= FULL_THRESHOLD) return;
    
    uint8_t slot_id = find_empty_index_slot(pager, page_id, MAX_DATA_PER_INDEX_SLOT);
    if (slot_id == 0) return;
    
    // Find insertion point for sorted order
    SlotEntry* entries = (SlotEntry*)(page->data + page->header.free_start);
    uint8_t insert_pos = page->header.total_slots;
    for (uint8_t i = 0; i < page->header.total_slots; i++) {
        IndexSlotData existing;
        read_index_slot(pager, page_id, entries[i].slot_id, &existing);
        if (compare_keys(slot->key, existing.key, MAX_DATA_PER_INDEX_SLOT) < 0) {
            insert_pos = i;
            break;
        }
    }
    
    // Shift entries if needed
    if (insert_pos < page->header.total_slots) {
        memmove(&entries[insert_pos + 1], &entries[insert_pos], (page->header.total_slots - insert_pos) * sizeof(SlotEntry));
    }
    
    // Write slot data
    uint64_t offset = page->header.free_end - INDEX_SLOT_DATA_SIZE;
    memcpy(page->data + offset, slot->key, MAX_DATA_PER_INDEX_SLOT);
    *(uint16_t*)(page->data + offset + MAX_DATA_PER_INDEX_SLOT) = slot->next_page_id;
    *(uint8_t*)(page->data + offset + MAX_DATA_PER_INDEX_SLOT + 2) = slot->next_slot_id;
    *(uint16_t*)(page->data + offset + MAX_DATA_PER_INDEX_SLOT + 3) = slot->overflow.next_page_id;
    *(uint16_t*)(page->data + offset + MAX_DATA_PER_INDEX_SLOT + 5) = slot->overflow.next_chunk_id;
    
    // Update slot entry
    entries[insert_pos].slot_id = slot_id;
    entries[insert_pos].offset = offset;
    entries[insert_pos].size = MAX_DATA_PER_INDEX_SLOT;
    
    // Update header
    page->header.total_slots++;
    page->header.free_end = offset;
    page->header.free_total -= (INDEX_SLOT_DATA_SIZE + SLOT_ENTRY_SIZE);
    
    pager_write_page(pager, page);
}

// Free an index slot
void free_index_slot(Pager* pager, uint16_t page_id, uint8_t slot_id) {
    DBPage* page = pager_get_page(pager, page_id);
    if (!page || slot_id >= page->header.total_slots) return;
    
    SlotEntry* entries = (SlotEntry*)(page->data + page->header.free_start);
    for (uint8_t i = 0; i < page->header.total_slots; i++) {
        if (entries[i].slot_id == slot_id) {
            memmove(&entries[i], &entries[i + 1], (page->header.total_slots - i - 1) * sizeof(SlotEntry));
            page->header.total_slots--;
            page->header.free_slot_list[page->header.free_slot_count++] = slot_id;
            page->header.free_total += entries[i].size + sizeof(SlotEntry);
            pager_write_page(pager, page);
            break;
        }
    }
}

// Initialize a new B+ tree
PSqlStatus btree_init(Pager* pager, const char* table_name, uint8_t index_type, uint16_t* out_root_page) {
    uint16_t root_page_id = get_free_page(pager);
    if (root_page_id == 0) return PSQL_STATUS_OUT_OF_MEMORY;
    
    DBPage* root = init_index_leaf_page(pager, root_page_id);
    if (!root) {
        mark_page_free(pager, root_page_id);
        return PSQL_STATUS_IO_ERROR;
    }
    
    uint16_t table_id;
    PSqlStatus status = catalog_add_table(table_name, root_page_id, index_type, 0, &table_id);
    if (status != PSQL_STATUS_OK) {
        mark_page_free(pager, root_page_id);
        return status;
    }
    
    *out_root_page = root_page_id;
    return PSQL_STATUS_OK;
}

// Destroy a B+ tree
PSqlStatus btree_destroy(Pager* pager, uint16_t root_page_id) {
    DBPage* page = pager_get_page(pager, root_page_id);
    if (!page) return PSQL_STATUS_INVALID_PAGE;
    
    if (IS_INTERNAL(page)) {
        for (uint8_t i = 0; i < page->header.total_slots; i++) {
            IndexSlotData slot;
            read_index_slot(pager, root_page_id, i, &slot);
            btree_destroy(pager, slot.next_page_id);
        }
    }
    
    for (uint8_t i = 0; i < page->header.total_slots; i++) {
        IndexSlotData slot;
        read_index_slot(pager, root_page_id, i, &slot);
        if (slot.overflow.next_page_id != 0) {
            mark_page_free(pager, slot.overflow.next_page_id);
        }
        if (IS_LEAF(page)) {
            DBPage* data_page = pager_get_page(pager, slot.next_page_id);
            if (data_page) {
                data_page->header.ref_counter--;
                if (data_page->header.ref_counter == 0) {
                    mark_page_free(pager, slot.next_page_id);
                } else {
                    vaccum_page(data_page);
                }
                pager_write_page(pager, data_page);
            }
        }
    }
    
    mark_page_free(pager, root_page_id);
    return PSQL_STATUS_OK;
}

// Search for a key in the B+ tree
PSqlStatus btree_search(Pager* pager, uint16_t root_page_id, const uint8_t* key, size_t key_size, uint16_t* result_page_id, uint8_t* result_slot_id) {
    if (key_size > MAX_DATA_PER_INDEX_SLOT) return PSQL_STATUS_INVALID_ARGUMENT;
    
    DBPage* page = pager_get_page(pager, root_page_id);
    if (!page) return PSQL_STATUS_INVALID_PAGE;
    
    while (page) {
        if (IS_LEAF(page)) {
            for (uint8_t i = 0; i < page->header.total_slots; i++) {
                IndexSlotData slot;
                read_index_slot(pager, page->header.page_id, i, &slot);
                int cmp = compare_keys(key, slot.key, key_size);
                if (cmp == 0) {
                    *result_page_id = page->header.page_id;
                    *result_slot_id = i;
                    return PSQL_STATUS_OK;
                }
                if (cmp < 0) break;
            }
            return PSQL_STATUS_NOT_FOUND;
        } else {
            uint8_t i;
            for (i = 0; i < page->header.total_slots; i++) {
                IndexSlotData slot;
                read_index_slot(pager, page->header.page_id, i, &slot);
                if (compare_keys(key, slot.key, key_size) < 0) break;
            }
            IndexSlotData slot;
            read_index_slot(pager, page->header.page_id, i, &slot);
            page = pager_get_page(pager, slot.next_page_id);
        }
    }
    
    return PSQL_STATUS_NOT_FOUND;
}

// Insert a key-value pair into the B+ tree
PSqlStatus btree_insert(Pager* pager, uint16_t root_page_id, const uint8_t* key, size_t key_size, uint16_t data_page_id, uint8_t data_slot_id) {
    if (key_size > MAX_DATA_PER_INDEX_SLOT) return PSQL_STATUS_INVALID_ARGUMENT;
    
    if (root_page_id == 0) {
        root_page_id = get_free_page(pager);
        if (root_page_id == 0) return PSQL_STATUS_OUT_OF_MEMORY;
        init_index_leaf_page(pager, root_page_id);
    }
    
    DBPage* page = pager_get_page(pager, root_page_id);
    if (!page) return PSQL_STATUS_INVALID_PAGE;
    
    // Traverse to leaf
    while (IS_INTERNAL(page)) {
        uint8_t i;
        for (i = 0; i < page->header.total_slots; i++) {
            IndexSlotData slot;
            read_index_slot(pager, page->header.page_id, i, &slot);
            if (compare_keys(key, slot.key, key_size) < 0) break;
        }
        IndexSlotData slot;
        read_index_slot(pager, page->header.page_id, i, &slot);
        page = pager_get_page(pager, slot.next_page_id);
    }
    
    // Insert into leaf
    IndexSlotData new_slot = {0};
    memcpy(new_slot.key, key, key_size);
    new_slot.next_page_id = data_page_id;
    new_slot.next_slot_id = data_slot_id;
    
    write_index_slot(pager, page->header.page_id, &new_slot);
    
    // Check for overflow and split if needed
    if (USED_SPACE(page) > FULL_THRESHOLD) {
        uint16_t new_page_id;
        PSqlStatus status = btree_split_leaf(pager, page->header.page_id, &new_page_id);
        if (status != PSQL_STATUS_OK) return status;
        
        // Update parent (simplified: assume root split for now)
        if (page->header.page_id == root_page_id) {
            uint16_t new_root_id = get_free_page(pager);
            if (new_root_id == 0) return PSQL_STATUS_OUT_OF_MEMORY;
            
            DBPage* new_root = init_index_internal_page(pager, new_root_id);
            IndexSlotData parent_slot = {0};
            read_index_slot(pager, new_page_id, 0, &parent_slot);
            parent_slot.next_page_id = page->header.page_id;
            write_index_slot(pager, new_root_id, &parent_slot);
            
            parent_slot.next_page_id = new_page_id;
            write_index_slot(pager, new_root_id, &parent_slot);
            
            // Update catalog with new root (requires catalog access)
            // For simplicity, assume root_page_id is updated externally
        }
    }
    
    return PSQL_STATUS_OK;
}

// Delete a key from the B+ tree
PSqlStatus btree_delete(Pager* pager, uint16_t root_page_id, const uint8_t* key, size_t key_size) {
    if (key_size > MAX_DATA_PER_INDEX_SLOT) return PSQL_STATUS_INVALID_ARGUMENT;
    
    DBPage* page = pager_get_page(pager, root_page_id);
    if (!page) return PSQL_STATUS_INVALID_PAGE;
    
    DBPage* parent = NULL;
    uint8_t parent_idx = 0;
    
    // Traverse to leaf
    while (IS_INTERNAL(page)) {
        parent = page;
        uint8_t i;
        for (i = 0; i < page->header.total_slots; i++) {
            IndexSlotData slot;
            read_index_slot(pager, page->header.page_id, i, &slot);
            if (compare_keys(key, slot.key, key_size) < 0) break;
        }
        parent_idx = i;
        IndexSlotData slot;
        read_index_slot(pager, page->header.page_id, i, &slot);
        page = pager_get_page(pager, slot.next_page_id);
    }
    
    // Find and delete entry
    for (uint8_t i = 0; i < page->header.total_slots; i++) {
        IndexSlotData slot;
        read_index_slot(pager, page->header.page_id, i, &slot);
        if (compare_keys(key, slot.key, key_size) == 0) {
            free_index_slot(pager, page->header.page_id, i);
            break;
        }
    }
    
    // Rebalance if underflow - can be told by the threshold occupancy
    if (USED_SPACE(page) < MIN_THRESHOLD && parent) {
        IndexSlotData parent_slot;
        read_index_slot(pager, parent->header.page_id, parent_idx, &parent_slot);
        DBPage* sibling = pager_get_page(pager, parent_slot.next_page_id);
        if (sibling && USED_SPACE(sibling) > MIN_THRESHOLD) {
            // Borrow from sibling
            IndexSlotData sibling_slot;
            read_index_slot(pager, sibling->header.page_id, 0, &sibling_slot);
            write_index_slot(pager, page->header.page_id, &sibling_slot);
            free_index_slot(pager, sibling->header.page_id, 0);
            
            // Update parent key
            read_index_slot(pager, sibling->header.page_id, 0, &parent_slot);
            write_index_slot(pager, parent->header.page_id, &parent_slot);
        } else {
            // Merge with sibling
            for (uint8_t i = 0; i < sibling->header.total_slots; i++) {
                IndexSlotData sibling_slot;
                read_index_slot(pager, sibling->header.page_id, i, &sibling_slot);
                write_index_slot(pager, page->header.page_id, &sibling_slot);
            }
            page->header.right_sibling_page_id = sibling->header.right_sibling_page_id;
            free_index_slot(pager, parent->header.page_id, parent_idx);
            mark_page_free(pager, sibling->header.page_id);
        }
    }
    
    return PSQL_STATUS_OK;
}

// Split a leaf node
PSqlStatus btree_split_leaf(Pager* pager, uint16_t leaf_page_id, uint16_t* new_page_id) {
    DBPage* leaf_page = pager_get_page(pager, leaf_page_id);
    if (!leaf_page || !IS_LEAF(leaf_page)) return PSQL_STATUS_INVALID_PAGE;
    
    uint16_t new_leaf_id = get_free_page(pager);
    if (new_leaf_id == 0) return PSQL_STATUS_OUT_OF_MEMORY;
    
    DBPage* new_leaf = init_index_leaf_page(pager, new_leaf_id);
    if (!new_leaf) {
        mark_page_free(pager, new_leaf_id);
        return PSQL_STATUS_IO_ERROR;
    }
    
    // Set up sibling pointers
    new_leaf->header.right_sibling_page_id = leaf_page->header.right_sibling_page_id;
    leaf_page->header.right_sibling_page_id = new_leaf_id;
    
    // Move half of the slots to the new page
    uint8_t slot_count = leaf_page->header.total_slots;
    uint8_t mid = slot_count / 2;
    
    for (uint8_t i = mid; i < slot_count; i++) {
        IndexSlotData slot;
        read_index_slot(pager, leaf_page_id, i, &slot);
        write_index_slot(pager, new_leaf_id, &slot);
        free_index_slot(pager, leaf_page_id, i);
    }
    
    leaf_page->header.total_slots = mid;
    
    pager_write_page(pager, leaf_page);
    pager_write_page(pager, new_leaf);
    
    *new_page_id = new_leaf_id;
    return PSQL_STATUS_OK;
}

// Split an internal node
PSqlStatus btree_split_internal(Pager* pager, uint16_t internal_page_id, uint16_t* new_page_id) {
    DBPage* internal_page = pager_get_page(pager, internal_page_id);
    if (!internal_page || !IS_INTERNAL(internal_page)) return PSQL_STATUS_INVALID_PAGE;
    
    uint16_t new_internal_id = get_free_page(pager);
    if (new_internal_id == 0) return PSQL_STATUS_OUT_OF_MEMORY;
    
    DBPage* new_internal = init_index_internal_page(pager, new_internal_id);
    if (!new_internal) {
        mark_page_free(pager, new_internal_id);
        return PSQL_STATUS_IO_ERROR;
    }
    
    // Move half of the slots to the new page
    uint8_t slot_count = internal_page->header.total_slots;
    uint8_t mid = slot_count / 2;
    
    for (uint8_t i = mid; i < slot_count; i++) {
        IndexSlotData slot;
        read_index_slot(pager, internal_page_id, i, &slot);
        write_index_slot(pager, new_internal_id, &slot);
        free_index_slot(pager, internal_page_id, i);
    }
    
    internal_page->header.total_slots = mid;
    
    pager_write_page(pager, internal_page);
    pager_write_page(pager, new_internal);
    
    *new_page_id = new_internal_id;
    return PSQL_STATUS_OK;
}

// Create a B+ tree iterator
BTreeIterator* btree_iterator_create(Pager* pager, uint16_t root_page_id) {
    BTreeIterator* iterator = (BTreeIterator*)malloc(sizeof(BTreeIterator));
    if (!iterator) return NULL;
    
    memset(iterator, 0, sizeof(BTreeIterator));
    iterator->pager = pager;
    iterator->root_page_id = root_page_id;
    iterator->current_page_id = root_page_id;
    
    return iterator;
}

// Create a B+ tree iterator with a key range
BTreeIterator* btree_iterator_range(Pager* pager, uint16_t root_page_id, const uint8_t* start_key, const uint8_t* end_key, size_t key_size) {
    BTreeIterator* iterator = btree_iterator_create(pager, root_page_id);
    if (!iterator) return NULL;
    
    iterator->has_range = 1;
    iterator->key_size = key_size;
    
    if (start_key) {
        iterator->start_key = (uint8_t*)malloc(key_size);
        if (!iterator->start_key) {
            free(iterator);
            return NULL;
        }
        memcpy(iterator->start_key, start_key, key_size);
        
        uint16_t page_id;
        uint8_t slot_id;
        PSqlStatus status = btree_search(pager, root_page_id, start_key, key_size, &page_id, &slot_id);
        if (status == PSQL_STATUS_OK) {
            iterator->current_page_id = page_id;
            iterator->current_slot_id = slot_id;
        }
    }
    
    if (end_key) {
        iterator->end_key = (uint8_t*)malloc(key_size);
        if (!iterator->end_key) {
            free(iterator->start_key);
            free(iterator);
            return NULL;
        }
        memcpy(iterator->end_key, end_key, key_size);
    }
    
    return iterator;
}

// Get the next key-value pair from the iterator
int btree_iterator_next(BTreeIterator* iterator, uint16_t* data_page_id, uint8_t* data_slot_id) {
    if (!iterator || iterator->current_page_id == 0) return 0;
    
    DBPage* page = pager_get_page(iterator->pager, iterator->current_page_id);
    if (!page) return 0;
    
    // Check if we've reached the end of the current page
    if (iterator->current_slot_id >= page->header.total_slots) {
        uint16_t next_page_id = page->header.right_sibling_page_id;
        if (next_page_id == 0) return 0;
        
        iterator->current_page_id = next_page_id;
        iterator->current_slot_id = 0;
        page = pager_get_page(iterator->pager, next_page_id);
        if (!page) return 0;
    }
    
    // Get the current slot
    IndexSlotData slot;
    read_index_slot(iterator->pager, iterator->current_page_id, iterator->current_slot_id, &slot);
    
    // Check if we've reached the end of the range
    if (iterator->has_range && iterator->end_key && compare_keys(slot.key, iterator->end_key, iterator->key_size) > 0) {
        return 0;
    }
    
    *data_page_id = slot.next_page_id;
    *data_slot_id = slot.next_slot_id;
    
    iterator->current_slot_id++;
    return 1;
}

// Destroy the iterator
void btree_iterator_destroy(BTreeIterator* iterator) {
    if (iterator) {
        free(iterator->start_key);
        free(iterator->end_key);
        free(iterator);
    }
}
