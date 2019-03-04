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
#include <sched.h>
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
#define MAX_THREADS 1000
#define THREAD_MAX 500

//Globals
int fd_server;

// Function prototypes
static void SystemFatal (const char* message);
static int ClearSocket (int fd);
void *epollLoop(void *data);
void close_fd (int);

struct threadArgs
{
	int svr_fd;
	struct sockaddr_in t_addr;
	struct sockaddr_in t_remote_addr;
};

pthread_mutex_t tlock = PTHREAD_MUTEX_INITIALIZER;

// Logging
int requests_log[EPOLL_QUEUE_LEN] = {0};
unsigned short client_port[EPOLL_QUEUE_LEN];
int clientNumber[EPOLL_QUEUE_LEN] = {0};
struct in_addr client_ip[EPOLL_QUEUE_LEN], emptyInAddr;
pthread_t threadList[MAX_THREADS];
int numOfClients = 0;

int main (int argc, char* argv[])
{
	int i, arg;
	int num_fds, fd_new, epoll_fd;
	int port = SERVER_PORT;
	int requests = 0;

	struct threadArgs tArgs;
	struct threadArgs *argsPt;
	struct sockaddr_in addr, remote_addr;
	struct sigaction act;

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

	tArgs.svr_fd = fd_server;
	tArgs.t_addr = addr;
	tArgs.t_remote_addr = remote_addr;
	argsPt = &tArgs;

	// Create threads
	for (int i = 0; i < MAX_THREADS; i++)
	{
		pthread_create(&threadList[i], NULL, epollLoop, (void *)argsPt);
	}

	for (int i = 0; i < MAX_THREADS; i++)
	{
		pthread_join(threadList[i], NULL);
	}

	close(fd_server);
	exit (EXIT_SUCCESS);
}

void *epollLoop(void *args)
{
	struct epoll_event events[EPOLL_QUEUE_LEN], event, emptyEvent;
	struct threadArgs *t_args = args;

	int z = 0;
	int modified = 0;
	int epoll_fd, num_fds, fd_new;
	int fd_server = t_args->svr_fd;
	double cpu_time_used = 0;

	struct sockaddr_in remote_addr = t_args->t_remote_addr;

	clock_t end;
	socklen_t addr_size = sizeof(struct sockaddr_in);

	// Create the epoll file descriptor
	epoll_fd = epoll_create(THREAD_MAX);
	if (epoll_fd == -1)
		SystemFatal("epoll_create");

	// Add the server socket to the epoll event loop
	event.events = EPOLLIN | EPOLLERR | EPOLLHUP | EPOLLET;
	event.data.fd = fd_server;

	if (epoll_ctl (epoll_fd, EPOLL_CTL_ADD, fd_server, &event) == -1)
		SystemFatal("epoll_ctl");

	while (TRUE)
	{
		num_fds = epoll_wait (epoll_fd, events, THREAD_MAX, -1);
		double startTimer = 0;

		if (num_fds < 0)
		{
			SystemFatal ("Error in epoll_wait!");
			break;
		}

		// Read through triggered fd
		for (int i = 0; i < num_fds; i++)
		{
			// Current file descriptor
			int curr_fd = events[i].data.fd;

    		// Case: Hang up condition Error condition
    		if (events[i].events & (EPOLLHUP | EPOLLERR))
			{
				close(curr_fd);
				//clear the data when finished processesing;
				events[i] = emptyEvent;
				continue;
    		}

    		// Case 2: Server is receiving a connection request
    		if (curr_fd == fd_server)
			{
				fd_new = accept (fd_server, (struct sockaddr*) &remote_addr, &addr_size);

				if (fd_new == -1)
				{
		    			if (errno != EAGAIN && errno != EWOULDBLOCK)
					{
						perror("accept");
		    			}
		    			continue;
				}

				pthread_mutex_lock(&tlock);
				numOfClients += 1;
				pthread_mutex_unlock(&tlock);

				// Make the fd_new non-blocking
				if (fcntl (fd_new, F_SETFL, O_NONBLOCK | fcntl(fd_new, F_GETFL, 0)) == -1)
					SystemFatal("fcntl");

				// Add the new socket descriptor to the epoll loop
				event.events = EPOLLIN | EPOLLOUT | EPOLLERR | EPOLLHUP | EPOLLET | EPOLLEXCLUSIVE;
				event.data.fd = fd_new;
				if (epoll_ctl (epoll_fd, EPOLL_CTL_ADD, fd_new, &event) == -1)
					SystemFatal ("epoll_ctl");

				// Logging requests
				// requests_log[fd_new]++;
                //
				// client_ip[fd_new] = remote_addr.sin_addr;
				// client_port[fd_new] = remote_addr.sin_port;
				startTimer = clock();
				// clientNumber[fd_new] = numOfClients;

				//clear the data when finished processesing;
				events[i] = emptyEvent;
				continue;
    		}

			if(curr_fd & (EPOLLIN && EPOLLOUT))
			{
				if (!ClearSocket(curr_fd))
				{
					// epoll will remove the fd from its set
					// automatically when the fd is closed

					// Clean fd removal
					// client_ip[curr_fd] = emptyInAddr;
					// client_port[curr_fd] = 0;
					// clientNumber[curr_fd] = 0;
					epoll_ctl(epoll_fd, EPOLL_CTL_DEL, curr_fd, NULL);
					close (curr_fd);

					end = clock();
					cpu_time_used = ((double) (end - startTimer)) / CLOCKS_PER_SEC;
					fprintf(stderr, "Client #%d, Time: %lf\n", numOfClients, cpu_time_used);

					//clear the data when finished processesing;
					events[i] = emptyEvent;
				}
			}
			z += 1;
		}
	}
	close(epoll_fd);
}

static int ClearSocket(int fd)
{
	int	n, bytes_to_read;
	char *bp, buf[BUFLEN];
	int dataTransferred = 0;
	int transferCount = 1;

	while (TRUE)
	{
		bp = buf;
		bytes_to_read = BUFLEN;
		n = recv (fd, bp, bytes_to_read, 0);
		dataTransferred += BUFLEN;
		transferCount++;
		if(errno == EAGAIN)
		{
			//MORE TO READ, SEND BACK TO OUTER LOOP
			sched_yield();
			errno = 0;
			continue;
		}
		else
		{
			if(buf[0] != '\0')
			{
				send(fd, buf, BUFLEN, 0);
				return TRUE;
			}
			else
				break;
		}
	}

	fprintf(stderr, "Client#%d, Transfer Amnt: %d, Transfer Count: %d\n ", numOfClients, dataTransferred, transferCount);

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
