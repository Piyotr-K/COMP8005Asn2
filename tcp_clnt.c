/*---------------------------------------------------------------------------------------
--	SOURCE FILE:		tcp_clnt.c - A simple TCP client program.
--
--	PROGRAM:		tclnt.exe
--
--	FUNCTIONS:		Berkeley Socket API
--
--	DATE:			January 23, 2001
--
--	REVISIONS:		(Date and Description)
--				January 2005
--				Modified the read loop to use fgets.
--				While loop is based on the buffer length
--
--
--	DESIGNERS:		Aman Abdulla
--
--	PROGRAMMERS:		Aman Abdulla
--
--	NOTES:
--	The program will establish a TCP connection to a user specifed server.
-- 	The server can be specified using a fully qualified domain name or and
--	IP address. After the connection has been established the user will be
-- 	prompted for date. The date string is then sent to the server and the
-- 	response (echo) back from the server is displayed.
---------------------------------------------------------------------------------------*/
#include <pthread.h>
#include <stdio.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <stdlib.h>
#include <strings.h>
#include <unistd.h>
#include <arpa/inet.h>

// Function Prototypes
void *ClntConnection(void *data);

#define SERVER_TCP_PORT 7000 // Default port
#define BUFLEN 80           // Buffer length

//Struct
struct ConArgs
{
    int port;
    char *host;
};

pthread_mutex_t lock;

int main(int argc, char **argv)
{
    int i, n, bytes_to_read;
    int sd, port;
    struct hostent *hp;
    struct sockaddr_in server;
    struct ConArgs connectionArgs;
    struct ConArgs *argPT;

    char *host, *bp, rbuf[BUFLEN], sbuf[BUFLEN], **pptr, *sptr;
    char str[16];
    int numOfThreads = 1;

    switch (argc)
    {
    case 2:
        host = argv[1]; // Host name
        port = SERVER_TCP_PORT;
        break;
    case 3:
        host = argv[1];
        port = atoi(argv[2]); // User specified port
        break;
    case 4:
        host = argv[1];
        port = atoi(argv[2]);
        numOfThreads = atoi(argv[3]);
        break;
    default:
        fprintf(stderr, "Usage: %s host [port] [number of threads]\n", argv[0]);
        exit(1);
    }
    connectionArgs.host = host;
    connectionArgs.port = port;
    argPT = &connectionArgs;

    //Creates list of threads
    pthread_t threadList[numOfThreads];

    //Create # of clients
    for (i = 0; i < numOfThreads; i++)
    {
        sleep(0.1);
        pthread_create(&threadList[i], NULL, ClntConnection, (void *)argPT);
    }

    //Let main function finish when all threads finish
    for (i = 0; i < numOfThreads; i++)
    {
        pthread_join(threadList[i], NULL);
    }
    printf("Done\n");
    return (0);
}

void *ClntConnection(void *data)
{
    struct ConArgs *connectionArgs = data;
    int port = connectionArgs->port;
    char *host = connectionArgs->host;
    int n, bytes_to_read, sd;
    struct hostent *hp;
    struct sockaddr_in server;
    char *bp, rbuf[BUFLEN], sbuf[BUFLEN], **pptr, *sptr;
    char str[16];

    //Mutex lock important Memory functions to prevent segmentation faults
    pthread_mutex_lock(&lock);

    // Open the text file and read it for data for sending

    //Ensure ulimit is a high value when testing: ulimit -n ####
    FILE *fp = fopen("alice.txt", "r");

    // Create the socket
    if ((sd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        perror("Cannot create socket");
        exit(1);
    }

    bzero((char *)&server, sizeof(struct sockaddr_in));

    server.sin_family = AF_INET;
    server.sin_port = htons(port);

    if ((hp = gethostbyname(host)) == NULL)
    {
        fprintf(stderr, "Unknown server address\n");
        exit(1);
    }

    bcopy(hp->h_addr, (char *)&server.sin_addr, hp->h_length);

    // Connecting to the server
    if (connect(sd, (struct sockaddr *)&server, sizeof(server)) == -1)
    {
        fprintf(stderr, "Can't connect to server\n");
        perror("connect");
        exit(1);
    }
    // printf("Connected: Server Name: %s\n", hp->h_name);
    pptr = hp->h_addr_list;
    // printf("\t\tIP Address: %s\n", inet_ntop(hp->h_addrtype, *pptr, str, sizeof(str)));

    //Enters Loop to periodically Send text
    //gets(sbuf); // get user's text

    //Finished Memory stuff, let threads send stuff now
    pthread_mutex_unlock(&lock);

    while (fgets(sbuf, BUFLEN, fp) != 0)
    {

        // printf("Now sleeping\n");
        // sleep(1);

        //Get from file
        // printf("Transmit:\n");
        // printf("%s", sbuf);
        write(sd, sbuf, BUFLEN);

        //Set up receive
        // printf("Receive:\n");
        bp = rbuf;
        bytes_to_read = BUFLEN;

        // client makes repeated calls to recv until no more data is expected to arrive.
        n = 0;
        while ((n = recv(sd, bp, bytes_to_read, 0)) < BUFLEN)
        {
            bp += n;
            bytes_to_read -= n;
        }
        // printf("%s\n", rbuf);
        // fflush(stdout);
    }

    sbuf[0] = '\0';

    // printf("Finished with reading file, sending EOF\n");
    //Send last string with ending line
    write(sd, sbuf, BUFLEN);

    //Properly close file when finished
    fclose(fp);
    close(sd);
    // printf("%d done\n", pthread_self());
    return NULL;
}
