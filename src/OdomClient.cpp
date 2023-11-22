#include <math.h>
#include <stdio.h>      /* for printf() and fprintf() */
#include <sys/socket.h> /* for socket(), connect(), send(), and recv() */
#include <arpa/inet.h>  /* for sockaddr_in and inet_addr() */
#include <stdlib.h>     /* for atoi() and exit() */
#include <string.h>     /* for memset() */
#include <unistd.h>     /* for close() */
#include <sys/wait.h>   /*For Fork*/
#include <sys/types.h>  /*For Fork*/
#include <string>
#include <iostream>
#include <sys/ipc.h> //shared memory
#include <sys/shm.h> //shared memory
#include <signal.h>
#include <sys/sem.h>
using namespace std;
#define RCVBUFSIZE 20000 /* Size of receive buffer */
#define Key (816)

struct SharedMemoryLidar
{
    float distance;
    int angle;
    float odom[10];
};

int LidarID;                  // Shared memory id
SharedMemoryLidar *LidarPtr;  // Pointer to shared memory
int mutex, empty, full, init; // Semaphore ids COPIED

void DieWithError(const char *errorMessage)
{
    perror(errorMessage);
    exit(1);
}

int main(int argc, char *argv[])
{
    // Definition der Variablen


    // Definition der Semaphoren


    // Shared Memory


    // Shared Memory erstellen


    // Schauen, ob Programm korrekt aufgerufen wurde

 
    // Semaphore erstellen


    // Shared Memory für Position erstellen


    // FOREVER SCHLEIFE

    // Stream erstellen über TCPIP
    // if ((sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
    //     {
    //         DieWithError("socket() failed");
    //     }


    // Struktur der Server-Adresse definieren


    // Verbindung zu Echo Server aufbauen


    // Daten erhalten bis zur Buffer-Größe (Minus 1 damit noch Platz für NULL Terminator ist)


    // Erhaltene Daten ausschneiden bis auf Zahlen


    // Eingehende Nummern speichern


    // Filter für Range 0.02 - 0.8


    // sembuf Struktur aufsetzen


    // Alle Ranges durchgehen und den kleinsten speichern inkl. coressponding Winkel


    // sembuf Struktur aufsetzen


}