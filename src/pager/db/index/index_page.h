/* 
* B+ Tree operations: 1) init/destroy - to create and clean up B+ Tree nodes - implicitly part of init data page (a page is a node) - so maybe no need make - probs just need to implement a Radix tree way to find either a freed page or fallback to highest page ID + 1 (stored in header as metadata)
* 2) btree_search() - to find entry
* 3) btree_insert() - to add new rows to the B+ Tree index
* 4) btree_split_leaf() - when leaf is too full, we have to split it. This also sets up the right sibling pointers that B+ Tree is known for fast linear access.
* 5) btree_split_interal() - when internal is too small. Its a separate function just due to how internal nodes are routers instead of data pointers.
* 6) btree_iterator_range() - returns a BTreeIterator (or something similar in idea, name needs to be more consistent), allowing me to get range of values
* 7) Row* btree_iterator_next(BPlusIterator *it) - steps through the iterator and returns the row stored
* 8) btree_delete() - to delete entries. This is optional because of the complexity to rebalance the tree after operations, but might be useful. Also when entries are deleted, this also updates some reference counter, updates the free page radix tree and so on. Its a cascade effect.
*/

#ifndef PRESEQL_PAGER_DB_INDEX_PAGE_H
#define PRESEQL_PAGER_DB_INDEX_PAGE_H

#include "status/db.h"
#include "pager/db/free_space.h"
#include "pager/db/catalog/catalog.h"
#include "pager/pager.h"
#include "pager/types.h"
#include "pager/db/overflow/overflow_page.h"

/* BTreeIterator structure - FOr stepping through results of range search */
typedef struct BTreeIterator {
    Pager* pager;
    uint16_t root_page_id;
    uint16_t current_page_id;
    uint8_t current_slot_id;
    uint8_t* start_key;
    uint8_t* end_key;
    size_t key_size;
    int has_range;
} BTreeIterator;

/* B+ Tree operations */
DBPage* init_index_internal_page(Pager* pager, uint16_t page_no);
DBPage* init_index_leaf_page(Pager* pager, uint16_t page_no);

/* Manipulating slots - Index */
uint8_t find_empty_index_slot(Pager* pager, uint16_t page_id, uint64_t key_size);
void read_index_slot(Pager* pager, uint16_t page_id, uint8_t slot_id, IndexSlotData *slot);
void write_index_slot(Pager* pager, uint16_t page_id, IndexSlotData *slot);
void free_index_slot(Pager* pager, uint16_t page_id, uint8_t slot_id);

/* Lexicographic comparison - NULL < INT < TEXT */
uint64_t encode_int_key(int64_t key);  // Key encoding for lexicographic comparison
int compare_keys(const uint8_t* key1, const uint8_t* key2, size_t key_size);

/* B+ Tree operations */
PSqlStatus btree_split_leaf(Pager* pager, uint16_t leaf_page_id, uint16_t* new_page_id);
PSqlStatus btree_split_internal(Pager* pager, uint16_t internal_page_id, uint16_t* new_page_id);

PSqlStatus btree_insert(Pager* pager, uint16_t root_page_id, const uint8_t* key, size_t key_size, uint16_t data_page_id, uint8_t data_slot_id);
PSqlStatus btree_insert_value(Pager* pager, uint16_t root_page_id, const uint8_t* key, size_t key_size, const uint8_t* value, size_t value_size);  // store as value buffer instead

PSqlStatus btree_search(Pager* pager, uint16_t root_page_id, const uint8_t* key, size_t key_size, uint16_t* result_page_id, uint8_t* result_slot_id);

PSqlStatus btree_delete(Pager* pager, uint16_t root_page_id, const uint8_t* key, size_t key_size);

/* Iterator and range search functions */
BTreeIterator* btree_iterator_create(Pager* pager, uint16_t root_page_id);
BTreeIterator* btree_iterator_range(Pager* pager, uint16_t root_page_id, const uint8_t* start_key, const uint8_t* end_key, size_t key_size);
int btree_iterator_next(BTreeIterator* iterator, uint16_t* data_page_id, uint8_t* data_slot_id);
void btree_iterator_destroy(BTreeIterator* iterator);

PSqlStatus btree_find_row(Pager* pager, uint16_t page_no, const uint8_t* key, uint16_t key_size, uint8_t* value, uint16_t* value_size);
#endif 



