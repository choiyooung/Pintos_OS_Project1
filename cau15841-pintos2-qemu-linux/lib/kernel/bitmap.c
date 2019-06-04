#include "bitmap.h"
#include <debug.h>
#include <limits.h>
#include <round.h>
#include <stdio.h>
#include "threads/malloc.h"
#ifdef FILESYS
#include "filesys/file.h"
#endif
//BUDDY를 위한 구조체

//정책으로 buddy가 사용되어지면 해당 list를 buddy init에서 초기화한다.
buddy_list *b_list;
/* Element type.

   This must be an unsigned integer type at least as wide as int.

   Each bit represents one bit in the bitmap.
   If bit 0 in an element represents bit K in the bitmap,
   then bit 1 in the element represents bit K+1 in the bitmap,
   and so on. */
typedef unsigned long elem_type;

/* Number of bits in an element. */
#define ELEM_BITS (sizeof (elem_type) * CHAR_BIT)

/* From the outside, a bitmap is an array of bits.  From the
   inside, it's an array of elem_type (defined above) that
   simulates an array of bits. */
struct bitmap
  {
    size_t bit_cnt;     /* Number of bits. */
    elem_type *bits;    /* Elements that represent bits. */
  };

/* Returns the index of the element that contains the bit
   numbered BIT_IDX. */
static inline size_t
elem_idx (size_t bit_idx) 
{
  return bit_idx / ELEM_BITS;
}

/* Returns an elem_type where only the bit corresponding to
   BIT_IDX is turned on. */
static inline elem_type
bit_mask (size_t bit_idx) 
{
  return (elem_type) 1 << (bit_idx % ELEM_BITS);
}

/* Returns the number of elements required for BIT_CNT bits. */
static inline size_t
elem_cnt (size_t bit_cnt)
{
  return DIV_ROUND_UP (bit_cnt, ELEM_BITS);
}

/* Returns the number of bytes required for BIT_CNT bits. */
static inline size_t
byte_cnt (size_t bit_cnt)
{
  return sizeof (elem_type) * elem_cnt (bit_cnt);
}

/* Returns a bit mask in which the bits actually used in the last
   element of B's bits are set to 1 and the rest are set to 0. */
static inline elem_type
last_mask (const struct bitmap *b) 
{
  int last_bits = b->bit_cnt % ELEM_BITS;
  return last_bits ? ((elem_type) 1 << last_bits) - 1 : (elem_type) -1;
}

/* Creation and destruction. */

/* Creates and returns a pointer to a newly allocated bitmap with room for
   BIT_CNT (or more) bits.  Returns a null pointer if memory allocation fails.
   The caller is responsible for freeing the bitmap, with bitmap_destroy(),
   when it is no longer needed. */
struct bitmap *
bitmap_create (size_t bit_cnt) 
{
  struct bitmap *b = malloc (sizeof *b);
  if (b != NULL)
    {
      b->bit_cnt = bit_cnt;
      b->bits = malloc (byte_cnt (bit_cnt));
      if (b->bits != NULL || bit_cnt == 0)
        {
          bitmap_set_all (b, false);
          return b;
        }
      free (b);
    }
  return NULL;
}

/* Creates and returns a bitmap with BIT_CNT bits in the
   BLOCK_SIZE bytes of storage preallocated at BLOCK.
   BLOCK_SIZE must be at least bitmap_needed_bytes(BIT_CNT). */
struct bitmap *
bitmap_create_in_buf (size_t bit_cnt, void *block, size_t block_size UNUSED)
{
  struct bitmap *b = block;
  
  ASSERT (block_size >= bitmap_buf_size (bit_cnt));

  b->bit_cnt = bit_cnt;
  b->bits = (elem_type *) (b + 1);
  bitmap_set_all (b, false);
  return b;
}

/* Returns the number of bytes required to accomodate a bitmap
   with BIT_CNT bits (for use with bitmap_create_in_buf()). */
size_t
bitmap_buf_size (size_t bit_cnt) 
{
  return sizeof (struct bitmap) + byte_cnt (bit_cnt);
}

/* Destroys bitmap B, freeing its storage.
   Not for use on bitmaps created by bitmap_create_in_buf(). */
