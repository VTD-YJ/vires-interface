#!/bin/bash

# compile the RDB shm reader and writer examples

echo "compiling shmReader..."
g++ -o shmReader ../Common/RDBHandler.cc ShmReader.cpp -I../Common/
echo "...done"

echo "compiling shmWriter..."
g++ -o shmWriter ../Common/RDBHandler.cc ShmWriter.cpp -I../Common/
echo "...done"

echo "compiling shmWriterExtNewSyncDualIG..."
g++ -o shmWriterExtNewSyncDualIG -I../Common ../Common/RDBHandler.cc ShmWriterExtNewSyncDualIG.cpp
echo "...done"

