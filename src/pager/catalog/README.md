Catalog pages are special pages implemented as real B+ Tree backed Index tables.

The only difference is that their initialization is hardcoded into the database engine - such that whenever it boots it calls all the 3 initialization methods to immediately populate the database with these values.

