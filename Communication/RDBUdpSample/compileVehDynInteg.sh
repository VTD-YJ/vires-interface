#!/bin/tcsh

# compile the RDB client example

g++ -o rdbTest ../Common/RDBHandler.cc ExampleVehDynInteg.cpp -I../Common
