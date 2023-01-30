#include "../include/brain.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <esp_log.h>

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
    out[3*i] >>= 5;
    out[3*i] <<= 5;
    appo =  (in[3*(bigh*WIDTH+bigw)+1]     + in[3*(bigh*WIDTH+bigw+1)+1]     + in[3*(bigh*WIDTH+bigw+2)+1]     + in[3*(bigh*WIDTH+bigw+3)+1]);
    appo += (in[3*((bigh+1)*WIDTH+bigw)+1] + in[3*((bigh+1)*WIDTH+bigw+1)+1] + in[3*((bigh+1)*WIDTH+bigw+2)+1] + in[3*((bigh+1)*WIDTH+bigw+3)+1]);
    appo += (in[3*((bigh+2)*WIDTH+bigw)+1] + in[3*((bigh+2)*WIDTH+bigw+1)+1] + in[3*((bigh+2)*WIDTH+bigw+2)+1] + in[3*((bigh+2)*WIDTH+bigw+3)+1]);
    appo += (in[3*((bigh+3)*WIDTH+bigw)+1] + in[3*((bigh+3)*WIDTH+bigw+1)+1] + in[3*((bigh+3)*WIDTH+bigw+2)+1] + in[3*((bigh+3)*WIDTH+bigw+3)+1]);
    out[3*i+1] = appo/16;
    out[3*i+1] >>= 5;
    out[3*i+1] <<= 5;
    appo =  (in[3*(bigh*WIDTH+bigw)+2]     + in[3*(bigh*WIDTH+bigw+1)+2]     + in[3*(bigh*WIDTH+bigw+2)+2]     + in[3*(bigh*WIDTH+bigw+3)+2]);
    appo += (in[3*((bigh+1)*WIDTH+bigw)+2] + in[3*((bigh+1)*WIDTH+bigw+1)+2] + in[3*((bigh+1)*WIDTH+bigw+2)+2] + in[3*((bigh+1)*WIDTH+bigw+3)+2]);
    appo += (in[3*((bigh+2)*WIDTH+bigw)+2] + in[3*((bigh+2)*WIDTH+bigw+1)+2] + in[3*((bigh+2)*WIDTH+bigw+2)+2] + in[3*((bigh+2)*WIDTH+bigw+3)+2]);
    appo += (in[3*((bigh+3)*WIDTH+bigw)+2] + in[3*((bigh+3)*WIDTH+bigw+1)+2] + in[3*((bigh+3)*WIDTH+bigw+2)+2] + in[3*((bigh+3)*WIDTH+bigw+3)+2]);
    out[3*i+2] = appo/16;
    out[3*i+2] >>= 5;
    out[3*i+2] <<= 5;
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
  printf("in sumareas\n");
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
  printf("inside cumulative merge\n");
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

