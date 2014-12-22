/* Maja Zalewska  */
/* nr 336088      */
/* Zadanie 2, SO  */

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
#include "list.h"

#define QUEUES_NUMBER 4

typedef struct {
   int m;
   int i;   /* eligable for voting */
   int j;   /* all valid and invalid votes */
   int w;   /* number of entries */
   int n;   /* all valid votes */
   int candidates[MAX_L][MAX_K];
} committee;

/* data */
long long counted[MAX_L][MAX_K];    /* ready data for reports */
int committees[MAX_M];              /* 0 if not yet used, -1 if used and ended, [pthread] if in progress */
node * reports = 0;                 /* list to store working report processes */
node * end = 0;                     /* end of this list */

int M;            /* number of all committees */
int K;            /* number of candidates per list */
int L;            /* number of all lists */
int done = 0;     /* number of done committees */

long long eligible = 0;    /* number of eligible voters */
long long valid = 0;       /* number of valid votes */
long long invalid = 0;     /* number of invalid votes */

/* threads */
pthread_attr_t attr;
pthread_attr_t attr_rep;
pthread_t threads[MAX_M];  /* array with threads ids */

/* synchronisation */
pthread_mutex_t rep_mutex; /* mutex ensuring exclusive access to reports list */
pthread_mutexattr_t rep_attr;

pthread_rwlock_t rwlp;     /* mutex solving readers/writers problem */
pthread_rwlockattr_t rwlp_attr;

int msg_queues[QUEUES_NUMBER];

void *handle_committee (void *data) 
{
   int finished = 0;
   int m = 0;
   pid_t mesg_type = *(pid_t *)data;
   free ((pid_t *)data);
   int err;
   int oldtype;
   if ((err = pthread_setcanceltype (PTHREAD_CANCEL_ASYNCHRONOUS, &oldtype)) != 0)
      syserr (err, "setcanceltype");
   
   /* send successful connection msg */
   ComReturn com_r;
   memset(&com_r, 0, sizeof(ComReturn));
   com_r.mesg_type = MAX_PID + mesg_type;
   com_r.sum_n = 0;
   com_r.w = 0;
   if (msgsnd(msg_queues[3], (void *) &com_r, sizeof(ComReturn) - sizeof(long), 0) != 0)
      syserr(errno, "msgsnd new committee %d", (int)mesg_type);
   /* receive data from committee */
   ComMsg mesg = {0,0,0,0,0};
   int read_bytes = 0;
   committee c = {0,0,0,0,0};
   memset(c.candidates, 0, sizeof(c.candidates));
   while (finished == 0) {
      if ((read_bytes = msgrcv(msg_queues[2], &mesg, sizeof(ComMsg) - sizeof(long), mesg_type, 0)) <= 0)
         syserr(errno, "msgrcv handle_committee");
      /* first msg with i & j & m */     
      if ((mesg.l == 0) && (m == 0)){
         c.i = mesg.k;
         c.j = mesg.n;
         m = mesg.m;
      }
      else if (mesg.m == -1) {
         finished = 1;
      }
      else {
         c.w = c.w + 1;
         c.n = c.n + mesg.n;
         c.candidates[mesg.l][mesg.k] = mesg.n;
      }
      memset(&mesg, 0, sizeof(ComMsg));
   }

   /* get mutex and update data */
   if ((err = pthread_rwlock_wrlock(&rwlp)) != 0)
      syserr (err, "wrlock failed");
   pthread_cleanup_push(pthread_rwlock_unlock, &rwlp);

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

   if ((err = pthread_rwlock_unlock(&rwlp)) != 0)
      syserr (err, "wrlock failed");
   pthread_cleanup_pop(0);
   
   /* send ending message to committee */
   ComReturn com_ret = {mesg_type, c.n, c.w};
   if ((err = msgsnd(msg_queues[3], (void *) &com_ret, sizeof(ComReturn) - sizeof(long), 0)) != 0)
      syserr(err, "msgsnd return msg from server to committee %d\n", m);

   committees[m] = -1;  

   return 0;
}

