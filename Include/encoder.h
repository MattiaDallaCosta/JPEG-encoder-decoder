#pragma once

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include "define.h"

typedef struct __huff_code
{
	int sym_freq[257];     // frequency of occurrence of symbol i
	int code_len[257];     // code length of symbol i
	int next[257];         // index to next symbol in chain of all symbols in current branch of code tree
	int code_len_freq[32]; // the frequencies of huff-symbols of length i
	int sym_sorted[256];   // the symbols to be encoded
	int sym_code_len[256]; // the huffman code length of symbol i
	int sym_code[256];     // the huffman code of the symbol i
} huff_code;

typedef struct {
  int w, h;
} dims_t;
int read_ppm(FILE* f, int raw[3][PIX_LEN]);
void encodeNsend(char * name, int raw[3][PIX_LEN]);
