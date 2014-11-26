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

int sendData(const char* srcIp, const char* dstIp, const int byteCount, const long startms, std::ostream& out, const bool enablemtcp, const int retries = 3);

int diff_ms(timeval t1, timeval t2);

#endif
