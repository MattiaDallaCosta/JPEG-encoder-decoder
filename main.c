#include <stdint.h>
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

#include "include/define.h"
#include "include/brain.h"
#include "include/encoder.h"
#include "include/structs.h"

uint8_t raw[3*PIX_LEN];
uint8_t sub[3*PIX_LEN/16];
uint8_t jpg[3*PIX_LEN];

int16_t ordered_dct_Y[PIX_LEN];
int16_t ordered_dct_Cb[PIX_LEN/4];
int16_t ordered_dct_Cr[PIX_LEN/4];

uint8_t saved[3*PIX_LEN/16];
area_t diffDims[20];
pair_t differences[2][WIDTH/8];
huff_code Luma[2];
huff_code Chroma[2];
int len = 0;
uint8_t different = 0;

area_t fullImage = { .x = 0, .y = 0, .w = WIDTH, .h = HEIGHT };

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
  // Main loop
  while(1) {
    msg_t msg;
    msgrcv(qid, &msg, 100, 1, 0);
    gettimeofday(&start, NULL);
      different = 0;
    strcpy(text, msg.mtext);

    if(! strcmp(text, "exit")){
      printf("\033[0;033mExititng\033[0m\n");
      msgctl(qid, IPC_RMID, NULL);
      return 0;
    }
    if(strcmp(text, "saved") == 0){
      if(stored){
        printf("\033[0;032mCreating saved file\033[0m\n");
        getSavedName(text, savedName);
        FILE * f_sub = fopen(savedName, "w");
        writePpm(f_sub, saved);
        fclose(f_sub);
      } else printf("\033[0;031mNo previous image stored\033[0m\n");
      printf("\033[0;36mDone\033[0m\n");
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
    gettimeofday(&read_t, NULL);
    millielapsed = (read_t.tv_usec - start.tv_usec)/1000;
    secelapsed = (read_t.tv_sec - start.tv_sec);
    printf("reading time:                         %li:%li:%li.%s%li\n",(secelapsed/3600)%60, (secelapsed/60)%60, (secelapsed)%60, ((millielapsed)%1000) > 99 ? "" : (((millielapsed)%1000) > 9 ? "0" : "00"), (millielapsed)%1000);
    subsample(raw, sub);
    gettimeofday(&sub_t, NULL);
    millielapsed = (sub_t.tv_usec - read_t.tv_usec)/1000;
    secelapsed = (sub_t.tv_sec - read_t.tv_sec)/1000;
    printf("subsampling time:                     %li:%li:%li.%s%li\n",(secelapsed/3600)%60, (secelapsed/60)%60, (secelapsed)%60, ((millielapsed)%1000) > 99 ? "" : (((millielapsed)%1000) > 9 ? "0" : "00"), (millielapsed)%1000);
    if(stored){
      different = compare(sub, saved, diffDims, differences);
      gettimeofday(&comp_t, NULL);
      millielapsed = (comp_t.tv_usec - sub_t.tv_usec)/1000;
      secelapsed = (comp_t.tv_sec - sub_t.tv_sec);
      printf("comparison time:                      %li:%li:%li.%s%li\n",(secelapsed/3600)%60, (secelapsed/60)%60, (secelapsed)%60, ((millielapsed)%1000) > 99 ? "" : (((millielapsed)%1000) > 9 ? "0" : "00"), (millielapsed)%1000);
      if (different) {
        printf("\033[0;036mImages are different\033[0m\n");
        for (int i = 0; i < different; i++) {
          gettimeofday(&appo, NULL);
          printf("pre getName\n");
          getName(text, newname, i);
          printf("\033[1A          \033[10");
          printf("diffDims[%i] = {%i, %i, %i, %i}\n", i, diffDims[i].x, diffDims[i].y, diffDims[i].w, diffDims[i].h);
          rgb_to_dct(raw, ordered_dct_Y, ordered_dct_Cb, ordered_dct_Cr, diffDims[i]);
	        init_huffman(ordered_dct_Y, ordered_dct_Cb, ordered_dct_Cr, diffDims[i], Luma, Chroma);
          size_t size = write_file(newname, jpg, ordered_dct_Y, ordered_dct_Cb, ordered_dct_Cr, diffDims[i], Luma, Chroma);
          FILE * out = fopen(newname, "w+");
          for (int i = 0; i < size; i++) fputc(jpg[i], out);
          fclose(out);
          gettimeofday(&op_t, NULL);
          millielapsed = (op_t.tv_usec - appo.tv_usec)/1000;
          secelapsed = (op_t.tv_sec - appo.tv_sec);
          printf("conversion time for diff #%i:          %li:%li:%li.%s%li\n", i, (secelapsed/3600)%60, (secelapsed/60)%60, (secelapsed)%60, ((millielapsed)%1000) > 99 ? "" : (((millielapsed)%1000) > 9 ? "0" : "00"), (millielapsed)%1000);
        }
      } else {
        printf("\033[0;036mImages are the same\033[0m\n");
        getName(text, newname, -1);
        rgb_to_dct(raw, ordered_dct_Y, ordered_dct_Cb, ordered_dct_Cr, fullImage);
        init_huffman(ordered_dct_Y, ordered_dct_Cb, ordered_dct_Cr, fullImage, Luma, Chroma);
        size_t size = write_file(newname, jpg, ordered_dct_Y, ordered_dct_Cb, ordered_dct_Cr, fullImage, Luma, Chroma);
      }
    } else {
      printf("\033[0;036mNo image stored\nStoring and encoding\033[0m\n");
      stored = 1;
      printf("pre getName\n");
      getName(text, newname, -1);
      printf("\033[1A");
      rgb_to_dct(raw, ordered_dct_Y, ordered_dct_Cb, ordered_dct_Cr, fullImage);
      init_huffman(ordered_dct_Y, ordered_dct_Cb, ordered_dct_Cr, fullImage, Luma, Chroma);
      size_t size = write_file(newname, jpg, ordered_dct_Y, ordered_dct_Cb, ordered_dct_Cr, fullImage, Luma, Chroma);
      printf("size = %zu       \n", size);
      FILE * out = fopen("comparison.jpg", "w+");
      for (i = 0; i < size; i++) fputc(jpg[i], out);
      fclose(out);
      gettimeofday(&op_t, NULL);
      millielapsed = (op_t.tv_usec - sub_t.tv_usec)/1000;
      secelapsed = (op_t.tv_sec - sub_t.tv_sec);
      printf("conversion time for full image:       %li:%li:%li.%s%li\n",(secelapsed/3600)%60, (secelapsed/60)%60, (secelapsed)%60, ((millielapsed)%1000) > 99 ? "" : (((millielapsed)%1000) > 9 ? "0" : "00"), (millielapsed)%1000);
    }
    store(sub, saved);
    gettimeofday(&store_t, NULL);
    millielapsed = (store_t.tv_usec - op_t.tv_usec)/1000;
    secelapsed = (store_t.tv_sec - op_t.tv_sec);
    printf("storing time:                         %li:%li:%li.%s%li\n",(secelapsed/3600)%60, (secelapsed/60)%60, (secelapsed)%60, ((millielapsed)%1000) > 99 ? "" : (((millielapsed)%1000) > 9 ? "0" : "00"), (millielapsed)%1000);
    gettimeofday(&end, NULL);
    millielapsed = (end.tv_usec - start.tv_usec)/1000;
    secelapsed = (end.tv_sec - start.tv_sec);
    printf("comparison and conversion total time: %li:%li:%li.%s%li\n",(secelapsed/3600)%60, (secelapsed/60)%60, (secelapsed)%60, ((millielapsed)%1000) > 99 ? "" : (((millielapsed)%1000) > 9 ? "0" : "00"), (millielapsed)%1000);
    printf("\033[0;36mDone\033[0m\n");
  }
	return 0;
}
