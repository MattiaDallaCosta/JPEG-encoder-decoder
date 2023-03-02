#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/types.h>
#include "include/structs.h"

int main (int argc, char ** argv) {
  char imagename[100];
  key_t key = ftok("/tmp/queue", 1);
  if (key == -1) {
    printf("\033[0;031mUnable to create key for queue\033[0m\n");
    return -1;
  }

  int qid = msgget(key, IPC_EXCL|IPC_CREAT|0644);
  if (qid != -1) {
    printf("\033[0;031mUnable to open  existing queue\033[0m\n");
    msgctl(qid, IPC_RMID, NULL);
    return -1;
  }
  qid = msgget(key, 0644);

  printf("Insert the name of the file you want to compare:\n-> ");
  scanf("%s", imagename);
  imagename[strlen(imagename)] = 0;
  msg_t msg;
  msg.mtype = 1l;
  strcpy(msg.mtext, imagename);
  // msg.len = 0;

  msgsnd(qid, &msg, strlen(imagename)+sizeof(int), 0);

  return 0;
}
