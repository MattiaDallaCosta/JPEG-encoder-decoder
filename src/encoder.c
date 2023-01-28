#include "../include/encoder.h"
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>

const int64_t lookup_table[] = {        // contains cos((double_t)(2*(i/8)+1)*(i%8)*M_PI/16) with i = [0-63] saved as int64_t to avoid aproximation
4607182418800017408,  4607009347991985328,  4606496786581982534,  4605664432017547683,  4604544271217802189,  4603179351334086857,  4600565431771507044,  4596196889902818829,
4607182418800017408,  4605664432017547683,  4600565431771507044, -4627175146951956984, -4618827765636973620, -4616362688862790480, -4616875250272793273, -4620192685520688952,
4607182418800017408,  4603179351334086857, -4622806605083268766, -4616362688862790480, -4618827765636973618,  4596196889902818828,  4606496786581982532,  4605664432017547685,
4607182418800017408,  4596196889902818829, -4616875250272793274, -4620192685520688952,  4604544271217802187,  4605664432017547685, -4622806605083268763, -4616362688862790478,
4607182418800017408, -4627175146951956984, -4616875250272793273,  4603179351334086853,  4604544271217802190, -4617707604837228126, -4622806605083268751,  4607009347991985328,
4607182418800017408, -4620192685520688954, -4622806605083268755,  4607009347991985328, -4618827765636973627, -4627175146951956990,  4606496786581982534, -4617707604837228127,
4607182418800017408, -4617707604837228124,  4600565431771507047,  4596196889902818845, -4618827765636973623,  4607009347991985330, -4616875250272793277,  4603179351334086850,
4607182418800017408, -4616362688862790480,  4606496786581982532, -4617707604837228126,  4604544271217802180, -4620192685520688958,  4600565431771507039, -4627175146951956970};

const int luma_quantizer[] = {
16, 11, 10, 16,  24,  40,  51,  61,
12, 12, 14, 19,  26,  58,  60,  55,
14, 13, 16, 24,  40,  57,  69,  56,
14, 17, 22, 29,  51,  87,  80,  62,
18, 22, 37, 56,  68, 109, 103,  77,
24, 35, 55, 64,  81, 104, 113,  92,
49, 64, 78, 87, 103, 121, 120, 101,
72, 92, 95, 98, 112, 100, 103,  99};

const int chroma_quantizer[] = {
17, 18, 24, 47, 99, 99, 99, 99,
18, 21, 26, 66, 99, 99, 99, 99,
24, 26, 56, 99, 99, 99, 99, 99,
47, 66, 99, 99, 99, 99, 99, 99,
99, 99, 99, 99, 99, 99, 99, 99,
99, 99, 99, 99, 99, 99, 99, 99,
99, 99, 99, 99, 99, 99, 99, 99,
99, 99, 99, 99, 99, 99, 99, 99};

const int scan_order[] = {
 0,  1,  8, 16,  9,  2,  3, 10,
17, 24, 32, 25, 18, 11,  4,  5,
12, 19, 26, 33, 40, 48, 41, 34,
27, 20, 13,  6,  7, 14, 21, 28,
35, 42, 49, 56, 57, 50, 43, 36,
29, 22, 15, 23, 30, 37, 44, 51,
58, 59, 52, 45, 38, 31, 39, 46,
53, 60, 61, 54, 47, 55, 62, 63};

int readPpm(FILE* f, uint8_t * raw) {
	if( fgetc(f) != 'P' || fgetc(f) != '6' )
	{
		fprintf(stderr, "Could not find magic number for this PPM!\n");
		return -1;
	}

	if (fgetc(f) != '\n' )
		goto X;

	char buf[1024];
	while(1)
	{
		char* p = buf;
		while ((*p = fgetc(f)) != '\n')
			p++;
		*p = '\0';

		if (buf[0] != '#')
			break;
	}
	int width, height;
	if (sscanf(buf, "%d %d\n", &width, &height) != 2)
		goto X;
	if (width != WIDTH || height != HEIGHT) {
		fprintf(stderr, "Only pictures with dimensions pre-defined\n");
		return -1;
	}

	int depth;
	if (fscanf(f, "%d\n", &depth) != 1)
		goto X;

	if (depth != 255)
	{
		printf("For simplicity, only a bit-depth of 256 is supported!\n");
		return -1;
	}

	long len = ftell(f);
	fseek(f, 0L, SEEK_END);
	len = ftell(f) - len;
	fseek(f, -len, SEEK_END);
	if (len != 3*PIX_LEN) // 3 color channels
		goto X;

	int i;
	for (i=0; i<PIX_LEN; i++)
	{
	  raw[3*i+0] = fgetc(f);
	  raw[3*i+1] = fgetc(f);
	  raw[3*i+2] = fgetc(f);
	}
	return 0;
	
X:	fprintf(stderr, "Could not parse the PPM file properly\n");
	return -1;
}


