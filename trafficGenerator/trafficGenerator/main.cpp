//
//  main.cpp
//  trafficGenerator
//
//  Created by Philip Wette on 27.06.13.
//  Copyright (c) 2013 Uni Paderborn. All rights reserved.
//


#include <iostream>
#include <fstream>
#include <string>
#include <cstddef>
#include <boost/tokenizer.hpp>
#include <boost/ptr_container/ptr_list.hpp>
#include "flow.h"
#include "utils.h"
#include "threadpool.hpp"
#include <chrono>
#include <thread>
#include <mutex>

using namespace boost::threadpool;

using namespace std;
using namespace boost;

const static char* argname[] = { "hostsperRack", "ipBase", "startId", "endId", "flowFile", "scaleFactorSize", "scaleFactorTime", "bitRateSingleHost", "generateInRackTraffic", "latency", "mtcp", "logfile"};
static const int numargs = 11;
static const int optargs = 2;

static const bool debug=false;

static void usage(const char* name,std::ostream & out)
{
    out << "usage: " << name;
    for (unsigned int i=0;i < numargs;i++)
        out << " "<< argname[i];

}


std::ostream & getOut(int argc, const char* argv[]) 
{
    if (argc >= 13) {
        auto fout = new std::ofstream(argv[12]);
        return *fout;
    } else {
        return std::cout;
    }
}



int main (int argc, const char * argv[])
{
    std::ostream & out= getOut(argc, argv);

    if (debug) {
        for(int i = 1; i < std::max(argc,numargs); i++) {
            if (i - 1 < numargs)
                out << argname[i-1] << " = ";
            else
                out << "argv[" << i << "] = ";
            
            if (i < argc)
                out << argv[i] << endl;
            else if ( i -1 < numargs-optargs)
                out << " missing" << std::endl;
            else
                out << " optional" << std::endl;
        }
    }
    if (argc -1 < numargs-optargs) {
        std::cerr << "not enoguh parameters. Got " << argc -1 << " need " << numargs -optargs << std::endl;
        usage(argv[0], out);
        return 1;
    }
    
    int hostsPerRack = atoi(argv[1]);
    string ipBase = argv[2];
    
    int startid = atoi(argv[3]);
    int endid = atoi(argv[4]);
    string flowFile = argv[5];
    
    double scaleFactorSize = stod(argv[6]);
    
    double scaleFactorTime = stod(argv[7]);
	
	int bitRateSingleHost = atoi(argv[8]);
	
	int generateInRackTraffic = atoi(argv[9]);
	
	double latency = stod(argv[10]);
    bool enablemtcp;

    if (argc >= 12)
        enablemtcp = atoi(argv[11]);
    else
        enablemtcp= false;
    
    const char *interface = "en1";
    

    int *openConnections = (int*)malloc((endid - startid +1) * sizeof(int));
    int *bucket = (int*)malloc((endid - startid +1) * sizeof(int));
    typedef std::chrono::high_resolution_clock Clock;
    Clock::time_point *lastBucketUpdate = (Clock::time_point*)malloc((endid - startid +1) * sizeof(Clock::time_point));
	
    
    std::mutex **mutexe = (std::mutex**)malloc((endid - startid +1) * sizeof(std::mutex*));
    for(int i = 0; i < endid - startid + 1; i++) {
        mutexe[i] = new std::mutex();
        openConnections[i] = 0;
		bucket[i] = 0;
		lastBucketUpdate[i] = Clock::now();
    }
    
    
    //read flows from file - only hold the ones we really want to have:
    ifstream infile;
	infile.open (flowFile.c_str());
    string line;
    
    std::vector<struct flow> flows;

    int numFlows = 0;
    if(!infile.good()) {
        out << "Some errors has occured opening " << flowFile << std::endl;
        return 7;
    } 

    if (true || debug)
        out << "IPs: " << getIp(startid, hostsPerRack, ipBase) << " - " << getIp(endid, hostsPerRack, ipBase) << std::endl;

    while(!infile.eof()) {
        getline(infile,line);
        
        if (line.front() == '#')
            continue;
        
        //check if this flow belongs to us:
        size_t pos = line.find_first_of(",");
        string sid = "";
        if(pos!=std::string::npos) {
            sid = line.substr(0, pos);
        }
        int id = -1;
        if(sid != "")
            id = stoi(sid);
        
        if(id >= startid && id <= endid) {
            //we keep this line. Append it to string.
            
            //style= 1, 4, 40.06287, 11045.23 (from, to, time, bytes)
            struct flow f;
            f.fromString(line, ipBase, hostsPerRack, scaleFactorSize, scaleFactorTime);
			
            if(debug)
                out << "flow from " << f.fromIP << " to " << f.toIP  << endl;
			
			
			//check if this is in rack traffic - if so it may be skipped.
			if(generateInRackTraffic == 0) {
				if(getRackId(f.fromId, hostsPerRack) == getRackId(f.toId, hostsPerRack))
					continue;
			}
            
            // WATT?!
            if(f.bytes < 1)
                f.bytes = f.bytes*2;
            
            if(f.bytes > 1) {
                flows.push_back(f);
                numFlows++;
            }
            cout << ".";
        }

    }
	infile.close();
    cout  << endl;
    
    cout << "read " << numFlows << " flows. Using " << flows.size() << " flows" << endl;
    
    //sort by time:
    std::sort (flows.begin(), flows.end(), compairFlow);
    
   //debug
    out << "Flow start times: ";
    for (auto f:flows) {
       out << ", " << f.start;
    }
    out << std::endl;
    
    cout << "creating thread pool..."  << endl;
    
    //send data according to schedule.
    
    pool tp((endid - startid +1) * 4);   //create a thread pool
    
    

    
    //tp->schedule(boost::bind(task_with_parameter, 42));
    
    
    cout << "start scheduling flows..."  << endl;
    
    typedef std::chrono::high_resolution_clock Clock;
    typedef std::chrono::milliseconds milliseconds;
    
    Clock::time_point t0 = Clock::now();
    Clock::time_point t1;
    milliseconds diff;
    
    for (struct flow f:flows) {
        t1 = Clock::now();
        diff = std::chrono::duration_cast<milliseconds>(t1 - t0);
        
        //cout << "flow should start at " << f.start << " diff " << diff.count() << "\n";
        
        if(diff.count() >= f.start) {
            std::cout << diff.count() << ">=" << f.start << "\n";
            //we have to start this flow!
        } else {
            //cout << "sleeping for " << f.start - diff.count() << "\n";
            //we should wait for the flow to begin...
            std::this_thread::sleep_for(milliseconds(f.start - diff.count()));
        }
        //execute flow:
		
		//we have to add latency for inRack Communications!
		double lat = 0.0;
		if(getRackId(f.fromId, hostsPerRack) == getRackId(f.toId, hostsPerRack))
			lat = latency;
		
        int idx = f.fromId - startid;
        counterIncrease(idx, openConnections, mutexe);
        tp.schedule(std::bind(sendData, f.fromIP.c_str(), f.toIP.c_str(),
                   (int)f.bytes, interface, &openConnections, mutexe, idx, 
                    bitRateSingleHost, bucket, lastBucketUpdate, lat, 
                              std::ref(out), diff.count(), enablemtcp));

    }
    
    
    return 0;
}

