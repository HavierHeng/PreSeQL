/* Anything related to the data pages
 * Free slots in the data pages are tracked via means of bitmaps rather than radix tree, its a fixed small number of slots (<256), 
 * so its smaller to store, and the penalty from searching linearly is not that bad
 */

#define MAX_DATA_SLOTS 256  // Max number of slots per data page

typedef struct {
    uint16_t num_slots;
    uint16_t slot_directory_offset;
    uint16_t reference_count;  // If secondary indexes are made, knowing no. of references to the data page will indicate when we can mark the page as free
} DataPageHeader;


