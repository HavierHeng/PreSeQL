// Journal handles transactions 
// This is a separate file as having two dynamically growing regions in one db file is too much trouble to keep track of

// Journal Header - on disk

// Journal Page - on disk
typedef struct {
    uint32_t txn_id;        // Transaction this journal entry belongs to
    uint32_t original_page; // The page number being backed up
    uint16_t data_size;     // Optional: size of payload (should usually = PAGE_SIZE)
    uint16_t reserved;      // Alignment / future use
    uint8_t  page_data[PAGE_SIZE]; // Full page backup
} RollbackJournalPage;
