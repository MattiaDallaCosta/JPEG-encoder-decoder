 #pragma once

#include "structs.h"
#include <stdint.h>
#include <stdio.h>

void subsample(FILE * f, uint8_t *in, uint8_t *out);
void store(uint8_t *in, uint8_t saved[3*PIX_LEN/16]);
uint8_t compare(uint8_t *in, uint8_t saved[3*PIX_LEN/16], area_t * outs, pair_t differences[2][WIDTH/8]);
void enlargeAdjust(area_t * a);
