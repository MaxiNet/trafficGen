#!/bin/bash
srcDir=`pwd`

#sed -i 's/TIME_UTC_/TIME_UTC/g' threadpool/boost/./threadpool/./detail/../task_adaptors.hpp

if [ ${CXX}x = x ]; then
    CXX=g++
fi

#cd $srcDir/server
#g++ -std=gnu++11 -pthread -o server TCPEchoServer-Thread.c DieWithError.c HandleTCPClient.c AcceptTCPConnection.c CreateTCPServerSocket.c

cd $srcDir/trafficGeneratorServer/trafficGeneratorServer/trafficGeneratorServer
$CXX -std=gnu++11 -Wall  -g -pthread -o server main.cpp -I /usr/local/Cellar/boost/1.56.0/include


cd $srcDir/trafficGenerator/trafficGenerator
$CXX -pthread -g -Wall -std=gnu++11 -I../../threadpool/boost/ main.cpp utils.cpp -lboost_program_options -lboost_system -o traffGen -I /usr/local/Cellar/boost/1.56.0/include -L/usr/local/Cellar/boost/1.56.0/lib
