#ifndef BUDDY_SYSTEM_H
#define BUDDY_SYSTEM_H

#include <stdint.h>
#include <stddef.h>
#include "common.h"

#define MIN_BLOCK_SIZE 4096
#define MIN_BLOCK_SHIFT 12
#define MIN_ORDER     0
#define MAX_ORDER     8
#define MEM_START     __free_ram
#define MEM_SIZE      (128 * 1024 * 1024)

struct page_meta {
    uint8_t order;
    uint8_t flags;
};

#define META_IS_FIRST_PAGE  0x01
#define META_ALLOCATED      0x02

struct list_head {
    struct list_head *next, *prev;
};

struct buddy_system {
    struct page_meta *meta_array;
    void *memory_base;
    size_t total_pages;
    int max_order;
    struct list_head free_lists[MAX_ORDER+1];
};

#define INIT_LIST_HEAD(ptr) do { \
    (ptr)->next = (ptr); (ptr)->prev = (ptr); \
} while (0)


void buddy_init(struct buddy_system *buddy, void *meta_base, void *data_base, size_t total_pages);
paddr_t buddy_alloc(struct buddy_system *buddy, size_t size);
void buddy_free(struct buddy_system *buddy, void *addr);

#endif