double_t getDouble(const int64_t *bitval) {                                              // convert the double_t value, saved as int64_t in order
    return *((double_t*)(bitval));                                                       // to maintain the preciseness, into an actual double_t
}

void getName(char *name, char *buff, int num) {
  char *pos;
  strcpy(buff, name);
  pos = strrchr(buff, '.') ? (strrchr(buff, '.') < strrchr(buff, '/') ? buff + strlen(buff) : strrchr(buff, '.')) : buff + strlen(buff);
  char *end;
  if(num < 0) strcpy(end, ".jpg");
  else sprintf(end, "-%i.jpg", num);
  strcpy(pos, end);
}

void getSavedName(char *name, char *buff) {
  char *pos;
  strcpy(buff, name);
  pos =  strrchr(buff, '/') ? strrchr(buff, '/') : buff;
  strcpy(*pos == '/' ? pos+1 : pos,  "savedImage.ppm");
}

void zigzag_block(int16_t in[64], int16_t out[64]) {
	int i = 0;
	for (; i < 64; i++) {
    out[i] = in[scan_order[i]];
  }
}

void dct_block(int gap, uint8_t in[], int16_t out[],const int quantizer[]) {
	int x_f, y_f; // frequency domain coordinates
	int x_t, y_t; // time domain coordinates

	double inner_lookup[64];
	int16_t dct[64];
	for (x_t=0; x_t<8; x_t++)
		for (y_f=0; y_f<8; y_f++)
		{
			inner_lookup[x_t*8 + y_f] = 0;
			for (y_t=0; y_t<8; y_t++)
				inner_lookup[x_t*8 + y_f] += ( in[y_t*gap+x_t] - 128 ) * getDouble(lookup_table + (y_t*8 + y_f)); // cos_lookup[y_t][y_f];

		}

	// freq(x_f,y_f) = ...
	double freq;
	for (y_f=0; y_f<8; y_f++) for (x_f=0; x_f<8; x_f++) {
			freq = 0;
			for(x_t=0; x_t<8; x_t++)
				freq += inner_lookup[x_t*8 + y_f] * getDouble(lookup_table + (x_t*8 + x_f)); //cos_lookup[x_t][x_f];

			if (x_f == 0)
				freq *= M_SQRT1_2;
			if (y_f == 0)
				freq *= M_SQRT1_2;
			freq /= 4;

		  dct[y_f*8+x_f] = freq / quantizer[y_f*8+x_f%64];
		  dct[y_f*8+x_f] = CLIP(dct[y_f*8+x_f], -2048, 2047);
		}
  zigzag_block(dct, out);
}

// void zigzag(int in[3][PIX_LEN], int out[3][PIX_LEN]) {
// 	int i;
// 	for (i=0; i<PIX_LEN; i++) {
// 		out[0][i] = in[0][(i/64)*64+scan_order[i%64]];
//     if (i< PIX_LEN/4) {
// 		   out[1][i] = in[1][(i/64)*64+scan_order[i%64]];
// 		   out[2][i] = in[2][(i/64)*64+scan_order[i%64]];
//     }
// 	}
// }

void rgb_to_dct_block(uint8_t *in, int16_t *Y, int16_t *Cb, int16_t *Cr,int offx, int offy) {
  int i = 0;
  uint8_t app[2][32];
  uint8_t offCx = offx/2;
  uint8_t offCy = offy/2;
  uint8_t midY[256];
  uint8_t midCbCr[2][64];
  int begin, j;
  for (i = 0; i < 256; i++) {
  int r = i%16;
  int l = i/16;
	midY[i]            =       0.299    * in[3*((offy+l)*WIDTH+offx+r)] + 0.587    * in[3*((offy+l)*WIDTH+offx+r)+1] + 0.114    * in[3*((offy+l)*WIDTH+offx+r)+2];
	app[0][(l%2)*16+r] = 128 - 0.168736 * in[3*((offy+l)*WIDTH+offx+r)] - 0.331264 * in[3*((offy+l)*WIDTH+offx+r)+1] + 0.5      * in[3*((offy+l)*WIDTH+offx+r)+2];
	app[1][(l%2)*16+r] = 128 + 0.5      * in[3*((offy+l)*WIDTH+offx+r)] - 0.418688 * in[3*((offy+l)*WIDTH+offx+r)+1] - 0.081312 * in[3*((offy+l)*WIDTH+offx+r)+2];
    if (r%2 == 1 && l%2 == 1) {
			 midCbCr[0][(l/2*16)/2+r/2] = (app[0][r-1] + app[0][r] + app[0][15+r] + app[0][16+r])/4;
			 midCbCr[1][(l/2*16)/2+r/2] = (app[1][r-1] + app[1][r] + app[1][15+r] + app[1][16+r])/4;
    }
    if (r%8 == 7 && l%8 == 7) {
      begin = i - 119;
      j = ((begin%16) + (begin/16)*2)/8;
      int ih = j%2; 
      int iv = j/2; 
      dct_block(16, midY + (iv)*128 + ih*8, Y + (offy/8*WIDTH/8+offx/8+iv*2+ih)*64, luma_quantizer);
    }
  }
  dct_block(8, midCbCr[0], Cb + (offCy/8*WIDTH/16+offCx/8)*64, chroma_quantizer);
  dct_block(8, midCbCr[1], Cr + (offCy/8*WIDTH/16+offCx/8)*64, chroma_quantizer);
}
void rgb_to_dct(uint8_t *in, int16_t *Y, int16_t *Cb, int16_t *Cr, area_t dims) {
  int i, j;  
  uint8_t offx = 0, offy = 0;
  int last[3] = {0, 0, 0};
  for (i = 0; i < (dims.w/16)*(dims.h/16); i++) {
    rgb_to_dct_block(in, Y, Cb, Cr, offx, offy);
    for (j = 0; j < 4; j++) {
      Y[((offy+(j%2))*dims.w)*8 + (offx+(j/2))*8] += last[0];
      last[0] += Y[((offy+(j%2))*dims.w)*8 + (offx+(j/2))*8];
    }
    Cb[(offy/2)*dims.w/2+offx/2] -= last[1]; 
    last[1] += Cb[(offy/2)*dims.w/2+offx/2]; 
    Cr[(offy/2)*dims.w/2+offx/2] -= last[2]; 
    last[2] += Cr[(offy/2)*dims.w/2+offx/2]; 
    if(i%(dims.w/16) == (dims.w/16)-1) {
      offx += 16;
      offy = dims.y;
    } else offy += 16;
  }
}

