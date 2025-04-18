# Arena Allocation

## Attribution to original author
Code comes from Tsoding's one header file [Arena Allocator](https://github.com/tsoding/arena/tree/master).

## Description
The motivation behind using an arena allocator is due to the complex nature of the database structs, with lots of nesting and large pages being allocated at once.
As such, for any structs that are not on disk (not `mmap`), but rather in memory, a better way to manage them is via an Arena allocator.
This allows for easy allocation and freeing of huge chunks of nested memory with similar lifetimes:
- e.g Radix Tree in memory for speeding up finding free pages in the main database file and journal file
- e.g VM with its many attributes and structs are hard to manage without a scratch memory region that can be destroyed on exit.

The Arena allocator by default is using libc `malloc()` to allocate large chunks of memory. 
But it can also use the POSIX style anonymous `mmap()` on Linux and MacOS.

