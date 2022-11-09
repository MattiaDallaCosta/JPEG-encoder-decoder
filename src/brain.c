#include "../include/brain.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

int saved[3][PIX_LEN/3];

void subsample(int in[3][PIX_LEN], int out[3][PIX_LEN/16]) {
  int i = 0;
  for (; i < PIX_LEN/16; i++) {
    int bigh = (i/(WIDTH/4))*4;
    int bigw = (i%(WIDTH/4))*4;
    out[0][i] = (in[0][bigh*WIDTH+bigw]     + in[0][bigh*WIDTH+bigw+1]     + in[0][bigh*WIDTH+bigw+2]     + in[0][bigh*WIDTH+bigw+3]);
    out[0][i] += (in[0][(bigh+1)*WIDTH+bigw] + in[0][(bigh+1)*WIDTH+bigw+1] + in[0][(bigh+1)*WIDTH+bigw+2] + in[0][(bigh+1)*WIDTH+bigw+3]);
    out[0][i] += (in[0][(bigh+2)*WIDTH+bigw] + in[0][(bigh+2)*WIDTH+bigw+1] + in[0][(bigh+2)*WIDTH+bigw+2] + in[0][(bigh+2)*WIDTH+bigw+3]);
    out[0][i] += (in[0][(bigh+3)*WIDTH+bigw] + in[0][(bigh+3)*WIDTH+bigw+1] + in[0][(bigh+3)*WIDTH+bigw+2] + in[0][(bigh+3)*WIDTH+bigw+3]);
    out[0][i] /= 16;
    out[1][i] = (in[1][bigh*WIDTH+bigw]     + in[1][bigh*WIDTH+bigw+1]     + in[1][bigh*WIDTH+bigw+2]     + in[1][bigh*WIDTH+bigw+3]);
    out[1][i] += (in[1][(bigh+1)*WIDTH+bigw] + in[1][(bigh+1)*WIDTH+bigw+1] + in[1][(bigh+1)*WIDTH+bigw+2] + in[1][(bigh+1)*WIDTH+bigw+3]);
    out[1][i] += (in[1][(bigh+2)*WIDTH+bigw] + in[1][(bigh+2)*WIDTH+bigw+1] + in[1][(bigh+2)*WIDTH+bigw+2] + in[1][(bigh+2)*WIDTH+bigw+3]);
    out[1][i] += (in[1][(bigh+3)*WIDTH+bigw] + in[1][(bigh+3)*WIDTH+bigw+1] + in[1][(bigh+3)*WIDTH+bigw+2] + in[1][(bigh+3)*WIDTH+bigw+3]);
    out[1][i] /= 16;
    out[2][i] = (in[2][bigh*WIDTH+bigw]     + in[2][bigh*WIDTH+bigw+1]     + in[2][bigh*WIDTH+bigw+2]     + in[2][bigh*WIDTH+bigw+3]);
    out[2][i] += (in[2][(bigh+1)*WIDTH+bigw] + in[2][(bigh+1)*WIDTH+bigw+1] + in[2][(bigh+1)*WIDTH+bigw+2] + in[2][(bigh+1)*WIDTH+bigw+3]);
    out[2][i] += (in[2][(bigh+2)*WIDTH+bigw] + in[2][(bigh+2)*WIDTH+bigw+1] + in[2][(bigh+2)*WIDTH+bigw+2] + in[2][(bigh+2)*WIDTH+bigw+3]);
    out[2][i] += (in[2][(bigh+3)*WIDTH+bigw] + in[2][(bigh+3)*WIDTH+bigw+1] + in[2][(bigh+3)*WIDTH+bigw+2] + in[2][(bigh+3)*WIDTH+bigw+3]);
    out[2][i] /= 16;
  }
}

void store(int in[3][PIX_LEN/16]) {
  int i = 0;
  printf("\n");
  for(; i < PIX_LEN/16; i++) {
    printf("\033[1A%i %i %i\n", in[0][i], in[1][i], in[2][i]);
    saved[0][i] = in[0][i];
    saved[1][i] = in[1][i];
    saved[2][i] = in[2][i];
    usleep(500);
  }
}

int compare(int in[3][PIX_LEN/16], area_t outs[20]){
  int isDifferent = 0;
  diff_t differences[HEIGHT/4];
  // Area tmp = {-1, -1, -1, -1};
  diff_t app;
  for (int i = 0; i < HEIGHT/4; i++) {
    differences[i].row = -1;
    for (int j = 0; j < WIDTH/4; j++) {
      differences[i].diffs[j].beg = -1;
      differences[i].diffs[j].end = -1;
    }
  }
  int i = 0, j = 0;
  for(; i < PIX_LEN/16; i++) {
    if(!(i/(WIDTH/4))){
    int k = 0;
      for (; k < WIDTH/4; k++) {
        if (app.diffs[k].beg == -1 || app.diffs[k].end == -1) break;
      }
      if(k != 0) memcpy(&differences[j++], &app, sizeof(app));
      app.row = i/(WIDTH/4);
    }
    int z = 0;
    if (in[0][i] != saved[0][i] || in[1][i] != saved[1][i] || in[2][i] != saved[2][i]) {
      if (!isDifferent) {
        app.diffs[z].beg = i%(WIDTH/4);
      }
      app.diffs[z].end = i%(WIDTH/4);
      isDifferent = 1;
    } else if (isDifferent) {
      isDifferent = 0;
      z++;
    }
  }
  struct pair {
    int raw, val;
  } begs[WIDTH/4], ends[WIDTH/4], appo[WIDTH/4];
  return isDifferent;
}
