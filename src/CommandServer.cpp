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
int i = 0;

// SHARED MEMORY
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
    int sock;                        /* Socket descriptor */
    struct sockaddr_in echoServAddr; /* Echo server address */                  // Hiermit wird später über memset() Server Adresse angelegt/erstellt --> STRUCT: strukturierte Var. mit Untervariablen
    unsigned short echoServPort;     /* Echo server port */
    char *servIP;                    /* Server IP address (dotted quad) */      // Pointer auf String
    char *echoString;                /* String to send to echo server */        // Pointer auf String
    char echoBuffer[RCVBUFSIZE];     /* Buffer for echo string */               // Für das Wiederempfangen der gesendeten Message
    unsigned int echoStringLen;      /* Length of string to echo */
    unsigned int sendStringLen;
    float distance;
    int angle;
    int ID_LIDAR;
    float odom_x;
    float odom_y;
    int first = 1;
    float first_x_odom;
    float first_y_odom;
    float first_x_lidar;
    float first_y_lidar;
    float k_p = 0.1;
    float k_links = 0.1;
    float k_rechts = -0.01;

    union semun 
    {
        int val;
        struct semid_ds *buf;
        ushort *array;
    } arg;

    arg.val = 0;
    struct sembuf operations_LIDAR[1];
    int retval_LIDAR;

    // SEMAPHOREN
    /* Create the semaphore with external key KEY if it doesn't already exists. Give permissions to the world. */
    ID_LIDAR = semget(Key, 1, 0666 | IPC_CREAT);

    if (ID_LIDAR < 0)
    {
        fprintf(stderr, "Unable to obtain at least one of the semaphores.\n");
        exit(0);
    }

    if (semctl(ID_LIDAR, 0, SETVAL, arg) < 0)
    {
        fprintf(stderr, "Cannot set semaphore value.\n");
        exit(0);
    }
    else
    {
        fprintf(stderr, "Semaphore %d initialized.\n", Key);
    }

    LidarID = shmget(Key, sizeof(struct SharedMemoryLidar), 0666 | IPC_CREAT);
    if (LidarID == -1)
    {
        DieWithError("shmgetPos failed");
        exit(EXIT_FAILURE);
    }

    // SHARED MEMORY
    LidarPtr = (SharedMemoryLidar *)shmat(LidarID, NULL, 0);
    if (LidarPtr == (SharedMemoryLidar *)-1)
    {
        DieWithError("shmat failed");
        exit(EXIT_FAILURE);
    }


    // FIRST COMMAND TO MOVING
    char *sendString = new char[100];
    string msg = "---START---{\"linear\": 0.05, \"angular\": -0.00}___END___";
    strcpy(sendString, msg.c_str());
    // 1.045

    if ((argc < 3) || (argc > 4))    /* Test for correct number of arguments */
    {
       fprintf(stderr, "Usage: %s <Server IP> <Echo Word> [<Echo Port>]\n",     // Brauchen mind. die 3 Argumente (Usage = Programmname)
               argv[0]);
       exit(1);
    }

    servIP = argv[1];             /* First arg: server IP address (dotted quad) */  // Zuweisung der Argumente zu Variablen
    echoString = argv[2];         /* Second arg: string to echo */

    if (argc == 4)
        echoServPort = atoi(argv[3]); /* Use given port, if any */
    else
        echoServPort = 7;  /* 7 is the well-known port for the echo service */  // Wenn Argument <Echo Port> nicht übergeben wird, standardmäßig auf 7 gesetzt

    // CREATE RELIABLE STREAM
    if ((sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
    {
        DieWithError("socket() failed");
    }

    /* Construct the server address structure */
    memset(&echoServAddr, 0, sizeof(echoServAddr)); /* Zero out structure */
    echoServAddr.sin_family = AF_INET;              /* Internet address family */
    echoServAddr.sin_addr.s_addr = inet_addr(servIP);   /* Server IP address */
    echoServAddr.sin_port = htons(echoServPort);           /* Server port */

    /* Establish the connection to the echo server */
    if (connect(sock, (struct sockaddr *)&echoServAddr, sizeof(echoServAddr)) < 0)
    {
        DieWithError("connect() failed");
    }

    sendStringLen = msg.length();
    if (sendto(sock, sendString, sendStringLen, 0, (struct sockaddr *)&echoServAddr, sizeof(echoServAddr)) != sendStringLen)
        DieWithError("sendto() sent a different number of bytes than expected");
    sleep(1);
    float a = 22.0;

    while (a != 0)
    {
        a = close(sock);
        cout << a << endl;
        sleep(1);
    }

    for (;;)
    {
        while (semctl(ID_LIDAR, 0, GETVAL) != 0)
            ;

        /* Set up the sembuf structure. */
        operations_LIDAR[0].sem_num = 0; /* Which semaphore in the semaphore array : */
        operations_LIDAR[0].sem_op = +1; /* Which operation? Subtract 1 from semaphore value : */
        operations_LIDAR[0].sem_flg = 0; /* Set the flag so we will wait : */


        retval_LIDAR = semop(ID_LIDAR, operations_LIDAR, 1);
        if (retval_LIDAR != 0)
        {
            printf("semb: V-operation did not succeed.\n");
        }
        //SAVE FIRST VALUES TO RESET 
        if (first == 1)
        {
            first_x_odom = LidarPtr->odom[0];
            first_y_odom = LidarPtr->odom[1];
            
            first_x_lidar = cos((LidarPtr->angle*M_PI)/180)*LidarPtr->distance;
            first_y_lidar = sin((LidarPtr->angle*M_PI)/180)*LidarPtr->distance;

            
            first = first + 1;

        }

        //SAVE CURRENT VALUES
        angle = LidarPtr->angle;
        distance = LidarPtr->distance;
        odom_x = LidarPtr->odom[0] - first_x_odom;
        odom_y = LidarPtr->odom[1] - first_y_odom;

        //CALCULATE ALL RELEVANT VALUES 

        float lidar_x = cos((angle*M_PI)/180)*distance; 
        float lidar_y = sin((angle*M_PI)/180)*distance;

        lidar_x = first_x_lidar - lidar_x;
        lidar_y = first_y_lidar - lidar_y;

        float diff_x = 1.039 - 0.9*odom_x + 0.1*lidar_x;
        float diff_y = 0 - 0.9*odom_y + 0.1*lidar_y;

        float rho = sqrt(diff_x*diff_x + diff_y*diff_y);
        float alpha = 0 + atan2(diff_y,diff_x);
        float beta = 0 - alpha;



        //cout << distance << endl;
        //cout << angle << endl;
        //cout << rho << endl;
        //cout << alpha << endl;
        //cout << beta << endl;



        float v = k_p * rho;
        float w = k_rechts * alpha + k_links * beta;


        if(v > 0.06){
            v = 0.06;
        }

        if(w > 0.07){
            w = 0.07;
        }


        msg = "---START---{\"linear\": " + to_string(v) + ", \"angular\": " + to_string(w) + "}___END___";

        cout << msg << endl;

        strcpy(sendString, msg.c_str());
    //}

    if ((sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
    {
        DieWithError("socket() failed");
    }

    /* Construct the server address structure */
    memset(&echoServAddr, 0, sizeof(echoServAddr)); /* Zero out structure */
    echoServAddr.sin_family = AF_INET;              /* Internet address family */
    echoServAddr.sin_addr.s_addr = inet_addr(servIP);   /* Server IP address */
    echoServAddr.sin_port = htons(echoServPort);           /* Server port */

    if (connect(sock, (struct sockaddr *)&echoServAddr, sizeof(echoServAddr)) < 0)
    {
        DieWithError("connect() failed");
    }
    sendStringLen = msg.length();
    if (sendto(sock, sendString, sendStringLen, 0, (struct sockaddr *)&echoServAddr, sizeof(echoServAddr)) != sendStringLen)
        DieWithError("sendto() sent a different number of bytes than expected");
    close(sock);


        
        
        //cout << "DATA" << endl;
        //cout << odom_x << endl;
        //cout << odom_y << endl;
        //cout << lidar_x << endl;
        //cout << lidar_y << endl;
        //cout << "ANGLE" << endl;
        //cout << angle << endl;

        operations_LIDAR[0].sem_num = 0; /* Which semaphore in the semaphore array : */
        operations_LIDAR[0].sem_op = -1; /* Which operation? Subtract 1 from semaphore value : */
        operations_LIDAR[0].sem_flg = 0; /* Set the flag so we will wait : */
        /* So do the operation! */
        // std::cout<<"2:      LIDAR"<<std::endl;
        retval_LIDAR = semop(ID_LIDAR, operations_LIDAR, 1);
        if (retval_LIDAR != 0)
        {
            printf("semb: V-operation did not succeed.\n");
        }

        //cout << angle << endl;
        //cout << lidar_x << endl;


        //if (angle > 130)
        //{
            //cout << "FIRST" << endl;

            //ENDING PROGRAMM IF DISTANCE REACHED
            if (lidar_x >= first_x_lidar +0.4)
            {
                cout << "SECOND" << endl;
                break;
            }
        //}
        sleep(0.2);
    }

    /* Find length of sendString */
    // for (;;) /* Run forever */
    //{
    /* Broadcast sendString in datagram to clients every 3 seconds*/

    // sleep(10);   /* Avoids flooding the network */
    msg = "---START---{\"linear\": 0.0, \"angular\": 0.00}___END___";
    strcpy(sendString, msg.c_str());
    //}

    if ((sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
    {
        DieWithError("socket() failed");
    }

    /* Construct the server address structure */
    memset(&echoServAddr, 0, sizeof(echoServAddr)); /* Zero out structure */
    echoServAddr.sin_family = AF_INET;              /* Internet address family */
    echoServAddr.sin_addr.s_addr = inet_addr(servIP);   /* Server IP address */
    echoServAddr.sin_port = htons(echoServPort);           /* Server port */

    if (connect(sock, (struct sockaddr *)&echoServAddr, sizeof(echoServAddr)) < 0)
    {
        DieWithError("connect() failed");
    }
    sendStringLen = msg.length();
    if (sendto(sock, sendString, sendStringLen, 0, (struct sockaddr *)&echoServAddr, sizeof(echoServAddr)) != sendStringLen)
        DieWithError("sendto() sent a different number of bytes than expected");
    sleep(1);
    close(sock);
    sleep(1);
    /* NOT REACHED */
}