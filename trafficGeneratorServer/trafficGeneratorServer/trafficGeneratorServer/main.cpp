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
#include <iostream>
#include <netinet/tcp.h>


#ifndef MPTCP_ENABLED
#define MPTCP_ENABLED          26
#endif

/* port we're listening on */
#define PORT 13373
#define BUFSIZE 2*1024*1024

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
	char buf[BUFSIZE];
	int nbytes;
	/* for setsockopt() SO_REUSEADDR, below */
	uint addrlen;
	/* clear the master and temp sets */
	FD_ZERO(&master);
	FD_ZERO(&read_fds);

	
	
    if (argc < 2 || argc > 3)     /* Test for correct number of arguments */
    {
        fprintf(stderr, "Usage:  %s <Server IP> <enablemtcp 0/1>\n", argv[0]);
        exit(1);
    }
    std::string echoServIp(argv[1]);  /* First arg:  local port */

    bool enablemtcp;
    if (argc >=3) {
        enablemtcp= atoi(argv[2]);
    }else {
        enablemtcp=false;
    }

	
	fprintf(stdout, "starting traffGen server at IP %s and mtcp=%d\n", echoServIp.c_str(), enablemtcp);

	
	/* get the listener */
	if((listener = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1)
	{
		perror("Server-socket() error!");
		exit(1);
	}

#ifdef __linux__
    int enablemtcpint = enablemtcp;
    if(setsockopt(listener, SOL_TCP, MPTCP_ENABLED, &enablemtcpint, sizeof(enablemtcpint)) && enablemtcp) {
#else
        if(enablemtcp) {
#endif
            perror("Enable MTCP on listener socket");
            // lol, get out get of here!
            exit(32);
        }


	
	/* bind */
	memset(&serveraddr, 0, sizeof(serveraddr));   /* Zero out structure */
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_addr.s_addr = inet_addr(echoServIp.c_str());         /* Only at ip */
	serveraddr.sin_port = htons(PORT);
	
	if(bind(listener, (struct sockaddr *)&serveraddr, sizeof(serveraddr)) == -1)
	{
		perror("Server-bind() error!");
		exit(1);
	}
	
	/* listen */
	if(listen(listener, 50) == -1)
	{
		perror("Server-listen() error!");
		exit(1);
	}
	
	/* add the listener to the master set */
	FD_SET(listener, &master);
	/* keep track of the biggest file descriptor */
	fdmax = listener; /* so far, it's this one*/
	

	/* loop */
	for(;;)
	{
		/* copy it */
		read_fds = master;
		
		if(select(fdmax+1, &read_fds, NULL, NULL, NULL) == -1)
		{
			perror("Server-select() error!");
			exit(1);
		}
		
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
						perror("Server-accept() error!");
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
							perror("recv() error!");
						}
						/* close it... */
						close(i);
						/* remove from master set */
						FD_CLR(i, &master);
					}
					else
					{
						/* we got some data from a client*/

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
