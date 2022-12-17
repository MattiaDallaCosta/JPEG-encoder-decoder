#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>

#define MAX(a,b)  (((a)>(b))?(a):(b))
#define MIN(a,b)  (((a)<(b))?(a):(b))
#define CLIP(n, min, max) MIN((MAX((n),(min))), (max))

// #define SMCLK_FREQUENCY 48000000
// #define SAMPLE_FREQUENCY 8000

#define WIDTH 64
#define HEIGHT 64

typedef struct _huff_code {
    uint8_t sym_freq[257];                                                                         // frequency of occurrence of symbol i
    int8_t bits[257];                                                                              // code length of symbol i
    int16_t next[257];                                                                            // index to next symbol in chain of all symbols in current branch of code tree
    int32_t bits_freq[32];                                                                         // the frequencies of huff-symbols of length i
    uint8_t sym_sorted[256];                                                                       // the symbols to be encoded
    uint8_t sym_code_len[256];                                                                     // the huffman code length of symbol i
    uint16_t sym_code[256];                                                                        // the huffman code of the symbol i
} huff_code;

// typedef struct _huff_code {
//     int sym_freq[257];                                                                         // frequency of occurrence of symbol i
//     int bits[257];                                                                              // code length of symbol i
//     int next[257];                                                                            // index to next symbol in chain of all symbols in current branch of code tree
//     int bits_freq[32];                                                                         // the frequencies of huff-symbols of length i
//     int sym_sorted[256];                                                                       // the symbols to be encoded
//     int sym_code_len[256];                                                                     // the huffman code length of symbol i
//     int sym_code[256];                                                                        // the huffman code of the symbol i
// } huff_code;

// const eUSCI_UART_ConfigV1 uartConfig = {
//      EUSCI_A_UART_CLOCKSOURCE_SMCLK,                                                            // SMCLK Clock Source
//      78,                                                                                        // BRDIV = 78
//      2,                                                                                         // UCxBRF = 2
//      0,                                                                                         // UCxBRS = 0
//      EUSCI_A_UART_NO_PARITY,                                                                    // No ParityEUSCI_A_UART_LSB_FIRST,                                                                    // LSB First
//      EUSCI_A_UART_ONE_STOP_BIT,                                                                 // One stop bit
//      EUSCI_A_UART_MODE,                                                                         // UART mode
//      EUSCI_A_UART_OVERSAMPLING_BAUDRATE_GENERATION                                              // Oversampling
// };

uint8_t rgb[3][WIDTH*HEIGHT];
uint8_t YCbCr[3][WIDTH*HEIGHT];
uint8_t CbCr_sub[2][WIDTH*HEIGHT/4];

uint8_t Y_order[WIDTH*HEIGHT];
uint8_t CbCr_order[2][WIDTH*HEIGHT/4];

double_t Y_dct[WIDTH*HEIGHT];
double_t CbCr_dct[2][WIDTH*HEIGHT/4];

int32_t Y_quant[WIDTH*HEIGHT];
int32_t CbCr_quant[2][WIDTH*HEIGHT/4];

int32_t Y_zz[WIDTH*HEIGHT];
int32_t CbCr_zz[2][WIDTH*HEIGHT/4];

huff_code Luma_dc;
huff_code Luma_ac;
huff_code Chroma_ac;
huff_code Chroma_dc;

unsigned char byte_buffer;
int bits_written;

// Matrix for the quantization of the luma component (Y)
const uint8_t luma_quantizer[] = {
16, 11, 10, 16,  24,  40,  51,  61,
12, 12, 14, 19,  26,  58,  60,  55,
14, 13, 16, 24,  40,  57,  69,  56,
14, 17, 22, 29,  51,  87,  80,  62,
18, 22, 37, 56,  68, 109, 103,  77,
24, 35, 55, 64,  81, 104, 113,  92,
49, 64, 78, 87, 103, 121, 120, 101,
72, 92, 95, 98, 112, 100, 103,  99};

// Matrix for the quantization of the lumchroma components (Cb and Cr)
const uint8_t chroma_quantizer[] = {
17, 18, 24, 47, 99, 99, 99, 99,
18, 21, 26, 66, 99, 99, 99, 99,
24, 26, 56, 99, 99, 99, 99, 99,
47, 66, 99, 99, 99, 99, 99, 99,
99, 99, 99, 99, 99, 99, 99, 99,
99, 99, 99, 99, 99, 99, 99, 99,
99, 99, 99, 99, 99, 99, 99, 99,
99, 99, 99, 99, 99, 99, 99, 99};

// Matrix used for reordering the blocks in zigzag
const uint8_t scan_order[] = {
 0,  1,  8, 16,  9,  2,  3, 10,
17, 24, 32, 25, 18, 11,  4,  5,
12, 19, 26, 33, 40, 48, 41, 34,
27, 20, 13,  6,  7, 14, 21, 28,
35, 42, 49, 56, 57, 50, 43, 36,
29, 22, 15, 23, 30, 37, 44, 51,
58, 59, 52, 45, 38, 31, 39, 46,
53, 60, 61, 54, 47, 55, 62, 63};

const int64_t lookup_table[] = {        // contains cos((double_t)(2*(i/8)+1)*(i%8)*M_PI/16) with i = [0-63] saved as int64_t to avoid aproximation
4607182418800017408,  4607009347991985328,  4606496786581982534,  4605664432017547683,  4604544271217802189,  4603179351334086857,  4600565431771507044,  4596196889902818829,
4607182418800017408,  4605664432017547683,  4600565431771507044, -4627175146951956984, -4618827765636973620, -4616362688862790480, -4616875250272793273, -4620192685520688952,
4607182418800017408,  4603179351334086857, -4622806605083268766, -4616362688862790480, -4618827765636973618,  4596196889902818828,  4606496786581982532,  4605664432017547685,
4607182418800017408,  4596196889902818829, -4616875250272793274, -4620192685520688952,  4604544271217802187,  4605664432017547685, -4622806605083268763, -4616362688862790478,
4607182418800017408, -4627175146951956984, -4616875250272793273,  4603179351334086853,  4604544271217802190, -4617707604837228126, -4622806605083268751,  4607009347991985328,
4607182418800017408, -4620192685520688954, -4622806605083268755,  4607009347991985328, -4618827765636973627, -4627175146951956990,  4606496786581982534, -4617707604837228127,
4607182418800017408, -4617707604837228124,  4600565431771507047,  4596196889902818845, -4618827765636973623,  4607009347991985330, -4616875250272793277,  4603179351334086850,
4607182418800017408, -4616362688862790480,  4606496786581982532, -4617707604837228126,  4604544271217802180, -4620192685520688958,  4600565431771507039, -4627175146951956970};

