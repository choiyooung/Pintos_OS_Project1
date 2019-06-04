#ifndef VM_SWAP_H
#define VM_SWAP_H
typedef uint32_t swap_index_t;

void vm_swap_init (void); // initiallize
swap_index_t vm_swap_out (void *page); // 스왑아웃 후 스왑된 곳 주소 return
void vm_swap_in (swap_index_t swap_index, void *page); // 스왑인
void vm_swap_free (swap_index_t swap_index); //해당 스왑 공간 가용으로 강제 전환(이미 가용이먼 오류)
#endif