#include <list.h>
#include <stdio.h>
#include "lib/kernel/list.h"

#include "vm/frame.h"
#include "threads/thread.h"
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "userprog/pagedir.h"
#include "threads/vaddr.h"

// 프레임 테이블이 실제로 주소와 매핑 되어져 있는 걸 모아둔  frame table
static struct list frame_table;

struct frame_table_entry{
    
    void *kpage; //실제 주소에 메핑되는 커널 페이지

    struct list_elem lelem; // 페이지 테이블 저장 공간

    void *upage; // 페이지에 대한 유저 주소(Virtual Memory)
    struct thread *t; //이 페이지와 관련있는 쓰레드 ;
};

//frame table을 조기화한다.
void
frame_init(){
    list_init(&frame_table);
}

//새로운 page들이 할당이 될때, frame table에 대한 entry도 table에 추가한다.

void*
frame_allocate(void * upage, void* kpage){
    if(kpage == NULL){
        /*이 부부은 페이지가 할당이 안되서,
        어떤 페이지를 스왑 시킨다음에  그 공간에 대한 entry 할당해야한다.
         */
    }
     struct frame_table_entry *frame = malloc(sizeof(struct frame_table_entry));

    frame->t = thread_current();
    frame->upage = upage;
    frame->kpage = kpage;

    list_push_back(&frame_table,&frame->lelem);
    return frame;    
}
void
frame_free(void* kpage){

}