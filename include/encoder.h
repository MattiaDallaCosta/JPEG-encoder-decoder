#pragma once

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include "structs.h"

void getSavedName(char *name, char *buff);
void getName(char *name, char *buff, int num);
void rgb_to_dct(uint8_t *in, int16_t *Y, int16_t *Cb, int16_t *Cr, area_t dims);
size_t encodeNsend(uint8_t *jpg, uint8_t *raw, area_t dims);
