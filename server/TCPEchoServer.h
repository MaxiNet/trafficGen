#include <stdio.h>      /* for printf() and fprintf() */
#include <sys/socket.h> /* for socket(), bind(), and connect() */
#include <arpa/inet.h>  /* for sockaddr_in and inet_ntoa() */
#include <stdlib.h>     /* for atoi() and exit() */
#include <string.h>     /* for memset() */
#include <string>
#include <unistd.h>     /* for close() */
#include <mutex>

void DieWithError(char *errorMessage);  /* Error handling function */
void HandleTCPClient(int clntSocket, int*counter, std::mutex *mut, int rate);   /* TCP client handling function */
int CreateTCPServerSocket(unsigned short port, std::string ip); /* Create TCP server socket */
int AcceptTCPConnection(int servSock);  /* Accept TCP connection request */
