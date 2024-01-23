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

// ----------------.-------SHARED MEMORY----------------------------
struct SharedMemoryLidar 
{
    float distance;
    int angle;
    float odom[10];
};

int LidarID;                //Globale Variablen für den Zugriff auf den Shared Memory und die Semaphore.
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

        //--------------------------- SEMAPHOREN-------------------------------------

        int ID_LIDAR;

        union semun 
        {
            int val;
            struct semid_ds *buf;
            ushort *array;
        } arg;

        arg.val = 0;                        // initiale Wert des Semaphors wird auf 0 
        struct sembuf operations_LIDAR[1]; // Array von Strukturen vom Typ sembuf. Informationen über durchzuführende Semaphore-Operation.-> ein Array mit einer Struktur erstellt.
        int retval_LIDAR;

        ID_LIDAR = semget(Key, 1, 0666 | IPC_CREAT);    //fkt segmet= semaphor zugreifen oder erstellen. 1= anzahl

        // ------------------------------SHARED MEMORY----------------
        // Überprüfen, ob das Abrufen der Semaphor-ID erfolgreich war
        if (ID_LIDAR < 0) {
            fprintf(stderr, "Konnte mindestens eine der Semaphoren nicht erhalten.");
            exit(0);
        }

        // Setzen des Werts des Semaphors, der durch ID_LIDAR identifiziert wird, auf den angegebenen Wert (arg.val)
        if (semctl(ID_LIDAR, 0, SETVAL, arg) < 0) {
            fprintf(stderr, "Kann den Wert des Semaphors nicht setzen.");
            exit(0);
        } else {
            fprintf(stderr, "Semaphore %d initialisiert.", Key);
        }

        // -------------------------------CREATE SHARED MEMORY-------------------------------------

        // Erstellen oder Abrufen des gemeinsam genutzten Speichersegments, das durch Key identifiziert wird
        LidarID = shmget(Key, sizeof(struct SharedMemoryLidar), 0666 | IPC_CREAT);

        // Überprüfen, ob das Erstellen oder Abrufen des gemeinsam genutzten Speichersegments erfolgreich war
        if (LidarID == -1) {
            DieWithError("shmgetPos fehlgeschlagen");
            exit(EXIT_FAILURE);
        }

        // Überprüfen, ob die richtige Anzahl von Befehlszeilenargumenten vorhanden ist
        if ((argc < 3) || (argc > 4)) {
            fprintf(stderr, "Verwendung: %s <Server-IP> <Echo-Wort> [<Echo-Port>]\n", argv[0]);
            exit(1);
        }

        // Werte von Befehlszeilenargumenten Variablen zuweisen
        servIP = argv[1];             // Server-IP-Adresse
        echoString = argv[2];         // Zu echoender String

        // Verwenden des angegebenen Ports oder Standardport 7, wenn nicht angegeben
        if (argc == 4)
            echoServPort = atoi(argv[3]); // Verwenden Sie den angegebenen Port
        else
            echoServPort = 7;  // Standardport für den Echo-Dienst

        //----------------------------- SEMAPHOREN-----------------------------------------------------

        // Abrufen der Semaphor-ID für den vorhandenen Semaphor-Satz, der durch Key identifiziert wird
        ID_LIDAR = semget(Key, 1, 0666);

        // Überprüfen, ob das Abrufen der Semaphor-ID erfolgreich war
        if (ID_LIDAR < 0) {
            fprintf(stderr, "Programm semb kann den Semaphor nicht finden, wird beendet.\n");
            exit(0);
        }
        //--------------- SHARED MEMORY-----------------------------------------------------

        // An das durch LidarID identifizierte gemeinsam genutzte Speichersegment anhängen
        LidarPtr = (SharedMemoryLidar *)shmat(LidarID, NULL, 0);

        // Überprüfen, ob das Anhängen an das gemeinsam genutzte Speichersegment erfolgreich war
        if (LidarPtr == (SharedMemoryLidar *)-1) {
            DieWithError("shmat fehlgeschlagen");
            exit(EXIT_FAILURE);
        }

        // Erstellen Sie einen zuverlässigen, Stream-Socket mit TCP
        if ((sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
            DieWithError("socket() fehlgeschlagen");
        }

        // Struktur für die Serveradresse erstellen
        memset(&echoServAddr, 0, sizeof(echoServAddr));     // Struktur nullen
        echoServAddr.sin_family      = AF_INET;             // Internet-Adressfamilie
        echoServAddr.sin_addr.s_addr = inet_addr(servIP);   // Server-IP-Adresse
        echoServAddr.sin_port        = htons(echoServPort); // Server-Port

        // Verbindung zum Echo-Server herstellen
        if (connect(sock, (struct sockaddr *) &echoServAddr, sizeof(echoServAddr)) < 0) {
            DieWithError("connect() fehlgeschlagen");
        }

        // Eingabelänge bestimmen (Länge des zu sendenden Strings)
        echoStringLen = strlen(echoString);

        // Den String an den Server senden
        if (send(sock, echoString, echoStringLen, 0) != echoStringLen) {
            DieWithError("send() hat eine andere Anzahl von Bytes gesendet als erwartet");
        }

        // Den gleichen String vom Server empfangen
        totalBytesRcvd = 0;
        string text;
        string inputReceived;

        // Daten vom Server empfangen, bis "END" erreicht ist
        for (;;) {
            if ((bytesRcvd = recv(sock, echoBuffer, RCVBUFSIZE - 1, 0)) <= 0) {
                DieWithError("recv() fehlgeschlagen oder Verbindung vorzeitig geschlossen");
            }
            totalBytesRcvd += bytesRcvd;
            echoBuffer[bytesRcvd + 1] = '\0'; 

            text = text.append(echoBuffer);
            if (text.find("END") != string::npos) {
                close(sock);
                break;
            }    
        }

        // Eingabe-Stream-Werte vom Robot-Server verarbeiten und speichern
        inputReceived = text;
        size_t elem;
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
                // Extrahiere den Substring bis zum Komma
                zwispei = inputReceived.substr(0, elem);
                
                // Konvertiere den Substring in eine Gleitkommazahl und speichere sie im Array
                ranges[i] = stof(zwispei);
                
                // Entferne den verarbeiteten Teil des Eingabestrings
                inputReceived.erase(0, elem + 1);
            }
            catch(std::invalid_argument &zwispei) // Wenn ungültiger Parameter (z.B. Buchstaben)
            {
                break;        
            }
            catch(std::out_of_range &zwispei) // Wenn Länge des Strings (Arrays) überschritten
            {
                break;        
            }
            i++;
        }

        // Ausgabe der gespeicherten Werte im Array
        for(i = 0; i < size; i++)
        {
            std::cout << i << ": " << ranges[i] << std::endl;
        }

        // FILTER FÜR BEREICH 0.02 - 0.08
        for (i = 0; i <= 360; i++)
        {
            if (ranges[i] < 0.02 || ranges[i] > 0.8)
            {
                // Setze Werte außerhalb des Bereichs auf 0
                ranges[i] = 0;
            }
        }

        //------------------- SEMAPHOREN------------------------------------------

        // Warte, bis der Semaphorwert 0 erreicht
        while (semctl(ID_LIDAR, 0, GETVAL) != 0)
            ;

        /* Setze die sembuf-Struktur. */
        operations_LIDAR[0].sem_num = 0; /* Welcher Semaphor im Semaphor-Array: */
        operations_LIDAR[0].sem_op = +1; /* Welche Operation? 1 von Semaphorwert subtrahieren: */
        operations_LIDAR[0].sem_flg = 0; /* Setze die Flagge, damit wir warten: */

        // Führe die Semaphor-Inkrement-Operation durch
        retval_LIDAR = semop(ID_LIDAR, operations_LIDAR, 1);
        if (retval_LIDAR != 0)
        {
            printf("semb: V-Operation war nicht erfolgreich.\n");
        }

        // KLEINSTER RANGE-WERT INKL. WINKEL SPEICHERN
        LidarPtr->distance = 1;
        for (i = 0; i <= 360; i++)
        {
            if (ranges[i] > 0 && ranges[i] < LidarPtr->distance)
            {
                // Speichere den Winkel und die Entfernung des kleinsten Range-Werts
                LidarPtr->angle = i;
                LidarPtr->distance = ranges[i];
            }
        }
        cout << LidarPtr->angle << endl;
        cout << LidarPtr->distance << endl;

        //--------------------------- SEMAPHOREN--------------------

        // Setze die sembuf-Struktur für Semaphor-Dekrement
        operations_LIDAR[0].sem_num = 0; /* Welcher Semaphor im Semaphor-Array: */
        operations_LIDAR[0].sem_op = -1; /* Welche Operation? 1 von Semaphorwert subtrahieren: */
        operations_LIDAR[0].sem_flg = 0; /* Setze die Flagge, damit wir warten: */

        // Führe die Semaphor-Dekrement-Operation durch
        retval_LIDAR = semop(ID_LIDAR, operations_LIDAR, 1);
        if (retval_LIDAR != 0)
        {
            printf("semb: V-Operation war nicht erfolgreich.\n");
        }

        // Kurze Pause
        sleep(0.1); /*Ein wenig warten*/

    }
}