double_t getDouble(const int64_t *bitval);
void getName(char *name, char *buff, uint8_t op);
void init_huff_code(void);
// void InitHW(void);
// void write_bits(int fd, int code_word, int code_len);

// int encode(int in, int out);
int encode(int in, char * out);
int readPpm(int fd);
void toYcbcr(void);
void Subsampling(uint8_t *in, uint8_t *out);
void toBlockOrder(int width, int height, uint8_t *in, uint8_t *out);
void ddct(uint8_t *in, int16_t *out, const uint8_t *quant, int type, int gap); // type e gap sono da togliere
void toZigZag(int16_t *in, int16_t *out);
void diffDC(int pixels, int32_t *dct_quant);
void GenHuffTable(huff_code *hc);
uint8_t huffClass(int16_t value);
void calcDCFreq(uint32_t pixels, int32_t *dct_quant, uint8_t *freq);
void calcACFreq(uint32_t pixels, int32_t *dct_quant, uint8_t *freq);
void InitHuffman(void);
int writeJpeg(int fd);

int decode(int in, int out);
int writePpm(int fd);
void toRgb(void);
void Upsampling(int8_t *in, int8_t *out);
void fromBlockOrder(int8_t *in, int8_t *out);
void idct(int8_t *in, int8_t *out, const int8_t *quant);
void fromZigZag(int16_t *in, int16_t *out);
// insert other functions in the (reverse) decoder pipeline
int readJpeg(int fd);

// int (*operations[2])(int, int) = {
//   encode,
//   decode
// };


int main (int argc, char **argv) {
  char name[1024];
  int in, out, op;
  if (argc != 3) {
    printf("expected input = %s <file> <op>\nWhere:\n\t<file> is the file to be converted\n\t<op> is the operation to be executed [0 encode, 1 decode]\n", argv[0]);
    return 1;
  }
  op = atoi(argv[2]);
  if ((in = open(argv[1], O_RDONLY)) == -1) {
    fprintf(stderr, "\033[0;31mERROR: can't open file %s\033[0m\n", argv[1]);
    return 1;
  }
  getName(argv[1], name, op);
  // if ((out = open(name , O_WRONLY | O_CREAT, 0644)) == -1) {
  //   fprintf(stderr, "\033[0;31mERROR: can't open file %s\033[0m\n", name);
  //   return 1;
  // }
  encode(in, name);

  return 0;
}


inline double_t getDouble(const int64_t *bitval) {                                              // convert the double_t value, saved as int64_t in order
    return *((double_t*)(bitval));                                                                 // to maintain the preciseness, into an actual double_t
}

void getName(char *name, char *buff, uint8_t op){
  char *pos;
  strcpy(buff, name);
  pos = strrchr(buff, '.') ? (strrchr(buff, '.') < strrchr(buff, '/') ? buff + strlen(buff) : strrchr(buff, '.')) : buff + strlen(buff);
  strcpy(pos, op ? ".ppm" : ".jpg");
}

 // -----------------------------------------------------------------------------------------------------------------

double cos_lookup[8][8];
void init_dct_lookup()
{
	int i, j;
	for (i=0; i<8; i++)
		for (j=0; j<8; j++)
		{
			cos_lookup[i][j] = cos( (2*i+1)*j*M_PI/16 );
			// assert( -1<=cos_lookup[i][j] && cos_lookup[i][j]<=1 );
		}
}

/*
 * the discrete cosine transform per 8x8 block - outputs floating point values
 * optimized by using lookup tables and splitting the terms
 */
void dct_block(int gap, uint8_t in[], double_t out[])
{
	int x_f, y_f; // frequency domain coordinates
	int x_t, y_t; // time domain coordinates
	double inner_lookup[8][8];
	for (x_t=0; x_t<8; x_t++)
		for (y_f=0; y_f<8; y_f++)
		{
			inner_lookup[x_t][y_f] = 0;
			for (y_t=0; y_t<8; y_t++) inner_lookup[x_t][y_f] += ( in[y_t*gap+x_t] - 128 ) * cos_lookup[y_t][y_f];
		}

	// freq(x_f,y_f) = ...
	double freq;
	for (y_f=0; y_f<8; y_f++)
		for (x_f=0; x_f<8; x_f++)
		{
			freq = 0;
			for(x_t=0; x_t<8; x_t++)
				freq += inner_lookup[x_t][y_f] * cos_lookup[x_t][x_f];

			if (x_f == 0)
				freq *= M_SQRT1_2;
			if (y_f == 0)
				freq *= M_SQRT1_2;
			freq /= 4;
			out[y_f*8+x_f] = freq;
		}
}

/*
 * perform the discrete cosine transform
 */
void dct(int blocks_horiz, int blocks_vert, uint8_t in[], double_t out[])
{
	int h,v;
	for (v=0; v<blocks_vert; v++)
		for (h=0; h<blocks_horiz; h++)
			dct_block(8*blocks_horiz, in + v*blocks_horiz*64 + h*8, out + (v*blocks_horiz+h)*64);
}

void quantize(int num_pixel, double in[], int32_t out[], const uint8_t quantizer[])
{
	int i;
	for (i=0; i<num_pixel; i++)
	{
		out[i] = in[i] / quantizer[i%64];
		out[i] = CLIP(out[i], -2048, 2047);
	}
}

void calc_dc_freq(int num_pixel, int32_t dct_quant[], uint8_t freq[])
{
	int i;
	for (i=0; i<num_pixel; i+=64)
		freq[huffClass(dct_quant[i])]++;
}

/*
 * determine frequencies (num_zeros + AC) - bit0-3=num_preceding_zeros, bit4-7=class-id
 */
void calc_ac_freq(int num_pixel, int32_t dct_quant[],  uint8_t freq[]) 
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

		freq[ ((num_zeros<<4)&0xF0) | (huffClass(dct_quant[i])&0x0F) ]++;
		num_zeros = 0;
	}
}

