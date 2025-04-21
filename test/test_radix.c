/* Test program for radix tree operations */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include "algorithm/radix_tree.h"

// Shh, no unused warnings, I can't change my callback structure
void print_page(uint16_t page, void* user_data __attribute__((unused))) {
    printf("Free page: %u\n", page);
}

int main() {
    
    // Create a new radix tree
    RadixTree* tree = radix_tree_create();
    if (!tree) {
        fprintf(stderr, "Failed to create radix tree\n");
        return EXIT_FAILURE;
    }
    
    printf("\nInserting page test (100, 200, 300, 400, 500):\n");
    
    // Insert some free pages
    radix_tree_insert(tree, 100);
    radix_tree_insert(tree, 200);
    radix_tree_insert(tree, 300);
    radix_tree_insert(tree, 400);
    radix_tree_insert(tree, 500);
    
    // Check if pages are in the tree
    printf("\nChecking pages:\n");
    printf("Page 100: %s\n", radix_tree_lookup(tree, 100) ? "free" : "not free");
    printf("Page 200: %s\n", radix_tree_lookup(tree, 200) ? "free" : "not free");
    printf("Page 300: %s\n", radix_tree_lookup(tree, 300) ? "free" : "not free");
    printf("Page 999: %s\n", radix_tree_lookup(tree, 999) ? "free" : "not free");
    
    // Peek current minimum entry
    printf("Current minimum page: %u", radix_tree_peek_min(tree));

    // List all free pages
    printf("\nListing all free pages:\n");
    radix_tree_walk(tree, print_page, NULL);
    
    // Delete a page (mark as used)
    printf("\nMarking page 300 as used...\n");
    radix_tree_delete(tree, 300);
    printf("Page 300 after deletion: %s\n", radix_tree_lookup(tree, 300) ? "free" : "not free");
    
    // Walk the tree again to verify deletion
    printf("\nFree pages after deletion:\n");
    radix_tree_walk(tree, print_page, NULL);
    
    // Get free pages in order (smallest first)
    printf("\nGetting free pages in order:\n");
    uint16_t page;
    while ((page = radix_tree_pop_min(tree)) != 0) {
        printf("Got free page: %u\n", page);
    }
    
    // Verify tree is empty
    printf("\nVerifying tree is empty:\n");
    radix_tree_walk(tree, print_page, NULL);
    
    // Test freelist conversion
    printf("\nTesting freelist conversion:\n");
    uint16_t freelist[] = {666, 1337, 69, 420, 44100};
    size_t count = sizeof(freelist) / sizeof(freelist[0]);
    
    freelist_to_radix(tree, freelist, count);
    printf("Converted freelist to radix tree\n");
    
    printf("\nFree pages after freelist conversion:\n");
    radix_tree_walk(tree, print_page, NULL);
    
    // Convert back to freelist
    uint16_t output_freelist[10];
    size_t output_count = radix_to_freelist(tree, output_freelist, 10);
    
    printf("\nConverted back to freelist, got %zu items:\n", output_count);
    for (size_t i = 0; i < output_count; i++) {
        printf("  %u\n", output_freelist[i]);
    }
    
    // Clean up
    radix_tree_destroy(tree);
    printf("\nRadix tree test completed successfully\n");
    
    return EXIT_SUCCESS;
}
