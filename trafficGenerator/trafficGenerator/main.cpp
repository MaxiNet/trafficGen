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
#include <chrono>
#include <thread>
#include <mutex>
#include <signal.h>
#include <unistd.h>
#include <boost/program_options.hpp>
#include <boost/exception/diagnostic_information.hpp>

namespace po = boost::program_options;


static const bool debug=false;

static bool has_received_signal=false;
static bool has_received_signal_go=false;


std::ostream & getOut(const std::string outfile);
void setFlag(int sig);
void gogogo(int sig);


std::ostream & getOut(const std::string outfile)
{
    if (outfile != "-") {
        std::cout << "Using file \"" << outfile << "\" as log output" << std::endl;
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


static void mainLoop(std::ostream & out, const struct std::vector<flow> flows, const trafficGenConfig & tgConf )
{
    out << "start scheduling flows..."  << std::endl;

    typedef std::chrono::high_resolution_clock Clock;
    typedef std::chrono::milliseconds milliseconds;

    Clock::time_point t0 = Clock::now();

    volatile int running_threads = 0;
    pthread_mutex_t running_mutex = PTHREAD_MUTEX_INITIALIZER;

    for (struct flow f:flows) {
        Clock::time_point  t1 = Clock::now();
        auto diff = std::chrono::duration_cast<milliseconds>(t1 - t0);

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
        pthread_mutex_lock(&running_mutex);
        running_threads++;
        pthread_mutex_unlock(&running_mutex);

        std::thread runner(sendData, f, diff.count(), std::ref(out), std::ref(tgConf), 3, &has_received_signal, &running_mutex, &running_threads);
        runner.detach();

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

    out << "All threads finished" << std::endl;
    out.flush();
    
}

static void readConfigFile(const std::string & config_file, po::variables_map & vm, const po::options_description  & allcfgopts)
{
    if ( config_file != "") {
        std::cout << "Using configuration file " << config_file << std::endl;
        std::ifstream ifs(config_file.c_str());
        if (!ifs)
        {
            std::cerr << "can not open config file: " << config_file << "\n";
            return;
        }

        try {
            store(po::parse_config_file(ifs, allcfgopts), vm);
        } catch (boost::exception & be) {
            std::cerr << "Error parsing config file "<< config_file << std::endl;
            std::cerr << boost::diagnostic_information(be) << std::endl;
            return;
        }

        po::notify(vm);
    }

}


static std::function<void(int)> sighuphandler;

int main (int argc, const char * argv[])
{

	(void) signal(SIGUSR1, setFlag);
	(void) signal(SIGUSR2, gogogo);

    int hostsPerRack;
    std::string ipBase;
    
    int hostId;

    std::string flowFile;
    
    double scaleFactorTime;

    bool doLoop;


    long cutofftime;
    typedef std::chrono::high_resolution_clock Clock;

    trafficGenConfig tgConf;
    std::string config_file;


    po::options_description cmdonly("Command line only");

    cmdonly.add_options()
    ("config,C", po::value<std::string>(&config_file)->default_value(""),"a configuration file");


    // Declare the supported options.
    po::options_description desc("Allowed options");
    desc.add_options()
    ("help", "produce help message")
    ("hostsPerRack", po::value<int>(&hostsPerRack)->required(), "hosts per Rack")
    ("ipBase", po::value<std::string>(&ipBase)->required(), "prefix of all IPs, e.g. 10.0")
    ("hostId", po::value<int>(&hostId)->required(), "id of the own host (starts at 0?)")
    ("flowFile", po::value<std::string>(&flowFile)->required(), "flowfile to read at startup")
    ("scaleFactorSize", po::value<double>(&tgConf.scaleFactorSize)->required(), "Multiply each flow size by this factor")
    ("scaleFactorTime", po::value<double>(&scaleFactorTime)->required(), "Timedillation factor, larger is slower")
    ("participatory", po::value<int>(&tgConf.participatory)->default_value(0), "Announce flows larger than this [in bytes]")
    ("participatorySleep", po::value<int>(&tgConf.participatorySleep)->default_value(0), "Wait some time (in ms) before announcing flows")
	("falsePositives", po::value<double>(&tgConf.falsePositives)->default_value(0.0), "False Positives: Factor of mice falsely reported as elephants")
    ("mptcp", po::value<bool>(&tgConf.enablemtcp)->default_value(false), "Enable MPTCP per socket option")
    ("participartoyDiffPort", po::value<bool>(&tgConf.participatoryIsDifferentPort)->default_value(false), "Use different port to announce elepehants")
    ("logFile", po::value<std::string>()->default_value("-"), "Log file")
    ("cutOffTime", po::value<long>(&cutofftime)->default_value(0), "Don't play flows newer than this [ms]")
    ("loop", po::value<bool>(&doLoop)->default_value(false), "Loop over the traffic file.")
    ;

    boost::program_options::positional_options_description pd;
    for(const char* arg: {"hostsPerRack", "ipBase", "hostId", "flowFile", "scaleFactorSize",
		"scaleFactorTime", "participatory", "participatorySleep"}) {
        pd.add (arg, 1);
    }

    po::options_description allcmdopts;
    po::options_description allcfgopts;

    allcmdopts.add(desc);
    allcmdopts.add(cmdonly);

    allcfgopts.add(desc);

    po::variables_map vm;
    po::store(po::command_line_parser(argc, argv).options(desc).positional(pd).run(), vm);
    po::notify(vm);


    readConfigFile(config_file, vm, allcmdopts);



    sighuphandler = std::bind (readConfigFile, config_file, std::ref(vm), std::ref(allcfgopts));
    signal(SIGHUP, [](int signum) { sighuphandler(signum); });
    



    //read flows from file - only hold the ones we really want to have:
    std::ifstream infile;
	infile.open (flowFile.c_str());
    std::string line;
    
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
        std::string sid = "";
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
            f.fromString(line, ipBase, hostsPerRack, scaleFactorTime);
			
            if(debug)
                out << "flow from " << f.fromIP << " to " << f.toIP  << std::endl;

            if (cutofftime > 0 && f.start > (cutofftime * scaleFactorTime) )
                // Ignore the flow
                continue;
            
            if(f.bytes >= 1) {
                flows.push_back(f);
                numFlows++;
            }
            out << ".";
        }

    }
	infile.close();
    out  << std::endl;
    
    out << "read " << numFlows << " flows. Using " << flows.size() << " flows (cutoff " << cutofftime << ")" << std::endl;
    
    //sort by time:
    std::sort(flows.begin(), flows.end(), compareFlow);
    int i=0;
    for (auto & f: flows){
        f.number=i++;
    }
    
   //debug
	if(debug) {
		out << "Flow start times: ";
		for (auto f:flows) {
			out << ", " << f.start;
		}
		out << std::endl;
	}
    
	//waiting for GO signal:
	while(has_received_signal_go == false) {
		sleep(1);
	}
	
    
    do {
        mainLoop(out, flows, tgConf);
    } while (doLoop);


	
    return 0;
}

