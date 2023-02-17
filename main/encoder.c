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

double_t getDouble(const int64_t *bitval) {                                              // convert the double_t value, saved as int64_t in order
    return *((double_t*)(bitval));                                                       // to maintain the preciseness, into an actual double_t
}

/* zigzag_block(int16_t in[64], int16_t * out)
 *  function that reorders a block based on the scan_order array
 *  with the purpose of grouping the non 0 values at the beginning of the array 
 */
void zigzag_block(int16_t in[64], int16_t * out) {
	int i = 0;
	for (; i < 64; i++) {
    out[i] = in[scan_order[i]];
  }
}

/* dct_block(int gap, uint8_t in[], int16_t * out, const int quantizer[])
 *  function that applies the discrete cosine transform to the image blocks
 *  (8x8 blocks as specified from the JPG specification) 
 */
void dct_block(int gap, uint8_t in[], int16_t * out, const int quantizer[]) {
	int x_f, y_f; // frequency domain coordinates
	int x_t, y_t; // space domain coordinates

	double inner_lookup[64];            // an internal lookup where the first partial transform can be saved
	int16_t dct[64];                    // another internal lookup where the final dct values can be saved before the zigzag function
	for (x_t=0; x_t<8; x_t++)           // loops on the various columns
		for (y_f=0; y_f<8; y_f++)         // loops on the various frequencies on each column
		{
			inner_lookup[x_t*8 + y_f] = 0;  //creates the value of each partial transform
			for (y_t=0; y_t<8; y_t++)
				inner_lookup[x_t*8 + y_f] += ( in[y_t*gap+x_t] - 128 ) * getDouble(lookup_table + (y_t*8 + y_f)); // cos_lookup[y_t][y_f];

		}

	// freq(x_f,y_f) = ...
	double freq;
	for (y_f=0; y_f<8; y_f++)                 // loops on the various columns
    for (x_f=0; x_f<8; x_f++) {             // loops on the various frequencies on each column
			freq = 0;                                                                                
			for(x_t=0; x_t<8; x_t++)              //creates the value of each partial transform
				freq += inner_lookup[x_t*8 + y_f] * getDouble(lookup_table + (x_t*8 + x_f)); //cos_lookup[x_t][x_f];

			if (x_f == 0) freq *= M_SQRT1_2;      //  multiplies the values with fx = 0 by 1/((2)^(1/2))
			if (y_f == 0) freq *= M_SQRT1_2;      //  multiplies the values with fy = 0 by 1/((2)^(1/2))
			freq /= 4;

		  dct[y_f*8+x_f] = freq / quantizer[y_f*8+x_f%64];      // applies the quantization to the dct
		  dct[y_f*8+x_f] = CLIP(dct[y_f*8+x_f], -2048, 2047);
		}
  zigzag_block(dct, out);                   // applies the reordering funcion
}

/*  rgb_to_dct_block(uint8_t *in, int16_t *Y, int16_t *Cb, int16_t *Cr,int offx, int offy, int dimw) 
 *    in = array of rgb , [Y,Cb,Cr] recipients for the 3 output channels
 *    [offx,offy] offset in the two axes from the beginning of in (in number of 16x16 blocks done) 
 *  
 *    function that converts a block of rgb in a block of dct reorderd to group the 0s togeder 
 */