uint8_t compare_block(uint8_t *in, uint8_t saved[3*PIX_LEN/16], area_t outs[20], pair_t * differences, int numOuts, uint8_t off){
  printf("in compare\n");
  uint8_t offx = off%(WIDTH/4);
  uint8_t offy = off/(WIDTH/4);
  int isDifferent = 0;
  // pair_t differences[100];
  for (int i = 0; i < 100; i++) {
    differences[i].row = -1;
    differences[i].beg = -1;
    differences[i].end = -1;
    differences[i].done = 0;
    for (int j = 0; j < 8; j++) {
      differences[i].diff[j] = -1;
    }
  }
  printf("post init\n");
  int i = 0, j = 0, numdiff = 0, oldj = 0;
  for(; i < 256; i++) {
    if (!(i%16)) {
      for (int k = 0; k < j; k++) {
        int a = 0;
        for (int z = 0; z < oldj; z++) {
          if(differences[numdiff+k].end < differences[numdiff-oldj+z].beg-1 || differences[numdiff+k].beg > differences[numdiff-oldj+z].end+1) break;
          differences[numdiff+k].diff[a++] = numdiff-oldj+z; 
          // printf("link between %i and %i instantiated\n", numdiff+k, numdiff-oldj+z);
        }
      } 
      // printf("next line %i\n differences found: %i\n", i/16, j);
      isDifferent = 0;
      numdiff += j;
      oldj = j;
      j = 0;
    }
    if (in[3*(((i/16)+offy)*(WIDTH/4)+(i%16) + offx)]   != saved[3*(((i/16)+offy)*(WIDTH/4)+(i%16) + offx)]   ||
        in[3*(((i/16)+offy)*(WIDTH/4)+(i%16) + offx)+1] != saved[3*(((i/16)+offy)*(WIDTH/4)+(i%16) + offx)+1] ||
        in[3*(((i/16)+offy)*(WIDTH/4)+(i%16) + offx)+2] != saved[3*(((i/16)+offy)*(WIDTH/4)+(i%16) + offx)+2]) {
      if(!isDifferent || !(i%16)) {
        isDifferent = 1;
        differences[numdiff+j].beg = i%16;
        differences[numdiff+j].row = i/16;
      }
      differences[numdiff+j].end = i%16;
      if (i%16 == 15) j++;
    } else {
      if (isDifferent) { 
        // printf("found difference #%i in line #%i: beg = %i, end = %i\n", numdiff+j, differences[numdiff+j].row, differences[numdiff+j].beg, differences[numdiff+j].end);
        isDifferent = 0;
        j++;
      }
    }
  }
  printf("pre return\n");
  if (!numdiff) return numOuts;
  printf("post diff found: numdiff = %i\n", numdiff);
  isDifferent = numOuts;
  for (i = numdiff-1; i >= 0; i--) {
    // printf("done[%i] = %i\n", i, differences[i].done);
    if(differences[i].done) continue;
    if(differences[i].row == -1 || differences[i].beg == -1 || differences[i].end == -1) continue;
    int over = 0;
    area_t a = cumulativeMerge(differences, i);
    //printf("sons of diff %i:\n",i);
    j = 0;
    printf("post ciao\n");
    if (a.x == -1 || a.y == -1 || a.w == -1 || a.h == -1) continue;
    printf("a: x = %i, y = %i, w = %i, h = %i\n", a.x, a.y, a.w, a.h);
    for (j = 0; j < isDifferent; j++) {
      if (overlap(a, outs[j])) {
        sumAreas(&outs[j], a);
        over = 1;
        // printf("over[%i] = %i\n", j, over);
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
      if (isDifferent > 19) {
        printf("too many\n");
        return isDifferent;
      }
    }
  }
  return isDifferent;
}

uint8_t compare(uint8_t *in,uint8_t saved[3*PIX_LEN/16], area_t outs[20]){
  printf("in compare\n");
  int isDifferent = 0;
  pair_t differences[PIX_LEN/32];
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
    if (in[i] != saved[i] || in[i+1] != saved[i+1] || in[i+2] != saved[i+2]) {
      if(!isDifferent) {
        isDifferent = 1;
        differences[numdiff+j].beg = i%(WIDTH/4);
        differences[numdiff+j].row = i/(WIDTH/4);
      }
      differences[numdiff+j].end = i%(WIDTH/4);
    } else {
      if (isDifferent) { 
        //printf("found difference #%i in line #%i: beg = %i, end = %i\n", numdiff+j, differences[numdiff+j].row, differences[numdiff+j].beg, differences[numdiff+j].end);
        isDifferent = 0;
        j++;
      }
    }
  }
  //printf("post diff found\n");
  if (!numdiff) return 0;
  isDifferent = 0;
  for (i = numdiff-1; i >= 0; i--) {
    // printf("done[%i] = %i\n", i, differences[i].done);
    if(differences[i].done) continue;
    if(differences[i].row == -1 || differences[i].beg == -1 || differences[i].end == -1) continue;
    int over = 0;
    area_t a = cumulativeMerge(differences, i);
    //printf("sons of diff %i:\n",i);
    j = 0;
    while (differences[i].diff[j] > -1) {
      // printf("\t%i\n", differences[i].diff[j]);
      j++;
    }
    if (a.x == -1 || a.y == -1 || a.w == -1 || a.h == -1) continue;
    for (j = 0; j < isDifferent; j++) {
      if (overlap(a, outs[j])) {
        sumAreas(&outs[j], a);
        over = 1;
        //printf("over[%i] = %i\n", j, over);
        break;
      }
    }
    if(!over) {
      //printf("adding\n");
      outs[isDifferent].x = a.x;
      outs[isDifferent].y = a.y;
      outs[isDifferent].w = a.w;
      outs[isDifferent].h = a.h;
      isDifferent++;
      if (isDifferent > 19) {
        //printf("too many\n");
        return isDifferent;
      }
    }
  }
  return isDifferent;
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
  a->x -= a->x + a->w > WIDTH ? a->x + a->w - WIDTH : 0;
  a->y -= a->y + a->h > HEIGHT ? a->y + a->h - HEIGHT : 0;
  a->x = a->x < 0 ? 0 : a->x;
  a->y = a->y < 0 ? 0 : a->y;
  a->w = a->w > HEIGHT ? HEIGHT : a->w;
  a->h = a->h > HEIGHT ? HEIGHT : a->h;
}
