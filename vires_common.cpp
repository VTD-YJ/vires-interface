
#include <sys/shm.h>
#include <unistd.h>
#include <iostream>
#include <netinet/tcp.h>
#include <netdb.h>
#include <string.h>
#include "vires_common.h"


/*
 * Transport endpoint is already connected
 */


namespace Framework {
    void ViresInterface::handleMessage( RDB_MSG_t* msg )
    {
        bool csv = false;
        bool csvHeader = false;
        bool details = false;
        static uint32_t prevFrame=65535;
        if ( !msg )
        {
            fprintf( stderr, "RDBHandler::printMessage: no message available\n" );
            return;
        }

        if ( msg->hdr.frameNo == prevFrame ) {
            return;
        }

        if ( !csv && !csvHeader )
        {
            fprintf( stderr, "\nRDBHandler::printMessage: ---- %s ----- BEGIN\n", details ? "full info" : "short info" );
            fprintf( stderr, "  message: version = 0x%04x, simTime = %.3f, simFrame = %d, headerSize = %d, dataSize = %d\n",
                     msg->hdr.version, msg->hdr.simTime, msg->hdr.frameNo, msg->hdr.headerSize, msg->hdr.dataSize );
        }
        prevFrame = msg->hdr.frameNo;
        parseRDBMessage( msg );
    }


    int ViresInterface::openNetwork(int iPort)
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

