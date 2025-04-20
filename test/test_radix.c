/* Test program for radix tree operations */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include "algorithm/radix_tree.h"

// Shh, no unused warnings, I can't change my callback structure
void print_page(uint16_t page, void* user_data __attribute__((unused))) {
    printf("Page: %u\n", page);
}

int main() {
    printf("Radix Tree Test Program\n");
    printf("=======================\n\n");
    
    // Create a new radix tree
    RadixTree* tree = radix_tree_create();
    if (!tree) {
        fprintf(stderr, "Failed to create radix tree\n");
        return EXIT_FAILURE;
    }
    
    printf("Inserting pages into radix tree...\n");
    
    // Insert some test data
    radix_tree_insert(tree, 100);
    radix_tree_insert(tree, 200);
    radix_tree_insert(tree, 300);
    radix_tree_insert(tree, 400);
    radix_tree_insert(tree, 500);
    
    // Look up some values
    printf("\nLooking up values:\n");
    printf("Page 100: %s\n", radix_tree_lookup(tree, 100) ? "free" : "not free");
    printf("Page 200: %s\n", radix_tree_lookup(tree, 200) ? "free" : "not free");
    printf("Page 300: %s\n", radix_tree_lookup(tree, 300) ? "free" : "not free");
    printf("Page 999: %s\n", radix_tree_lookup(tree, 999) ? "free" : "not free");
    
    // Walk the tree
    printf("\nWalking the tree:\n");
    radix_tree_walk(tree, print_page, NULL);
    
    // Delete a page
    printf("\nDeleting page 300...\n");
    radix_tree_delete(tree, 300);
    printf("Page 300 after deletion: %s\n", radix_tree_lookup(tree, 300) ? "free" : "not free");
    
    // Test pop_min
    printf("\nPopping minimum values:\n");
    uint16_t page;
    while ((page = radix_tree_pop_min(tree)) != 0) {
        printf("Popped page: %u\n", page);
    }
    
    // Test freelist conversion
    printf("\nTesting freelist conversion:\n");
    uint16_t freelist[] = {1000, 2000, 3000, 4000, 5000};
    size_t count = sizeof(freelist) / sizeof(freelist[0]);
    
    freelist_to_radix(tree, freelist, count);
    printf("Converted freelist to radix tree\n");
    
    uint16_t output_freelist[10];
    size_t output_count = radix_to_freelist(tree, output_freelist, 10);
    
    printf("Converted back to freelist, got %zu items:\n", output_count);
    for (size_t i = 0; i < output_count; i++) {
        printf("  %u\n", output_freelist[i]);
    }
    
    // Clean up
    radix_tree_destroy(tree);
    printf("\nRadix tree test completed successfully\n");
    
    return EXIT_SUCCESS;
}
