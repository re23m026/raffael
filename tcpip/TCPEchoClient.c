#include <stdio.h>      /* for printf() and fprintf() */
#include <sys/socket.h> /* for socket(), connect(), send(), and recv() */
#include <arpa/inet.h>  /* for sockaddr_in and inet_addr() */
#include <stdlib.h>     /* for atoi() and exit() */
#include <string.h>     /* for memset() */
#include <unistd.h>     /* for close() */
#include <string.h>

#define RCVBUFSIZE 2000   /* Size of receive buffer */    // Wird für uns noch komplexer werden, weil wir nicht wissen wie viele Infos,... kommmen. 32 safe anpassen
                        // Client sendet zwar was, muss ja aber auch was zurück empfangen -- wissen aber nicht wie viel empfangen wird 

void DieWithError(char *errorMessage);  /* Error handling function */

int main(int argc, char *argv[]) 
{
    int sock;                        /* Socket descriptor */
    struct sockaddr_in echoServAddr; /* Echo server address */                  // Hiermit wird später über memset() Server Adresse angelegt/erstellt --> STRUCT: strukturierte Var. mit Untervariablen
    unsigned short echoServPort;     /* Echo server port */
    char *servIP;                    /* Server IP address (dotted quad) */      // Pointer auf String
    char *echoString;                /* String to send to echo server */        // Pointer auf String
    char echoBuffer[RCVBUFSIZE];     /* Buffer for echo string */               // Für das Wiederempfangen der gesendeten Message
    unsigned int echoStringLen;      /* Length of string to echo */
    int bytesRcvd, totalBytesRcvd = 0;   /* Bytes read in single recv() 
                                        and total bytes read */

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
    printf("Received: ");                /* Setup to print the echoed string */
    
    // while (totalBytesRcvd < echoStringLen)
    char *text;
    char *text2;
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

    text2 = text;

    printf(text2);

    printf(text);

    printf("\n");    /* Print a final linefeed */

    close(sock);
    exit(0);
}
