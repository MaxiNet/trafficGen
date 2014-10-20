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
struct flow;

int getRackId(int serverId, int numServersPerRack);
std::string getIp(int serverId, int numServersPerRack, std::string ipBase);
bool compairFlow (struct flow i,struct flow j);

void counterIncrease(int idx, int* openConnections, std::mutex **mutexe);
void counterDecrease(int idx, int* openConnections, std::mutex **mutexe);

int sendData(const char* srcIp, const char* dstIp, int byteCount, const char* interface, int** openConnections, std::mutex **mutexe, int srcServerId,
			 int bitRateSingleHost, int *bucket, std::chrono::high_resolution_clock::time_point *lastBucketUpdate, double latency, std::ostream& out, long startms);

int getBytes(int srcServerId, int *bucket, std::chrono::high_resolution_clock::time_point *lastBucketUpdate, std::mutex **mutexe, int bitrateSingleHost, int maxBytes, bool enablemtcp);

int diff_ms(timeval t1, timeval t2);

#endif