// void rgb_to_dct_full(uint8_t *in, int16_t *Y, int16_t *Cb, int16_t *Cr, area_t dims) {
//   // printf("dims x: %i y: %i w: %i h: %i\n", dims.x, dims.y, dims.w, dims.h);
//   int i = 0;
//   int off = dims.y*WIDTH + dims.x;
//   uint8_t app[2][2*dims.w];
//   uint8_t mid[3][dims.h*dims.w];
//   // int dctapp[3][8*WIDTH];
//   int last[3] = {0, 0, 0};
//   int begin, j;
// 	for (; i < dims.w*dims.h; i++) {
//     int r = i%dims.w;
//     int l = i/dims.w;
// 		// mid[0][(l%8)*WIDTH+r]                =       0.299    * in[0][i] + 0.587    * in[1][i] + 0.114    * in[2][i];
// 		mid[0][i]              =       0.299    * in[3*(i+off)] + 0.587    * in[3*(i+off)+1] + 0.114    * in[3*(i+off)+2];
// 		app[0][(l%2)*dims.w+r] = 128 - 0.168736 * in[3*(i+off)] - 0.331264 * in[3*(i+off)+1] + 0.5      * in[3*(i+off)+2];
// 		app[1][(l%2)*dims.w+r] = 128 + 0.5      * in[3*(i+off)] - 0.418688 * in[3*(i+off)+1] - 0.081312 * in[3*(i+off)+2];
//     if (r%2 == 1 && l%2 == 1) {
// 			 mid[1][(l/2*dims.w)/2+r/2] = (app[0][r-1] + app[0][r] + app[0][dims.w+r-1] + app[0][dims.w+r])/4;
// 			 mid[2][(l/2*dims.w)/2+r/2] = (app[1][r-1] + app[1][r] + app[1][dims.w+r-1] + app[1][dims.w+r])/4;
//     }
//     if (r%8 == 7 && l%8 == 7) {
//       begin = i - 7*(dims.w+1);
//       j = ((begin%dims.w) + (begin/dims.w)*(dims.w/8))/8;
//       int ih = j%(dims.w/8); 
//       int iv = j/(dims.w/8); 
//       // dct_block(WIDTH, mid[0] + ih*8, out[0] + (iv*(WIDTH/8)+ih)*64, luma_quantizer);
//       dct_block(dims.w, mid[0] + (iv)*dims.w*8 + ih*8, Y + (iv*(dims.w/8)+ih)*64, luma_quantizer);
//       Y[(iv*(dims.w/8)+ih)*64] -= last[0];
//       last[0] += Y[(iv*(dims.w/8)+ih)*64];
//       if (ih%2 == 1 && iv%2 == 1) {
//         begin = begin - 8*(dims.w+1);
//         j = ((begin%(dims.w)) + (begin/(dims.w))*(dims.w/8))/8;
//         ih = j%(dims.w/8); 
//         iv = j/(dims.w/8); 
//         // dct_block(WIDTH/2, mid[1] + (ih)*4, out[1] + (iv*WIDTH/16+ih)*32, chroma_quantizer);
//         // dct_block(WIDTH/2, mid[2] + (ih)*4, out[2] + (iv*WIDTH/16+ih)*32, chroma_quantizer);
//         dct_block(dims.w/2, mid[1] + (iv*dims.w + ih*2)*2, Cb + (iv*dims.w/16+ih)*32, chroma_quantizer);
//         dct_block(dims.w/2, mid[2] + (iv*dims.w + ih*2)*2, Cr + (iv*dims.w/16+ih)*32, chroma_quantizer);
//         Cb[(iv*dims.w/16+ih)*32] -= last[1];
//         last[1] += Cb[(iv*dims.w/16+ih)*32];
//         Cr[(iv*dims.w/16+ih)*32] -= last[2];
//         last[2] += Cr[(iv*dims.w/16+ih)*32];
//       }
//     }
//     if (r == (dims.w - 1)){
//       printf("off : %i\n", off);
//       off += (WIDTH - dims.w);
//       printf("off : %i\n", off);
//     }
// 	}
// }

