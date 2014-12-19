#define _XOPEN_SOURCE
/*
Serwer

Serwer uruchamia się poleceniem

./serwer L K M
przekazując wartość parametrów opisanych powyżej.

Serwer tworzy kolejki potrzebne do komunikacji i oczekuje na zgłoszenia klientów.

Obsługą klientów obu rodzajów mają zajmować się wątki tworzone i usuwane dynamicznie przez serwer,
 dla każdego klienta jeden wątek, ten sam przez cały czas komunikacji z danym klientem.
  Jedynie pierwsze komunikaty przysyłane przez poszczególnych klientów mogą być odbierane przez jeden wspólny wątek.

Jednocześnie może trwać komunikacja z wieloma klientami. 
Należy przy tym zapewnić adekwatną synchronizację i ochronę danych na poziomie pojedynczych wartości przechowywanych przez serwer.
 Proszę jednak zwrócić uwagę na generowanie raportu, które wymaga dostępu do spójnego obrazu danych.

Praca serwera może zostać przerwana sygnałem SIGINT (Ctrl+C). W takiej sytuacji serwer powinien zwolnić zajmowane zasoby systemowe 
(w szczególności kolejki IPC) i zakończyć się. Klienci także mają się w takiej sytuacji zakończyć, mogą w sposób "wyjątkowy". 
W zależności od implementacji dopuszczalne jest natychmiastowe zakończenie serwera lub obsłużenie bieżących komunikatów; 
serwer nie powinien natomiast odbierać już nowych komunikatów.

Serwer powinien działać potencjalnie dowolnie długo i być w stanie obsłużyć nieograniczenie wielu klientów w czasie swojego życia.

trzeba umieć wysyłać

Przetworzonych komisji: x / K
Uprawnionych do głosowania: y
Głosów ważnych: z
Głosów nieważnych: v
Frekwencja : (z + v) / y * 100%

*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <pthread.h>
#include <errno.h>
#include "err.h"
#include "common.h"

#define QUEUES_NUMBER 4

typedef struct {
   int m;
   int i;   // eligable for voting
   int j;   // all valid and invalid votes
   int w;   // number of entries
   int n;   // all valid votes
   int candidates[MAX_L][MAX_K];
} committee;

typedef struct node{
  pid_t p;
  pthread_t t;
  struct node *next;
} node;

void init(node ** n, pid_t p, pthread_t t)
{
   *n = malloc(sizeof(node));
   (*n)->p = p;
   (*n)->t = t;
   (*n)->next = 0;
}

void add(node ** end, pid_t p, pthread_t t)
{
   (*end)->next = malloc(sizeof(node));
   (*end)->next->p = p;
   (*end)->next->t = t;
   (*end)->next->next = 0;
   (*end) = (*end)->next;
}

void delete(node ** n, pid_t p)
{
   node * i, * tmp;
   i = *n;
   if (i != 0){
      if (i->p == p){
         *n = (*n)->next;
         free(i);
      }
      else {
         while ((i->next != 0) && (i->next->p != p)){
            i = i->next;
         } 
         if (i->next != 0) {
            tmp = i->next;
            i->next = i->next->next;
            free(tmp);
         }
      }
   }
}

void delete_all(node ** n)
{
   node * tmp;
   while (*n != 0) {
      tmp = *n;
      *n = (*n)->next;
      free(tmp);
   }
   free(*n);
}

void delete_err(node ** n)
{
   printf("deleting reports\n");
   fflush(stdout);
   node * tmp;
   int err;
   while (*n != 0) {
      tmp = *n;
      *n = (*n)->next;
      if ((err = pthread_cancel(tmp->t)) != 0)
         syserr(err, "pthread_cancel %d", tmp->t);
      kill ((pid_t)tmp->p, SIGKILL);
      free(tmp);
   }
   free(*n);
}

/* data */
int counted[MAX_L][MAX_K]; /* ready data for reports */
int committees[MAX_M]; /* 0 if not yet used, -1 if used and ended, [pthread] if in progress */
node * reports = 0; /* list to store working report processes */
node * end = 0; /* end of this list */
node * waitnig_com_head = 0; /* list for new committees, if there are > 100 committees */
node * waiting_com_end = 0; /* end of this list */

