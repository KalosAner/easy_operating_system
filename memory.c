#include "memory.h"
#include "kernel.h"
#include "buddy_system.h"

struct buddy_system phys_alloc;

void memory_allocator_init() {
    size_t total_pages = (MEM_SIZE - 0x10000) / PAGE_SIZE;
    size_t meta_size = total_pages * sizeof(struct page_meta);
    
    void *meta_base = (void *)((paddr_t) __free_ram + 0x100000);
    void *data_base = (void *)((paddr_t) __free_ram + 0x200000);
    
    static struct buddy_system phys_alloc;
    buddy_init(&phys_alloc, meta_base, data_base, total_pages);
}

paddr_t alloc_pages(uint32_t n) {
    return buddy_alloc(&phys_alloc, n * PAGE_SIZE);
    // static paddr_t next_paddr = (paddr_t) __free_ram;
    // paddr_t paddr = next_paddr;
    // next_paddr += n * PAGE_SIZE;
    // if (next_paddr > (paddr_t) __free_ram_end) {
    //     PANIC("out of memory");
    // }
    
    // memset((void*) paddr, 0, n * PAGE_SIZE);
    // return paddr;
}

void free_physical_page(void *page) {
    buddy_free(&phys_alloc, page);
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