void init_huff_table(huff_code* hc)
{
	int i;
	for (i=0; i<257; i++)
	{
		hc->code_len[i] = 0;
		hc->next[i] = -1;
	}

	// derive the code length for each symbol
	while (1)
	{
		int v1 = -1;
		int v2 = -1;
		int i;
		// find least value of freq(v1) and next least value of freq(v2)
		for (i=0; i<257; i++)
		{
			if (hc->sym_freq[i] == 0)
				continue;
			if ( v1 == -1 ||  hc->sym_freq[i] <= hc->sym_freq[v1] )
			{
				v2 = v1;
				v1 = i;
			}
			else if (v2 == -1 || hc->sym_freq[i] <= hc->sym_freq[v2])
				v2 = i;
		}
		if (v2 == -1)
			break;

		hc->sym_freq[v1] += hc->sym_freq[v2];
		hc->sym_freq[v2] = 0;
		while (1)
		{
			hc->code_len[v1]++;
			if (hc->next[v1] == -1)
				break;
			v1 = hc->next[v1];
		}
		hc->next[v1] = v2;
		while (1)
		{
			hc->code_len[v2]++;
			if (hc->next[v2] == -1)
				break;
			v2 = hc->next[v2];
		}
	}

	for (i=0; i<32; i++)
		hc->code_len_freq[i] = 0;

	// derive code length frequencies
	for (i=0; i<257; i++)
		if (hc->code_len[i] != 0)
			hc->code_len_freq[hc->code_len[i]]++;

	// limit the huffman code length to 16 bits
	i=31;
	while (1)
	{
		if (hc->code_len_freq[i] > 0) // if the code is too long ...
		{
			int j = i-1;
			while (hc->code_len_freq[--j] <= 0); // ... we search for an upper layer containing leaves
			hc->code_len_freq[i] -= 2; // we remove the two leaves from the lowest layer
			hc->code_len_freq[i-1]++; // and put one of them one position higher
			hc->code_len_freq[j+1] += 2; // the other one goes at a new branch ...
			hc->code_len_freq[j]--; // ... together with the leave node which was there before
			continue;
		}
		i--;
		if (i!=16)
			continue;
		while (hc->code_len_freq[i--] == 0);
      i++;
		hc->code_len_freq[i]--; // remove one leave from the lowest layer (the bottom symbol '111...111')
		break;
	}

	// sort the input symbols according to their code size
	for (i=0; i<256; hc->sym_sorted[i++] = -1);
	int j;
	int k = 0;
	for (i=1; i<32; i++)
		for (j=0; j<256; j++)
			if (hc->code_len[j] == i)
				hc->sym_sorted[k++] = j;
	// determine the size of the huffman code symbols - this may differ from code_len because
	// of the 16 bit limit
	for (i=0; i<256; i++)
		hc->sym_code_len[i] = 0;
	k=0;
	for (i=1; i<=16; i++)
		for (j=1; j<=hc->code_len_freq[i]; j++)
			hc->sym_code_len[hc->sym_sorted[k++]] = i;
	hc->sym_code_len[hc->sym_sorted[k]] = 0;

	// generate the codes for the symbols
	for (i=0; i<256; i++)
		hc->sym_code[i] = -1;
	k = 0;
	int code = 0;
	int si = hc->sym_code_len[hc->sym_sorted[0]];
	while (1)
	{
		do
		{
			hc->sym_code[hc->sym_sorted[k]] = code;
			k++;
			code++;
		} while (hc->sym_code_len[hc->sym_sorted[k]] == si);
		if (hc->sym_code_len[hc->sym_sorted[k]] == 0)
			break;
		do
		{
			code <<= 1;
			si++;
		} while (hc->sym_code_len[hc->sym_sorted[k]] != si);
	}
}

int huff_class(int value)
{
	value = value<0 ? -value : value;
	int class = 0;
	while (value>0)
	{
		value = value>>1;
		class++;
	}
	return class;
}

