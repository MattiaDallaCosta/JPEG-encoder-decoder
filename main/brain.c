#include "../include/brain.h"
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

/* subsample(FILE * f, uint8_t *in, uint8_t *out)
 *  f = pointer to the file where to save the subsampled image
 *  in = rgb array, out = recipient for the subsampled rgb array
 *  
 *  function that applies a 8:2:0:0:0 subsampling to the image
 *  dividing the sample rate of the image by 4 in each axis
 */
void subsample(FILE * f, uint8_t *in, uint8_t *out) {
  int i = 0;
  uint16_t appo;
  uint16_t bigh;
  uint16_t bigw;
  fprintf(f, "P6\n%i %i\n255\n", WIDTH/4, HEIGHT/4);
  for (; i < PIX_LEN/16; i++) {
    bigh = (i/(WIDTH/4))*4;
    bigw = (i%(WIDTH/4))*4;    
    appo =  (in[3*(bigh*WIDTH+bigw)+2]     + in[3*(bigh*WIDTH+bigw+1)+2]     + in[3*(bigh*WIDTH+bigw+2)+2]     + in[3*(bigh*WIDTH+bigw+3)+2]);
    appo += (in[3*((bigh+1)*WIDTH+bigw)+2] + in[3*((bigh+1)*WIDTH+bigw+1)+2] + in[3*((bigh+1)*WIDTH+bigw+2)+2] + in[3*((bigh+1)*WIDTH+bigw+3)+2]);
    appo += (in[3*((bigh+2)*WIDTH+bigw)+2] + in[3*((bigh+2)*WIDTH+bigw+1)+2] + in[3*((bigh+2)*WIDTH+bigw+2)+2] + in[3*((bigh+2)*WIDTH+bigw+3)+2]);
    appo += (in[3*((bigh+3)*WIDTH+bigw)+2] + in[3*((bigh+3)*WIDTH+bigw+1)+2] + in[3*((bigh+3)*WIDTH+bigw+2)+2] + in[3*((bigh+3)*WIDTH+bigw+3)+2]);
    out[3*i] = (appo/16);
    putc(out[3*i], f);
    appo =  (in[3*(bigh*WIDTH+bigw)+1]     + in[3*(bigh*WIDTH+bigw+1)+1]     + in[3*(bigh*WIDTH+bigw+2)+1]     + in[3*(bigh*WIDTH+bigw+3)+1]);
    appo += (in[3*((bigh+1)*WIDTH+bigw)+1] + in[3*((bigh+1)*WIDTH+bigw+1)+1] + in[3*((bigh+1)*WIDTH+bigw+2)+1] + in[3*((bigh+1)*WIDTH+bigw+3)+1]);
    appo += (in[3*((bigh+2)*WIDTH+bigw)+1] + in[3*((bigh+2)*WIDTH+bigw+1)+1] + in[3*((bigh+2)*WIDTH+bigw+2)+1] + in[3*((bigh+2)*WIDTH+bigw+3)+1]);
    appo += (in[3*((bigh+3)*WIDTH+bigw)+1] + in[3*((bigh+3)*WIDTH+bigw+1)+1] + in[3*((bigh+3)*WIDTH+bigw+2)+1] + in[3*((bigh+3)*WIDTH+bigw+3)+1]);
    out[3*i+1] = (appo/16);
    putc(out[3*i+1], f);
    appo =  (in[3*(bigh*WIDTH+bigw)]     + in[3*(bigh*WIDTH+bigw+1)]     + in[3*(bigh*WIDTH+bigw+2)]     + in[3*(bigh*WIDTH+bigw+3)]);
    appo += (in[3*((bigh+1)*WIDTH+bigw)] + in[3*((bigh+1)*WIDTH+bigw+1)] + in[3*((bigh+1)*WIDTH+bigw+2)] + in[3*((bigh+1)*WIDTH+bigw+3)]);
    appo += (in[3*((bigh+2)*WIDTH+bigw)] + in[3*((bigh+2)*WIDTH+bigw+1)] + in[3*((bigh+2)*WIDTH+bigw+2)] + in[3*((bigh+2)*WIDTH+bigw+3)]);
    appo += (in[3*((bigh+3)*WIDTH+bigw)] + in[3*((bigh+3)*WIDTH+bigw+1)] + in[3*((bigh+3)*WIDTH+bigw+2)] + in[3*((bigh+3)*WIDTH+bigw+3)]);
    out[3*i+2] =(appo/16);
    putc(out[3*i+2], f);
  }
}

