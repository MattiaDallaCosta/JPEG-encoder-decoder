#include <stdio.h>
#include <string.h>
#include <sys/msg.h>
#include <sys/ipc.h>
#include <sys/types.h>
#include <errno.h>
#include <fcntl.h>

#include "include/encoder.h"
#include "include/brain.h"

typedef struct {
  long mtype;
  char mtext[1024];
  int size;
} msg_t;

int raw[3][PIX_LEN];
int subsampled[3][PIX_LEN/9];
int ordered_dct[3][PIX_LEN];
Area diffArea;

int main(int argc, char ** argv) {
 //  char name[1024];
	FILE* f_in;

  // if (argc != 2) {
  //   printf("expected input = %s <file> \nWhere:\n\t<file> is the file to be converted\n", argv[0]);
  //   return -1;
  // }
	// if (f_in == NULL) {
 //    fprintf(stderr, "\033[0;31mERROR: can't open file %s\033[0m\n", argv[1]);
	// 	return -1;
	// }
  creat("/tmp/queue", 0644);
  key_t key = ftok("/tmp/queue", 1);
  int qid = msgget(key, O_RDWR|0644);
  msgctl(qid, IPC_RMID, NULL);
  qid = msgget(key, O_RDWR|IPC_CREAT|0644);
  msg_t msg;
  int i = 0;
  while (++i) {
    msgrcv(qid, (void *) &msg, 1024, 1, 0);
    printf("Comparator: %s\n", msg.mtext);
    if (!(f_in = fopen(msg.mtext, "r"))) {
      printf("\033[0;031mUnable to open file\033[0m\n");
      return -1;
    }
	  if (read_ppm(f_in, raw)) {
	  	fclose(f_in);
	  	return -1;
	  }
	  fclose(f_in);
    subsample(raw, subsampled);
    diffArea = compare(subsampled);
    if (validArea(diffArea)) {
      printf("images are different\n");
      store(subsampled);
      dims_t diffDims;
      // diffDims.w = diffArea
      encodeNsend(msg.mtext, raw, diffDims);
    } else printf("images are the same\n");
  }
	return 0;
}
