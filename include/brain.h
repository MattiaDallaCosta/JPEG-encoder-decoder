#pragma once

#include "structs.h"

void subsample(int in[3][PIX_LEN], int out[3][PIX_LEN/16]);
void store(int in[3][PIX_LEN/16]);
int compare(int in[3][PIX_LEN/16], area_t outs[20]);
// int validArea(Area a);
