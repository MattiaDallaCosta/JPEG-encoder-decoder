#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <assert.h>
#include <sys/types.h>

double_t cos_lookup[64];
int64_t icos_lookup[64];
inline void init_dct_lookup(){
	for (int i = 0; i < 64; i++) cos_lookup[i] = cos((double_t)(2*(i/8)+1)*(i%8)*M_PI/16);
}

inline void convert(void){
    for (int i = 0; i < 64; i++) icos_lookup[i] = *(int64_t*)(cos_lookup + i);
}

inline void compare(void){
  for (int i = 0; i < 64; i++) printf("[%i]  (cos)%.20lf = %.20lf(icos)\n", i, cos_lookup[i], i, *(double_t*)(icos_lookup + i));
}
int main(int argc, char *argv[]) {
  init_dct_lookup();
  convert();
  for (int i = 0; i < 64; i++) {
    printf("%li%c%s",icos_lookup[i], i == 63 ? ' ' : ',' , i%8 != 7 ? (cos_lookup[i+1] >= 0 ? "  " : " ") : "\n");
  }
  printf("\n\n\n");
  for (int i = 0; i < 64; i++) {
    printf("%lf%c%s",cos_lookup[i], i == 63 ? ' ' : ',' , i%8 != 7 ? (cos_lookup[i+1] >= 0 ? "  " : " ") : "\n");
  }
  printf("\n\n\n");
  compare();
  return 0; 
}
