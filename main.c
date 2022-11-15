#include <stdlib.h>
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

uint8_t raw[3][PIX_LEN];
uint8_t subsampled[3][PIX_LEN/16];
uint8_t stored = 0;
int qid;

int main(int argc, char ** argv) {
	FILE* f_in;
  creat("/tmp/queue", 0644);
  key_t key = ftok("/tmp/queue", 1);
  qid = msgget(key, O_RDWR|IPC_EXCL|IPC_CREAT|0644);
  printf("%i\n", qid);
  if (qid == -1) {
    msg_t msg;
    msg.mtype = 1;
    strcpy(msg.mtext, "exit");
    qid = msgget(key, O_RDWR|0644);
    msgsnd(qid, &msg, strlen(msg.mtext), 0);
    printf("\033[0;031mClosing existing queue\033[0m\n");
    msgctl(qid, IPC_RMID, NULL);
    qid = msgget(key, O_RDWR|IPC_CREAT|0644);
  }
  int i = 0;
  char text[100];
  char savedName[100];
  char newname[100];
  area_t diffDims[20];
  while (1) {
    msg_t msg;
    printf("pre msgrcv()\n");
    // printf("msgrcv = %zi\n", msgrcv(qid, &msg, 1000, 1, 0));
    msgrcv(qid, &msg, 100, 1, 0);
    // printf("%i\n", msg.mtext);
    perror("Error of msgrcv");
    printf("post msgrcv()\n");
    strcpy(text, msg.mtext);
    printf("strlen msg: %lu\n", strlen(text));
    printf("\n");
    printf("message: %s %s\n",text, msg.mtext);

    // printf("%i\n", msg.mtext);
    fflush(NULL);
    if(! strcmp(text, "exit")){
      printf("\033[0;033mExititng\033[0m\n");
      msgctl(qid, IPC_RMID, NULL);
      return 0;
    }
    // printf("%i\n", msg.mtext);
    if(strcmp(text, "saved") == 0){
      if(stored){
        printf("\033[0;032mCreating saved file\033[0m\n");
        FILE * f_sub = fopen(savedName, "w");
        writePpm(f_sub, subsampled);
        fclose(f_sub);
      } else printf("\033[0;031mNo previous image stored\033[0m\n");
      continue;
    }
    // printf("%i\n", msg.mtext);
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
    // printf("%i\n", msg.mtext);
    subsample(raw, subsampled);
    if(stored){
      int different = compare(subsampled, diffDims);
      printf("%i\n", different);
      if (different) {
        printf("Images are different\n");
        store(subsampled);
        getSavedName(text, savedName);
        printf("Post store\n");
        for (int i = 0; i < different; i++) {
          // sprintf(newname, "out-%i.jpg\n",i);
          getName(text, newname, i);
          encodeNsend(text, raw, diffDims[i]);
          printf("Component #%i x: %i y: %i w: %i h: %i\n", i, diffDims[i].x, diffDims[i].h, diffDims[i].w, diffDims[i].h);
        }
        printf("Post encodeNsend\n");
      } else printf("Images are the same\n");
    } else {
      // printf("%i\n", msg.mtext);
      printf("No image stored\nStoring and encoding\n");
      stored = 1;
      store(subsampled);
      // printf("%i\n", msg.mtext);
      getSavedName(text, savedName);
      // printf("%i\n", msg.mtext);
      area_t fullImage;
      fullImage.x = fullImage.y = 0;
      fullImage.w = WIDTH;
      fullImage.h = HEIGHT;
      printf("pre-encoding\n");
      // printf("%i\n", msg.mtext);
      getName(text, newname, -1);
      // printf("%i\n", msg.mtext);
      // strcpy(newname, "out.jpg");
      encodeNsend(newname, raw, fullImage);
      // printf("%i\n", msg.mtext);
    }
    printf("Done\n");
    sleep(1);
    // printf("%i\n", msg.mtext);
  }
	return 0;
}
