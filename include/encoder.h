#pragma once

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include "structs.h"

void getSavedName(char *name, char *buff);
void getName(char *name, char *buff, int num);
int readPpm(FILE* f, uint8_t raw[3][PIX_LEN]);
int writePpm(FILE * f, uint8_t sub[3][PIX_LEN/16]);
int writeDiffPpm(char * filename, uint8_t sub[3][PIX_LEN], area_t * dims);
void encodeNsend(char * name, uint8_t raw[3][PIX_LEN], area_t dims);
