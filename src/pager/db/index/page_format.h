/* Anything related to the B+ Tree Index */

typedef enum {
    BTREE_ROOT = 1,
    BTREE_INTERIOR,
    BTREE_LEAF
} BTreePageType;

// TODO: There will only be a max of 65535 pages - i.e 16 bits - hence page_id will never exceed that amount
// TODO : all B+ Tree Leafs are linked in order of the primary key to allow linear traversal - so there should be a right sibling pointer
// TODO: Implement the functions to insert, split, delete and so on
typedef struct {
    BTreePageType btree_type;
    uint16_t free_start;
    uint16_t free_end;
    uint16_t total_free;
    uint8_t flags;
} BTreePageHeader;

