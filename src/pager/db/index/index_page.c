#include "index_page.h"
#include "pager/pager_format.h"
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
            uint16_t offset = entry[i].offset;
            memcpy(slot->key, page->data + offset, MAX_DATA_PER_INDEX_SLOT);
            offset += MAX_DATA_PER_INDEX_SLOT;
            slot->key_size = *(uint16_t*)(page->data + offset);
            offset += sizeof(uint16_t);
            slot->next_page_id = *(uint16_t*)(page->data + offset);
            offset += sizeof(uint16_t);
            slot->next_slot_id = *(uint8_t*)(page->data + offset);
            offset += sizeof(uint8_t);
            slot->overflow.next_page_id = *(uint16_t*)(page->data + offset);
            offset += sizeof(uint16_t);
            slot->overflow.next_chunk_id = *(uint16_t*)(page->data + offset);
            offset += sizeof(uint16_t);
            slot->value_size = *(uint16_t*)(page->data + offset);
            break;
        }
    }
}

// Write an index slot with value
void write_index_slot_value(Pager* pager, uint16_t page_id, IndexSlotData* slot, const uint8_t* value, size_t value_size) {
    DBPage* page = pager_get_page(pager, page_id);
    if (!page || USED_SPACE(page) >= FULL_THRESHOLD) return;
    
    uint8_t slot_id = find_empty_index_slot(pager, page_id, slot->key_size);
    if (slot_id == 0) return;
    
    // Find insertion point for sorted order
    SlotEntry* entries = (SlotEntry*)(page->data + page->header.free_start);
    uint8_t insert_pos = page->header.total_slots;
    for (uint8_t i = 0; i < page->header.total_slots; i++) {
        IndexSlotData existing;
        read_index_slot(pager, page_id, entries[i].slot_id, &existing);
        if (compare_keys(slot->key, existing.key, slot->key_size) < 0) {
            insert_pos = i;
            break;
        }
    }
    
    // Shift entries if needed
    if (insert_pos < page->header.total_slots) {
        memmove(&entries[insert_pos + 1], &entries[insert_pos], (page->header.total_slots - insert_pos) * sizeof(SlotEntry));
    }
    
    // Handle overflow if value_size exceeds MAX_DATA_PER_INDEX_SLOT
    uint16_t overflow_page_id = 0;
    uint16_t overflow_chunk_id = 0;
    size_t data_to_write = value_size;
    uint8_t* data_ptr = (uint8_t*)value;
    
    if (value_size > MAX_DATA_PER_INDEX_SLOT) {
        overflow_page_id = get_free_page(pager);
        if (overflow_page_id == 0) return;
        DBPage* overflow_page = init_overflow_page(pager, overflow_page_id);
        if (!overflow_page) {
            mark_page_free(pager, overflow_page_id);
            return;
        }
        
        PSqlStatus status = write_overflow_data(pager, overflow_page_id, data_ptr, value_size, &overflow_chunk_id);
        if (status != PSQL_OK) {
            mark_page_free(pager, overflow_page_id);
            return;
        }
        data_to_write = 0; // Value stored in overflow
    }
    
    // Write slot data
    size_t slot_data_size = MAX_DATA_PER_INDEX_SLOT + sizeof(uint16_t) + sizeof(uint16_t) + sizeof(uint8_t) + sizeof(uint16_t) + sizeof(uint16_t) + sizeof(uint16_t); // key + key_size + next_page_id + next_slot_id + overflow_page_id + overflow_chunk_id + value_size
    uint16_t offset = page->header.free_end - slot_data_size - data_to_write;
    
    memcpy(page->data + offset, slot->key, slot->key_size);
    offset += MAX_DATA_PER_INDEX_SLOT;
    *(uint16_t*)(page->data + offset) = slot->key_size;
    offset += sizeof(uint16_t);
    *(uint16_t*)(page->data + offset) = slot->next_page_id;
    offset += sizeof(uint16_t);
    *(uint8_t*)(page->data + offset) = slot->next_slot_id;
    offset += sizeof(uint8_t);
    *(uint16_t*)(page->data + offset) = overflow_page_id;
    offset += sizeof(uint16_t);
    *(uint16_t*)(page->data + offset) = overflow_chunk_id;
    offset += sizeof(uint16_t);
    *(uint16_t*)(page->data + offset) = value_size;
    
    if (data_to_write > 0) {
        memcpy(page->data + offset + sizeof(uint16_t), data_ptr, data_to_write);
    }
    
    // Update slot entry
    entries[insert_pos].slot_id = slot_id;
    entries[insert_pos].offset = page->header.free_end - slot_data_size - data_to_write;
    entries[insert_pos].size = slot_data_size + data_to_write;
    
    // Update header
    page->header.total_slots++;
    page->header.free_end = entries[insert_pos].offset;
    page->header.free_total -= (slot_data_size + data_to_write + SLOT_ENTRY_SIZE);
    
    pager_write_page(pager, page);
}

