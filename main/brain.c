#include "../include/brain.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

void subsample(uint8_t *in, uint8_t *out) {
  int i = 0;
  uint16_t appo;
  uint8_t bigh;
  uint8_t bigw;
  for (; i < PIX_LEN/16; i++) {
    bigh = (i/(WIDTH/4))*4;
    bigw = (i%(WIDTH/4))*4;    
    appo =  (in[3*(bigh*WIDTH+bigw)]     + in[3*(bigh*WIDTH+bigw+1)]     + in[3*(bigh*WIDTH+bigw+2)]     + in[3*(bigh*WIDTH+bigw+3)]);
    appo += (in[3*((bigh+1)*WIDTH+bigw)] + in[3*((bigh+1)*WIDTH+bigw+1)] + in[3*((bigh+1)*WIDTH+bigw+2)] + in[3*((bigh+1)*WIDTH+bigw+3)]);
    appo += (in[3*((bigh+2)*WIDTH+bigw)] + in[3*((bigh+2)*WIDTH+bigw+1)] + in[3*((bigh+2)*WIDTH+bigw+2)] + in[3*((bigh+2)*WIDTH+bigw+3)]);
    appo += (in[3*((bigh+3)*WIDTH+bigw)] + in[3*((bigh+3)*WIDTH+bigw+1)] + in[3*((bigh+3)*WIDTH+bigw+2)] + in[3*((bigh+3)*WIDTH+bigw+3)]);
    out[3*i] = appo/16;
    // out[3*i] >>= 4;
    out[3*i] &= 0xF0;
    appo =  (in[3*(bigh*WIDTH+bigw)+1]     + in[3*(bigh*WIDTH+bigw+1)+1]     + in[3*(bigh*WIDTH+bigw+2)+1]     + in[3*(bigh*WIDTH+bigw+3)+1]);
    appo += (in[3*((bigh+1)*WIDTH+bigw)+1] + in[3*((bigh+1)*WIDTH+bigw+1)+1] + in[3*((bigh+1)*WIDTH+bigw+2)+1] + in[3*((bigh+1)*WIDTH+bigw+3)+1]);
    appo += (in[3*((bigh+2)*WIDTH+bigw)+1] + in[3*((bigh+2)*WIDTH+bigw+1)+1] + in[3*((bigh+2)*WIDTH+bigw+2)+1] + in[3*((bigh+2)*WIDTH+bigw+3)+1]);
    appo += (in[3*((bigh+3)*WIDTH+bigw)+1] + in[3*((bigh+3)*WIDTH+bigw+1)+1] + in[3*((bigh+3)*WIDTH+bigw+2)+1] + in[3*((bigh+3)*WIDTH+bigw+3)+1]);
    out[3*i+1] = appo/16;
    // out[3*i+1] >>= 4;
    out[3*i+1] &= 0xF0;
    appo =  (in[3*(bigh*WIDTH+bigw)+2]     + in[3*(bigh*WIDTH+bigw+1)+2]     + in[3*(bigh*WIDTH+bigw+2)+2]     + in[3*(bigh*WIDTH+bigw+3)+2]);
    appo += (in[3*((bigh+1)*WIDTH+bigw)+2] + in[3*((bigh+1)*WIDTH+bigw+1)+2] + in[3*((bigh+1)*WIDTH+bigw+2)+2] + in[3*((bigh+1)*WIDTH+bigw+3)+2]);
    appo += (in[3*((bigh+2)*WIDTH+bigw)+2] + in[3*((bigh+2)*WIDTH+bigw+1)+2] + in[3*((bigh+2)*WIDTH+bigw+2)+2] + in[3*((bigh+2)*WIDTH+bigw+3)+2]);
    appo += (in[3*((bigh+3)*WIDTH+bigw)+2] + in[3*((bigh+3)*WIDTH+bigw+1)+2] + in[3*((bigh+3)*WIDTH+bigw+2)+2] + in[3*((bigh+3)*WIDTH+bigw+3)+2]);
    out[3*i+2] = appo/16;
    // out[3*i+2] >>= 4;
    out[3*i+2] &= 0xF0;
  }
}

