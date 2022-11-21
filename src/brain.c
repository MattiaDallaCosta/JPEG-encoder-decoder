#include "../include/brain.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
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
    // printf("%i, %i, %i\n", out[0][i], out[1][i], out[2][i]);
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

int overlap(area_t a1, area_t a2){
  int orizover = !(a1.x > a2.w + 1 || a1.w + 1 < a2.x);
  int vertover = !(a1.y > a2.h + 1 || a1.h + 1 < a2.y);
  return orizover && vertover;
}

void sumAreas(area_t * a1, area_t a2) {
  if((a1->x == -1 || a1->y == -1 || a1->w == -1 || a1->h == -1) && (a2.x == -1 || a2.y == -1 || a2.w == -1 || a2.h == -1)) {
    a1->x = -1;
    a1->y = -1;
    a1->w = -1;
    a1->h = -1;
  } else if (a1->x == -1 || a1->y == -1 || a1->w == -1 || a1->h == -1) {
    a1->x = a2.x;
    a1->y = a2.y;
    a1->w = a2.w;
    a1->h = a2.h;
  } else if (a2.x == -1 || a2.y == -1 || a2.w == -1 || a2.h == -1) {
  } else {
    a1->x = MIN(a1->x, a2.x);
    a1->y = MIN(a1->y, a2.y);
    a1->w = MAX(a1->w, a2.w);
    a1->h = MAX(a1->h, a2.h);
  }
}

area_t cumulativeMerge(pair_t * diffs, int index){
  int i = 0;
  area_t a;
  if (diffs[index].done) {
    a.x = -1;
    a.y = -1;
    a.w = -1;
    a.h = -1;
    return a;
  }
  a.x = diffs[index].beg;
  a.w = diffs[index].end;
  a.y = diffs[index].row;
  a.h = diffs[index].row;
  diffs[index].done = 1;
  // printf("index = %i\n", index);
  while (diffs[index].diff[i] > -1) {
    // printf("son of %i is %i\n",index, diffs[index].diff[i]);
    // printf("\033[1Aapp[%i] = %i %i %i %i\n", index, a.x, a.y, a.w, a.h);
    area_t app = cumulativeMerge(diffs, diffs[index].diff[i]);
    sumAreas(&a, app);
    i++;
  }
  return a;
}

int compare(uint8_t in[3][PIX_LEN/16], area_t outs[1000]){
  int isDifferent = 0;
  pair_t differences[PIX_LEN/32];
  struct {
    int diff;
    int area;
  } links[PIX_LEN/32];
  int linksize = 0;
  for (int i = 0; i < PIX_LEN/32; i++) {
    differences[i].row = -1;
    differences[i].beg = -1;
    differences[i].end = -1;
    differences[i].done = 0;
    for (int j = 0; j < WIDTH/8; j++) {
      differences[i].diff[j] = -1;
    }
  }
  int i = 0, j = 0, numdiff = 0, oldj = 0;
  for(; i < PIX_LEN/16; i++) {
    if (!(i%(WIDTH/4))) {
      for (int k = 0; k < j; k++) {
        int a = 0;
        for (int z = 0; z < oldj; z++) {
          if(differences[numdiff+k].end < differences[numdiff-oldj+z].beg-1 || differences[numdiff+k].beg > differences[numdiff-oldj+z].end+1) break;
          differences[numdiff+k].diff[a++] = numdiff-oldj+z; 
        }
      } 
      isDifferent = 0;
      numdiff += j;
      oldj = j;
      j = 0;
    }
    if (in[0][i] != saved[0][i] || in[1][i] != saved[1][i] || in[2][i] != saved[2][i]) {
      if(!isDifferent) {
        isDifferent = 1;
        differences[numdiff+j].beg = i%(WIDTH/4);
        differences[numdiff+j].row = i/(WIDTH/4);
      }
      differences[numdiff+j].end = i%(WIDTH/4);
    } else {
      if (isDifferent) { 
        printf("found difference #%i in line #%i: beg = %i, end = %i\n", numdiff+j, differences[numdiff+j].row, differences[numdiff+j].beg, differences[numdiff+j].end);
        isDifferent = 0;
        j++;
      }
    }
  }
  printf("post diff found\n");
  if (!numdiff) return 0;
  isDifferent = 0;
  for (i = numdiff-1; i >= 0; i--) {
    // printf("done[%i] = %i\n", i, differences[i].done);
    if(differences[i].done) continue;
    if(differences[i].row == -1 || differences[i].beg == -1 || differences[i].end == -1) continue;
    int over = 0;
    area_t a = cumulativeMerge(differences, i);
    printf("sons of diff %i:\n",i);
    j = 0;
    while (differences[i].diff[j] > -1) {
      printf("\t%i\n", differences[i].diff[j]);
      j++;
    }
    if (a.x == -1 || a.y == -1 || a.w == -1 || a.h == -1) continue;
    for (j = 0; j < isDifferent; j++) {
      if (overlap(a, outs[j])) {
        sumAreas(&outs[j], a);
        over = 1;
        printf("over[%i] = %i\n", j, over);
        break;
      }
    }
    if(!over) {
      printf("adding\n");
      outs[isDifferent].x = a.x;
      outs[isDifferent].y = a.y;
      outs[isDifferent].w = a.w;
      outs[isDifferent].h = a.h;
      isDifferent++;
      if (isDifferent > 1000) {
        printf("too many\n");
        return isDifferent;
      }
    }
  }
  return isDifferent;
}

void enlargeAdjust(area_t * a) {
  a->x *= 4;
  a->y *= 4;
  a->w *= 4;
  a->h *= 4;
  a->w = a->w - a->x + 1;
  a->h = a->h - a->y + 1;
  a->w = a->w%16 ? a->w + (16 - a->w%16) : a->w;
  a->h = a->h%16 ? a->h + (16 - a->h%16) : a->h;
  a->x -= (a->w%16)/2;
  a->y -= (a->h%16)/2;
  a->x -= a->x + a->w > WIDTH ? a->x + a->w - WIDTH : 0;
  a->y -= a->y + a->h > HEIGHT ? a->y + a->h - HEIGHT : 0;
  a->x = a->x < 0 ? 0 : a->x;
  a->y = a->y < 0 ? 0 : a->y;
  a->w = a->w > HEIGHT ? HEIGHT : a->w;
  a->h = a->h > HEIGHT ? HEIGHT : a->h;
}
