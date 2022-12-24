 #pragma once

#include "structs.h"
#include <stdint.h>

void subsample(uint8_t *in, uint8_t *out);
void store(uint8_t *in);
int compare(uint8_t *in, area_t outs[20]);
void enlargeAdjust(area_t * a);
