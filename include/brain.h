 #pragma once

#include "structs.h"
#include <stdint.h>

void subsample(uint8_t *in, uint8_t *out);
void store(uint8_t *in, uint8_t saved[3*PIX_LEN/16]);
uint8_t compare_block(uint8_t *in, uint8_t saved[3*PIX_LEN/16], area_t * outs, pair_t * differences, int numOuts, uint8_t off);
// uint8_t compare(uint8_t *in, uint8_t saved[3*PIX_LEN/16], area_t outs[20]);
void enlargeAdjust(area_t * a);
