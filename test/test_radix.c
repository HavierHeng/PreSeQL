/* Test program for radix tree operations */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include "algorithm/radix_tree.h"

void print_page_slot(uint16_t page, int16_t slot, void* user_data) {
    printf("Page: %u, Slot: %d\n", page, slot);
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
    radix_tree_insert(tree, 100, 1);
    radix_tree_insert(tree, 200, 2);
    radix_tree_insert(tree, 300, 3);
    radix_tree_insert(tree, 400, 4);
    radix_tree_insert(tree, 500, 5);
    
    // Look up some values
    printf("\nLooking up values:\n");
    printf("Page 100: slot %d\n", radix_tree_lookup(tree, 100));
    printf("Page 200: slot %d\n", radix_tree_lookup(tree, 200));
    printf("Page 300: slot %d\n", radix_tree_lookup(tree, 300));
    printf("Page 999: slot %d (should be -1 if not found)\n", radix_tree_lookup(tree, 999));
    
    // Walk the tree
    printf("\nWalking the tree:\n");
    radix_tree_walk(tree, print_page_slot, NULL);
    
    // Delete a page
    printf("\nDeleting page 300...\n");
    radix_tree_delete(tree, 300);
    printf("Page 300 after deletion: slot %d (should be -1)\n", radix_tree_lookup(tree, 300));
    
    // Test pop_min
    printf("\nPopping minimum values:\n");
    int16_t slot;
    while ((slot = radix_tree_pop_min(tree)) != -1) {
        printf("Popped slot: %d\n", slot);
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
