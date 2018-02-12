// ExampleConsole.cpp : Defines the entry point for the console application.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <ctype.h>
#include "scpIcd.h"

#define DEFAULT_PORT        48179
#define DEFAULT_BUFFER      204800

char  szServer[128];             // Server to connect to
int   iPort     = DEFAULT_PORT;  // Port on server to connect to

void sendSCPMessage( int sClient, const char* text );

//
// Function: usage:
//
// Description:
//    Print usage information and exit
//
void usage()
{
    printf("usage: client [-p:x] [-s:IP]\n\n");
    printf("       -p:x      Remote port to send to\n");
    printf("       -s:IP     Server's IP address or hostname\n");
    exit(1);
}

//
// Function: ValidateArgs
//
// Description:
//    Parse the command line arguments, and set some global flags
//    to indicate what actions to perform
//
void ValidateArgs(int argc, char **argv)
{
    int i;
    
    strcpy( szServer, "127.0.0.1" );
 
    for(i = 1; i < argc; i++)
    {
        if ((argv[i][0] == '-') || (argv[i][0] == '/'))
        {
            switch (tolower(argv[i][1]))
            {
                case 'p':        // Remote port
                    if (strlen(argv[i]) > 3)
                        iPort = atoi(&argv[i][3]);
                    break;
                case 's':       // Server
                    if (strlen(argv[i]) > 3)
                        strcpy(szServer, &argv[i][3]);
                    break;
                default:
                    usage();
                    break;
            }
        }
    }
}


int main(int argc, char* argv[])
{
    int sClient;
    char* szBuffer = new char[DEFAULT_BUFFER];  // allocate on heap
    int ret;

    struct sockaddr_in server;
    struct hostent    *host = NULL;

    static bool sVerbose = true;

    // Parse the command line
    //
    ValidateArgs(argc, argv);
    
    //
    // Create the socket, and attempt to connect to the server
    //
    sClient = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if ( sClient == -1 )
    {
        printf( "socket() failed: %s\n", strerror( errno ) );
        return 1;
    }
    
    int opt = 1;
    setsockopt ( sClient, IPPROTO_TCP, TCP_NODELAY, &opt, sizeof( opt ) );

    server.sin_family = AF_INET;
    server.sin_port = htons(iPort);
    server.sin_addr.s_addr = inet_addr(szServer);
    
    //
    // If the supplied server address wasn't in the form
    // "aaa.bbb.ccc.ddd" it's a hostname, so try to resolve it
    //
    if (server.sin_addr.s_addr == INADDR_NONE)
    {
        host = gethostbyname(szServer);
        if (host == NULL)
        {
            printf("Unable to resolve server: %s\n", szServer);
            return 1;
        }
        memcpy(&server.sin_addr, host->h_addr_list[0], host->h_length);
    }
	// wait for connection
	bool bConnected = false;

    while ( !bConnected )
    {
        if (connect(sClient, (struct sockaddr *)&server, sizeof(server)) == -1)
        {
            printf( "connect() failed: %s\n", strerror( errno ) );
            sleep( 1 );
        }
        else
            bConnected = true;
    }

    printf( "connected!\n" );
    
    // Send and receive data
    //
    unsigned int  bytesInBuffer = 0;
    size_t        bufferSize    = sizeof( SCP_MSG_HDR_t );
    size_t        headerSize    = sizeof( SCP_MSG_HDR_t );
    unsigned char *pData        = ( unsigned char* ) calloc( 1, bufferSize );
    unsigned int  count         = 0;
    
    for(;;)
    {
        bool bMsgComplete = false;

        // read a complete message
        while ( !bMsgComplete )
        {
            ret = recv(sClient, szBuffer, DEFAULT_BUFFER, 0);

            if (ret == -1)
            {
                printf( "recv() failed: %s\n", strerror( errno ) );
                break;
            }

            if ( ret != 0 )
            {
                // do we have to grow the buffer??
                if ( ( bytesInBuffer + ret ) > bufferSize )
                    pData = ( unsigned char* ) realloc( pData, bytesInBuffer + ret );

                memcpy( pData + bytesInBuffer, szBuffer, ret );
                bytesInBuffer += ret;

                // already complete messagae?
                if ( bytesInBuffer >= headerSize )
                {
                    SCP_MSG_HDR_t* hdr = ( SCP_MSG_HDR_t* ) pData;

                    // is this message containing the valid magic number?
                    if ( hdr->magicNo != SCP_MAGIC_NO )
                    {
                        printf( "message receiving is out of sync; discarding data" );
                        bytesInBuffer = 0;
                    }

                    // Do I have a complete message_
                    while ( bytesInBuffer >= ( headerSize + hdr->dataSize ) )
                    {
                        unsigned int msgSize = headerSize + hdr->dataSize;
                        
                        fprintf( stderr, "received SCP message with %d characters\n", hdr->dataSize );
                        
                        char* pText = ( ( char* ) pData ) + headerSize;
                        
                        // copy the string into a new buffer and null-terminate it
                        
                        char* scpText = new char[ hdr->dataSize + 1 ];
                        memset( scpText, 0, hdr->dataSize + 1 );
                        memcpy( scpText, pText, hdr->dataSize );
                        
                        // print the message
                        if ( sVerbose )
                            fprintf( stderr, "message: ---%s---\n", scpText );

                        // remove message from queue
                        memmove( pData, pData + msgSize, bytesInBuffer - msgSize );
                        bytesInBuffer -= msgSize;

                        bMsgComplete = true;
                    }
                }
            }
        }
        // all right I got a message
        count++;
        
        // send some response once in a while
        if ( !( count % 5 ) )
            sendSCPMessage( sClient, "<Info><Message text=\"ExampleConsole is alive\"/></Info>" );




    }
    ::close(sClient);

    return 0;
}

void sendSCPMessage( int sClient, const char* text )
{
    if ( !text )
        return;
        
    if ( !strlen( text ) )
        return;
    
    size_t         totalSize = sizeof( SCP_MSG_HDR_t ) + strlen( text );
    SCP_MSG_HDR_t* msg       = ( SCP_MSG_HDR_t* ) new char[ totalSize ];
    
    // target pointer for actual SCP message data
    char* pData = ( char* ) msg;
    pData += sizeof( SCP_MSG_HDR_t );
    
    // fill the header information
    msg->magicNo  = SCP_MAGIC_NO;
    msg->version  = SCP_VERSION;
    msg->dataSize = strlen( text );
    sprintf( msg->sender, "ExampleConsole" );
    sprintf( msg->receiver, "any" );
    
    // fill the actual data section (no trailing \0 required)
    
    memcpy( pData, text, strlen( text ) );

    int retVal = ::send( sClient, msg, totalSize, 0 );

    if ( !retVal )
        fprintf( stderr, "sendSCPMessage: could not send message.\n" );
        
    fprintf( stderr, "sendSCPMessage: sent %d characters in a message of %d bytes: ***%s***\n", 
                      msg->dataSize, totalSize, text );
        
    // free allocated buffer space
    delete msg;
}


