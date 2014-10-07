//
//  utils.cpp
//  trafficGenerator
//
//  Created by Philip Wette on 27.06.13.
//  Copyright (c) 2013 Uni Paderborn. All rights reserved.
//

#include "utils.h"
#include "flow.h"
#include <stdio.h>      /* for printf() and fprintf() */
#include <sys/socket.h> /* for socket(), connect(), send(), and recv() */
#include <arpa/inet.h>  /* for sockaddr_in and inet_addr() */
#include <stdlib.h>     /* for atoi() and exit() */
#include <string.h>     /* for memset() */
#include <unistd.h>     /* for close() */
#include <netinet/in.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <thread>
#include <stdint.h>
#include <netinet/tcp.h>
#include <errno.h>
#include <stdlib.h>
#include <iomanip>

extern int	errno;



#define MTU 1460 /* buffer size in byte for send() function */


void* dummyData = calloc(MTU, 8); //dummy data for sending.



static std::mutex stdOutMutex;


// Host IDs start at 1 
// Rack IDs start at 1
// IP parts start at 1

int getRackId(int serverId, int numServersPerRack) {
    int r = ((serverId - 1) / numServersPerRack) + 1;

    // 
    // 1-n -> 1
    // n+1 - 2n ->2
    // ...
    return r;
}

std::string getIp(int serverId, int numServersPerRack, std::string ipBase) {
    int r = getRackId(serverId, numServersPerRack);
    int s = serverId - (r-1)*numServersPerRack;
    
    //return "127.0.0.1";
    
    return ipBase + "." + std::to_string(r) + "." + std::to_string(s);
}

bool compairFlow (struct flow i,struct flow j) { return (i.start<j.start); }


void counterDecrease(int idx, int* openConnections, std::mutex **mutexe) {
    mutexe[idx]->lock();
    openConnections[idx]--;
    mutexe[idx]->unlock();
}
void counterIncrease(int idx, int* openConnections, std::mutex **mutexe) {
    mutexe[idx]->lock();
    openConnections[idx]++;
    mutexe[idx]->unlock();
}

int getBytes(int srcServerId, int *bucket, std::chrono::high_resolution_clock::time_point *lastBucketUpdate, std::mutex **mutexe, int bitrateSingleHost, int maxBytes) {
	
	//bucket algorithm that fills the bucket every 250 ms.
	
    typedef std::chrono::high_resolution_clock Clock;
    //typedef std::chrono::milliseconds milliseconds;
	
	int ret = 1460;
	
	if(maxBytes < ret)
		ret = maxBytes;
	
	mutexe[srcServerId]->lock();
	int inBucket = bucket[srcServerId];
	if( inBucket > 0) {
		if(inBucket < ret)
			ret = bucket[srcServerId];
		
		bucket[srcServerId] -= ret;
		
		
	} else {
        Clock::time_point t0 = Clock::now();
        
        std::chrono::microseconds diff = std::chrono::duration_cast<std::chrono::microseconds>(t0 - lastBucketUpdate[srcServerId]);
		if(diff.count() > 250L*1000L) {
			//can make an immediate update.
		} else {
			//have to wait for the next bucket update
			long long toSleep = 250L*1000L - diff.count();
			
			std::this_thread::sleep_for(std::chrono::microseconds(toSleep));
			

		}
		bucket[srcServerId] = (bitrateSingleHost/8)/4;	// /4 because of 250ms update interval.
		if(bucket[srcServerId] < ret)
			ret = bucket[srcServerId];
		
		bucket[srcServerId] -= ret;
		lastBucketUpdate[srcServerId] = Clock::now();
		
	}
	
	
	mutexe[srcServerId]->unlock();
	
	return ret;
}


