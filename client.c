#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#define PORT "42000"
#define BUFFER_SIZE 1024
#define STDIN 0

int main(void)
{
	// STEP 1: Initialize the server socket
	// The server socket will be used to communicate with the server (both
	// for sending and receiving).
	
	int server;
	
	struct addrinfo hints; // Hints for getaddrinfo to fill the results
	memset(&hints, 0, sizeof hints); // Ensure the struct is empty
	hints.ai_family = AF_UNSPEC; // Don't care about IPv4 or IPv6
	hints.ai_socktype = SOCK_STREAM; // TCP stream socket
	hints.ai_flags = AI_PASSIVE; // Please fill the IP for me, I'm lazy
	
	struct addrinfo* servinfo; // Will point to the results (linked list)
	if(getaddrinfo(NULL, PORT, &hints, &servinfo) != 0) // Fill the addrinfo struct automagically
	{
		perror("getaddrinfo");
		exit(EXIT_FAILURE);
	}
	
	struct addrinfo* p;
	for(p = servinfo; p != NULL; p = p->ai_next) // Iterate over the list
	{
		server = socket(p->ai_family, p->ai_socktype, p->ai_protocol); // Try initializing the socket
		if(server == -1)
		{
			perror("socket");
			continue;
		}

		if(connect(server, p->ai_addr, p->ai_addrlen) == -1) // Connect to the remote socket
		{
			close(server);
			perror("connect");
			continue;
		}

		break;
	}
	
	if (p == NULL)  { // If the pointer is NULL, we iterated over all the results without succeeding to connect
		fprintf(stderr, "failed to bind\n");
		exit(EXIT_FAILURE);
	}
	
	freeaddrinfo(servinfo); // Clear the results of the getaddrinfo
	
	// STEP 2: Communicate with the server
	// Practically, we treat stdin as if it was a socket and read from
	// all available sources, maintaining a write buffer for the user inputs,
	// so we can display messages as they arrive while not messing the input.
	
	fd_set pool_fds; // Will contain all sockets handled by the server. It's actually a bit array… don't try to look at it with the naked eye
	fd_set read_fds; // Will contain the temporary list of sockets.
	
	FD_ZERO(&pool_fds); // Ensure the socket pool is empty
	FD_ZERO(&read_fds); // Ensure the socket pool is empty
	
	FD_SET(server, &pool_fds); // Add the server socket to the pool
	FD_SET(STDIN, &pool_fds); // Add stdin to the pool
	
	int max_descriptor = server; // Keep track of the bigest descriptor

	while(1) // Loop until the end of times
	{
		read_fds = pool_fds; // Copy the pool to prevent dumb errors
		
		if(select(max_descriptor + 1, &read_fds, NULL, NULL, NULL) ==  -1) // Wait for something to happen on any socket
		{
			perror("select");
			exit(EXIT_FAILURE);
		}
		
		if(FD_ISSET(STDIN, &read_fds))
		{
			char buffer[BUFFER_SIZE];
			int n = read(STDIN, buffer, BUFFER_SIZE);
			buffer[n] = '\0';
			
			if(send(server, buffer, n+1, 0) == -1)
			{
				perror("send");
				return EXIT_FAILURE;
			}
		}
		else
		{
			char buffer[BUFFER_SIZE];
			int n = recv(server, buffer, sizeof buffer, 0); // Read from the socket
			
			if(n == 0) // If the receive returns a zero value, the connection was closed by the client
			{
				fprintf(stdout, "socket disconnected\n");
				close(server); // Close the connection
				return EXIT_FAILURE;
			}
			
			if(n < 0) // If the receive returns a negative value, it's an error
			{
				perror("recv");
				close(server); // Close the connection
				return EXIT_FAILURE;
			}
			
			fprintf(stdout, buffer);
		}
	}
	
	return EXIT_SUCCESS;
}