    void ViresInterface::readScpNetwork (int sClient ) {

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

    void ViresInterface::sendSCPMessage( int sClient, const char* text )
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



    void ViresInterface::readNetwork(int mClient)
    {
        static char*         szBuffer       = 0;
        int                  ret            = 0;
        static unsigned int  sBytesInBuffer = 0;
        static size_t        sBufferSize    = sizeof( RDB_MSG_HDR_t );
        static unsigned char *spData        = ( unsigned char* ) calloc( 1, sBufferSize );
        static int           sVerbose       = 0;

        if ( !szBuffer )
            szBuffer = new char[DEFAULT_BUFFER];  // allocate on heap

        // make sure this is non-bloekcing and read everything that's available!
        do
        {
            ret = 0;        // nothing read yet

            int noReady = getNoReadyRead( mClient );

            if ( noReady < 0 )
            {
                fprintf( stderr, "recv() failed: %s\n", strerror( errno ) );
                break;
            }

            if ( noReady > 0 )
            {
                // read data
                if ( ( ret = recv( mClient, szBuffer, DEFAULT_BUFFER, 0 ) ) != 0 )
                {
                    // do we have to grow the buffer??
                    if ( ( sBytesInBuffer + ret ) > sBufferSize )
                    {
                        spData      = ( unsigned char* ) realloc( spData, sBytesInBuffer + ret );
                        sBufferSize = sBytesInBuffer + ret;
                    }

                    memcpy( spData + sBytesInBuffer, szBuffer, ret );
                    sBytesInBuffer += ret;

                    // already complete messagae?
                    if ( sBytesInBuffer >= sizeof( RDB_MSG_HDR_t ) )
                    {
                        RDB_MSG_HDR_t* hdr = ( RDB_MSG_HDR_t* ) spData;

                        // is this message containing the valid magic number?
                        if ( hdr->magicNo != RDB_MAGIC_NO )
                        {
                            fprintf( stderr, "message receiving is out of sync; discarding data" );
                            sBytesInBuffer = 0;
                        }
                        else
                        {
                            // handle all messages in the buffer before proceeding
                            while ( sBytesInBuffer >= ( hdr->headerSize + hdr->dataSize ) )
                            {
                                unsigned int msgSize = hdr->headerSize + hdr->dataSize;

                                // print the message
                                if ( sVerbose )
                                    Framework::RDBHandler::printMessage( ( RDB_MSG_t* ) spData, true );

                                // now parse the message
                                parseRDBMessage( ( RDB_MSG_t* ) spData );

                                // remove message from queue
                                memmove( spData, spData + msgSize, sBytesInBuffer - msgSize );
                                sBytesInBuffer -= msgSize;
                            }
                        }
                    }
                }
            }
        }
        while ( ret > 0 );
    }


/**
* open the shared memory segment
*/

    void ViresInterface::openShm(unsigned int shmKey)
    {
        // do not open twice!
        if ( mShmPtr )
            return;

        int shmid = 0;

        if ( ( shmid = shmget( shmKey, 0, 0 ) ) < 0 )
            return;

        if ( ( mShmPtr = (char *)shmat( shmid, (char *)0, 0 ) ) == (char *) -1 )
        {
            perror("openShm: shmat()");
            mShmPtr = 0;
        }

        if ( mShmPtr )
        {
            struct shmid_ds sInfo;

            if ( ( shmid = shmctl( shmid, IPC_STAT, &sInfo ) ) < 0 )
                perror( "openShm: shmctl()" );
            else
                mShmTotalSize = sInfo.shm_segsz;
        }
    }

    int ViresInterface::checkShm()
    {
        if ( !mShmPtr )
            return 0;

        // get a pointer to the shm info block
        RDB_SHM_HDR_t* shmHdr = ( RDB_SHM_HDR_t* ) ( mShmPtr );

        if ( !shmHdr )
            return 0;

        if ( ( shmHdr->noBuffers != 2 ) )
        {
            fprintf( stderr, "checkShm: no or wrong number of buffers in shared memory. I need two buffers!" );
            return 0;
        }

        // allocate space for the buffer infos
        RDB_SHM_BUFFER_INFO_t** pBufferInfo = ( RDB_SHM_BUFFER_INFO_t** ) ( new char [ shmHdr->noBuffers * sizeof( RDB_SHM_BUFFER_INFO_t* ) ] );
        RDB_SHM_BUFFER_INFO_t*  pCurrentBufferInfo = 0;

        char* dataPtr = ( char* ) shmHdr;
        dataPtr += shmHdr->headerSize;

        for ( int i = 0; i < shmHdr->noBuffers; i++ )
        {
            pBufferInfo[ i ] = ( RDB_SHM_BUFFER_INFO_t* ) dataPtr;
            dataPtr += pBufferInfo[ i ]->thisSize;
        }

        // get the pointers to message section in each buffer
        RDB_MSG_t* pRdbMsgA = ( RDB_MSG_t* ) ( ( ( char* ) mShmPtr ) + pBufferInfo[0]->offset );
        RDB_MSG_t* pRdbMsgB = ( RDB_MSG_t* ) ( ( ( char* ) mShmPtr ) + pBufferInfo[1]->offset );

        // pointer to the message that will actually be read
        RDB_MSG_t* pRdbMsg  = 0;

        // remember the flags that are set for each buffer
        unsigned int flagsA = pBufferInfo[ 0 ]->flags;
        unsigned int flagsB = pBufferInfo[ 1 ]->flags;

        // check whether any buffer is ready for reading (checkMask is set (or 0) and buffer is NOT locked)
        bool readyForReadA = ( ( flagsA & mCheckMask ) || !mCheckMask ) && !( flagsA & RDB_SHM_BUFFER_FLAG_LOCK );
        bool readyForReadB = ( ( flagsB & mCheckMask ) || !mCheckMask ) && !( flagsB & RDB_SHM_BUFFER_FLAG_LOCK );

        if ( mVerbose )
        {
            fprintf( stderr, "ImageReader::checkShm: before processing SHM\n" );
            fprintf( stderr, "ImageReader::checkShm: Buffer A: frameNo = %06d, flags = 0x%x, locked = <%s>, lock mask set = <%s>, readyForRead = <%s>\n",
                     pRdbMsgA->hdr.frameNo,
                     flagsA,
                     ( flagsA & RDB_SHM_BUFFER_FLAG_LOCK ) ? "true" : "false",
                     ( flagsA & mCheckMask ) ? "true" : "false",
                     readyForReadA ?  "true" : "false" );

            fprintf( stderr, "                       Buffer B: frameNo = %06d, flags = 0x%x, locked = <%s>, lock mask set = <%s>, readyForRead = <%s>\n",
                     pRdbMsgB->hdr.frameNo,
                     flagsB,
                     ( flagsB & RDB_SHM_BUFFER_FLAG_LOCK ) ? "true" : "false",
                     ( flagsB & mCheckMask ) ? "true" : "false",
                     readyForReadB ?  "true" : "false" );
        }

        if ( mForceBuffer < 0 )  // auto-select the buffer if none is forced to be read
        {
            // check which buffer to read
            if ( ( readyForReadA ) && ( readyForReadB ) )
            {
                if ( pRdbMsgA->hdr.frameNo > pRdbMsgB->hdr.frameNo )        // force using the latest image!!
                {
                    pRdbMsg            = pRdbMsgA;
                    pCurrentBufferInfo = pBufferInfo[ 0 ];
                }
                else
                {
                    pRdbMsg            = pRdbMsgB;
                    pCurrentBufferInfo = pBufferInfo[ 1 ];
                }
            }
            else if ( readyForReadA )
            {
                pRdbMsg            = pRdbMsgA;
                pCurrentBufferInfo = pBufferInfo[ 0 ];
            }
            else if ( readyForReadB )
            {
                pRdbMsg            = pRdbMsgB;
                pCurrentBufferInfo = pBufferInfo[ 1 ];
            }
        }
        else if ( ( mForceBuffer == 0 ) && readyForReadA )   // force reading buffer A
        {
            pRdbMsg            = pRdbMsgA;
            pCurrentBufferInfo = pBufferInfo[ 0 ];
        }
        else if ( ( mForceBuffer == 1 ) && readyForReadB ) // force reading buffer B
        {
            pRdbMsg            = pRdbMsgB;
            pCurrentBufferInfo = pBufferInfo[ 1 ];
        }

        // lock the buffer that will be processed now (by this, no other process will alter the contents)
        if ( pCurrentBufferInfo )
            pCurrentBufferInfo->flags |= RDB_SHM_BUFFER_FLAG_LOCK;

        // no data available?
        if ( !pRdbMsg || !pCurrentBufferInfo )
        {
            delete pBufferInfo;
            pBufferInfo = 0;

            // return with valid result if simulation is not yet running
            if ( ( pRdbMsgA->hdr.frameNo == 0 ) && ( pRdbMsgB->hdr.frameNo == 0 ) )
                return 1;

            // otherwise return a failure
            return 0;
        }

        // handle all messages in the buffer
        if ( !pRdbMsg->hdr.dataSize )
        {
            fprintf( stderr, "checkShm: zero message data size, error.\n" );
            return 0;
        }

        // running the same frame again?
        if ( (( int ) pRdbMsg->hdr.frameNo ) > mLastShmFrame )
        {
            // remember the last frame that was read
            mLastShmFrame = pRdbMsg->hdr.frameNo;

            while ( 1 )
            {
                // handle the message that is contained in the buffer; this method should be provided by the user (i.e. YOU!)
                handleMessage( pRdbMsg );

                // go to the next message (if available); there may be more than one message in an SHM buffer!
                pRdbMsg = ( RDB_MSG_t* ) ( ( ( char* ) pRdbMsg ) + pRdbMsg->hdr.dataSize + pRdbMsg->hdr.headerSize );

                if ( !pRdbMsg )
                    break;

                if ( pRdbMsg->hdr.magicNo != RDB_MAGIC_NO )
                    break;
            }
        }

        //fprintf( stderr, "checkShm: mLastShmFrame = %ld\n", mLastShmFrame );

        // release after reading
        pCurrentBufferInfo->flags &= ~mCheckMask;                   // remove the check mask
        pCurrentBufferInfo->flags &= ~RDB_SHM_BUFFER_FLAG_LOCK;     // remove the lock mask

        if ( mVerbose )
        {
            unsigned int flagsA = pBufferInfo[ 0 ]->flags;
            unsigned int flagsB = pBufferInfo[ 1 ]->flags;

            fprintf( stderr, "ImageReader::checkShm: after processing SHM\n" );
            fprintf( stderr, "ImageReader::checkShm: Buffer A: frameNo = %06d, flags = 0x%x, locked = <%s>, lock mask set = <%s>\n",
                     pRdbMsgA->hdr.frameNo,
                     flagsA,
                     ( flagsA & RDB_SHM_BUFFER_FLAG_LOCK ) ? "true" : "false",
                     ( flagsA & mCheckMask ) ? "true" : "false" );
            fprintf( stderr, "                       Buffer B: frameNo = %06d, flags = 0x%x, locked = <%s>, lock mask set = <%s>.\n",
                     pRdbMsgB->hdr.frameNo,
                     flagsB,
                     ( flagsB & RDB_SHM_BUFFER_FLAG_LOCK ) ? "true" : "false",
                     ( flagsB & mCheckMask ) ? "true" : "false" );
        }

        return 1;
    }

    /*
    ViresInterface::checkForShmData() {
        bool
        IfaceIG::checkForShmData()
        {
            // attach to shared memory first
            ....

            // get a pointer to the shm info block located at the very beginning of the SHM
            RDB_SHM_HDR_t* shmHdr = ( RDB_SHM_HDR_t* ) ( mImgShm->getStart() );

            if ( !shmHdr )
                return false;

            // expecting double-buffered SHM!
            if ( ( shmHdr->noBuffers != 2 ) )
                return false;

            // allocate space for the buffer infos
            RDB_SHM_BUFFER_INFO_t** pBufferInfo = ( RDB_SHM_BUFFER_INFO_t** ) ( new char [ shmHdr->noBuffers * sizeof( RDB_SHM_BUFFER_INFO_t* ) ] );
            RDB_SHM_BUFFER_INFO_t* pCurrentBufferInfo = 0;

            char* dataPtr = ( char* ) shmHdr;
            dataPtr += shmHdr->headerSize;

            for ( int i = 0; i < shmHdr->noBuffers; i++ )
            {
                pBufferInfo[ i ] = ( RDB_SHM_BUFFER_INFO_t* ) dataPtr;
                dataPtr += pBufferInfo[ i ]->thisSize;
            }

            // check which buffer to read
            if ( ( pBufferInfo[ 0 ]->flags & RDB_SHM_BUFFER_FLAG_TC ) && ( pBufferInfo[ 1 ]->flags & RDB_SHM_BUFFER_FLAG_TC ) )
            {
                ...
                // lock while reading
                pCurrentBufferInfo->flags |= RDB_SHM_BUFFER_FLAG_LOCK;
            }
            else if ( ( pBufferInfo[ 0 ]->flags & RDB_SHM_BUFFER_FLAG_TC ) )
            {
                ...
                // lock while reading
                pCurrentBufferInfo->flags |= RDB_SHM_BUFFER_FLAG_LOCK;
            }
            else if ( ( pBufferInfo[ 1 ]->flags & RDB_SHM_BUFFER_FLAG_TC ) )
            {
                ...
                // lock while reading
                pCurrentBufferInfo->flags |= RDB_SHM_BUFFER_FLAG_LOCK;
            }
            else
            {
                delete pBufferInfo;
                return false;
            }

            //process the actual data
            .......

            // release after reading
            pCurrentBufferInfo->flags &= ~RDB_SHM_BUFFER_FLAG_TC;  // this is for TC, 3rd party components should clear their own mask here
            pCurrentBufferInfo->flags &= ~RDB_SHM_BUFFER_FLAG_LOCK;

            delete pBufferInfo;
            return true;
        }
    }
*/
    void ViresInterface::parseRDBMessage( RDB_MSG_t* msg )
    {
        parseMessage(msg);
    }


    int ViresInterface::getNoReadyRead( int descriptor )
    {
        fd_set         fs;
        struct timeval time;

        // still no valid descriptor?
        if ( descriptor < 0 )
            return 0;

        FD_ZERO( &fs );
        FD_SET( descriptor, &fs );

        memset( &time, 0, sizeof( struct timeval ) );

        int retVal = 0;

        retVal = select( descriptor + 1, &fs, (fd_set*) 0, (fd_set*) 0, &time);

        if ( retVal > 0 )
            return FD_ISSET( descriptor, &fs );

        return retVal;
    }

    void ViresInterface::sendRDBTrigger( int & sendSocket, const double & simTime, const unsigned int & simFrame, bool
    requestImage, double deltaTime  ) {

        int mClient = sendSocket;
        if ( mClient < 0 )
            return;

        Framework::RDBHandler myHandler;

        // start a new message
        myHandler.initMsg();

        // add extended package for the object state
        RDB_TRIGGER_t *trigger = ( RDB_TRIGGER_t* ) myHandler.addPackage( simTime, simFrame, RDB_PKG_ID_TRIGGER, 1,
                                                                          true );

        if ( !trigger )
        {
            fprintf( stderr, "sendRDBTrigger: could not create trigger\n" );
            return;
        }

        trigger->frameNo = simFrame;
        trigger->deltaT  = deltaTime;
        trigger->features = 0x1 | ( requestImage ? 0x2 : 0x0 );

        fprintf( stderr, "sendRDBTrigger: sending trigger, deltaT = %.4lf, requestImage = %s\n", trigger->deltaT, requestImage ? "true" : "false" );

        int retVal = send( mClient, ( const char* ) ( myHandler.getMsg() ), myHandler.getMsgTotalSize(), 0 );

        if ( !retVal )
            fprintf( stderr, "sendRDBTrigger: could not send trigger\n" );

        // increase internal counters ( this is currently done by the application
        //mSimTime += mDeltaTime;
        //mSimFrame++;
    }

    void ViresInterface::parseStartOfFrame(const double &simTime, const unsigned int &simFrame) {
        fprintf( stderr, "RDBHandler::parseStartOfFrame: simTime = %.3f, simFrame = %d\n", simTime, simFrame );
    }

    void ViresInterface::parseEndOfFrame( const double & simTime, const unsigned int & simFrame )
    {
        fprintf( stderr, "RDBHandler::parseEndOfFrame: simTime = %.3f, simFrame = %d\n", simTime, simFrame );
    }

    void ViresInterface::parseEntry( RDB_OBJECT_CFG_t *data, const double & simTime, const unsigned int & simFrame, const
    unsigned short & pkgId, const unsigned short & flags, const unsigned int & elemId, const unsigned int & totalElem )
    {
        RDB_OBJECT_CFG_t* object = reinterpret_cast<RDB_OBJECT_CFG_t*>(data); /// raw image data
        std::cout << object->type;
    }

    void ViresInterface::parseEntry( RDB_OBJECT_STATE_t *data, const double & simTime, const unsigned int & simFrame, const
    unsigned short & pkgId, const unsigned short & flags, const unsigned int & elemId, const unsigned int & totalElem
    ) {
        RDB_OBJECT_STATE_t* object = reinterpret_cast<RDB_OBJECT_STATE_t*>(data); /// raw image data
    }


    void ViresInterface::parseEntry( RDB_IMAGE_t *data, const double & simTime, const unsigned int & simFrame, const
    unsigned short & pkgId, const unsigned short & flags, const unsigned int & elemId, const unsigned int & totalElem ) {
        if ( !data )
            return;
        fprintf( stderr, "handleRDBitem: image\n" );
        fprintf( stderr, "    simTime = %.3lf, simFrame = %d, mLastShmFrame = %d\n", simTime, simFrame, mLastShmFrame );
        fprintf( stderr, "    width / height = %d / %d\n", data->width, data->height );
        fprintf( stderr, "    dataSize = %d\n", data->imgSize );

        // ok, I have an image: the application sets this through a overriden function
        // mHaveImage = 1;
        char* image_data_=NULL;
        RDB_IMAGE_t* image = reinterpret_cast<RDB_IMAGE_t*>(data); /// raw image data

        /// RDB image information of \see image_data_
        RDB_IMAGE_t image_info_;
        memcpy(&image_info_, image, sizeof(RDB_IMAGE_t));

        if (NULL == image_data_) {
            image_data_ = reinterpret_cast<char*>(malloc(image_info_.imgSize));
        } else {
            image_data_ = reinterpret_cast<char*>(realloc(image_data_, image_info_.imgSize));
        }
        // jump data header
        memcpy(image_data_, reinterpret_cast<char*>(image) + sizeof(RDB_IMAGE_t), image_info_.imgSize);
    }

}


