# Radix Tree

## Motivation

Radix tree is for finding and freeing free pages quickly. It works by acting as a compressed trie, rather than storing unnecessary edges for nodes that has no children, it instead opts to merge them into a prefix of more than 1 character. 

In essence, I'm using it as a compacted min-heap (it reduces the space that a binary tree based min-heap would otherwise take). Everytime I free a page, I put the 16 bit page number into the prefix tree, and it will sort itself such that I can pop the minimum instantly the next time.

In our case, the inputs to the Radix tree are binaries representing the page number. (we have up to 65535 pages - 16 bits).

This is used in a few parts of the code to reuse pages to prevent fragmentation: 
1) Finding Free pages in Pager 
2) Free Pages in Journal if Transaction is used - e.g when pages are dirty, we make a copy into a journal for rollback


This is as pages in the database can get huge in number, and is dynamic in count.
This job cannot be handled by just a bitmap alone since it scales badly to search the high number of 65535 pages for the lowest available page.


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