int M;
int K;
int L;
int done = 0;
int working_threads = 0; /* counter for working committee threads */
int wannabe_threads = 0;   /* counter for waiting committees, which are not yet threads */

long long eligible = 0;
long long valid = 0;
long long invalid = 0;

/* threads */
pthread_attr_t attr;
pthread_attr_t attr_rep;
pthread_t threads[MAX_M];

/* synchronisation */
pthread_mutex_t mutex; /* mutex ensuring exclusive access to votes */
pthread_mutexattr_t attr_mutex;
pthread_mutex_t rep_mutex; /* mutex ensuring exclusive access to reports list */
pthread_mutexattr_t rep_attr;
pthread_mutex_t com_mutex; /* mutex allowing only one at a time committee to write */
pthread_mutexattr_t com_attr;
//pthread_mutex_t server_mutex; /* mutex allowing only 100 threads at a time */
//pthread_mutexattr_t server_attr;

pthread_cond_t rep_wait;
pthread_cond_t com_wait;

int all_rep = 0;
int all_com = 0;
int reading_rep = 0;
int writing_com = 0;

int msg_queues[QUEUES_NUMBER];

void *handle_committee (void *data) 
{
   int finished = 0;
   int m = 0;
   pid_t mesg_type = *(pid_t *)data;
   //printf("committee nr %d\n", mesg_type);
   //fflush(stdout);
   free ((pid_t *)data);
   int err;
   int oldtype;
   if ((err = pthread_setcanceltype (PTHREAD_CANCEL_ASYNCHRONOUS, &oldtype)) != 0)
      syserr (err, "setcanceltype");
   
   /* send successful connection msg */
   ComReturn com_r;
   memset(&com_r, 0, sizeof(ComReturn));
   com_r.mesg_type = MAX_PID + mesg_type;
   com_r.m = 0;
   com_r.sum_n = -1;
   com_r.w = -1;
   if (msgsnd(msg_queues[3], (void *) &com_r, sizeof(ComReturn) - sizeof(long), 0) != 0)
      syserr(errno, "msgsnd new committee %d", (int)mesg_type);
   // rób swoje dalej
   ComMsg mesg = {0,0,0,0,0,0,0,0};
   int read_bytes = 0;
   committee c = {0,0,0,0,0};
   memset(c.candidates, 0, sizeof(c.candidates));
   while (finished == 0) {
      if ((read_bytes = msgrcv(msg_queues[2], &mesg, sizeof(ComMsg) - sizeof(long), mesg_type, 0)) <= 0)
         syserr(errno, "msgrcv handle_committee");
      /* first msg with i & j & m */     
      if ((mesg.l == 0) && (m == 0)){
         c.i = mesg.i;
         c.j = mesg.j;
         m = mesg.m;
      }
      else {
         c.w = c.w + 1;
         c.n = c.n + mesg.n;
         c.candidates[mesg.l][mesg.k] = mesg.n;
      }
      if (mesg.m == -1)
         finished = 1;
   }

   /* get mutex and update data */
   if ((err = pthread_mutex_lock(&mutex)) != 0)
      syserr (err, "lock failed");
   pthread_cleanup_push(pthread_mutex_unlock, &mutex);
 
   all_com++;
   if ((writing_com > 0) || (reading_rep > 0)) {
      do {
         printf("zaśnij komisjo\n");
         fflush(stdout);
         if ((err = pthread_cond_wait(&com_wait, &mutex)) != 0)
            syserr (err, "com_wait wait failed");  
      } while ((writing_com > 0) || (reading_rep > 0));
   }
   writing_com++;
   if ((err = pthread_mutex_unlock(&mutex)) != 0)
      syserr (err, "unlock failed");
   pthread_cleanup_pop(0);
 
   /* figth for the write */
   if ((err = pthread_mutex_lock(&com_mutex)) != 0)
      syserr (err, "lock com_mutex failed");
   pthread_cleanup_push(pthread_mutex_unlock, &com_mutex);

   /* copy data */
   int k,l;
   for (l = 1; l <= L; l++) {
      for (k = 1; k <= K; k++) {
         if (c.candidates[l][k] != 0)
            counted[l][k] = counted[l][k] + c.candidates[l][k];
      }
   }
   eligible = eligible + c.i;
   valid = valid + c.n;
   invalid = invalid + (c.j - c.n);
   done++;

   if ((err = pthread_mutex_unlock(&com_mutex)) != 0)
      syserr (err, "unlock com_mutex failed");
   pthread_cleanup_pop(0);

   /* wake reports */
   if ((err = pthread_mutex_lock(&mutex)) != 0)
      syserr (err, "lock failed");
   pthread_cleanup_push(pthread_mutex_unlock, &mutex);
   writing_com--;
   all_com--;
   if (writing_com == 0) {
      if (all_rep > 0) {
         if ((err = pthread_cond_broadcast(&rep_wait)) != 0)
            syserr (err, "pthread_cond broadcast rep_wait");
      }
      else {
         printf("nie ma raportów, wiec budze komisje\n");
         fflush(stdout);
         if ((err = pthread_cond_signal(&com_wait)) != 0)
            syserr (err, "pthread_cond signal com_wait");  
      }
   }
   if ((err = pthread_mutex_unlock(&mutex)) != 0)
      syserr (err, "unlock failed");
   pthread_cleanup_pop(0);


   /* send ending message to committee */
   ComReturn com_ret = {mesg_type, c.m, c.n, c.w};
   if ((err = msgsnd(msg_queues[3], (void *) &com_ret, sizeof(ComReturn) - sizeof(long), 0)) != 0)
      syserr(err, "msgsnd return msg from server to committee %d\n", com_ret.m);

   committees[m] = -1;  
   
   /*if ((err = pthread_mutex_lock(&server_mutex)) != 0)
      syserr (err, "lock failed");
   pthread_cleanup_push(pthread_mutex_unlock, &server_mutex);

   working_threads--;


   if ((err = pthread_mutex_unlock(&server_mutex)) != 0)
      syserr (err, "unlock failed");
   pthread_cleanup_pop(0);
   */
   printf("Committee nr %d ending...\n", m);
   fflush(stdout);

   return 0;
}