void
bitmap_destroy (struct bitmap *b) 
{
  if (b != NULL) 
    {
      free (b->bits);
      free (b);
    }
}

/* Bitmap size. */

/* Returns the number of bits in B. */
size_t
bitmap_size (const struct bitmap *b)
{
  return b->bit_cnt;
}

/* Setting and testing single bits. */

/* Atomically sets the bit numbered IDX in B to VALUE. */
void
bitmap_set (struct bitmap *b, size_t idx, bool value) 
{
  ASSERT (b != NULL);
  ASSERT (idx < b->bit_cnt);
  if (value)
    bitmap_mark (b, idx);
  else
    bitmap_reset (b, idx);
}

/* Atomically sets the bit numbered BIT_IDX in B to true. */
void
bitmap_mark (struct bitmap *b, size_t bit_idx) 
{
  size_t idx = elem_idx (bit_idx);
  elem_type mask = bit_mask (bit_idx);

  /* This is equivalent to `b->bits[idx] |= mask' except that it
     is guaranteed to be atomic on a uniprocessor machine.  See
     the description of the OR instruction in [IA32-v2b]. */
  asm ("orl %1, %0" : "=m" (b->bits[idx]) : "r" (mask) : "cc");
}

/* Atomically sets the bit numbered BIT_IDX in B to false. */
void
bitmap_reset (struct bitmap *b, size_t bit_idx) 
{
  size_t idx = elem_idx (bit_idx);
  elem_type mask = bit_mask (bit_idx);

  /* This is equivalent to `b->bits[idx] &= ~mask' except that it
     is guaranteed to be atomic on a uniprocessor machine.  See
     the description of the AND instruction in [IA32-v2a]. */
  asm ("andl %1, %0" : "=m" (b->bits[idx]) : "r" (~mask) : "cc");
}

/* Atomically toggles the bit numbered IDX in B;
   that is, if it is true, makes it false,
   and if it is false, makes it true. */
void
bitmap_flip (struct bitmap *b, size_t bit_idx) 
{
  size_t idx = elem_idx (bit_idx);
  elem_type mask = bit_mask (bit_idx);

  /* This is equivalent to `b->bits[idx] ^= mask' except that it
     is guaranteed to be atomic on a uniprocessor machine.  See
     the description of the XOR instruction in [IA32-v2b]. */
  asm ("xorl %1, %0" : "=m" (b->bits[idx]) : "r" (mask) : "cc");
}

/* Returns the value of the bit numbered IDX in B. */
bool
bitmap_test (const struct bitmap *b, size_t idx) 
{
  ASSERT (b != NULL);
  ASSERT (idx < b->bit_cnt);
  return (b->bits[elem_idx (idx)] & bit_mask (idx)) != 0;
}

/* Setting and testing multiple bits. */

/* Sets all bits in B to VALUE. */
void
bitmap_set_all (struct bitmap *b, bool value) 
{
  ASSERT (b != NULL);

  bitmap_set_multiple (b, 0, bitmap_size (b), value);
}

/* Sets the CNT bits starting at START in B to VALUE. */
void
bitmap_set_multiple (struct bitmap *b, size_t start, size_t cnt, bool value) 
{
  size_t i;
  
  ASSERT (b != NULL);
  ASSERT (start <= b->bit_cnt);
  ASSERT (start + cnt <= b->bit_cnt);

  for (i = 0; i < cnt; i++)
    bitmap_set (b, start + i, value);
}

/* Returns the number of bits in B between START and START + CNT,
   exclusive, that are set to VALUE. */
size_t
bitmap_count (const struct bitmap *b, size_t start, size_t cnt, bool value) 
{
  size_t i, value_cnt;

  ASSERT (b != NULL);
  ASSERT (start <= b->bit_cnt);
  ASSERT (start + cnt <= b->bit_cnt);

  value_cnt = 0;
  for (i = 0; i < cnt; i++)
    if (bitmap_test (b, start + i) == value)
      value_cnt++;
  return value_cnt;
}

/* Returns true if any bits in B between START and START + CNT,
   exclusive, are set to VALUE, and false otherwise. */
