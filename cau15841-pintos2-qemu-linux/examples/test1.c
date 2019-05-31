#include <stdio.h>
#include <syscall.h>
#include <threads/palloc.h>
#include <threads/palloc.c>
#include <lib\kernel\bitmap.h>
#include <lib\kernel\bitmap.c>
#include <userprog/pagedir.h>

int
main (int argc, char** argv)
{
    size_t i;
    size_t start = 0;
    size_t last = user_pool.used_map->bit_cnt;
    int j = 0;

    /*
    출력 이거말고
    bitmap_dump(user_pool.used_map);
    이것도 써볼것
    */

    //0 ~ 30까지의 페이지 set(true로 만듬)
    bitmap_set_multiple(user_pool.used_map, 0, 30, true);
    //간격 40
    bitmap_set_multiple(user_pool.used_map, 70, 90, true);
    //간격 30
    bitmap_set_multiple(user_pool.used_map, 120, 150, true);

    //전체 bitmap 출력
    for(i = start; i <= last; i++){
        if(bitmap_test (user_pool.used_map, 0) == true){
            printf("1");
        }       
        else{
            printf("0");
        }
        j++;
        if(j%10 == 0)
            printf("\n");
    }
    j = 0;

    // //bestfit으로 할당
    // size_t page_idx = bitmap_best(user_pool.used_map, 20, false);

    //firstfit OR nextfit으로 할당
    size_t page_idx = bitmap_scan_and_flip(user_pool.used_map, 0, 20, false);

    //page_idx가 간격 30인 곳으로 나와야 맞는것
    printf("page_idx = %u\n", page_idx);
    
    //전체 bitmap 출력
    for(i = start; i <= last; i++){
        if(bitmap_test (user_pool.used_map, 0) == true){
            printf("1");
        }       
        else{
            printf("0");
        }
        j++;
        if(j%10 == 0)
            printf("\n");
    }
    j = 0;
    
  return EXIT_SUCCESS;
}