void *handle_report (void *data)
{
   InitMsg init = *(InitMsg *)data;
   free((InitMsg *)data);
   pid_t pid = init.pid;
   int l = init.ml;
   int err;
   int oldtype;
   if ((err = pthread_setcanceltype (PTHREAD_CANCEL_ASYNCHRONOUS, &oldtype)) != 0)
      syserr (err, "setcanceltype");

   if ((err = pthread_rwlock_rdlock(&rwlp)) != 0)
      syserr (err, "lock failed");
   pthread_cleanup_push(pthread_rwlock_unlock, &rwlp);

   RepReturn1 rep_ret = {pid, L, done, M, eligible, valid, invalid};
   if (msgsnd(msg_queues[1], (void *) &rep_ret, sizeof(RepReturn1) - sizeof(long), 0) != 0)
      syserr(errno, "msgsnd return msg from server to report %d\n", pid);
   if (l == 0) {
      /* send all lists */
      int i, j;
      i = 1;
      while (i <= L) {
         RepReturn2 rep_ret_i = {pid, i, K};   
         memset(rep_ret_i.list, 0, sizeof(long long)*MAX_K);
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
      /* send just list nr l */
      int j;
      RepReturn2 rep_ret_l = {pid, l, K};
      memset(rep_ret_l.list, 0, sizeof(long long)*MAX_K);
      for (j = 1; j <= K; j++){
         rep_ret_l.list[j] = counted[l][j];
      }
      if (msgsnd(msg_queues[1], (void *) &rep_ret_l, sizeof(RepReturn2) - sizeof(long), 0) != 0)
         syserr(errno, "msgsnd return msg from server to report %d\n", pid);
   }

   if ((err = pthread_rwlock_unlock(&rwlp)) != 0)
      syserr (err, "unlock failed");
   pthread_cleanup_pop(0);

   /* delete report from reports list */
   if ((err = pthread_mutex_lock(&rep_mutex)) != 0)
      syserr (err, "lock failed");
   pthread_cleanup_push(pthread_mutex_unlock, &rep_mutex);
   delete(&reports, pid);
   if ((err = pthread_mutex_unlock(&rep_mutex)) != 0)
      syserr (err, "unlock failed");
   pthread_cleanup_pop(0);

   return 0;
}

void exit_server(int sig) 
{
   printf("Caught interrupt signal. Exiting...\n");
   /* send kill signals to reports and committees */
   int i;
   int err;
   for (i = 1; i < MAX_M; i++){
      if (committees[i] > 0) {
         if ((err = pthread_cancel(threads[i])) != 0)
            syserr(err, "pthread_cancel %d", i);
         kill ((pid_t)committees[i], SIGKILL);
      }
   }
   delete_err(&reports);
   for (i = 0; i < QUEUES_NUMBER; i++){
      if (msg_queues[i] != -1)
         if (msgctl(msg_queues[i], IPC_RMID, 0) == -1)
            syserr(errno, "msgctl msg_queues[%d]", i);      
   }
   if ((err = pthread_attr_destroy (&attr)) != 0)
      syserr (err, "attrdestroy");
   if ((err = pthread_attr_destroy (&attr_rep)) != 0)
      syserr (err, "attr rep destroy");
   if ((err = pthread_mutex_destroy (&rep_mutex)) != 0)
      syserr (err, "rep_mutex destroy failed");
   if ((err = pthread_rwlock_destroy (&rwlp)) != 0)
     syserr (err, "rwlp destroy failed");  
   exit(0);
}

int main(int argc, char *argv[])
{
   int err;
   /* clean everything nicely up, if SIGINT */
   if (signal(SIGINT,  exit_server) == SIG_ERR)
      syserr(errno, "signal");

   memset(counted, 0, MAX_L*MAX_K*sizeof (int));
   memset(committees, 0, MAX_M*sizeof(int));
   memset(threads, 0, MAX_M*sizeof(pthread_t));

   L = atoi(argv[1]);
   K = atoi(argv[2]);
   M = atoi(argv[3]);
   
   /* init queues */
   if ((msg_queues[0] = msgget(M_ALL_KEY, 0666 | IPC_CREAT | IPC_EXCL)) == -1)
    syserr(errno, "msgget 0");
   if ((msg_queues[1] = msgget(M_REP_KEY_2, 0666 | IPC_CREAT | IPC_EXCL)) == -1)
    syserr(errno, "msgget 1");
   if ((msg_queues[2] = msgget(M_COM_KEY, 0666 | IPC_CREAT | IPC_EXCL)) == -1)
    syserr(errno, "msgget 2");
   if ((msg_queues[3] = msgget(M_COM_KEY_2, 0666 | IPC_CREAT | IPC_EXCL)) == -1)
    syserr(errno, "msgget 3");

   /* init the mutexes and conditions */
   if ((err = pthread_mutex_init(&rep_mutex, 0) != 0))
    syserr (err, "rep_mutex init failed");
   if ((err = pthread_rwlock_init(&rwlp, 0) != 0))
    syserr (err, "rwlp init failed");

   /* start receiving committees and reports */
   int read_bytes = 0;
   while (1) {
      InitMsg ini = {0,0,0,0,0};
         /* create a thread for the incoming committee */
         if ((read_bytes = msgrcv(msg_queues[0], &ini, sizeof(InitMsg) - sizeof(long), INIT_KEY, 0)) <= 0)
            syserr(errno, "msgrcv committee");
         /* if server receives msg from committee */
         if (ini.type == COM_TYPE) {
            if ((ini.ml < MAX_M) && (committees[ini.ml] == 0)) {
               if ((err = pthread_attr_init(&attr)) != 0 )
                  syserr(err, "attrinit committee nr %d", ini.ml);
               if ((err = pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED)) != 0)
                  syserr(err, "setdetach committee nr %d", ini.ml);
               pid_t *pid = (pid_t *) malloc (sizeof(pid_t));
               if (pid == NULL)
                  syserr (err, "malloc committee nr %d", ini.ml);
               *pid = ini.pid;
               committees[ini.ml] = ini.pid;
               if ((err = pthread_create (threads + ini.ml, &attr, handle_committee, (void *)pid)) != 0)
                  syserr (err, "create pthread for committee nr %d", ini.ml); 
            }
            else {
               ComReturn com_r;
               com_r.mesg_type = MAX_PID + ini.pid;
               com_r.sum_n = -1;
               com_r.w = -1;
               printf("Sent access denied nr %ld\n", com_r.mesg_type - MAX_PID);
               fflush(stdout);

               if (msgsnd(msg_queues[3], (void *) &com_r, sizeof(ComReturn) - sizeof(long), 0) != 0)
                  syserr(errno, "msgsnd another committee with nr %d\n", ini.ml);
            }
         }
         /* if server receives msg from report */
         else {
            /* create a thread for the incoming report */
            if ((err = pthread_attr_init(&attr_rep)) != 0 )
               syserr(err, "attrinit report pid %d", ini.pid);
            if ((err = pthread_attr_setdetachstate(&attr_rep, PTHREAD_CREATE_DETACHED)) != 0)
               syserr(err, "setdetach committee nr %d", ini.pid);
            InitMsg * report = (InitMsg *) malloc (sizeof(InitMsg));
            *report = ini;
            /* use mutex for accesing reports list */
            if ((err = pthread_mutex_lock(&rep_mutex)) != 0)
               syserr( err, "lock failed");
            pthread_cleanup_push(pthread_mutex_unlock, &rep_mutex);
            if (reports == 0){
               init(&reports, ini.pid, 0);
               end = reports;
            }
            else {
               add(&end, ini.pid, 0);
            }
            if ((err = pthread_create (&(end->t), &attr_rep, handle_report, (void *) report)) != 0)
               syserr (err, "create pthread for report with pid %d", ini.pid); 
            if ((err = pthread_mutex_unlock(&rep_mutex)) != 0)
               syserr (err, "unlock failed");
            pthread_cleanup_pop(0);  
         }
   }
   return 0;
}