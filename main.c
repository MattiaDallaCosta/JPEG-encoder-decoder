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
// Include FreeRTOS for delay

#include "include/define.h"
#include "include/brain.h"
#include "include/encoder.h"

uint8_t raw[3*PIX_LEN];
uint8_t sub[3*PIX_LEN/16];
uint8_t jpg[3*PIX_LEN];

int16_t ordered_dct_Y[PIX_LEN];
int16_t ordered_dct_Cb[PIX_LEN/4];
int16_t ordered_dct_Cr[PIX_LEN/4];

// uint8_t *raw;
// uint8_t *sub;
// uint8_t *jpg;

// int16_t *ordered_dct_Y;
// int16_t *ordered_dct_Cb;
// int16_t *ordered_dct_Cr;
// area_t * diffDims;

uint8_t saved[3*PIX_LEN/16];
area_t diffDims[20];
pair_t differences[16];
huff_code Luma[2];
huff_code Chroma[2];
int len = 0;
uint8_t different = 0;

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
  int i = 0, j = 0;
  char text[100];
  char savedName[100];
  char newname[100];
  // Main loop
  while(1) {
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
        writePpm(f_sub, saved);
        fclose(f_sub);
        gettimeofday(&store_t, NULL);
        millielapsed = (store_t.tv_usec - start.tv_usec)/1000;
        secelapsed = (store_t.tv_sec - start.tv_sec)/1000;
        printf("storing time: %li:%li:%li.%s%li\n",(secelapsed/3600)%60, (secelapsed/60)%60, (secelapsed)%60, ((millielapsed)%1000) > 99 ? "" : (((millielapsed)%1000) > 9 ? "0" : "00"), (millielapsed)%1000);
      } else printf("\033[0;031mNo previous image stored\033[0m\n");
      printf("\033[0;36mDone\033[0m\n");
      continue;
    }
    if (!(f_in = fopen(text, "r"))) {
      fprintf(stderr, "\033[0;031mUnable to open file\033[0m\n");
      continue;
    }
    // raw = (uint8_t*)malloc(3*PIX_LEN);
	  if (readPpm(f_in, raw)) {
      printf("\033[0;031mError reading input file\033[0m\n");
	  	fclose(f_in);
	  	continue;
	  }
	  fclose(f_in);
    gettimeofday(&read_t, NULL);
    uint8_t offx = 0, offy = 0;
    millielapsed = (read_t.tv_usec - start.tv_usec)/1000;
    secelapsed = (read_t.tv_sec - start.tv_sec);
    printf("reading time:                         %li:%li:%li.%s%li\n",(secelapsed/3600)%60, (secelapsed/60)%60, (secelapsed)%60, ((millielapsed)%1000) > 99 ? "" : (((millielapsed)%1000) > 9 ? "0" : "00"), (millielapsed)%1000);
    // sub = (uint8_t*)malloc(3*PIX_LEN/16);
    subsample(raw, sub);
    gettimeofday(&sub_t, NULL);
    millielapsed = (sub_t.tv_usec - read_t.tv_usec)/1000;
    secelapsed = (sub_t.tv_sec - read_t.tv_sec)/1000;
    printf("subsampling time:                     %li:%li:%li.%s%li\n",(secelapsed/3600)%60, (secelapsed/60)%60, (secelapsed)%60, ((millielapsed)%1000) > 99 ? "" : (((millielapsed)%1000) > 9 ? "0" : "00"), (millielapsed)%1000);
    if(stored){
      different = 0;
      for (i = 0; i < 20; i++) {
        diffDims[i].x = -1;
        diffDims[i].y = -1;
        diffDims[i].w = -1;
        diffDims[i].h = -1;
      }
      for (i = 0; i < PIX_LEN/256; i++) {
        // printf("loop %i, offx = %i, offy = %i\n", i, offx, offy);
        different = compare_block(sub, saved, diffDims, differences, different, offx, offy);
        if ((i%(WIDTH/16) == (WIDTH/16)-1)) {
          offx = 0;
          offy += 4;
        } else offx += 4;
      }
      printf("different = %i\n", different);
      // for (int i = 0; i < different; i++) {
      //   enlargeAdjust(diffDims+i);
      //   printf("diffDims[%i]: x = %i y = %i w = %i h = %i\n", i, diffDims[i].x, diffDims[i].y, diffDims[i].w, diffDims[i].h);
      // }
      store(sub, saved);
      // free(sub);
      gettimeofday(&comp_t, NULL);
      millielapsed = (comp_t.tv_usec - sub_t.tv_usec)/1000;
      secelapsed = (comp_t.tv_sec - sub_t.tv_sec);
      printf("comparison time:                      %li:%li:%li.%s%li\n",(secelapsed/3600)%60, (secelapsed/60)%60, (secelapsed)%60, ((millielapsed)%1000) > 99 ? "" : (((millielapsed)%1000) > 9 ? "0" : "00"), (millielapsed)%1000);
    //   if (different) {
    //     printf("\033[0;036mImages are different\033[0m\n");
    //     gettimeofday(&store_t, NULL);
    //     millielapsed = (store_t.tv_usec - comp_t.tv_usec)/1000;
    //     secelapsed = (store_t.tv_sec - comp_t.tv_sec);
    //     printf("storing time:                         %li:%li:%li.%s%li\n",(secelapsed/3600)%60, (secelapsed/60)%60, (secelapsed)%60, ((millielapsed)%1000) > 99 ? "" : (((millielapsed)%1000) > 9 ? "0" : "00"), (millielapsed)%1000);
    //     for (int i = 0; i < different; i++) {
    //       jpg = (uint8_t*)malloc(3*diffDims[i].h*diffDims[i].w);
    //       ordered_dct_Y = (int16_t*)malloc(diffDims[i].h*diffDims[i].w);
    //       ordered_dct_Cb = (int16_t*)malloc(diffDims[i].h*diffDims[i].w/4);
    //       ordered_dct_Cr = (int16_t*)malloc(diffDims[i].h*diffDims[i].w/4);
    //       gettimeofday(&appo, NULL);
    //       printf("pre getName\n");
    //       getName(text, newname, i);
    //       enlargeAdjust(&diffDims[i]);
    //       int last[3] = {0, 0, 0};
    //       offx = 0;
    //       offy = 0;
    //       for (i = 0; i < (diffDims[i].w/16)*(diffDims[i].h/16); i++) {
    //         rgb_to_dct_block(raw, ordered_dct_Y, ordered_dct_Cb, ordered_dct_Cr, (offy+diffDims[i].y)*WIDTH+(offx+diffDims[i].x));
    //         for (j = 0; j < 4; j++) {
    //         ordered_dct_Y[((offy+(j%2))*diffDims[i].w)*8 + (offx+(j/2))*8] += last[0];
    //         last[0] += ordered_dct_Y[((offy+(j%2))*diffDims[i].w)*8 + (offx+(j/2))*8];
    //         }
    //         ordered_dct_Cb[(offy/2)*diffDims[i].w+offx/2] -= last[1]; 
    //         last[1] += ordered_dct_Cb[(offy/2)*diffDims[i].w+offx/2]; 
    //         ordered_dct_Cr[(offy/2)*diffDims[i].w+offx/2] -= last[2]; 
    //         last[2] += ordered_dct_Cr[(offy/2)*diffDims[i].w+offx/2]; 
    //         if(i%(diffDims[i].w/16) == (diffDims[i].w/16)-1) {
    //           offx += 16;
    //           offy = diffDims[i].y;
    //         } else offy += 16;
    //       }
    //       printf("post dct\n");
	   //      init_huffman(ordered_dct_Y, ordered_dct_Cb, ordered_dct_Cr, diffDims[i], Luma, Chroma);
    //       printf("post huffman\n");
	   //      size_t size = write_jpg(jpg, ordered_dct_Y, ordered_dct_Cb, ordered_dct_Cr, diffDims[i], Luma, Chroma);
    //       FILE * out = fopen(newname, "w");
    //       for (i = 0; i < size; i++) {
    //         fputc(jpg[i], out);
    //       }
    //       fclose(out);
    //       free(ordered_dct_Y);
    //       free(ordered_dct_Cb);
    //       free(ordered_dct_Cr);
    //       free(jpg);
    //       gettimeofday(&op_t, NULL);
    //       millielapsed = (op_t.tv_usec - appo.tv_usec)/1000;
    //       secelapsed = (op_t.tv_sec - appo.tv_sec);
    //       printf("\033[1Aconversion time for diff #%i:          %li:%li:%li.%s%li\n", i, (secelapsed/3600)%60, (secelapsed/60)%60, (secelapsed)%60, ((millielapsed)%1000) > 99 ? "" : (((millielapsed)%1000) > 9 ? "0" : "00"), (millielapsed)%1000);
    //       free(ordered_dct_Y);
    //       free(ordered_dct_Cb);
    //       free(ordered_dct_Cr);
    //       free(raw);
    //     }
    //   } else printf("\033[0;036mImages are the same\033[0m\n");
    } else {
      printf("\033[0;036mNo image stored\nStoring and encoding\033[0m\n");
      stored = 1;
      store(sub, saved);
      // free(sub);
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
      // jpg = (uint8_t*)malloc(3*fullImage.h*fullImage.w);
      // ordered_dct_Y = (int16_t*)malloc(fullImage.h*fullImage.w);
      // ordered_dct_Cb = (int16_t*)malloc(fullImage.h*fullImage.w/4);
      // ordered_dct_Cr = (int16_t*)malloc(fullImage.h*fullImage.w/4);
      getName(text, newname, -1);
      int last[3] = {0, 0, 0};
      offx = 0;
      offy = 0;
      for (i = 0; i < fullImage.w*fullImage.h/256; i++) {
        offx = i%(fullImage.w/16);
        offy = i/(fullImage.w/16);
        rgb_to_dct_block_old(raw, ordered_dct_Y, ordered_dct_Cb, ordered_dct_Cr,offx, offy, fullImage.w);
      }
      for (i = 0; i < fullImage.w*fullImage.h/64; i++) {
        if(i >= fullImage.w && i < fullImage.w*16+256) printf("pre: %i - ", ordered_dct_Y[i*64]);
        ordered_dct_Y[i*64] -= last[0];
        if(i >= fullImage.w && i < fullImage.w*16+256) printf("post: %i - ", ordered_dct_Y[i*64]);
        last[0] += ordered_dct_Y[i*64];
        if(i >= fullImage.w && i < fullImage.w*16+256) printf("new last: %i\n", last[0]);
        if(i < fullImage.w*fullImage.h/256){
          ordered_dct_Cb[i*64] -= last[1]; 
          last[1] += ordered_dct_Cb[i*64]; 
          ordered_dct_Cr[i*64] -= last[2]; 
          last[2] += ordered_dct_Cr[i*64]; 
        }
      }
      FILE * Y = fopen("dct_Y", "w");
      FILE * Cb = fopen("dct_Cb", "w");
      FILE * Cr = fopen("dct_Cr", "w");
      for (int i = 0; i < 256; i++) {
        if (i < fullImage.w*fullImage.h/16) {
          fprintf(Cb, "%i ", ordered_dct_Cb[i]);
          fprintf(Cr, "%i ", ordered_dct_Cr[i]);
        }
        fprintf(Y, "%i ", ordered_dct_Y[i]);
      }
      fclose(Y);
      fclose(Cb);
      fclose(Cr);
      printf("Ciao Bello 1\n");
      init_huffman(ordered_dct_Y, ordered_dct_Cb, ordered_dct_Cr, fullImage, Luma, Chroma);
      printf("Ciao Bello 2\n");
	    size_t size = write_jpg(jpg, ordered_dct_Y, ordered_dct_Cb, ordered_dct_Cr, fullImage, Luma, Chroma);
      printf("name: %s\n",newname);
      printf("size = %zu\n\n\n\n\n\n\n\n\n\n\n", size);
      // int fd = open(newname, O_RDWR | O_CREAT);
      // printf("fd: %d\n",fd);
      FILE * out = fopen(newname, "w+");
      // size--;
      // printf("size = %zu, write-size = %zi\n\n",size, write(fd, jpg, size));
      for (i = 0; i < size; i++) {
        printf("\033[10Ajpg[%i] = %s%i\n", i-9, jpg[i-8] < 100 ? (jpg[i-8] < 10 ? "  " : " "): "", jpg[i-9]);
        printf("jpg[%i] = %s%i\n", i-8, jpg[i-8] < 100 ? (jpg[i-8] < 10 ? "  " : " "): "", jpg[i-8]);
        printf("jpg[%i] = %s%i\n", i-7, jpg[i-7] < 100 ? (jpg[i-7] < 10 ? "  " : " "): "", jpg[i-7]);
        printf("jpg[%i] = %s%i\n", i-6, jpg[i-6] < 100 ? (jpg[i-6] < 10 ? "  " : " "): "", jpg[i-6]);
        printf("jpg[%i] = %s%i\n", i-5, jpg[i-5] < 100 ? (jpg[i-5] < 10 ? "  " : " "): "", jpg[i-5]);
        printf("jpg[%i] = %s%i\n", i-4, jpg[i-4] < 100 ? (jpg[i-4] < 10 ? "  " : " "): "", jpg[i-4]);
        printf("jpg[%i] = %s%i\n", i-3, jpg[i-3] < 100 ? (jpg[i-3] < 10 ? "  " : " "): "", jpg[i-3]);
        printf("jpg[%i] = %s%i\n", i-2, jpg[i-2] < 100 ? (jpg[i-2] < 10 ? "  " : " "): "", jpg[i-2]);
        printf("jpg[%i] = %s%i\n", i-1, jpg[i-1] < 100 ? (jpg[i-1] < 10 ? "  " : " "): "", jpg[i-1]);
        printf("jpg[%i] = %s%i\n", i, jpg[i] < 100 ? (jpg[i] < 10 ? "  " : " "): "", jpg[i]);
        // fprintf(out,"\033[1Ajpg[%i] = %i\n", i, jpg[i]);
        fputc(jpg[i], out);
      }
      // close(fd);
      fclose(out);
      printf("pre free\n");
      // free(jpg);
      printf("post free\n");
      gettimeofday(&op_t, NULL);
      millielapsed = (op_t.tv_usec - store_t.tv_usec)/1000;
      secelapsed = (op_t.tv_sec - store_t.tv_sec);
      printf("conversion time for full image:       %li:%li:%li.%s%li\n",(secelapsed/3600)%60, (secelapsed/60)%60, (secelapsed)%60, ((millielapsed)%1000) > 99 ? "" : (((millielapsed)%1000) > 9 ? "0" : "00"), (millielapsed)%1000);
      // free(ordered_dct_Y);
      // free(ordered_dct_Cb);
      // free(ordered_dct_Cr);
      // free(raw);
    }
    gettimeofday(&end, NULL);
    millielapsed = (end.tv_usec - start.tv_usec)/1000;
    secelapsed = (end.tv_sec - start.tv_sec);
    printf("comparison and conversion total time: %li:%li:%li.%s%li\n",(secelapsed/3600)%60, (secelapsed/60)%60, (secelapsed)%60, ((millielapsed)%1000) > 99 ? "" : (((millielapsed)%1000) > 9 ? "0" : "00"), (millielapsed)%1000);
    printf("\033[0;36mDone\033[0m\n");
    // sleep(1);
  }
	return 0;
  //   different = 0;
  //   fb = esp_camera_fb_get();
  //   esp_camera_fb_return(fb);
  //   fb = esp_camera_fb_get();
  //   printf("image capture\n"); 
  //   printf("pre decoding\n");
  //   fmt2rgb888(fb->buf, fb->len, PIXFORMAT_JPEG, raw);
  //   ESP_LOGI(TAG, "post decoding");
  //   esp_camera_fb_return(fb);
  //   ESP_LOGI(TAG, "pre sub");
  //   sub = (uint8_t*)heap_caps_malloc(3*PIX_LEN/16, MALLOC_CAP_SPIRAM);
  //   subsample(raw,sub);
  //   ESP_LOGI(TAG, "post sub");
  //   uint8_t offx = 0, offy = 0;
  //   for (i = 0; i < PIX_LEN/256; i++) {
  //   // ESP_LOGI(TAG, "offx = %i, offy = %i", offx, offy);
  //     different = compare_block(sub, saved, diffDims, differences, different, offy*(WIDTH/4)+offx);
  //     if (!(i%(WIDTH/16)) && i) {
  //       offx = 0;
  //       offy += 4;

  //     } else offx += 4;
  //   // ESP_LOGI(TAG, "done loop %i : different = %i", i, different);
  //   if(different > 19) break;
  //   }
  //   ESP_LOGI(TAG, "post compare: different = %i", different);
  //   for (int i = 0; i < different; i++) {
  //     enlargeAdjust(diffDims+i);
  //     ESP_LOGI(TAG, "diffDims[%i]: .x = %i, .y = %i, .w = %i, .h = %i", i, diffDims[i].x, diffDims[i].y, diffDims[i].w, diffDims[i].h);
  //   }
  //   store(sub, saved);
  //   free(sub);
  //   ESP_LOGI(TAG, "post save");
  //   area_t diff = { .x = 0, .y = 0, .w = WIDTH, .h = HEIGHT };
  //   jpg = (uint8_t*)heap_caps_malloc(3*diff.h*diff.w, MALLOC_CAP_SPIRAM);
  //   ordered_dct_Y = (int16_t*)heap_caps_malloc(diff.h*diff.w, MALLOC_CAP_SPIRAM);
  //   ordered_dct_Cb = (int16_t*)heap_caps_malloc(diff.h*diff.w/4, MALLOC_CAP_SPIRAM);
  //   ordered_dct_Cr = (int16_t*)heap_caps_malloc(diff.h*diff.w/4, MALLOC_CAP_SPIRAM);
  //   ESP_LOGI(TAG, "post malloc");
  //   int j;  
  //   int last[3] = {0, 0, 0};
  //   offx = 0;
  //   offy = 0;
  //   for (i = 0; i < (diff.w/16)*(diff.h/16); i++) {
  //     rgb_to_dct_block(raw, ordered_dct_Y, ordered_dct_Cb, ordered_dct_Cr, (offy+diff.y)*WIDTH+(offx+diff.x));
  //     for (j = 0; j < 4; j++) {
  //     ordered_dct_Y[((offy+(j%2))*diff.w)*8 + (offx+(j/2))*8] += last[0];
  //     last[0] += ordered_dct_Y[((offy+(j%2))*diff.w)*8 + (offx+(j/2))*8];
  //     }
  //     ordered_dct_Cb[(offy/2)*diff.w+offx/2] -= last[1]; 
  //     last[1] += ordered_dct_Cb[(offy/2)*diff.w+offx/2]; 
  //     ordered_dct_Cr[(offy/2)*diff.w+offx/2] -= last[2]; 
  //     last[2] += ordered_dct_Cr[(offy/2)*diff.w+offx/2]; 
  //     if(i%(diff.w/16) == (diff.w/16)-1) {
  //       offx += 16;
  //       offy = diff.y;
  //     } else offy += 16;
  //   }
  //   ESP_LOGI(TAG, "post dct");
	 //  init_huffman(ordered_dct_Y, ordered_dct_Cb, ordered_dct_Cr, diff, Luma, Chroma);
  //   ESP_LOGI(TAG, "post huffman");
	 //  size_t size = write_jpg(jpg, ordered_dct_Y, ordered_dct_Cb, ordered_dct_Cr, diff, Luma, Chroma);
  //   ESP_LOGI(TAG, "post encode: size = %zu", size);
  //   ESP_LOGW(TAG, "last characters: %i, %i", jpg[size - 2], jpg[size - 1]);
  //   // len = encodeNsend(jpg, raw, diff);
  //   free(ordered_dct_Y);
  //   free(ordered_dct_Cb);
  //   free(ordered_dct_Cr);
  //  //  free(jpg);
  //   free(raw);
  //   ESP_LOGI(TAG, "post free");
  //   // if (different) {
  //   //   ESP_LOGI(TAG, "Images are different");
  //   //   for (int i = 0; i < different; i++) {
  //   //     ESP_LOGI(TAG, "area #%i\nx: %i, y: %i, w: %i, h:%i", i, diffDims[i].x, diffDims[i].y, diffDims[i].w, diffDims[i].h);
  //   //     enlargeAdjust(&diffDims[i]);
  //   //     ESP_LOGI(TAG, "\t¦\n\tV\nx: %i, y: %i, w: %i, h:%i", diffDims[i].x, diffDims[i].y, diffDims[i].w, diffDims[i].h);
  //   //     jpg = (uint8_t*)heap_caps_malloc(3*diffDims[i].w*diffDims[i].h, MALLOC_CAP_SPIRAM);
  //   //     len = encodeNsend(jpg, raw, diffDims[i]);
  //   //     if (len) {
  //   //       // myBot.sendPhoto(msg, jpg, len);
  //   //       len = 0;
  //   //     }
  //   //     free(jpg);
  //   //   }
  //   //   ESP_LOGI(TAG, "Post encodeNsend");
  //   // } else ESP_LOGI(TAG, "Images are the same");
  //   // free(raw);
  //     vTaskDelay(1000 / portTICK_PERIOD_MS);
  // ESP_LOGI(TAG, "--- finished cicle ---\n");
  // }
}
