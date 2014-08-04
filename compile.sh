#!/bin/bash
srcDir=`pwd`

sed -i 's/TIME_UTC_/TIME_UTC/g' threadpool/boost/./threadpool/./detail/../task_adaptors.hpp

#cd $srcDir/server
#g++ -std=gnu++11 -pthread -o server TCPEchoServer-Thread.c DieWithError.c HandleTCPClient.c AcceptTCPConnection.c CreateTCPServerSocket.c

cd $srcDir/trafficGeneratorServer/trafficGeneratorServer/trafficGeneratorServer
g++ -std=gnu++11 -pthread -o server main.cpp


cd $srcDir/trafficGenerator/trafficGenerator
g++ -pthread -std=gnu++11 -I../../threadpool/boost/ main.cpp utils.cpp -lboost_thread -lboost_system -o traffGen
