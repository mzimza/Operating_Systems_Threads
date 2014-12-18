#ifndef COMMON_H
#define COMMON_H

/*
ZADANIE NR 2

Wprowadzenie

Treścią zadania jest system służący do liczenia głosów w wyborach :), przy czym upraszczamy rzeczywistość i obsługujemy tylko jeden rodzaj wyborów i jeden globalny zbiór kandydatów (bez podziału na okręgi wyborcze itp.).

Do napisania są w sumie trzy programy: serwer, komisja i raport.
W wyborach startuje L * K kandydatów umieszczonych na 0 < L < 100 listach numerowanych od 1 do L.
 Na każdej liście znajduje się 0 < K < 100 kandydatów numerowanych od 1 do K. 
 Para (numer listy, numer kandydata) identyfikuje kandydata. Wyborca ma wskazać jednego kandydata spośród wszystkich.
Jest 0 < M < 10000 lokali wyborczych numerowanych od 1 do M i odpowiadających im komisji.
Komisja sumuje głosy oddane na poszczególnych kandydatów oraz liczy głosy nieważne i korzystając z programu komisja wysyła te dane na serwer.
serwer sumuje głosy oddane na poszczególnych kandydatów i umożliwia pobranie raportu z bieżącymi wynikami.
Program raport pozwala pobrać z serwera częściowe lub końcowe wyniki wyborów.
Technologia

Komunikacja między klientami a serwerem odbywać się może wyłącznie przy pomocy kolejek komunikatów IPC Systemu V. W rozwiązaniu należy użyć stałej liczby kolejek.

Dokładny format komunikatów do ustalenia przez studenta. Zalecam zdefiniowanie odpowiednich typów oraz kluczy kolejek w pliku nagłówkowym włączanym do wszystkich programów.

Do realizacji wielowątkowości na serwerze należy użyć biblioteki pthreads. Synchronizację między wątkami należy zapewnić wyłącznie za pomocą mechanizmów pthreads takich jak mutexy, zmienne warunkowe lub blokady rwlock.

Dla potrzeb oceny poprawności rozwiązania można przyjąć, że kolejki komunikatów oraz mechanizmy pthreads są sprawiedliwe.

Można założyć, a nie trzeba sprawdzać, że parametry wywołania programów oraz dane wejściowe są poprawne, a także że klienci
 nie będą niespodziewanie przerywani.


Informacje organizacyjne

Rozwiązania (tylko Makefile, pliki źródłowe i opcjonalny plik z opisem) należy nadsyłać za pomocą skryptu submit na adres: solab@mimuw.edu.pl w terminie do 22 grudnia, 23:59

W przypadku wątpliwości można zadać pytanie autorowi zadania P. Czarnikowi. Przed zadaniem pytania warto sprawdzić, czy odpowiedź na nie nie pojawiła się na liście często zadawanych pytań.
*/


#define MAX_L 100
#define MAX_K 100
#define MAX_M 10000

#define MAXMESGDATA 1024

#define  M_REP_KEY  1234L  /* kolejka komunikatów dla raportów */
#define  M_REP_KEY_2 4321L /* kolejka komunikatów na odpowiedzi serwera na raport*/
#define  M_COM_KEY  5678L /* kolejka komunikatów dla komisji */
#define  M_COM_KEY_2  8765L  /* kolejka komunikatów na odpowiedzi serwera do komisji */

#define  INIT_COM_KEY 1L
#define  INIT_REP_KEY 2L

typedef struct {
  long   mesg_type;
  pid_t  pid; 
  int    m; 
  int    l;
  int    k;
  int    n;
  int    i;
  int    j;
} ComMsg;

typedef struct {
  long   mesg_type;
  int    m;
  int    sum_n;
  int    w;
 // int    x;
} ComReturn;

typedef struct {
  long   mesg_type;  
  pid_t  pid;
  int    l;   
} RepMsg;

typedef struct {
  long   mesg_type;  
  int    L;   
  int    x;
  int    K;
  long long    y;
  long long    z;
  long long    v;
} RepReturn1;

typedef struct {
   long mesg_type;
   int l;
   int list[MAX_K];
} RepReturn2;


#endif