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

#ifndef PRESEQL_PAGER_JOURNAL_FORMAT_H
#define PRESEQL_PAGER_JOURNAL_FORMAT_H

#include <stdint.h>
#include "pager/constants.h"
#include "base/page.h"
#include "data/page.h"
#include "index/page.h"

// Note: Journal is a lil special - the data pages are always 100% filled. i.e no vaccuming nor slots. Only one Index page can make to a Data page, negating the need for a reference counter.
// Meanwhile the index pages function the exact same as before
//
//TODO: A journal page should only be allowed to rollback if transaction id reported by the VM does not have any untracked transactions in between the numbers. otherwise, psql_step() should return invalid operation
//TODO: A journal page also has transaction names in practice - this might need a new B+ table in the journal itself?
// If Journal is implemented, on database opening:
// 1) `mmap()` file - Create a blank journal file with only the header metadata. This serves as a blank canvas for any transactions later on.
// 2) If transaction happens via `BEGIN TRANSACTIONS`: Create the necessary copies of the pages into Journal. This is handled by VM.
// 3) If transaction number returned by the VM is not continguous with the highest transaction number seen by the journal - this invalidate the whole journal. Journal is cleared and reinitialized.


// Journal operations
PSqlStatus journal_init(const char* db_filename);
PSqlStatus journal_close();
PSqlStatus journal_begin_transaction(uint32_t txn_id);
PSqlStatus journal_add_page(uint32_t txn_id, uint32_t page_no, const uint8_t* page_data, uint16_t data_size);
PSqlStatus journal_commit_transaction(uint32_t txn_id);
PSqlStatus journal_rollback_transaction(uint32_t txn_id);
PSqlStatus journal_is_valid_transaction(uint32_t txn_id);

#endif
