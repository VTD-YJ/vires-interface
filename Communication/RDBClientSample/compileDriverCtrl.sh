#!/bin/tcsh

# compile the RDB client example

g++ -o sampleClientRDBDriverCtrl ../Common/RDBHandler.cc ExampleConsoleDriverCtrl.cpp -I../Common/