void *handle_report (void *data)
{
   RepMsg init = *(RepMsg *)data;
   free((RepMsg *)data);
   pid_t pid = init.pid;
   //printf("%d pid raportu\n", pid);
   int l = init.l;
   int err;
   int oldtype;
   if ((err = pthread_setcanceltype (PTHREAD_CANCEL_ASYNCHRONOUS, &oldtype)) != 0)
      syserr (err, "setcanceltype");

   if ((err = pthread_mutex_lock(&mutex)) != 0)
      syserr (err, "lock failed");
   pthread_cleanup_push(pthread_mutex_unlock, &mutex);
   all_rep++;
   if (writing_com > 0) {
      do {
         if ((err = pthread_cond_wait(&rep_wait, &mutex)) != 0)
            syserr (err, "rep_wait wait failed");  
      } while (writing_com > 0);
   }
   reading_rep++;
   if ((err = pthread_mutex_unlock(&mutex)) != 0)
      syserr (err, "unlock failed");
   pthread_cleanup_pop(0);
   RepReturn1 rep_ret = {pid, L, done, M, eligible, valid, invalid};
   if (msgsnd(msg_queues[1], (void *) &rep_ret, sizeof(RepReturn1) - sizeof(long), 0) != 0)
      syserr(errno, "msgsnd return msg from server to report %d\n", pid);
   if (l == 0) {
      // send all lists
      int i, j;
      i = 1;
      while (i <= L) {
         RepReturn2 rep_ret_i = {pid, i, K};   
         memset(rep_ret_i.list, 0, sizeof(int)*MAX_K);
         for (j = 1; j <= K; j++){
            rep_ret_i.list[j] = counted[i][j];
         }
         if (msgsnd(msg_queues[1], (void *) &rep_ret_i, sizeof(RepReturn2) - sizeof(long), 0) != 0)
            syserr(errno, "msgsnd return msg from server to report %d\n", pid);
         memset(rep_ret_i.list, 0, sizeof(int)*MAX_K);
         i++;
      }
   }
   else {
      //send just list nr l
      int j;
      RepReturn2 rep_ret_l = {pid, l, K};
      memset(rep_ret_l.list, 0, sizeof(int)*MAX_K);
      for (j = 1; j <= K; j++){
         rep_ret_l.list[j] = counted[l][j];
      }
      if (msgsnd(msg_queues[1], (void *) &rep_ret_l, sizeof(RepReturn2) - sizeof(long), 0) != 0)
         syserr(errno, "msgsnd return msg from server to report %d\n", pid);
   }
   
   // delete report from reports list
   if ((err = pthread_mutex_lock(&rep_mutex)) != 0)
      syserr (err, "lock failed");
   pthread_cleanup_push(pthread_mutex_unlock, &rep_mutex);
   delete(&reports, pid);
   if ((err = pthread_mutex_unlock(&rep_mutex)) != 0)
      syserr (err, "unlock failed");
   pthread_cleanup_pop(0);

   // wake committees
   if ((err = pthread_mutex_lock(&mutex)) != 0)
      syserr (err, "lock failed");
   pthread_cleanup_push(pthread_mutex_unlock, &mutex);
   reading_rep--;
   all_rep--;
   printf("konczy sie raport\n");
   fflush(stdin);
   if (reading_rep == 0) {
      if ((err = pthread_cond_broadcast(&com_wait)) != 0)
         syserr (err, "pthread_cond broadcast com_wait");
   }
   if ((err = pthread_mutex_unlock(&mutex)) != 0)
      syserr (err, "unlock failed");
   pthread_cleanup_pop(0);

   //printf("Report ending...\n");

  // free((RepMsg *)data);
   return 0;
}

