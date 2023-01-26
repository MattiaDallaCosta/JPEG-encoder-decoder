#pragma once

#include "define.h"

// typedef struct {
//   long mtype;
//   char mtext[100];
//   int len;
// } msg_t;

typedef struct __huff_code {
	int sym_freq[257];     // frequency of occurrence of symbol i
	int code_len[257];     // code length of symbol i
	int next[257];         // index to next symbol in chain of all symbols in current branch of code tree
	int code_len_freq[32]; // the frequencies of huff-symbols of length i
	int sym_sorted[256];   // the symbols to be encoded
	int sym_code_len[256]; // the huffman code length of symbol i
	int sym_code[256];     // the huffman code of the symbol i
} huff_code;

typedef struct {
  int x, y;
  int w, h;
} area_t;

typedef struct {
  int beg, end, row, done;
  int diff[8];
} pair_t;