void diff_dc(int num_pixel, int dct_quant[])
{
	int i;
	int last = dct_quant[0];
	for (i=64; i<num_pixel; i+=64)
	{
		dct_quant[i] -= last;
		last += dct_quant[i];
	}
}

void init_huff_table(huff_code* hc)
{
	int i;
	for (i=0; i<257; i++)
	{
		hc->bits[i] = 0;
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
			hc->bits[v1]++;
			if (hc->next[v1] == -1)
				break;
			v1 = hc->next[v1];
		}
		hc->next[v1] = v2;
		while (1)
		{
			hc->bits[v2]++;
			if (hc->next[v2] == -1)
				break;
			v2 = hc->next[v2];
		}
	}

	for (i=0; i<32; i++)
		hc->bits_freq[i] = 0;

	// derive code length frequencies
	for (i=0; i<257; i++)
		if (hc->bits[i] != 0)
			hc->bits_freq[hc->bits[i]]++;

	// limit the huffman code length to 16 bits
	i=31;
	while (1)
	{
		if (hc->bits_freq[i] > 0) // if the code is too long ...
		{
			int j = i-1;
			while (hc->bits_freq[--j] <= 0); // ... we search for an upper layer containing leaves
			hc->bits_freq[i] -= 2; // we remove the two leaves from the lowest layer
			hc->bits_freq[i-1]++; // and put one of them one position higher
			hc->bits_freq[j+1] += 2; // the other one goes at a new branch ...
			hc->bits_freq[j]--; // ... together with the leave node which was there before
			continue;
		}
		i--;
		if (i!=16)
			continue;
		while (hc->bits_freq[i] == 0)
			i--;
		hc->bits_freq[i]--; // remove one leave from the lowest layer (the bottom symbol '111...111')
		break;
	}

	// sort the input symbols according to their code size
	for (i=0; i<256; i++)
		hc->sym_sorted[i] = -1;
	int j;
	int k = 0;
	for (i=1; i<32; i++)
		for (j=0; j<256; j++)
			if (hc->bits[j] == i)
				hc->sym_sorted[k++] = j;

	// determine the size of the huffman code symbols - this may differ from code_len because
	// of the 16 bit limit
	for (i=0; i<256; i++)
		hc->sym_code_len[i] = 0;
	k=0;
	for (i=1; i<=16; i++)
		for (j=1; j<=hc->bits_freq[i]; j++)
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

void init_huffman()
{
	int i;

	huff_code* luma_dc   = &Luma_dc;
	huff_code* luma_ac   = &Luma_ac;
	huff_code* chroma_dc = &Chroma_dc;
	huff_code* chroma_ac = &Chroma_ac;

	// initialize
	for (i=0; i<257; i++)
		luma_dc->sym_freq[i] = luma_ac->sym_freq[i] = chroma_dc->sym_freq[i] = chroma_ac->sym_freq[i] = 0;
	// reserve one code point
	luma_dc->sym_freq[256] = luma_ac->sym_freq[256] = chroma_dc->sym_freq[256] = chroma_ac->sym_freq[256] = 1;

	// calculate frequencies as basis for the huffman table construction
	calc_dc_freq(WIDTH*HEIGHT, Y_quant, luma_dc->sym_freq);
	calc_ac_freq(WIDTH*HEIGHT, Y_quant, luma_ac->sym_freq);
	calc_dc_freq(WIDTH*HEIGHT/4, CbCr_quant[0], chroma_dc->sym_freq);
	calc_dc_freq(WIDTH*HEIGHT/4, CbCr_quant[1], chroma_dc->sym_freq);
	calc_ac_freq(WIDTH*HEIGHT/4, CbCr_quant[0], chroma_ac->sym_freq);
	calc_ac_freq(WIDTH*HEIGHT/4, CbCr_quant[1], chroma_ac->sym_freq);

	init_huff_table(luma_dc);
	init_huff_table(luma_ac);
	init_huff_table(chroma_dc);
	init_huff_table(chroma_ac);
}

void zigzag(int num_pixel, int32_t dct[])
{
	int i,j;
	int tmp[64];
	for (i=0; i<num_pixel; i+=64)
	{
		for (j=0; j<64; j++)
			tmp[j] = dct[i+j];
		for (j=0; j<64; j++)
			dct[i+j] = tmp[scan_order[j]];
	}
}

void write_byte(FILE* f, int code_word, int start, int end) {
	if (start == end) return;

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
		fputc(byte_buffer, f);
		if (byte_buffer == 0xFF) fputc(0, f); // stuffing bit
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
#ifdef DEBUG
	printf("** write_bits(%s)\n", binary_string(code_word, code_len));
#endif
	write_byte(f, code_word, 8-bits_written, 8-bits_written-code_len);
}

void encode_dc_value(FILE* f, int dc_val, huff_code* huff)
{
#ifdef DEBUG
	printf("writing dc_val=%d\n", dc_val);
#endif
	int class = huffClass(dc_val);
	int class_code = huff->sym_code[class];
	int class_size = huff->sym_code_len[class];
	write_bits(f, class_code, class_size);

	unsigned int id = abs(dc_val);
	if (dc_val < 0)
		id = ~id;
	write_bits(f, id, class);
}

void encode_ac_value(FILE* f, int ac_val, int num_zeros, huff_code* huff)
{
#ifdef DEBUG
	printf("writing ac_val=%d, num_zeros=%d\n", ac_val, num_zeros);
#endif

	int class = huffClass(ac_val);
	int v = ((num_zeros<<4)&0xF0) | (class&0x0F);
	int code = huff->sym_code[v];
	int size = huff->sym_code_len[v];
	write_bits(f, code, size);

	unsigned int id = abs(ac_val);
	if (ac_val < 0)
		id = ~id;
	write_bits(f, id, class);
}
void fill_last_byte(FILE* f)
{
	byte_buffer |= (1<<(8-bits_written))-1;
	fputc(byte_buffer, f);
	byte_buffer = 0;
	bits_written = 0;
}

void write_dht_header(FILE* f, int code_len_freq[], uint8_t sym_sorted[], int tc_th)
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

