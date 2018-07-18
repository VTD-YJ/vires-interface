cmake_minimum_required(VERSION 3.8)
project(vires_interface)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_VERBOSE_MAKEFILE ON)
set(CMAKE_BUILD_TYPE Debug)

set(CMAKE_INSTALL_PREFIX ${CMAKE_SOURCE_DIR}/../../libs-install/libs/vires-interface-install)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC")
find_package(Boost EXACT 1.64 COMPONENTS filesystem REQUIRED)
find_package(Qt5 EXACT 5.5.1 COMPONENTS Core Gui REQUIRED)

message("Qt5 ${Qt5_FOUND} ${Qt5Core_INCLUDE_DIRS} ${Qt5Gui_INCLUDE_DIRS} ${Qt5Core_LIBS}")

set(SOURCE_FILES
        # I am using vires RDB implementation
        Communication/Common/RDBHandler.cc
        ViresInterface.cpp ViresConfiguration.h ViresConfiguration.cpp)

# I am not using bosch rdb implementation because there is no substatial difference between vires and bosch.
#bosch_adsim/src/rdb_camera_sensor.cpp
#bosch_adsim/src/rdb_codec.cpp
#bosch_adsim/src/rdb_transceiver_shared_memory.cpp
#bosch_adsim/src/rdb_transceiver_tcp.cpp
#bosch_adsim/src/scp_transceiver.cpp

include_directories("${CMAKE_SOURCE_DIR}/Communication")
include_directories(${QT5_INCLUDE_DIR})
include_directories(${Qt5Gui_INCLUDE_DIRS})

add_library(viresrdb SHARED ${SOURCE_FILES})
#install(TARGETS viresrdb DESTINATION "lib")

#install(FILES "vires/RDBHandler.hh" DESTINATION "include/vires/vires")
install(FILES "ViresInterface.h" DESTINATION "include/vires-interface")
install(FILES "ViresConfiguration.h" DESTINATION "include/vires-interface")

set_target_properties(viresrdb PROPERTIES PUBLIC_HEADER "Communication/Common/scpIcd.h;Communication/Common/viRDBIcd.h;Communication/Common/RDBHandler.hh")

install(TARGETS viresrdb
        LIBRARY DESTINATION lib
        PUBLIC_HEADER DESTINATION include/vires-interface/Common
        )


#echo "compiling shmReader..."
#g++ -o shmReader ../Common/RDBHandler.cc ShmReader.cpp -I../Common/
#echo "...done"

#echo "compiling shmWriter..."
#g++ -o shmWriter ../Common/RDBHandler.cc ShmWriter.cpp -I../Common/
#echo "...done"

#echo "compiling shmWriterExtNewSyncDualIG..."
#g++ -o shmWriterExtNewSyncDualIG -I../Common ../Common/RDBHandler.cc ShmWriterExtNewSyncDualIG.cpp
#echo "...done"

