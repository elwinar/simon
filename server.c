#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#define PORT "42000"
#define BACKLOG 10
#define BUFFER_SIZE 1024

int main(void)
{
	// STEP 1: Initialize the master socket
	// The master socket is the one that will listen on the public port
	// and accept the new clients.
	
	int master;
	
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
		master = socket(p->ai_family, p->ai_socktype, p->ai_protocol); // Try initializing the socket
		if(master == -1)
		{
			perror("socket");
			continue;
		}

		int yes = 1;
		if(setsockopt(master, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) // Bust the "Address already in use" error message
		{
			perror("setsockopt");
			exit(1);
		}

		if(bind(master, p->ai_addr, p->ai_addrlen) == -1) // Try binding the socket to the port
		{
			perror("bind");
			close(master);
			continue;
		}

		break;
	}
	
	if (p == NULL)  { // If the pointer is NULL, we iterated over all the results without succeeding to bind the socket
		fprintf(stderr, "failed to bind\n");
		exit(EXIT_FAILURE);
	}
	
	freeaddrinfo(servinfo); // Clear the results of the getaddrinfo
	
	if(listen(master, BACKLOG) == -1) // Start listening on the socket, allowing BACKLOG connections to wait in the queue
	{
		perror("listen");
		exit(EXIT_FAILURE);
	}
	
	// STEP 2: Handle the clients in a non-blocking way
	// This use a pool of connection, and the select() function that will
	// wait until a socket is ready.
	
	fd_set pool_fds; // Will contain all sockets handled by the server. It's actually a bit array… don't try to look at it with the naked eye
	fd_set read_fds; // Will contain the temporary list of sockets.
	
	FD_ZERO(&pool_fds); // Ensure the socket pool is empty
	FD_ZERO(&read_fds); // Ensure the socket pool is empty
	
	FD_SET(master, &pool_fds); // Add the master socket to the pool
	
	int max_descriptor = master; // Keep track of the bigest descriptor

	while(1) // Loop until the end of times
	{
		read_fds = pool_fds; // Copy the pool to prevent dumb errors
		
		if(select(max_descriptor + 1, &read_fds, NULL, NULL, NULL) ==  -1) // Wait for something to happen on any socket
		{
			perror("select");
			exit(EXIT_FAILURE);
		}
		
		int i;
		for(i = 0; i <= max_descriptor; i++) // Loop on every descriptor (brutal and ugly… but I don't give a fuck for educationnal purposes)
		{
			if(!FD_ISSET(i, &read_fds)) // If the descriptor isn't in the pool, skip to the next
			{
				continue;
			}
			
			if(i == master) // If the active descriptor is the master, this is a new client trying to connect
			{
				int conn = accept(master, NULL, NULL); // Accept the new connection
				if(conn == -1) // In case of error, skip
				{
					perror("accept");
					continue;
				}
				
				FD_SET(conn, &pool_fds); // Add the new connection to the pool
				if(conn > max_descriptor) // Update the bigest descriptor
				{
					max_descriptor = conn;
				}
				
				continue;
			}
			
			char buffer[BUFFER_SIZE]; // Initialize the buffer
			int n = recv(i, buffer, sizeof buffer, 0); // Read from the socket
			
			if(n == 0) // If the receive returns a zero value, the connection was closed by the client
			{
				fprintf(stdout, "socket %d disconnected\n", i);
				close(i); // Close the connection
				FD_CLR(i, &pool_fds); // Remove it from the pool
				continue;
			}
			
			if(n < 0) // If the receive returns a negative value, it's an error
			{
				perror("recv");
				close(i); // Close the connection
				FD_CLR(i, &pool_fds); // Remove it from the pool
				continue;
			}
			
			int j;
			for(j = 0; j <= max_descriptor; j++) // Iterate over each descriptor
			{
				if(!FD_ISSET(j, &pool_fds)) // Skip those that are not in the pool
				{
					continue;
				}
				
				if(j == master || j == i) // Don't send to the master and the sender
				{
					continue;
				}
				
				if(send(j, buffer, n, 0) == -1) // Notify about errors
				{
					perror("send");
				}
			}
		}
	}
	
	return EXIT_SUCCESS;
}