void exit_server(int sig) 
{
   printf("Caught interrupt signal. Exiting...\n");
   //send kill signals to reports and committees
   int i;
   int err;
   for (i = 1; i < MAX_M; i++){
      if (committees[i] > 0) {
     //    printf("usuwam komisje nr %d\n", i);
         if ((err = pthread_cancel(threads[i])) != 0)
            syserr(err, "pthread_cancel %d", i);
         kill ((pid_t)committees[i], SIGKILL);
      }
   }
   delete_err(&reports);
  // delete_err(&waitnig_com_head);
   for (i = 0; i < QUEUES_NUMBER; i++){
      if (msg_queues[i] != -1)
         if (msgctl(msg_queues[i], IPC_RMID, 0) == -1)
            syserr(errno, "msgctl msg_queues[%d]", i);      
   }
   if ((err = pthread_attr_destroy (&attr)) != 0)
      syserr (err, "attrdestroy");
   if ((err = pthread_attr_destroy (&attr_rep)) != 0)
      syserr (err, "attr rep destroy");
   if ((err = pthread_cond_destroy (&rep_wait)) != 0)
      syserr (err, "cond destroy rep_wait failed");
   if ((err = pthread_cond_destroy (&com_wait)) != 0)
      syserr (err, "cond destroy com_wait failed");
   if ((err = pthread_mutex_destroy (&mutex)) != 0)
      syserr (err, "mutex destroy failed");
   if ((err = pthread_mutex_destroy (&rep_mutex)) != 0)
      syserr (err, "rep_mutex destroy failed");
   if ((err = pthread_mutex_destroy (&com_mutex)) != 0)
      syserr (err, "com_mutex destroy failed");  
   //if ((err = pthread_mutex_destroy (&server_mutex)) != 0)
   //  syserr (err, "server_mutex destroy failed");  
   exit(0);
}

