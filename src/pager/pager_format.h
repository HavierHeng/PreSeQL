#ifndef PRESEQL_PAGER_FORMAT_H
#define PRESEQL_PAGER_FORMAT_H

#include <stdint.h>
#include <stdlib.h>
#include "constants.h"
#include "types.h"

/* Page initialization functions */
DBPage* init_index_internal_page(Pager* pager, uint16_t page_no);
DBPage* init_index_leaf_page(Pager* pager, uint16_t page_no);
DBPage* init_overflow_page(Pager* pager, uint16_t page_no);
DBPage* init_data_page(Pager* pager, uint16_t page_no);

/* Memory mapping functions */
void* resize_mmap(int fd, void* old_map, size_t old_size, size_t new_size);
uint16_t allocate_new_db_page(Pager* pager);
uint16_t allocate_new_db_pages(Pager* pager, size_t num_pages);
uint16_t allocate_new_journal_page(Pager* pager);
uint16_t allocate_new_journal_pages(Pager* pager, size_t num_pages);

#endif /* PRESEQL_PAGER_FORMAT_H */

