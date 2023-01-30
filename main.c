#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <sys/msg.h>
#include <sys/ipc.h>
#include <sys/types.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/time.h>

#include "include/encoder.h"
#include "include/brain.h"
#include "include/structs.h"

uint8_t raw[3][PIX_LEN];
uint8_t rawA[3*PIX_LEN];
uint8_t subsampled[3][PIX_LEN/16];
uint8_t stored = 0;
int qid;
struct timeval start, end, store_t, read_t, comp_t, op_t, sub_t, appo;
unsigned long millielapsed;
unsigned long secelapsed;

void handleClose(int signo){
  msgctl(qid, IPC_RMID, NULL);
  remove("/tmp/queue");
  exit(1);
}

int main(int argc, char ** argv) {
  signal(SIGTERM, handleClose);
	FILE* f_in;
  creat("/tmp/queue", 0644);
  key_t key = ftok("/tmp/queue", 1);
  qid = msgget(key, O_RDWR|IPC_EXCL|IPC_CREAT|0644);
  if (qid == -1) {
    msg_t msg;
    msg.mtype = 1;
    msg.len = 0;
    strcpy(msg.mtext, "exit");
    qid = msgget(key, O_RDWR|0644);
    msgsnd(qid, &msg, strlen(msg.mtext)+sizeof(msg.len), 0);
    printf("\033[0;031mClosing existing queue\033[0m\n");
    msgctl(qid, IPC_RMID, NULL);
    qid = msgget(key, O_RDWR|IPC_CREAT|0644);
  }
  int i = 0;
  char text[100];
  char savedName[100];
  char newname[100];
  area_t diffDims[1000];
  while (1) {
    msg_t msg;
    msgrcv(qid, &msg, 100, 1, 0);
    gettimeofday(&start, NULL);
    strcpy(text, msg.mtext);

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
        gettimeofday(&store_t, NULL);
        millielapsed = (store_t.tv_usec - start.tv_usec)/1000;
        secelapsed = (store_t.tv_sec - start.tv_sec)/1000;
        printf("storing time: %li:%li:%li.%s%li\n",(secelapsed/3600)%60, (secelapsed/60)%60, (secelapsed)%60, ((millielapsed)%1000) > 99 ? "" : (((millielapsed)%1000) > 9 ? "0" : "00"), (millielapsed)%1000);
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
    for (int i = 0; i < PIX_LEN; i++) {
      rawA[3*i] = raw[0][i];
      rawA[3*i+1] = raw[1][i];
      rawA[3*i+2] = raw[2][i];
    }
    gettimeofday(&read_t, NULL);
    millielapsed = (read_t.tv_usec - start.tv_usec)/1000;
    secelapsed = (read_t.tv_sec - start.tv_sec);
    printf("reading time:                         %li:%li:%li.%s%li\n",(secelapsed/3600)%60, (secelapsed/60)%60, (secelapsed)%60, ((millielapsed)%1000) > 99 ? "" : (((millielapsed)%1000) > 9 ? "0" : "00"), (millielapsed)%1000);
    subsample(raw, subsampled);
    gettimeofday(&sub_t, NULL);
    millielapsed = (sub_t.tv_usec - read_t.tv_usec)/1000;
    secelapsed = (sub_t.tv_sec - read_t.tv_sec)/1000;
    printf("subsampling time:                     %li:%li:%li.%s%li\n",(secelapsed/3600)%60, (secelapsed/60)%60, (secelapsed)%60, ((millielapsed)%1000) > 99 ? "" : (((millielapsed)%1000) > 9 ? "0" : "00"), (millielapsed)%1000);
    if(stored){
      int different = compare(subsampled, diffDims);
      gettimeofday(&comp_t, NULL);
      millielapsed = (comp_t.tv_usec - sub_t.tv_usec)/1000;
      secelapsed = (comp_t.tv_sec - sub_t.tv_sec);
      printf("comparison time:                      %li:%li:%li.%s%li\n",(secelapsed/3600)%60, (secelapsed/60)%60, (secelapsed)%60, ((millielapsed)%1000) > 99 ? "" : (((millielapsed)%1000) > 9 ? "0" : "00"), (millielapsed)%1000);
      if (different) {
        printf("\033[0;036mImages are different\033[0m\n");
        store(subsampled);
        gettimeofday(&store_t, NULL);
        millielapsed = (store_t.tv_usec - comp_t.tv_usec)/1000;
        secelapsed = (store_t.tv_sec - comp_t.tv_sec);
        printf("storing time:                         %li:%li:%li.%s%li\n",(secelapsed/3600)%60, (secelapsed/60)%60, (secelapsed)%60, ((millielapsed)%1000) > 99 ? "" : (((millielapsed)%1000) > 9 ? "0" : "00"), (millielapsed)%1000);
        for (int i = 0; i < different; i++) {
          gettimeofday(&appo, NULL);
          printf("pre getName\n");
          getName(text, newname, i);
          // sprintf(newname, "out-%i.jpg", i);
          enlargeAdjust(&diffDims[i]);
          encodeNsend(newname, raw, diffDims[i]);
          gettimeofday(&op_t, NULL);
          millielapsed = (op_t.tv_usec - appo.tv_usec)/1000;
          secelapsed = (op_t.tv_sec - appo.tv_sec);
          printf("\033[1Aconversion time for diff #%i:          %li:%li:%li.%s%li\n", i, (secelapsed/3600)%60, (secelapsed/60)%60, (secelapsed)%60, ((millielapsed)%1000) > 99 ? "" : (((millielapsed)%1000) > 9 ? "0" : "00"), (millielapsed)%1000);
        }
      } else printf("\033[0;036mImages are the same\033[0m\n");
    } else {
      printf("\033[0;036mNo image stored\nStoring and encoding\033[0m\n");
      stored = 1;
      store(subsampled);
      gettimeofday(&store_t, NULL);
      millielapsed = (store_t.tv_usec - sub_t.tv_usec)/1000;
      secelapsed = (store_t.tv_sec - sub_t.tv_sec);
      printf("storing time:                         %li:%li:%li.%s%li\n",(secelapsed/3600)%60, (secelapsed/60)%60, (secelapsed)%60, ((millielapsed)%1000) > 99 ? "" : (((millielapsed)%1000) > 9 ? "0" : "00"), (millielapsed)%1000);
      getSavedName(text, savedName);
      area_t fullImage;
      fullImage.x = 0;
      fullImage.y = 0;
      fullImage.w = WIDTH;
      fullImage.h = HEIGHT;
      // fullImage.w = 16;
      // fullImage.h = 16;
      getName(text, newname, -1);
      // encodeNsend(newname, raw, fullImage);
      encodeNsend_blocks(newname, rawA, fullImage);
      gettimeofday(&op_t, NULL);
      millielapsed = (op_t.tv_usec - store_t.tv_usec)/1000;
      secelapsed = (op_t.tv_sec - store_t.tv_sec);
      printf("conversion time for full image:       %li:%li:%li.%s%li\n",(secelapsed/3600)%60, (secelapsed/60)%60, (secelapsed)%60, ((millielapsed)%1000) > 99 ? "" : (((millielapsed)%1000) > 9 ? "0" : "00"), (millielapsed)%1000);
    }
    gettimeofday(&end, NULL);
    millielapsed = (end.tv_usec - start.tv_usec)/1000;
    secelapsed = (end.tv_sec - start.tv_sec);
    printf("comparison and conversion total time: %li:%li:%li.%s%li\n",(secelapsed/3600)%60, (secelapsed/60)%60, (secelapsed)%60, ((millielapsed)%1000) > 99 ? "" : (((millielapsed)%1000) > 9 ? "0" : "00"), (millielapsed)%1000);
    printf("\033[0;36mDone\033[0m\n");
    // sleep(1);
  }
	return 0;
}