void write_file(char* file_name)
{
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

	write_dht_header(f, Luma_dc.bits_freq,Luma_dc.sym_sorted, 0x00);
	write_dht_header(f, Luma_ac.bits_freq,Luma_ac.sym_sorted, 0x10);
	write_dht_header(f, Chroma_dc.bits_freq, Chroma_dc.sym_sorted, 0x01);
	write_dht_header(f, Chroma_ac.bits_freq, Chroma_ac.sym_sorted, 0x11);

	fputc(0xff, f); fputc(0xc0, f); // sof0 symbol (baseline dct)
		fputc(0, f); fputc(17, f); // len
		fputc(0x08, f); // data precision - 8bit
		fputc(((HEIGHT)>>8)&0xFF, f); fputc((HEIGHT)&0xFF, f); // picture height
		fputc(((WIDTH)>>8)&0xFF, f); fputc((WIDTH)&0xFF, f); // picture width
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
		// write_coefficients(f, WIDTH*HEIGHT, Y_quant, &Luma_dc, &Luma_ac);
		// fill_last_byte(f);

	fputc(0xFF, f); fputc(0xDA, f); // SOS Symbol
		fputc(0, f); fputc(8, f); // len
		fputc(1, f); // number of components
		fputc(2, f); // id of component
		fputc(0x11, f); // table index, bit0-3=AC-table, bit4-7=DC-table
		fputc(0x00, f); // start of spectral or predictor selection - not used
		fputc(0x3F, f); // end of spectral selection - default value
		fputc(0x00, f); // successive approximation bits - default value
		// write_coefficients(f, WIDTH*HEIGHT/4, CbCr_quant[0], &Chroma_dc, &Chroma_ac);
		// fill_last_byte(f);

	fputc(0xFF, f); fputc(0xDA, f); // SOS Symbol
		fputc(0, f); fputc(8, f); // len
		fputc(1, f); // number of components
		fputc(3, f); // id of component
		fputc(0x11, f); // table index, bit0-3=AC-table, bit4-7=DC-table
		fputc(0x00, f); // start of spectral or predictor selection - not used
		fputc(0x3F, f); // end of spectral selection - default value
		fputc(0x00, f); // successive approximation bits - default value
		// write_coefficients(f, WIDTH*HEIGHT/4, CbCr_quant[1], &Chroma_dc, &Chroma_ac);
		// fill_last_byte(f);

	fputc(0xFF, f); fputc(0xD9, f); // EOI Symbol

	fclose(f);
}

int read_ppm(FILE* f)
{
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
#ifdef DEBUG
		else
			printf("PPM Comment: %s\n", buf+1);
#endif
	}
	int width, height;
	if (sscanf(buf, "%d %d\n", &width, &height) != 2)
		goto X;
#ifdef DEBUG
	printf("Dimension: %dx%d\n", data->width, data->height);
#endif

	int depth;
	if (fscanf(f, "%d\n", &depth) != 1) goto X;
	
  long len = ftell(f);
	fseek(f, 0L, SEEK_END);
	len = ftell(f) - len;
	fseek(f, -len, SEEK_END);
	if (len != 3*WIDTH*HEIGHT) // 3 color channels
		goto X;

	int i;
	for (i=0; i<WIDTH*HEIGHT; i++)
	{
		rgb[0][i] = fgetc(f);
		rgb[1][i] = fgetc(f);
		rgb[2][i] = fgetc(f);
	}
	
	return 0;
	
X:	fprintf(stderr, "Could not parse the PPM file properly\n");
	return -1;
}

                                                     /* --- JPEG ENCODER --- */