void store(uint8_t *in, uint8_t saved[3*PIX_LEN/16]) {
  int i = 0;
  for(; i < PIX_LEN/16; i++) {
    saved[i]   = in[i];
    saved[i+1] = in[i+1];
    saved[i+2] = in[i+2];
  }
}

void store_file(uint8_t *in, char * file_name) {
  FILE * f = fopen(file_name, "w");
  int i = 0;
  for(; i < PIX_LEN/16; i++) {
    fputc(in[i], f);
    fputc(in[i+1], f);
    fputc(in[i+2], f);
  }
}

int overlap(area_t a1, area_t a2){
  int orizover = !(a1.x > a2.w + 1 || a1.w + 1 < a2.x);
  int vertover = !(a1.y > a2.h + 1 || a1.h + 1 < a2.y);
  return orizover && vertover;
}

void sumAreas(area_t * a1, area_t a2) {
  // printf("in sumareas\n");
  if((a1->x < 0 -1 || a1->y < 0 || a1->w < 0 -1 || a1->h < 0) && (a2.x < 0 || a2.y < 0 || a2.w <0 || a2.h < 0)) {
    a1->x = -1;
    a1->y = -1;
    a1->w = -1;
    a1->h = -1;
  } else if (a1->x < 0 -1 || a1->y < 0 || a1->w < 0 -1 || a1->h < 0) {
    a1->x = a2.x;
    a1->y = a2.y;
    a1->w = a2.w;
    a1->h = a2.h;
  } else if (a2.x < 0 || a2.y < 0 || a2.w <0 || a2.h < 0) {
  } else {
    a1->x = MIN(a1->x, a2.x);
    a1->y = MIN(a1->y, a2.y);
    a1->w = MAX(a1->w, a2.w);
    a1->h = MAX(a1->h, a2.h);
  }
}

