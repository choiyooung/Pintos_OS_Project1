#include <bitmap.h>
#include "threads/vaddr.h"
#include "devices/block.h"
#include "vm/swap.h"

static struct block *swap_block; // block_get_role(BLOCK_SWAP)으로 스왑장치에 대한 Block Device 구조체 포인터 가져옴
static struct bitmap *swap_available; //true = 빔(스왑들여오기 가능)

static size_t swap_size; // init에서 얼마나 페이지가 스왑될수 있는지로 초기화(페이지 수)
static const size_t SECTORS_PER_PAGE = PGSIZE / BLOCK_SECTOR_SIZE; // 페이지당 block sector 개수

void
vm_swap_init ()
{
  ASSERT (SECTORS_PER_PAGE > 0);

  // 스왑장치 포인터 매핑
  swap_block = block_get_role(BLOCK_SWAP);
  if(swap_block == NULL) {
    PANIC ("Error: Can't initialize(mapping) swap block");
  }

  swap_size = block_size(swap_block) / SECTORS_PER_PAGE; //총 저장될 수 있는 페이지 개수 저장(swap_block의 블럭 sector 개수/페이지당 섹터갯수)
  swap_available = bitmap_create(swap_size); //페이지 개수 만큼 bitmap 생성
  bitmap_set_all(swap_available, true); //전부 스왑으로 들어올 수 있음으로 true로 초기화
}

//블록은 내부 동기화 되어있어서 따로 장치 필요없다고 써있음(block.c)

swap_index_t vm_swap_out (void *page)
{
  ASSERT (page >= PHYS_BASE); //user page인가
  ////////////////////////////////bitmap 이름, start, 길이, value
  size_t swap_index = bitmap_scan (swap_available, 0, 1, true); //페이지 한개 들어갈 수 있는 index 저장
  
  size_t i;
  for (i = 0; i < SECTORS_PER_PAGE; ++ i) { // sector별로 block에 작성해준다.(block이 sector 단위로 구성되어있어서)
    ////////////swap_block(block device),   sector번호,          스왑공간에 쓸 내용의 주소
    block_write(swap_block, swap_index * SECTORS_PER_PAGE + i, page + (BLOCK_SECTOR_SIZE * i) );
  }
  bitmap_set(swap_available, swap_index, false); //해당 공간에 페이지 스왑 시켰음으로 false로 표시
  return swap_index;
}


void vm_swap_in (swap_index_t swap_index, void *page)
{
  ASSERT (page >= PHYS_BASE);
  ASSERT (swap_index < swap_size);

  if (bitmap_test(swap_available, swap_index) == true) { //빈 swap공간 오류
    PANIC ("Error, invalid read access to unassigned swap block");
  }

  size_t i;
  for (i = 0; i < SECTORS_PER_PAGE; ++ i) {
    block_read (swap_block, swap_index * SECTORS_PER_PAGE + i, page + (BLOCK_SECTOR_SIZE * i) );
    ////////////swap_block(block device),   sector번호,          쓰여질 페이지 주소
  }

  bitmap_set(swap_available, swap_index, true); //다시 swap 공간 가용으로 전환(true)
}

void
vm_swap_free (swap_index_t swap_index)
{
  ASSERT (swap_index < swap_size);
  if (bitmap_test(swap_available, swap_index) == true) {
    PANIC ("Error, invalid free request to unassigned swap block");
  }
  bitmap_set(swap_available, swap_index, true);
}