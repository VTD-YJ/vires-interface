#!/bin/tcsh

# compile the RDB client example

g++ -o sampleVehDynRDB ../Common/RDBHandler.cc ExampleVehDynInteg.cpp -I../Common