uint8_t compare(uint8_t *in, uint8_t saved[3*PIX_LEN/16], area_t * outs, pair_t differences[2][WIDTH/8]) {
  // printf("in compare\n");
  int isDifferent = 0;
  int index = 0;
  int outsIndex = 0;
  int i = 0, j = 0, oldj = 0;
  for(; i < PIX_LEN/16; i++) {
    if (!(i%(WIDTH/4))) {
      for (int k = 0; k < j; k++) {
        int a = 0;
        int added = 0;
        for (int z = 0; z < oldj; z++) {
          if(differences[index][k].end < differences[!index][z].beg-1 || differences[index][k].beg > differences[!index][z].end+1)  continue;
          added = 1;
          if (differences[index][k].done >= 0) {
            int minout = MIN(differences[!index][z].done, differences[index][k].done);
            int maxout = MAX(differences[!index][z].done, differences[index][k].done);
            if (maxout == minout) continue;
            sumAreas(&outs[minout], outs[maxout]);
            outsIndex--;
            if (maxout < outsIndex) {
              outs[maxout].x = outs[outsIndex].x;
              outs[maxout].y = outs[outsIndex].y;
              outs[maxout].w = outs[outsIndex].w;
              outs[maxout].h = outs[outsIndex].h;
            }
            differences[index][k].done = differences[!index][z].done = minout;
            for (a = 0; a < k; a++) {
              if (differences[index][a].done == maxout) differences[index][a].done = minout;
              if (differences[index][a].done == outsIndex) differences[index][a].done = maxout;
            }
            for (a = z+1; a < oldj; a++) {
              if (differences[!index][a].done == maxout) differences[!index][a].done = minout;
              if (differences[!index][a].done == outsIndex) differences[!index][a].done = maxout;
            }
          } else {
            area_t a = {differences[index][k].beg, differences[index][k].row, differences[index][k].end, differences[index][k].row};
            sumAreas(&outs[differences[!index][z].done], a);
          }
        }
        if(!added) {
          if(outsIndex > 19) {
            for (int i = 0; i < outsIndex; i++) for (int j = i+1; j < outsIndex; j++) {
              if (overlap(outs[i], outs[j])) {
                sumAreas(&outs[i], outs[j]);
                outsIndex--;
                outs[j].x = outs[outsIndex].x;
                outs[j].y = outs[outsIndex].y;
                outs[j].w = outs[outsIndex].w;
                outs[j].h = outs[outsIndex].h;
              }
            }
            if (outsIndex > 19) return outsIndex;
          }
          differences[index][k].done = outsIndex;
          outs[outsIndex].x = differences[index][k].beg;
          outs[outsIndex].y = differences[index][k].row;
          outs[outsIndex].w = differences[index][k].end;
          outs[outsIndex].h = differences[index][k].row;
          outsIndex++;
        }
      } 
      index = !index;
      isDifferent = 0;
      oldj = j;
      j = 0;
    }
    if (in[3*((i/(WIDTH/4))*(WIDTH/4)+(i%(WIDTH/4)))]   != saved[3*((i/(WIDTH/4))*(WIDTH/4)+(i%(WIDTH/4)))]   ||
        in[3*((i/(WIDTH/4))*(WIDTH/4)+(i%(WIDTH/4)))+1] != saved[3*((i/(WIDTH/4))*(WIDTH/4)+(i%(WIDTH/4)))+1] ||
        in[3*((i/(WIDTH/4))*(WIDTH/4)+(i%(WIDTH/4)))+2] != saved[3*((i/(WIDTH/4))*(WIDTH/4)+(i%(WIDTH/4)))+2]) {
      // printf("[%i|%i|%i] != ", in[3*((i/(WIDTH/4))*(WIDTH/4)+(i%(WIDTH/4)))+1], in[3*((i/(WIDTH/4))*(WIDTH/4)+(i%(WIDTH/4)))], in[3*((i/(WIDTH/4))*(WIDTH/4)+(i%(WIDTH/4)))+2]);
      // printf("[%i|%i|%i]\n", saved[3*((i/(WIDTH/4))*(WIDTH/4)+(i%(WIDTH/4)))+1], saved[3*((i/(WIDTH/4))*(WIDTH/4)+(i%(WIDTH/4)))], saved[3*((i/(WIDTH/4))*(WIDTH/4)+(i%(WIDTH/4)))+2]);
      if(!isDifferent) {
        isDifferent = 1;
        differences[index][j].beg = i%(WIDTH/4);
        differences[index][j].row = i/(WIDTH/4);
        differences[index][j].done = -1;
      }
      differences[index][j].end = i%(WIDTH/4);
    } else {
      if (isDifferent) { 
        isDifferent = 0;
        j++;
      }
    }
  }
  for (int i = 0; i < outsIndex; i++) for (int j = i+1; j < outsIndex; j++) {
    if (overlap(outs[i], outs[j])) {
      sumAreas(&outs[i], outs[j]);
      outsIndex--;
      outs[j].x = outs[outsIndex].x;
      outs[j].y = outs[outsIndex].y;
      outs[j].w = outs[outsIndex].w;
      outs[j].h = outs[outsIndex].h;
    }
  }
  return outsIndex;
}

inline void enlargeAdjust(area_t * a){
  a->x *= 4;
  a->y *= 4;
  a->w *= 4;
  a->h *= 4;
  a->w = a->w - a->x + 1;
  a->h = a->h - a->y + 1;
  a->x -= (16 - (a->w%16))/2;
  a->y -= (16 - (a->h%16))/2;
  a->w = a->w%16 ? a->w + (16 - a->w%16) : a->w;
  a->h = a->h%16 ? a->h + (16 - a->h%16) : a->h;
  a->w = a->w > WIDTH ? WIDTH : a->w;
  a->h = a->h > HEIGHT ? HEIGHT : a->h;
  a->x -= a->x + a->w > WIDTH ? (a->x + a->w) - WIDTH : 0;
  a->y -= a->y + a->h > HEIGHT ? (a->y + a->h) - HEIGHT : 0;
  a->x = a->x < 0 ? 0 : a->x;
  a->y = a->y < 0 ? 0 : a->y;
}
