
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

#include "pager/db/db_format.h"
#include "types/psql_types.h"
#include <stdint.h>
#include <string.h>

#define PAGE_SIZE 4096
#define PREFIX_SIZE 16
#define ENTRY_SIZE 24 // 16 prefix + 4 overflow_ptr + 4 pointer
#define MAX_ENTRIES (PAGE_SIZE / ENTRY_SIZE) // ~170
#define MIN_ENTRIES (MAX_ENTRIES / 2) // ~85

// Type tags for data page
#define TYPE_NULL 0x00
#define TYPE_INT 0x01
#define TYPE_TEXT 0x02

// Node types
#define NODE_INTERNAL 0
#define NODE_LEAF 1

// Pointer types
typedef uint32_t page_id_t; // Page ID for index/data/overflow pages
typedef uint32_t slot_id_t; // Slot offset in data page

// B+ tree node entry
typedef struct {
    uint8_t prefix[PREFIX_SIZE]; // 16-byte prefix
    page_id_t overflow_ptr; // Overflow page ID (0 if none)
    union {
        page_id_t child_ptr; // Internal node: child page
        struct {
            page_id_t page_id; // Leaf node: data page ID
            slot_id_t slot_id; // Slot offset
        } value_ptr; // Combined for simplicity
    };
} NodeEntry;

// B+ tree node (index page)
// TODO: This would be the whole DBPage in practice
typedef struct Node {
    uint8_t type; // NODE_INTERNAL or NODE_LEAF
    uint16_t num_entries; // Number of entries
    page_id_t right_sibling; // Leaf nodes: next leaf (0 if none)
    NodeEntry entries[MAX_ENTRIES];
} Node;

// B+ tree structure
typedef struct {
    page_id_t root_id; // Root page ID
    // Assume page storage (in-memory for simplicity)
    Node* pages[1000]; // Map page_id to Node*
    uint32_t next_page_id;
} BPlusTree;

typedef struct {
    PSqlDataTypes* type; // TYPE_NULL, TYPE_INT, TYPE_TEXT
    uint8_t** values;  // List of values - since values can be any sized
    uint64_t* sizes;  // Size in bytes
    OverflowPointer* overflow_pointers;  // If returned row has overflows
} Row;

// Iterator for range queries
typedef struct {
    BPlusTree* tree;
    page_id_t current_page;
    uint16_t current_entry;
    uint8_t end_prefix[PREFIX_SIZE]; // End of range (inclusive)
    int has_end; // 1 if range has end
} BPlusIterator;

// Page storage (simplified in-memory)
Node* allocate_page(BPlusTree* tree) {
    Node* node = (Node*)calloc(1, PAGE_SIZE);
    tree->pages[tree->next_page_id] = node;
    tree->next_page_id++;
    return node;
}

Node* get_page(BPlusTree* tree, page_id_t page_id) {
    return tree->pages[page_id];
}

// Encode INTEGER key (2^63 offset)
// Use of an XOR trick - since two's complement means negative values start with a bit of 1, doing an XOR with 0x80000000... will flip the value
// In effect, we remap from (-2^63) - (2^63-1) to 0 - (2^64-1)
uint64_t encode_int_key(int64_t value) {
    return (uint64_t)value ^ 0x8000000000000000ULL;
}

// Compare prefixes (lexicographic)
int compare_prefix(const uint8_t* prefix1, const uint8_t* prefix2) {
    return memcmp(prefix1, prefix2, PREFIX_SIZE);
}

// Compare entries (including overflow for TEXT)
int compare(BPlusTree* tree, const NodeEntry* entry1, const NodeEntry* entry2) {
    // Compare directly lexicographically - logic works on encoded int and null as well
    int prefix_cmp = compare_prefix(entry1->prefix, entry2->prefix);
    if (prefix_cmp != 0) return prefix_cmp;

    // Prefix comparison is insufficent to tell if equal - check for overflow pointers
    // Both no overflow pointer - actually equal in value - logic works on encoded int and null too
    if (entry1->overflow_ptr == 0 && entry2->overflow_ptr == 0) return 0;

    // If first entry has an overflow pointer - first entry is always going to be larger in value lexicographically - return first > second (-1)
    if (entry1->overflow_ptr == 0) return -1;

    // If second entry has an overflow pointer - second entry is always going to be larger in value lexicographically - return second > first (1)
    if (entry2->overflow_ptr == 0) return 1;
    // Assume overflow stores TEXT remainder (simplified)
    char* full_key1 = (char*)get_page(tree, entry1->overflow_ptr);
    char* full_key2 = (char*)get_page(tree, entry2->overflow_ptr);
    return strcmp(full_key1, full_key2);
}