bool
bitmap_contains (const struct bitmap *b, size_t start, size_t cnt, bool value) 
{
  size_t i;
  
  ASSERT (b != NULL);
  ASSERT (start <= b->bit_cnt);
  ASSERT (start + cnt <= b->bit_cnt);

  for (i = 0; i < cnt; i++)
    if (bitmap_test (b, start + i) == value)
      return true;
  return false;
}

/* Returns true if any bits in B between START and START + CNT,
   exclusive, are set to true, and false otherwise.*/
bool
bitmap_any (const struct bitmap *b, size_t start, size_t cnt) 
{
  return bitmap_contains (b, start, cnt, true);
}

/* Returns true if no bits in B between START and START + CNT,
   exclusive, are set to true, and false otherwise.*/
bool
bitmap_none (const struct bitmap *b, size_t start, size_t cnt) 
{
  return !bitmap_contains (b, start, cnt, true);
}

/* Returns true if every bit in B between START and START + CNT,
   exclusive, is set to true, and false otherwise. */
bool
bitmap_all (const struct bitmap *b, size_t start, size_t cnt) 
{
  return !bitmap_contains (b, start, cnt, false);
}

/* Finding set or unset bits. */

/* Finds and returns the starting index of the first group of CNT
   consecutive bits in B at or after START that are all set to
   VALUE.
   If there is no such group, returns BITMAP_ERROR. */
size_t
bitmap_scan (const struct bitmap *b, size_t start, size_t cnt, bool value) 
{
  ASSERT (b != NULL);
  ASSERT (start <= b->bit_cnt);

  if (cnt <= b->bit_cnt) 
    {
      size_t last = b->bit_cnt - cnt;
      size_t i;
      for (i = start; i <= last; i++)
        if (!bitmap_contains (b, i, cnt, !value))
          return i; 
    }
  return BITMAP_ERROR;
}

/* Finds the first group of CNT consecutive bits in B at or after
   START that are all set to VALUE, flips them all to !VALUE,
   and returns the index of the first bit in the group.
   If there is no such group, returns BITMAP_ERROR.
   If CNT is zero, returns 0.
   Bits are set atomically, but testing bits is not atomic with
   setting them. */
size_t
bitmap_scan_and_flip (struct bitmap *b, size_t start, size_t cnt, bool value)
{
  size_t idx = bitmap_scan (b, start, cnt, value);
  if (idx != BITMAP_ERROR) 
    bitmap_set_multiple (b, idx, cnt, !value);
  return idx;
}

/* File input and output. */

#ifdef FILESYS
/* Returns the number of bytes needed to store B in a file. */
size_t
bitmap_file_size (const struct bitmap *b) 
{
  return byte_cnt (b->bit_cnt);
}

/* Reads B from FILE.  Returns true if successful, false
   otherwise. */
bool
bitmap_read (struct bitmap *b, struct file *file) 
{
  bool success = true;
  if (b->bit_cnt > 0) 
    {
      off_t size = byte_cnt (b->bit_cnt);
      success = file_read_at (file, b->bits, size, 0) == size;
      b->bits[elem_cnt (b->bit_cnt) - 1] &= last_mask (b);
    }
  return success;
}

/* Writes B to FILE.  Return true if successful, false
   otherwise. */
bool
bitmap_write (const struct bitmap *b, struct file *file)
{
  off_t size = byte_cnt (b->bit_cnt);
  return file_write_at (file, b->bits, size, 0) == size;
}
#endif /* FILESYS */

/* Debugging. */

/* Dumps the contents of B to the console as hexadecimal. */
void
bitmap_dump (const struct bitmap *b) 
{
  hex_dump (0, b->bits, byte_cnt (b->bit_cnt), false);
}



