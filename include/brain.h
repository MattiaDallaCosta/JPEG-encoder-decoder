#pragma once

#include "define.h"

typedef struct {
  int minh, minw, maxh, maxw;
} Area;

void subsample(int in[3][PIX_LEN], int out[3][PIX_LEN/9]);
void store(int in[3][PIX_LEN/9]);
Area compare(int in[3][PIX_LEN/9]);
int validArea(Area a);
