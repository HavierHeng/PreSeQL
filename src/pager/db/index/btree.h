
/* B+ Tree operations - Technically all Page types are part and parcel of B+ trees, 
 * Internal index and interal leaf just deal with routing and data pointing respectively 
 * B+ Tree operation here usually check the catalog tables - table catalog (for root page), column catalog (for schema), and foreign key catalog (for constraints)
 * */
#include <stdint.h>
#include "index_page.h"  // IndexSlotData
#include "pager/pager.h"  // Pager
#include "pager/db/base/page_format.h"  // DBPage, Row, Cell
#include "status/db.h"  // PSqlStatus

uint16_t btree_init(Pager* pager, const char* table_name, uint8_t index_type); // Create a new B+ Tree index with a primary key. get_free_page() to get a page, init an index internal page to spawn a B+ tree root. Put the entry into the table catalog.

PSqlStatus btree_destroy(Pager* pager, uint16_t root_page); // recursively mark all child nodes as freed by walking through the tree, while adding these changes to the free page radix - increment global count for freed pages. For data and overflow pages, decrement reference counts, clear the slots if unused. If data/overflow page has 0 references left, free apge. Else if still have reference, vaccum and redistribute buckets

PSqlStatus btree_search(Pager* pager, uint16_t root_page, uint8_t* key, uint64_t key_size, Row* out_row);  // Get Row given a key value. use compare_prefix and size of keys to compare in order and traverse B+ Tree

PSqlStatus btree_split_leaf(Pager* pager, uint16_t left_page, uint16_t right_page);  // Parent initiates this call to split - top down logic so we don't have to track parent
PSqlStatus btree_split_internal(Pager* pager, uint16_t left_page, uint16_t right_page);

PSqlStatus btree_insert(Pager* pager, uint16_t root_page, Row* row);  // Given a page of a root B+ Tree index, insert row
//
PSqlStatus btree_insert_row(Pager* pager, const char* table_name, Row* row);  // Insert row into B+ Tree. Finds the correct tables that are affected by this change via table catalog table and then calls btree_insert() on each B+ root 

PSqlStatus btree_delete(Pager* pager, uint16_t root_page, uint8_t* key, uint64_t key_size);   // Delete row from B+ Tree

PSqlStatus btree_delete_row(Pager* pager, const char* table_name, uint8_t* key, uint64_t key_size);  // Deletes row from B+ Tree. Finds the correct tables that are affected via this change via the table catalog table, and btree_delete() on each B+ root.

BTreeIterator* btree_iterator_range(Pager* pager, uint16_t root_page, uint8_t* start_key, uint8_t* end_key, uint64_t key_size); // Performs a traversal search through B+ Tree index based on key. Returns multiple Row results that can be stepped through. 

Row* btree_iterator_next(BTreeIterator* it); // Steps through the Row results

uint64_t encode_int_key(int64_t key);  // For lexicographic comparison. Encodes signed int64_t values and encodes it by adding an offset of 2^63 to remap the range to a positive unsigned value for comparison. Uses XOR trick to x^0x80000

int compare_prefix(uint8_t* key1, uint8_t* key2, uint64_t size);  // Compares keys lexicographically, chaes overflow pages if available
int compare(uint8_t* key1, uint64_t size1, uint8_t* key2, uint64_t size2); 

