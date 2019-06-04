#ifndef THREADS_PALLOC_H
#define THREADS_PALLOC_H

#include <stddef.h>

/* How to allocate pages. */
enum palloc_flags
  {
    PAL_ASSERT = 001,           /* Panic on failure. */
    PAL_ZERO = 002,             /* Zero page contents. */
    PAL_USER = 004              /* User page. */
  };
//어떤 할당 정책을 펼친것인가
enum polloc_policys
{
  FIRSTFIT = 0,
  NEXTFIT = 1,
  BESTFIT = 2,
  BUDDY = 3
};

void palloc_init (size_t user_page_limit);
void *palloc_get_page (enum palloc_flags);
void *palloc_get_multiple (enum palloc_flags, size_t page_cnt);
void palloc_free_page (void *);
void palloc_free_multiple (void *, size_t page_cnt);
void *palloc_get_user_pool_base();
void create_dump_page();
void print_userpool();
size_t *palloc_get_user_pool_bitmap();
#endif /* threads/palloc.h */
