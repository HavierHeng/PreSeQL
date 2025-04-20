# Radix Tree

## Motivation

Radix tree is for finding and freeing free pages quickly. It works by acting as a compressed trie, rather than storing unnecessary edges for nodes that has no children, it instead opts to merge them into a prefix of more than 1 character. 

In essence, I'm using it as a compacted min-heap (it reduces the space that a binary tree based min-heap would otherwise take). Everytime I free a page, I put the 16 bit page number into the prefix tree, and it will sort itself such that I can pop the minimum instantly the next time.

In our case, the inputs to the Radix tree are binaries representing the page number. (we have up to 65535 pages - 16 bits).

This is used in a few parts of the code to reuse pages to prevent fragmentation: 
1) Finding Free pages in Pager 
2) Free Pages in Journal if Transaction is used - e.g when pages are dirty, we make a copy into a journal for rollback
3) Sorting Data and Overflow Pages into 3 buckets representing how full they are (FULL, MOSTLY_FULL, MOSTLY_FREE), so that Pages can be quickly found that can still accomodate more slots/data.

This is as pages in the database can get huge in number, and is dynamic in count.
This job cannot be handled by just a bitmap alone since it scales badly to search the high number of 65535 pages for the lowest available page.

> Note: At first I wanted to use a 4 bucket Radix tree to get the smallest chunk that fits as well. But thinking back, that is stupid, since the problem definition is that an overflow page has max 255 chunks. The cost of using a radix tree is lost since linear scan on 255 entries is fast, and the overhead of using radix tree means i have to copy to memory from disk, and dealing with desync issues. I could just scan the chunk entries from disk directly instead.

## Why not just use a Queue to track free pages?
Ah ha, good catch. Queue would give me pages to reuse in order of freeing rather than in the lowest page number.

Its the only reason why I'm bothering to sort the free pages is for:
1. Locality of Reference (Cache Performance)
    - Lower-numbered pages tend to be near each other.
    - Access patterns that stay within a small range of pages are more cache- and disk-friendly (especially if you're using memory-mapped I/O or SSD read-ahead).

2. Fragmentation Control
    - Allocating low pages first can minimize fragmentation in your file — free space tends to stay near the end.
    - That means the database file grows in chunks instead of swiss cheese scattered through the file.

3. Better File Growth Behavior
    - If you allocate higher-numbered pages early, your file might "look" bigger than it is — wasting disk space even if only some of it is used.

4. Debuggability / Simplicity
    - Sequential allocation makes files easier to understand and debug in tools like hex editors.


## Choice of RADIX_BITS based on some maffs

For 16-bit page numbers, we have a few configurations. We can evaluate their efficiency and space complexity in memory.

One way is to use 8-bit stride (RADIX_BITS = 8), which gives us only 2 levels. Each `RadixNode` contains 256 child pointers - which will be 8 bytes per pointer on 64-bit system = 2048 bytes and 1 Status integer (4 bytes), with a gran total of ~2052 bytes per node.
- Level 1: 1 node
- Level 2: Up to 256 nodes
- Max total nodes: 257
- Max size: 2052 bytes * 257 = approx 527KB

Another way is to use 4-bit strides (RADIX_BITS = 4), which gives us 4 levels. Each `RadixNode` contains 16 child pointers - approx 16 x 8 bytes = 128 bytes and 1 Status of 4 bytes, with a total of ~132 bytes per node. 
- Level 1: 1
- Level 2: up to 16 nodes
- Level 3: up to 256 nodes
- Level 4: up to 4096 nodes
- Max total nodes at worst case: 4369 nodes
- Max memory: ~576KB

Generally, if we have a smaller stride, memory efficiency is smaller, and can better fit into cache. But in exchange, the search takes slightly longer due to the increased height of the trees. Smaller nodes do save more space for sparse trees. 

The choice is to use a RADIX_BITS=4 then, since its a decent balance.
