#include <stdio.h>      /* for printf() and fprintf() */
#include <sys/socket.h> /* for socket(), bind(), and connect() */
#include <arpa/inet.h>  /* for sockaddr_in and inet_ntoa() */
#include <stdlib.h>     /* for atoi() and exit() */
#include <string.h>     /* for memset() */
#include <unistd.h>     /* for close() */

#define MAXPENDING 5    /* Maximum outstanding connection requests */

void DieWithError(char *errorMessage);  /* Error handling function */
void HandleTCPClient(int clntSocket);   /* TCP client handling function */  // "Können wir nachher als Sub-Thread aufbauen"

int main(int argc, char *argv[])
{
    int servSock;                    /* Socket descriptor for server */
    int clntSock;                    /* Socket descriptor for client */     // Wird später verwendet, wenn auf Client gelistened wird
    struct sockaddr_in echoServAddr; /* Local address */
    struct sockaddr_in echoClntAddr; /* Client address */
    unsigned short echoServPort;     /* Server port */
    unsigned int clntLen;            /* Length of client address data structure */

    if (argc != 2)     /* Test for correct number of arguments */   // Brauchen Name und Port
    {
        fprintf(stderr, "Usage:  %s <Server Port>\n", argv[0]);
        exit(1);
    }

    echoServPort = atoi(argv[1]);  /* First arg:  local port */

    /* Create socket for incoming connections */    // Wieder erstmal Socker erstellen mit gleichen Attr. wie Client
    if ((servSock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
        DieWithError("socket() failed");
      
    // Zusammenbauen der Server-Adresse  
    /* Construct local address structure */
    memset(&echoServAddr, 0, sizeof(echoServAddr));   /* Zero out structure */
    echoServAddr.sin_family = AF_INET;                /* Internet address family */
    echoServAddr.sin_addr.s_addr = htonl(INADDR_ANY); /* Any incoming interface */
    echoServAddr.sin_port = htons(echoServPort);      /* Local port */

    // "Jetzt lokalen Socker zum Leben zu erwecken"
    /* Bind to the local address */     // bind vordef. Funktion von socket
    if (bind(servSock, (struct sockaddr *) &echoServAddr, sizeof(echoServAddr)) < 0)        // sockaddr z.B. unsere 10000
        DieWithError("bind() failed");

    // Wenn das klappt erhalten wir "Server Socket", mit dem wir dann arbeiten können...
    // Durch sockaddr & echoServAddr wird "Stecker" eingesteckt/hergestellt

    // Jetzt warten wir auf einen Client, der sich versucht mit dem Socket zu verbinden
    /* Mark the socket so it will listen for incoming connections */
    if (listen(servSock, MAXPENDING) < 0)       // listen - Gegenteil von "connect"
        DieWithError("listen() failed");

    for (;;) /* Run forever */  // oder while True
    {
        /* Set the size of the in-out parameter */
        clntLen = sizeof(echoClntAddr);

        /* Wait for a client to connect */
        // Bsp: Client1 verbindet sich mit Server über Port 10000, jedoch ist Port 10000 ja nur der "Stecker"
        // Auch andere Clients können sich ja mit dem Server über Port 10000 verbinden
        // Deswegen findet private Kommunikation mit Client1 über einen vom Server festgelegten Port (z.B. 15000) statt
        // Dieser wird in "clntSock" gespeichert
        // clntSock wird dann dem Progr. HandleTCPClient übergeben --> dort findet dann die eigentliche Verarbeitung statt 

        if ((clntSock = accept(servSock, (struct sockaddr *) &echoClntAddr, 
                               &clntLen)) < 0)
            DieWithError("accept() failed");

        /* clntSock is connected to a client! */

        printf("Handling client %s\n", inet_ntoa(echoClntAddr.sin_addr));

        HandleTCPClient(clntSock);
    }
    /* NOT REACHED */
}