/* store(uint8_t *in, uint8_t saved[3*PIX_LEN/16])
 *  in = subsampled rgb array, saved = array containing the saved image used by the comparator
 *  
 *  function that copies the subsampled image to the saved array
 */
void store(uint8_t *in, uint8_t saved[3*PIX_LEN/16]) {
  int i = 0;
  for(; i < PIX_LEN/16; i++) {
    saved[3*i]   = in[3*i];
    saved[3*i+1] = in[3*i+1];
    saved[3*i+2] = in[3*i+2];
  }
}

/* overlap(area_t a1, area_t a2)
 *  [a1, a2] = areas to be compared in order ot understand if they are overlapping or not
 *  
 *  functions that check if 2 areas are overlapping 
 *  (overlap is used for non enlargedAdjusted areas while overlap2 is used after enlargeAdjust is applied)
 */
int overlap(area_t a1, area_t a2){
  int orizover = !(a1.x > a2.w + 1 || a1.w + 1 < a2.x);
  int vertover = !(a1.y > a2.h + 1 || a1.h + 1 < a2.y);
  return orizover & vertover;
}

int overlap2(area_t a1, area_t a2){
  int orizover = !(a1.x > a2.x + a2.w + 2 || a1.x + a1.w + 2 < a2.x);
  int vertover = !(a1.y > a2.y + a2.h + 2 || a1.y + a1.h + 2 < a2.y);
  return orizover & vertover;
}

/* sumAreas(area_t * a1, area_t a2)
 *  [a1, a2] = areas to be merged in a single bigged area
 *  
 *  function that, given 2 areas that are overlapping, creates a single area beeing the sum of the 2 given areas 
 */
void sumAreas(area_t * a1, area_t a2) {
  if((a1->x < 0 || a1->y < 0 || a1->w < 0 || a1->h < 0) && (a2.x < 0 || a2.y < 0 || a2.w <0 || a2.h < 0)) {
    a1->x = -1;
    a1->y = -1;
    a1->w = -1;
    a1->h = -1;
  } else if (a1->x < 0 || a1->y < 0 || a1->w < 0 || a1->h < 0) {
    a1->x = a2.x;
    a1->y = a2.y;
    a1->w = a2.w;
    a1->h = a2.h;
  } else if (a2.x < 0 || a2.y < 0 || a2.w < 0 || a2.h < 0) {
  } else {
    a1->x = MIN(a1->x, a2.x);
    a1->y = MIN(a1->y, a2.y);
    a1->w = MAX(a1->w, a2.w);
    a1->h = MAX(a1->h, a2.h);
  }
}

/* compare(uint8_t *in, uint8_t saved[3*PIX_LEN/16], area_t * outs, pair_t differences[2][WIDTH/8])
 *  [in, saved] = arrays containing the 2 subsampled images to be compared
 *  outs = recipient for the output differences
 *  differences = recipient for the intermediate 1D differences (2 image rows)
 *  
 *  function that generates the 2D differences between the 2 images given as input 
 */
