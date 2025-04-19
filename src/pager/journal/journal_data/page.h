/* Journal handles transactions - e.g when BEGIN TRANSACTION is performed
* This is a separate file from the main db file
* as having two dynamically growing regions in one db file 
* is too much trouble to keep track of.
* 
* Journal creates a copy of pages that have been modified during transaction
* It only clears its journal of modified pages on COMMIT
* i.e the client is certain of changes
* ROLLBACK can allow for copying back the original data back the pages
*/

#ifndef PRESEQL_PAGER_JOURNAL_DATA_PAGE_H
#define PRESEQL_PAGER_JOURNAL_DATA_PAGE_H

#include <stdint.h>
#include <stdlib.h>
#include "page_format.h"
#include "../../status/db.h"

// Journal operations
PSqlStatus journal_init(const char* db_filename);
PSqlStatus journal_close();
PSqlStatus journal_begin_transaction(uint32_t txn_id);
PSqlStatus journal_add_page(uint32_t txn_id, uint32_t page_no, const uint8_t* page_data, uint16_t data_size);
PSqlStatus journal_commit_transaction(uint32_t txn_id);
PSqlStatus journal_rollback_transaction(uint32_t txn_id);
PSqlStatus journal_is_valid_transaction(uint32_t txn_id);

#endif /* PRESEQL_PAGER_JOURNAL_DATA_PAGE_H */