int encode(int in, char * out) {
  printf("inside encode\n");
  if (!(readPpm(in) == 0)) {
		close(in);
  printf("Error\n");
		return -1;
	}
	close(in);
  toYcbcr();
  Subsampling(YCbCr[1], CbCr_sub[0]);
  Subsampling(YCbCr[2], CbCr_sub[1]);
  FILE * f0 = fopen("myParts/Y-Pre", "w");
  FILE * f1 = fopen("myParts/Cb-Pre", "w");
  FILE * f2 = fopen("myParts/Cr-Pre", "w");
  for (int i = 0; i < WIDTH*HEIGHT; i++) {
    fprintf(f0,"%i%s", YCbCr[0][i], i%WIDTH == WIDTH-1 ? "\n" : " ");
    if (!(i%4)) {
      fprintf(f1,"%i%s", CbCr_sub[0][i], (i/4)%(WIDTH/2) == (WIDTH/2)-1 ? "\n" : " ");
      fprintf(f2,"%i%s", CbCr_sub[1][i], (i/4)%(WIDTH/2) == (WIDTH/2)-1 ? "\n" : " ");
    }
  }
  // int32_t i = 0;
  // for (; i < WIDTH*HEIGHT/64; i++) {
  //   ddct(Y_order+(i*64), Y_quant+(i*64), luma_quantizer, 0, i*64);
  //   toZigZag(Y_quant+(i*64), Y_zz+(i*64));
  //   if (!(i%4)) {
  //     ddct(CbCr_order[0]+((i/4)*64), CbCr_quant[0]+((i/4)*64), chroma_quantizer, 1, (i/4)*64);
  //     toZigZag(CbCr_quant[0]+((i/4)*64), CbCr_zz[0]+((i/4)*64));
  //     ddct(CbCr_order[1]+((i/4)*64), CbCr_quant[1]+((i/4)*64), chroma_quantizer, 2, (i/4)*64);
  //     toZigZag(CbCr_quant[1]+((i/4)*64), CbCr_zz[1]+((i/4)*64));
  //   }
  // }
  //  not my stuff
	init_dct_lookup();
	dct(WIDTH/8,  HEIGHT/8, YCbCr[0], Y_dct);
	dct(WIDTH/16, HEIGHT/16, CbCr_sub[0], CbCr_dct[0]);
	dct(WIDTH/16, HEIGHT/16, CbCr_sub[1], CbCr_dct[1]);
  f0 = fopen("myParts/Y-Dct", "w");
  f1 = fopen("myParts/Cb-Dct", "w");
  f2 = fopen("myParts/Cr-Dct", "w");
  for (int i = 0; i < WIDTH*HEIGHT; i++) {
    fprintf(f0,"%f%s", Y_dct[i], i%WIDTH == WIDTH-1 ? "\n" : " ");
    if (!(i%4)) {
      fprintf(f1,"%f%s", CbCr_dct[0][i], (i/4)%(WIDTH/2) == (WIDTH/2)-1 ? "\n" : " ");
      fprintf(f2,"%f%s", CbCr_dct[1][i], (i/4)%(WIDTH/2) == (WIDTH/2)-1 ? "\n" : " ");
    }
  }
	quantize(WIDTH*HEIGHT, Y_dct, Y_quant, luma_quantizer);
	quantize(WIDTH*HEIGHT/4, CbCr_dct[0], CbCr_quant[0], chroma_quantizer);
	quantize(WIDTH*HEIGHT/4, CbCr_dct[1], CbCr_quant[1], chroma_quantizer);
  f0 = fopen("myParts/Y-Quant", "w");
  f1 = fopen("myParts/Cb-Quant", "w");
  f2 = fopen("myParts/Cr-Quant", "w");
  for (int i = 0; i < WIDTH*HEIGHT; i++) {
    fprintf(f0,"%i%s", Y_quant[i], i%WIDTH == WIDTH-1 ? "\n" : " ");
    if (!(i%4)) {
      fprintf(f1,"%i%s", CbCr_quant[0][i], (i/4)%(WIDTH/2) == (WIDTH/2)-1 ? "\n" : " ");
      fprintf(f2,"%i%s", CbCr_quant[1][i], (i/4)%(WIDTH/2) == (WIDTH/2)-1 ? "\n" : " ");
    }
  }
  fclose(f0);
  fclose(f1);
  fclose(f2);
	zigzag(WIDTH*HEIGHT, Y_quant);
	zigzag(WIDTH*HEIGHT/4, CbCr_quant[0]);
	zigzag(WIDTH*HEIGHT/4, CbCr_quant[1]);
  f0 = fopen("myParts/Y-ZigZag", "w");
  f1 = fopen("myParts/Cb-ZigZag", "w");
  f2 = fopen("myParts/Cr-ZigZag", "w");
  for (int i = 0; i < WIDTH*HEIGHT; i++) {
    fprintf(f0,"%i%s", Y_quant[i], i%WIDTH == WIDTH-1 ? "\n" : " ");
    if (!(i%4)) {
      fprintf(f1,"%i%s", CbCr_quant[0][i], (i/4)%(WIDTH/2) == (WIDTH/2)-1 ? "\n" : " ");
      fprintf(f2,"%i%s", CbCr_quant[1][i], (i/4)%(WIDTH/2) == (WIDTH/2)-1 ? "\n" : " ");
    }
  }
  fclose(f0);
  fclose(f1);
  fclose(f2);
  // my stuff
  // toBlockOrder(WIDTH, HEIGHT, YCbCr[0], Y_order);
  // toBlockOrder(WIDTH/2, HEIGHT/2, CbCr_sub[0], CbCr_order[0]);
  // toBlockOrder(WIDTH/2, HEIGHT/2, CbCr_sub[1], CbCr_order[1]);
  // FILE * f0 = fopen("myParts/Y-Dct", "w");
  // FILE * f1 = fopen("myParts/Y-Quant", "w");
  // FILE * f2 = fopen("myParts/Cb-Dct", "w");
  // uint32_t i = 0;
  // for (; i < WIDTH*HEIGHT/64; i++) {
  //   ddct(Y_order+(i*64), Y_quant+(i*64), luma_quantizer, 0, i*64);
  //   toZigZag(Y_quant+(i*64), Y_zz+(i*64));
  //   for (int k = 0; k < 64; k++) {
  //     fprintf(f0," %f%s", *(Y_dct + i*64 + k), k%8 == 7 ? "\n" : " ");
  //     fprintf(f1," %i%s", *(Y_quant + i*64 + k), k%8 == 7 ? "\n" : " ");
  //   }
  //   fprintf(f0," ---====--- \n");
  //   fprintf(f1," ---====--- \n");
  //   if (!(i%4)) {
  //     ddct(CbCr_order[0]+((i/4)*64), CbCr_quant[0]+((i/4)*64), chroma_quantizer, 1, (i/4)*64);
  //     toZigZag(CbCr_quant[0]+((i/4)*64), CbCr_zz[0]+((i/4)*64));
  //     ddct(CbCr_order[1]+((i/4)*64), CbCr_quant[1]+((i/4)*64), chroma_quantizer, 2, (i/4)*64);
  //     toZigZag(CbCr_quant[1]+((i/4)*64), CbCr_zz[1]+((i/4)*64));
  //     for (int k = 0; k < 64; k++) {
  //       fprintf(f2," %f%s", *(CbCr_dct[0] + i*64 + k), k%8 == 7 ? "\n" : " ");
  //       fprintf(f3," %f%s", *(CbCr_dct[1] + i*64 + k), k%8 == 7 ? "\n" : " ");
  //       fprintf(f4," %i%s", *(CbCr_quant[0] + i*64 + k), k%8 == 7 ? "\n" : " ");
  //       fprintf(f5," %i%s", *(CbCr_quant[1] + i*64 + k), k%8 == 7 ? "\n" : " ");
  //     }
  //     fprintf(f2," ---====--- \n");
  //     fprintf(f3," ---====--- \n");
  //     fprintf(f4," ---====--- \n");
  //     fprintf(f5," ---====--- \n");
  //   }
  // }
  // for (int i = 0; i < 64; i++) {
  //   printf("%f%c", getDouble(lookup_table + i), i%8 == 7 ? '\n' : ' ');
  // }
  // ----
  // f0 = fopen("myParts/Y-Dct", "w");
  // f1 = fopen("myParts/Cb-Dct", "w");
  // f2 = fopen("myParts/Cr-Dct", "w");
  // for (int i = 0; i < WIDTH*HEIGHT; i++) {
  //   fprintf(f0," %i%s", Y_zz[i], i%WIDTH == 0 ? "\n" : " ");
  // }
  // for (int i = 0; i < WIDTH*HEIGHT/4; i++) {
  //   fprintf(f1," %i%s", CbCr_zz[0][i], i%(WIDTH/2) == 0 ? "\n" : " ");
  //   fprintf(f2," %i%s", CbCr_zz[1][i], i%(WIDTH/2) == 0 ? "\n" : " ");
  // }
  // fclose(f0);
  // fclose(f1);
  // fclose(f2);
  // fclose(f3);
  // fclose(f4);
  // fclose(f5);
  // ---
  // my stuff
  diff_dc(WIDTH*HEIGHT, Y_quant);
  diff_dc(WIDTH*HEIGHT/4, CbCr_quant[0]);
  diff_dc(WIDTH*HEIGHT/4, CbCr_quant[1]);
  f0 = fopen("myParts/Y-Diff", "w");
  f1 = fopen("myParts/Cb-Diff", "w");
  f2 = fopen("myParts/Cr-Diff", "w");
  for (int i = 0; i < WIDTH*HEIGHT; i++) {
    fprintf(f0,"%i%s", Y_quant[i], i%WIDTH == WIDTH-1 ? "\n" : " ");
    if (!(i%4)) {
      fprintf(f1,"%i%s", CbCr_quant[0][i], (i/4)%(WIDTH/2) == (WIDTH/2)-1 ? "\n" : " ");
      fprintf(f2,"%i%s", CbCr_quant[1][i], (i/4)%(WIDTH/2) == (WIDTH/2)-1 ? "\n" : " ");
    }
  }
  fclose(f0);
  fclose(f1);
  fclose(f2);
  // InitHuffman();
  // not my stuff
  init_huffman();
  printf("post InitHuffman\n");
	write_file(out);
  // if(!(writeJpeg(out) == 0)){
  //   close(out);
  //   printf("Error\n");
  //   return -1;
  // }
  // close(out);
  printf("post close out\n");
  return 0;
}

