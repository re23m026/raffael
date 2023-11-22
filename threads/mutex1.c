#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

// Mutex ist bestimmte Struktur in C, die EXCLUSION erlaubt

void *functionC ();
pthread_mutex_t mutex1 = PTHREAD_MUTEX_INITIALIZER; // MUTEX schützt vor Mehrfachaufrufen von Threads
int counter = 0;      // Wollen mit beiden Threads diese globale Variable erhöhen

int main (void )
{
  int rc1, rc2;
  pthread_t thread1, thread2;

  /* Create independent threads each of which will execute functionC */

  if ((rc1 = pthread_create (&thread1, NULL, &functionC, NULL)))
    {
      printf ("Thread creation failed: %d\n", rc1);
    }

  if ((rc2 = pthread_create (&thread2, NULL, &functionC, NULL)))
    {
      printf ("Thread creation failed: %d\n", rc2);
    }

  /* Wait till threads are complete before main continues. Unless we  */
  /* wait we run the risk of executing an exit which will terminate   */
  /* the process and all threads before the threads have completed.   */

  pthread_join (thread1, NULL);
  pthread_join (thread2, NULL);

  exit (0);
  return 0;
}

void *
functionC ()
{
  pthread_mutex_lock (&mutex1);   // Wenn einer von beiden Prozessen auf diesen MUTEX zugreift, muss der andere Prozess warten
  counter++;
  printf ("Counter value: %d\n", counter);
  pthread_mutex_unlock (&mutex1);
}
