 #include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

// Mit diesem Programm werden THREADS gestartet

void *print_message_function (void *ptr);

// MAIN ist der Mutter-Prozess
int main (void )
{
  pthread_t thread1, thread2;
  char *message1 = "Thread 1";
  char *message2 = "Thread 2";
  int iret1, iret2;
  /* Create independent threads each of which will execute function */

  // Hier werden Threads (Art Unterprozesse) gestartet, die parallel zueinander laufen
  // Reihenfolge der Threads zufällig vom Betriebssystem festgelegt 
  iret1 =
    pthread_create (&thread1, NULL, print_message_function,
		    (void *) message1);
  iret2 =
    pthread_create (&thread2, NULL, print_message_function,
		    (void *) message2);
  /* Wait till threads are complete before main continues. Unless we  */
  /* wait we run the risk of executing an exit which will terminate   */
  /* the process and all threads before the threads have completed.   */
  pthread_join (thread1, NULL);     // Über diesen Befehl werden die Unter-Prozesse (Threads) wieder dem Mutter-Prozess (Main) zugeordnet
  pthread_join (thread2, NULL);     // Über diesen Befehl werden die Unter-Prozesse (Threads) wieder dem Mutter-Prozess (Main) zugeordnet
  printf ("Thread 1 returns: %d\n", iret1);
  printf ("Thread 2 returns: %d\n", iret2);
  exit (0);
  return 0;
}

// Diese Funktion wird parallel von beiden Unter-Prozessen (Threads) ausgeführt
void *
print_message_function (void *ptr)
{
  char *message;
  message = (char *) ptr;
  printf ("%s \n", message);
}
