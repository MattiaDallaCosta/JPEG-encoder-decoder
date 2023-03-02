#include "../include/encoder.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

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

/* readPpm(FILE* f, uint8_t * raw)
 *  f = pointer to the file beeing read
 *  raw = array where the regb sequence retreived from the file is saved
 *
 *  function that reads a ppm file and stores in an array the rgb sequence contained in it 
 */
int readPpm(FILE* f, uint8_t raw[3][PIX_LEN]) {
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
	  raw[0][i] = fgetc(f);
	  raw[1][i] = fgetc(f);
	  raw[2][i] = fgetc(f);
	}
	
	return 0;
	
X:	fprintf(stderr, "Could not parse the PPM file properly\n");
	return -1;
}

/* getDouble(const int64_t *bitval)
 *  bitval = pointer to the int64_t to be converted in double
 *
 *  function that returns the double_t value stored in an int64_t variable
 *  (to maintain preciseness and avoid calculating it at startup)
 */
double_t getDouble(const int64_t *bitval) {
    return *((double_t*)(bitval));         
}

/* void getName(char *name, char *buff, int num)
 *  name = the string containing the name of the original file
 *  buff = the string where the new name is stored
 *  num = the number of the difference of which the jpg is beeing done (-1 if full image)
 *
 *  function that writes the string containing the name for the jpg file where to save the converted image 
 */
void getName(char *name, char *buff, int num) {
  char *pos;
  strcpy(buff, name);
  pos = strrchr(buff, '.') ? (strrchr(buff, '.') < strrchr(buff, '/') ? buff + strlen(buff) : strrchr(buff, '.')) : buff + strlen(buff);
  char *end;
  if(num < 0) strcpy(end, ".jpg");
  else sprintf(end, "-%i.jpg", num);
  strcpy(pos, end);
}

/* getSavedName(char *name, char *buff) 
 *  name = the string containing the name of the original file
 *  buff = the string where the new name is stored
 *  
 *  function that writes the string containing the name for the ppm where to save the subsampled image 
 */
void getSavedName(char *name, char *buff) {
  char *pos;
  strcpy(buff, name);
  pos =  strrchr(buff, '/') ? strrchr(buff, '/') : buff;
  strcpy(*pos == '/' ? pos+1 : pos,  "savedImage.ppm");
}

/* zigzag_block(int16_t in[64], int16_t * out)
 *  in = array containing the block to be reordered
 *  out = array where to put the reordered sequence
 *
 *  function that reorders a block based on the scan_order array
 *  with the purpose of grouping the non 0 values at the beginning of the array 
 */
void zigzag_block(int16_t in[64], int16_t out[64]) {
	int i = 0;
	for (; i < 64; i++) {
    out[i] = in[scan_order[i]];
  }
}

/* dct_block(int gap, uint8_t in[], int16_t * out, const int quantizer[])
 *  gap = distance from the left border of the image
 *  in = input array containing a single channel of YCbCr (the 2 crominances are subsampled)
 *  out = array where to store the reordered dct block
 *  quantizer = the array containing the quantization table for the channel to be transformed
 *
 *  function that applies the discrete cosine transform to the image blocks
 *  (8x8 blocks as specified from the JPG specification) 
 */
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

/* rgb_to_dct(uint8_t in[3][PIX_LEN], int16_t out[3][PIX_LEN], area_t dims)
 *  in = array of rgb , out = recipients for the 3 output channels
 *  dims = area of the image containing the difference 
 *  
 *  function that converts a sub-image of the raw image contained in the in array to YCbCr to which is appiled a subsampling, a dct and a quantization
 *  the sub-image dimensions are contained into dims
 */