void calc_dc_freq(int num_pixel, int16_t dct_quant[], int freq[])
{
	int i;
	for (i=0; i<num_pixel; i+=64) freq[huff_class(dct_quant[i])]++;
}

void calc_ac_freq(int num_pixel, int16_t dct_quant[], int freq[])
{
	int i;
	int num_zeros = 0;
	int last_nonzero = 0;
	for (i=0; i<num_pixel; i++)
	{
		if (i%64 == 0)
		{
			for (last_nonzero = i+63; last_nonzero>i; last_nonzero--)
				if (dct_quant[last_nonzero] != 0)
					break;
			continue;
		}

		if (i == last_nonzero + 1)
		{
			freq[0x00]++; // EOB byte
			// jump to the next block
			i = (i/64+1)*64-1;
			continue;
		}
		
		if (dct_quant[i] == 0)
		{
			num_zeros++;
			if (num_zeros == 16)
			{
				freq[0xF0]++; // ZRL byte
				num_zeros = 0;
			}
			continue;
		}

		freq[ ((num_zeros<<4)&0xF0) | (huff_class(dct_quant[i])&0x0F) ]++;
		num_zeros = 0;
	}
}

void init_huffman(int16_t * Y, int16_t * Cb, int16_t * Cr, area_t dims, huff_code Luma[2], huff_code Chroma[2]) {
	int i;

	huff_code* luma_dc =   Luma;
	huff_code* luma_ac =   Luma+1;
	huff_code* chroma_dc = Chroma;
	huff_code* chroma_ac = Chroma+1;

	// initialize
	for (i=0; i<256; i++)
		luma_dc->sym_freq[i] = luma_ac->sym_freq[i] = chroma_dc->sym_freq[i] = chroma_ac->sym_freq[i] = 0;
	// reserve one code point
	luma_dc->sym_freq[256] = luma_ac->sym_freq[256] = chroma_dc->sym_freq[256] = chroma_ac->sym_freq[256] = 1;

	// calculate frequencies as basis for the huffman table construction
	calc_dc_freq(dims.w*dims.h,   Y, luma_dc->sym_freq);
	calc_ac_freq(dims.w*dims.h,   Y, luma_ac->sym_freq);
	calc_dc_freq(dims.w*dims.h/4, Cb, chroma_dc->sym_freq);
	calc_ac_freq(dims.w*dims.h/4, Cb, chroma_ac->sym_freq);
	calc_dc_freq(dims.w*dims.h/4, Cr, chroma_dc->sym_freq);
	calc_ac_freq(dims.w*dims.h/4, Cr, chroma_ac->sym_freq);

	init_huff_table(luma_dc);
	init_huff_table(luma_ac);
	init_huff_table(chroma_dc);
	init_huff_table(chroma_ac);
}

unsigned char byte_buffer;
int bits_written;
size_t write_byte(uint8_t * jpg, int code_word, int start, int end, size_t initial) {
  size_t size = 0;
	if (start == end) return size;

	if (end>0) { // we just write into the buffer
		code_word <<= end;
		code_word &= (1<<start)-1;
		byte_buffer |= code_word;
		bits_written += start-end;
	} else { // we have to split & write to the disk
		int part2 = code_word & ((1<<(-end))-1);
		code_word >>= (-end);
		code_word &= (1<<start)-1;
		byte_buffer |= code_word;
    jpg[initial+(size++)] = byte_buffer;
		// fputc(byte_buffer, f);
		if (byte_buffer == 0xFF) {
    jpg[initial+(size++)] = 0;
      // fputc(0, f); // stuffing bit
    }
		bits_written = 0;
		byte_buffer = 0;
		size += write_byte(jpg, part2, 8, 8+end, size);
		return size;
	}
  return size;
}

size_t write_bits(uint8_t * jpg, int code_word, int code_len, size_t initial) {
	if (code_len == 0)
		return 0;
	return write_byte(jpg, code_word, 8-bits_written, 8-bits_written-code_len, initial);
}

void fill_last_byte(uint8_t * jpg, size_t initial) {
	byte_buffer |= (1<<(8-bits_written))-1;
	jpg[initial] = byte_buffer;
	byte_buffer = 0;
	bits_written = 0;
}

size_t encode_dc_value(uint8_t * jpg, int dc_val, huff_code* huff, size_t initial) {
  size_t size = 0;
	int class = huff_class(dc_val);
	int class_code = huff->sym_code[class];
	int class_size = huff->sym_code_len[class];
	size += write_bits(jpg, class_code, class_size, initial+size);

	unsigned int id = dc_val < 0 ? -dc_val : dc_val;
	if (dc_val < 0)
		id = ~id;
	size += write_bits(jpg, id, class, initial+size);
  return size;
}

