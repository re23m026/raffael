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

#define RCVBUFSIZE 20000   /* Size of receive buffer */    // Wird für uns noch komplexer werden, weil wir nicht wissen wie viele Infos,... kommmen. 32 safe anpassen
                        // Client sendet zwar was, muss ja aber auch was zurück empfangen -- wissen aber nicht wie viel empfangen wird 

#define Key (816)

// SHARED MEMORY
struct SharedMemoryLidar 
{
    float distance;
    int angle;
    float odom[10];
};

int LidarID;
SharedMemoryLidar *LidarPtr;
int mutex, empty, full, init;

void DieWithError(const char *errorMessage) /* Error handling function */
{
    perror(errorMessage);
    exit(1);
}  

int main(int argc, char *argv[]) 
{
    for( ; ; ) {
        int sock;                        /* Socket descriptor */
        struct sockaddr_in echoServAddr; /* Echo server address */                  // Hiermit wird später über memset() Server Adresse angelegt/erstellt --> STRUCT: strukturierte Var. mit Untervariablen
        unsigned short echoServPort;     /* Echo server port */
        char *servIP;                    /* Server IP address (dotted quad) */      // Pointer auf String
        char *echoString;                /* String to send to echo server */        // Pointer auf String
        char echoBuffer[RCVBUFSIZE];     /* Buffer for echo string */               // Für das Wiederempfangen der gesendeten Message
        unsigned int echoStringLen;      /* Length of string to echo */
        int bytesRcvd, totalBytesRcvd;   /* Bytes read in single recv() 
                                            and total bytes read */
        float ranges[360];

        float distance;
        int angle;
        int cnt = 1;

        // SEMAPHOREN

        int ID_LIDAR;

        union semun 
        {
            int val;
            struct semid_ds *buf;
            ushort *array;
        } arg;

        arg.val = 0;
        struct sembuf operations_LIDAR[1];
        int retval_LIDAR;

        ID_LIDAR = semget(Key, 1, 0666 | IPC_CREAT);

        // SHARED MEMORY
        if (ID_LIDAR < 0){
            fprintf(stderr, "Unable to obtain at least one of the semaphores.");
            exit(0);
        }
        if (semctl(ID_LIDAR, 0, SETVAL, arg) < 0) {
            fprintf(stderr, "Cannot set semaphore value.");
            exit(0);
        }
        else {
            fprintf(stderr, "Semaphore %d initialized.", Key);
        }

        // CREATE SHARED MEMORY
        LidarID = shmget(Key, sizeof(struct SharedMemoryLidar), 0666 | IPC_CREAT);

        if (LidarID == -1)
        {
            DieWithError("shmgetPos failed");
            exit(EXIT_FAILURE);
        }

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


        // SEMAPHOREN
        ID_LIDAR = semget(Key, 1, 0666);
        if (ID_LIDAR < 0)
        {
            fprintf(stderr, "Program semb cannot find semaphore, exiting.\n");
            exit(0);
        }

        // SHARED MEMORY
        LidarPtr = (SharedMemoryLidar *)shmat(LidarID, NULL, 0);
        if (LidarPtr == (SharedMemoryLidar *)-1)
        {
            DieWithError("shmat failed");
            exit(EXIT_FAILURE);
        }


        /* Create a reliable, stream socket using TCP */            // Erstmal socket erstellen, in den der Port später gesteckt werden kann
        if ((sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) // PF_INET = Internetprotokoll = TCP/IP Protokoll ;; Soll STREAMING Protokoll sein, bedeutet: Es sollen fortlaufend Daten geschickt und empfangen werden können
            DieWithError("socket() failed");

        // Bis hier wurde jetzt erstmal nur die "Steckerbuchse" erstellt
        // Jetzt die Connection vorbereiten --> Brauchen "Serveradresse"   

        /* Construct the server address structure */
        memset(&echoServAddr, 0, sizeof(echoServAddr));     /* Zero out structure */        // Serveradresse hergestellt
        echoServAddr.sin_family      = AF_INET;             /* Internet address family */   // 4x 3 Zahlengruppen
        echoServAddr.sin_addr.s_addr = inet_addr(servIP);   /* Server IP address */
        echoServAddr.sin_port        = htons(echoServPort); /* Server port */   // Beim Server ist hier Local Port

        // In "structed" Variable echoServAddr werden die benötigten Untervariablen gepspeichert (servIP, echoServPort, ...)
        // Wenn Var. echoServAddr vollständig befüllt ist mit allen notwendigen Infos, kann versucht werden Connection aufzubauen..

        /* Establish the connection to the echo server */
        if (connect(sock, (struct sockaddr *) &echoServAddr, sizeof(echoServAddr)) < 0) // Über die echoServAddr versuchen wir Verbindung zu sockaddr herzustellen
            DieWithError("connect() failed");                                           // Wenn es klappt, kriegen wir Zahl die > 0 ist

        // Wenn klappt, haben wir erfolgreich Verbindung zw. Server und Client hergestellt (Nicht nur Socker und Port erstellt)

        echoStringLen = strlen(echoString);          /* Determine input length */
        // Man muss wissen wie lang der String ist, weil die Daten nicht zusammen sondern in einzelnen Paketen gesendet werden, und beim Empfänger wieder zsmgesetzt werden
        // Empfänger muss also wissen, wie viele Pakete insg. kommmen

        // Jetzt versuchen über den "Sock" etwas zu senden
        /* Send the string to the server */
        if (send(sock, echoString, echoStringLen, 0) != echoStringLen)      // Anzahl an Paketen die wir gesendet haben (Zahl), bekommen wir wieder zurück
            DieWithError("send() sent a different number of bytes than expected");  // Wenn Anzahl an Paketen die gesendet werden sollen (echoStringLen) nicht der Anzahl an Paketen entspricht, die wir tatsächlich gesendet haben (send...) --> Fehler

        // Das was wir weggeschickt haben, wollen/müssen wir auch wieder empfangen ("Echo")
        /* Receive the same string back from the server */
        totalBytesRcvd = 0;
        // printf("Received: ");                /* Setup to print the echoed string */
        
        // while (totalBytesRcvd < echoStringLen)
        string text;
        string inputReceived;
        for (;;)
        {
            /* Receive up to the buffer size (minus 1 to leave space for
            a null terminator) bytes from the sender */
            if ((bytesRcvd = recv(sock, echoBuffer, RCVBUFSIZE - 1, 0)) <= 0)   // Wenn es nicht klappt, < 0
                DieWithError("recv() failed or connection closed prematurely");     // Wenn es nicht geklappt hat --> Handler
            totalBytesRcvd += bytesRcvd;   /* Keep tally of total bytes */          // Was wir tatsächlich empfangen haben wird "totalBytesRcvd" hinzugefügt (+=)
            echoBuffer[bytesRcvd + 1] = '\0';  /* Terminate the string! */              // wird auch in "echoBuffer" reingeschrieben ( = '/0' bedeutet ... end of string)
            
            //printf("%s", echoBuffer);      /* Print the echo buffer */

            text = text.append(echoBuffer);
            if (text.find("END") != string::npos)  
            {
                close(sock);
                break;
            }    
        }

        // Hier werden alle Input-Stream-Werte vom Robot-Server gespeichert
        inputReceived = text;
        // std::cout << inputReceived << std::endl;

        // Aufteilen und sortieren der erhaltenen Daten und Parameter
        // Wir brauchen nur die ranges
        size_t elem;    // Element i in inputReceived
        elem = inputReceived.find("[");
        inputReceived = inputReceived.substr(elem + 1, inputReceived.length());

        elem = inputReceived.find("]");
        inputReceived = inputReceived.substr(0, elem);

        // Speichern aller Werte in einem Array
        string zwispei = "";
        int i = 0;
        int size = 360;
        while((elem = inputReceived.find(",")) != string::npos)
        {
            try
            {
                zwispei = inputReceived.substr(0, elem);
                ranges[i] = stof(zwispei);
                inputReceived.erase(0, elem + 1);
            }
            catch(std::invalid_argument &zwispei) // Wenn ungültiger Parameter (z.B. Buchstaben)
            {
                break;        
            }
            catch(std::out_of_range &zwispei) // Wenn Länge des Strings(Arrays) überschritten
            {
                break;        
            }
            i++;
        }

        for(i=0 ; i < size ; i++)
        {
            std::cout << i << ": " << ranges[i] << std::endl;
        }

        // FILTER FOR RANGE 0.02 - 0.08
        for (i = 0; i <= 360; i++)
        {
            if (ranges[i] < 0.02 || ranges[i] > 0.8)
            {
                ranges[i] = 0;
            }
        }

        // SEMBUF
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

        // KLEINSTEN RANGE-WERT INKL. WINKEL SPEICHERN
        LidarPtr->distance = 1;
        for (i = 0; i <= 360; i++)
        {

            if (ranges[i] > 0 && ranges[i] < LidarPtr->distance)
            {
                LidarPtr->angle = i;
                LidarPtr->distance = ranges[i];
            }
        }
        cout << LidarPtr->angle << endl;
        cout << LidarPtr->distance << endl;

        // SEMBUF
        operations_LIDAR[0].sem_num = 0; /* Which semaphore in the semaphore array : */
        operations_LIDAR[0].sem_op = -1; /* Which operation? Subtract 1 from semaphore value : */
        operations_LIDAR[0].sem_flg = 0; /* Set the flag so we will wait : */

        retval_LIDAR = semop(ID_LIDAR, operations_LIDAR, 1);
        if (retval_LIDAR != 0)
        {
            printf("semb: V-operation did not succeed.\n");
        }

        sleep(0.1); /*Sleep a little bit*/


        // std::cout << inputReceived << std::endl;

        // printf("\n");    /* Print a final linefeed */

        // close(sock);
        // exit(0);
    }
}
