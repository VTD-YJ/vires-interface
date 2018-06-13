//
// Created by veikas on 13.06.18.
//

#include <netinet/in.h>
#include <cstdio>
#include <cerrno>
#include <cstring>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <zconf.h>

namespace Framework {
    class Utils {

    public:
        static int openNetwork(int iPort, char *szServer)
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

    };
}