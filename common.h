/* Maja Zalewska  */
/* nr 336088      */
/* Zadanie 2, SO  */

#ifndef COMMON_H
#define COMMON_H

#define MAX_L 100     /*  0 < L < 100  lists */
#define MAX_K 100     /*  0 < K < 100  candidates per list */
#define MAX_M 10000   /*  0 < M < 10000 committees */

#define MAX_PID 32768 /* maximal PID, as stated in /proc/sys/kernel/pid_max */

#define  M_ALL_KEY  1234L   /* queue for all init msg */
#define  M_REP_KEY_2 4321L  /* queue for replies from server to reports */
#define  M_COM_KEY  5678L   /* queue for committee msg */
#define  M_COM_KEY_2  8765L /* queue for replies from server to committees */

#define  INIT_KEY     1L    /* msg_type for all initialisation */

#define  COM_TYPE  0  /* init mesg process type for committee */
#define  REP_TYPE  1  /* init mesg process type for report */

typedef struct {      
  long  mesg_type;    /* set to INIT_KEY */
  pid_t pid;          /* PID of the process(report/committee) */
  int   type;         /* COM_TYPE if committee, REP_TYPE if report */
  int   ml;           /* nr committee if type == COM_TYPE, else (0 if no list specified, else nr of the list) */
  int   padding;      /* added to have 8*x bytes in a message */
} InitMsg;

typedef struct {
  long   mesg_type; /* set to PID of committee */
  int    m;         /* number of committee or -1 if committee ended sending input */
  int    l;         /* after successful conncection to server first set to 0, then nr of list */
  int    k;         /* when l == 0 set to nr of eligible voters, then nr of candidate */
  int    n;         /* when l == 0 set to nr of valid+invalid votes, then nr of voted for lk */
} ComMsg;

typedef struct {
  long   mesg_type; /* after first ComMsg set to PID+MAX_PID(for response to initialisation msg), then set to PID */
  int    sum_n;     /* all valid votes */
  int    w;         /* if initialisation successful set to 0, else -1, then number of entries */
} ComReturn;

typedef struct {
  long   mesg_type;   /* PID of the report */
  int    L;           /* number of all lists */
  int    x;           /* number of done committees */
  int    K;           /* number of all committees */
  long long    y;     /* number of eligible voters */
  long long    z;     /* number of valid votes */
  long long    v;     /* number of invalid votes */
  int padding;        /* added to have 8*x bytes in a message */
} RepReturn1;

typedef struct {
   long mesg_type;          /* PID of the report */ 
   int l;                   /* number of currently printed list */
   int K;                   /* number of candidates per list */
   long long list[MAX_K];   /* data with how many votes for candidate lk */
} RepReturn2;

#endif