/* Best Fit에 이용 scan 변형
   다음에 set되어 있는 자료 주소 리턴
   or 다음 자료가 없거나 탐색결과가 마지막일 시에는 b->bit_cnt가 return
*/
size_t
bitmap_return_next_1 (const struct bitmap *b, size_t start, size_t cnt, bool value) 
{
  ASSERT (b != NULL);
  ASSERT (start <= b->bit_cnt);

  size_t last = b->bit_cnt;
  if(start >= last)
    return b->bit_cnt;

  size_t i;
    for (i = start; i < last; i++)
      if (bitmap_test(b, i)) // i항목에 1이 있으면 1을 return
        return i; 
    
  return b->bit_cnt;
}
/* Best Fit(scan_and_flip 응용)
   start = 0 이라 매개변수 없앰
   b에는 pool->used_map로 들어옴 (user 공간), cnt 필요 공간 size
*/
size_t
bitmap_best (struct bitmap *b, size_t cnt, bool value)
{
  ASSERT (b != NULL);

  size_t idx = bitmap_scan(b, 0, cnt, value);
  size_t next_1 = bitmap_return_next_1(b, idx+cnt+1, cnt, !value);
  size_t size = next_1 - idx;

  size_t best_size = size;
  size_t best = idx;
  printf("best_size : %d   ", best_size);
  while( (next_1 != b->bit_cnt) && (idx != BITMAP_ERROR) ){
    idx = bitmap_scan(b,next_1, cnt, value);
    next_1 = bitmap_return_next_1(b, idx+cnt+1, cnt, !value);
    size = next_1-idx;
    printf("size : %d \n", size);
    if(idx != BITMAP_ERROR && best_size >= size){
      best = idx;
      best_size = size;
      printf("new best_size : %d   ", best_size);
    }
  }
  if (idx != BITMAP_ERROR) 
    bitmap_set_multiple (b, best, cnt, !value);
  return best;
}

size_t
bitmap_buddy(struct bitmap *b, size_t cnt, bool value){
  ASSERT (b != NULL);
  struct buddy_node *buddy = b_list->head;
  size_t bitmap_size = buddy->pages_size;
  size_t bitmap_start_idx =  buddy->idx;
  size_t idx;
  printf("bitma_buddy start\n");
  idx = bitmap_buddy_scan(buddy,cnt);
  printf("bitma_buddy finish\n");
   if (idx != BITMAP_ERROR) 
    bitmap_set_multiple (b, idx, cnt, !value);
  return idx;
}

size_t
bitmap_buddy_scan(struct buddy_node* parent,size_t cnt){
  size_t bitmap_size = parent->pages_size;
  size_t start_idx = parent->idx;
  size_t idx = -1;
  printf("bitma_buddy_scan start, parent->pagesize : %d, parent->idx : %d, page_cnt : %d\n",bitmap_size, start_idx,cnt); 
  if(cnt > bitmap_size){
    //bitmap 보다 더 큰 page 개수를 요구하면 , error를 return 한다.
    return BITMAP_ERROR;
  }else if(cnt <= bitmap_size && cnt > bitmap_size/2){
     if(parent->buddy_child_right == NULL, parent->buddy_child_left == NULL){
       parent->flag = true;
      return start_idx;
     }
     return idx;
  }else if(cnt <= bitmap_size/2){
    printf("this come here?\n");
    //4가지 경우로 나누어진다.
    //현재 현 노드의 자시기 없을경우
    if(parent->buddy_child_right == NULL, parent->buddy_child_left == NULL){
      printf("this come here????\n");
      //오른쪽과 왼쪽의 자식노드를 생성한다.
      struct buddy_node* right_child = (struct buddy_node *)malloc(sizeof(struct buddy_node));
      struct buddy_node* left_child = (struct buddy_node *)malloc(sizeof(struct buddy_node));
      //오른쪽과 왼쪽 자식노드의 bitmap size를 계산해서 넣어준다.
      left_child->pages_size =  bitmap_size/2;
      right_child->pages_size = bitmap_size/2 + bitmap_size%2;

      //오른쪽과 왼쪽의 시작 idx를 계산해서 넣어준다.
      left_child->idx = start_idx;
      right_child->idx = start_idx+bitmap_size/2;
      //flag도 초기화해준다.
      left_child->flag = false;
      right_child->flag = false;
      //자기 자신은 leaf노드가 아니기때문에 is_leaf를 false로 바꿔주고, 자식노드는  lead node이기 떄문에 ture로 초기화한다.
      parent->is_leaf = false;

      left_child->is_leaf = true;
      right_child->is_leaf = true;

      left_child->buddy_child_left = NULL;
      left_child->buddy_child_right = NULL;
      right_child->buddy_child_right = NULL;
      right_child->buddy_child_left = NULL;
      //부모노드와 자식노드를 이어준다.
      parent->buddy_child_left = left_child;
      parent->buddy_child_right = right_child;
      // 해당 노드를 왼쪽부터 실행한다.
      printf("this come here!!!!!!\n");
      return bitmap_buddy_scan(parent->buddy_child_left,cnt);
    }else{
    //현 노드의 자식이 있고, 둘다 사용중일 경우
    struct buddy_node* left_child = parent->buddy_child_left;
    struct buddy_node* right_child = parent->buddy_child_right;
      if(left_child->flag == true && right_child->flag == true){
        return idx;
      }else if(left_child->flag == true && right_child->flag == false){
        return bitmap_buddy_scan(parent->buddy_child_right,cnt);
      }else if(left_child->flag == false && right_child->flag == true){
        return bitmap_buddy_scan(parent->buddy_child_left,cnt);
      }else{
        printf("that come here!!!!\n");
        idx =  bitmap_buddy_scan(parent->buddy_child_left,cnt);
        if(idx ==-1){
          idx =  bitmap_buddy_scan(parent->buddy_child_right,cnt);
          if(idx == -1){
            idx = BITMAP_ERROR;
          }
        }
        return idx;
      }

    //현 노드의 자식은 있고, 하나만 사용중일 경우
    }  
  }
  return BITMAP_ERROR;
}
//list를 초기화환다.
void
bitmap_buddy_init(size_t b_start, size_t cnt){

  printf("budy init start");
  //list를 할당하고,하나의 노드를 연결한다.
  b_list = (buddy_list*)malloc(sizeof(buddy_list));
  //노드 하나를 만들어서 list에 넣어준다.
  struct  buddy_node* node = (struct buddy_node *)malloc(sizeof(struct buddy_node));
  b_list->head = node;
  b_list->tail = node;
  //pagesize 값은 bitamp의 전체 수로 초기화한다.
  node->idx = 0;
  node->pages_size = cnt;
  //flag를 false라고 초기화한다.
  node->flag = false;
  node->is_leaf = true;
  printf("budy init finish\n");
}

