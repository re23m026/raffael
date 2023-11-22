#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

long int COUNT_MAX;           // Threads probieren die GLOBALE VARIABLE zu erhöhen
volatile long int in;

void *increase_in(void *arg)    // Called as thread
{
  long int i;
  long int next_free_slot;
  for (i = 0; i < COUNT_MAX; ++i)   // Globale Variable 'in' wird einfach immer um 1 erhöht
    {
      next_free_slot = in;
      next_free_slot = next_free_slot + 1;
      in = next_free_slot;
    }
  return NULL;
}
// 2 Threads können sich bei hohen Durchlaufzahlen in die Quere kommen !
// Thread 1 ist z.B. gerade in Zeile 14, dann wird die Funktion auch von Thread 2 aufgerufen
// Thread 1 wird dann "Schlagen gelegt" --> Thread 2 überschreibt die globale Variable in (Zeile 16)
// Thread 1 kann weitermachen, wenn Thread 2 fertig ist --> hat aber noch den alten Wert von in und überschreibt diesen erneut auf in
// Lokale Variable (next_free_slot) --> Hat jeder THREAD seine eigene ///// Globale Variable (in) --> wird geteilt von beiden THREADS

int main(int argc, char *argv[])
{
  pthread_t thread1_id;
  pthread_t thread2_id;

  if (argc != 2)
    {
      printf("Usage: %s COUNT_MAX\n", argv[0]);
      exit(EXIT_FAILURE);
    }
  sscanf(argv[1], "%lu", &COUNT_MAX);

  in = 0;
  pthread_create(&thread1_id, NULL, &increase_in, NULL);  // Hier wird Thread erstellt
  pthread_create(&thread2_id, NULL, &increase_in, NULL);  // Hier wird Thread erstellt --> PARALLEL
  pthread_join(thread1_id, NULL);
  pthread_join(thread2_id, NULL);
  printf("in=%ld\n", in);
  return EXIT_SUCCESS;
}
