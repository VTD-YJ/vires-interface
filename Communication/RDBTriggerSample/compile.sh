#!/bin/tcsh

# compile the RDB client example

g++ -o triggerGenerator ../Common/RDBHandler.cc TriggerSample.cc -I../Common
