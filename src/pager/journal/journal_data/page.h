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

// Journal Header - on disk
#include "page_format.h"


