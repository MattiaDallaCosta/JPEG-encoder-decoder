#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>

typedef struct _pair {
  int w;
  int h;
} pair;

uint8_t *rgb[3];
uint8_t *newrgb[3];
pair olddims, newdims;

int readPpm(int fd);
int writePpm(int fd);
pair getNewDims(char * dims);
void getNewName(char * oldname,char * newname);
void createImmage();

int main (int argc, char ** argv) {
  int fd;
  if (argc != 3) printf("usage: %s <file> <new res>\n", argv[0]);
  if ((fd = open(argv[1], O_RDWR, 0644)) == -1) {
    printf("can't open file %s, Exiting", argv[1]);
  }
  newdims = getNewDims(argv[2]);
  int i = 0;
  for (; i < 3; i++) newrgb[i] = malloc(newdims.w*newdims.h*sizeof(uint8_t)); 

  if(readPpm(fd) != 0){
    close(fd);
    return -1;
  }
    createImmage();
    char * newfile;
    getNewName(argv[1],newfile);
  if ((fd = open(newfile, O_RDWR | O_CREAT, 0644)) == -1) {
    printf("can't open file %s, Exiting\n",newfile);
  }
   if (writePpm(fd) != 0){
    close(fd);
    return -1;
    }
  return 0;
}

int readPpm(int fd){
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
        olddims.w = atoi(num);
        i = 0;
        while (read(fd, &buf, 1) > 0) {
          if(buf == '\n') break;
          num[i++] = buf;
        }
        num[i] = 0;
        olddims.h = atoi(num);
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
      if(pix_length == 3*olddims.w*olddims.h) count++;
      else return -1;
    } else if (count == 4) {
      i = 0;
      for (; i < 3; i++) rgb[i] = malloc(olddims.w*olddims.h*sizeof(uint8_t)); 
      i = 0;
      for(; i < 3*olddims.w*olddims.h; i++){
        read(fd, &buf, 1);
        rgb[i%3][i/3] = (uint8_t) buf;
      }
      return 0;
    } else return -1;
  }
}
int writePpm(int fd){
  char dims[100];
  sprintf(dims, "P6\n%i %i\n255\n", newdims.w, newdims.h);
  write(fd, dims, strlen(dims));
  for (int i = 0; i < 3*newdims.w*newdims.h; i++) write(fd, &(newrgb[i%3][i/3]), 1);
  return 0;
}

pair getNewDims(char * dims){
  int offset = 0;
  char num[10];
  pair p;
  int i = 0;
  for (; i < strlen(dims); i++) {
    if (dims[i] == 'x') {
      num[i-offset] = 0;
      offset = i+1;
      p.w = atoi(num);
    }else if (isdigit(dims[i])) {
      num[i-offset] = dims[i];
    } else {
        exit(-1);
      }
  }
  num[i-offset] = 0;
  offset = i+1;
  p.h = atoi(num);
  return p;
}

void createImmage(){
  int i = 0, j = 0,inoff = 0, outoff = 0, prei;
  int8_t ops[2];
  ops[0] = ((newdims.w - olddims.w) > 0 ? 1 : (newdims.w == olddims.w ? 0 : -1));
  ops[1] = ((newdims.h - olddims.h) > 0 ? 1 : (newdims.h == olddims.h ? 0 : -1));
  int wdiff = ops[0] > 0 ? newdims.w - olddims.w : olddims.w - newdims.w;
  int hdiff = ops[1] > 0 ? newdims.h - olddims.h : olddims.h - newdims.h;
  int loopval = 3*(ops[1] ? olddims.w : newdims.w)*(ops[0] ? olddims.h : newdims.h);
  for (; i < newdims.w*(ops[1] > 0 ? olddims.h : newdims.h); i++) {
    if(i > 0 && i%(ops[0] > 0 ? newdims.w : olddims.w) == (ops[0] > 0 ? olddims.w : (ops[0] == 0 ? 0 : newdims.w))){
      if(ops[0] < 0) {
        outoff += wdiff;
      } else {
        prei = i;
         for (; i < wdiff + prei; i++) {
          newrgb[0][i] = 0xFF;
          newrgb[1][i] = 0xFF;
          newrgb[2][i] = 0xFF;          
          inoff++;
        }
      }
    }
    newrgb[0][i] = rgb[0][i+outoff-inoff];
    newrgb[1][i] = rgb[1][i+outoff-inoff];
    newrgb[2][i] = rgb[2][i+outoff-inoff];
  }
  prei = i;
  if(ops[1] > 0) for (; i < hdiff*newdims.w + prei; i++) {
      newrgb[0][i] = 0xFF;
      newrgb[1][i] = 0xFF; 
      newrgb[2][i] = 0xFF;
    }
}

void getNewName(char * oldname,char * newname) {
  int count = 0, fd;
  char on[strlen(oldname)], tmp[100];
  strcpy(on, oldname);
  char * dot = strrchr(on, '.');
  char end[strlen(dot)];
  strcpy(end, strrchr(on, '.'));
  *dot = 0;
  while(1){
    sprintf(tmp, "%s-%i%s%c",on, count++, end, 0);
    if((fd = open(tmp, O_WRONLY, 0644)) == -1) break;
    close(fd);
  }
  strcpy(newname, tmp);
}