void rgb_to_dct(uint8_t in[3][PIX_LEN], int16_t out[3][PIX_LEN], area_t dims) {
  int i = 0;
  int off = dims.y*WIDTH + dims.x;
  uint8_t app[2][2*dims.w];
  uint8_t mid[3][dims.w*dims.h];
  int last[3] = {0, 0, 0};
  int begin, j;
	for (; i < dims.w*dims.h; i++) {
    int r = i%dims.w;
    int l = i/dims.w;
		mid[0][i]              =       0.299    * in[0][i+off] + 0.587    * in[1][i+off] + 0.114    * in[2][i+off];
		app[0][(l%2)*dims.w+r] = 128 - 0.168736 * in[0][i+off] - 0.331264 * in[1][i+off] + 0.5      * in[2][i+off];
		app[1][(l%2)*dims.w+r] = 128 + 0.5      * in[0][i+off] - 0.418688 * in[1][i+off] - 0.081312 * in[2][i+off];
    if (r%2 == 1 && l%2 == 1) {
			 mid[1][(l/2*dims.w)/2+r/2] = (app[0][r-1] + app[0][r] + app[0][dims.w+r-1] + app[0][dims.w+r])/4;
			 mid[2][(l/2*dims.w)/2+r/2] = (app[1][r-1] + app[1][r] + app[1][dims.w+r-1] + app[1][dims.w+r])/4;
    if (r%8 == 7 && l%8 == 7) {
      begin = i - 7*(dims.w+1);
      j = ((begin%dims.w) + (begin/dims.w)*(dims.w/8))/8;
      int ih = j%(dims.w/8); 
      int iv = j/(dims.w/8); 
      dct_block(dims.w, mid[0] + iv*dims.w*8 + ih*8, out[0] + (iv*(dims.w/8)+ih)*64, luma_quantizer);
      out[0][(iv*(dims.w/8)+ih)*64] -= last[0];
      last[0] += out[0][(iv*(dims.w/8)+ih)*64];
      if (ih%2 == 1 && iv%2 == 1) {
        begin = begin - 8*(dims.w+1);
        j = ((begin%(dims.w)) + (begin/(dims.w))*(dims.w/8))/8;
        ih = j%(dims.w/8); 
        iv = j/(dims.w/8); 
        dct_block(dims.w/2, mid[1] + (iv*dims.w + ih*2)*2, out[1] + (iv*dims.w/16+ih)*32, chroma_quantizer);
        dct_block(dims.w/2, mid[2] + (iv*dims.w + ih*2)*2, out[2] + (iv*dims.w/16+ih)*32, chroma_quantizer);
        out[1][(iv*dims.w/16+ih)*32] -= last[1];
        last[1] += out[1][(iv*dims.w/16+ih)*32];
        out[2][(iv*dims.w/16+ih)*32] -= last[2];
        last[2] += out[2][(iv*dims.w/16+ih)*32];
      }
    }
    }
    if (r == (dims.w - 1)){
      off += (WIDTH - dims.w);
    }
	}
}

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
	for (i=0; i<num_pixel; i+=64)
		freq[huff_class(dct_quant[i])]++;
}

