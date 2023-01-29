#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include "include/encoder.h"

int main (int argc, char *argv[])
{
  char newname[100];
  // getName(argv[1], newname, -1);
  FILE * Cr = fopen("dct_Cr", "w+");
  fprintf(Cr, "ciao");
  fclose(Cr);
  int fd = open("ciao", O_RDWR | O_CREAT);
  printf("fd: %d\n",fd);  
  return 0;
}


