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

int main(int argc, char ** argv) {
	FILE* f_in;
  creat("/tmp/queue", 0644);
  key_t key = ftok("/tmp/queue", 1);
  int qid = msgget(key, IPC_EXCL|IPC_CREAT|0644);
  if (qid == -1) {
    msg_t msg;
    msg.mtype = 1;
    strcpy(msg.mtext, "exit");
    qid = msgget(key, O_RDWR|0644);
    msgsnd(qid, &msg, strlen(msg.mtext), 0);
    printf("\033[0;031mClosing existing queue\033[0m\n");
    msgctl(qid, IPC_RMID, NULL);
  }
  qid = msgget(key, O_RDWR|IPC_CREAT|0644);
  int i = 0;
  char text[100];
  char savedName[100];
  area_t diffDims[20];
  while (1) {
    msg_t msg;
    printf("pre msgrcv()\n");
    printf("msgrcv = %zi\n", msgrcv(qid, &msg, 104, 1, MSG_NOERROR));
    printf("post msgrcv()\n");
    strcpy(text, msg.mtext);
    printf("strlen msg: %lu\n", strlen(text));
    for (int i = 0; i < strlen(msg.mtext); i++) {
      printf("%c\n", text[i]);
    }
    printf("\n");
    printf("message: %s %s\n",text, msg.mtext);

    printf("is the message received equal to exit? %i\n", strcmp(text, "exit")/*  ? "No" : "Yes" */);
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
        printf("Images are different\n");
        store(subsampled);
        getSavedName(text, savedName);
        printf("Post store\n");
        for (int i = 0; i < different; i++) {
          encodeNsend(text, raw, diffDims[i], i);
          printf("Component #%i x: %i y: %i w: %i h: %i\n", i, diffDims[i].x, diffDims[i].h, diffDims[i].w, diffDims[i].h);
        }
        printf("Post encodeNsend\n");
      } else printf("ImPges are the same\n");
    } else {
      printf("No image stored\nStoring and encoding\n");
      stored = 1;
      store(subsampled);
      getSavedName(text, savedName);
      area_t fullImage;
      fullImage.x = fullImage.y = 0;
      fullImage.w = WIDTH;
      fullImage.h = HEIGHT;
      printf("pre-encoding\n");
      encodeNsend(text, raw, fullImage, -1);
    }
    printf("Done\n");
    sleep(1);
  }
	return 0;
}
