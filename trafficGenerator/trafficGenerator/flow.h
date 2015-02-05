//
//  flow.h
//  trafficGenerator
//
//  Created by Philip Wette on 27.06.13.
//  Copyright (c) 2013 Uni Paderborn. All rights reserved.
//
#include <string>
#include "utils.h"
#include <boost/tokenizer.hpp>

#ifndef trafficGenerator_main_h
#define trafficGenerator_main_h

struct flow {
    int fromId;
    int toId;
    long long start; //start in ms.
    ssize_t bytes;
    std::string fromIP;
    std::string toIP;
    int number;
    
    void fromString(std::string line, std::string ipBase, int numServersPerRack, double scaleFactorTime) {
        //style= 1, 4, 40.06287, 11045.23 (from, to, time, bytes)
        
        typedef boost::tokenizer<boost::char_separator<char> > tokenizer;
        boost::char_separator<char> sep(",");
        
        tokenizer tokens(line, sep);
        int i = 0;
        for (tokenizer::iterator tok_iter = tokens.begin(); tok_iter != tokens.end(); ++tok_iter) {
            std::string v = *tok_iter;

            if(i == 0) fromId = stoi(v);
            if(i == 1) toId = stoi(v);
            if(i == 2) start = (long long)((stod(v) * scaleFactorTime * 1000.0));
            if(i == 3) {
                double flowBytes = stod(v);
                bytes = flowBytes;
            }


            
            i++;
        }
        
        fromIP = getIp(fromId, numServersPerRack, ipBase);
        toIP = getIp(toId, numServersPerRack, ipBase);
        
	};
	
	
	ssize_t getSize(const trafficGenConfig &tgConf) const {
		return bytes * tgConf.scaleFactorSize;
	};
	
};


#endif
