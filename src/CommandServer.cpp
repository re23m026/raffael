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
struct SharedMemoryLidar     //LidarPtr greift hierauf zurück
{
    float distance;
    int angle;
    float odom[10];
};

int LidarID;                  // saves ID of the shared-memory segment, as soon as the segment is produced (via 'shmget')
SharedMemoryLidar *LidarPtr;  // Pointer to shared memory

// Semaphore ids COPIED
int mutex; // semaphor mutex (=gegenseitiger Auschluss) schützt krit. Abschnitte des Codes und stellt sicher, dass nur ein Thread/Prozess gleichzeitig auf den shared memory zugreift
int empty; // ??semaphor empty gibt Zustand der Warteschlange an??
int full; //??semaphor full gibt Zustand der Warteschlange an??
int init;  // semaphor zur Initialisierung des shared memories

void DieWithError(const char *errorMessage)
{
    perror(errorMessage);
    exit(1);
}

int main(int argc, char *argv[]) //Aufruf der main() mit Befehlszeilenargument
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
    float lidar_x;
    float lidar_y;
    int first = 1;
    float first_x_odom;
    float first_y_odom;
    float first_x_lidar;
    float first_y_lidar;
    float k_rho = 0.2;         // (zu fahrende Distanz) - Hyperparameter, muss definiert werden
    float k_beta = 0.2;        // Abstand zur linken Wand
    float k_alpha = -0.5;     // Abstand zur rechten Wand
    float distance_to_drive = 5;  // Strecke die wir zu Beginn linear fahren wollen
    float distance_b;
    float distance_c;
    float diameter_d;

    // SEMAPHOREN
    union semun     //union: datenstruktur, speichert verschiedene Datentypen unter demselben Speicherplatz  --> Selber SPeicherbereich repräsentier versch. Datentypen 
    {
        int val;   
        struct semid_ds *buf;
        ushort *array;
    } arg;

    arg.val = 0; 
    struct sembuf operations_LIDAR[1];
    int retval_LIDAR;

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
    // char *sendString = new char[100];
    // string msg = "---START---{\"linear\": 0.05, \"angular\": -0.05}___END___";
    // strcpy(sendString, msg.c_str());
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

    char *sendString = new char[100];
    string msg = "---START---{\"linear\": 0.05, \"angular\": -0.05}___END___";
        
    strcpy(sendString, msg.c_str());

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
        
        
        
        // SAVE FIRST VALUES TO RESET 
        if (first == 1)
        {
            first_x_odom = LidarPtr->odom[0];
            first_y_odom = LidarPtr->odom[1];
            
            first_x_lidar = cos((LidarPtr->angle*M_PI)/180)*LidarPtr->distance;
            first_y_lidar = sin((LidarPtr->angle*M_PI)/180)*LidarPtr->distance;

            
            first = first + 1;
        }


        // Aktuellen Werte speichern (Relativ zum Startpunkt)
        odom_x = LidarPtr->odom[0] - first_x_odom;
        odom_y = LidarPtr->odom[1] - first_y_odom; 
        angle = LidarPtr->angle;
        distance = LidarPtr->distance;

        lidar_x = first_x_lidar - (cos((angle*M_PI)/180)*distance); // nur zur Überprüfung der Abweichung
        lidar_y = first_y_lidar - (sin((angle*M_PI)/180)*distance); // nur zur Überprüfung der Abweichung

        std::cout << "X: " << odom_x << std::endl;
        std::cout << "Y: " << odom_y << std::endl;
        std::cout << "Lidar X: " << lidar_x << std::endl;
        std::cout << "Lidar Y: " << lidar_y << std::endl;

        // Calculate the pos. + ori. of the robot in respect to the laser and odom data

        float diff_x = 1.039 - 0.9*odom_x + 0.1*lidar_x;    // ?
        float diff_y = 0 - 0.9*odom_y + 0.1*lidar_y;        // ?
        float diff_teta = 0;                                // Für lineare Beweg. bleibt teta = 0

        float rho = sqrt(diff_x*diff_x + diff_y*diff_y);    
        float alpha = diff_teta + atan2(diff_y,diff_x);
        float beta = diff_teta - alpha;

        float v = k_rho * rho;
        float w = k_alpha * alpha + k_beta * beta;
            
        if(v > 0.06){       // ?
            v = 0.06;
        }

        if(w > 0.07){
            w = 0.07;
        }

        msg = "---START---{\"linear\": " + to_string(v) + ", \"angular\": " + to_string(w) + "}___END___";
        strcpy(sendString, msg.c_str()); // Befehl zum Fahren

        std::cout << msg << std::endl;

        // Überprüfung ob Zieldistanz erreicht wurde
        if (lidar_x >= first_x_lidar + distance_to_drive)
        {
            cout << "Startpunkt für die Kreisbahn erreicht" << endl;
            break;
        }


        // 2. LINEARE REGELUNG UM DIE SÄULE     (IDEE)
            // Am besten kontinuerilich in die Drehung wechseln

        // float distance_bar = 0.2;       // muss angepasst werden
        // float diff_distance_bar = 0.05;

        // for ( ; ; ) {
        //     odom_x = LidarPtr->odom[0] - first_x_odom;
        //     odom_y = LidarPtr->odom[1] - first_y_odom; 
        //     angle = LidarPtr->angle;                    // Winkel mit kleinstem Abstand zur Säule (um die wir fahren wollen)
        //     distance = LidarPtr->distance;              // Distanz zur Säule bei dem entsprechenden Winkel 

        //     lidar_x = first_x_lidar - (cos((angle*M_PI)/180)*distance); // nur zur Überprüfung der Abweichung
        //     lidar_y = first_y_lidar - (sin((angle*M_PI)/180)*distance); // nur zur Überprüfung der Abweichung

        //     std::cout << angle << std::endl;
        //     std::cout << distance << std::endl;

        //     if (distance < distance_bar || distance > distance_bar) {

        //         if(distance < distance_bar) {
        //             float diff_x = 1.039 - 0.9*odom_x + 0.1*lidar_x;    // TODO: Wenn Distanz kleiner, Korrektur nach rechts
        //             float diff_y = 0 - 0.9*odom_y + 0.1*lidar_y;        
        //             float diff_teta = 0;                                

        //             float rho = sqrt(diff_x*diff_x + diff_y*diff_y);    
        //             float alpha = diff_teta + atan2(diff_y,diff_x);
        //             float beta = diff_teta - alpha;

        //             float v = k_rho * rho;
        //             float w = k_alpha * alpha + k_beta * beta;    

        //         }

        //         if(distance > distance_bar) {
        //             float diff_x = 1.039 - 0.9*odom_x + 0.1*lidar_x;  // TODO: wenn Distanz größer, Korrektur nach links
        //             float diff_y = 0 - 0.9*odom_y + 0.1*lidar_y;        
        //             float diff_teta = 0;                                

        //             float rho = sqrt(diff_x*diff_x + diff_y*diff_y);    
        //             float alpha = diff_teta + atan2(diff_y,diff_x);
        //             float beta = diff_teta - alpha;

        //             float v = k_rho * rho;
        //             float w = k_alpha * alpha + k_beta * beta;                 
        //         }

        //     }

        // }

        // 3. LINEAR ZURÜCK ZUM STARTPUNKT


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
            // if (lidar_x >= first_x_lidar +0.4)
            // {
            //     cout << "SECOND" << endl;
            //     break;
            // }
        //}
        sleep(0.1);
    }

    /* Find length of sendString */
    // for (;;) /* Run forever */
    //{
    /* Broadcast sendString in datagram to clients every 3 seconds*/


    // LINEAR ZURÜCK FAHREN

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
