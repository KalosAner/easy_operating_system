#include "buddy_system.h"

static paddr_t get_buddy_address(struct buddy_system *buddy, void *addr, uint8_t order) {
    uintptr_t base = (uintptr_t)buddy->memory_base;
    uintptr_t offset = (uintptr_t)addr - base;
    uintptr_t buddy_offset = offset ^ (1UL << (order + MIN_BLOCK_SHIFT));
    return (paddr_t)(base + buddy_offset);
}

void list_add(struct list_head *new, struct list_head *head) {
    new->next = head->next;
    new->prev = head;
    head->next->prev = new;
    head->next = new;
}

void list_del(struct list_head *node) {
    node->prev->next = node->next;
    node->next->prev = node->prev;
    node->next = node->prev = NULL;
}

bool list_empty(struct list_head *head) {
    return head->next == head;
}

static inline uintptr_t align_up(uintptr_t addr, size_t alignment) {
    return (addr + alignment - 1) & ~(alignment - 1);
}

static inline uint32_t get_ppn(struct buddy_system *buddy, void *addr) {
    return ((uintptr_t)addr - (uintptr_t)buddy->memory_base) / PAGE_SIZE;
}

void buddy_init(struct buddy_system *buddy, void *meta_base, void *data_base, size_t total_pages) {
    buddy->meta_array = (struct page_meta *)meta_base;
    buddy->memory_base = data_base;
    buddy->total_pages = total_pages;
    
    size_t pages = total_pages;
    buddy->max_order = 0;
    while (pages >>= 1) buddy->max_order++;
    if (buddy->max_order > MAX_ORDER) 
        buddy->max_order = MAX_ORDER;
    
    for (int i = 0; i <= MAX_ORDER; i++) {
        INIT_LIST_HEAD(&buddy->free_lists[i]);
    }
    
    size_t block_order = buddy->max_order;
    size_t block_pages = 1UL << block_order;
    
    for (size_t i = 0; i < total_pages; i++) {
        buddy->meta_array[i].order = block_order;
        buddy->meta_array[i].flags = 0;
    }

    size_t current_page = 0;
    while (current_page + block_pages < total_pages) {
        buddy->meta_array[current_page].flags = META_IS_FIRST_PAGE;
        struct list_head *node = (struct list_head *) ((paddr_t) buddy->memory_base + current_page * PAGE_SIZE);
        INIT_LIST_HEAD(node);
        list_add(node, &buddy->free_lists[block_order]);
        current_page += block_pages;
    }
}

paddr_t buddy_alloc(struct buddy_system *buddy, size_t size) {
    size_t pages_needed = (size + PAGE_SIZE - 1) / PAGE_SIZE;
    if (pages_needed == 0) pages_needed = 1;
    
    uint8_t order = 0;
    size_t tmp = pages_needed;
    while (tmp > 1) {
        order++;
        tmp >>= 1;
    }
    if ((1UL << order) < pages_needed) order++;
    
    if (order > buddy->max_order) return (paddr_t) NULL;
    
    uint8_t current_order = order;
    struct list_head *alloc_node = NULL;
    
    while (current_order <= buddy->max_order) {
        if (!list_empty(&buddy->free_lists[current_order])) {
            alloc_node = buddy->free_lists[current_order].next;
            list_del(alloc_node);
            break;
        }
        current_order++;
    }
    
    if (!alloc_node) return (paddr_t) NULL;
    
    size_t first_page_idx = ((uintptr_t)alloc_node - (uintptr_t)buddy->memory_base) / PAGE_SIZE;
    
    while (current_order > order) {
        current_order--;
        size_t half_pages = 1UL << current_order;
        
        size_t buddy_page_idx = first_page_idx + half_pages;
        buddy->meta_array[buddy_page_idx].order = current_order;
        buddy->meta_array[buddy_page_idx].flags = META_IS_FIRST_PAGE;
        
        for (size_t i = 1; i < half_pages; i++) {
            buddy->meta_array[buddy_page_idx + i].order = current_order;
            buddy->meta_array[buddy_page_idx + i].flags = 0;
        }
        
        struct list_head *buddy_node = (struct list_head *)(
            (uint8_t *)buddy->memory_base + buddy_page_idx * PAGE_SIZE);
        INIT_LIST_HEAD(buddy_node);
        list_add(buddy_node, &buddy->free_lists[current_order]);
        
        buddy->meta_array[first_page_idx].order = current_order;
    }
    
    size_t block_pages = 1UL << current_order;
    for (size_t i = 0; i < block_pages; i++) {
        buddy->meta_array[first_page_idx + i].flags |= META_ALLOCATED;
    }
    
    return (paddr_t)((uint8_t *)buddy->memory_base + first_page_idx * PAGE_SIZE);
}

void buddy_free(struct buddy_system *buddy, void *addr) {
    size_t page_idx = ((uintptr_t)addr - (uintptr_t)buddy->memory_base) / PAGE_SIZE;
    
    struct page_meta *meta = &buddy->meta_array[page_idx];
    
    if (!(meta->flags & META_IS_FIRST_PAGE)) {
        uint8_t order = meta->order;
        size_t block_size = 1UL << order;
        page_idx = page_idx & ~(block_size - 1);
        meta = &buddy->meta_array[page_idx];
    }
    
    uint8_t order = meta->order;
    size_t block_pages = 1UL << order;
    
    for (size_t i = 0; i < block_pages; i++) {
        buddy->meta_array[page_idx + i].flags &= ~META_ALLOCATED;
    }
    
    while (order < buddy->max_order) {
        size_t buddy_page_idx = page_idx ^ (1UL << order);
        
        if (buddy_page_idx >= buddy->total_pages) break;
        
        struct page_meta *buddy_meta = &buddy->meta_array[buddy_page_idx];
        if (!(buddy_meta->flags & META_IS_FIRST_PAGE)) break;
        if (buddy_meta->flags & META_ALLOCATED) break;
        if (buddy_meta->order != order) break;
        
        struct list_head *buddy_node = (struct list_head *)(
            (uint8_t *)buddy->memory_base + buddy_page_idx * PAGE_SIZE);
        list_del(buddy_node);
        
        if (buddy_page_idx < page_idx) {
            page_idx = buddy_page_idx;
            meta = buddy_meta;
        }
        
        order++;
        meta->order = order;
        
        size_t new_block_pages = 1UL << order;
        for (size_t i = 0; i < new_block_pages; i++) {
            if (i == 0) {
                buddy->meta_array[page_idx + i].flags = META_IS_FIRST_PAGE;
            } else {
                buddy->meta_array[page_idx + i].flags = 0;
            }
            buddy->meta_array[page_idx + i].order = order;
        }
    }
    
    struct list_head *node = (struct list_head *)(
        (uint8_t *)buddy->memory_base + page_idx * PAGE_SIZE);
    INIT_LIST_HEAD(node);
    list_add(node, &buddy->free_lists[order]);
}