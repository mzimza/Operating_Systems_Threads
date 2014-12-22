/* Maja Zalewska  */
/* nr 336088      */
/* Zadanie 2, SO  */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <signal.h>
#include <string.h>
#include <errno.h>
#include "err.h"
#include "common.h"

int main (int argc, char *argv[])
{
  int l = (argc < 2) ? 0 : atoi(argv[1]);
  double frec;
  int id, id2;

  if ((id = msgget(M_ALL_KEY, 0)) == -1)
  syserr(errno, "msgget M_REP_KEY");
  if ((id2 = msgget(M_REP_KEY_2, 0)) == -1)
  syserr(errno, "msgget M_REP_KEY_2");

  /* send init msg to server */
  InitMsg ini = {INIT_KEY, getpid(), REP_TYPE, l, 0};
  if (msgsnd(id, (char *) &ini, sizeof(InitMsg) - sizeof(long), 0) != 0)
    syserr(errno, "msgsnd init msg %d", l);
  int read_bytes;
  /* read summary respons */
  RepReturn1 ret1;
  if ((read_bytes = msgrcv(id2, &ret1, sizeof(RepReturn1) - sizeof(long), getpid(), 0)) <= 0)
    syserr(errno, "msgrcv init");
  printf("Przetworzonych komisji: %d / %d\n", ret1.x, ret1.K);
  printf("Uprawnionych do głosowania: %lld\n", ret1.y);
  printf("Głosów ważnych: %lld\n", ret1.z);
  printf("Głosów nieważnych: %lld\n", ret1.v);
  frec = ((double)ret1.z + (double)ret1.v)/(double)ret1.y * 100;
  printf("Frekwencja: %.2f%%\n", frec);
  printf("Wyniki poszczególnych list:\n");

  /* read response for given lists */
  if (l != 0) { 
    /* list number specified */
    RepReturn2 ret2;
    if ((read_bytes = msgrcv(id2, &ret2, sizeof(RepReturn2) - sizeof(long), getpid(), 0)) <= 0)
       syserr(errno, "msgrcv ret2");
    long long sum = 0;
    int j;
    for (j = 1; j <= ret2.K; j++){
       sum = sum + ret2.list[j];
    }
    printf("%d %lld ", l, sum);
    for (j = 1; j <= ret2.K; j++){
       printf("%lld", ret2.list[j]);
       if (j < ret2.K)
          printf(" ");
    }
    printf("\n");
  }
  else {
    /* list number not specified */
    int i = 1;
    int j;
    long long sum = 0;
    RepReturn2 ret3;
    while (i <= ret1.L) {
       if ((read_bytes = msgrcv(id2, &ret3, sizeof(RepReturn2) - sizeof(long), getpid(), 0)) <= 0)
          syserr(errno, "msgrcv ret3");
       for (j = 1; j <= ret3.K; j++){
          sum = sum + ret3.list[j];
       }
       printf("%d %lld ", i, sum);
       for (j = 1; j <= ret3.K; j++){
          printf("%lld", ret3.list[j]);
          if (j < ret3.K)
             printf(" ");
       }
       printf("\n");
       sum = 0;
       i++;
    }
  }
  return 0;
 }