uint8_t compare(uint8_t *in, uint8_t saved[3*PIX_LEN/16], area_t * outs, pair_t differences[2][WIDTH/8]) {
  int isDifferent = 0;
  int index = 0;
  int outsIndex = 0;
  int i = 0, j = 0, oldj = 0;
  for (i = 0; i < 100; i++) {
    outs[i].x = outs[i].y = outs[i].w = outs[i].h = -1;
  }
  for (i = 0; i < WIDTH/8; i++) {
    differences[0][i].beg = differences[0][i].end = differences[0][i].row = differences[0][i].done = -1;
    differences[1][i].beg = differences[1][i].end = differences[1][i].row = differences[1][i].done = -1;
  }
  for(i = 0; i < PIX_LEN/16; i++) {
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
            differences[index][k].done = differences[!index][z].done;
            area_t a = {.x = differences[index][k].beg, .y = differences[index][k].row, .w = differences[index][k].end, .h = differences[index][k].row};
            sumAreas(&outs[differences[!index][z].done], a);
          }
        }
        if(!added) {
          if(outsIndex > 99) {
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
            if (outsIndex > 99) return outsIndex;
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
    double_t cR =  in[3*((i/(WIDTH/4))*(WIDTH/4)+(i%(WIDTH/4)))] + saved[3*((i/(WIDTH/4))*(WIDTH/4)+(i%(WIDTH/4)))];
    cR /= 2;
    uint32_t Rdelta = (double_t)(in[3*((i/(WIDTH/4))*(WIDTH/4)+(i%(WIDTH/4)))] - saved[3*((i/(WIDTH/4))*(WIDTH/4)+(i%(WIDTH/4)))]);
    uint32_t Gdelta = (double_t)(in[3*((i/(WIDTH/4))*(WIDTH/4)+(i%(WIDTH/4)))+1] - saved[3*((i/(WIDTH/4))*(WIDTH/4)+(i%(WIDTH/4)))+1]);
    uint32_t Bdelta = (double_t)(in[3*((i/(WIDTH/4))*(WIDTH/4)+(i%(WIDTH/4)))+2] - saved[3*((i/(WIDTH/4))*(WIDTH/4)+(i%(WIDTH/4)))+2]);
    Rdelta *= Rdelta;
    Gdelta *= Gdelta;
    Bdelta *= Bdelta;
    Rdelta *= (2 + (cR/256));
    Gdelta *= 4;
    Bdelta *= (2 + ((255 - cR)/256));
    if ((Rdelta + Gdelta + Bdelta) > 600) {
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
  for (i = 0; i < outsIndex; i++) enlargeAdjust(&outs[i]);
  for (i = 0; i < outsIndex; i++) for (int j = i+1; j < outsIndex; j++) if (overlap2(outs[i], outs[j])) {
    sumAreas(&outs[i], outs[j]);
    outsIndex--;
    outs[j].x = outs[outsIndex].x;
    outs[j].y = outs[outsIndex].y;
    outs[j].w = outs[outsIndex].w;
    outs[j].h = outs[outsIndex].h;
    j--;
  }
  i = 0;
  while(i < outsIndex) if (outs[i].w < 32 && outs[i].h < 24) {
    outsIndex--;
    if (i < outsIndex) {
      outs[i].x = outs[outsIndex].x;
      outs[i].y = outs[outsIndex].y;
      outs[i].w = outs[outsIndex].w;
      outs[i].h = outs[outsIndex].h;
    }
    outs[outsIndex].x = -1;
    outs[outsIndex].y = -1;
    outs[outsIndex].w = -1;
    outs[outsIndex].h = -1;
  } else i++;
  return outsIndex;
}

/* enlargeAdjust(area_t * a)
 *  a = area that is going to be enlarged
 *  
 *  function that enlarges the given area in order to make it fit the
 *  original image dimensions and enshures that the generated areas are
 *  compatible with the 2DDCT constraints (width and height multiple of 16)
 */
inline void enlargeAdjust(area_t * a) {
  a->w = a->w - a->x + 1;
  a->h = a->h - a->y + 1;
  a->x *= 4;
  a->y *= 4;
  a->w *= 4;
  a->h *= 4;
  a->x -= (16 - (a->w%16))/2;
  a->y -= (16 - (a->h%16))/2;
  a->w += a->w%16 ? (16 - a->w%16) : 0;
  a->h += a->h%16 ? (16 - a->h%16) : 0;
  a->w = a->w > WIDTH ? WIDTH : a->w;
  a->h = a->h > HEIGHT ? HEIGHT : a->h;
  a->x -= a->x + a->w > WIDTH ? (a->x + a->w) - WIDTH : 0;
  a->y -= a->y + a->h > HEIGHT ? (a->y + a->h) - HEIGHT : 0;
  a->x = a->x < 0 ? 0 : a->x;
  a->y = a->y < 0 ? 0 : a->y;
}