void rgb_to_dct_block(uint8_t *in, int16_t *Y, int16_t *Cb, int16_t *Cr,int offx, int offy, area_t dims) {
  // printf("offx = %i, offy = %i, dimmw = %i\n", offx, offy, dimw);
  int i = 0;
  uint8_t app[2][32];                                               //2 channels (Cb, Cr) of 2 lines of the 16x16 block of pixels
  uint32_t offCbCr = (offy*dims.w/16 + offx)*64;                    
  uint8_t midY[256];                                                // Luminance not subsampled
  uint8_t midCbCr[2][64];                                           // sublsampled Cb[0] and Cr[1]                                                                                         
  int begin, j;                                                     
  for (i = 0; i < 256; i++) {                                       // loop on the block (16*16 = 256)
  int r = i%16;                                                     // value that rappresents the index of the pixel in the line (which column of the block)
  int l = i/16;                                                     // value that rappresents the line we are currently in (which row of the block)
  int index = 3*((offy*16+dims.y+l)*WIDTH+offx*16+dims.x+r);        // index of red component of pixel in input array
	midY[i]            =       0.299    * in[index+2] + 0.587    * in[index+1] + 0.114    * in[index]; // Rgb -> Y
	app[0][(l%2)*16+r] = 128 - 0.168736 * in[index+2] - 0.331264 * in[index+1] + 0.5      * in[index]; // Rgb -> Cb
	app[1][(l%2)*16+r] = 128 + 0.5      * in[index+2] - 0.418688 * in[index+1] - 0.081312 * in[index]; // Rgb -> Cr
    if (r%2 == 1 && l%2 == 1) {
      midCbCr[0][(l/2)*8+r/2] = (app[0][r-1] + app[0][r] + app[0][r+15] + app[0][r+16])/4; // sublsampling Cb
      midCbCr[1][(l/2)*8+r/2] = (app[1][r-1] + app[1][r] + app[1][r+15] + app[1][r+16])/4; // sublsampling Cr
    }
    if (r%8 == 7 && l%8 == 7) {                      // checks when it is at the end of a dct_block (8x8)
      begin = i - 119;                               // goes to the beginning of the block (subtracts 119 = 7*(16+1) meaning it goes back of 7 lines (7*16) and 7 blocks (7*1))
      j = ((begin%16) + (begin/16)*2)/8;             // index of the 8x8 block now worked on in the bigger 16x16 block
      int ih = j%2;                                  // x = the block (0 or 1)
      int iv = j/2;                                  // y = the block (0 or 1) 
      dct_block(16, midY + (iv)*128 + ih*8, Y + ((offy*2+iv)*dims.w/8+offx*2+ih)*64, luma_quantizer); // executes the dct of one of the 8x8 blocks of Y
    }
  }
  dct_block(8, midCbCr[0], Cb + offCbCr, chroma_quantizer); // executes the dct of the 8x8 block of Cb (only one because of subsampling)
  dct_block(8, midCbCr[1], Cr + offCbCr, chroma_quantizer); // executes the dct of the 8x8 block of Cr (only one because of subsampling)
}

/* rgb_to_dct(uint8_t *in, int16_t *Y, int16_t *Cb, int16_t *Cr, area_t dims)
 *  applies the rgb_to_dct_block to all the 16x16 blocks of the image in the area defined by dims
 */