size_t encode_ac_value(uint8_t * jpg, int ac_val, int num_zeros, huff_code* huff, size_t initial) {
  size_t size = 0;
	int class = huff_class(ac_val);
	int v = ((num_zeros<<4)&0xF0) | (class&0x0F);
	int code = huff->sym_code[v];
	int s = huff->sym_code_len[v];
	size += write_bits(jpg, code, s, initial+size);

	unsigned int id = ac_val > 0 ? ac_val : -ac_val;
	if (ac_val < 0) id = ~id;
	size += write_bits(jpg, id, s, initial+size);
  return size;
}

size_t write_coefficients(uint8_t * jpg, int num_pixel, int16_t dct_quant[], huff_code* huff_dc, huff_code* huff_ac, size_t initial) {
  size_t size = 0;
	int num_zeros = 0;
	int last_nonzero = 0;
	int i;
	for (i=0; i < num_pixel; i++) {
		if (i%64 == 0) {
			size += encode_dc_value(jpg, dct_quant[i], huff_dc, initial+size);
			for (last_nonzero = i+63; last_nonzero>i; last_nonzero--)
				if (dct_quant[last_nonzero] != 0) break;
			continue;
		}
	
		if (i == last_nonzero + 1) {
			size += write_bits(jpg, huff_ac->sym_code[0x00], huff_ac->sym_code_len[0x00], initial+size); // EOB symbol
			// jump to the next block
			i = (i/64+1)*64-1;
			continue;
		}

		if (dct_quant[i] == 0) {
			num_zeros++;
			if (num_zeros == 16) {
				size += write_bits(jpg, huff_ac->sym_code[0xF0], huff_ac->sym_code_len[0xF0], initial+size); // ZRL symbol
				num_zeros = 0;
			}
			continue;
		}

		size += encode_ac_value(jpg, dct_quant[i], num_zeros, huff_ac, initial+size);
		num_zeros = 0;
	}
  return size;
}

size_t write_dht_header(uint8_t * jpg, int code_len_freq[], int sym_sorted[], int tc_th, size_t initial) {
  size_t size = 0;
	int length = 19;
	int i;
	for (i=1; i<=16; i++) length += code_len_freq[i];

  jpg[initial+(size++)] = 0xFF;
  jpg[initial+(size++)] = 0xC4;
  jpg[initial+(size++)] = (length>>8)&0xFF;
  jpg[initial+(size++)] = length&0xFF;
  jpg[initial+(size++)] = (uint8_t)tc_th;

	// fputc(0xFF, f); fputc(0xC4, f); // DHT Symbol

	// fputc( (length>>8)&0xFF , f); fputc( length&0xFF, f); // len

	// fputc(tc_th, f); // table class (0=DC, 1=AC) and table id (0=luma, 1=chroma)

	for (i = 0; i < 16; i++) jpg[initial+size+i] = code_len_freq[i+1];
  size += i;
		// 	for (i = 1; i <= 16; i++) fputc(code_len_freq[i], f); // number of codes of length i

	for (i = 0; length>19; i++, length--) jpg[initial+size+i] = sym_sorted[i]; // huffval, needed to reconstruct the huffman code at the receiver
  size += i;
	// for (i=0; length>19; i++, length--) fputc(sym_sorted[i], f); // huffval, needed to reconstruct the huffman code at the receiver
  return size;
}

// Working on this

// void write_file(char * filename, int16_t in[3][PIX_LEN], area_t dims, huff_code Luma[2], huff_code Chroma[2]) {
// 	FILE* f = fopen(filename, "w");
// 	fputc(0xFF, f); fputc(0xD8, f); // SOI Symbol

// 	fputc(0xFF, f);	fputc(0xE0, f); // APP0 Tag
// 		fputc(0, f); fputc(16, f); // len
// 		fputc(0x4A, f); fputc(0x46, f); fputc(0x49, f); fputc(0x46, f); fputc(0x00, f); // JFIF ID
// 		fputc(0x01, f); fputc(0x01, f); // JFIF Version
// 		fputc(0x00, f); // units
// 		fputc(0x00, f); fputc(0x48, f); // X density
// 		fputc(0x00, f); fputc(0x48, f); // Y density
// 		fputc(0x00, f); // x thumbnail
// 		fputc(0x00, f); // y thumbnail

// 	fputc(0xFF, f); fputc(0xDB, f); // DQT Symbol
// 		fputc(0, f); fputc(67, f); // len
// 		fputc(0, f); // quant-table id
// 		int i;
// 		for (i=0; i<64; i++)
// 			fputc(luma_quantizer[scan_order[i]], f);

// 	fputc(0xFF, f); fputc(0xDB, f); // DQT Symbol
// 		fputc(0, f); fputc(67, f); // len
// 		fputc(1, f); // quant-table id
// 		for (i=0; i<64; i++)
// 			fputc(chroma_quantizer[scan_order[i]], f);