void
bitmap_buddy_free(size_t page_idx,size_t page_cnt){
  struct buddy_node* node = b_list->head; 
  buddy_list_search_leafnode(node,page_idx,page_cnt);
  buddy_merge_leafnode(node);
}
int
buddy_list_search_leafnode(struct buddy_node* parent,size_t page_idx,size_t page_cnt){
  int stop = 0;
  if(parent->buddy_child_right == NULL && parent->buddy_child_left ==NULL && parent->flag == true){
    if(page_idx>= parent->idx && page_idx < parent->idx + parent->pages_size){
      ASSERT(page_cnt< parent->pages_size);
      parent->flag = false;
      return 1;
  }
  stop = buddy_list_search_leafnode(parent->buddy_child_left,page_idx,page_cnt);
  if(stop == 1){
    return stop;
  }
  stop = buddy_list_search_leafnode(parent->buddy_child_right,page_idx,page_cnt);
    if(stop == 1){
      return stop;
   }
  }
}

int 
buddy_merge_leafnode(struct buddy_node* parent){
  int check= 0;
    struct buddy_node* left_child = parent->buddy_child_left;
    struct buddy_node* right_child = parent->buddy_child_right;
    //왼쪽의 leaf노드이고, 사용중인 것이 아니면, check 증가
    if(left_child->is_leaf == true && left_child->flag == false ){
      check++;
    }else if(left_child->is_leaf == false){
      check += buddy_merge_leafnode(left_child);
    }else if(left_child->is_leaf == true && left_child->flag == true ){
      return 0;
    }
    //오른쪽이 leaf노드이고 사용중이지 않으면 check 증가
    if(right_child->is_leaf == true && right_child->flag == false){
      check++;
    }else if(right_child->is_leaf == false){
      check += buddy_merge_leafnode(right_child);
    }else if(right_child->is_leaf == true && right_child->flag == true ){
      return 0;
    }
    // check가 2라면, 자식 노드들 제거
    if(check ==2){
      free(left_child);
      free(right_child);

      //parent 도 초기화
      parent->buddy_child_left = NULL;
      parent->buddy_child_right = NULL;
      return 1;
    }
}

