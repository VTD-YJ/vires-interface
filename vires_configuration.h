//
// Created by veikas on 13.06.18.
//

#ifndef VIRES_INTERFACE_VIRES_CONFIGURATION_H
#define VIRES_INTERFACE_VIRES_CONFIGURATION_H

#include <cstring>
#include <netinet/in.h>
#include <cstdio>
#include <cerrno>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <zconf.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include "Utils.h"

namespace Framework {
    class ViresConfiguration {

    private:
        char         szServer[128];                                     // Server to connect to
        bool         mVerbose      = false;                             // run in verbose mode?


    public:

        void setServer(const char *val) {
            strcpy(szServer, val );
        }

        int openNetwork(int iPort) {
            Utils::openNetwork(iPort, szServer);
        }



    };
}

#endif //VIRES_INTERFACE_VIRES_CONFIGURATION_H
