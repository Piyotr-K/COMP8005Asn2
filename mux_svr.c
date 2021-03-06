/*---------------------------------------------------------------------------------------
--	SOURCE FILE:		mux_svr.c -   A simple multiplexed echo server using TCP
--
--	PROGRAM:		mux.exe
--
--	FUNCTIONS:		Berkeley Socket API
--
--	DATE:			February 18, 2001
--
--	REVISIONS:		(Date and Description)
--				February 20, 2008
--				Added a proper read loop
--				Added REUSEADDR
--				Added fatal error wrapper function
--
--
--	DESIGNERS:		Based on Richard Stevens Example, p165-166
--				Modified & redesigned: Aman Abdulla: February 16, 2001
--
--
--	PROGRAMMER:		Aman Abdulla
--
--	NOTES:
--	The program will accept TCP connections from multiple client machines.
-- 	The program will read data from each client socket and simply echo it back.
---------------------------------------------------------------------------------------*/
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <strings.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <time.h>

#define SERVER_TCP_PORT 7001 // Default port
# define BUFLEN 80 //Buffer length
# define TRUE 1
# define LISTENQ 5
# define MAXLINE 4096

// Function Prototypes
static void SystemFatal(const char * );

int main(int argc, char ** argv) {
    int i, maxi, nready, bytes_to_read, arg;
    int listen_sd, new_sd, sockfd, client_len, port, maxfd, client[FD_SETSIZE];

    unsigned long ip_num[FD_SETSIZE]; // Saves ip address of the connected clients
    int portNum[FD_SETSIZE]; // Saves port numbers of the connected clients
    double startTimer[FD_SETSIZE]; // Saves start Timer of each of the clients
    int requestedGenerated[FD_SETSIZE];
    size_t dataTransfered[FD_SETSIZE];
    int clientNumber[FD_SETSIZE];
    int numOfClients = 0;
    clock_t end;
    struct sockaddr_in server, client_addr;
    char * bp, buf[BUFLEN];
    ssize_t n;
    fd_set rset, allset;
    char tmpc;
    double cpu_time_used;

    switch (argc)
	{
	    case 1:
	        port = SERVER_TCP_PORT; // Use the default port
	        break;
	    case 2:
	        port = atoi(argv[1]); // Get user specified port
	        break;
	    default:
	        fprintf(stderr, "Usage: %s [port]\n", argv[0]);
	        exit(1);
    }

    // Create a stream socket
    if ((listen_sd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
        SystemFatal("Cannot Create Socket!");

    // set SO_REUSEADDR so port can be resused imemediately after exit, i.e., after CTRL-c
    arg = 1;
    if (setsockopt(listen_sd, SOL_SOCKET, SO_REUSEADDR, & arg, sizeof(arg)) == -1)
        SystemFatal("setsockopt");

    // Bind an address to the socket
    bzero((char * ) & server, sizeof(struct sockaddr_in));
    server.sin_family = AF_INET;
    server.sin_port = htons(port);
    server.sin_addr.s_addr = htonl(INADDR_ANY); // Accept connections from any client

    if (bind(listen_sd, (struct sockaddr * ) & server, sizeof(server)) == -1)
        SystemFatal("bind error");

    // Listen for connections
    // queue up to LISTENQ connect requests
    listen(listen_sd, LISTENQ);

    maxfd = listen_sd; // initialize
    maxi = -1; // index into client[] array

    for (i = 0; i < FD_SETSIZE; i++)
        client[i] = -1; // -1 indicates available entry
    FD_ZERO( & allset);
    FD_SET(listen_sd, & allset);

    while (TRUE)
	{
        rset = allset; // structure assignment
        nready = select(maxfd + 1, & rset, NULL, NULL, NULL);

        if (FD_ISSET(listen_sd, & rset)) // new client connection
        {
            client_len = sizeof(client_addr);
            if ((new_sd = accept(listen_sd, (struct sockaddr * ) & client_addr, & client_len)) == -1)
                SystemFatal("accept error");

            // printf(" Remote Address:  %s:%hu\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

            //Accepted the a new client, increment to tell what client number they are.
            numOfClients += 1;

            for (i = 0; i < FD_SETSIZE; i++)
                if (client[i] < 0)
				{
                    client[i] = new_sd; // save descriptor
                    portNum[i] = ntohs(client_addr.sin_port); // saves the client's port number
                    ip_num[i] = inet_ntoa(client_addr.sin_addr); // save the client's ip address
                    startTimer[i] = clock();
                    clientNumber[i] = numOfClients;
                    break;
                }
            if (i == FD_SETSIZE)
			{
                printf("Too many clients\n");
                exit(1);
            }

            FD_SET(new_sd, & allset); // add new descriptor to set
            if (new_sd > maxfd)
                maxfd = new_sd; // for select

            if (i > maxi)
                maxi = i; // new max index in client[] array

            if (--nready <= 0)
                continue; // no more readable descriptors
        }

        for (i = 0; i <= maxi; i++) // check all clients for data
        {
            if ((sockfd = client[i]) < 0)
                continue;

            if (FD_ISSET(sockfd, & rset)) {
                bp = buf;
                bytes_to_read = BUFLEN;
                while ((n = read(sockfd, bp, bytes_to_read)) > 0)
                {
					bp += n;
                    bytes_to_read -= n;
                }

                //Connection should be closed when reaching EOF instead of when text is done being sent
                if (buf[0] != '\0')
                {
                    // printf("%s\n", buf);
                    requestedGenerated[i] += 1;
                    dataTransfered[i] += sizeof(buf);
                    write(sockfd, buf, BUFLEN); // echo to client
                }
                else
                {
                    end = clock();
                    cpu_time_used = ((double) (end - startTimer[i])) / CLOCKS_PER_SEC;
                    // printf("Connection #, Remote Address:Port Number, Time used, Requests Generated, Data Transfered\n");
                    // printf("====================================================\n");
                    printf("%d, %s:%hu, %lf, %d, %d\n", clientNumber[i], ip_num[i], portNum[i], cpu_time_used, requestedGenerated[i], dataTransfered[i]);
                    close(sockfd);
                    FD_CLR(sockfd, &allset);
                    client[i] = -1;
                    portNum[i] = 0;
                    startTimer[i] = 0;
                    requestedGenerated[i] = 0;
                    dataTransfered[i] = 0;
                    clientNumber[i] = -1;
                }

                if (--nready <= 0)
                    break; // no more readable descriptors
            }
        }
    }
    return (0);
}

// Prints the error stored in errno and aborts the program.
static void SystemFatal(const char * message)
{
    perror(message);
    exit(EXIT_FAILURE);
}
