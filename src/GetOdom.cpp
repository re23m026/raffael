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
    
}