int readPpm(int fd){
  printf("inside read ppm\n");
  char buf;
  uint8_t count = 0;
  char comment[1024], num[10];
  uint32_t i = 0;
  uint64_t pix_length;
  read(fd, &buf, 1);
  while (1) {
    if (count == 0 && buf == 'P') {
      read(fd, &buf, 1);
      if(buf == '6') read(fd, &buf, 1);
      else return -1;
      if(buf == '\n') count++;
      else return -1;
    } else if (count == 1) {
      read(fd, &buf, 1);
      if(buf == '#'){
        i = 0;
        comment[i++] = buf;
        while (read(fd, &buf, 1) > 0) {
        comment[i++] = buf;
          if(buf == '\n') break;
        }
      } else if(isdigit(buf)){
        i = 0;
        num[i++] = buf;
        while (read(fd, &buf, 1) > 0) {
          if(buf == ' ') break;
          num[i++] = buf;
        }
        num[i] = 0;
        if(atoi(num) != WIDTH) return -1;
        i = 0;
        while (read(fd, &buf, 1) > 0) {
          if(buf == '\n') break;
          num[i++] = buf;
        }
        num[i] = 0;
        if(atoi(num) != HEIGHT) return -1;
        count++;
      } else return -1;
    } else if (count == 2) {
      read(fd, &buf, 1);
      if(isdigit(buf)){
        i = 0;
        num[i++] = buf;
        while (read(fd, &buf, 1) > 0) {
          if(buf == '\n') break;
          num[i++] = buf;
        }
        num[i] = 0;
        if(atoi(num) != 255) return -1;
      count++;
      } else return -1;
    } else if (count == 3) {
      pix_length = lseek(fd, 0, SEEK_CUR);
      pix_length = lseek(fd, 0, SEEK_END) - pix_length;
      lseek(fd, -pix_length, SEEK_END);
      if(pix_length == 3*WIDTH*HEIGHT) count++;
      else return -1;
    } else if (count == 4) {
      i = 0;
      for(; i < WIDTH*HEIGHT; i++){

        read(fd, &buf, 1);
        rgb[0][i] = (uint8_t) buf;
        read(fd, &buf, 1);
        rgb[1][i] = (uint8_t) buf;
        read(fd, &buf, 1);
        rgb[2][i] = (uint8_t) buf;
      }
      return 0;
    } else return -1;
  }
}

void toYcbcr(void) {                                                                            // Converts RGB values to YCbCr values
    uint32_t i = 0;
    for (; i < WIDTH*HEIGHT; i++) {
        YCbCr[0][i] =       0.299    * rgb[0][i] + 0.587    * rgb[1][i] + 0.114    * rgb[2][i];
        YCbCr[1][i] = 128 - 0.168736 * rgb[0][i] - 0.331264 * rgb[1][i] + 0.5      * rgb[2][i];
        YCbCr[2][i] = 128 + 0.5      * rgb[0][i] - 0.418688 * rgb[1][i] - 0.081312 * rgb[2][i];
    }
}

void Subsampling(uint8_t *in, uint8_t *out) {                                                     // Subsampling of the Chroma components (Cb & Cr)
    uint32_t i = 0;
	  uint32_t h = 0,w = 0;
	for (h=0; h<HEIGHT/2; h++)
		for (w=0; w<WIDTH/2; w++)
		{
			int i = 2*h*WIDTH + 2*w;
			out[h*WIDTH/2+w] = ( in[i] + in[i+1] + in[i+WIDTH] + in[i+WIDTH+1] ) / 4;
		}
    // for (; i < HEIGHT*WIDTH/4; i++) {
    //         uint32_t j = 2*(i/(WIDTH/2))*WIDTH + 2*(i%(WIDTH/2));
    //         out[(i/(WIDTH/2))*WIDTH/2+(i%(WIDTH/2))] = (in[j] + in[j+1] + in[j+WIDTH] + in[j+WIDTH+1])/4;
    //     }
}

inline void toBlockOrder(int width, int height, uint8_t *in, uint8_t *out) {
    uint32_t i = 0;                                                                                                                    // (i%64) = blocknumber (WIDTH/8) = blocks in a line
    for (; i < height*width; i++) out[i] = in[(((i/height)/8)*(width/8) + (i%width)/8)*64 + ((i/height)%8)*8 + (i%width)%8];          // (i/8) = line of the block (i%8) = pixel in the line
}

