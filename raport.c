#define _XOPEN_SOURCE

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
/*
Raport

Program raport uruchamia się poleceniem

./raport
lub
./raport l
gdzie 0 < l ≤ L jest opcjonalnym parametrem oznaczającym numer listy.

Klient wysyła na serwer odpowiednie zapytanie i odbiera dane pozwalające mu na wypisanie raportu. Na standardowe wyjście wypisywany 
jest raport o następującym formacie:

Przetworzonych komisji: x / K
Uprawnionych do głosowania: y
Głosów ważnych: z
Głosów nieważnych: v
Frekwencja : (z + v) / y * 100%
Wyniki poszczególnych list:
a następnie wiersze z wynikami poszczególnych list zawierające liczby rozdzielone pojedynczymi spacjami:
numer listy
suma liczby głosów na tę listę
K liczb z sumą głosów na poszczególnych kandydatów w kolejności od 1 do K (także zera).
W przypadku podania parametru l wypisywany jest tylko jeden wiersz z wynikami dla jednej listy. 
W przypadku braku parametru – L wierszy z wynikami list od 1 do L.

Statystyki na początku raportu odnoszą się zawsze do wyników zbiorczych.

Wyniki wypisywane w raporcie muszą być spójnym obrazem danych uwzględniającym pewną liczbę kompletnych wyników nadesłanych przez komisje,
 przy czym klienci jednego rodzaju nie mogą zagłodzić klientów drugiego rodzaju (w ścisłym sensie w jedną stronę zagłodzenie 
 nam nie grozi, gdyż komisji jest skończona liczba, ale chodzi o to, by żądania nie czekały dłużej niż jest to konieczne).

typedef struct {
  long   mesg_type;  
  pid_t  pid;
  int    l;   
} RepMsg;

typedef struct {
  long   mesg_type;   
  int    x;
  int    K;
  int    y;
  int    z;
  int    v;
} RepReturn1;

typedef struct {
   long mesg_type;
   pid_t pid;
   //int K;
   int l;
   int l_sum;
   int list[MAX_K];
} RepReturn2;
*/

 int main (int argc, char *argv[])
 {
   int l = (argc < 2) ? 0 : atoi(argv[1]);
  // int K, L;
 //  int y, z, v;
   double frec;
   int id, id2;

//   printf("%d to je moj pid\n", getpid());
 //  fflush(stdout);
   if ((id = msgget(M_REP_KEY, 0)) == -1)
    syserr(errno, "msgget M_REP_KEY");
   if ((id2 = msgget(M_REP_KEY_2, 0)) == -1)
    syserr(errno, "msgget M_REP_KEY_2");

   /* send init msg to server */
   RepMsg mesg = {INIT_REP_KEY, getpid(), l};
   if (msgsnd(id, (char *) &mesg, sizeof(RepMsg) - sizeof(long), 0) != 0)
      syserr(errno, "msgsnd init msg %d", l);
   int read_bytes;
   RepReturn1 ret1;
   if ((read_bytes = msgrcv(id2, &ret1, sizeof(RepReturn1) - sizeof(long), getpid(), 0)) <= 0)
      syserr(errno, "msgrcv init");
   //L = ret1.L;
   //K = ret1.K;
   printf("Przetworzonych komisji: %d / %d\n", ret1.x, ret1.K);
   printf("Uprawnionych do głosowania: %lld\n", ret1.y);
   printf("Głosów ważnych: %lld\n", ret1.z);
   printf("Głosów nieważnych: %lld\n", ret1.v);
   frec = ((double)ret1.z + (double)ret1.v)/(double)ret1.y * 100;
   printf("Frekwencja: %.2f%%\n", frec);
   printf("Wyniki poszczególnych list:\n");

 
   if (l != 0) {
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
         printf("%d", ret2.list[j]);
         if (j < ret2.K)
            printf(" ");
      }
      printf("\n");
   }
   else {
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
            printf("%d", ret3.list[j]);
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