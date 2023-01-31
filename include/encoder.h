#pragma once

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include "structs.h"

void getSavedName(char *name, char *buff);
void getName(char *name, char *buff, int num);
int readPpm(FILE* f, uint8_t * raw);
int writePpm(FILE * f, uint8_t * sub);
void rgb_to_dct(uint8_t *in, int16_t *Y, int16_t *Cb, int16_t *Cr, area_t dims);
void rgb_to_dct_block(uint8_t *in, int16_t *Y, int16_t *Cb, int16_t *Cr,int offx, int offy, area_t dims);
void init_huffman(int16_t * Y, int16_t * Cb, int16_t * Cr, area_t dims, huff_code Luma[2], huff_code Chroma[2]);
size_t write_jpg(uint8_t * jpg, int16_t * Y, int16_t * Cb, int16_t * Cr, area_t dims, huff_code Luma[2], huff_code Chroma[2]);
size_t write_file(char * filename, uint8_t* jpg, int16_t * Y, int16_t * Cb, int16_t * Cr, area_t dims, huff_code Luma[2], huff_code Chroma[2]);

