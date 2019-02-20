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
void* ClntConnection (int port, char *host);

#define SERVER_TCP_PORT		7000	// Default port
#define BUFLEN			80  	// Buffer length

int main (int argc, char **argv)
{
		int n, bytes_to_read;
		int sd, port;
		struct hostent	*hp;
		struct sockaddr_in server;
		char  *host, *bp, rbuf[BUFLEN], sbuf[BUFLEN], **pptr, *sptr;
		char str[16];

		switch(argc)
		{
			case 2:
				host =	argv[1];	// Host name
				port =	SERVER_TCP_PORT;
			break;
			case 3:
				host =	argv[1];
				port =	atoi(argv[2]);	// User specified port
			break;
			default:
				fprintf(stderr, "Usage: %s host [port]\n", argv[0]);
				exit(1);
		}

		ClntConnection(port, host);

		return (0);
}

void* ClntConnection(int port, char *host)
{
		int n, bytes_to_read, sd;
		struct hostent	*hp;
		struct sockaddr_in server;
		char *bp, rbuf[BUFLEN], sbuf[BUFLEN], **pptr, *sptr;
		char str[16];

		// Open the text file and read it for data for sending
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
		if (connect (sd, (struct sockaddr *)&server, sizeof(server)) == -1)
		{
			fprintf(stderr, "Can't connect to server\n");
			perror("connect");
			exit(1);
		}
		printf("Connected:    Server Name: %s\n", hp->h_name);
		pptr = hp->h_addr_list;
		printf("\t\tIP Address: %s\n", inet_ntop(hp->h_addrtype, *pptr, str, sizeof(str)));

		//Enters Loop to periodically Send text
		//gets(sbuf); // get user's text
		while(1)
		{
			printf("Now sleeping\n");
			sleep(2);

			//Get from file
			fgets (sbuf, BUFLEN, fp);

			// Transmit data through the socket
			printf("Transmit:\n");
			printf("%s", sbuf);
			write(sd, sbuf, BUFLEN);

			//Set up receive
			printf("Receive:\n");
			bp = rbuf;
			bytes_to_read = BUFLEN;

			// client makes repeated calls to recv until no more data is expected to arrive.
			n = 0;
			while ((n = recv (sd, bp, bytes_to_read, 0)) < BUFLEN)
			{
				bp += n;
				bytes_to_read -= n;
			}
			printf ("%s\n", rbuf);
			
			fflush(stdout);
		}
		close (sd);
		return NULL;
}
