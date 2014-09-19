

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <chrono>
#include <string>
#include <thread>

/* port we're listening on */
#define PORT 13373

int main(int argc, char *argv[])
{
	/* master file descriptor list */
	fd_set master;
	/* temp file descriptor list for select() */
	fd_set read_fds;
	/* server address */
	struct sockaddr_in serveraddr;
	/* client address */
	struct sockaddr_in clientaddr;
	/* maximum file descriptor number */
	int fdmax;
	/* listening socket descriptor */
	int listener;
	/* newly accept()ed socket descriptor */
	int newfd;
	/* buffer for client data */
	char buf[1460];
	int nbytes;
	/* for setsockopt() SO_REUSEADDR, below */
	uint addrlen;
	/* clear the master and temp sets */
	FD_ZERO(&master);
	FD_ZERO(&read_fds);
	
	typedef std::chrono::high_resolution_clock Clock;
	
	
    if (argc != 3)     /* Test for correct number of arguments */
    {
        fprintf(stderr, "Usage:  %s <Server IP> <Data Rate in BitsPerSec>\n", argv[0]);
        exit(1);
    }
    std::string echoServIp(argv[1]);  /* First arg:  local port */
	int datarateInBytePerSec = atoi(argv[2]) / 8;
	
	fprintf(stdout, "starting traffGen server at IP %s with max recv data rate %d bytes per sec\n", echoServIp.c_str(), datarateInBytePerSec);
	
	
	/* get the listener */
	if((listener = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1)
	{
		perror("Server-socket() error lol!");
		/*just exit lol!*/
		exit(1);
	}
	
	/* bind */
	memset(&serveraddr, 0, sizeof(serveraddr));   /* Zero out structure */
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_addr.s_addr = inet_addr(echoServIp.c_str());         /* Only at ip */
	serveraddr.sin_port = htons(PORT);
	
	if(bind(listener, (struct sockaddr *)&serveraddr, sizeof(serveraddr)) == -1)
	{
		perror("Server-bind() error lol!");
		exit(1);
	}
	//printf("Server-bind() is OK...\n");
	
	/* listen */
	if(listen(listener, 50) == -1)
	{
		perror("Server-listen() error lol!");
		exit(1);
	}
	//printf("Server-listen() is OK...\n");
	
	/* add the listener to the master set */
	FD_SET(listener, &master);
	/* keep track of the biggest file descriptor */
	fdmax = listener; /* so far, it's this one*/
	
	Clock::time_point t0 = Clock::now();
	int bytesInLastIteration = 0;
	
	/* loop */
	for(;;)
	{
		
		//sleep
		Clock::time_point t1 = Clock::now();
		std::chrono::microseconds r_diff = std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0);

		
		long long timeToReceive = ((long long)bytesInLastIteration * 1000000L) / (long long) datarateInBytePerSec;	//in microseconds
		long long timeToSleep = timeToReceive - r_diff.count();
		
		//printf("received %d bytes in %d ms. Have to sleep for %d ms\n", bytesInLastIteration, (int)(r_diff.count()/1000), (int)(timeToSleep/1000));
		if(timeToSleep > 0 && bytesInLastIteration > 0) {
			std::this_thread::sleep_for(std::chrono::microseconds( timeToSleep ));
		}
		
		t0 = Clock::now();
		
		
		/* copy it */
		read_fds = master;
		
		if(select(fdmax+1, &read_fds, NULL, NULL, NULL) == -1)
		{
			perror("Server-select() error lol!");
			exit(1);
		}
		//printf("Server-select() is OK...\n");
		
		bytesInLastIteration = 0;
		
		/*run through the existing connections looking for data to be read*/
		for(int i = 0; i <= fdmax; i++)
		{
			if(FD_ISSET(i, &read_fds))
			{ /* we got one... */
				if(i == listener)
				{
					/* handle new connections */
					addrlen = sizeof(clientaddr);
					if((newfd = accept(listener, (struct sockaddr *)&clientaddr, &addrlen)) == -1)
					{
						perror("Server-accept() error lol!");
					}
					else
					{
						//printf("Server-accept() is OK...\n");
						
						FD_SET(newfd, &master); /* add to master set */
						if(newfd > fdmax)
						{ /* keep track of the maximum */
							fdmax = newfd;
						}
						//printf("%s: New connection from %s on socket %d\n", argv[0], inet_ntoa(clientaddr.sin_addr), newfd);
					}
				}
				else
				{
					/* handle data from a client */
					if((nbytes = recv(i, buf, sizeof(buf), 0)) <= 0)
					{
						/* got error or connection closed by client */
						if(nbytes == 0) {
						/* connection closed */
						//printf("%s: socket %d hung up\n", argv[0], i);
						
						} else {
							perror("recv() error lol!");
						}
						/* close it... */
						close(i);
						/* remove from master set */
						FD_CLR(i, &master);
					}
					else
					{
						/* we got some data from a client*/
						bytesInLastIteration += nbytes;
						
						
						//check if this was the last message (last byte '1')
						if(buf[nbytes-1] == '1') {
							//printf("%s: socket %d hung up\n", argv[0], i);
							/* close it... */
							close(i);
							/* remove from master set */
							FD_CLR(i, &master);
						} else {
							//printf("%s\n", buf);
						}
						

					}
				}
			}
		}
	}
	return 0;
}
