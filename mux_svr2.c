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
#include <pthread.h>
#include <stdio.h>
#include <fcntl.h>
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

#define SERVER_TCP_PORT 7002 // Default port
# define BUFLEN 80 //Buffer length
# define TRUE 1
# define LISTENQ 5
# define MAXLINE 4096

// Function Prototypes
static void SystemFatal(const char * );
void* threadFunction(void *);

struct clientArgs
{
    int clientFD;
    struct in_addr clientIP;
    int clientPort;
    double startTimer;
    fd_set *allSetPT;
    int clientNumber;
    int loopIteration;
};

pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

int requestsGenerated[FD_SETSIZE] = {0};
size_t dataTransfered[FD_SETSIZE] = {0};
int client[FD_SETSIZE];

int main(int argc, char ** argv) {
    int i, maxi, nready, nreadyCopy, arg, threadInt;
    int listen_sd, new_sd, sockfd, client_len, port, maxfd;
    struct clientArgs clientArguments, emptyClientArgs;
    struct clientArgs *argsPT;
    struct in_addr ip_num[FD_SETSIZE]; // Saves ip address of the connected clients
    unsigned short portNum[FD_SETSIZE]; // Saves port numbers of the connected clients
    double startTimer[FD_SETSIZE]; // Saves start Timer of each of the clients
    int clientNumber[FD_SETSIZE];
    pthread_t threadList[FD_SETSIZE];
    int numOfClients = 0;
    struct sockaddr_in server, client_addr;
    // char * bp, buf[BUFLEN];
    fd_set rset, allset;
    // char tmpc;
    // int allThreadList[FD_SETSIZE] = {0};

    //clears content of csv file
    // FILE *fp = fopen("Server.csv", "w");
    // fclose(fp);

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

            //make socket non-blocking
            if (fcntl (new_sd, F_SETFL, O_NONBLOCK | fcntl(new_sd, F_GETFL, 0)) == -1)
                SystemFatal("fcntl");

            for (i = 0; i < FD_SETSIZE; i++)
                if (client[i] < 0)
				{
                    client[i] = new_sd; // save descriptor
                    portNum[i] = client_addr.sin_port; // saves the client's port number
                    ip_num[i] = client_addr.sin_addr; // save the client's ip address
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

        threadInt = 0;
        int threadLimit = 3;
        for (i = 0; i <= maxi; i++) // check all clients for data
        {
            //If there was nothing found, move on.
            if ((sockfd = client[i]) < 0)
                continue;

            //If there was an item that needs to be checked, do something.
            if (FD_ISSET(sockfd, & rset))
            {
                if (threadLimit < threadInt)
                    break;
                //struct initialization
                clientArguments = emptyClientArgs;
                clientArguments.clientFD = sockfd;
                clientArguments.loopIteration = i;
                clientArguments.allSetPT = &allset;

                clientArguments.clientIP = ip_num[i];
                clientArguments.clientPort = portNum[i];
                clientArguments.startTimer = startTimer[i];
                clientArguments.clientNumber = clientNumber[i];

                argsPT = &clientArguments;

                //create thread
                // printf("Created Thread: %d\n", threadInt);
                pthread_create(&threadList[threadInt], NULL, threadFunction, (void *)argsPT);
                threadInt++;
                if (--nready <= 0)
                    break; // no more readable descriptors
            }
        }

        //for each thread that was spawned earlier, join them all and finish.
        for (i = 0; i < threadInt; i++)
        {
            // printf("Joining Thread: %d\n", i);
            pthread_join(threadList[i], NULL);
        }
        printf("Main, Re-Selecting\n");
    }
    return (0);
}

void* threadFunction(void* data)
{
    //Initialize thread variables from struct
    // pthread_detach(pthread_self());
    struct clientArgs *clientArgs = data;
    int sockfd = clientArgs->clientFD;
    int outerCounter = clientArgs->loopIteration;
    fd_set *allSetPT = clientArgs->allSetPT;

    int clientNumber = clientArgs->clientNumber;
    struct in_addr ip_num = clientArgs->clientIP;
    int clientPort = clientArgs->clientPort;
    double startTimer = clientArgs->startTimer;

    //Initialize thread variables
    char * bp, buf[BUFLEN];
    clock_t end;
    double cpu_time_used;
    int n, s, endRequestsGenerated, endDataTransfered, bytes_to_read;
    // int counter = 0;
    while(1)
    {
        //Set up buffer pointer
        bp = buf;

        //Sets up buffer lengths
        bytes_to_read = BUFLEN;
        pthread_mutex_lock(&lock);
        n = recv(sockfd, bp, bytes_to_read, 0);
        pthread_mutex_unlock(&lock);
        if(errno == EAGAIN)
        {
            // if (counter > 3)
            // {
            //     break;
            // }
            //Would mean this job is finished for now
            errno = 0;
            // counter++;
            // sleep(0.1);
            printf("%d, %d EAGAIN\n", sockfd, pthread_self());
            break;
        }
        else
        {
            //mutex lock to ensure handling on variable for number of requests
            // pthread_mutex_lock(&lock);
            // requestsGenerated[outerCounter] += 1;
            // pthread_mutex_unlock(&lock);

            //Connection should be closed when reaching EOF instead of when text is done being sent

            printf("%d, %d PROCESSING\n", sockfd, pthread_self());
            if (buf[0] != '\0')
            {
                //mutex lock to ensure handling on variable for data transfered
                // pthread_mutex_lock(&lock);
                // dataTransfered[outerCounter] += sizeof(buf);
                // printf("%s:%hu send: %s\n", inet_ntoa(ip_num), ntohs(clientPort), buf);
                // pthread_mutex_unlock(&lock);
                pthread_mutex_lock(&lock);
                printf("%d, %d ECHOING: %s\n", sockfd, pthread_self(), buf);
                write(sockfd, buf, BUFLEN); // echo to client
                pthread_mutex_unlock(&lock);
                break;
            }
            else if (buf[0] == '\0')
            {
                end = clock();
            	cpu_time_used = ((double) (end - startTimer)) / CLOCKS_PER_SEC;
                pthread_mutex_lock(&lock);
                printf("%d, %d FINISHING\n", sockfd, pthread_self());
                FILE *fp = fopen("Server.csv", "a");
                fprintf(fp, "%d, %s:%d, %lf, %d, %d\n", clientNumber, inet_ntoa(ip_num), ntohs(clientPort), cpu_time_used);
                fclose(fp);
                pthread_mutex_unlock(&lock);

                pthread_mutex_lock(&lock);
                close(sockfd);
                pthread_mutex_unlock(&lock);

                pthread_mutex_lock(&lock);
                FD_CLR(sockfd, allSetPT);
                pthread_mutex_unlock(&lock);

                pthread_mutex_lock(&lock);
                client[outerCounter] = -1;
                pthread_mutex_unlock(&lock);

                pthread_exit(pthread_self());
            }
        }
    }
    return NULL;
}

// Prints the error stored in errno and aborts the program.
static void SystemFatal(const char * message)
{
    perror(message);
    exit(EXIT_FAILURE);
}