void calc_ac_freq(int num_pixel, int16_t dct_quant[], int freq[])
{
	int i;
	int num_zeros = 0;
	int last_nonzero;
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

void init_huffman(int16_t processed[3][PIX_LEN], area_t dims, huff_code Luma[2], huff_code Chroma[2]) {
	int i;

	huff_code* luma_dc =   &Luma[0];
	huff_code* luma_ac =   &Luma[1];
	huff_code* chroma_dc = &Chroma[0];
	huff_code* chroma_ac = &Chroma[1];

	// initialize
	for (i=0; i<256; i++)
		luma_dc->sym_freq[i] = luma_ac->sym_freq[i] = chroma_dc->sym_freq[i] = chroma_ac->sym_freq[i] = 0;
	// reserve one code point
	luma_dc->sym_freq[256] = luma_ac->sym_freq[256] = chroma_dc->sym_freq[256] = chroma_ac->sym_freq[256] = 1;

	// calculate frequencies as basis for the huffman table construction
	calc_dc_freq(dims.w*dims.h,   processed[0], luma_dc->sym_freq);
	calc_ac_freq(dims.w*dims.h,   processed[0], luma_ac->sym_freq);
	calc_dc_freq(dims.w*dims.h/4, processed[1], chroma_dc->sym_freq);
	calc_dc_freq(dims.w*dims.h/4, processed[2], chroma_dc->sym_freq);
	calc_ac_freq(dims.w*dims.h/4, processed[1], chroma_ac->sym_freq);
	calc_ac_freq(dims.w*dims.h/4, processed[2], chroma_ac->sym_freq);

	init_huff_table(luma_dc);
	init_huff_table(luma_ac);
	init_huff_table(chroma_dc);
	init_huff_table(chroma_ac);
}

unsigned char byte_buffer;
int bits_written;
void write_byte(FILE* f, int code_word, int start, int end)
{
	if (start == end)
		return;

	if (end>0) // we just write into the buffer
	{
		code_word <<= end;
		code_word &= (1<<start)-1;
		byte_buffer |= code_word;
		bits_written += start-end;
	}
	else // we have to split & write to the disk
	{
		int part2 = code_word & ((1<<(-end))-1);
		code_word >>= (-end);
		code_word &= (1<<start)-1;
		byte_buffer |= code_word;
		fputc(byte_buffer, f);
		if (byte_buffer == 0xFF)
			fputc(0, f); // stuffing bit
		bits_written = 0;
		byte_buffer = 0;
		write_byte(f, part2, 8, 8+end);
		return;
	}
}

void write_bits(FILE* f, int code_word, int code_len)
{
	if (code_len == 0)
		return;
	write_byte(f, code_word, 8-bits_written, 8-bits_written-code_len);
}

void fill_last_byte(FILE* f)
{
	byte_buffer |= (1<<(8-bits_written))-1;
	fputc(byte_buffer, f);
	byte_buffer = 0;
	bits_written = 0;
}

void encode_dc_value(FILE* f, int dc_val, huff_code* huff)
{
	int class = huff_class(dc_val);
	int class_code = huff->sym_code[class];
	int class_size = huff->sym_code_len[class];
	write_bits(f, class_code, class_size);

	unsigned int id = dc_val < 0 ? -dc_val : dc_val;
	if (dc_val < 0)
		id = ~id;
	write_bits(f, id, class);
}

void encode_ac_value(FILE* f, int ac_val, int num_zeros, huff_code* huff)
{
	int class = huff_class(ac_val);
	int v = ((num_zeros<<4)&0xF0) | (class&0x0F);
	int code = huff->sym_code[v];
	int size = huff->sym_code_len[v];
	write_bits(f, code, size);

	unsigned int id = abs(ac_val);
	if (ac_val < 0)
		id = ~id;
	write_bits(f, id, class);
}

void write_coefficients(FILE* f, int num_pixel, int16_t dct_quant[], huff_code* huff_dc, huff_code* huff_ac)
{
	int num_zeros = 0;
	int last_nonzero;
	int i;
	for (i=0; i<num_pixel; i++)
	{
		if (i%64 == 0)
		{
			encode_dc_value(f, dct_quant[i], huff_dc);
			for (last_nonzero = i+63; last_nonzero>i; last_nonzero--)
				if (dct_quant[last_nonzero] != 0)
					break;
			continue;
		}
	
		if (i == last_nonzero + 1)
		{
			write_bits(f, huff_ac->sym_code[0x00], huff_ac->sym_code_len[0x00]); // EOB symbol
			// jump to the next block
			i = (i/64+1)*64-1;
			continue;
		}

		if (dct_quant[i] == 0)
		{
			num_zeros++;
			if (num_zeros == 16)
			{
				write_bits(f, huff_ac->sym_code[0xF0], huff_ac->sym_code_len[0xF0]); // ZRL symbol
				num_zeros = 0;
			}
			continue;
		}

		encode_ac_value(f, dct_quant[i], num_zeros, huff_ac);
		num_zeros = 0;
	}
}

void write_dht_header(FILE* f, int code_len_freq[], int sym_sorted[], int tc_th)
{
	int length = 19;
	int i;
	for (i=1; i<=16; i++)
		length += code_len_freq[i];

	fputc(0xFF, f); fputc(0xC4, f); // DHT Symbol

	fputc( (length>>8)&0xFF , f); fputc( length&0xFF, f); // len

	fputc(tc_th, f); // table class (0=DC, 1=AC) and table id (0=luma, 1=chroma)

	for (i=1; i<=16; i++)
		fputc(code_len_freq[i], f); // number of codes of length i

	for (i=0; length>19; i++, length--)
		fputc(sym_sorted[i], f); // huffval, needed to reconstruct the huffman code at the receiver
}

/* write_file(char* file_name, int16_t out[3][PIX_LEN], area_t dims, huff_code Luma[2], huff_code Chroma[2])
 *  file_name = name of the file  where to write the jpg out
 *  out = recipients for the 3 output channels
 *  dims = area of the image containing the difference 
 *  [Luma, Chroma] pair of structs containing the information of the huffman code for each channel
 *
 *  function that generates and writes the jpg data into a file 
 */
void write_file(char* file_name, int16_t out[3][PIX_LEN], area_t dims, huff_code Luma[2], huff_code Chroma[2]) {
	FILE* f = fopen(file_name, "w");
	fputc(0xFF, f); fputc(0xD8, f); // SOI Symbol

	fputc(0xFF, f);	fputc(0xE0, f); // APP0 Tag
		fputc(0, f); fputc(16, f); // len
		fputc(0x4A, f); fputc(0x46, f); fputc(0x49, f); fputc(0x46, f); fputc(0x00, f); // JFIF ID
		fputc(0x01, f); fputc(0x01, f); // JFIF Version
		fputc(0x00, f); // units
		fputc(0x00, f); fputc(0x48, f); // X density
		fputc(0x00, f); fputc(0x48, f); // Y density
		fputc(0x00, f); // x thumbnail
		fputc(0x00, f); // y thumbnail

	fputc(0xFF, f); fputc(0xDB, f); // DQT Symbol
		fputc(0, f); fputc(67, f); // len
		fputc(0, f); // quant-table id
		int i;
		for (i=0; i<64; i++)
			fputc(luma_quantizer[scan_order[i]], f);

	fputc(0xFF, f); fputc(0xDB, f); // DQT Symbol
		fputc(0, f); fputc(67, f); // len
		fputc(1, f); // quant-table id
		for (i=0; i<64; i++)
			fputc(chroma_quantizer[scan_order[i]], f);

	write_dht_header(f, Luma[0].code_len_freq,   Luma[0].sym_sorted, 0x00);
	write_dht_header(f, Luma[1].code_len_freq,   Luma[1].sym_sorted, 0x10);
	write_dht_header(f, Chroma[0].code_len_freq, Chroma[0].sym_sorted, 0x01);
	write_dht_header(f, Chroma[1].code_len_freq, Chroma[1].sym_sorted, 0x11);

	fputc(0xFF, f); fputc(0xC0, f); // SOF0 Symbol (Baseline DCT)
		fputc(0, f); fputc(17, f); // len
		fputc(0x08, f); // data precision - 8bit
		fputc(((dims.h)>>8)&0xFF, f); fputc((dims.h)&0xFF, f); // picture height
		fputc(((dims.w)>>8)&0xFF, f); fputc((dims.w)&0xFF, f); // picture width
		fputc(0x03, f); // num components - 3 for y, cb and cr
//		fputc(0x01, f); // num components - 3 for y, cb and cr
		fputc(1, f); // #1 id
		fputc(0x22, f); // sampling factor (bit0-3=vertical, bit4-7=horiz)
		fputc(0, f); // quantization table index
		fputc(2, f); // #2 id
		fputc(0x11, f); // sampling factor (bit0-3=vertical, bit4-7=horiz)
		fputc(1, f); // quantization table index
		fputc(3, f); // #3 id
		fputc(0x11, f); // sampling factor (bit0-3=vertical, bit4-7=horiz)
		fputc(1, f); // quantization table index

	fputc(0xFF, f); fputc(0xDA, f); // SOS Symbol
		fputc(0, f); fputc(8, f); // len
		fputc(1, f); // number of components
		fputc(1, f); // id of component
		fputc(0x00, f); // table index, bit0-3=AC-table, bit4-7=DC-table
		fputc(0x00, f); // start of spectral or predictor selection - not used
		fputc(0x3F, f); // end of spectral selection - default value
		fputc(0x00, f); // successive approximation bits - default value
		write_coefficients(f, dims.w*dims.h, out[0], &Luma[0], &Luma[1]);
		fill_last_byte(f);

	fputc(0xFF, f); fputc(0xDA, f); // SOS Symbol
		fputc(0, f); fputc(8, f); // len
		fputc(1, f); // number of components
		fputc(2, f); // id of component
		fputc(0x11, f); // table index, bit0-3=AC-table, bit4-7=DC-table
		fputc(0x00, f); // start of spectral or predictor selection - not used
		fputc(0x3F, f); // end of spectral selection - default value
		fputc(0x00, f); // successive approximation bits - default value
		write_coefficients(f, dims.w*dims.h/4, out[1], &Chroma[0], &Chroma[1]);
		fill_last_byte(f);

	fputc(0xFF, f); fputc(0xDA, f); // SOS Symbol
		fputc(0, f); fputc(8, f); // len
		fputc(1, f); // number of components
		fputc(3, f); // id of component
		fputc(0x11, f); // table index, bit0-3=AC-table, bit4-7=DC-table
		fputc(0x00, f); // start of spectral or predictor selection - not used
		fputc(0x3F, f); // end of spectral selection - default value
		fputc(0x00, f); // successive approximation bits - default value
		write_coefficients(f, dims.w*dims.h/4, out[2], &Chroma[0], &Chroma[1]);
		fill_last_byte(f);

	fputc(0xFF, f); fputc(0xD9, f); // EOI Symbol

	fclose(f);
}

/* writePpm(FILE * f, uint8_t sub[3][PIX_LEN/16])
 *  f = pointer to the file where to write the jpg out
 *  sub = array containing the subsampled image
 *
 *  function that writes the subsampled raw image data into a file 
 */
int writePpm(FILE * f, uint8_t sub[3][PIX_LEN/16]) {
  fprintf(f, "P6\n%i %i\n255\n", WIDTH, HEIGHT);
  for (int i = 0; i < (PIX_LEN); i++){
    int w = i%(WIDTH);
    int h = i/(WIDTH);
    putc(sub[0][(h/4)*WIDTH/4+w/4], f);
    putc(sub[1][(h/4)*WIDTH/4+w/4], f);
    putc(sub[2][(h/4)*WIDTH/4+w/4], f);
  }
  fclose(f);
  return 0;
}

/* writeDiffPpm(char * filename, uint8_t sub[3][PIX_LEN], area_t * dims)
 *  file_name = name of the file where to write the raw out
 *  sub = array containing the complete image
 *  dims = area of the image containing the difference 
 *
 *  function that writes the raw image data of a sub-image into a file 
 */
int writeDiffPpm(char * filename, uint8_t sub[3][PIX_LEN], area_t * dims) {
  FILE * f = fopen(filename, "w");
  int off = WIDTH * dims->y + dims->x;
  fprintf(f, "P6\n%i %i\n255\n", dims->w, dims->h);
  for (int i = 0; i < (dims->h * dims->w); i++){
    int w = i%(dims->w);
    int h = i/(dims->w);
    putc(sub[0][off+h*WIDTH+w], f);
    putc(sub[1][off+h*WIDTH+w], f);
    putc(sub[2][off+h*WIDTH+w], f);
  }
  fclose(f);
  return 0;
}

/* encodeNsend(char * name, uint8_t raw[3][PIX_LEN], area_t dims)
 *  name = name of the file where to write the jpg out
 *  raw = array containing the raw image data
 *  dims = area of the image containing the difference 
 *
 *  function that encapsulates all the steps of
 *  the jpeg conversion in a single function call
 */
void encodeNsend(char * name, uint8_t raw[3][PIX_LEN], area_t dims) {
  int16_t ordered_dct[3][PIX_LEN];
  huff_code Luma[2];
  huff_code Chroma[2];
  rgb_to_dct(raw, ordered_dct, dims);
	init_huffman(ordered_dct, dims, Luma, Chroma);
	write_file(name, ordered_dct, dims, Luma, Chroma);
}
