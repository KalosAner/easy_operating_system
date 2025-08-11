#ifndef BUDDY_SYSTEM_H
#define BUDDY_SYSTEM_H

#include <stdint.h>
#include <stddef.h>
#include "common.h"

#define MIN_BLOCK_SIZE 4096
#define MIN_BLOCK_SHIFT 12
#define MAX_ORDER     8
#define MEM_START     __free_ram
#define MEM_SIZE      (128 * 1024 * 1024)

struct buddy_system {
    uint8_t *memory_map;
    void *memory_base;
    size_t total_pages;
    size_t min_order;
    size_t max_order;
    uint32_t free_lists[MAX_ORDER + 1];
};

struct block_meta {
    uint8_t order;
    bool allocated;
};

static paddr_t get_buddy_address(struct buddy_system *buddy, void *addr, uint8_t order);
void buddy_init(struct buddy_system *buddy, void *base, size_t size);
paddr_t buddy_alloc(struct buddy_system *buddy, size_t size);
void buddy_free(struct buddy_system *buddy, void *ptr);

#endif