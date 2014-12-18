#define _XOPEN_SOURCE
/*
Komisja

Program komisja uruchamia się poleceniem

./komisja m
gdzie 0 < m ≤ M oznacza numer komisji.

Program ze swojego standardowego wejścia czyta:

jeden wiersz, w którym znajdują się 2 liczby całkowite 0 < i < 10000 oraz 0 ≤ j < 10000, rozdzielone pojedynczą spacją,
 które oznaczają: i – liczbę osób uprawnionych do głosowania w danym lokalu, j – liczbę osób, które oddały głos w danym lokalu (łącznie z nieważnymi);
pewną nieznaną z góry liczbę w wierszy (nie więcej niż L * K), w których znajdują się 3 dodatnie liczby całkowite:
 l k n oznaczające l – numer listy, k – numer kandydata na liście; n < 10000 – liczbę głosów oddanych na danego kandydata w danym lokalu.
Można założyć, że pary (l, k) są unikalne, nie można jednak założyć niczego o ich kolejności. Suma wszystkich głosów (∑n) 
oznacza jednocześnie liczbę głosów ważnych.

Program używając kolejek IPC powinien wysłać te dane na serwer. Po zakończeniu wejścia (koniec pliku wejściowego lub Ctrl+D w konsoli)
program powinien wysłać komunikat informujący o zakończeniu danych, a serwer powinien odpowiedzieć potwierdzeniem zawierającym co najmniej
liczby w oraz ∑n obliczone przez serwer. Na zakończenie program komisja powinien wypisać podsumowanie:

Przetworzonych wpisów: w
Uprawnionych do głosowania: i
Głosów ważnych: ∑n
Głosów nieważnych: j
Frekwencja w lokalu: (j + ∑n) / i * 100%
Serwer powinien kontrolować które komisje przysłały już swoje wyniki. Dla danej komisji m dopuszczalne jest tylko jednorazowe 
połączenie w celu przesłania wyników. Jeśli serwer już odebrał lub jest w trakcie odbierania wyników dla danego m, próby połączenia kolejnego 
programu komisja z takim samym m powinny zostać odrzucone (można w tym celu użyć odpowiedniego komunikatu), a program komisja powinien się
 zakończyć z komunikatem Odmowa dostępu.
*/
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
   printf("Access denied. Exiting...\n");
   exit(0);
}

int main (int argc, char *argv[])
{
   if (signal(SIGTERM,  access_denied) == SIG_ERR)
     syserr(errno, "signal");

   int m;
   int id, id2;
   m = atoi(argv[1]);

 //  printf("%d\n", getpid());

   if ((id = msgget(M_COM_KEY, 0)) == -1)
    syserr(errno, "msgget M_COM_KEY");
   if ((id2 = msgget(M_COM_KEY_2, 0)) == -1)
    syserr(errno, "msgget M_COM_KEY_2");

   /* send init msg to server */
   ComMsg mesg = {INIT_COM_KEY, getpid(), m, 0, 0, 0, 0, 0};
   if (msgsnd(id, (char *) &mesg, sizeof(ComMsg) - sizeof(long), 0) != 0)
      syserr(errno, "msgsnd init msg %d", m);
   int read_bytes;
   ComReturn com = {0,0,0,0};
   if ((read_bytes = msgrcv(id2, &com, sizeof(ComReturn) - sizeof(long), 10*getpid(), 0)) <= 0)
      syserr(errno, "msgrcv init");
   if (com.m == -1){
      //zakończ się z komunikatem odmowa dostępu
      if (raise(SIGTERM) == -1)
         syserr(errno, "raise SIGTERM");
   }

   int x = 0;
   int l, k, n;
   int i, j;   
   x = scanf("%d %d", &i, &j);
   //printf("i: %d, j: %d\n", i, j);
   x = 0;
   ComMsg mesg1 = {getpid(), getpid(), m, 0, 0, 0, i, j};
      if (msgsnd(id, (char *) &mesg1, sizeof(ComMsg) - sizeof(long), 0) != 0)
         syserr(errno, "msgsnd %d", x);
   while ((x < MAX_L*MAX_K) && (scanf("%d %d %d", &l, &k, &n) != EOF)) {
   //   printf("%d %d %d %d\n",x, l, k, n);
      ComMsg mesg = {getpid(), getpid(), m, l, k, n, i, j};
      if (msgsnd(id, (char *) &mesg, sizeof(ComMsg) - sizeof(long), 0) != 0)
         syserr(errno, "msgsnd %d", x);
      x++;   
   }
   ComMsg end = {getpid(), getpid(), -1, 0, 0, 0, i, j};
   if (msgsnd(id, (char *) &end, sizeof(ComMsg) - sizeof(long), 0) != 0)
         syserr(errno, "msgsnd end msg");
   ComReturn ret = {0,0,0,0};
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