BPlusTree* btree_init() {
    BPlusTree* tree = (BPlusTree*)malloc(sizeof(BPlusTree));
    tree->root_id = 0;
    tree->next_page_id = 1;
    memset(tree->pages, 0, sizeof(tree->pages));
    
    // Create root (initially a leaf)
    Node* root = allocate_page(tree);
    root->type = NODE_LEAF;
    root->num_entries = 0;
    root->right_sibling = 0;
    tree->root_id = tree->next_page_id - 1;
    return tree;
}

// 1) Destroy B+ tree
void btree_destroy(BPlusTree* tree) {
    for (uint32_t i = 1; i < tree->next_page_id; i++) {
        if (tree->pages[i]) free(tree->pages[i]);
    }
    free(tree);
}

/* Searching
* Start from root - find appropriate leaf node to insert key
* This can be done by comparing key with key in current node - if key is less than a key in the node, follow child pointer, else if key is greater, move to next key or child pointer
* Cotinue process until reached leaf node
* Look for the key in the leaf node (this key points to a data page no.)
*/
NodeEntry* btree_search(BPlusTree* tree, const uint8_t* prefix, page_id_t* page_id, uint16_t* entry_idx) {
    Node* node = get_page(tree, tree->root_id);
    while (node) {
        if (node->type == NODE_LEAF) {
            for (uint16_t i = 0; i < node->num_entries; i++) {
                int cmp = compare_prefix(prefix, node->entries[i].prefix);
                if (cmp == 0) {
                    *page_id = tree->root_id;
                    *entry_idx = i;
                    return &node->entries[i];
                }
                if (cmp < 0) break;
            }
            return NULL; // Not found
        } else {
            // Internal node: find child
            for (uint16_t i = 0; i < node->num_entries; i++) {
                if (compare_prefix(prefix, node->entries[i].prefix) < 0) {
                    node = get_page(tree, node->entries[i].child_ptr);
                    break;
                }
            }
            // Last child if prefix >= all keys
            node = get_page(tree, node->entries[node->num_entries].child_ptr);
        }
    }
    return NULL;
}

// Split a leaf node
void btree_split_leaf(BPlusTree* tree, Node* leaf, page_id_t leaf_id) {
    Node* new_leaf = allocate_page(tree);
    page_id_t new_leaf_id = tree->next_page_id - 1;
    new_leaf->type = NODE_LEAF;
    
    // Split entries (roughly half)
    uint16_t split_idx = leaf->num_entries / 2;
    new_leaf->num_entries = leaf->num_entries - split_idx;
    memcpy(new_leaf->entries, leaf->entries + split_idx, new_leaf->num_entries * ENTRY_SIZE);
    leaf->num_entries = split_idx;
    
    // Set right sibling
    new_leaf->right_sibling = leaf->right_sibling;
    leaf->right_sibling = new_leaf_id;
    
    // Update parent
    Node* parent = get_page(tree, leaf_id); // Simplified: assume parent known
    if (!parent) {
        // Create new root
        parent = allocate_page(tree);
        parent->type = NODE_INTERNAL;
        tree->root_id = tree->next_page_id - 1;
    }
    
    // Insert new key and pointer into parent
    NodeEntry new_entry;
    memcpy(new_entry.prefix, new_leaf->entries[0].prefix, PREFIX_SIZE);
    new_entry.overflow_ptr = 0;
    new_entry.child_ptr = new_leaf_id;
    
    uint16_t i;
    for (i = 0; i < parent->num_entries; i++) {
        if (compare(tree, &new_entry, &parent->entries[i]) < 0) break;
    }
    memmove(&parent->entries[i + 1], &parent->entries[i], (parent->num_entries - i) * ENTRY_SIZE);
    parent->entries[i] = new_entry;
    parent->num_entries++;
    
    if (parent->num_entries > MAX_ENTRIES) {
        btree_split_internal(tree, parent, tree->root_id);
    }
}