void ddct(uint8_t *in, int16_t *out, const uint8_t *quant, int type, int gap) {                                             // Direct discrete cosine transform for the block
    uint32_t i = 0, j;                                    // Formula: (1/(2*N)^(1/2))C(h)C(w)sum(x=[0-7],sum(y=[0-7],in[x][y]*cos(((2I+1)pi*x)/2N)*cos(((2J+1)pi*y)/2N)))

    // first step: 1D dct of the immage in the vertical axis

    double_t inner_lookup[64], dct[64];
    for (; i < 64; i++){                       // first sum by cos(((2J+1)pi*y)/2N) i = x*8+J and j = y
        inner_lookup[i] = 0;
        for(j = 0; j < 8; j++) inner_lookup[i] += (in[j*8+i/8] - 128) * getDouble(lookup_table + (j*8+i%8)); // max val = 127 * 1 * 8 = 1016 (inner_lookup[0] with white image)
    }

    // secoond step: 1D dct of the partial dct matrix obtained from the first step in the orizontal axis

    i = 0;
    for(; i < 64; i++){                         // second sum by cos(((2I+1)pi*x)/2N) i = J*8+I and j = x
        dct[i] = 0;
        for (j = 0; j < 8; j++) dct[i] += inner_lookup[j*8+i/8] * getDouble(lookup_table + (j*8+i%8));
        if (!i/8) dct[i] *= M_SQRT1_2;                                                          // multiplication by C(h) (coefficient of vert axis)
        if (!i%8) dct[i] *= M_SQRT1_2;                                                          // multiplication by C(w) (coefficient of oriz axis)
        dct[i] /= 4;                                                                            // divison by (2*N)^(1/2) where N = 8
        if(!type) Y_dct[i+gap] = dct[i];
        else CbCr_dct[type-1][i+gap] = dct[i];
         ;
    }
    // third step: applying a quantization

    i = 0;
  for (; i < 64 ; i++) out[i] = CLIP(dct[i]/quant[i], -2048, 2047);
}

inline void toZigZag(int16_t *in, int16_t *out) {                                                 // Reorders the values in the block in a zig zag pattern
    int8_t i = 0;
    for (; i < 64; i++) out[i] = in[scan_order[i]];
}

void diffDC(int pixels, int32_t *dct_quant) {                                                              // Turns the absolute DC coefficients in differential values
    int i = 64;
    int last = dct_quant[0];
    for (; i < pixels; i += 64) {
        dct_quant[i] -= last;
        last += dct_quant[i];
    }
}
 // -----------------------------------------------------------------------------------------------------------------

void GenHuffTable(huff_code* hc) {
  printf("dentro GenHufTable\n");
    uint32_t i = 0;
    for(; i < 257; i++) {
        hc->bits[i] = 0;
        hc->next[i] = -1;
    }
  printf("post init\n");
    // derive the code length for each symbol
    while (1) {
        int16_t v1 =  -1;
        int16_t v2 =  -1;
        int32_t i = 0;
        // find least value of freq(v1) and next least value of freq(v2)
        for (; i < 257; i++) {
            if (hc->sym_freq[i] == 0) continue;
            if (v1 == -1 ||  hc->sym_freq[i] <= hc->sym_freq[v1] ) {
                v2 = v1;
                v1 = i;
            } else if (v2 == -1 || hc->sym_freq[i] <= hc->sym_freq[v2]) v2 = i;
        }
        if (v2 ==  -1) break; // if there aren't 2 new elements it means the tree is complete

        hc->sym_freq[v1] += hc->sym_freq[v2]; // adding the new node (equal to the sum of v1 and v2) to the node list
        hc->sym_freq[v2] = 0;
        while (1) { // adding a bit to the length of each code in the sub-tree of v1
            hc->bits[v1]++;
            if (hc->next[v1] == -1) break;
            v1 = hc->next[v1];
        }
        hc->next[v1] = v2; // adds v2 as next to v1
        while (1) { // adding a bit to the length of each code in the sub-tree of v2
            hc->bits[v2]++;
            if (hc->next[v2] == -1) break;
            v2 = hc->next[v2];
        }
    }
  printf("post tree\n");
    i = 0;
    for(; i < 32; hc->bits_freq[i++] = 0);
    i = 0;
    // derive code length frequencies
    for (; i < 257; i++) if(hc->bits[i]) hc->bits_freq[hc->bits[i]]++; // how many leafs are in each level
  printf("derived code length frequencies\n\n");

    // limit the huffman code length to 16 bits JPEG doesn't allow codes longer than 16b
    i = 31;
    while (1) {
        if (hc->bits_freq[i] > 0) {             // if the code is too long ...
            int j = i-1;
            while (hc->bits_freq[--j] <= 0);    // ... we search for an upper layer containing leaves
            hc->bits_freq[i] -= 2;              // we remove the two leaves from the lowest layer
            hc->bits_freq[i-1]++;               // and put one of them one position higher
            hc->bits_freq[j+1] += 2;            // the other one goes at a new branch ...
            hc->bits_freq[j]--;                 // ... together with the leave node which was there before
            continue;
        }
        i--;
    printf("\033[1A[%i]   \n", i);
        if (i != 16) continue; 
      printf("\n");
            while (hc->bits_freq[i] == 0 && i-- > 0) printf("\033[1A[%i]   \n", i);
            i++;
            hc->bits_freq[i]--; // remove one leaf from the lowest layer (the bottom symbol '111...111')
            break;              // JPEG doesn't allow codes composed by only ones
    }
  printf("post tree adjustment\n");
    i = 0;
    // sort the input symbols according to their code size
    for (; i < 256; hc->sym_sorted[i++] = -1);
    int j = 0, k = 0;
    i = 0;
    for (; i < 8192; i++) {
    if (hc->bits[i%256] == i/256 + 1) hc->sym_sorted[k++] = i%256;
  }

    // determine the size of the huffman code symbols - this may differ from bits because
    // of the 16 bit limit
    i = 0;
    for (; i < 256; hc->sym_code_len[i++] = 0);
    i = 0;
    k = 0;
    for (; i < 16; i++) while (j++ < hc->bits_freq[i + 1]) hc->sym_code_len[hc->sym_sorted[k++]] = i + 1;
    hc->sym_code_len[hc->sym_sorted[k]] = 0;
    printf("post some stuff, dunno\n");
    // generate the codes for the symbols
    i = 0;
    for (; i < 256; hc->sym_code[i++] = -1);
    k = 0;
    printf("other unknown loops\n");
    int code = 0, cs = 0;
    int si = hc->sym_code_len[hc->sym_sorted[0]];
  printf("\n");
    while (1) {
        do {
      printf("\033[1A[%i]\n", k);
            hc->sym_code[hc->sym_sorted[k++]] = code++;
      // code++;
      // k++;
        } while (hc->sym_code_len[hc->sym_sorted[k]] == si); /*  || hc->sym_sorted + k == NULL); */
        if (hc->sym_code_len[hc->sym_sorted[k]] == 0) break;
        do {
            code <<= 1;
            si++;
        } while (hc->sym_code_len[hc->sym_sorted[k]] != si);
    }
  printf("fine GenHuffTable\n");
}