// Write an index slot (for data page references)
void write_index_slot(Pager* pager, uint16_t page_id, IndexSlotData* slot) {
    write_index_slot_value(pager, page_id, slot, NULL, 0);
}

// Free an index slot
void free_index_slot(Pager* pager, uint16_t page_id, uint8_t slot_id) {
    DBPage* page = pager_get_page(pager, page_id);
    if (!page || slot_id >= page->header.total_slots) return;
    
    SlotEntry* entries = (SlotEntry*)(page->data + page->header.free_start);
    for (uint8_t i = 0; i < page->header.total_slots; i++) {
        if (entries[i].slot_id == slot_id) {
            IndexSlotData slot;
            read_index_slot(pager, page_id, slot_id, &slot);
            if (slot.overflow.next_page_id != 0) {
                mark_page_free(pager, slot.overflow.next_page_id);
            }
            memmove(&entries[i], &entries[i + 1], (page->header.total_slots - i - 1) * sizeof(SlotEntry));
            page->header.total_slots--;
            page->header.free_slot_list[page->header.free_slot_count++] = slot_id;
            page->header.free_total += entries[i].size + SLOT_ENTRY_SIZE;
            pager_write_page(pager, page);
            break;
        }
    }
}

// Initialize a new B+ tree
PSqlStatus btree_init(Pager* pager, const char* table_name, uint8_t index_type, uint16_t* out_root_page) {
    uint16_t root_page_id = get_free_page(pager);
    if (root_page_id == 0) return PSQL_NOMEM;
    
    DBPage* root = init_index_leaf_page(pager, root_page_id);
    if (!root) {
        mark_page_free(pager, root_page_id);
        return PSQL_IOERR;
    }
    
    uint16_t table_id;
    PSqlStatus status = catalog_add_table(table_name, root_page_id, index_type, 0, &table_id);
    if (status != PSQL_OK) {
        mark_page_free(pager, root_page_id);
        return status;
    }
    
    *out_root_page = root_page_id;
    return PSQL_OK;
}

// Destroy a B+ tree
PSqlStatus btree_destroy(Pager* pager, uint16_t root_page_id) {
    DBPage* page = pager_get_page(pager, root_page_id);
    if (!page) return PSQL_IOERR;
    
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
        if (IS_LEAF(page) && slot.value_size == 0) {
            DBPage* data_page = pager_get_page(pager, slot.next_page_id);
            if (data_page) {
                data_page->header.ref_counter--;
                if (data_page->header.ref_counter == 0) {
                    mark_page_free(pager, slot.next_page_id);
                } else {
                    vacuum_page(data_page);
                }
                pager_write_page(pager, data_page);
            }
        }
    }
    
    mark_page_free(pager, root_page_id);
    return PSQL_OK;
}