// 	write_dht_header(f, Luma[0].code_len_freq,   Luma[0].sym_sorted, 0x00);
// 	write_dht_header(f, Luma[1].code_len_freq,   Luma[1].sym_sorted, 0x10);
// 	write_dht_header(f, Chroma[0].code_len_freq, Chroma[0].sym_sorted, 0x01);
// 	write_dht_header(f, Chroma[1].code_len_freq, Chroma[1].sym_sorted, 0x11);

// 	fputc(0xFF, f); fputc(0xC0, f); // SOF0 Symbol (Baseline DCT)
// 		fputc(0, f); fputc(17, f); // len
// 		fputc(0x08, f); // data precision - 8bit
// 		fputc(((dims.h)>>8)&0xFF, f); fputc((dims.h)&0xFF, f); // picture height
// 		fputc(((dims.w)>>8)&0xFF, f); fputc((dims.w)&0xFF, f); // picture width
// 		fputc(0x03, f); // num components - 3 for y, cb and cr
// //		fputc(0x01, f); // num components - 3 for y, cb and cr
// 		fputc(1, f); // #1 id
// 		fputc(0x22, f); // sampling factor (bit0-3=vertical, bit4-7=horiz)
// 		fputc(0, f); // quantization table index
// 		fputc(2, f); // #2 id
// 		fputc(0x11, f); // sampling factor (bit0-3=vertical, bit4-7=horiz)
// 		fputc(1, f); // quantization table index
// 		fputc(3, f); // #3 id
// 		fputc(0x11, f); // sampling factor (bit0-3=vertical, bit4-7=horiz)
// 		fputc(1, f); // quantization table index

// 	fputc(0xFF, f); fputc(0xDA, f); // SOS Symbol
// 		fputc(0, f); fputc(8, f); // len
// 		fputc(1, f); // number of components
// 		fputc(1, f); // id of component
// 		fputc(0x00, f); // table index, bit0-3=AC-table, bit4-7=DC-table
// 		fputc(0x00, f); // start of spectral or predictor selection - not used
// 		fputc(0x3F, f); // end of spectral selection - default value
// 		fputc(0x00, f); // successive approximation bits - default value
// 		write_coefficients(f, dims.w*dims.h, in[0], &Luma[0], &Luma[1]);
// 		fill_last_byte(f);

// 	fputc(0xFF, f); fputc(0xDA, f); // SOS Symbol
// 		fputc(0, f); fputc(8, f); // len
// 		fputc(1, f); // number of components
// 		fputc(2, f); // id of component
// 		fputc(0x11, f); // table index, bit0-3=AC-table, bit4-7=DC-table
// 		fputc(0x00, f); // start of spectral or predictor selection - not used
// 		fputc(0x3F, f); // end of spectral selection - default value
// 		fputc(0x00, f); // successive approximation bits - default value
// 		write_coefficients(f, dims.w*dims.h/4, in[1], &Chroma[0], &Chroma[1]);
// 		fill_last_byte(f);

// 	fputc(0xFF, f); fputc(0xDA, f); // SOS Symbol
// 		fputc(0, f); fputc(8, f); // len
// 		fputc(1, f); // number of components
// 		fputc(3, f); // id of component
// 		fputc(0x11, f); // table index, bit0-3=AC-table, bit4-7=DC-table
// 		fputc(0x00, f); // start of spectral or predictor selection - not used
// 		fputc(0x3F, f); // end of spectral selection - default value
// 		fputc(0x00, f); // successive approximation bits - default value
// 		write_coefficients(f, dims.w*dims.h/4, in[2], &Chroma[0], &Chroma[1]);
// 		fill_last_byte(f);

// 	fputc(0xFF, f); fputc(0xD9, f); // EOI Symbol

// 	fclose(f);
// }

int head[] = { 0xFF, 0xD8, 0xFF, 0xE0, 0x00, 0x10, 0x4A, 0x46, 0x49, 0x46, 0x00, 0x01, 0x01, 0x00, 0x00, 0x48, 0x00, 0x48, 0x00, 0x00 };
int dqt_sym[] = { 0xFF, 0xDB, 0x00, 0x43, 0x00 };
int mid[] = { 0xFF, 0xC0, 0x00, 0x11, 0x08, 0x03, 0x01, 0x22, 0x00, 0x02, 0x11, 0x01, 0x03, 0x11, 0x01 };
int coef_info[] = { 0xFF, 0xDA, 0x00, 0x08, 0x01, 0x01, 0x00, 0x00, 0x3F, 0x00 };
int eoi[] = { 0xFF, 0xD9 };

