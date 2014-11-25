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
#include <chrono>
#include <iostream>

extern int	errno;




void* dummyData = calloc(64*1024, 8); //dummy data for sending.



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





int sendData(const char* srcIp, const char* dstIp, int byteCount, long startms, std::ostream & out, bool enablemtcp) {
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
        return 1;
    }

#ifdef __linux__
#ifndef MPTCP_ENABLED
#define MPTCP_ENABLED          26
#endif
    int enablemtcpint = enablemtcp;
    if(setsockopt(sock, SOL_TCP, MPTCP_ENABLED, &enablemtcpint, sizeof(enablemtcpint)) && enablemtcp) {
#else
   if(enablemtcp) {
#endif
       perror ("Setting mptcp on socket");
       out << "Error enabling Multipath TCP!" << std::endl;
       exit(32);
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
        return 1;
    }
    
    
    /* Establish the connection to the echo server */
    if (connect(sock, (struct sockaddr *) &servAddr, sizeof(servAddr)) < 0) {
        char buf[200];
        sprintf(buf, "connect to %s failed", dstIp);
        perror(buf);
        return 1;
    }

    int sentBytes = 0;
    
    typedef std::chrono::high_resolution_clock Clock;
    
    /* Send the data to the server */
	
	void * sendData = dummyData;
	bool alloc = false;	//did we alloc ones?
    
    while(sentBytes < byteCount) {
		
		int sendThisTime = byteCount - sentBytes;
		
		if(sendThisTime > 64*1024)
			sendThisTime = 64*1024;
		
		
		//this is the last message. Fill them with ones to mark the transmission as beeing completed.
		if((byteCount - sentBytes) - sendThisTime == 0) {
			char* ones = (char*)malloc(sendThisTime);
			alloc = true;
			for(int i = 0; i < sendThisTime-1; i++) {
				ones[i] = '0';
			}
			ones[sendThisTime-1] = '1';
			sendData = ones;
		} else {
			alloc = false;
		}
		
		
		size_t sent = send (sock, sendData, sendThisTime, 0);
		
        if (sent <= 0) {
            perror ("error while sending");
            out << "send() sent a different number of bytes than expected";
			if(alloc)
				free(sendData);
            return 1;
        }
		
		if(alloc)
			free(sendData);
		
		sentBytes += sent;

    }
	
	//wait for the connection to be closed by the remote peer.
	char* recBuffer = (char*)malloc(64*1024);
	recv(sock, recBuffer, 64*1024, 0);
	
	free(recBuffer);
	
    t1 = Clock::now();

    std::chrono::system_clock sysclock;
    auto tnow =sysclock.now();

    close(sock);
    

    
    diff = std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0);
    
    float ms =diff.count() / 1000.0;
	
	
    
    int rate = (sentBytes / (ms / 1000.0) / 1000.0) * 8.0;
    
    stdOutMutex.lock();
    char mbstr[100];

    auto ctime = std::chrono::system_clock::to_time_t(tnow);
    if (!std::strftime(mbstr, sizeof(mbstr), "%c %Z ", std::localtime(&ctime)))
        std::cerr << "Failed to call strftime?" << std::endl;

    auto endms = startms + (diff.count()/1000);
    out << startms << " " <<  endms << " " << sentBytes << " bytes in " << ms << " ms " << rate
        << " kbit/s from "  << srcIp << " to " << dstIp << std::endl;

    out.flush();
    stdOutMutex.unlock();
    

    return(0);
}