int main(int argc, char *argv[])
{
   if (signal(SIGINT,  exit_server) == SIG_ERR)
    syserr(errno, "signal");
    if (signal(SIGSEGV,  exit_server) == SIG_ERR)
    syserr(errno, "signal");

   memset(counted, 0, MAX_L*MAX_K*sizeof (int));
   memset(committees, 0, MAX_M*sizeof(int));
   memset(threads, 0, MAX_M*sizeof(pthread_t));

   L = atoi(argv[1]);
   K = atoi(argv[2]);
   M = atoi(argv[3]);
   //printf("There are %d lists, %d candidates, %d committees.\n", L, K, M);
   
   /* stworzenie kolejek komunikatów */

   if ((msg_queues[0] = msgget(M_REP_KEY, 0666 | IPC_CREAT | IPC_EXCL)) == -1)
    syserr(errno, "msgget 0");
   if ((msg_queues[1] = msgget(M_REP_KEY_2, 0666 | IPC_CREAT | IPC_EXCL)) == -1)
    syserr(errno, "msgget 1");
   if ((msg_queues[2] = msgget(M_COM_KEY, 0666 | IPC_CREAT | IPC_EXCL)) == -1)
    syserr(errno, "msgget 2");
   if ((msg_queues[3] = msgget(M_COM_KEY_2, 0666 | IPC_CREAT | IPC_EXCL)) == -1)
    syserr(errno, "msgget 3");

   /* init the mutexes and conditions */

   int err;
  
   if ((err = pthread_mutex_init(&mutex, 0) != 0))
    syserr (err, "mutex init failed");
   if ((err = pthread_mutex_init(&rep_mutex, 0) != 0))
    syserr (err, "rep_mutex init failed");
   if ((err = pthread_mutex_init(&com_mutex, 0) != 0))
    syserr (err, "com_mutex init failed");
  // if ((err = pthread_mutex_init(&server_mutex, 0) != 0))
   // syserr (err, "server_mutex init failed");

   if ((err = pthread_cond_init(&rep_wait, 0)) != 0)
      syserr (err, "cond init rep_wait failed");
   if ((err = pthread_cond_init(&com_wait, 0)) != 0)
      syserr (err, "cond init com_wait failed");

   /* start receiving committees and reports */

   int com_number = 0;
   int rep_number = 0;
   struct msqid_ds com_buf, rep_buf;
   int read_bytes = 0;
   while (1) {
      ComMsg com = {0,0,0,0,0,0,0,0};
      RepMsg rep = {0,0,0};
      if (msgctl(msg_queues[2], IPC_STAT, &com_buf) == -1)
           syserr(errno, "msgctl check com_queue if empty");
      com_number = com_buf.msg_qnum;

      if (com_number != 0) {
         /* create a thread for the incoming committee */
         if ((read_bytes = msgrcv(msg_queues[2], &com, sizeof(ComMsg) - sizeof(long), INIT_COM_KEY, IPC_NOWAIT)) <= 0)
            if (errno != ENOMSG)
               syserr(errno, "msgrcv committee");
         // zczytaj dane z kolejki, czy to aby na pewno nowa komisja jest
         //utworz watek obslugujacy komisje, o ile jest to nowa komisja, poznanie się
         if (com.pid != 0) {
            //wrzuc pod server_mutex
            /*if ((err = pthread_mutex_lock(&server_mutex)) != 0)
               syserr( err, "lock failed");
            pthread_cleanup_push(pthread_mutex_unlock, &server_mutex);
         */
            if ((com.m < MAX_M) && (committees[com.m] == 0)) {
               //printf("new committee nr %d %d\n", com.pid, com.m);   
               // committee number com.m has never contacted server before
               if ((err = pthread_attr_init(&attr)) != 0 )
                  syserr(err, "attrinit committee nr %d", com.m);
               if ((err = pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED)) != 0)
                  syserr(err, "setdetach committee nr %d", com.m);
               pid_t *pid = (pid_t *) malloc (sizeof(pid_t));
               if (pid == NULL)
                  syserr (err, "malloc committee nr %d", com.m);
               *pid = com.pid;
           //    if (working_threads < MAX_T) {
                  committees[com.m] = com.pid;
                  if ((err = pthread_create (threads + com.m, &attr, handle_committee, (void *)pid)) != 0)
                     syserr (err, "create pthread for committee nr %d", com.m); 
             //     working_threads++;
               /*}
               else {
                  wannabe_threads++;
                  if (waitnig_com_head == 0) {
                     init(&waitnig_com_head, com.p, 0);
                     waiting_com_end = waiting_com_head;
                  }
                  else {
                     add(&waitnig_com_end, com.p, 0);
                  }
               }
               if ((err = pthread_mutex_unlock(&server_mutex)) != 0)
                  syserr (err, "server_mutex unlock failed");
               pthread_cleanup_pop(0);  
            */
            }
            else {
               // wyślij do danej komisji komunikat błędu
               ComReturn com_r;
               com_r.mesg_type = MAX_PID + com.pid;
               com_r.m = -1;
               com_r.sum_n = -1;
               com_r.w = -1;
               printf("Sent access denied nr %ld\n", com_r.mesg_type - MAX_PID);
               fflush(stdout);

               if (msgsnd(msg_queues[3], (void *) &com_r, sizeof(ComReturn) - sizeof(long), 0) != 0)
                  syserr(errno, "msgsnd another committee with nr %d\n", com.m);
            }
         }
      }
      if (msgctl(msg_queues[0], IPC_STAT, &rep_buf) == -1)
           syserr(errno, "msgctl check rep_queue if empty");
      rep_number = rep_buf.msg_qnum;
      
      if (rep_number != 0) {
         // create a thread for the incoming report
         int read_bytes2;
         if ((read_bytes2 = msgrcv(msg_queues[0], &rep, sizeof(RepMsg) - sizeof(long), INIT_REP_KEY, 0)) <= 0)
            syserr(errno, "msgrcv reportx");
         //printf("new report\n");
         int err2;
         //utworz watek
         if ((err2 = pthread_attr_init(&attr_rep)) != 0 )
            syserr(err, "attrinit report pid %d", rep.pid);
         if ((err2 = pthread_attr_setdetachstate(&attr_rep, PTHREAD_CREATE_DETACHED)) != 0)
            syserr(err, "setdetach committee nr %d", rep.pid);
         RepMsg * report = (RepMsg *) malloc (sizeof(RepMsg));
         *report = rep;
         // użyj mutexa na raporty
         if ((err = pthread_mutex_lock(&rep_mutex)) != 0)
            syserr( err, "lock failed");
         pthread_cleanup_push(pthread_mutex_unlock, &rep_mutex);
         if (reports == 0){
            init(&reports, rep.pid, 0);
            end = reports;
            //printf("pid raportu %d %d\n", reports->p, rep.pid);
         }
         else {
            add(&end, rep.pid, 0);
         }
         if ((err2 = pthread_create (&(end->t), &attr_rep, handle_report, (void *) report)) != 0)
            syserr (err, "create pthread for report with pid %d", rep.pid); 
         if ((err = pthread_mutex_unlock(&rep_mutex)) != 0)
            syserr (err, "unlock failed");
         pthread_cleanup_pop(0);
         
      }


   }


   return 0;
}