size_t write_jpg(uint8_t * jpg, int16_t * Y, int16_t * Cb, int16_t * Cr, area_t dims, huff_code Luma[2], huff_code Chroma[2]) {
  size_t size = 0;
  int i;

	for (i=0; i<20; i++) jpg[size+i] = head[i];
  size += i;
	for (i=0; i<5; i++) jpg[size+i] = dqt_sym[i];
  size += i;
	for (i=0; i<64; i++) jpg[size+i] = luma_quantizer[scan_order[i]];
  dqt_sym[4] += 1;
  size += i;
	for (i=0; i<5; i++) jpg[size+i] = dqt_sym[i];
  size += i;
  dqt_sym[4] -= 1;
	for (i=0; i<64; i++) jpg[size+i] = chroma_quantizer[scan_order[i]];
  size += i;

	size += write_dht_header(jpg, Luma[0].code_len_freq,   Luma[0].sym_sorted, 0x00, size); // da sistemare
	size += write_dht_header(jpg, Luma[1].code_len_freq,   Luma[1].sym_sorted, 0x10, size);
	size += write_dht_header(jpg, Chroma[0].code_len_freq, Chroma[0].sym_sorted, 0x01, size);
	size += write_dht_header(jpg, Chroma[1].code_len_freq, Chroma[1].sym_sorted, 0x11, size);

	for (i=0; i<5; i++) jpg[size+i] = mid[i];
  jpg[(size++)+i] = ((dims.h)>>8)&0xFF;
  jpg[(size++)+i] = (dims.h)&0xFF;
  jpg[(size++)+i] = ((dims.w)>>8)&0xFF;
  jpg[(size++)+i] = (dims.w)&0xFF;
  for (; i < 15; i++) jpg[size+i] = mid[i];
  size += i;
	for (i = 0; i < 10; i++) jpg[size+i] = coef_info[i];
  size += i;
  coef_info[5]++;
  coef_info[6] = 0x11;

	size += write_coefficients(jpg, dims.w*dims.h, Y, &Luma[0], &Luma[1], size); // da sistemare
  fill_last_byte(jpg, size); // da sistemare             
	size += 1;

	for (i = 0; i < 10; i++) jpg[size+i] = coef_info[i];
  size += i;
  coef_info[5]++;

  size += write_coefficients(jpg, dims.w*dims.h/4, Cb, &Chroma[0], &Chroma[1], size);
  fill_last_byte(jpg, size); // da sistemare                       
	size += 1;                                                       
                                                                   
	for (i = 0; i < 10; i++) jpg[size+i] = coef_info[i];             
  size += i;                                                       
  coef_info[5] = 0x01;                                             
  coef_info[6] = 0x00;                                             
                                                                   
  size += write_coefficients(jpg, dims.w*dims.h/4, Cr, &Chroma[0], &Chroma[1], size);
  fill_last_byte(jpg, size); // da sistemare
	size += 1;

  for (i = 0; i < 2; i++) jpg[size+i] = eoi[i];
  size += i;

  return size;
}

int writePpm(FILE * f, uint8_t *sub) {
  fprintf(f, "P6\n%i %i\n255\n", WIDTH, HEIGHT);
  printf("max num = %i\n\n", PIX_LEN);
  for (int i = 0; i < (PIX_LEN); i++){
    int w = i%(WIDTH);
    int h = i/(WIDTH);
    putc(sub[3*((h/4)*WIDTH/4+w/4)+0], f);
    putc(sub[3*((h/4)*WIDTH/4+w/4)+1], f);
    putc(sub[3*((h/4)*WIDTH/4+w/4)+2], f);
  }
  fclose(f);
  return 0;
}

// int writeDiffPpm(char * filename, uint8_t sub[3][PIX_LEN], area_t * dims) {
//   FILE * f = fopen(filename, "w");
//   int off = WIDTH * dims->y + dims->x;
//   fprintf(f, "P6\n%i %i\n255\n", dims->w, dims->h);
//   printf("maxnum = %i\n\n", dims->w*dims->h);
//   for (int i = 0; i < (dims->h * dims->w); i++){
//     printf("\033[1A%i\n",i);
//     int w = i%(dims->w);
//     int h = i/(dims->w);
//     putc(sub[0][off+h*WIDTH+w], f);
//     putc(sub[1][off+h*WIDTH+w], f);
//     putc(sub[2][off+h*WIDTH+w], f);
//   }
//   fclose(f);
//   return 0;
// }

// size_t encodeNsend(uint8_t *jpg, uint8_t *raw, area_t dims) {
//   printf("inside encode");
//   int16_t ordered_dct_Y[PIX_LEN];
//   int16_t ordered_dct_CbCr[2][PIX_LEN/4];
//   huff_code Luma[2];
//   huff_code Chroma[2];
//   int size = 0;
//   rgb_to_dct(raw, ordered_dct_Y, ordered_dct_CbCr, dims);
// 	// init_huffman(ordered_dct_Y, ordered_dct_CbCr, dims, Luma, Chroma);
// 	// size_t size = write_jpg(jpg, ordered_dct_Y, ordered_dct_CbCr, dims, Luma, Chroma);
//   printf("encodeNsend done!\n");
//   return size;
// }