// 5) Split an internal node
void btree_split_internal(BPlusTree* tree, Node* internal, page_id_t internal_id) {
    Node* new_internal = allocate_page(tree);
    page_id_t new_internal_id = tree->next_page_id - 1;
    new_internal->type = NODE_INTERNAL;
    
    // Split entries
    uint16_t split_idx = internal->num_entries / 2;
    new_internal->num_entries = internal->num_entries - split_idx - 1;
    memcpy(new_internal->entries, internal->entries + split_idx + 1, new_internal->num_entries * ENTRY_SIZE);
    internal->num_entries = split_idx;
    
    // Middle key goes to parent
    NodeEntry middle;
    memcpy(&middle, &internal->entries[split_idx], ENTRY_SIZE);
    
    // Update parent
    Node* parent = get_page(tree, internal_id); // Simplified
    if (!parent) {
        parent = allocate_page(tree);
        parent->type = NODE_INTERNAL;
        tree->root_id = tree->next_page_id - 1;
    }
    
    uint16_t i;
    for (i = 0; i < parent->num_entries; i++) {
        if (compare(tree, &middle, &parent->entries[i]) < 0) break;
    }
    memmove(&parent->entries[i + 1], &parent->entries[i], (parent->num_entries - i) * ENTRY_SIZE);
    parent->entries[i] = middle;
    parent->entries[i].child_ptr = new_internal_id;
    parent->num_entries++;
    
    if (parent->num_entries > MAX_ENTRIES) {
        btree_split_internal(tree, parent, tree->root_id);
    }
}

/* Insertion
* Start from root - find appropraite leaf node to insert key
* Insert key in leaf node in sorted order
* If the leaf exceeds max order (> m-1) - split the node by creating a new parent internal node. Promote middle key to parent node.
* If parent node also overflows, repeat splitting process up to root
* Link leaf nodes as right siblings after split
*/
void btree_insert(BPlusTree* tree, const uint8_t* prefix, page_id_t overflow_ptr, page_id_t data_page_id, slot_id_t slot_id) {
    Node* node = get_page(tree, tree->root_id);
    
    // Traverse to leaf
    while (node->type == NODE_INTERNAL) {
        uint16_t i;
        for (i = 0; i < node->num_entries; i++) {
            if (compare_prefix(prefix, node->entries[i].prefix) < 0) break;
        }
        node = get_page(tree, node->entries[i].child_ptr);
    }
    
    // Insert into leaf
    NodeEntry new_entry;
    memcpy(new_entry.prefix, prefix, PREFIX_SIZE);
    new_entry.overflow_ptr = overflow_ptr;
    new_entry.value_ptr.page_id = data_page_id;
    new_entry.value_ptr.slot_id = slot_id;
    
    uint16_t i;
    for (i = 0; i < node->num_entries; i++) {
        int cmp = compare(tree, &new_entry, &node->entries[i]);
        if (cmp == 0 && memcmp(prefix, "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0", PREFIX_SIZE) == 0) {
            // NULL before -2^63
            if (node->entries[i].value_ptr.page_id != 0) break;
        }
        if (cmp <= 0) break;
    }
    
    memmove(&node->entries[i + 1], &node->entries[i], (node->num_entries - i) * ENTRY_SIZE);
    node->entries[i] = new_entry;
    node->num_entries++;
    
    if (node->num_entries > MAX_ENTRIES) {
        btree_split_leaf(tree, node, tree->root_id);
    }
}

