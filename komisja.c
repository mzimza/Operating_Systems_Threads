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

#define INPUT_SIZE 4

void access_denied(int sig){
   printf("Odmowa dostępu\n");
   exit(0);
}

int m;

int main (int argc, char *argv[])
{
   if (signal(SIGTERM,  access_denied) == SIG_ERR)
     syserr(errno, "signal");

  
   int id, id2, id_init;
   m = atoi(argv[1]);

   if ((id_init = msgget(M_ALL_KEY, 0)) == -1)
    syserr(errno, "msgget M_COM_KEY");
   if ((id = msgget(M_COM_KEY, 0)) == -1)
    syserr(errno, "msgget M_COM_KEY");
   if ((id2 = msgget(M_COM_KEY_2, 0)) == -1)
    syserr(errno, "msgget M_COM_KEY_2");

   /* send init msg to server */
   InitMsg ini = {INIT_KEY, getpid(), COM_TYPE, m, 0};
   if (msgsnd(id_init, (char *) &ini, sizeof(InitMsg) - sizeof(long), 0) != 0)
      syserr(errno, "msgsnd init msg %d", m);
   /* wait for response */
   int read_bytes;
   ComReturn com = {0,0,0};
   if ((read_bytes = msgrcv(id2, &com, sizeof(ComReturn) - sizeof(long), MAX_PID + getpid(), 0)) <= 0)
      syserr(errno, "msgrcv init");
   if (com.w == -1){
      /* end with "odmowa dostępu" */
      if (raise(SIGTERM) == -1)
         syserr(errno, "raise SIGTERM");
   }
   int x = 0;
   int l, k, n;
   int i, j;   
   scanf("%d %d", &i, &j);
   ComMsg mesg1 = {getpid(), m, 0, i, j};
   if (msgsnd(id, (char *) &mesg1, sizeof(ComMsg) - sizeof(long), 0) != 0)
         syserr(errno, "msgsnd %d", x);
   while ((x < MAX_L*MAX_K) && (scanf("%d %d %d", &l, &k, &n) != EOF)) {
      if (l == -1)
         sleep(1);
      else {
      ComMsg mesg = {getpid(), m, l, k, n};
      if (msgsnd(id, (char *) &mesg, sizeof(ComMsg) - sizeof(long), 0) != 0)
         syserr(errno, "msgsnd %d", x);
      x++;  
      } 
   }
   /* send end of input mesg */
   ComMsg end = {getpid(), -1, 0, 0, 0};
   if (msgsnd(id, (char *) &end, sizeof(ComMsg) - sizeof(long), 0) != 0)
         syserr(errno, "msgsnd end msg");
   /* read response */
   ComReturn ret = {0,0,0};
   if ((read_bytes = msgrcv(id2, &ret, sizeof(ComReturn) - sizeof(long), getpid(), 0)) <= 0)
      syserr(errno, "msgrcv init");
   printf("Przetworzonych wpisów: %d\n", ret.w);
   printf("Uprawnionych do głosowania: %d\n", i);
   printf("Głosów ważnych: %d\n", ret.sum_n);
   printf("Głosów nieważnych: %d\n", j - ret.sum_n);
   double frec = (double)j/(double)i*100;
   printf("Frekwencja w lokalu: %.2f%%\n", frec);
   return 0;
}