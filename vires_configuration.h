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
#include <cstdlib>

#define DEFAULT_BUFFER      204800


namespace Framework {

    class ViresConfiguration {

    private:
        char         szServer[128];                                     // Server to connect to
        bool         mVerbose      = false;                             // run in verbose mode?


    public:

        void setServer(const char *val) {
            strcpy(szServer, val );
        }

        void* openShm(unsigned int shmKey);

        int openNetwork(int iPort)
        {

            int mClient;
            struct sockaddr_in server;
            struct hostent    *host = NULL;

            // Create the socket, and attempt to connect to the server
            mClient = socket( AF_INET, SOCK_STREAM, IPPROTO_TCP );

            if ( mClient == -1 )
            {
                fprintf( stderr, "socket() failed: %s\n", strerror( errno ) );
                return mClient;
            }

            int opt = 1;
            setsockopt ( mClient, IPPROTO_TCP, TCP_NODELAY, &opt, sizeof( opt ) );

            server.sin_family      = AF_INET;
            server.sin_port        = htons(iPort);
            server.sin_addr.s_addr = inet_addr(szServer);

            // If the supplied server address wasn't in the form
            // "aaa.bbb.ccc.ddd" it's a hostname, so try to resolve it
            if ( server.sin_addr.s_addr == INADDR_NONE )
            {
                host = gethostbyname(szServer);
                if ( host == NULL )
                {
                    fprintf( stderr, "Unable to resolve server: %s\n", szServer );
                    return -1;
                }
                memcpy( &server.sin_addr, host->h_addr_list[0], host->h_length );
            }

            // wait for connection
            bool bConnected = false;
            ushort retries = 0;
            ushort MAX_RETRIES = 5;
            while ( !bConnected && retries < MAX_RETRIES)
            {
                if ( connect( mClient, (struct sockaddr *)&server, sizeof( server ) ) == -1 )
                {
                    fprintf( stderr, "%d connect() failed: %s\n", iPort, strerror( errno ) );
                    sleep( 1 );
                    retries++;
                }
                else
                    bConnected = true;
            }

            if (!bConnected)
                return -1;

            fprintf( stderr, "%d connected! with no error %s\n", iPort, strerror( errno ) );
            return mClient;
        }

        void readScpNetwork ( int sClient );

        void sendSCPMessage( int sClient, const char* text );

    };
}

#endif //VIRES_INTERFACE_VIRES_CONFIGURATION_H
