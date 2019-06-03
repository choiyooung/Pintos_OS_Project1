#ifndef VM_FRAME_H
#define VM_FRAME_H
void frame_init(size_t user_page_limit);
void* frame_allocate(void * upage, void* kpage);
void* frame_entry_init(void * upage, void* kpage);
void frame_free(void* kpage);
void print_frame_table();
void foo();
#endif