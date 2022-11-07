#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/types.h>


typedef struct {
  long mtype;
  char mtext[1024];
} msg_t;

int main (int argc, char ** argv) {
  // if (argc != 2) {
  //   printf("Usage: %s <file>\nWhere <file> is the image to be compared\n",argv[0]);
  //   return -1;
  // }
  char imagename[1024];
  key_t key = ftok("/tmp/queue", 1);
  if (key == -1) {
    printf("\033[0;031mUnable to create key for queue\033[0m\n");
    return -1;
  }

  int qid = msgget(key, 0644);
  if (qid == -1) {
    printf("\033[0;031mUnable to open queue\033[0m\n");
    return -1;
  }

  printf("Insert the name of the file you want to compare:\n->");
  scanf("%s", imagename);
  printf("sending message containing [%s] to comparator\n", imagename);

  msg_t msg;
  msg.mtype = 1;
  strcpy(msg.mtext, imagename);

  msgsnd(qid, &msg, strlen(imagename), 0);
  

  return 0;
}
