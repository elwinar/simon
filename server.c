#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include <sys/wait.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>

#define PORT "3490"      // the port users will be connecting to
#define BACKLOG 10       // how many pending connections queue will hold
#define MAXDATASIZE 1024 // max number of bytes we can get at once 

int main(void)
{
	int sock; // The socket file descriptor
	
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
		sock = socket(p->ai_family, p->ai_socktype, p->ai_protocol); // Try initializing the socket
		if(sock == -1)
		{
			continue;
		}

		int yes = 1;
		if(setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) // Bust the "Address already in use" error message
		{
			perror("setsockopt");
			exit(1);
		}

		if(bind(sock, p->ai_addr, p->ai_addrlen) == -1) // Try binding the socket to the port
		{
			close(sock);
			continue;
		}

		break;
	}
	
	if (p == NULL)  { // If the pointer is NULL, we iterated over all the results without succeeding to bind the socket
		fprintf(stderr, "failed to bind\n");
		exit(EXIT_FAILURE);
	}
	
	freeaddrinfo(servinfo); // Clear the results of the getaddrinfo
	
	if(listen(sock, BACKLOG) == -1) // Start listening on the socket, allowing BACKLOG connections to wait in the queue
	{
		perror("listen");
		exit(EXIT_FAILURE);
	}

	while(1) // Loop to accept incomming connections
	{
		struct sockaddr_storage addr; // The client address
		socklen_t sin_size = sizeof addr;
		
		int conn = accept(sock, (struct sockaddr *) &addr, &sin_size); // Accept a new connection
		if(conn == -1) // In case of accept error
		{
			perror("accept");
			continue;
		}

		if(!fork()) // Fork to handle the connection in a separate thread
		{
			close(sock); // The child doesn't need the listening socket
	
			while(1)
			{
				char buffer[MAXDATASIZE];
				int n = recv(conn, buffer, MAXDATASIZE-1, 0);
				if (n == -1) {
					perror("recv");
					exit(EXIT_FAILURE);
				}
				
				if(send(conn, buffer, n, 0) == -1)
				{
					perror("send");
					exit(EXIT_FAILURE);
				}
			}
			
			close(conn); // Close the connection
			exit(EXIT_SUCCESS); // Exit
		}
		
		close(conn);  // The parent process doesn't need the connection socket
	}
	
	return EXIT_SUCCESS;
}
