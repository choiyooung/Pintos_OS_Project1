#include "threads/palloc.h"
#include <bitmap.h>
#include <debug.h>
#include <inttypes.h>
#include <round.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "threads/loader.h"
#include "threads/synch.h"
#include "threads/vaddr.h"

/* Page allocator.  Hands out memory in page-size (or
   page#include <lib/kernel/bitmap.h>
#include <lib/kernel/bitmap.c>-multiple) chunks.  See malloc.h for an allocator that
   hands out smaller chunks.

   System memory is divided into two "pools" called the kernel
   and user pools.  The user pool is for user (virtual) memory
   pages, the kernel pool for everything else.  The idea here is
   that the kernel needs to have memory for its own operations
   even if user processes are swapping like mad.

   By default, half of system RAM is given to the kernel pool and
   half to the user pool.  That should be huge overkill for the
   kernel pool, but that's just fine for demonstration purposes. */

/* A memory pool. */
struct pool
  {
    struct lock lock;                   /* Mutual exclusion. */
    struct bitmap *used_map;            /* Bitmap of free pages. */
    uint8_t *base;                      /* Base of pool. */
  };

/* Two pools: one for kernel data, one for user pages. */
struct pool kernel_pool, user_pool;

//NEXTfIT 시작 지점을 계석 업데이트하면서 해당 구역부터시작하게 한다.
static enum polloc_policys policy = BESTFIT;
static size_t nextfitStart;


static void init_pool (struct pool *, void *base, size_t page_cnt,
                       const char *name);
static bool page_from_pool (const struct pool *, void *page);

/* Initializes the page allocator.  At most USER_PAGE_LIMIT
   pages are put into the user pool. */
void
palloc_init (size_t user_page_limit)
{
  /* Free memory starts at 1 MB and runs to the end of RAM. */
  uint8_t *free_start = ptov (1024 * 1024);
  uint8_t *free_end = ptov (init_ram_pages * PGSIZE);
  size_t free_pages = (free_end - free_start) / PGSIZE;
  size_t user_pages = free_pages / 2;
  size_t kernel_pages;
  if (user_pages > user_page_limit)
    user_pages = user_page_limit;
  kernel_pages = free_pages - user_pages;
  /* Give half of memory to kernel, half to user. */
  init_pool (&kernel_pool, free_start, kernel_pages, "kernel pool");
  init_pool (&user_pool, free_start + kernel_pages * PGSIZE,
             user_pages, "user pool");     
  //nextfit을 한다면 nextfitStart를 0으로 초기화한다.
  switch (policy)
  {
     case NEXTFIT:
     nextfitStart =0;
    break;
    case BUDDY:
    
     bitmap_buddy_init(palloc_get_user_pool_bitmap(),user_pages);
    break;
  default:
    break;
  }
  create_dump_page();
  print_userpool();
}

/* Obtains and returns a group of PAGE_CNT contiguous free pages.
   If PAL_USER is set, the pages are obtained from the user pool,
   otherwise from the kernel pool.  If PAL_ZERO is set in FLAGS,
   then the pages are filled with zeros.  If too few pages are
   available, returns a null pointer, unless PAL_ASSERT is set in
   FLAGS, in which case the kernel panics. */
