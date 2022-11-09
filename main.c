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
    msg_t msg;
  char text[100];
  char savedName[100];
  area_t diffDims[20];
  while (1) {
    msgrcv(qid, (void *) &msg, 1028, 1, 0);
    printf("post msgrcv\n");
    strcpy(text, msg.mtext);
    printf("%s\n", msg.mtext);
    printf("%lu %lu\n",strlen(msg.mtext), strlen(text));
    printf("%s\n", text);
    for (int i = 0; i < strlen(text); i++) printf("%i\n", text[i]);
    printf("\n");
    for (int i = 0; i < strlen(msg.mtext); i++) printf("%i\n", msg.mtext[i]);
    printf("\n");
    printf("%i\n", msg.len);
    // printf("string %s is equal to end ? %s [%i]\n", text, comp == 0 ? "Yes" : "No", comp);
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
      } else printf("\033[0;032mNo previous image stored\033[0m\n");
      // msgctl(qid, IPC_RMID, NULL);
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
    int different = compare(subsampled, diffDims);
    if (different) {
      printf("images are different\n");
      store(subsampled);
      stored = 1;
      getSavedName(text, savedName);
      printf("post store\n");
      for (int i = 0; i < different; i++) {
      encodeNsend(text, raw, diffDims[i]);
      }
      printf("post encodeNsend\n");
    } else printf("images are the same\n");
    sleep(1);
  }
	return 0;
}
