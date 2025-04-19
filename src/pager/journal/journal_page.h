/* Journal Page - on disk */


//TODO: A journal page should only be allowed to rollback if transaction id reported by the VM does not have any untracked transactions in between the numbers. otherwise, psql_step() should return invalid operation
//TODO: A journal page also has transaction names in practice - this might need a new B+ table in the journal itself?
// If Journal is implemented, on database opening:
// 1) `mmap()` file - Create a blank journal file with only the header metadata. This serves as a blank canvas for any transactions later on.
// 2) If transaction happens via `BEGIN TRANSACTIONS`: Create the necessary copies of the pages into Journal. This is handled by VM.
// 3) If transaction number returned by the VM is not continguous with the highest transaction number seen by the journal - this invalidate the whole journal. Journal is cleared and reinitialized.

typedef struct {
    uint32_t txn_id;        // Transaction this journal entry belongs to
    uint32_t original_page; // The page number being backed up
    uint16_t data_size;     // Optional: size of payload (should usually = PAGE_SIZE)
    uint16_t reserved;      // Alignment / future use
    uint16_t highest_page;  /* By tracking the highest page allocated, 
                            speeds up the future allocation if no free pages to be reused */
    uint8_t  page_data[MAX_DATA_BYTES]; // Full page backup
} RollbackJournalPage;

