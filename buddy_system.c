#include "buddy_system.h"

static paddr_t get_buddy_address(struct buddy_system *buddy, void *addr, uint8_t order) {
    uintptr_t base = (uintptr_t)buddy->memory_base;
    uintptr_t offset = (uintptr_t)addr - base;
    uintptr_t buddy_offset = offset ^ (1UL << (order + MIN_BLOCK_SHIFT));
    return (paddr_t)(base + buddy_offset);
}

void buddy_init(struct buddy_system *buddy, void *base, size_t size) {
    buddy->memory_base = base;
    buddy->total_pages = size / MIN_BLOCK_SIZE;
    buddy->min_order = 0;
    buddy->max_order = 0;
    
    size_t total_size = MIN_BLOCK_SIZE;
    while (total_size < size && buddy->max_order < MAX_ORDER) {
        buddy->max_order++;
        total_size <<= 1;
    }
    
    for (int i = 0; i <= MAX_ORDER; i++) {
        buddy->free_lists[i] = 0;
    }
    
    size_t map_size = (buddy->total_pages + 7) / 8;
    buddy->memory_map = (uint8_t *)base;
    
    struct block_meta *meta = (struct block_meta *)buddy->memory_base;
    meta->order = buddy->max_order;
    printf("maxorder = %u, %u\n", sizeof(struct block_meta), (uint8_t *)meta);
    meta->allocated = false;
    
    *(uint32_t *)((uint8_t *)meta + sizeof(struct block_meta)) = buddy->free_lists[buddy->max_order];
    buddy->free_lists[buddy->max_order] = (uint32_t)meta;
    printf("meta = %u\n", buddy->free_lists[buddy->max_order]);
}

paddr_t buddy_alloc(struct buddy_system *buddy, size_t size) {
    size_t required_size = size + sizeof(struct block_meta);
    uint8_t order = buddy->min_order;
    size_t block_size = MIN_BLOCK_SIZE;
    
    while (block_size < required_size && order < buddy->max_order) {
        order++;
        block_size <<= 1;
    }
    
    uint8_t current_order = order;
    struct block_meta *block = NULL;
    
    while (current_order <= buddy->max_order) {
        if (buddy->free_lists[current_order] != 0) {
            block = (struct block_meta *)buddy->free_lists[current_order];
            buddy->free_lists[current_order] = *(uint32_t *)((uint8_t *)block + sizeof(struct block_meta));
            break;
        }
        current_order++;
    }
    printf("corder = %u\n", current_order);
    
    if (!block) {
        printf("test7\n");
        return (paddr_t) NULL;
    }
    
    while (current_order > order) {
        current_order--;
        
        size_t half_size = MIN_BLOCK_SIZE << current_order;
        struct block_meta *buddy_block = 
            (struct block_meta *)((uint8_t *)block + half_size);
        
        buddy_block->order = current_order;
        buddy_block->allocated = false;
        
        *(uint32_t *)((uint8_t *)buddy_block + sizeof(struct block_meta)) = 
            buddy->free_lists[current_order];
        buddy->free_lists[current_order] = (uint32_t)buddy_block;
        
        block->order = current_order;
    }
    
    block->allocated = true;
    
    return (paddr_t)((uint8_t *)block + sizeof(struct block_meta));
}

void buddy_free(struct buddy_system *buddy, void *ptr) {
    if (!ptr) return;
    
    struct block_meta *block = 
        (struct block_meta *)((uint8_t *)ptr - sizeof(struct block_meta));
    
    if ((uint8_t *)block < buddy->memory_base || 
        (uint8_t *)block >= (uint8_t *)buddy->memory_base + MEM_SIZE) {
        return;
    }
    
    block->allocated = false;
    uint8_t order = block->order;
    
    while (order < buddy->max_order) {
        size_t block_size = MIN_BLOCK_SIZE << order;
        size_t buddy_offset = (uint8_t *)block - (uint8_t *)buddy->memory_base;
        size_t buddy_index = buddy_offset ^ block_size;
        
        if (buddy_index >= MEM_SIZE) break;
        
        struct block_meta *buddy_block = 
            (struct block_meta *)((uint8_t *)buddy->memory_base + buddy_index);
        
        if (!buddy_block->allocated && buddy_block->order == order) {
            uint32_t *prev = &buddy->free_lists[order];
            while (*prev) {
                struct block_meta *current = (struct block_meta *)*prev;
                if (current == buddy_block) {
                    *prev = *(uint32_t *)((uint8_t *)current + sizeof(struct block_meta));
                    break;
                }
                prev = (uint32_t *)((uint8_t *)current + sizeof(struct block_meta));
            }
            
            if (block > buddy_block) {
                block = buddy_block;
            }
            
            order++;
            block->order = order;
        } else {
            break;
        }
    }
    
    *(uint32_t *)((uint8_t *)block + sizeof(struct block_meta)) = 
        buddy->free_lists[order];
    buddy->free_lists[order] = (uint32_t)block;
}