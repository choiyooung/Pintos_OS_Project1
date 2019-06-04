#include <list.h>
#include <stdio.h>
#include "lib/kernel/list.h"
#include "lib/kernel/hash.h"
#include "vm/frame.h"
#include "threads/thread.h"
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "userprog/pagedir.h"
#include "threads/vaddr.h"

//lock
static struct lock frame_lock;
// 프레임 테이블이 실제로 주소와 매핑 되어져 있는 걸 모아둔  frame table
static struct list frame_table_list;
static struct hash frame_table_hash;

static unsigned frame_hash_func(const struct hash_elem *elem, void *aux);
static bool     frame_less_func(const struct hash_elem *, const struct hash_elem *, void *aux);



struct frame_table_entry{
    
    void *kpage; //실제 주소에 메핑되는 커널 페이지

    struct hash_elem helem;  
    struct list_elem lelem; // 페이지 테이블 저장 공간

    void *upage; // 페이지에 대한 유저 주소(Virtual Memory)
    struct thread *t; //이 페이지와 관련있는 쓰레드 ;


};

//frame table을 조기화한다.
void
frame_init(size_t user_page_limit){
    lock_init (&frame_lock);
    void *kpage_base = palloc_get_user_pool_base();
    void *kpage;
    int i;

    hash_init (&frame_table_hash, frame_hash_func, frame_less_func, NULL);
    list_init(&frame_table_list);
    printf("frame : pool->base : %ul\n",kpage_base);
     for(i = 0; i<367; i++){
         kpage = kpage_base + i*PGSIZE;
         frame_entry_init(kpage);
    }
    print_frame_table();
    
}

//새로운 page들이 할당이 될때, frame table에 대한 entry도 table안에 해당 kpage에 대한 entry를 set한다.
void*
frame_allocate(enum palloc_flags flags, void * upage){
     void *kpage = palloc_get_page (PAL_USER | flags);
     if(kpage == NULL){
         //이때 해당 페이지의 얼로케이션이 실패했따는 것이기 떄문에
         // 스왑이 발생해야한다.
     }
    struct frame_table_entry f_tmp;
    f_tmp.kpage = kpage;
    printf("kpage : %d\n", pg_no(kpage));
    struct hash_elem *h = hash_find (&frame_table_hash, &(f_tmp.helem));
    if (h == NULL) {
        PANIC ("The page to be freed is not stored in the table");
    }
    //kpage에 대한 entry를 찾았으면 frame에다가 옮겨주고, table(hash,list)안에 해당 entry를 수정한다.  .
    struct frame_table_entry *frame;
    frame = hash_entry(h, struct frame_table_entry, helem);


    frame->t = thread_current();
    frame->upage = upage;
    //출력해본다. 시험삼아
    print_frame_table();

    return kpage;    
}

//367(the number of page in user pool)entry  init
void
frame_entry_init(void* kpage){
    lock_acquire (&frame_lock);
    if(kpage == NULL){
        /*이 부부은 페이지가 할당이 안되서,
        어떤 페이지를 스왑 시킨다음에  그 공간에 대한 entry 할당해야한다.
         */
    }
     struct frame_table_entry *frame = malloc(sizeof(struct frame_table_entry));

    frame->t = NULL;
    frame->upage = NULL;
    frame->kpage = kpage;


    //페이지 테이블에 해당 entry를 추가한다.
    hash_insert (&frame_table_hash, &frame->helem);
    list_push_back(&frame_table_list,&frame->lelem);


    lock_release (&frame_lock);
}
void
frame_free(void* kpage){
    // frame table kpage에 대한 페이지를 헤재한다.

    //임시로 argument kpage로 hash table에 있는 entry를 찾는다.
    struct frame_table_entry f_tmp;
    f_tmp.kpage = kpage;

    struct hash_elem *h = hash_find (&frame_table_hash, &(f_tmp.helem));
    if (h == NULL) {
        PANIC ("The page to be freed is not stored in the table");
    }

    //kpage에 대한 entry를 찾았으면 f에다가 옮겨주고, table(hash,list)안에 해당 entry를 삭제한다.
    struct frame_table_entry *f;
     f = hash_entry(h, struct frame_table_entry, helem);
     
     palloc_free_page(kpage);

     f->upage = NULL;
     f->t = NULL;
}
void 
print_frame_table(){
    struct list *list = &frame_table_list;
    struct list_elem *begin;
    struct frame_table_entry *e;
    int idx;
    size_t last_elem_cnt = list_size(list);
    begin = list_begin(list);
    for(idx=0 ; idx<last_elem_cnt;idx++){
        e = list_entry(begin, struct frame_table_entry, lelem);
        if(e->upage!=NULL){
            printf("this upage is allocated!! address is %ul\n",e->upage);
        }
        begin = list_next (begin);
    }


}
// Hash Functions required for [frame_map]. Uses 'kpage' as key.
static unsigned frame_hash_func(const struct hash_elem *elem, void *aux UNUSED)
{
  struct frame_table_entry *entry = hash_entry(elem, struct frame_table_entry, helem);
  return hash_bytes( &entry->kpage, sizeof entry->kpage );
}
static bool frame_less_func(const struct hash_elem *a, const struct hash_elem *b, void *aux UNUSED)
{
  struct frame_table_entry *a_entry = hash_entry(a, struct frame_table_entry, helem);
  struct frame_table_entry *b_entry = hash_entry(b, struct frame_table_entry, helem);
  return a_entry->kpage < b_entry->kpage;
}
