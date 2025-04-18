# Radix Tree

## Motivation

Radix tree is for finding and freeing free pages in O(nlogn) time
This is used in a few parts of the code to reuse pages to prevent fragmentation: 
1) Finding Free pages in Pager 
2) Free Pages in Journal if Transaction is used - e.g when pages are dirty, we make a copy into a journal for rollback


This is as pages in the database can get huge in number, and is dynamic in count.
This job cannot be handled by just a bitmap alone since it scales badly to search the high number of 65535 pages for the lowest available page.


