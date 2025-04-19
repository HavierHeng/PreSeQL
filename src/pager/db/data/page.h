/* Implements Data pages and slotted page operations */

/* TODO:
 * Data operations:
 * 1) In reality, due to overflow pages, pulling out data is harder than it sounds. it might require the following of a linked list style overflow page. Overflow pages themselves also have chunks, i.e multiple data pages might store chunks of their data inside an overflow page. So you need to way to follow through each overflow page
 * 2) You need to deal with slotted page vaccuming - define vaccum()
 * 3) Also need to deal with storing an overflow data type
*/

// TODO: Fix up each of these operations to match


int insert_row(Page* page, const uint8_t* row_data, uint16_t row_size) {
    DataPageHeader* hdr = &page->header.type_specific.data;

    // Calculate free space
    uint16_t free_start = sizeof(PageHeader); // fixed
    uint16_t free_end = MAX_DATA_BYTES;

    uint16_t slot_area_end = free_start + hdr->num_slots * sizeof(uint16_t);
    uint16_t data_area_start = hdr->slot_directory_offset;

    uint16_t required = row_size + sizeof(uint16_t); // space for row + new slot

    // Find a reusable slot - linear search
    int reusable_slot = -1;
    for (int i = 0; i < hdr->num_slots; i++) {
        uint16_t* slot = (uint16_t*)(page->payload + free_start + i * sizeof(uint16_t));
        if (*slot == SLOT_DELETED) {
            reusable_slot = i;
            break;
        }
    }

    if (data_area_start < slot_area_end + row_size) {
        // Not enough space
        return -1;
    }

    // Allocate space
    data_area_start -= row_size;
    memcpy(page->payload + data_area_start, row_data, row_size);

    if (reusable_slot >= 0) {
        // Reuse old slot
        uint16_t* slot = (uint16_t*)(page->payload + free_start + reusable_slot * sizeof(uint16_t));
        *slot = data_area_start;
    } else {
        // Add new slot
        uint16_t* slot = (uint16_t*)(page->payload + free_start + hdr->num_slots * sizeof(uint16_t));
        *slot = data_area_start;
        hdr->num_slots += 1;
    }

    hdr->slot_directory_offset = data_area_start;
    page->header.dirty = 1;
    return 0;
}


int delete_row(Page* page, int slot_index) {
    DataPageHeader* hdr = &page->header.type_specific.data;

    if (slot_index >= hdr->num_slots) return -1;

    uint16_t* slot = (uint16_t*)(page->payload + sizeof(PageHeader) + slot_index * sizeof(uint16_t));
    if (*slot == SLOT_DELETED) return -1;

    *slot = SLOT_DELETED;
    page->header.dirty = 1;
    return 0;
}


uint8_t* get_row(Page* page, int slot_index, uint16_t* out_row_size) {
    DataPageHeader* hdr = &page->header.type_specific.data;

    if (slot_index >= hdr->num_slots) return NULL;

    uint16_t* slot = (uint16_t*)(page->payload + sizeof(PageHeader) + slot_index * sizeof(uint16_t));
    if (*slot == SLOT_DELETED) return NULL;

    uint16_t offset = *slot;

    // Guess row size (distance to next slot or to end of page)
    uint16_t next_offset = MAX_DATA_BYTES;
    for (int i = 0; i < hdr->num_slots; i++) {
        uint16_t* s = (uint16_t*)(page->payload + sizeof(PageHeader) + i * sizeof(uint16_t));
        if (*s != SLOT_DELETED && *s > offset && *s < next_offset) {
            next_offset = *s;
        }
    }

    if (out_row_size) *out_row_size = next_offset - offset;
    return page->payload + offset;
}

