#include "../include/brain.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

int saved[3][PIX_LEN/16];

void subsample(uint8_t in[3][PIX_LEN], uint8_t out[3][PIX_LEN/16]) {
  int i = 0;
  for (; i < PIX_LEN/16; i++) {
    int bigh = (i/(WIDTH/4))*4;
    int bigw = (i%(WIDTH/4))*4;
    int appo;
    appo =  (in[0][bigh*WIDTH+bigw]     + in[0][bigh*WIDTH+bigw+1]     + in[0][bigh*WIDTH+bigw+2]     + in[0][bigh*WIDTH+bigw+3]);
    appo += (in[0][(bigh+1)*WIDTH+bigw] + in[0][(bigh+1)*WIDTH+bigw+1] + in[0][(bigh+1)*WIDTH+bigw+2] + in[0][(bigh+1)*WIDTH+bigw+3]);
    appo += (in[0][(bigh+2)*WIDTH+bigw] + in[0][(bigh+2)*WIDTH+bigw+1] + in[0][(bigh+2)*WIDTH+bigw+2] + in[0][(bigh+2)*WIDTH+bigw+3]);
    appo += (in[0][(bigh+3)*WIDTH+bigw] + in[0][(bigh+3)*WIDTH+bigw+1] + in[0][(bigh+3)*WIDTH+bigw+2] + in[0][(bigh+3)*WIDTH+bigw+3]);
    out[0][i] = appo/16;
    appo =  (in[1][bigh*WIDTH+bigw]     + in[1][bigh*WIDTH+bigw+1]     + in[1][bigh*WIDTH+bigw+2]     + in[1][bigh*WIDTH+bigw+3]);
    appo += (in[1][(bigh+1)*WIDTH+bigw] + in[1][(bigh+1)*WIDTH+bigw+1] + in[1][(bigh+1)*WIDTH+bigw+2] + in[1][(bigh+1)*WIDTH+bigw+3]);
    appo += (in[1][(bigh+2)*WIDTH+bigw] + in[1][(bigh+2)*WIDTH+bigw+1] + in[1][(bigh+2)*WIDTH+bigw+2] + in[1][(bigh+2)*WIDTH+bigw+3]);
    appo += (in[1][(bigh+3)*WIDTH+bigw] + in[1][(bigh+3)*WIDTH+bigw+1] + in[1][(bigh+3)*WIDTH+bigw+2] + in[1][(bigh+3)*WIDTH+bigw+3]);
    out[1][i] = appo/16;
    appo = (in[2][bigh*WIDTH+bigw]     + in[2][bigh*WIDTH+bigw+1]     + in[2][bigh*WIDTH+bigw+2]     + in[2][bigh*WIDTH+bigw+3]);
    appo += (in[2][(bigh+1)*WIDTH+bigw] + in[2][(bigh+1)*WIDTH+bigw+1] + in[2][(bigh+1)*WIDTH+bigw+2] + in[2][(bigh+1)*WIDTH+bigw+3]);
    appo += (in[2][(bigh+2)*WIDTH+bigw] + in[2][(bigh+2)*WIDTH+bigw+1] + in[2][(bigh+2)*WIDTH+bigw+2] + in[2][(bigh+2)*WIDTH+bigw+3]);
    appo += (in[2][(bigh+3)*WIDTH+bigw] + in[2][(bigh+3)*WIDTH+bigw+1] + in[2][(bigh+3)*WIDTH+bigw+2] + in[2][(bigh+3)*WIDTH+bigw+3]);
    out[2][i] = appo/16;
    printf("%i, %i, %i\n", out[0][i], out[1][i], out[2][i]);
  }
}

void store(uint8_t in[3][PIX_LEN/16]) {
  int i = 0;
  for(; i < PIX_LEN/16; i++) {
    saved[0][i] = in[0][i];
    saved[1][i] = in[1][i];
    saved[2][i] = in[2][i];
    usleep(500);
  }
}

int compare(uint8_t in[3][PIX_LEN/16], area_t outs[20]){
  int isDifferent = 0;
  pair_t differences[PIX_LEN/32];
  // diff_t app;
  for (int i = 0; i < PIX_LEN/32; i++) {
    differences[i].beg = -1;
    differences[i].end = -1;
    if (i < 20) {
      outs[i].x = -1;
      outs[i].y = -1;
      outs[i].w = -1;
      outs[i].h = -1;
    }
  }
  int i = 0, j = 0, numdiff = 0;
  for(; i < PIX_LEN/16; i++) {
    if (!(i%(WIDTH/4))) {
      isDifferent = 0;
      if(i != 0) printf("found %i differences in raw %i\n", j, i/(WIDTH/4)-1);
      numdiff += j;
      j = 0;
    }
    if (in[0][i] != saved[0][i] || in[1][i] != saved[1][i] || in[2][i] != saved[2][i]) {
      if(!isDifferent) {
        isDifferent = 1;
        differences[i/(WIDTH/4)+j].beg = i%(WIDTH/4);
      }
      differences[i/(WIDTH/4)+j].end = i%(WIDTH/4);
    } else {
      printf("%i == %i, %i == %i, %i == %i\n", in[0][i], saved[0][i], in[1][i], saved[1][i], in[2][i], saved[2][i]);
      if (isDifferent) { 
        isDifferent = 0;
        j++;
      }
    }
  }
  printf("found %i differences in raw %i\n", j, i/(WIDTH/4)-1);
  if (!numdiff) return 0;
  outs[0].x = differences[0].beg;
  outs[0].w = differences[0].end - differences[0].beg;
  outs[0].y = 0;
  outs[0].h = 1;
  isDifferent = 1;
  int k = 0;
  int overlap = 0;
  for (i = 0; i < HEIGHT/4; i++) {
    j = 0;
    while (differences[i*(WIDTH/4)+j].beg >= 0 && differences[i*(WIDTH/4)+j].end >= 0) {
      for(int k = 0; k < isDifferent; k++){
        if ((differences[i*(WIDTH/4)+j].beg <= outs[k].w + outs[k].x || differences[i*(WIDTH/4)+j].end >= outs[k].x) && i == outs[k].y + outs[k].h) {
          int newend = MAX(outs[k].w + outs[k].x, differences[i*(WIDTH/4)+j].end);
          outs[k].x = MIN(outs[k].x, differences[i*(WIDTH/4)+j].beg);
          outs[k].w = newend - outs[k].x;
          outs[k].h++;
          overlap = 1;
        }
      }
      if(!overlap) {
        outs[isDifferent].x = differences[i+j].beg;
        outs[isDifferent].w = differences[i+j].end - differences[i+j].beg;
        outs[isDifferent].y = i;
        outs[isDifferent].h = 1;
        isDifferent++;
        if(isDifferent == 20) {
          printf("too many differences\n");
          return isDifferent;
        }
      } else overlap = 0;
      j++;
    }
  }
  return isDifferent;
}
