#include <stdio.h>      /* for printf() and fprintf() */
#include <sys/socket.h> /* for recv() and send() */
#include <unistd.h>     /* for close() */

#define RCVBUFSIZE 32   /* Size of receive buffer */    // Speicherplatz mit dem wir arbeiten können

// ähnlicher Aufbau wie vorher bei Client

void DieWithError(char *errorMessage);  /* Error handling function */

// clntSocket (Aus TCPServer) wird mit der Funktion dargestellt

void HandleTCPClient(int clntSocket)
{
    char echoBuffer[RCVBUFSIZE];        /* Buffer for echo string */
    int recvMsgSize;                    /* Size of received message */

    /* Receive message from client */
    // Bsp. "Hello" wird vom Client gesendet --> Aber nicht unbedingt am Stück, in Paketen! Z.B. He - ll - o
    // Keines dieser Pakete dard größer sein als 32 (Byte)
    if ((recvMsgSize = recv(clntSocket, echoBuffer, RCVBUFSIZE, 0)) < 0)
        DieWithError("recv() failed");

    // Während auf die restlichen Pakete gewartet wird, können die bereits empfangenen Pakete bereits zurückgesendet werden an Client
    /* Send received string and receive again until end of transmission */
    
    // solange noch Messages empfangen werden...
    while (recvMsgSize > 0)      /* zero indicates end of transmission */
    {
        /* Echo message back to client */
        if (send(clntSocket, echoBuffer, recvMsgSize, 0) != recvMsgSize)
            DieWithError("send() failed");

        // Schauen, ob noch weitere Daten zu empfangen sind
        /* See if there is more data to receive */
        if ((recvMsgSize = recv(clntSocket, echoBuffer, RCVBUFSIZE, 0)) < 0)
            DieWithError("recv() failed");
    }

    // Wenn alle Daten empfangen wurden, kann der spezielle clntSocket (z.B. Port 15000) wieder geschlossen werden
    close(clntSocket);    /* Close client socket */ 
}
