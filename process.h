#ifndef PROCESS_H
#define PROCESS_H

#include "common.h"
#define PROCS_MAX 8
#define PROC_UNUSED   0
#define PROC_RUNNABLE 1
#define PROC_EXITED   2

struct process {
    int pid; // 0 if it's an idle process
    int state; // PROC_UNUSED, PROC_RUNNABLE, PROC_EXITED
    vaddr_t sp; // kernel stack pointer
    uint32_t *page_table; // points to first level page table
    uint8_t stack[8192]; // kernel stack
};

extern struct process procs[PROCS_MAX];
extern struct process *current_proc;
extern struct process *idle_proc;

__attribute__((naked)) void switch_context(uint32_t *prev_sp, uint32_t *next_sp);
struct process *create_process(const void *image, size_t image_size);
void yield(void);

#endif