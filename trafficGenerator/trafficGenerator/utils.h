//
//  utils.h
//  trafficGenerator
//
//  Created by Philip Wette on 27.06.13.
//  Copyright (c) 2013 Uni Paderborn. All rights reserved.
//

#ifndef trafficGenerator_utils_h
#define trafficGenerator_utils_h

#include <math.h>
#include <string>
#include <stdlib.h>
#include <time.h>
#include <mutex>
#include <sys/socket.h>
struct flow;


struct flowIdent {
    char ipSrc[16];     //0-15
    char ipDst[16];     //16-31
    int portSrc;        //32-35
    int portDst;        //36-39
    int flowSize;       //40-43
};

int getRackId(int serverId, int numServersPerRack);
std::string getIp(int serverId, int numServersPerRack, std::string ipBase);
bool compairFlow (struct flow i,struct flow j);

void informAboutElephant(const int participatory, const char* srcIp, const char* dstIp, struct sockaddr_in* src, struct sockaddr_in* servAddr, int sock);

int sendData(const char* srcIp, const char* dstIp, const int byteCount, const long startms, std::ostream& out,
             const bool enablemtcp, const int participatory, const int retries = 3);

int diff_ms(timeval t1, timeval t2);

#endif