// Search for a key in the B+ tree
PSqlStatus btree_search(Pager* pager, uint16_t root_page_id, const uint8_t* key, size_t key_size, uint16_t* result_page_id, uint8_t* result_slot_id) {
    if (key_size > MAX_DATA_PER_INDEX_SLOT) return PSQL_MISUSE;
    
    DBPage* page = pager_get_page(pager, root_page_id);
    if (!page) return PSQL_IOERR;
    
    while (page) {
        if (IS_LEAF(page)) {
            for (uint8_t i = 0; i < page->header.total_slots; i++) {
                IndexSlotData slot;
                read_index_slot(pager, page->header.page_id, i, &slot);
                int cmp = compare_keys(key, slot.key, key_size);
                if (cmp == 0) {
                    *result_page_id = page->header.page_id;
                    *result_slot_id = i;
                    return PSQL_OK;
                }
                if (cmp < 0) break;
            }
            return PSQL_NOTFOUND;
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
    
    return PSQL_NOTFOUND;
}

// Insert a key-value pair into the B+ tree (data page reference)
PSqlStatus btree_insert(Pager* pager, uint16_t root_page_id, const uint8_t* key, size_t key_size, uint16_t data_page_id, uint8_t data_slot_id) {
    if (key_size > MAX_DATA_PER_INDEX_SLOT) return PSQL_MISUSE;
    
    if (root_page_id == 0) {
        root_page_id = get_free_page(pager);
        if (root_page_id == 0) return PSQL_NOMEM;
        init_index_leaf_page(pager, root_page_id);
    }
    
    DBPage* page = pager_get_page(pager, root_page_id);
    if (!page) return PSQL_IOERR;
    
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
    new_slot.key_size = key_size;
    new_slot.next_page_id = data_page_id;
    new_slot.next_slot_id = data_slot_id;
    new_slot.value_size = 0;
    
    write_index_slot(pager, page->header.page_id, &new_slot);
    
    // Check for overflow and split if needed
    if (USED_SPACE(page) > FULL_THRESHOLD) {
        uint16_t new_page_id;
        PSqlStatus status = btree_split_leaf(pager, page->header.page_id, &new_page_id);
        if (status != PSQL_OK) return status;
        
        if (page->header.page_id == root_page_id) {
            uint16_t new_root_id = get_free_page(pager);
            if (new_root_id == 0) return PSQL_NOMEM;
            
            // DBPage* new_root = init_index_internal_page(pager, new_root_id);
            IndexSlotData parent_slot = {0};
            read_index_slot(pager, new_page_id, 0, &parent_slot);
            parent_slot.next_page_id = page->header.page_id;
            write_index_slot(pager, new_root_id, &parent_slot);
            
            parent_slot.next_page_id = new_page_id;
            write_index_slot(pager, new_root_id, &parent_slot);
        }
    }
    
    return PSQL_OK;
}

// Insert a key-value pair into the B+ tree (direct value storage)
PSqlStatus btree_insert_value(Pager* pager, uint16_t root_page_id, const uint8_t* key, size_t key_size, const uint8_t* value, size_t value_size) {
    if (key_size > MAX_DATA_PER_INDEX_SLOT) return PSQL_MISUSE;
    
    if (root_page_id == 0) {
        root_page_id = get_free_page(pager);
        if (root_page_id == 0) return PSQL_NOMEM;
        init_index_leaf_page(pager, root_page_id);
    }
    
    DBPage* page = pager_get_page(pager, root_page_id);
    if (!page) return PSQL_IOERR;
    
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
    new_slot.key_size = key_size;
    new_slot.value_size = value_size;
    
    write_index_slot_value(pager, page->header.page_id, &new_slot, value, value_size);
    
    // Check for overflow and split if needed
    if (USED_SPACE(page) > FULL_THRESHOLD) {
        uint16_t new_page_id;
        PSqlStatus status = btree_split_leaf(pager, page->header.page_id, &new_page_id);
        if (status != PSQL_OK) return status;
        
        if (page->header.page_id == root_page_id) {
            uint16_t new_root_id = get_free_page(pager);
            if (new_root_id == 0) return PSQL_NOMEM;
            
            // DBPage* new_root = init_index_internal_page(pager, new_root_id);
            IndexSlotData parent_slot = {0};
            read_index_slot(pager, new_page_id, 0, &parent_slot);
            parent_slot.next_page_id = page->header.page_id;
            write_index_slot(pager, new_root_id, &parent_slot);
            
            parent_slot.next_page_id = new_page_id;
            write_index_slot(pager, new_root_id, &parent_slot);
        }
    }
    
    return PSQL_OK;
}

// Delete a key from the B+ tree
PSqlStatus btree_delete(Pager* pager, uint16_t root_page_id, const uint8_t* key, size_t key_size) {
    if (key_size > MAX_DATA_PER_INDEX_SLOT) return PSQL_MISUSE;
    
    DBPage* page = pager_get_page(pager, root_page_id);
    if (!page) return PSQL_IOERR;
    
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
    
    // Rebalance if underflow
    if (USED_SPACE(page) < MIN_THRESHOLD && parent) {
        IndexSlotData parent_slot;
        read_index_slot(pager, parent->header.page_id, parent_idx, &parent_slot);
        DBPage* sibling = pager_get_page(pager, parent_slot.next_page_id);
        if (sibling && USED_SPACE(sibling) > MIN_THRESHOLD) {
            IndexSlotData sibling_slot;
            read_index_slot(pager, sibling->header.page_id, 0, &sibling_slot);
            write_index_slot(pager, page->header.page_id, &sibling_slot);
            free_index_slot(pager, sibling->header.page_id, 0);
            
            read_index_slot(pager, sibling->header.page_id, 0, &parent_slot);
            write_index_slot(pager, parent->header.page_id, &parent_slot);
        } else {
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
    
    return PSQL_OK;
}

// Split a leaf node
PSqlStatus btree_split_leaf(Pager* pager, uint16_t leaf_page_id, uint16_t* new_page_id) {
    DBPage* leaf_page = pager_get_page(pager, leaf_page_id);
    if (!leaf_page || !IS_LEAF(leaf_page)) return PSQL_IOERR;
    
    uint16_t new_leaf_id = get_free_page(pager);
    if (new_leaf_id == 0) return PSQL_NOMEM;
    
    DBPage* new_leaf = init_index_leaf_page(pager, new_leaf_id);
    if (!new_leaf) {
        mark_page_free(pager, new_leaf_id);
        return PSQL_IOERR;
    }
    
    new_leaf->header.right_sibling_page_id = leaf_page->header.right_sibling_page_id;
    leaf_page->header.right_sibling_page_id = new_leaf_id;
    
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
    return PSQL_OK;
}

// Split an internal node
PSqlStatus btree_split_internal(Pager* pager, uint16_t internal_page_id, uint16_t* new_page_id) {
    DBPage* internal_page = pager_get_page(pager, internal_page_id);
    if (!internal_page || !IS_INTERNAL(internal_page)) return PSQL_IOERR;
    
    uint16_t new_internal_id = get_free_page(pager);
    if (new_internal_id == 0) return PSQL_NOMEM;
    
    DBPage* new_internal = init_index_internal_page(pager, new_internal_id);
    if (!new_internal) {
        mark_page_free(pager, new_internal_id);
        return PSQL_IOERR;
    }
    
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
    return PSQL_OK;
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
        if (status == PSQL_OK) {
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
    
    if (iterator->current_slot_id >= page->header.total_slots) {
        uint16_t next_page_id = page->header.right_sibling_page_id;
        if (next_page_id == 0) return 0;
        
        iterator->current_page_id = next_page_id;
        iterator->current_slot_id = 0;
        page = pager_get_page(iterator->pager, next_page_id);
        if (!page) return 0;
    }
    
    IndexSlotData slot;
    read_index_slot(iterator->pager, iterator->current_page_id, iterator->current_slot_id, &slot);
    
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


// Helper function to find a row in a B+ tree
PSqlStatus btree_find_row(Pager* pager, uint16_t page_no, const uint8_t* key, uint16_t key_size, uint8_t* value, uint16_t* value_size) {
    uint16_t result_page_id;
    uint8_t result_slot_id;
    PSqlStatus status = btree_search(pager, page_no, key, key_size, &result_page_id, &result_slot_id);
    if (status != PSQL_OK) return status;

    IndexSlotData slot;
    read_index_slot(pager, result_page_id, result_slot_id, &slot);
    
    if (slot.value_size == 0) {
        // Value is stored in a data page (not expected for catalog)
        return PSQL_NOTFOUND;
    }
    
    if (slot.value_size <= MAX_DATA_PER_INDEX_SLOT && slot.overflow.next_page_id == 0) {
        // Value fits in the slot
        memcpy(value, slot.key + slot.key_size, slot.value_size);
        *value_size = slot.value_size;
    } else {
        // Read from overflow page
        status = read_overflow_data(pager, slot.overflow.next_page_id, slot.overflow.next_chunk_id, value, value_size);
        if (status != PSQL_OK) return status;
    }
    
    return PSQL_OK;
}

