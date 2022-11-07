#include "../include/brain.h"

int saved[3][PIX_LEN/3];

void subsample(int in[3][PIX_LEN], int out[3][PIX_LEN/9]) {
  int i = 0;
  for (; i < PIX_LEN/9; i++) {
    int bigh = (i/(WIDTH/3))*3;
    int bigw = (i%(WIDTH/3))*3;
    out[0][i] = (in[0][bigh*WIDTH+bigw] + in[0][bigh*WIDTH+bigw+1] + in[0][bigh*WIDTH+bigw+2] + in[0][(bigh+1)*WIDTH+bigw] + in[0][(bigh+1)*WIDTH+bigw+1] + in[0][(bigh+1)*WIDTH+bigw+2] + in[0][(bigh+2)*WIDTH+bigw] + in[0][(bigh+2)*WIDTH+bigw+1] + in[0][(bigh+2)*WIDTH+bigw+2])/9;
    out[1][i] = (in[1][bigh*WIDTH+bigw] + in[1][bigh*WIDTH+bigw+1] + in[1][bigh*WIDTH+bigw+2] + in[1][(bigh+1)*WIDTH+bigw] + in[1][(bigh+1)*WIDTH+bigw+1] + in[1][(bigh+1)*WIDTH+bigw+2] + in[1][(bigh+2)*WIDTH+bigw] + in[1][(bigh+2)*WIDTH+bigw+1] + in[1][(bigh+2)*WIDTH+bigw+2])/9;
    out[2][i] = (in[2][bigh*WIDTH+bigw] + in[2][bigh*WIDTH+bigw+1] + in[2][bigh*WIDTH+bigw+2] + in[2][(bigh+1)*WIDTH+bigw] + in[2][(bigh+1)*WIDTH+bigw+1] + in[2][(bigh+1)*WIDTH+bigw+2] + in[2][(bigh+2)*WIDTH+bigw] + in[2][(bigh+2)*WIDTH+bigw+1] + in[2][(bigh+2)*WIDTH+bigw+2])/9;
  }
}

void store(int in[3][PIX_LEN/9]) {
  int i = 0;
  for(; i < PIX_LEN/9; i++) {
    saved[0][i] = in[0][i];
    saved[1][i] = in[1][i];
    saved[2][i] = in[2][i];
  }
}

Area compare(int in[3][PIX_LEN/9]){
  int isDifferent = 0;
  Area tmp = {-1, -1, -1, -1};
  int i = 0;
  for(; i < PIX_LEN/9; i++) {
    if (in[0][i] != saved[0][i] || in[1][i] != saved[1][i] || in[2][i] != saved[2][i]) {
      int w = i%(WIDTH/3), h = i/(WIDTH/3);
      if(!isDifferent){
        isDifferent = 1;
        tmp.minh = h;
        tmp.minw = w;
        tmp.maxh = h;
        tmp.maxw = w;
      } else {
        tmp.minw = MIN(tmp.minw, w);
        tmp.maxw = MAX(tmp.maxw, w);
        tmp.maxh = MAX(tmp.maxh, h);
      }
    }
  }
  return tmp;
}

inline int validArea(Area a){
  if(a.minw >= 0 && a.minh >= 0 && a.maxw >= 0 && a.maxh >= 0) return 1;
  return 0;
}
