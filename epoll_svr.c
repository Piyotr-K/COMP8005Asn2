/*---------------------------------------------------------------------------------------
--	SOURCE FILE:		epoll_svr.c -   A simple echo server using the epoll API
--
--	PROGRAM:		epolls
--				gcc -Wall -ggdb -o epolls epoll_svr.c
--
--	FUNCTIONS:		Berkeley Socket API
--
--	DATE:			February 2, 2008
--
--	REVISIONS:		(Date and Description)
--
--	DESIGNERS:		Design based on various code snippets found on C10K links
--				Modified and improved: Aman Abdulla - February 2008
--
--	PROGRAMMERS:		Aman Abdulla
--
--	NOTES:
--	The program will accept TCP connections from client machines.
-- 	The program will read data from the client socket and simply echo it back.
--	Design is a simple, single-threaded server using non-blocking, edge-triggered
--	I/O to handle simultaneous inbound connections.
--	Test with accompanying client application: epoll_clnt.c
---------------------------------------------------------------------------------------*/

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <netdb.h>
#include <strings.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>
#include <time.h>

#define TRUE 		1
#define FALSE 		0
#define EPOLL_QUEUE_LEN	30000
#define BUFLEN		80
#define SERVER_PORT	7000

//Globals
int fd_server;

// Function prototypes
static void SystemFatal (const char* message);
static int ClearSocket (int fd);
void close_fd (int);

pthread_mutex_t tlock = PTHREAD_MUTEX_INITIALIZER;

// Logging
int requests_log[EPOLL_QUEUE_LEN] = {0};
unsigned short client_port[EPOLL_QUEUE_LEN];
int clientNumber[EPOLL_QUEUE_LEN] = {0};
struct in_addr client_ip[EPOLL_QUEUE_LEN], emptyInAddr;
double startTimer[EPOLL_QUEUE_LEN];
size_t dataTransfered[EPOLL_QUEUE_LEN] = {0};

