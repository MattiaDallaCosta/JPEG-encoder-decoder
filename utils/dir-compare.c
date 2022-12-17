#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

void compareFiles(char *dir1, char *file1, char*dir2, char *file2);

int main (int argc, char ** argv) {
  char cmd[100] = "ls ";
  char buf[1024];
  char out1[32][1024];
  char out2[32][1024];
  char c;
  int n = 0, len1 = 0, len2 = 0, i, off;
  if (argc != 3) {
    printf("usage: %s <dir1> <dir2>\n where <dir1> <dir2> are the 2 directories to be compared\n", argv[0]);
  }
  FILE * dir1 = popen(strcat(cmd, argv[1]), "r");
  cmd[3] = 0;
  FILE * dir2 = popen(strcat(cmd, argv[2]), "r");
  n = fread(buf, 1, sizeof(buf)-1, dir1);
  buf[n] = 0;
  i = 0;
  off = 0;
  while (i < n) {
    if(buf[i] == '\n'){
      out1[len1][(i)-off] = 0;
      printf("[%i] %s\n", len1, out1[len1]);
      len1++;
      off = i+1;
    } else out1[len1][(i)-off] = buf[i];
    i++;
  }
  n = fread(buf, 1, sizeof(buf)-1, dir2);
  buf[n] = 0;
  i = 0;
  off = 0;
  while (i < n) {
    if(buf[i] == '\n'){
      out2[len2][i - off] = 0;
      printf("[%i] %s\n", len2, out2[len2]);
      len2++;
      off = i+1;
    } else out2[len2][i-off] = buf[i];
    i++;
  }
  pclose(dir1);
  pclose(dir2);
  i = 0;
  int j;
  for (i = 0; i < len1; i++) {
    for (j = 0; j < len2; j++) {
      if (strcmp(out1[i], out2[j]) == 0) {
        compareFiles(argv[1], out1[i], argv[2], out2[j]);
        break;
      }
    }
  }
  return 0;
} 

void compareFiles(char *dir1, char *file1, char*dir2, char *file2) {
  int state = 0;
  char tmp[1024];
  strcpy(tmp, dir1);
  strcat(tmp, "/");
  strcat(tmp, file1);
  int f1 = open(tmp, O_RDONLY, 0644);
  strcpy(tmp, dir2);
  strcat(tmp, "/");
  strcat(tmp, file2);
  int f2 = open(tmp, O_RDONLY, 0644);
  int i = 0;
  char s1[6], s2[6];
  int r1 = read(f1, s1, 5);
  int r2 = read(f2, s2, 5);
  while (r1 && r2 && (r1 == r2)) {
    if (strcmp(s1, s2) != 0) {
      state = 1;
      break;
    }
    r1 = read(f1, s1, 5);
    r2 = read(f2, s2, 5);
  }
if(r1 != r2) state = 1;
printf("%s%s\033[0m\n", state ? "\033[0;31m" : "\033[0;36m", file1);
close(f1);
close(f2);
}
