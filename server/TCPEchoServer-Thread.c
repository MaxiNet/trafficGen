#include "TCPEchoServer.h"  /* TCP echo server includes */
#include <pthread.h>        /* for POSIX threads */
#include <mutex>
#include <string>
#include <iostream>

void *ThreadMain(void *arg);            /* Main program of a thread */

/* Structure of arguments to pass to client thread */
struct ThreadArgs
{
    int clntSock;                      /* Socket descriptor for client */
    int* counter;
	int datarateInBytePerSec;
    std::mutex *mut;
};

int main(int argc, char *argv[])
{
    int servSock;                    /* Socket descriptor for server */
    int clntSock;                    /* Socket descriptor for client */
    unsigned short echoServPort;     /* Server port */
    pthread_t threadID;              /* Thread ID from pthread_create() */
    struct ThreadArgs *threadArgs;   /* Pointer to argument structure for thread */
    
    std::mutex *mut = new std::mutex();
    int numConnections = 0;
	int datarateInBytePerSec = 0;

    echoServPort = 13373;
    
    if (argc != 3)     /* Test for correct number of arguments */
    {
        fprintf(stderr, "Usage:  %s <Server IP> <Data Rate in BitsPerSec>\n", argv[0]);
        exit(1);
    }
    
    std::string echoServIp(argv[1]);  /* First arg:  local port */
	datarateInBytePerSec = atoi(argv[2]) / 8;
	
	fprintf(stdout, "starting traffGen server at IP %s with max recv data rate %d bytes per sec\n", echoServIp.c_str(), datarateInBytePerSec);

    servSock = CreateTCPServerSocket(echoServPort, echoServIp);

    for (;;) /* run forever */
    {
        clntSock = AcceptTCPConnection(servSock);
        
        mut->lock();
        numConnections++;
        //std::cout << "inc to " << numConnections << "\n";
        mut->unlock();

        /* Create separate memory for client argument */
        if ((threadArgs = (struct ThreadArgs *) malloc(sizeof(struct ThreadArgs))) 
               == NULL)
            DieWithError("malloc() failed");
        threadArgs -> clntSock = clntSock;
        threadArgs->mut = mut;
        threadArgs->counter = &numConnections;
		threadArgs->datarateInBytePerSec = datarateInBytePerSec;

        /* Create client thread */
        if (pthread_create(&threadID, NULL, ThreadMain, (void *) threadArgs) != 0)
            DieWithError("pthread_create() failed");
        //printf("with thread %ld\n", (long int) threadID);
    }
    /* NOT REACHED */
}

void *ThreadMain(void *threadArgs)
{
    int clntSock;                   /* Socket descriptor for client connection */
    
    std::mutex* mut;
    int*counter;
	
	int datarateInBytePerSec = 0;

    /* Guarantees that thread resources are deallocated upon return */
    pthread_detach(pthread_self()); 

    /* Extract socket file descriptor from argument */
    clntSock = ((struct ThreadArgs *) threadArgs) -> clntSock;
    counter = ((struct ThreadArgs *) threadArgs) -> counter;
    mut = ((struct ThreadArgs *) threadArgs) -> mut;
	datarateInBytePerSec = ((struct ThreadArgs *) threadArgs) -> datarateInBytePerSec;
    
    free(threadArgs);              /* Deallocate memory for argument */

    HandleTCPClient(clntSock, counter, mut, datarateInBytePerSec);

    return (NULL);
}
