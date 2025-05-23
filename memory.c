//memC
#include "memory.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MEMORY_LOG_FILE "memory.log"

static struct MemoryBlock* create_block(int start, int size, struct MemoryBlock* parent) {
    struct MemoryBlock* block = (struct MemoryBlock*)malloc(sizeof(struct MemoryBlock));
    block->start = start;
    block->size = size;
    block->is_free = true;
    block->left = NULL;
    block->right = NULL;
    block->parent = parent;
    return block;
}

void init_memory_manager(struct MemoryManager* manager) {
    manager->root = create_block(0, TOTAL_MEMORY_SIZE, NULL);
}

static int next_power_of_2(int n) {
    int p = 1;
    while (p < n) p <<= 1;
    return p;
}


static struct MemoryBlock* find_block(struct MemoryBlock* block, int size) {
    if (!block) {
        printf("[MEMORY][DEBUG] Block is NULL\n");
        return NULL;
    }

    printf("[MEMORY][DEBUG] Checking block at %d, size=%d for allocation of %d\n", 
           block->start, block->size, size);
    
 
    if (!block->is_free) {
        printf("[MEMORY][DEBUG] Block at %d not free, checking children\n", block->start);
        struct MemoryBlock* res = NULL;
        if (block->left) {
            printf("[MEMORY][DEBUG] Trying left child at %d\n", block->left->start);
            res = find_block(block->left, size);
        }
        if (!res && block->right) {
            printf("[MEMORY][DEBUG] Trying right child at %d\n", block->right->start);
            res = find_block(block->right, size);
        }
        return res;
    }
    
    // If block is free and exactly the size we need
    if (block->size == size) {
        printf("[MEMORY][DEBUG] Found exact fit at %d\n", block->start);
        block->is_free = false;
        return block;
    }
    
    // If block is too small
    if (block->size < size) {
        printf("[MEMORY][DEBUG] Block too small: %d < %d\n", block->size, size);
        return NULL;
    }
    
    // If block is larger, split if not already split
    if (!block->left && !block->right) {
        int half = block->size / 2;
        printf("[MEMORY][DEBUG] Splitting block at %d into two blocks of size %d\n", 
               block->start, half);
        block->left = create_block(block->start, half, block);
        block->right = create_block(block->start + half, half, block);
    }
    
    // Try left first, then right
    struct MemoryBlock* res = NULL;
    if (block->left) {
        printf("[MEMORY][DEBUG] Trying left child at %d\n", block->left->start);
        res = find_block(block->left, size);
    }
    if (!res && block->right) {
        printf("[MEMORY][DEBUG] Trying right child at %d\n", block->right->start);
        res = find_block(block->right, size);
    }
    

    if (res) {
        printf("[MEMORY][DEBUG] Found allocation in child, marking parent at %d as not free\n", block->start);
        block->is_free = false;
    }
    // If both children are fully used, block is not free
    else if (block->left && block->right && !block->left->is_free && !block->right->is_free) {
        printf("[MEMORY][DEBUG] Both children used, marking block at %d as not free\n", block->start);
        block->is_free = false;
    }
    
    return res;
}

struct MemoryBlock* allocate_memory(struct MemoryManager* manager, int size) {
    printf("[MEMORY][DEBUG] allocate_memory called!\n");
    int alloc_size = next_power_of_2(size);
    printf("[MEMORY][DEBUG] allocate_memory called with size=%d (rounded=%d)\n", size, alloc_size);
    
    // Check if the size is too large for the available memory
    if (alloc_size > TOTAL_MEMORY_SIZE) {
        printf("[MEMORY][DEBUG] Requested size %d (rounded to %d) exceeds total memory size %d\n", 
               size, alloc_size, TOTAL_MEMORY_SIZE);
        return NULL;
    }
    
    // Find a suitable block
    struct MemoryBlock* block = find_block(manager->root, alloc_size);
    
    if (block) {
        printf("[MEMORY][DEBUG] Allocation success: start=%d, size=%d\n", block->start, block->size);
        // Make sure the block is marked as not free
        block->is_free = false;
        
        // Mark parent blocks as not free going up the tree
        struct MemoryBlock* current = block;
        while (current->parent) {
            current = current->parent;
            current->is_free = false;
        }
    } else {
        printf("[MEMORY][DEBUG] Allocation failed for size=%d\n", alloc_size);
    }
    
    return block;
}

