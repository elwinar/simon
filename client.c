#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#define PORT "3490"     // the port client will be connecting to 
#define MAXDATASIZE 1024 // max number of bytes we can get at once 

int main(int argc, char *argv[])
{
	int sock;

	if (argc != 2) {
		fprintf(stderr, "usage: client hostname\n");
		exit(EXIT_FAILURE);
	}

	struct addrinfo hints;
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	struct addrinfo* servinfo;
	if(getaddrinfo(argv[1], PORT, &hints, &servinfo) != 0)
	{
		perror("getaddrinfo");
		exit(EXIT_FAILURE);
	}

	struct addrinfo* p;
	for(p = servinfo; p != NULL; p = p->ai_next)
	{
		if((sock = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1)
		{
			continue;
		}

		if(connect(sock, p->ai_addr, p->ai_addrlen) == -1)
		{
			close(sock);
			continue;
		}

		break;
	}

	if (p == NULL) {
		fprintf(stderr, "failed to connect\n");
		exit(EXIT_FAILURE);
	}

	freeaddrinfo(servinfo);
	
	while(1)
	{
		fprintf(stdout, "> ");
	
		char input[MAXDATASIZE];
		if(fgets(input, sizeof(input), stdin) == NULL) // Read from the standard input
		{
			perror("fgets");
			exit(EXIT_FAILURE);
		}
		
		if(send(sock, input, MAXDATASIZE-1, 0) == -1) // Send the message
		{
			perror("send");
			exit(EXIT_FAILURE);
		}
		
		char buffer[MAXDATASIZE];
		int n = recv(sock, buffer, MAXDATASIZE-1, 0); // Read the response
		if (n == -1) {
			perror("recv");
			exit(EXIT_FAILURE);
		}
		buffer[n] = '\0';

		fprintf(stdout, "%s", buffer);
	}

	close(sock);

	return 0;
}
