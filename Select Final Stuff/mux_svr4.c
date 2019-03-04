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
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <strings.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <signal.h>
#include <sched.h>

#define SERVER_TCP_PORT 7001 // Default port
# define BUFLEN 80 //Buffer length
# define TRUE 1
# define LISTENQ 5
# define MAXLINE 4096

// Function Prototypes
static void SystemFatal(const char * );
void *resetClient(int fd, int clientPos);
void *clientAction(void*);
int accessGlobal(int mode, int fd, int clientPos);

//Global Client Descripter and FD Set.
int client[FD_SETSIZE] = {-1};
fd_set clientSet, allset;;

pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

void handler(int s) {
    printf("Caught SIGPIPE\n");
    pthread_exit(NULL);
}

struct clientArgs
{
    int clientNumber;
    int clientFD;
    int clientPosition;
    struct in_addr clientIP;
    int port;
    double startTimer;
};


int main(int argc, char ** argv) {
    int i, maxClients, nready, arg;
    int listen_sd, new_sd, sockfd, client_len, port, maxfd;
    int counter = 0;

    struct sockaddr_in server, client_addr;

    ssize_t n;
    fd_set rset;
    char tmpc;
    double cpu_time_used;
    pthread_t threadList[FD_SETSIZE];

    struct clientArgs clientArguments, emptyClientArgs;
    struct clientArgs *argsPT;

    signal(SIGPIPE, handler);

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
    maxClients = -1; // index into client[] array

    for (i = 0; i < FD_SETSIZE; i++)
        client[i] = -1; // -1 indicates available entry
    FD_ZERO( & allset);
    FD_SET(listen_sd, &allset);

    FD_ZERO(&clientSet);

    while (TRUE)
	{
        pthread_mutex_lock(&lock);
        rset = allset; // structure assignment
        pthread_mutex_unlock(&lock);
        nready = select(maxfd + 1, & rset, NULL, NULL, NULL);

        //If the listener socket had something, do Add to all fd listeners
        if (FD_ISSET(listen_sd, & rset)) // new client connection
        {
            //gets client's address
            client_len = sizeof(client_addr);
            if ((new_sd = accept(listen_sd, (struct sockaddr * ) & client_addr, & client_len)) == -1)
                SystemFatal("accept error");

            // printf(" Remote Address:  %s:%hu\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

            //Accepted the a new client, increment to tell what client number they are.

            //gets the file descripter for the new client and save it. no one should be doing anything here.
            //iterate until there is an available location in the client array = 1.
            pthread_mutex_lock(&lock);
            i = accessGlobal(0, new_sd, 0);
            pthread_mutex_unlock(&lock);
            if (i >= FD_SETSIZE)
			{
                printf("Too many clients\n");
                exit(1);
            }
            pthread_mutex_lock(&lock);
            FD_SET(new_sd, &rset); // add new descriptor to set
            pthread_mutex_unlock(&lock);

            if (i > maxClients)
                maxClients = i; //Largest value in the client[] Array

            // more jobs for this iteration
            if(--nready <= 0)
                continue;
        }

        for (i = 0; i < maxi; i++)
        {
            if ((socketfd = client[i]) < 0)
            {
                continue;
            }
            if (FD_ISSET(client[i], & rset))
                {
                    // Sets up a new struct for the arguments to be passed in
                    clientArguments = emptyClientArgs;

                    //client number
                    clientArguments.clientNumber = ++counter;

                    // pass in the fd to be worked on
                    clientArguments.clientFD = new_sd;

                    //pass in their location
                    clientArguments.clientPosition = i;

                    //Ip
                    clientArguments.clientIP = client_addr.sin_addr;

                    //saves port number
                    clientArguments.port = client_addr.sin_port;

                    clientArguments.startTimer = clock();

                    argsPT = &clientArguments;

                    pthread_create(&threadList[i], NULL, &clientAction, (void *) argsPT);
                }
        }
        int threadTest = 1;
        for (i = 0; i < FD_SETSIZE; i++)
        {
            threadTest = pthread_join(threadList[i], NULL)
            if (threadTest == 0)
            {
                if (errno == ESRCH)
                {
                    errno = 0;
                    continue;
                }
            }
        }
    }
    return (0);
}

int accessGlobal(int mode, int fd, int clientPos)
{
    int i = 0;
    switch(mode)
    {
        case 0:
            for (i = 0; i < FD_SETSIZE; i++)
                if (client[i] < 0)
                {
                    client[i] = fd;
                    break;
                }
            return i;
        case 1:
            resetClient(fd, clientPos);
            break;
    }
    return -1;
}

// Client is now done. and initates a close for the main loop.
void *resetClient(int fd, int clientPos)
{
    // client is done, and initiated finish line. safetly remove them with mutex
    //Remove the client from the listening handler
    close(fd);
    FD_CLR(fd, &allset);
    //client's position in array. set back to -1 which means it's now ready for next person to use
    client[clientPos] = -1;
    pthread_mutex_unlock(&lock);
    pthread_exit(NULL);
}

// Thread Action
void *clientAction(void* data)
{
    //let each thread handle their own client. and detach. only need to tell main, that when it's done, reset and give it to next person.
    // pthread_detach(pthread_self());
    struct clientArgs *clientArguments = data;
    int sockfd = clientArguments->clientFD;
    int clientNumber = clientArguments->clientNumber;
    int clientPosition = clientArguments->clientPosition;
    struct in_addr ip= clientArguments->clientIP;
    int port = ntohs(clientArguments->port);
    double startTimer = clientArguments->startTimer;
    int n, bytes_to_read;
    char * bp, buf[BUFLEN];
    int attempts = 0;
    double cpu_time_used;
    clock_t end;
    bp = buf;
    bytes_to_read = BUFLEN;

    //Do action once only.
    while ((n = read(sockfd, bp, bytes_to_read)) > 0)
    {
        bp += n;
        bytes_to_read -= n;
    }
    sched_yield();

    if (n == 0)
    {
        if (buf[0] != '\0')
        {
            write(sockfd, buf, BUFLEN); // echo to client
            pthread_exit(NULL); //Done for this iteration
        }
        else
        {
            pthread_mutex_lock(&lock);
            end = clock();
            cpu_time_used = ((double) (end - startTimer)) / CLOCKS_PER_SEC;
            FILE *fp = fopen("Select.csv", "a");
            fprintf(fp, "%d, %s:%hu, %lf\n", clientNumber, inet_ntoa(ip), port, cpu_time_used);
            fclose(fp);
            printf("%d Attempting to finish\n", port);
            accessGlobal(1, sockfd, clientPosition);
        }
    }
}

// Prints the error stored in errno and aborts the program.
static void SystemFatal(const char * message)
{
    perror(message);
    exit(EXIT_FAILURE);
}
