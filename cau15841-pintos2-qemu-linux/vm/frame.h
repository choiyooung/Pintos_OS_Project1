#ifndef VM_FRAME_H
#define VM_FRAME_H
#include "threads/palloc.h"
void frame_init(size_t user_page_limit);
void* frame_allocate(enum palloc_flags flags, void * upage);
void frame_entry_init(void* kpage);
void frame_free(void* kpage);
void print_frame_table();
#endif