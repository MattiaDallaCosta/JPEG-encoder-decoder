#include <stdio.h>
#include <string.h>
#include <sys/types.h>

#include "include/encoder.h"

int raw[3][PIX_LEN];
int ordered_dct[3][PIX_LEN];

int main(int argc, char ** argv) {
  char name[1024];

  if (argc != 2) {
    printf("expected input = %s <file> \nWhere:\n\t<file> is the file to be converted\n", argv[0]);
    return -1;
  }
	
  FILE* f_in = fopen(argv[1], "r");

	if (f_in == NULL) {
    fprintf(stderr, "\033[0;31mERROR: can't open file %s\033[0m\n", argv[1]);
		return -1;
	}
	if (read_ppm(f_in, raw)) {
		fclose(f_in);
		return -1;
	}
	fclose(f_in);
  encodeNsend(argv[1], raw);
	return 0;
}
