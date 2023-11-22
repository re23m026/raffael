/* Create a reliable, stream socket using TCP */
        if ((sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
        {
            DieWithError("socket() failed");
        }

        /* Construct the server address structure */
        memset(&echoServAddr, 0, sizeof(echoServAddr)); /* Zero out structure */
        echoServAddr.sin_family = AF_INET;              /* Internet address family */
        echoServAddr.sin_addr.s_addr = inet_addr(IP);   /* Server IP address */
        echoServAddr.sin_port = htons(Port1);           /* Server port */

        /* Establish the connection to the echo server */
        if (connect(sock, (struct sockaddr *)&echoServAddr, sizeof(echoServAddr)) < 0)
        {
            DieWithError("connect() failed");
        }

        /* Receive up to the buffer size (minus 1 to leave space for
        a null terminator) bytes from the sender */
        string text;
        string text2;
        for (;;)
        {
            if ((bytesRcvd = recv(sock, echoBuffer, RCVBUFSIZE - 1, 0)) <= 0)
            {
                DieWithError("recv() failed or connection closed prematurely");
            }

            totalBytesRcvd += bytesRcvd;      /* Keep tally of total bytes */
            echoBuffer[bytesRcvd + 1] = '\0'; /* Terminate the string! */

            text = text.append(echoBuffer);
            if (text.find("END") != string::npos)
            {
                close(sock);
                break;
            }
        }