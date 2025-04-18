/* Journal Page - on disk */

typedef struct {
    uint32_t txn_id;        // Transaction this journal entry belongs to
    uint32_t original_page; // The page number being backed up
    uint16_t data_size;     // Optional: size of payload (should usually = PAGE_SIZE)
    uint16_t reserved;      // Alignment / future use
    uint8_t  page_data[MAX_DATA_BYTES]; // Full page backup
} RollbackJournalPage;
