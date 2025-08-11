#include "memory.h"
#include "kernel.h"
#include "buddy_system.h"

struct buddy_system physical_memory_allocator;

void memory_allocator_init() {
    uint32_t aligned_start = align_up((uint32_t) MEM_START, MIN_BLOCK_SIZE << MAX_ORDER);
    printf("start = %u\n", aligned_start);
    size_t aligned_size = MEM_SIZE - (aligned_start - (uint32_t) MEM_START);
    printf("size = %u\n", aligned_size);
    buddy_init(&physical_memory_allocator, (void *)aligned_start, aligned_size);
}

paddr_t alloc_pages(uint32_t n) {
    uint32_t cnt = n;
    uint8_t order = 0;
    while (cnt > 1) {
        order += 1;
        cnt >>= 1;
    }
    
    return buddy_alloc(&physical_memory_allocator, n * MIN_BLOCK_SIZE);
    // static paddr_t next_paddr = (paddr_t) __free_ram;
    // paddr_t paddr = next_paddr;
    // next_paddr += n * PAGE_SIZE;
    // if (next_paddr > (paddr_t) __free_ram_end) {
    //     PANIC("out of memory");
    // }
    
    // memset((void*) paddr, 0, n * PAGE_SIZE);
    // return paddr;
}

void free_pages(void *pages) {
    buddy_free(&physical_memory_allocator, pages);
}

void map_page(uint32_t *table1, uint32_t vaddr, paddr_t paddr, uint32_t flags) {
    if (!is_aligned(vaddr, PAGE_SIZE)) {
        PANIC("unaligned vaddr %x", vaddr);
    }

    if (!is_aligned(paddr, PAGE_SIZE)) {
        PANIC("unaligned paddr %x", paddr);
    }
    
    uint32_t vpn1 = (vaddr >> 22) & 0x3ff;
    if ((table1[vpn1] & PAGE_V) == 0) {
        uint32_t pt_paddr = alloc_pages(1);
        table1[vpn1] = ((pt_paddr / PAGE_SIZE) << 10) | PAGE_V;
    }

    uint32_t vpn0 = (vaddr >> 12) & 0x3ff;
    uint32_t *table0 = (uint32_t *) ((table1[vpn1] >> 10) * PAGE_SIZE);
    table0[vpn0] = ((paddr / PAGE_SIZE) << 10) | flags | PAGE_V;
}