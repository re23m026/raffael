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
#include <cstdlib>
#include <cstdio>
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

// SEMAPHOREN
union semun     //union: datenstruktur, speichert verschiedene Datentypen unter demselben Speicherplatz  --> Selber SPeicherbereich repräsentier versch. Datentypen 
{
    int val;   
    struct semid_ds *buf;
    ushort *array;
};

// // Post (V-Operation)
// void post(int semId) {
//     struct sembuf op;
//     op.sem_num = 0;
//     op.sem_op = 1;
//     op.sem_flg = 0;
//     semop(semId, &op, 1);
// }


// // Wait (P-Operation)
// void wait(int semId) {
//     struct sembuf op;
//     op.sem_num = 0;
//     op.sem_op = -1;
//     op.sem_flg = 0;
//     semop(semId, &op, 1);
// }

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
    float odom_theta;
    float lidar_x;
    float lidar_y;
    int first = 1;
    float x_odom_1;
    float y_odom_1;
    float theta_odom_1;
    float first_x_lidar;
    float first_y_lidar;
    float k_rho = 0.1;         
    float k_beta = 0;        // Abstand zur linken Wand
    float k_alpha = 0;     // Abstand zur rechten Wand
    float distance_to_drive = 0.8;  // Strecke die wir zu Beginn linear fahren wollen
    float radius = 0.2;
    //float distance_b;
    //float distance_c;
    //float diameter_d;
    //float distance_bar = 0.2;

    union semun arg;
    arg.val = 0; 
    struct sembuf operations_LIDAR[1];
    int retval_LIDAR;

    /* Create the semaphore with external key KEY if it doesn't already exists. Give permissions to the world. */
    ID_LIDAR = semget(Key, 1, 0666 | IPC_CREAT);

    if (ID_LIDAR < 0) {
        fprintf(stderr, "Unable to obtain at least one of the semaphores.\n");
        exit(0);
    }

    if (semctl(ID_LIDAR, 0, SETVAL, arg) < 0) {
        fprintf(stderr, "Cannot set semaphore value.\n");
        exit(0);
    }
    else {
        fprintf(stderr, "Semaphore %d initialized.\n", Key);
    }

    LidarID = shmget(Key, sizeof(struct SharedMemoryLidar), 0666 | IPC_CREAT);
    if (LidarID == -1) {
        DieWithError("shmgetPos failed");
        exit(EXIT_FAILURE);
    }

    // SHARED MEMORY
    LidarPtr = (SharedMemoryLidar *)shmat(LidarID, NULL, 0);
    if (LidarPtr == (SharedMemoryLidar *)-1) {
        DieWithError("shmat failed");
        exit(EXIT_FAILURE);
    }

    // FIRST COMMAND TO MOVING
    // char *sendString = new char[100];
    // string msg = "---START---{\"linear\": 0.05, \"angular\": -0.05}___END___";
    // strcpy(sendString, msg.c_str());
    // 1.045
    
    /* Test for correct number of arguments */
    if ((argc < 3) || (argc > 4)) {
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
    if ((sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
        DieWithError("socket() failed");
    }

    /* Construct the server address structure */
    memset(&echoServAddr, 0, sizeof(echoServAddr)); /* Zero out structure */
    echoServAddr.sin_family = AF_INET;              /* Internet address family */
    echoServAddr.sin_addr.s_addr = inet_addr(servIP);   /* Server IP address */
    echoServAddr.sin_port = htons(echoServPort);           /* Server port */

    /* Establish the connection to the echo server */
    if (connect(sock, (struct sockaddr *)&echoServAddr, sizeof(echoServAddr)) < 0) {
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

    while (a != 0) {
        a = close(sock);
        cout << a << endl;
        sleep(1);
    }

    for (;;)
    {
        while (semctl(ID_LIDAR, 0, GETVAL) != 0)
         
        // SEMAPHOREN-POST-FUNKTION
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
            x_odom_1 = LidarPtr->odom[0];
            y_odom_1 = LidarPtr->odom[1];
            theta_odom_1 = LidarPtr->odom[4];
            //first_x_lidar = cos((LidarPtr->angle*M_PI)/180)*LidarPtr->distance;
            //first_y_lidar = sin((LidarPtr->angle*M_PI)/180)*LidarPtr->distance;
            first = first + 1;
        }

        float x_goal = distance_to_drive + x_odom_1;
        float y_goal = y_odom_1;
        float theta_goal = theta_odom_1;

        std::cout<< "x_odom_1: " << x_odom_1 << std::endl;
        std::cout<< "y_odom_1: " << y_odom_1 << std::endl;
        std::cout<< "theta_odom_1: " << theta_odom_1 << std::endl;
        // std::cout<< "x_goal: " << x_goal << std::endl;q
        // std::cout<< "y_goal: " << y_goal << std::endl;
        // std::cout<< "theta_goal: " << theta_goal << std::endl;


        // Aktuellen Werte speichern (Relativ zum Startpunkt)
        odom_x = LidarPtr->odom[0];
        odom_y = LidarPtr->odom[1]; 
        odom_theta = LidarPtr->odom[4];
        //angle = LidarPtr->angle;
        //distance = LidarPtr->distance;

        //lidar_x = first_x_lidar - (cos((angle*M_PI)/180)*distance); // nur zur Überprüfung der Abweichung
        //lidar_y = first_y_lidar - (sin((angle*M_PI)/180)*distance); // nur zur Überprüfung der Abweichung

        std::cout << "X: " << odom_x << std::endl;
        std::cout << "Y: " << odom_y << std::endl;
        std::cout << "Theta: " << odom_theta << std::endl;
        //std::cout << "Lidar X: " << lidar_x << std::endl;
        //std::cout << "Lidar Y: " << lidar_y << std::endl;

        // Calculate the pos. + ori. of the robot in respect to the laser and odom data

        float diff_x = x_goal - odom_x;    // ?
        float diff_y = y_goal - odom_y;        // ?
        float diff_theta = 0;                                // Für lineare Beweg. bleibt teta = 0

        std::cout << "diff_x: " << diff_x << std::endl;
        float rho = sqrt((diff_x*diff_x) + (diff_y*diff_y));    
        float alpha = diff_theta + atan2(diff_y,diff_x);
        float beta = diff_theta - alpha;
        std::cout << "rho: " << rho << std::endl;

        k_alpha = 0;
        k_beta = 0;
        k_rho=0.4;

        float v = k_rho * rho;
        float w = k_alpha * alpha + k_beta * beta;
            
        if(v > 0.06){     
            v = 0.06;
        }

        if(w > 0.07){
            w = 0.07;
        }

        msg = "---START---{\"linear\": " + to_string(v) + ", \"angular\": " + to_string(w) + "}___END___";
        strcpy(sendString, msg.c_str()); // Befehl zum Fahren

        std::cout << msg << std::endl;

        if (diff_x < 0.05)
        {
            cout << "Startpunkt für die Kreisbahn erreicht" << endl;
            cout << "....................................." << endl;
            msg = "---START---{\"linear\": 0.0, \"angular\": 0.00}___END___";
            strcpy(sendString, msg.c_str());
            sleep(0.5);
            break;
        }
        
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
    
        
        // SEMAPHOREN-WAIT-FUNCTION
        operations_LIDAR[0].sem_num = 0; /* Which semaphore in the semaphore array : */
        operations_LIDAR[0].sem_op = -1; /* Which operation? Subtract 1 from semaphore value : */
        operations_LIDAR[0].sem_flg = 0; /* Set the flag so we will wait : */
        /* So do the operation! */
       
        retval_LIDAR = semop(ID_LIDAR, operations_LIDAR, 1);
        if (retval_LIDAR != 0)
        {
            printf("semb: V-operation did not succeed.\n");
        }

    }

    // Speichern der aktullen Position, da diese später erneut angefahren werden muss
    float x_odom_2 = LidarPtr->odom[0];
    float y_odom_2 = LidarPtr->odom[1];
    float theta_odom_2 = LidarPtr->odom[4];


    std::cout << "Schleife verlassen......................................" << std::endl;
    std::cout << "...........x_odom_2: " << x_odom_2 << std::endl;
    std::cout << "...........y_odom_2: " << y_odom_2 << std::endl;
    std::cout << "............theta_odom_2: " << theta_odom_2 << std::endl;

    // Nach Erreichen des ersten Zielpunktes, kurz stehen bleiben
    sleep(1);   /* Avoids flooding the network */
    
    //}


    //------------- ERSTE KREIS_VIERTEL_BEWEGUNG------------------------------------------------------------------------------------
    for ( ; ; )
    {
        while (semctl(ID_LIDAR, 0, GETVAL) != 0)
        
        /* Set up the sembuf structure. */
        operations_LIDAR[0].sem_num = 0; /* Which semaphore in the semaphore array : */
        operations_LIDAR[0].sem_op = +1; /* Which operation? Subtract 1 from semaphore value : */
        operations_LIDAR[0].sem_flg = 0; /* Set the flag so we will wait : */


        retval_LIDAR = semop(ID_LIDAR, operations_LIDAR, 1);
        if (retval_LIDAR != 0)
        {
            printf("semb: V-operation did not succeed.\n");
        }

        float x_goal;
        float y_goal;
        float theta_goal;

        // if(j == 0) {
        //     x_goal = x_odom_2;
        //     y_goal = y_odom_2;
        //     theta_goal = theta_odom_2 + 90;
        // }

        x_goal = x_odom_2 + 0.5;
        y_goal = y_odom_2 + 0.5;  //wieso -? wegen Fahrtrichtung?
        theta_goal = theta_odom_2 - 45;       
        
        // else if(j == 1) {
        //     x_goal = 1.2;
        //     y_goal = 0;
        //     theta_goal = -90;       
        // }
        // else if(j == 2) {
        //     x_goal = 1;
        //     y_goal = -0.2;
        //     theta_goal = 180;       
        // }
        // else if(j == 3) {
        //     x_goal = x_odom_2; 
        //     y_goal = y_odom_2;
        //     theta_goal = theta_odom_2 + 180;       
        // }

        //float gamma = theta_odom_2 - 45;     // 45 Grad, entsprechend der Geometrie des Viertelkreises
        // float x_goal = sqrt(2*radius*radius)*cos(gamma) + x_odom_2;
        // float y_goal = sqrt(2*radius*radius)*sin(gamma) + y_odom_2;
        // //float theta_goal = 0 + theta_odom_2;

        std::cout<< "x_goal 1. Kreis: " << x_goal << std::endl;
        std::cout<< "y_goal 1. Kreis: " << y_goal << std::endl;
        std::cout<< "theta_goal 1. Kreis: " << theta_goal << std::endl;
     
        // Aktuellen Werte speichern (Relativ zum Startpunkt)
        odom_x = LidarPtr->odom[0]; //- sqrt(x_odom_2*x_odom_2);
        odom_y = LidarPtr->odom[1]; //- sqrt(y_odom_2*y_odom_2); 
        odom_theta = LidarPtr->odom[4];
        //angle = LidarPtr->angle;
        //distance = LidarPtr->distance;

        //lidar_x = first_x_lidar - (cos((angle*M_PI)/180)*distance); // nur zur Überprüfung der Abweichung
        //lidar_y = first_y_lidar - (sin((angle*M_PI)/180)*distance); // nur zur Überprüfung der Abweichung

        std::cout << "X 1. Kreis: " << odom_x << std::endl;
        std::cout << "Y 1. Kreis: " << odom_y << std::endl;
        std::cout << "Theta 1. Kreis: " << odom_theta << std::endl;
        //std::cout << "Lidar X: " << lidar_x << std::endl;
        //std::cout << "Lidar Y: " << lidar_y << std::endl;

        // Calculate the pos. + ori. of the robot in respect to the laser and odom data

        float diff_x = x_goal - odom_x;    // ?
        float diff_y = y_goal - odom_y;        // ?
        float diff_theta = theta_goal - odom_theta;              // Für lineare Beweg. bleibt teta = 0

        float rho = sqrt(diff_x*diff_x + diff_y*diff_y);    
        float alpha = - diff_theta + atan2(diff_y,diff_x);
        float beta = - diff_theta - alpha;

        k_alpha = 0.2;
        k_beta = -0.05;
        k_rho  = 0.1;

        float v = k_rho * rho;
        float w = k_alpha * alpha + k_beta * beta;
            
        if(v > 0.06){     
            v = 0.06;
        }

        if(w > 0.07){
            w = 0.07;
        }

        std::cout << "Diff X " << diff_x << std::endl;
        std::cout << "Diff Y "<< diff_y << std::endl;
        std::cout <<  "Rho "<<rho << std::endl;
        std::cout << "Theta Goal "<< theta_goal << std::endl;


        msg = "---START---{\"linear\": " + to_string(v) + ", \"angular\": " + to_string(w) + "}___END___";
        strcpy(sendString, msg.c_str()); // Befehl zum Fahren

        std::cout << msg << std::endl;
        // std::cout << angle << std::endl;
        // std::cout << distance << std::endl;

        if (diff_y < 0.1)
        {
            cout << "Erster Kreispunkt erreicht" << endl;
            cout << "....................................." << endl;
            msg = "---START---{\"linear\": 0.0, \"angular\": 0.00}___END___";
            strcpy(sendString, msg.c_str());
            sleep(5);
            break;
        }
        
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
       
    }

    //--------------------------------------ZWEITER KREISVIERTEL--------------------------------------------

    float x_odom_3 = LidarPtr->odom[0];
    float y_odom_3 = LidarPtr->odom[1];
    float theta_odom_3 = LidarPtr->odom[4];

    for ( ; ; )
    {
        while (semctl(ID_LIDAR, 0, GETVAL) != 0)
        
        /* Set up the sembuf structure. */
        operations_LIDAR[0].sem_num = 0; /* Which semaphore in the semaphore array : */
        operations_LIDAR[0].sem_op = +1; /* Which operation? Subtract 1 from semaphore value : */
        operations_LIDAR[0].sem_flg = 0; /* Set the flag so we will wait : */


        retval_LIDAR = semop(ID_LIDAR, operations_LIDAR, 1);
        if (retval_LIDAR != 0)
        {
            printf("semb: V-operation did not succeed.\n");
        }

        float x_goal;
        float y_goal;
        float theta_goal;

        // if(j == 0) {
        //     x_goal = x_odom_2;
        //     y_goal = y_odom_2;
        //     theta_goal = theta_odom_2 + 90;
        // }

        // x_goal = 1;
        // y_goal = 0.2;
        // theta_goal = theta_odom_2;       
        
        x_goal = x_odom_3 + 0.5;
        y_goal = y_odom_3 - 0.5;
        theta_goal = theta_odom_2 - 90;       
    
        // else if(j == 2) {
        //     x_goal = 1;
        //     y_goal = -0.2;
        //     theta_goal = 180;       
        // }
        // else if(j == 3) {
        //     x_goal = x_odom_2; 
        //     y_goal = y_odom_2;
        //     theta_goal = theta_odom_2 + 180;       
        // }

        //float gamma = theta_odom_2 - 45;     // 45 Grad, entsprechend der Geometrie des Viertelkreises
        // float x_goal = sqrt(2*radius*radius)*cos(gamma) + x_odom_2;
        // float y_goal = sqrt(2*radius*radius)*sin(gamma) + y_odom_2;
        // //float theta_goal = 0 + theta_odom_2;

        std::cout<< "x_goal 2.K: " << x_goal << std::endl;
        std::cout<< "y_goal 2.K: " << y_goal << std::endl;
        std::cout<< "theta_goal 2.K: " << theta_goal << std::endl;
     
        // Aktuellen Werte speichern (Relativ zum Startpunkt)
        odom_x = LidarPtr->odom[0]; //- sqrt(x_odom_2*x_odom_2);
        odom_y = LidarPtr->odom[1]; //- sqrt(y_odom_2*y_odom_2); 
        odom_theta = LidarPtr->odom[4];
        //angle = LidarPtr->angle;
        //distance = LidarPtr->distance;

        //lidar_x = first_x_lidar - (cos((angle*M_PI)/180)*distance); // nur zur Überprüfung der Abweichung
        //lidar_y = first_y_lidar - (sin((angle*M_PI)/180)*distance); // nur zur Überprüfung der Abweichung

        std::cout << "X 2.K: " << odom_x << std::endl;
        std::cout << "Y 2.K: " << odom_y << std::endl;
        std::cout << "Theta 2.K: " << odom_theta << std::endl;
        //std::cout << "Lidar X: " << lidar_x << std::endl;
        //std::cout << "Lidar Y: " << lidar_y << std::endl;

        // Calculate the pos. + ori. of the robot in respect to the laser and odom data

        float diff_x = x_goal - odom_x;    // ?
        float diff_y = y_goal - odom_y;        // ?
        float diff_theta = theta_goal - odom_theta;              // Für lineare Beweg. bleibt teta = 0

        float rho = sqrt(diff_x*diff_x + diff_y*diff_y);    
        float alpha = diff_theta + atan2(diff_y,diff_x);
        float beta = diff_theta - alpha;

        k_alpha = 0.2;
        k_beta = -0.05;
        k_rho  = 0.1;

        float v = k_rho * rho;
        float w = k_alpha * alpha + k_beta * beta;
            
        if(v > 0.06){     
            v = 0.06;
        }

        if(w > 0.07){
            w = 0.07;
        }

        msg = "---START---{\"linear\": " + to_string(v) + ", \"angular\": " + to_string(w) + "}___END___";
        strcpy(sendString, msg.c_str()); // Befehl zum Fahren

        std::cout << msg << std::endl;
        // std::cout << angle << std::endl;
        // std::cout << distance << std::endl;

        if (rho < 0.1)
        {
            cout << "Zweiter Kreispunkt erreicht" << endl;
            cout << "....................................." << endl;
            msg = "---START---{\"linear\": 0.0, \"angular\": 0.00}___END___";
            strcpy(sendString, msg.c_str());
            sleep(1);
            break;
        }

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
    }

    // ----------------------------------------- 3. KREISVIERTEL ---------------------------
    for ( ; ; )
    {
        while (semctl(ID_LIDAR, 0, GETVAL) != 0)
        
        /* Set up the sembuf structure. */
        operations_LIDAR[0].sem_num = 0; /* Which semaphore in the semaphore array : */
        operations_LIDAR[0].sem_op = +1; /* Which operation? Subtract 1 from semaphore value : */
        operations_LIDAR[0].sem_flg = 0; /* Set the flag so we will wait : */


        retval_LIDAR = semop(ID_LIDAR, operations_LIDAR, 1);
        if (retval_LIDAR != 0)
        {
            printf("semb: V-operation did not succeed.\n");
        }

        float x_goal;
        float y_goal;
        float theta_goal;
  

        x_goal = 1;
        y_goal = -0.2;
        theta_goal = theta_odom_2 + 135;       
        
        // else if(j == 3) {
        //     x_goal = x_odom_2; 
        //     y_goal = y_odom_2;
        //     theta_goal = theta_odom_2 + 180;       
        // }

        //float gamma = theta_odom_2 - 45;     // 45 Grad, entsprechend der Geometrie des Viertelkreises
        // float x_goal = sqrt(2*radius*radius)*cos(gamma) + x_odom_2;
        // float y_goal = sqrt(2*radius*radius)*sin(gamma) + y_odom_2;
        // //float theta_goal = 0 + theta_odom_2;

        std::cout<< "x_goal 3.K: " << x_goal << std::endl;
        std::cout<< "y_goal 3.K: " << y_goal << std::endl;
        std::cout<< "theta_goal 3.K: " << theta_goal << std::endl;
     
        // Aktuellen Werte speichern (Relativ zum Startpunkt)
        odom_x = LidarPtr->odom[0]; //- sqrt(x_odom_2*x_odom_2);
        odom_y = LidarPtr->odom[1]; //- sqrt(y_odom_2*y_odom_2); 
        odom_theta = LidarPtr->odom[4];
        //angle = LidarPtr->angle;
        //distance = LidarPtr->distance;

        //lidar_x = first_x_lidar - (cos((angle*M_PI)/180)*distance); // nur zur Überprüfung der Abweichung
        //lidar_y = first_y_lidar - (sin((angle*M_PI)/180)*distance); // nur zur Überprüfung der Abweichung

        std::cout << "X 3.K: " << odom_x << std::endl;
        std::cout << "Y 3.K: " << odom_y << std::endl;
        std::cout << "Theta 3.K: " << odom_theta << std::endl;
        //std::cout << "Lidar X: " << lidar_x << std::endl;
        //std::cout << "Lidar Y: " << lidar_y << std::endl;

        // Calculate the pos. + ori. of the robot in respect to the laser and odom data

        float diff_x = x_goal - odom_x;    // ?
        float diff_y = y_goal - odom_y;        // ?
        float diff_theta = theta_goal - odom_theta;              // Für lineare Beweg. bleibt teta = 0

        float rho = sqrt(diff_x*diff_x + diff_y*diff_y);    
        float alpha = diff_theta + atan2(diff_y,diff_x);
        float beta = diff_theta - alpha;

        k_alpha = 0.2;
        k_beta = -0.05;
        k_rho  = 0.1;

        float v = k_rho * rho;
        float w = k_alpha * alpha + k_beta * beta;
            
        if(v > 0.06){     
            v = 0.06;
        }

        if(w > 0.07){
            w = 0.07;
        }

        msg = "---START---{\"linear\": " + to_string(v) + ", \"angular\": " + to_string(w) + "}___END___";
        strcpy(sendString, msg.c_str()); // Befehl zum Fahren

        std::cout << msg << std::endl;
        // std::cout << angle << std::endl;
        // std::cout << distance << std::endl;

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

        if (diff_x<0.1 && diff_y<0.1 && diff_theta<4)
        {
            cout << "Erster Kreispunkt erreicht" << endl;
            cout << "....................................." << endl;
            msg = "---START---{\"linear\": 0.0, \"angular\": 0.00}___END___";
            strcpy(sendString, msg.c_str());
            sleep(1);
            break;
        }
        
    }

}
    // Speichern der aktullen Position, da diese später erneut angefahren werden muss
    // float x_odom_3 = LidarPtr->odom[0];
    // float y_odom_3 = LidarPtr->odom[1];
    // float theta_odom_3 = LidarPtr->odom[4];


//     // 2. VIERTEL-KREIS
//     for (;;)
//     {
       
//         float gamma = theta_odom_3 + 45;     // entspricht den Winkelbeziehungen im Kreis
//         float x_goal = sqrt(2*radius*radius)*cos(gamma) + x_odom_3;
//         float y_goal = sqrt(2*radius*radius)*sin(gamma) + y_odom_3;
        
//         float theta_goal =  theta_odom_3 + 90;
     
//         // Aktuellen Werte speichern (Relativ zum Startpunkt)
//         odom_x = LidarPtr->odom[0] - x_odom_3;
//         odom_y = LidarPtr->odom[1] - y_odom_3; 
//         odom_theta = LidarPtr->odom[4] - theta_odom_3;
//         //angle = LidarPtr->angle;
//         //distance = LidarPtr->distance;

//         //lidar_x = first_x_lidar - (cos((angle*M_PI)/180)*distance); // nur zur Überprüfung der Abweichung
//         //lidar_y = first_y_lidar - (sin((angle*M_PI)/180)*distance); // nur zur Überprüfung der Abweichung

//         std::cout << "X: " << odom_x << std::endl;
//         std::cout << "Y: " << odom_y << std::endl;
//         std::cout << "Theta: " << odom_theta << std::endl;
//         //std::cout << "Lidar X: " << lidar_x << std::endl;
//         //std::cout << "Lidar Y: " << lidar_y << std::endl;

//         // Calculate the pos. + ori. of the robot in respect to the laser and odom data

//         float diff_x = x_goal - odom_x;    // ?
//         float diff_y = y_goal - odom_y;        // ?
//         float diff_theta = theta_goal - odom_theta;                                // Für lineare Beweg. bleibt teta = 0

//         std::cout<< "x_goal: " << x_goal << std::endl;
//         std::cout<< "y_goal: " << y_goal << std::endl;
//         std::cout<< "theta_goal: " << theta_goal << std::endl;

//         float rho = sqrt(diff_x*diff_x + diff_y*diff_y);    
//         float alpha = -diff_theta + atan2(diff_y,diff_x);
//         float beta = -diff_theta - alpha;

//         k_alpha = 0.2;
//         k_beta = -0.05;

//         float v = k_rho * rho;
//         float w = k_alpha * alpha + k_beta * beta;
            
//         if(v > 0.06){     
//             v = 0.06;
//         }

//         if(w > 0.07){
//             w = 0.07;
//         }

//         msg = "---START---{\"linear\": " + to_string(v) + ", \"angular\": " + to_string(w) + "}___END___";
//         strcpy(sendString, msg.c_str()); // Befehl zum Fahren

//         std::cout << msg << std::endl;
//         // std::cout << angle << std::endl;
//         // std::cout << distance << std::endl;

//         if ((sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
//         {
//             DieWithError("socket() failed");
//         }

//         /* Construct the server address structure */
//         memset(&echoServAddr, 0, sizeof(echoServAddr)); /* Zero out structure */
//         echoServAddr.sin_family = AF_INET;              /* Internet address family */
//         echoServAddr.sin_addr.s_addr = inet_addr(servIP);   /* Server IP address */
//         echoServAddr.sin_port = htons(echoServPort);           /* Server port */

//         if (connect(sock, (struct sockaddr *)&echoServAddr, sizeof(echoServAddr)) < 0)
//         {
//             DieWithError("connect() failed");
//         }
//         sendStringLen = msg.length();
//         if (sendto(sock, sendString, sendStringLen, 0, (struct sockaddr *)&echoServAddr, sizeof(echoServAddr)) != sendStringLen)
//             DieWithError("sendto() sent a different number of bytes than expected");
//         close(sock);


//         if (diff_x<0.07)
//         {
//             cout << "Zweiter Kreispunkt erreicht" << endl;
//             cout << "....................................." << endl;

//             break;
//         }

//         sleep(0.1);
//     }

//     sleep(1);
//     /* NOT REACHED */
// }