int main (int argc, char* argv[])
{
	int i, arg;
	int num_fds, fd_new, epoll_fd;
	int port = SERVER_PORT;
	int requests = 0;
	int numOfClients = 0;

	static struct epoll_event events[EPOLL_QUEUE_LEN], event, emptyEvent;
	struct sockaddr_in addr, remote_addr;
	struct sigaction act;

    // pthread_t threadList[EPOLL_QUEUE_LEN];
	socklen_t addr_size = sizeof(struct sockaddr_in);


	// set up the signal handler to close the server socket when CTRL-c is received
        act.sa_handler = close_fd;
        act.sa_flags = 0;
        if ((sigemptyset (&act.sa_mask) == -1 || sigaction (SIGINT, &act, NULL) == -1))
        {
                perror ("Failed to set SIGINT handler");
                exit (EXIT_FAILURE);
        }

	// Create the listening socket
	fd_server = socket (AF_INET, SOCK_STREAM, 0);
    	if (fd_server == -1)
		SystemFatal("socket");

    	// set SO_REUSEADDR so port can be resused imemediately after exit, i.e., after CTRL-c
    	arg = 1;
    	if (setsockopt (fd_server, SOL_SOCKET, SO_REUSEADDR, &arg, sizeof(arg)) == -1)
		SystemFatal("setsockopt");

    	// Make the server listening socket non-blocking
    	if (fcntl (fd_server, F_SETFL, O_NONBLOCK | fcntl (fd_server, F_GETFL, 0)) == -1)
		SystemFatal("fcntl");

    	// Bind to the specified listening port
    	memset (&addr, 0, sizeof (struct sockaddr_in));
    	addr.sin_family = AF_INET;
    	addr.sin_addr.s_addr = htonl(INADDR_ANY);
    	addr.sin_port = htons(port);
    	if (bind (fd_server, (struct sockaddr*) &addr, sizeof(addr)) == -1)
		SystemFatal("bind");

    	// Listen for fd_news; SOMAXCONN is 128 by default
    	if (listen (fd_server, SOMAXCONN) == -1)
		SystemFatal("listen");

    	// Create the epoll file descriptor
    	epoll_fd = epoll_create(EPOLL_QUEUE_LEN);
    	if (epoll_fd == -1)
		SystemFatal("epoll_create");

    	// Add the server socket to the epoll event loop
    	event.events = EPOLLIN | EPOLLERR | EPOLLHUP | EPOLLET;
    	event.data.fd = fd_server;
    	if (epoll_ctl (epoll_fd, EPOLL_CTL_ADD, fd_server, &event) == -1)
		SystemFatal("epoll_ctl");
	int z = 0;
	int firstRun = 0;
	int modified = 0;
	// Execute the epoll event loop
	while (TRUE)
	{
		// printf("Starting iteration: %d\n", z);
		// printf("Waiting epoll now\n");
		//struct epoll_event events[MAX_EVENTS];

		//first run
		if (firstRun == 0)
		{
			num_fds = epoll_wait (epoll_fd, events, EPOLL_QUEUE_LEN, -1);
		}
		else
		{
			//runs after the first connection
			num_fds = epoll_wait (epoll_fd, events, EPOLL_QUEUE_LEN, 1000);
		}
		// printf("%d\n", num_fds);

		//If epoll timed out, change edge trigger to level trigger to flush out missed items. Reset at the end.
		if(num_fds == 0)
		{
			printf("Timed out!!!\n");
			event.events = EPOLLIN | EPOLLERR | EPOLLHUP;
			event.data.fd = fd_server;
			epoll_ctl(epoll_fd, EPOLL_CTL_MOD, fd_server, &event);
			modified = 1;
		}

		if (num_fds < 0)
		{
			SystemFatal ("Error in epoll_wait!");
			break;
		}

		for (i = 0; i < num_fds; i++)
		{
	    		// Case: Hang up condition Error condition
	    		if (events[i].events & (EPOLLHUP | EPOLLERR))
				{
					// fputs("epoll: EPOLLHUP | EPOLLERR\n", stderr);
					// send ((events[i].data.fd), "There was a HangUp, goodbye", BUFLEN, 0);
					close(events[i].data.fd);
					//clear the data when finished processesing;
					events[i] = emptyEvent;
					continue;
	    		}

	    		// Case 2: Server is receiving a connection request
	    		if (events[i].data.fd == fd_server)
				{
					//socklen_t addr_size = sizeof(remote_addr);
					fd_new = accept (fd_server, (struct sockaddr*) &remote_addr, &addr_size);

					if (fd_new == -1)
					{
			    			if (errno != EAGAIN && errno != EWOULDBLOCK)
						{
							perror("accept");
			    			}
			    			continue;
					}

					numOfClients += 1;

					// Make the fd_new non-blocking
					if (fcntl (fd_new, F_SETFL, O_NONBLOCK | fcntl(fd_new, F_GETFL, 0)) == -1)
						SystemFatal("fcntl");

					// Add the new socket descriptor to the epoll loop
    				event.events = EPOLLIN | EPOLLOUT | EPOLLERR | EPOLLHUP | EPOLLET | EPOLLEXCLUSIVE;
					event.data.fd = fd_new;
					if (epoll_ctl (epoll_fd, EPOLL_CTL_ADD, fd_new, &event) == -1)
						SystemFatal ("epoll_ctl");

					// Logging requests
					requests_log[fd_new]++;

					client_ip[fd_new] = remote_addr.sin_addr;
					client_port[fd_new] = remote_addr.sin_port;
					startTimer[fd_new] = clock();
					clientNumber[fd_new] = numOfClients;

					// printf(" Remote Address:  %s\n", inet_ntoa(remote_addr.sin_addr));
					//clear the data when finished processesing;
					events[i] = emptyEvent;
					continue;
	    		}
				// pthread_create(&threadList[i], NULL, CheckSocket, events[i].data.fd);
				// pthread_join(threadList[i]);
				// printf("Split!");
				if(events[i].events & (EPOLLIN && EPOLLOUT))
				{
					if (!ClearSocket(events[i].data.fd))
						{
							// epoll will remove the fd from its set
							// automatically when the fd is closed

							//Clean fd removal
							client_ip[events[i].data.fd] = emptyInAddr;
							client_port[events[i].data.fd] = 0;
							clientNumber[events[i].data.fd] = 0;
							startTimer[events[i].data.fd] = 0;
							dataTransfered[events[i].data.fd] = 0;
							epoll_ctl(epoll_fd, EPOLL_CTL_DEL, events[i].data.fd, NULL);
							close (events[i].data.fd);
							//clear the data when finished processesing;
							events[i] = emptyEvent;
						}
				}
				// printf("Iteration: %d completed\n", z);
				z += 1;
			}
			firstRun = -1;

			// Reset epoll's Trigger
			if(modified = 1)
			{
				event.events = EPOLLIN | EPOLLERR | EPOLLHUP | EPOLLET;
				event.data.fd = fd_server;
				epoll_ctl(epoll_fd, EPOLL_CTL_MOD, fd_server, &event);
				modified = 0;
				firstRun = 0;
			}
    	}
	close(fd_server);
	exit (EXIT_SUCCESS);
}

static int ClearSocket (int fd)
{
	int	n, bytes_to_read;
	char *bp, buf[BUFLEN];
	clock_t end;
	double cpu_time_used;

	while (TRUE)
	{

		bp = buf;
		bytes_to_read = BUFLEN;
		n = recv (fd, bp, bytes_to_read, 0);
		if(errno == EAGAIN)
		{
			printf("EAGAIN\n");
			//MORE TO READ, SEND BACK TO OUTER LOOP
			errno = 0;
			sleep(1);
			continue;
		}
		else
		{
			// Request logging
			requests_log[fd]++;

			if(buf[0] != '\0')
			{
				dataTransfered[fd] += sizeof(buf);
				send (fd, buf, BUFLEN, 0);
				return TRUE;
			}
			else
				break;
		}
	}

	// Request logging
	end = clock();
	cpu_time_used = ((double) (end - startTimer[fd])) / CLOCKS_PER_SEC;
	fprintf(stderr, "%d, %s:%d, %lf, %d, %d\n", clientNumber[fd], inet_ntoa(client_ip[fd]), ntohs(client_port[fd]), cpu_time_used, requests_log[fd], dataTransfered[fd]);
	requests_log[fd] = 0;
	return(0);

}

// Prints the error stored in errno and aborts the program.
static void SystemFatal(const char* message)
{
    perror (message);
    exit (EXIT_FAILURE);
}

// close fd
void close_fd (int signo)
{
    close(fd_server);
	exit (EXIT_SUCCESS);
}
