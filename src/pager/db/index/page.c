#include "page.h"
#include "page_format.h"


/* TODO: Implement B+ Tree operations here
* B+ Tree operations:
* 1) init/destroy - to create and clean up B+ Tree nodes - implicitly part of init data page (a page is a node) - so maybe no need make - probs just need to implement a Radix tree way to find either a freed page or fallback to highest page ID + 1 (stored in header as metadata)
* 2) btree_search() - to find entry
* 3) btree_insert() - to add new rows to the B+ Tree index
* 4) btree_split_leaf() - when leaf is too full, we have to split it. This also sets up the right sibling pointers that B+ Tree is known for fast linear access.
* 5) btree_split_interal() - when internal is too small. Its a separate function just due to how internal nodes are routers instead of data pointers.
* 6) btree_iterator_range() - returns a BTreeIterator (or something similar in idea, name needs to be more consistent), allowing me to get range of values
* 7) Row* btree_iterator_next(BPlusIterator *it) - steps through the iterator and returns the row stored
* 8) btree_delete() - to delete entries. This is optional because of the complexity to rebalance the tree after operations, but might be useful. Also when entries are deleted, this also updates some reference counter, updates the free page radix tree and so on. Its a cascade effect.
*/

/* Searching
* Start from root - find appropriate leaf node to insert key
* This can be done by comparing key with key in current node - if key is less than a key in the node, follow child pointer, else if key is greater, move to next key or child pointer
* Cotinue process until reached leaf node
* Look for the key in the leaf node (this key points to a data page no.)
*/

/* Insertion
* Start from root - find appropraite leaf node to insert key
* Insert key in leaf node in sorted order
* If the leaf exceeds max order (> m-1) - split the node by creating a new parent internal node. Promote middle key to parent node.
* If parent node also overflows, repeat splitting process up to root
* Link leaf nodes as right siblings after split
* /

/* Deletion - (this one is hard
* Start from root - find leaf node containing key
* Remove the key from leaf node
* Merge of redistribute nodes:
*   If leaf node < m/2 keys after deletion, merge or redistribute nodes
*   If merging remove the corresponding key from parent and merge nodes
*   If paarent node underflows, repeat process up to root
*/

/* In-order traversal
* Start at leftmost leaf node
* Vist keys in current leaf node - and save
* Move to next leaf node via a right sibling pointer - save keys 
* Repeat by following right sibling pointers until the pointer is null
*/

/* Range query
* Start from root and find the leaf node containing the starting key of the range (can use search)
* Visit each key in the current leaf node tht falls within the range
* follow the pointer to the next right sibling and repeat until all the keys in the range are found
*/

