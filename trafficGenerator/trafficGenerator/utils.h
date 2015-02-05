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
#include <stdint.h>
#include <mutex>
#include <sys/socket.h>
struct flow;


struct flowIdent {
    char ipSrc[16];     //0-15
    char ipDst[16];     //16-31
    uint16_t portSrc;        //32-33
	uint16_t portDst;        //34-35
    uint32_t flowSize;       //36-40
};

struct trafficGenConfig {
    bool participatoryIsDifferentPort;
    int participatory;
    int participatorySleep;
    double falsePositives;
    bool enablemtcp;
	double scaleFactorSize;
};

int getRackId(int serverId, int numServersPerRack);
std::string getIp(int serverId, int numServersPerRack, std::string ipBase);
bool compareFlow (struct flow i,struct flow j);


void informAboutElephant(const flow & f, struct sockaddr_in* src, struct sockaddr_in* servAddr, int sock, int elephantSleep, const trafficGenConfig& tgConf);

ssize_t sendData(const flow f, const long startms, std::ostream& out, const trafficGenConfig & tgConf, const int retries, volatile bool* has_received_signal, pthread_mutex_t* running_mutex, volatile int* running_threads);


int diff_ms(timeval t1, timeval t2);

#endif