void rgb_to_dct(uint8_t *in, int16_t *Y, int16_t *Cb, int16_t *Cr, area_t dims) {
  int i;
  uint16_t offx = 0;                        // offset in (16x16 blocks) in the x axis
  uint16_t offy = 0;                        // offset in (16x16 blocks) in the y axis
  int last[3] = {0, 0, 0};                  // value of the last dc frequency, used for the differentiation
  for (i = 0; i < dims.w*dims.h/256; i++) { // loops on the blocks of the image
    offx = i%(dims.w/16);
    offy = i/(dims.w/16);
    rgb_to_dct_block(in, Y, Cb, Cr, offx, offy, dims);
  }
  for (i = 0; i < dims.w*dims.h/64; i++) {  // loops on each dc frequency and applyes
    Y[i*64] -= last[0];
    last[0] += Y[i*64];
    if(i < dims.w*dims.h/256){
      Cb[i*64] -= last[1]; 
      last[1] += Cb[i*64]; 
      Cr[i*64] -= last[2]; 
      last[2] += Cr[i*64]; 
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

	// initialize
	for (i=0; i<256; i++)
		Luma[0].sym_freq[i] = Luma[1].sym_freq[i] = Chroma[0].sym_freq[i] = Chroma[1].sym_freq[i] = 0;
	// reserve one code point
	Luma[0].sym_freq[256] = Luma[1].sym_freq[256] = Chroma[0].sym_freq[256] = Chroma[1].sym_freq[256] = 1;

	// calculate frequencies as basis for the huffman table construction
	calc_dc_freq(dims.w*dims.h,   Y, Luma[0].sym_freq);
	calc_ac_freq(dims.w*dims.h,   Y, Luma[1].sym_freq);
	calc_dc_freq(dims.w*dims.h/4, Cb, Chroma[0].sym_freq);
	calc_ac_freq(dims.w*dims.h/4, Cb, Chroma[1].sym_freq);
	calc_dc_freq(dims.w*dims.h/4, Cr, Chroma[0].sym_freq);
	calc_ac_freq(dims.w*dims.h/4, Cr, Chroma[1].sym_freq);

	init_huff_table(&Luma[0]);
	init_huff_table(&Luma[1]);
	init_huff_table(&Chroma[0]);
	init_huff_table(&Chroma[1]);
}

unsigned char byte_buffer;
int bits_written;
size_t write_byte(FILE* f, uint8_t * jpg, int code_word, int start, int end, int initial) {
  size_t size = 0;
	if (start == end)
		return size;

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
    jpg[initial+(size++)] = byte_buffer;
		if (byte_buffer == 0xFF) {
			fputc(0, f); // stuffing bit
      jpg[initial+(size++)] = 0;
    }
		bits_written = 0;
		byte_buffer = 0;
		size += write_byte(f, jpg, part2, 8, 8+end, initial+size);
		return size;
	}
  return size;
}


size_t write_bits(FILE* f, uint8_t * jpg, int code_word, int code_len, int start)
{
	if (code_len == 0)
		return 0;
	return write_byte(f, jpg, code_word, 8-bits_written, 8-bits_written-code_len, start);
}

void fill_last_byte(FILE* f, uint8_t * jpg, int index)
{
	byte_buffer |= (1<<(8-bits_written))-1;
	fputc(byte_buffer, f);
  jpg[index] = byte_buffer;
	byte_buffer = 0;
	bits_written = 0;
}

size_t encode_dc_value(FILE* f, uint8_t * jpg, int dc_val, huff_code* huff, int start)
{
  size_t size = 0;
	int class = huff_class(dc_val);
	int class_code = huff->sym_code[class];
	int class_size = huff->sym_code_len[class];
	size += write_bits(f, jpg, class_code, class_size, start+size);

	unsigned int id = dc_val < 0 ? -dc_val : dc_val;
	if (dc_val < 0) id = ~id;
	size += write_bits(f, jpg, id, class, start+size);
  return size;
}

size_t encode_ac_value(FILE* f, uint8_t* jpg, int ac_val, int num_zeros, huff_code* huff,int start) {
  size_t size = 0;
	int class = huff_class(ac_val);
	int v = ((num_zeros<<4)&0xF0) | (class&0x0F);
	int code = huff->sym_code[v];
	int s = huff->sym_code_len[v];
	size += write_bits(f, jpg, code, s, start+size);

	unsigned int id =  ac_val < 0 ? -ac_val : ac_val;
	if (ac_val < 0) id = ~id;
	size += write_bits(f, jpg, id, class, start+size);
  return size;
}

size_t write_coefficients(FILE* f, uint8_t * jpg, int num_pixel, int16_t dct_quant[], huff_code* huff_dc, huff_code* huff_ac, int start)
{
  size_t size = 0;
	int num_zeros = 0;
	int last_nonzero = 0;
	int i;
	for (i=0; i<num_pixel; i++)
	{
		if (i%64 == 0)
		{
			size += encode_dc_value(f, jpg, dct_quant[i], huff_dc, start+size);
			for (last_nonzero = i+63; last_nonzero>i; last_nonzero--)
				if (dct_quant[last_nonzero] != 0)
					break;
			continue;
		}
	
		if (i == last_nonzero + 1)
		{
			size += write_bits(f, jpg, huff_ac->sym_code[0x00], huff_ac->sym_code_len[0x00], start+size); // EOB symbol
			// jump to the next block
			i = (i/64+1)*64-1;
			continue;
		}

		if (dct_quant[i] == 0)
		{
			num_zeros++;
			if (num_zeros == 16)
			{
				size += write_bits(f, jpg, huff_ac->sym_code[0xF0], huff_ac->sym_code_len[0xF0], start+size); // ZRL symbol
				num_zeros = 0;
			}
			continue;
		}

		size += encode_ac_value(f, jpg, dct_quant[i], num_zeros, huff_ac, start+size);
		num_zeros = 0;
	}
  return size;
}

size_t write_dht_header(FILE* f, uint8_t * jpg, int code_len_freq[], int sym_sorted[], int tc_th, int start)
{
  size_t size = 0;
	int length = 19;
	int i;
	for (i=1; i<=16; i++) length += code_len_freq[i];

	fputc(0xFF, f); fputc(0xC4, f); // DHT Symbol
  jpg[start+(size++)] = 0xFF;
  jpg[start+(size++)] = 0xC4;

	fputc( (length>>8)&0xFF , f); fputc( length&0xFF, f); // len
  jpg[start+(size++)] = (length>>8)&0xFF;
  jpg[start+(size++)] = length&0xFF;

	fputc(tc_th, f); // table class (0=DC, 1=AC) and table id (0=luma, 1=chroma)
  jpg[start+(size++)] = tc_th;

	for (i=1; i<=16; i++){
		fputc(code_len_freq[i], f); // number of codes of length i
    jpg[start+(size++)] = code_len_freq[i];
  }

	for (i=0; length>19; i++, length--) {
		fputc(sym_sorted[i], f); // huffval, needed to reconstruct the huffman code at the receiver
    jpg[start+(size++)] = sym_sorted[i];
  }
  return size;
}

int head[] = { 0xFF, 0xD8, 0xFF, 0xE0, 0x00, 0x10, 0x4A, 0x46, 0x49, 0x46, 0x00, 0x01, 0x01, 0x00, 0x00, 0x48, 0x00, 0x48, 0x00, 0x00 };
int dqt_sym[] = { 0xFF, 0xDB, 0x00, 0x43, 0x00 };
int mid[] = { 0xFF, 0xC0, 0x00, 0x11, 0x08, 0x03, 0x01, 0x22, 0x00, 0x02, 0x11, 0x01, 0x03, 0x11, 0x01 };
int coef_info[] = { 0xFF, 0xDA, 0x00, 0x08, 0x01, 0x01, 0x00, 0x00, 0x3F, 0x00 };
int eoi[] = { 0xFF, 0xD9 };

size_t write_jpg(FILE * f, uint8_t * jpg, int16_t * Y, int16_t * Cb, int16_t * Cr, area_t dims, huff_code Luma[2], huff_code Chroma[2]) {
	size_t size = 0;
  int i;
	for (i=0; i<20; i++) {
    fputc(head[i], f);
    jpg[size+i] = head[i];
  }
  size += i;

	for (i=0; i<5; i++) {
    fputc(dqt_sym[i], f);
    jpg[size+i] = dqt_sym[i];
  }
  size += i;

	for (i=0; i<64; i++) {
    fputc(luma_quantizer[scan_order[i]], f);
    jpg[size+i] = luma_quantizer[scan_order[i]];
  }
  size += i;

  dqt_sym[4] += 1; // increases the quantization table id from 0 to 1
	for (i=0; i<5; i++) {
    fputc(dqt_sym[i], f);
    jpg[size+i] = dqt_sym[i];
  }
  size += i;
  dqt_sym[4] -= 1; // to make it ready for another encription

	for (i=0; i<64; i++) {
    fputc(chroma_quantizer[scan_order[i]], f);
    jpg[size+i] = chroma_quantizer[scan_order[i]];
  }
  size += i;

	size += write_dht_header(f, jpg, Luma[0].code_len_freq,   Luma[0].sym_sorted, 0x00, size);
	size += write_dht_header(f, jpg, Luma[1].code_len_freq,   Luma[1].sym_sorted, 0x10, size);
	size += write_dht_header(f, jpg, Chroma[0].code_len_freq, Chroma[0].sym_sorted, 0x01, size);
	size += write_dht_header(f, jpg, Chroma[1].code_len_freq, Chroma[1].sym_sorted, 0x11, size);

	for (i=0; i<5; i++) {
    fputc(mid[i], f);
    jpg[size+i] = mid[i];
  }
	fputc(((dims.h)>>8)&0xFF, f); fputc((dims.h)&0xFF, f); // picture height
	fputc(((dims.w)>>8)&0xFF, f); fputc((dims.w)&0xFF, f); // picture width
  jpg[(size++)+i] = ((dims.h)>>8)&0xFF;
  jpg[(size++)+i] = (dims.h)&0xFF;
  jpg[(size++)+i] = ((dims.w)>>8)&0xFF;
  jpg[(size++)+i] = (dims.w)&0xFF;
  for (; i < 15; i++) {
    fputc(mid[i], f);
    jpg[size+i] = mid[i];
  }
  size += i;

	for (i = 0; i < 10; i++) {
    fputc(coef_info[i], f);
    jpg[size+i] = coef_info[i];
  }
  size += i;
  coef_info[5]++;
  coef_info[6] = 0x11;
	size += write_coefficients(f, jpg, dims.w*dims.h, Y, &Luma[0], &Luma[1], size);
	fill_last_byte(f, jpg, size);
  size++;

	for (i = 0; i < 10; i++) {
    fputc(coef_info[i], f);
    jpg[size+i] = coef_info[i];
  }
  size += i;
  coef_info[5]++;
	size += write_coefficients(f, jpg, dims.w*dims.h/4, Cb, &Chroma[0], &Chroma[1], size);
	fill_last_byte(f, jpg, size);
  size++;

	for (i = 0; i < 10; i++) {
    fputc(coef_info[i], f);
    jpg[size+i] = coef_info[i];
  }
  size += i;
  coef_info[5] = 0x01;                                             
  coef_info[6] = 0x00;                                             
	size += write_coefficients(f, jpg, dims.w*dims.h/4, Cr, &Chroma[0], &Chroma[1], size);
	fill_last_byte(f, jpg, size);
  size++;

  for (i = 0; i < 2; i++) {
    fputc(eoi[i], f);
    jpg[size+i] = eoi[i];
  }
  size += i;

  return size;
}