// Helper: recursively find block by start and size
static struct MemoryBlock* find_block_by_addr(struct MemoryBlock* block, int start, int size) {
    if (!block) return NULL;
    if (block->start == start && block->size == size) return block;
    struct MemoryBlock* res = find_block_by_addr(block->left, start, size);
    if (res) return res;
    return find_block_by_addr(block->right, start, size);
}

// Helper: try to merge buddies
static void try_merge(struct MemoryBlock* block) {
    if (!block || !block->parent) return;
    
    printf("[MEMORY][DEBUG] Attempting to merge block at start=%d, size=%d\n", block->start, block->size);
    
    struct MemoryBlock* parent = block->parent;
    struct MemoryBlock* left = parent->left;
    struct MemoryBlock* right = parent->right;
    
    // Check if both buddies are free
    if (left && right && left->is_free && right->is_free) {
        printf("[MEMORY][DEBUG] Found free buddies: left[%d-%d] and right[%d-%d]\n", 
               left->start, left->start + left->size - 1,
               right->start, right->start + right->size - 1);
        
        // Merge
        free(left); 
        free(right);
        parent->left = parent->right = NULL;
        parent->is_free = true;
        
        printf("[MEMORY][DEBUG] Merged into parent block at start=%d, size=%d\n", 
               parent->start, parent->size);
        
        // Recursively try to merge with parent's buddy
        try_merge(parent);
    } else {
        printf("[MEMORY][DEBUG] No merge possible: left=%p, right=%p, left_free=%d, right_free=%d\n",
               (void*)left, (void*)right, 
               left ? left->is_free : 0, 
               right ? right->is_free : 0);
        
        // Even if we can't merge this block, we need to check if parent can be marked as free
        // Parent can be free only if both children are free
        if (parent->left && parent->right) {
            parent->is_free = parent->left->is_free && parent->right->is_free;
        }
    }
}

void free_memory(struct MemoryManager* manager, int start, int size) {
    int block_size = next_power_of_2(size);
    printf("[MEMORY][DEBUG] free_memory called with start=%d, size=%d (rounded=%d)\n", start, size, block_size);
    
    // Find the block to free
    struct MemoryBlock* block = find_block_by_addr(manager->root, start, block_size);
    if (!block) {
        printf("[MEMORY][DEBUG] Free failed: block not found for start=%d, size=%d\n", start, block_size);
        return;
    }
    
    printf("[MEMORY][DEBUG] Found block to free at start=%d, size=%d\n", block->start, block->size);
    
    // Mark the block as free
    block->is_free = true;
    
    // Try to merge with buddies
    printf("[MEMORY][DEBUG] Attempting to merge freed block\n");
    try_merge(block);
    
    // After merging, try to merge parent blocks recursively
    struct MemoryBlock* current = block;
    while (current && current->parent) {
        current = current->parent;
        if (current->left && current->right && 
            current->left->is_free && current->right->is_free) {
            printf("[MEMORY][DEBUG] Merging parent blocks at level %d\n", current->size);
            try_merge(current);
        }
    }
    
    printf("[MEMORY][DEBUG] Free success: start=%d, size=%d\n", start, block_size);
}

void log_memory_action(const char* action, int time, int pid, int size, int start, int end) {
    printf("[MEMORY][DEBUG] log_memory_action: %s %d bytes for process %d from %d to %d at time %d\n", action, size, pid, start, end, time);
    FILE* log = fopen(MEMORY_LOG_FILE, "a");
    if (!log) { printf("[MEMORY][DEBUG] Failed to open memory.log!\n"); return; }
    fprintf(log, "At time %d %s %d bytes for process %d from %d to %d\n", time, action, size, pid, start, end);
    fclose(log);
}

static void destroy_memory_block(struct MemoryBlock* block) {
    if (!block) return;
    
    // Recursively free children first
    if (block->left) {
        destroy_memory_block(block->left);
        block->left = NULL;
    }
    
    if (block->right) {
        destroy_memory_block(block->right);
        block->right = NULL;
    }
    
    // Then free the block itself
    printf("[MEMORY][DEBUG] Destroying memory block at start=%d, size=%d\n", block->start, block->size);
    free(block);
}

void destroy_memory_manager(struct MemoryManager* manager) {
    // Recursively free all blocks
    if (manager && manager->root) {
        destroy_memory_block(manager->root);
        manager->root = NULL;
    }
}