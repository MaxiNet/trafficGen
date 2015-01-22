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
#include <netdb.h>
#include <cstdlib>


extern int	errno;

#define BUFSIZE 2*1024*1024



void* dummyData = calloc(BUFSIZE, 8); //dummy data for sending.



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





ssize_t sendData(const flow f, const long startms, std::ostream & out,
                 const bool enablemtcp, const int participatory, const int participatorySleep, const double falsePositives, const int retries, volatile bool* has_received_signal,
                 pthread_mutex_t* running_mutex, volatile int* running_threads) {
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
        return -1;
    }

#ifdef __linux__
#ifndef MPTCP_ENABLED
#define MPTCP_ENABLED          26
#endif
    int enablemtcpint = enablemtcp;
    if (setsockopt(sock, SOL_TCP, MPTCP_ENABLED, &enablemtcpint, sizeof(enablemtcpint)) && enablemtcp)
#else
        if(enablemtcp)
#endif
        {
            perror ("Setting mptcp on socket");
            out << "Error enabling Multipath TCP!" << std::endl;
            exit(32);
        }


    /* Construct the server address structure */
    memset(&servAddr, 0, sizeof(servAddr));         /* Zero out structure */
    servAddr.sin_family      = AF_INET;             /* Internet address family */
    servAddr.sin_addr.s_addr = inet_addr(f.toIP.c_str());    /* Server IP address */
    servAddr.sin_port        = htons(13373);        /* Server port */

    /* src struct */
    memset(&src, 0, sizeof(src));               /* Zero out structure */
    src.sin_family      = AF_INET;              /* Internet address family */
    src.sin_addr.s_addr = inet_addr(f.fromIP.c_str());     /* client IP address */
    src.sin_port        = htons(0);             /* client port */



    //bind socket to src ip:
    if (bind(sock, (struct sockaddr*)&src, sizeof src) != 0) {
        char buf[200];
        sprintf(buf, "could not bind to %s", f.fromIP.c_str());
        perror(buf);
        return -2;
    }

    ssize_t sentBytes = 0;

    /* Establish the connection to the echo server */
    if (connect(sock, (struct sockaddr *) &servAddr, sizeof(servAddr)) < 0) {
        char buf[200];
        sprintf(buf, "connect to %s failed", f.toIP.c_str());
        perror(buf);
        goto finished;
    }


    /* PARTICIPATORY: inform the controller of this flow
     if flow is larger than given value, we send a UDP datagram to IP 10.255.255.254
     containing all information of the flow; the server will use this information for traffic engineering
     */
	{
		//false positives: coin toss
		bool falsePositive = false;
		int coin = rand();
		double w = coin / double(RAND_MAX);
		if(w <= falsePositives) {
			//this is going to be a false positive
			falsePositive = true;
		}
		
		if(((participatory > 0) && (f.bytes >= participatory)) || falsePositive) {
			if(participatorySleep == 0) {
				//mein OpenFlow controller hat scheinbar ein Problem, wenn der Flow instantan gemeldet wird. da geht dann ein SYN ack oder so verloren; daher warten wir ein ganz bisschen (10 ms)
				std::thread newthread(informAboutElephant, f, &src, &servAddr, sock, 10);
				newthread.detach();
			} else {
				std::thread newthread(informAboutElephant, f, &src, &servAddr, sock, participatorySleep);
				newthread.detach();
			}
		}
	
	}

    typedef std::chrono::high_resolution_clock Clock;

    /* Send the data to the server */

    {
        void * sendData = dummyData;

        while(sentBytes < f.bytes - 1 and *has_received_signal == false) {

            auto sendThisTime = (f.bytes-1) - sentBytes;

            if(sendThisTime > BUFSIZE)
                sendThisTime = BUFSIZE;


            ssize_t sent = send (sock, sendData, sendThisTime, 0);

            if (sent <= 0) {
                perror ("error while sending");
                goto finished;
            }

            sentBytes += sent;

        }

        //this is the last message. Contains a one to mark the transmission as beeing completed.
        char* ones = (char*)malloc(sizeof(char));
        ones[0] = '1';
        ssize_t sent = send(sock, (void*) ones, sizeof(char), 0);
        free(ones);
        if (sent <= 0) {
            perror ("error while sending '1' Byte");
            goto finished;
        }
        sentBytes+=1;


    }

    {
        //wait for the connection to be closed by the remote peer.
        char* recBuffer = (char*)malloc(BUFSIZE);
        recv(sock, recBuffer, BUFSIZE, 0);
        free(recBuffer);
    }

    //### GOTO: finished
finished:
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

    auto marker = " finished";
    if (sentBytes < f.bytes)
        marker = " partial";
    if(*has_received_signal == true)
        marker = " aborted";

    auto endms = startms + (diff.count()/1000);
    out << startms << " " <<  endms << " " << sentBytes << " of " << f.bytes << " bytes in " << ms << " ms " << rate
    << " kbit/s from "  << f.fromIP << " to " << f.toIP << marker << " #" << f.number << std::endl;

    out.flush();
    stdOutMutex.unlock();

    if(f.bytes - sentBytes > 0 && retries > 0 && *has_received_signal == false) {
        sendData(f, startms, out, enablemtcp, participatory, participatorySleep, falsePositives, retries-1, has_received_signal, running_mutex, running_threads);
    }

    pthread_mutex_lock(running_mutex);
    *running_threads = *running_threads - 1;
    pthread_mutex_unlock(running_mutex);

    return (f.bytes - sentBytes);
}



void informAboutElephant(const flow & f, struct sockaddr_in* src, struct sockaddr_in* servAddr, int sock, int elephantSleep) {
    struct flowIdent fi;

    socklen_t len = sizeof(struct sockaddr);

    //get local ip and port:
    if(getsockname(sock, (struct sockaddr*) src, &len) < 0) {
        perror("getsockname");
    } else {

        fi.portSrc = src->sin_port;       //in network byte order
        fi.portDst = servAddr->sin_port;  //in network byte order
        fi.flowSize = htonl(f.bytes);



        strncpy(fi.ipSrc, f.fromIP.c_str(), 16);
        strncpy(fi.ipDst, f.toIP.c_str(), 16);
    }

	if(elephantSleep > 1) {
		std::this_thread::sleep_for(std::chrono::milliseconds(elephantSleep));
	}
	
    //send udp datagram:

    const char* hostname="10.255.255.254";
    const char* portname="2222";

    struct addrinfo hints;
    memset(&hints,0,sizeof(hints));
    hints.ai_family=AF_UNSPEC;
    hints.ai_socktype=SOCK_DGRAM;
    hints.ai_protocol=0;
    hints.ai_flags=AI_ADDRCONFIG;

    struct addrinfo* res = 0;

    if(getaddrinfo(hostname,portname,&hints,&res) < 0) {
        perror("getaddrinfo informAboutElephant");
    }

    int fd=socket(res->ai_family,res->ai_socktype,res->ai_protocol);
    if (fd==-1) {
        perror("socket informAboutElephant");
    } else {
        //send packet to controller:
        if (sendto(fd,&fi,sizeof(fi), 0, res->ai_addr,res->ai_addrlen) < 0) {
            perror("sendto informAboutElephant");
        }
    }
}