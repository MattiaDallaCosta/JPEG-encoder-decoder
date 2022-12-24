#pragma once

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include "structs.h"

void getSavedName(char *name, char *buff);
void getName(char *name, char *buff, int num);
size_t encodeNsend(uint8_t *jpg, uint8_t *raw, area_t dims);
