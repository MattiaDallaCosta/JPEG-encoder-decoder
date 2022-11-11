#pragma once

#include "structs.h"
#include <stdint.h>

void subsample(uint8_t in[3][PIX_LEN], uint8_t out[3][PIX_LEN/16]);
void store(uint8_t in[3][PIX_LEN/16]);
int compare(uint8_t in[3][PIX_LEN/16], area_t outs[20]);
