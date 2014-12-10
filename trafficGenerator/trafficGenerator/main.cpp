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
#include <signal.h>
#include <unistd.h>
#include <boost/program_options.hpp>

using namespace std;

namespace po = boost::program_options;


static const bool debug=false;

static bool has_received_signal=false;
static bool has_received_signal_go=false;


std::ostream & getOut(const std::string outfile)
{
    if (outfile != "-") {
        auto fout = new std::ofstream(outfile);
        return *fout;
    } else {
        return std::cout;
    }
}

void setFlag(int sig) {
	has_received_signal = true;
}

void gogogo(int sig) {
	has_received_signal_go = true;
}


int main (int argc, const char * argv[])
{

	(void) signal(SIGUSR1, setFlag);
	(void) signal(SIGUSR2, gogogo);
	


    boost::program_options::positional_options_description pd;

    for (const char* arg: {"hostsPerRack", "ipBase", "hostId", "flowFile", "scaleFactorSize", "scaleFactorTime", "participatory"}) {
        pd.add (arg, 1);
    }


    int hostsPerRack;
    string ipBase;
    
    int hostId;

    string flowFile;

    double scaleFactorSize;
    
    double scaleFactorTime;

	int participatory;

    bool enablemtcp;
    long cutofftime;
    typedef std::chrono::high_resolution_clock Clock;


    // Declare the supported options.
    po::options_description desc("Allowed options");
    desc.add_options()
    ("help", "produce help message")
    ("hostsPerRack", po::value<int>(&hostsPerRack)->required(), "hosts per Rack")
    ("ipBase", po::value<std::string>(&ipBase)->required(), "prefix of all IPs, e.g. 10.0")
    ("hostId", po::value<int>(&hostId)->required(), "id of the own host (starts at 0?)")
    ("flowFile", po::value<std::string>(&flowFile)->required(), "flowfile to read at startup")
    ("scaleFactorSize", po::value<double>(&scaleFactorSize)->required(), "Multiply each flow size by this factor")
    ("scaleFactorTime", po::value<double>(&scaleFactorTime)->required(), "Timedillation factor, larger is slower")
    ("participatory", po::value<int>(&participatory)->default_value(0), "Announce flows larger than this")
    ("mptcp", po::value<bool>(&enablemtcp)->default_value(false), "Enable MPTCP per socket option")
    ("logFile", po::value<std::string>()->default_value("-"), "Log file")
    ("cutOffTime", po::value<long>(&cutofftime)->default_value(0), "Don't play flower newer than this")
    ;

    po::variables_map vm;
    po::store(po::command_line_parser(argc, argv).options(desc).positional(pd).run(), vm);
    po::notify(vm);

    //read flows from file - only hold the ones we really want to have:
    ifstream infile;
	infile.open (flowFile.c_str());
    string line;
    
    std::vector<struct flow> flows;

    int numFlows = 0;

    std::ostream & out= getOut(vm["logFile"].as<std::string>());

    if(!infile.good()) {
        out << "Some errors has occured opening " << flowFile << std::endl;
        return 7;
    } 

    if (debug)
        out << "IP: " << getIp(hostId, hostsPerRack, ipBase) << std::endl;

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
        
        if(id == hostId) {
            //we keep this line. Append it to string.
            
            //style= 1, 4, 40.06287, 11045.23 (from, to, time, bytes)
            struct flow f;
            f.fromString(line, ipBase, hostsPerRack, scaleFactorSize, scaleFactorTime);
			
            if(debug)
                out << "flow from " << f.fromIP << " to " << f.toIP  << endl;

            if (cutofftime > 0 && cutofftime * scaleFactorTime > f.start )
                // Ignore the flow
                continue;

            // WATT?!
            if(f.bytes < 1)
                f.bytes = f.bytes*2;
            
            if(f.bytes > 1) {
//            if(f.bytes > 1 && f.bytes < 30*1000*1000) {
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
    std::sort(flows.begin(), flows.end(), compairFlow);
    
   //debug
	if(debug) {
		out << "Flow start times: ";
		for (auto f:flows) {
			out << ", " << f.start;
		}
		out << std::endl;
	}
    
    cout << "creating thread pool..."  << endl;
    
    //send data according to schedule.
    
    //pool tp(256);   //create a large thread pool
    
	
	
	//waiting for GO signal:
	while(has_received_signal_go == false) {
		sleep(1);
	}
	

    
    //tp->schedule(boost::bind(task_with_parameter, 42));
    
    
    cout << "start scheduling flows..."  << endl;
    
    typedef std::chrono::high_resolution_clock Clock;
    typedef std::chrono::milliseconds milliseconds;
    
    Clock::time_point t0 = Clock::now();
    Clock::time_point t1;
    milliseconds diff;
	
	volatile int running_threads = 0;
	pthread_mutex_t running_mutex = PTHREAD_MUTEX_INITIALIZER;
    
    for (struct flow f:flows) {
        t1 = Clock::now();
        diff = std::chrono::duration_cast<milliseconds>(t1 - t0);
        
        //cout << "flow should start at " << f.start << " diff " << diff.count() << "\n";
        
        if(diff.count() >= f.start) {
            //std::cout << diff.count() << ">=" << f.start << "\n";
            //we have to start this flow!
        } else {
            //cout << "sleeping for " << f.start - diff.count() << "\n";
            //we should wait for the flow to begin...
            std::this_thread::sleep_for(milliseconds(f.start - diff.count()));
        }
        //execute flow:

        // tp.schedule(std::bind(sendData, f.fromIP.c_str(), f.toIP.c_str(), (int)f.bytes,
        //                      diff.count(), std::ref(out), enablemtcp, participatory, 3, &has_received_signal)
        //            );
		
		pthread_mutex_lock(&running_mutex);
		running_threads++;
		pthread_mutex_unlock(&running_mutex);
		
		new std::thread(sendData, f.fromIP.c_str(), f.toIP.c_str(), (int)f.bytes,
				   diff.count(), std::ref(out), enablemtcp, participatory, 3, &has_received_signal, &running_mutex, &running_threads);
		

    }
	
	pthread_mutex_lock(&running_mutex);
	int n = running_threads;
	pthread_mutex_unlock(&running_mutex);
	while(n > 0) {
		pthread_mutex_lock(&running_mutex);
		n = running_threads;
		pthread_mutex_unlock(&running_mutex);
		sleep(1);
	}
	
	
    return 0;
}

