#include <stdio.h>      /* for printf() and fprintf() */
#include <sys/socket.h> /* for recv() and send() */
#include <unistd.h>     /* for close() */
#include <mutex>
#include <chrono>
#include <thread>
#include <iostream>

#define RCVBUFSIZE 1460 /* Size of receive buffer */


void DieWithError(char *errorMessage);  /* Error handling function */

std::chrono::high_resolution_clock::time_point start = std::chrono::high_resolution_clock::now();

std::mutex *mutOut = new std::mutex();

void HandleTCPClient(int clntSocket, int*counter, std::mutex *mut, int datarateInBytePerSec)
{
    char echoBuffer[RCVBUFSIZE];        /* Buffer for echo string */
    int recvMsgSize;                    /* Size of received message */

    typedef std::chrono::high_resolution_clock Clock;
    
    Clock::time_point t0 = Clock::now();

    
    /* Receive message from client */
    if ((recvMsgSize = recv(clntSocket, echoBuffer, RCVBUFSIZE, 0)) < 0)
        DieWithError("recv() failed");

    size_t allRec = recvMsgSize;

    /* Send received string and receive again until end of transmission */
    while (recvMsgSize > 0)      /* zero indicates end of transmission */
    {
        Clock::time_point r0 = Clock::now();
        /* See if there is more data to receive */
        if ((recvMsgSize = recv(clntSocket, echoBuffer, RCVBUFSIZE, 0)) < 0)
            DieWithError("recv() failed");

        Clock::time_point r1 = Clock::now();
        std::chrono::microseconds r_diff = std::chrono::duration_cast<std::chrono::microseconds>(r1 - r0);
        
        //sleep
        
        allRec +=recvMsgSize;
        
        float tt = (datarateInBytePerSec / *counter) / (recvMsgSize+0.001);
        
        int timeToSleep = (1000000.0 / tt) - r_diff.count();
        
        if(recvMsgSize > 0 && timeToSleep > 0) {
            //std::cout << "sleeping for " << timeToSleep << " microsecs " << r_diff.count() << " \n";
            std::this_thread::sleep_for(std::chrono::microseconds( timeToSleep ));
            
        }
		
		if(recvMsgSize == 0) {
			//this must not happen...
			mutOut->lock();
			std::cout << "error: received 0 bytes. Should get a '1'. ";
			mutOut->unlock();
			break;
		}
		
		//we received a 1 which means the transmission is over! Close the socket.
		if(echoBuffer[recvMsgSize-1] == '1') {
			break;
		}
		
		
        
    }

    mut->lock();
    (*counter)--;
    //std::cout << "dec to " << *counter << "\n";
    mut->unlock();
    

    close(clntSocket);    /* Close client socket */
    
    Clock::time_point t1 = Clock::now();
    
    std::chrono::microseconds diff = std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0);
    
    float ms =diff.count() / 1000.0;
    
    int rate = (allRec / (ms / 1000.0) / 1000.0) * 8.0;
    
    std::chrono::milliseconds abs = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - start);
    
    mutOut->lock();
    std::cout << "@ " << abs.count() << "ms: " << allRec << " bytes in " << ms << " ms " << rate << " kbit/s"  << std::endl;
    mutOut->unlock();
    
}