void *
palloc_get_multiple (enum palloc_flags flags, size_t page_cnt)
{
  struct pool *pool = flags & PAL_USER ? &user_pool : &kernel_pool;
  void *pages;
  size_t page_idx;

  if (page_cnt == 0)
    return NULL;

  lock_acquire (&pool->lock);
  if(flags & PAL_USER ? 1 : 0){
    switch (policy)
    {
      case FIRSTFIT:
        page_idx = bitmap_scan_and_flip (pool->used_map, 0, page_cnt, false);
        break;
      case NEXTFIT:
        printf("before nextfit Start : %d", nextfitStart);
        page_idx = bitmap_scan_and_flip (pool->used_map, nextfitStart, page_cnt, false);
        //만약 btmap 에러가 뜬다면, nextfitStart를 0으로 초기화하고, 다시 해본다.
        if(page_idx == BITMAP_ERROR){
          nextfitStart = 0;
          page_idx = bitmap_scan_and_flip (pool->used_map, nextfitStart, page_cnt, false);
        }
        //page idx가 나온다면, 해당 page가 할당 된후 다음 주소를 nextfitStart에 넣어준다.
        nextfitStart =  (page_idx + page_cnt)% 367;
        printf("after nextfit Start : %d \n", nextfitStart);
        break;
      case BESTFIT:
        page_idx = bitmap_best(pool->used_map, page_cnt, false);
        break;
      case BUDDY:
        page_idx = bitmap_best(pool->used_map, page_cnt, false);
        break;
      default:
        break;
    }
  }else{
    page_idx = bitmap_scan_and_flip (pool->used_map, 0, page_cnt, false);
  }
  
  lock_release (&pool->lock);

  if (page_idx != BITMAP_ERROR)
    pages = pool->base + PGSIZE * page_idx;
  else
    pages = NULL;

  if (pages != NULL) 
    {
      if (flags & PAL_ZERO)
        memset (pages, 0, PGSIZE * page_cnt);
    }
  else 
    {
      if (flags & PAL_ASSERT)
        PANIC ("palloc_get: out of pages");
    }
    if(flags & PAL_USER? 1 : 0){
       print_userpool();
       printf("page cnt : %d \n",page_cnt);
       printf("pool->base : %ul, page %ul\n ",pool->base, pages);
    }
  return pages;
}
//frame use
void *
palloc_get_user_pool_base(){
  struct pool *pool = &user_pool;
  return pool->base;
}
size_t *
palloc_get_user_pool_bitmap(){
  struct pool *pool = &user_pool;
  return pool->used_map;
}
// 
void create_dump_page(){
  struct pool *pool = &user_pool; 
  //0 ~ 30까지의 페이지 set(true로 만듬)
  bitmap_set_multiple(pool->used_map, 0, 30, true);
  //간격 40
  bitmap_set_multiple(pool->used_map, 70, 20, true);
  //간격 30
  bitmap_set_multiple(pool->used_map, 120, 30, true);
  nextfitStart = 150;

}
void print_userpool(){
  size_t i;
    size_t start = 0;
    struct pool *pool = &user_pool; 
    size_t last = 367;
    int j = 0;

    //전체 bitmap 출력
    for(i = start; i < last; i++){
        if(bitmap_test (pool->used_map, i) == true){
            printf("1");
        }       
        else{
            printf("0");
        }
        j++;
        if(j%60 == 0)
            printf("\n");
    }
    j = 0;
    printf("\n");
}

/* Obtains a single free page and returns its kernel virtual
   address.
   If PAL_USER is set, the page is obtained from the user pool,
   otherwise from the kernel pool.  If PAL_ZERO is set in FLAGS,
   then the page is filled with zeros.  If no pages are
   available, returns a null pointer, unless PAL_ASSERT is set in
   FLAGS, in which case the kernel panics. */
void *
palloc_get_page (enum palloc_flags flags) 
{
  return palloc_get_multiple (flags, 1);
}

/* Frees the PAGE_CNT pages starting at PAGES. */
void
palloc_free_multiple (void *pages, size_t page_cnt) 
{
  struct pool *pool;
  size_t page_idx;

  ASSERT (pg_ofs (pages) == 0);
  if (pages == NULL || page_cnt == 0)
    return;

  if (page_from_pool (&kernel_pool, pages))
    pool = &kernel_pool;
  else if (page_from_pool (&user_pool, pages))
    pool = &user_pool;
  else
    NOT_REACHED ();

  page_idx = pg_no (pages) - pg_no (pool->base);

#ifndef NDEBUG
  memset (pages, 0xcc, PGSIZE * page_cnt);
#endif

  ASSERT (bitmap_all (pool->used_map, page_idx, page_cnt));
  bitmap_set_multiple (pool->used_map, page_idx, page_cnt, false);
}

/* Frees the page at PAGE. */
void
palloc_free_page (void *page) 
{
  palloc_free_multiple (page, 1);
}

/* Initializes pool P as starting at START and ending at END,
   naming it NAME for debugging purposes. */
static void
init_pool (struct pool *p, void *base, size_t page_cnt, const char *name) 
{
  /* We'll put the pool's used_map at its base.
     Calculate the space needed for the bitmap
     and subtract it from the pool's size. */
  size_t bm_pages = DIV_ROUND_UP (bitmap_buf_size (page_cnt), PGSIZE);
  if (bm_pages > page_cnt)
    PANIC ("Not enough memory in %s for bitmap.", name);
  page_cnt -= bm_pages;

  printf ("%zu pages availables in %s.\n", page_cnt, name);
  printf("%u pages is bm_pages\n",bm_pages);

  /* Initialize the pool. */
  lock_init (&p->lock);
  p->used_map = bitmap_create_in_buf (page_cnt, base, bm_pages * PGSIZE);
  p->base = base + bm_pages * PGSIZE;
}

/* Returns true if PAGE was allocated from POOL,
   false otherwise. */
static bool
page_from_pool (const struct pool *pool, void *page) 
{
  size_t page_no = pg_no (page);
  size_t start_page = pg_no (pool->base);
  size_t end_page = start_page + bitmap_size (pool->used_map);

  return page_no >= start_page && page_no < end_page;
}
