//
// Created by veikas on 15.06.18.
//

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
#include <Common/scpIcd.h>
#include <cstdlib>
#include "vires_configuration.h"

namespace Framework {

    void ViresConfiguration::readScpNetwork (int sClient ) {
        char* szBuffer = new char[DEFAULT_BUFFER];  // allocate on heap
        int ret;

        static bool sVerbose = true;

        // Send and receive data
        //
        unsigned int  bytesInBuffer = 0;
        size_t        bufferSize    = sizeof( SCP_MSG_HDR_t );
        size_t        headerSize    = sizeof( SCP_MSG_HDR_t );
        unsigned char *pData        = ( unsigned char* ) calloc( 1, bufferSize );
        unsigned int  count         = 0;

        bool bMsgComplete = false;

        // read a complete message
        while ( !bMsgComplete )
        {
            ret = static_cast<int>(recv(sClient, szBuffer, DEFAULT_BUFFER, 0));

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
                        unsigned int msgSize = static_cast<int>(headerSize + hdr->dataSize);

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

            // all right I got a message
            count++;

            // send some response once in a while
            if ( !( count % 5 ) )
                sendSCPMessage( sClient, "<Info><Message text=\"ExampleConsole is alive\"/></Info>" );
        }
    }

    void ViresConfiguration::sendSCPMessage( int sClient, const char* text )
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
        msg->dataSize = (int)strlen( text );
        sprintf( msg->sender, "ExampleConsole" );
        sprintf( msg->receiver, "any" );

        // fill the actual data section (no trailing \0 required)

        memcpy( pData, text, strlen( text ) );

        int retVal = static_cast<int>(send( sClient, msg, totalSize, 0 ));

        if ( !retVal )
            fprintf( stderr, "sendSCPMessage: could not send message.\n" );

        fprintf( stderr, "sendSCPMessage: sent %d characters in a message of %d bytes: ***%s***\n",
                 msg->dataSize, (int)totalSize, text );

        // free allocated buffer space
        delete msg;
    }

    /**
    * open the shared memory segment
    */

    void* ViresConfiguration::openShm(unsigned int shmKey)
    {

        int shmid = 0;
        void *mShmPtr;
        size_t       mShmTotalSize = 0;                                 // remember the total size of the SHM segment

        if ( ( shmid = shmget( shmKey, 0, 0 ) ) < 0 )
            return NULL;

        if ( ( mShmPtr = (char *)shmat( shmid, (char *)0, 0 ) ) == (char *) -1 )
        {
            perror("openShm: shmat()");
            mShmPtr = NULL;
        }

        if ( mShmPtr )
        {
            struct shmid_ds sInfo;

            if ( ( shmid = shmctl( shmid, IPC_STAT, &sInfo ) ) < 0 )
                perror( "openShm: shmctl()" );
            else
                mShmTotalSize = sInfo.shm_segsz;
        }

        return mShmPtr;

    }


}
