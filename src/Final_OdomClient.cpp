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

#define RCVBUFSIZE 20000 /* Größe des Empfangsbuffers */
#define Key (816)

// Struktur für gemeinsam genutzten Speicher mit Lidar-Daten
struct SharedMemoryLidar
{
    float distance;
    int angle;
    float odom[10];
};

// Variablen für gemeinsam genutzten Speicher, Semaphore und IDs
int LidarID;                  // Shared Memory-ID
SharedMemoryLidar *LidarPtr;  // Zeiger auf den gemeinsam genutzten Speicher
int mutex, empty, full, init; // Semaphore-IDs COPIED

// Funktion zur Behandlung von Fehlern
void DieWithError(const char *errorMessage)
{
    perror(errorMessage);
    exit(1);
}

int main(int argc, char *argv[])
{
 for (;;)
    {
        int sock;                        /* Socket-Deskriptor */
        struct sockaddr_in echoServAddr; /* Adresse des Echo-Servers */
        unsigned short echoServPort;     /* Port des Echo-Servers */
        char *servIP;                    /* IP-Adresse des Servers (dotted quad) */
        char *echoString;                /* String, der an den Echo-Server gesendet wird */
        char echoBuffer[RCVBUFSIZE];     /* Buffer für den Echo-String */
        unsigned int echoStringLen;      /* Länge des zu echoenden Strings */
        int bytesRcvd, totalBytesRcvd;   /* Bytes gelesen in einer einzelnen recv()
                                            und insgesamt gelesene Bytes */
        float odom[10];

        //------------------------- SEMAPHOREN -----------------------------------------------------------------------------------

        int ID_LIDAR;
        union semun
        {
            int val;
            struct semid_ds *buf;
            ushort *array;
        } arg;

        arg.val = 0;                   // Initialwert des Semaphors wird auf 0 gesetzt
        struct sembuf operations_LIDAR[1]; // Funktioniert wie ein Segment, auf das das Semaphore zugreifen oder erstellt werden soll. 1 = Anzahl der Semaphoren

        int retval_LIDAR; // Gibt den Wert von semop() zurück

        // --------------------------- SHARED MEMORY------------------------------------------------
        ID_LIDAR = semget(Key, 1, 0666 | IPC_CREAT); //

        if (ID_LIDAR < 0)
        {
            fprintf(stderr, "Kann eines der Semaphoren nicht erhalten. \n");
            exit(0);
        }

        if ((argc < 3) || (argc > 4)) /* Test auf die richtige Anzahl von Argumenten */
        {
            fprintf(stderr, "Verwendung: %s <Server IP> <Echo Wort> [<Echo Port>]\n", // Mindestens 3 Argumente erforderlich (Verwendung = Programmname)
                    argv[0]);
            exit(1);
        }

        servIP = argv[1];          /* Erstes Argument: Server IP-Adresse (dotted quad) */
        echoString = argv[2];      /* Zweites Argument: zu echoender String */

        if (argc == 4)
            echoServPort = atoi(argv[3]); /* Verwende den angegebenen Port, falls vorhanden */
        else
            echoServPort = 7; /* 7 ist der bekannte Port für den Echo-Dienst */ // Wenn Argument <Echo Port> nicht übergeben wird, standardmäßig auf 7 gesetzt
 // -----------------------------------Semaphoren-------------------------------------------------
        LidarID = semget(Key, 1, 0666);
        if (ID_LIDAR < 0)
        {
            fprintf(stderr, "Kann eines der Semaphoren nicht erhalten.\n");
            exit(0);
        }

        if (semctl(ID_LIDAR, 0, SETVAL, arg) < 0)
        {
            fprintf(stderr, "Kann den Semaphore-Wert nicht setzen.\n");
            exit(0);
        }
        else
        {
            fprintf(stderr, "Semaphore %d initialisiert.\n", Key);
        }

        LidarID = shmget(Key, sizeof(struct SharedMemoryLidar), 0666 | IPC_CREAT);
        if (LidarID == -1)
        {
            DieWithError("shmgetPos fehlgeschlagen");
            exit(EXIT_FAILURE);
        }
        //--------------------- Shared Memory -------------------------- 
        LidarPtr = (SharedMemoryLidar *)shmat(LidarID, NULL, 0);
        if (LidarPtr == (SharedMemoryLidar *)-1)
        {
            DieWithError("shmat fehlgeschlagen");
            exit(EXIT_FAILURE);
        }

        // ERSTELLEN EINER ZUVERLÄSSIGEN STREAM-SOCKET MIT TCP

        /* Erstelle eine zuverlässige, streamorientierte Socket-Verbindung mit TCP */ // Zuerst die Buchse erstellen, in die später der Port gesteckt wird
        if ((sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)                // PF_INET = Internetprotokoll = TCP/IP Protokoll ;; Soll STREAMING-Protokoll sein, bedeutet: Daten sollen fortlaufend gesendet und empfangen werden können
            DieWithError("socket() fehlgeschlagen");

        // Bis hier wurde nur die "Buchse" erstellt
        // Jetzt die Verbindung vorbereiten --> Wir brauchen die "Serveradresse"

        /* Konstruiere die Server-Adressstruktur */
        memset(&echoServAddr, 0, sizeof(echoServAddr));          /* Nullt die Struktur aus */ // Serveradresse erstellt
        echoServAddr.sin_family = AF_INET;                       /* Internet-Adressfamilie */  // 4 x 3 Zahlengruppen
        echoServAddr.sin_addr.s_addr = inet_addr(servIP);        /* Server-IP-Adresse */
        echoServAddr.sin_port = htons(echoServPort);             /* Server-Port */             // Beim Server handelt es sich hier um den Local Port

        // In der "strukturierten" Variable echoServAddr werden die erforderlichen Untervariablen gespeichert (servIP, echoServPort, ...)
        // Wenn die Variable echoServAddr vollständig mit allen notwendigen Informationen gefüllt ist, kann versucht werden, eine Verbindung herzustellen.

        /* Stelle die Verbindung zum Echo-Server her */
        if (connect(sock, (struct sockaddr *)&echoServAddr, sizeof(echoServAddr)) < 0) // Über die echoServAddr versuchen wir, eine Verbindung zur sockaddr herzustellen
            DieWithError("connect() fehlgeschlagen");                                   // Wenn es klappt, erhalten wir eine Zahl, die > 0 ist

        // Wenn es klappt, wurde erfolgreich eine Verbindung zwischen Server und Client hergestellt (nicht nur Sockel und Port erstellt)

        echoStringLen = strlen(echoString); /* Bestimme die Länge des Eingabe-Strings */
        // Man muss wissen, wie lang der String ist, weil die Daten nicht zusammen, sondern in einzelnen Paketen gesendet werden, und beim Empfänger wieder zusammengesetzt werden müssen
        // Der Empfänger muss also wissen, wie viele Pakete insgesamt kommen

        // Jetzt versuchen, über den "Sockel" etwas zu senden
        /* Sende den String an den Server */
        if (send(sock, echoString, echoStringLen, 0) != echoStringLen) // Die Anzahl der Pakete, die wir gesendet haben (Zahl), wird uns zurückgegeben
            DieWithError("send() hat eine andere Anzahl von Bytes gesendet als erwartet"); // Wenn die Anzahl der Pakete, die gesendet werden sollen (echoStringLen), nicht der Anzahl der tatsächlich gesendeten Pakete entspricht (send...) --> Fehler

        // Das, was wir gesendet haben, wollen/müssen wir auch wieder empfangen ("Echo")
        /* Empfange den gleichen String vom Server zurück */
        totalBytesRcvd = 0;
        // printf("Empfangen: ");                /* Einrichten zum Drucken des echo-Strings */

        
        // while (totalBytesRcvd < echoStringLen)
        
        
       string input;
        string inputReceived;
        string inputReceived2;
        float x;
        float y;
        float z;
        float w;

        for (;;)
        {
            /* Empfange bis zu die Puffergröße (minus 1, um Platz für
            einen Null-Terminator zu lassen) Bytes vom Sender */
            if ((bytesRcvd = recv(sock, echoBuffer, RCVBUFSIZE - 1, 0)) <= 0) // Wenn es nicht klappt, < 0
                DieWithError("recv() fehlgeschlagen oder Verbindung vorzeitig geschlossen"); // Wenn es nicht geklappt hat --> Handler
            totalBytesRcvd += bytesRcvd; /* Behalte die Gesamtanzahl der Bytes im Auge */ // Was wir tatsächlich empfangen haben, wird "totalBytesRcvd" hinzugefügt (+=)
            echoBuffer[bytesRcvd + 1] = '\0'; /* Beende den String! */                  // Wird auch in "echoBuffer" geschrieben ( = '/0' bedeutet ... Ende des Strings)

            // printf("%s", echoBuffer);      /* Drucke den Echo-Buffer */

            input = input.append(echoBuffer);
            if (input.find("END") != string::npos)
            {
                close(sock);
                break;
            }
        }

        // Hier werden alle Werte des Input-Streams vom Robot-Server gespeichert
        // 2 Variablen speichern Odom-Werte, da wir den String an 2 verschiedenen Stellen aufteilen müssen (input + inputReceived)

        inputReceived = input;
        inputReceived2 = input;
        size_t elem;
        string getValue;

        // Pose speichern (Position bis Orientierung)
        elem = input.find("position");
        input = input.substr(elem, input.length());

        input = input.substr(0, elem);

        // Einzelne Parameter (x, y, z, ... w) aus dem String 'input' abspeichern

        // X
        getValue = input;
        elem = getValue.find("x");
        getValue = getValue.substr(elem + 3, getValue.length());
        elem = getValue.find(",");
        getValue = getValue.substr(0, elem);
        odom[0] = stof(getValue);

        // Y
        getValue = input;
        elem = getValue.find("y");
        getValue = getValue.substr(elem + 3, getValue.length());
        elem = getValue.find(",");
        getValue = getValue.substr(0, elem);
        odom[1] = stof(getValue);

        // Array schneiden ab Orientation
        elem = inputReceived2.find("orientation");
        inputReceived2 = inputReceived2.substr(elem, input.length());
        inputReceived2 = inputReceived2.substr(0, elem);

        // Orientation X
        getValue = inputReceived2;
        elem = getValue.find("x");
        getValue = getValue.substr(elem + 3, getValue.length());
        elem = getValue.find(",");
        getValue = getValue.substr(0, elem);
        x = stof(getValue);

        // Orientation Y
        getValue = inputReceived2;
        elem = getValue.find("y");
        getValue = getValue.substr(elem + 3, getValue.length());
        elem = getValue.find(",");
        getValue = getValue.substr(0, elem);
        y = stof(getValue);

        // Orientation Z
        getValue = inputReceived2;
        elem = getValue.find("z");
        getValue = getValue.substr(elem + 3, getValue.length());
        elem = getValue.find(",");
        getValue = getValue.substr(0, elem);
        z = stof(getValue);

        // Orientation W
        getValue = inputReceived2;
        elem = getValue.find("w");
        getValue = getValue.substr(elem + 3, getValue.length());
        elem = getValue.find(",");
        getValue = getValue.substr(0, elem);
        w = stof(getValue);

        // Linear-Angular-Werte speichern
        elem = inputReceived.find("linear");
        inputReceived = inputReceived.substr(elem, inputReceived.length());

        elem = inputReceived.find("covariance");
        inputReceived = inputReceived.substr(0, elem - 5);

        // Lineargeschwindigkeit (in x)
        getValue = inputReceived;
        elem = getValue.find("x");
        getValue = getValue.substr(elem + 3, getValue.length());
        elem = getValue.find(",");
        getValue = getValue.substr(0, elem);
        odom[2] = stof(getValue);

        // Winkelgeschwindigkeit (um z)
        getValue = inputReceived;
        elem = getValue.find("angular");
        getValue = getValue.substr(elem + 3, getValue.length());
        elem = getValue.find("z");
        getValue = getValue.substr(elem + 3, getValue.length());
        elem = getValue.find(",");
        getValue = getValue.substr(0, elem);
        odom[3] = stof(getValue);

        // Berechnung von Theta

        float theta_in_rad;
        float theta_in_deg;

        theta_in_rad = atan2(2 * (w * z + x * y), 1 - 2 * ((y * y) + (z * z)));
        theta_in_deg = theta_in_rad * 180 / M_PI;

        odom[4] = theta_in_deg;

        // Terminalausgabe des Odom-Arrays
        for (int i = 0; i < 5; i++)
        {
            std::cout << odom[i] << std::endl;
        }

        // Warten, bis der Semaphore-Wert 0 erreicht
        while (semctl(ID_LIDAR, 0, GETVAL) != 0)
            ;

        // Semaphore-Struktur für Inkrementierung
        operations_LIDAR[0].sem_num = 0;
        operations_LIDAR[0].sem_op = +1;
        operations_LIDAR[0].sem_flg = 0;

        // Semaphore-Inkrementierung durchführen
        retval_LIDAR = semop(ID_LIDAR, operations_LIDAR, 1);
        if (retval_LIDAR != 0)
        {
            printf("semb: V-Operation did not succeed.");
        }

        // X- und Y-Koordinaten in den Shared Memory schreiben
        LidarPtr->odom[0] = odom[0]; // x
        LidarPtr->odom[1] = odom[1]; // y
        LidarPtr->odom[2] = odom[2]; // Lineargeschwindigkeit (in x)
        LidarPtr->odom[3] = odom[3]; // Winkelgeschwindigkeit
        LidarPtr->odom[4] = odom[4]; // Theta-Winkel in Grad

        // Semaphore-Struktur für Dekrementierung
        operations_LIDAR[0].sem_num = 0;
        operations_LIDAR[0].sem_op = -1;
        operations_LIDAR[0].sem_flg = 0;

        // Semaphore-Dekrementierung durchführen
        retval_LIDAR = semop(ID_LIDAR, operations_LIDAR, 1);
        if (retval_LIDAR != 0)
        {
            printf("semb: V-operation did not succeed.\n");
        }

        // Kurze Pause
        sleep(0.1);
    }
}