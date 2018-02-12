#!/bin/tcsh

# compile the RDB shm reader and writer examples

echo "compiling the demo applicaton..."
g++  -o driverCtrlTest ../Common/RDBHandler.cc DriverCtrlViaShm.cpp -I../Common
echo "...done"