uint8_t huffClass(int16_t value) {
    value = value < 0 ? -value : value;
    uint8_t class = 0;
    for (; value > 0; class++, value >>= 1);
    return class;
}

void calcDCFreq(uint32_t pixels, int32_t *dct_quant, uint8_t *freq) {
    uint32_t i = 0;
    for (; i < pixels; i+=64) freq[huffClass(dct_quant[i])]++;
}

void calcACFreq(uint32_t pixels, int32_t *dct_quant, uint8_t *freq) {
    uint32_t i = 0;
    uint8_t num_zeros = 0;
    uint8_t last_nonzero;
    for (; i < pixels; i++) {
        if (!(i%64)) {
            for (last_nonzero = i+63; last_nonzero > i; last_nonzero--) if (dct_quant[/* (i/64)*64 +  */last_nonzero] != 0) break;
            continue;
        }

        if (i == /* (i/64)*64 +  */last_nonzero + 1) {
            freq[0x00]++; // EOB byte
            // jump to the next block
            i = (i/64+1)*64-1;
            continue;
        }

        if (dct_quant[i] == 0) {
            num_zeros++;
            if (num_zeros == 16) {
                freq[0xF0]++; // ZRL byte
                num_zeros = 0;
            }
            continue;
        }

        freq[ ((num_zeros<<4)&0xF0) | (huffClass(dct_quant[i])&0x0F) ]++;
        num_zeros = 0;
    }
}

void InitHuffman(void) {
    uint16_t i = 0;
    // initialize
  printf("\n");
    for (; i < 256; i++){
        Luma_dc.sym_freq[i] = Luma_ac.sym_freq[i] = Chroma_dc.sym_freq[i] = Chroma_ac.sym_freq[i] = 0;
  }
    // reserve one code point
    Luma_dc.sym_freq[256] = Luma_ac.sym_freq[256] = Chroma_dc.sym_freq[256] = Chroma_ac.sym_freq[256] = 1;

    // calculate frequencies as basis for the huffman table construction
    calcDCFreq(WIDTH*HEIGHT, Y_zz, Luma_dc.sym_freq);
    calcACFreq(WIDTH*HEIGHT, Y_zz, Luma_ac.sym_freq);
    calcDCFreq(WIDTH*HEIGHT/4, CbCr_zz[0], Chroma_dc.sym_freq);
    calcDCFreq(WIDTH*HEIGHT/4, CbCr_zz[1], Chroma_dc.sym_freq);
    calcACFreq(WIDTH*HEIGHT/4, CbCr_zz[0], Chroma_ac.sym_freq);
    calcACFreq(WIDTH*HEIGHT/4, CbCr_zz[1], Chroma_ac.sym_freq);
    printf("post Frequency calculations\n");
    GenHuffTable(&Luma_dc);
    GenHuffTable(&Luma_ac);
    GenHuffTable(&Chroma_dc);
    GenHuffTable(&Chroma_ac);
}


                                            /* --- JPEG DECODER --- */
int decode(int in, int out) {
  return 0;
}

void toRgb(void) {                                                                              // Converts YCbCr values to RGB values
    uint32_t i = 0;
    for (; i < WIDTH*HEIGHT; i++) {
        rgb[0][i] = 1 * YCbCr[0][i] + 0     * (YCbCr[1][i] - 128) + 1.4   * (YCbCr[2][i] - 128);
        rgb[1][i] = 1 * YCbCr[0][i] - 0.343 * (YCbCr[1][i] - 128) - 0.711 * (YCbCr[2][i] - 128);
        rgb[2][i] = 1 * YCbCr[0][i] + 1.765 * (YCbCr[1][i] - 128) + 0     * (YCbCr[2][i] - 128);
    }
}

void Upsampling(int8_t *in, int8_t *out) {                                                      // Upsampling of the Chroma components (Cb & Cr)
    uint32_t i = 0;
    for(; i < HEIGHT*WIDTH; i++) out[i] = in[(i - i%WIDTH)/2 + (i%WIDTH)/2];
}

inline void fromBlockOrder(int8_t *in, int8_t *out) {
    uint32_t i = 0;
    for(; i < HEIGHT*WIDTH; i++) out[((i%64)%(WIDTH/8))+ (i/8)*WIDTH + i%8] = in[i];
}

void idct(int8_t *in, int8_t *out, const int8_t *quant) {
    double_t inner_lookup[64], dct[64];
    // first step: applying an inverse quantization
    uint8_t i = 0, j;
    for (; i < 64 ; i++) {
        dct[i] = (double_t)(in[i]*quant[i]);
        dct[i] = CLIP(dct[i], -2048, 2047); // capire gli effettivi valori di min e max di clip
    }

    i = 0;
    for(; i < 64; i++){
        inner_lookup[i] = 0;
        for (j = 0; j < 8; j++) inner_lookup[i] += dct[j*8+i/8] * getDouble(lookup_table + ((i%8)*8+j)); // potrebbe essere giusto
    }

    i = 0;
    for (; i < 64; i++){
        out[i] = 0;
        for(j = 0; j < 8; j++) out[i] += (inner_lookup[j*8+i/8] - 128) * getDouble(lookup_table + ((i%8)*8+j)); // potrebbe essere giusto
        if (!i/8) dct[i] *= M_SQRT1_2;
        if (!i%8) dct[i] *= M_SQRT1_2;
        out[i] /= 4;
    }
    //TODO: capire come implementare l'inversa della dct
}

inline void fromZigZag(int16_t *in, int16_t *out) {                                               // Reorders the values in the block from a zig zag pattern to the original one
    int8_t i = 0;
    for (; i < 64; i++) out[scan_order[i]] = in[i];
}

void abs_dc(int8_t *dct_quant) {                                                                // Turns the absolute DC coefficients in differential values
    int i = 64;
    for (; i < WIDTH*HEIGHT; i += 64) dct_quant[i] += dct_quant[i - 64];
}

// -----------------------------------------------------------------------------------------------------------------
