/* Maja Zalewska  */
/* nr 336088      */
/* Zadanie 2, SO  */

#ifndef LIST_H
#define LIST_H

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <signal.h>

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


#endif