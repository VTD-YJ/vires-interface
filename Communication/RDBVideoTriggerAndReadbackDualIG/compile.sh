#!/bin/tcsh

# compile the example

echo "compiling shmReader..."
g++ -o videoTest ../Common/RDBHandler.cc TriggerAndReadback.cpp -I../Common
echo "...done"
