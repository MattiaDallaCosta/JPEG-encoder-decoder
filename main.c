#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/msg.h>
#include <sys/ipc.h>
#include <sys/types.h>
#include <errno.h>
#include <fcntl.h>

#include "include/encoder.h"
#include "include/brain.h"
#include "include/structs.h"

int raw[3][PIX_LEN];
int subsampled[3][PIX_LEN/16];
int * ordered_dct[3];
int stored = 0;
int main(int argc, char ** argv) {
	FILE* f_in;
  creat("/tmp/queue", 0644);
  key_t key = ftok("/tmp/queue", 1);
  int qid = msgget(key, O_RDWR|0644);
  msgctl(qid, IPC_RMID, NULL);
  qid = msgget(key, O_RDWR|IPC_CREAT|0644);
  int i = 0;
  char text[100];
  msg_t msg;
  char savedName[100];
  area_t diffDims[20];
  while (1) {
    msgrcv(qid, (void *) &msg, 1028, 1, 0);
    strcpy(text, msg.mtext);
    fflush(NULL);
    if(! strcmp(text, "exit")){
      printf("\033[0;033mExititng\033[0m\n");
      msgctl(qid, IPC_RMID, NULL);
      return 0;
    }
    if(strcmp(text, "saved") == 0){
      if(stored){
        printf("\033[0;032mCreating saved file\033[0m\n");
        FILE * f_sub = fopen(savedName, "w");
        writePpm(f_sub, subsampled);
        fclose(f_sub);
      } else printf("\033[0;031mNo previous image stored\033[0m\n");
      continue;
    }
    if (!(f_in = fopen(text, "r"))) {
      fprintf(stderr, "\033[0;031mUnable to open file\033[0m\n");
      continue;
    }
	  if (readPpm(f_in, raw)) {
      printf("\033[0;031mError reading input file\033[0m\n");
	  	fclose(f_in);
	  	continue;
	  }
	  fclose(f_in);
    subsample(raw, subsampled);
    if(stored){
      int different = compare(subsampled, diffDims);
      printf("%i\n", different);
      if (different) {
        printf("images are different\n");
        store(subsampled);
        getSavedName(text, savedName);
        printf("post store\n");
        for (int i = 0; i < different; i++) {
          encodeNsend(text, raw, diffDims[i], i);
          printf("component #%i x: %i y: %i w: %i h: %i\n", i, diffDims[i].x, diffDims[i].h, diffDims[i].w, diffDims[i].h);
        }
        printf("post encodeNsend\n");
      } else printf("images are the same\n");
    } else {
      printf("images are different\n");
      stored = 1;
      store(subsampled);
      getSavedName(text, savedName);
      area_t fullImage;
      fullImage.x = fullImage.y = 0;
      fullImage.w = WIDTH;
      fullImage.h = HEIGHT;
      encodeNsend(text, raw, fullImage, -1);
    }
    sleep(1);
  }
	return 0;
}
