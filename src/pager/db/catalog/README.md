Catalog pages are special pages implemented as real B+ Tree backed Index tables.

The only difference is that their initialization is hardcoded into the database engine - such that whenever it boots it calls all the 3 initialization methods to immediately populate the database with indexes based on their primary `id` keys. 

After this bootstrap initialization, secondary keys for the catalog tables are also created to speed up certain types of searches. 

All other non-system tables relying on these catalog indexes can then be created.
