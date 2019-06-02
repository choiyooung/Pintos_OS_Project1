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
frame_init(){
     hash_init (&frame_table_hash, frame_hash_func, frame_less_func, NULL);
    list_init(&frame_table_hash);
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
    
    //페이지 테이블에 해당 entry를 추가한다.
    hash_insert (&frame_table_hash, &frame->helem);
    list_push_back(&frame_table_list,&frame->lelem);
    return frame;    
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

    hash_delete (&frame_table_hash, &f->helem);
    list_remove (&f->lelem);
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