/* Range query (in-order traversal using right sibling pointer)
* Start from root and find the leaf node containing the starting key of the range (can use search)
* Visit each key in the current leaf node tht falls within the range
* follow the pointer to the next right sibling and repeat until all the keys in the range are found
*/
BPlusIterator* btree_iterator_range(BPlusTree* tree, const uint8_t* start_prefix, const uint8_t* end_prefix) {
    BPlusIterator* it = (BPlusIterator*)malloc(sizeof(BPlusIterator));
    it->tree = tree;
    it->current_page = 0;
    it->current_entry = 0;
    it->has_end = (end_prefix != NULL);
    if (it->has_end) {
        memcpy(it->end_prefix, end_prefix, PREFIX_SIZE);
    }
    
    // Find first leaf with start_prefix
    Node* node = get_page(tree, tree->root_id);
    while (node->type == NODE_INTERNAL) {
        uint16_t i;
        for (i = 0; i < node->num_entries; i++) {
            if (compare_prefix(start_prefix, node->entries[i].prefix) < 0) break;
        }
        node = get_page(tree, node->entries[i].child_ptr);
    }
    
    // Find first entry >= start_prefix
    for (uint16_t i = 0; i < node->num_entries; i++) {
        if (compare_prefix(start_prefix, node->entries[i].prefix) <= 0) {
            it->current_page = tree->root_id;
            it->current_entry = i;
            return it;
        }
    }
    
    // No entries in range
    it->current_page = 0;
    return it;
}

// 7) Get next row from iterator
Row* btree_iterator_next(BPlusIterator* it) {
    if (!it->current_page) return NULL;
    
    Node* node = get_page(it->tree, it->current_page);
    if (it->current_entry >= node->num_entries) {
        // Move to next leaf
        it->current_page = node->right_sibling;
        it->current_entry = 0;
        if (!it->current_page) return NULL;
        node = get_page(it->tree, it->current_page);
    }
    
    // Check if past end_prefix
    if (it->has_end && compare_prefix(node->entries[it->current_entry].prefix, it->end_prefix) > 0) {
        it->current_page = 0;
        return NULL;
    }
    
    // Simplified: Return row from data page
    NodeEntry* entry = &node->entries[it->current_entry];
    Row* row = (Row*)malloc(sizeof(Row));
    // Assume data page access (mock)
    row->type = TYPE_INT; // Simplified
    row->int_value = 0; // Mock: fetch from data page
    it->current_entry++;
    return row;
}

/* Deletion - (this one is harder due to redistribution)
* Start from root - find leaf node containing key
* Remove the key from leaf node
* Merge of redistribute nodes:
*   If leaf node < m/2 keys after deletion, merge or redistribute nodes
*   If merging remove the corresponding key from parent and merge nodes
*   If paarent node underflows, repeat process up to root
*/
void btree_delete(BPlusTree* tree, const uint8_t* prefix) {
    Node* node = get_page(tree, tree->root_id);
    Node* parent = NULL;
    uint16_t parent_idx = 0;
    
    // Traverse to leaf
    while (node->type == NODE_INTERNAL) {
        parent = node;
        uint16_t i;
        for (i = 0; i < node->num_entries; i++) {
            if (compare_prefix(prefix, node->entries[i].prefix) < 0) break;
        }
        parent_idx = i;
        node = get_page(tree, node->entries[i].child_ptr);
    }
    
    // Find and delete entry
    uint16_t i;
    for (i = 0; i < node->num_entries; i++) {
        if (compare_prefix(prefix, node->entries[i].prefix) == 0) {
            memmove(&node->entries[i], &node->entries[i + 1], (node->num_entries - i - 1) * ENTRY_SIZE);
            node->num_entries--;
            break;
        }
    }
    
    // Rebalance if underflow
    if (node->num_entries < MIN_ENTRIES && parent) {
        // Simplified: Merge with sibling or borrow
        Node* sibling = get_page(tree, parent->entries[parent_idx + 1].child_ptr);
        if (sibling && sibling->num_entries > MIN_ENTRIES) {
            // Borrow from right sibling
            node->entries[node->num_entries] = sibling->entries[0];
            node->num_entries++;
            memmove(sibling->entries, sibling->entries + 1, (sibling->num_entries - 1) * ENTRY_SIZE);
            sibling->num_entries--;
            // Update parent key
            memcpy(parent->entries[parent_idx].prefix, sibling->entries[0].prefix, PREFIX_SIZE);
        } else {
            // Merge with right sibling
            memcpy(&node->entries[node->num_entries], sibling->entries, sibling->num_entries * ENTRY_SIZE);
            node->num_entries += sibling->num_entries;
            node->right_sibling = sibling->right_sibling;
            // Remove sibling from parent
            memmove(&parent->entries[parent_idx], &parent->entries[parent_idx + 1], (parent->num_entries - parent_idx - 1) * ENTRY_SIZE);
            parent->num_entries--;
            free(sibling);
        }
    }
}

#endif
