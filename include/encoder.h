#pragma once

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include "structs.h"

void rgb_to_dct(uint8_t *in, int16_t *Y, int16_t *Cb, int16_t *Cr, area_t dims);
void init_huffman(int16_t * Y, int16_t * Cb, int16_t * Cr, area_t dims, huff_code Luma[2], huff_code Chroma[2]);
size_t write_jpg(FILE * f, uint8_t * jpg, int16_t * Y, int16_t * Cb, int16_t * Cr, area_t dims, huff_code Luma[2], huff_code Chroma[2]);
