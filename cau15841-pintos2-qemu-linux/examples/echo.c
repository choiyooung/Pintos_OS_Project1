#include <stdio.h>
#include <syscall.h>
#include "threads/palloc.h"

int
main (int argc, char** argv)
{
  int i;
  
  for(i =0;i<argc;i++){
	printf("%s ",argv[i]);
  }
  printf("\n");

  printf("hi\n");
  return EXIT_SUCCESS;
}