int sendData(const char* srcIp, const char* dstIp, int byteCount, const char* interface, int** openConnections, std::mutex **mutexe, int srcServerId,
			 int bitRateSingleHost, int *bucket, std::chrono::high_resolution_clock::time_point *lastBucketUpdate, double latency, std::ostream & out, long startms) {
    int sock;
    struct sockaddr_in servAddr; /* server address */
    struct sockaddr_in src; /* src address */
    
    typedef std::chrono::high_resolution_clock Clock;
    typedef std::chrono::microseconds microseconds;
    
    Clock::time_point t0 = Clock::now();
    Clock::time_point t1;
    microseconds diff;
    
    if ((sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
        out << "socket() failed";
        counterDecrease(srcServerId, *openConnections, mutexe);
        return 1;
    }
    
    /*
    //bind socket to interface:
    if(!setsockopt(sock, SOL_SOCKET, SO_BINDTODEVICE, interface, sizeof(interface))) {
        perror("SO_BINDTODEVICE");
        counterDecrease(srcServerId, openConnections, mutexe);
        return 1;
    }
    */
    
    
    /* Construct the server address structure */
    memset(&servAddr, 0, sizeof(servAddr));         /* Zero out structure */
    servAddr.sin_family      = AF_INET;             /* Internet address family */
    servAddr.sin_addr.s_addr = inet_addr(dstIp);    /* Server IP address */
    servAddr.sin_port        = htons(13373);        /* Server port */
    
    /* src struct */
    memset(&src, 0, sizeof(src));               /* Zero out structure */
    src.sin_family      = AF_INET;              /* Internet address family */
    src.sin_addr.s_addr = inet_addr(srcIp);     /* Server IP address */
    src.sin_port        = htons(0);             /* Server port */
    
    
    
    //bind socket to src ip:
    if (bind(sock, (struct sockaddr*)&src, sizeof src) != 0) {
        char buf[200];
        sprintf(buf, "could not bind to %s", srcIp);
        perror(buf);
        counterDecrease(srcServerId, *openConnections, mutexe);
        return 1;
    }
    
    
    /* Establish the connection to the echo server */
    if (connect(sock, (struct sockaddr *) &servAddr, sizeof(servAddr)) < 0) {
        char buf[200];
        sprintf(buf, "connect to %s failed", dstIp);
        perror(buf);
        counterDecrease(srcServerId, *openConnections, mutexe);
        return 1;
    }
    
    
    int mtu = MTU; //generate 1460 bytes (hopefully 1 packet) at once.

    int sentBytes = 0;
    
    typedef std::chrono::high_resolution_clock Clock;
    
    /* Send the data to the server */
    
    if(byteCount < mtu)
        mtu = byteCount;
	
	void * sendData = dummyData;
	
	bool alloc = false;
    
    while(sentBytes < byteCount) {
		
		int min = byteCount - sentBytes;
		if(mtu < min)
			min = mtu;
		
		
		int bytesSendThisTime = getBytes(srcServerId, bucket, lastBucketUpdate, mutexe, bitRateSingleHost, min);
		

		
		//this is the last message. Fill them with ones to mark the transmission as beeing completed.
		if((byteCount - sentBytes) - bytesSendThisTime == 0) {
			char* ones = (char*)malloc(MTU);
			alloc = true;
			for(int i = 0; i < MTU; i++) {
				ones[i] = '1';
			}
			sendData = ones;
		} else {
			alloc = false;
		}
        
        if (send(sock, sendData, bytesSendThisTime, 0) != bytesSendThisTime) {
            out << "send() sent a different number of bytes than expected";
            counterDecrease(srcServerId, *openConnections, mutexe);
			if(alloc)
				free(sendData);
            return 1;
        }
		
		if(alloc)
			free(sendData);
		
		sentBytes += bytesSendThisTime;

    }
	
	//wait for the connection to be closed by the remote peer.
	char* recBuffer = (char*)malloc(MTU);
	recv(sock, recBuffer, MTU, 0);
	
	free(recBuffer);
	
    t1 = Clock::now();
    
    close(sock);
    

    
    diff = std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0);
    
    float ms =diff.count() / 1000.0 + 2*latency;
	
	
    
    int rate = (sentBytes / (ms / 1000.0) / 1000.0) * 8.0;
    
    stdOutMutex.lock();
    char mbstr[100];
    auto ctime = Clock::to_time_t(t1);
    if (!std::strftime(mbstr, sizeof(mbstr), "%c %Z ", std::localtime(&ctime)))
        std::cerr << "Failed to call strftime?" << std::endl;

    out << mbstr << sentBytes << " bytes in " << ms << " ms " << rate << " kbit/s from "  << srcIp << " to " << dstIp 
        << "start: " << startms << "ms end: "<< startms + (diff.count()/1000) << std::endl;
    stdOutMutex.unlock();
    

    counterDecrease(srcServerId, *openConnections, mutexe);
    return(0);
}



