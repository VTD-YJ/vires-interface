
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
        bool csv = true;
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


    void ViresInterface::readNetwork(int mClient)
    {
        static char*         szBuffer       = 0;
        int                  ret            = 0;
        static unsigned int  sBytesInBuffer = 0;
        static size_t        sBufferSize    = sizeof( RDB_MSG_HDR_t );
        static unsigned char *spData        = ( unsigned char* ) calloc( 1, sBufferSize );
        static bool           sVerbose       = false;

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
                                    Framework::RDBHandler::printMessage( ( RDB_MSG_t* ) spData, false);

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


    int ViresInterface::checkShm(const void *mShmPtr)
    {
        if ( !mShmPtr )
            return 0;

        const unsigned int mCheckMask    = RDB_SHM_BUFFER_FLAG_TC;
        const int          mForceBuffer  = -1;                                // force reading one of the SHM buffers (0=A, 1=B)


        // get a pointer to the shm info block
        const RDB_SHM_HDR_t* shmHdr = ( RDB_SHM_HDR_t* ) ( mShmPtr );

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

        const char* dataPtr = ( char* ) shmHdr;
        dataPtr += shmHdr->headerSize;

        for ( int i = 0; i < shmHdr->noBuffers; i++ )
        {
            pBufferInfo[ i ] = ( RDB_SHM_BUFFER_INFO_t* ) dataPtr;
            dataPtr += pBufferInfo[ i ]->thisSize;
        }

        // get the pointers to message section in each buffer
        const RDB_MSG_t* pRdbMsgA = ( RDB_MSG_t* ) ( ( ( char* ) mShmPtr ) + pBufferInfo[0]->offset );
        const RDB_MSG_t* pRdbMsgB = ( RDB_MSG_t* ) ( ( ( char* ) mShmPtr ) + pBufferInfo[1]->offset );

        // pointer to the message that will actually be read
        RDB_MSG_t* pRdbMsg  = 0;

        // remember the flags that are set for each buffer
        const unsigned int flagsA = pBufferInfo[ 0 ]->flags;
        const unsigned int flagsB = pBufferInfo[ 1 ]->flags;

        // check whether any buffer is ready for reading (checkMask is set (or 0) and buffer is NOT locked)
        const bool readyForReadA = ( ( flagsA & mCheckMask ) || !mCheckMask ) && !( flagsA & RDB_SHM_BUFFER_FLAG_LOCK );
        const bool readyForReadB = ( ( flagsB & mCheckMask ) || !mCheckMask ) && !( flagsB & RDB_SHM_BUFFER_FLAG_LOCK );

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
                    pRdbMsg            = (RDB_MSG_t *)pRdbMsgA;
                    pCurrentBufferInfo = pBufferInfo[ 0 ];
                }
                else
                {
                    pRdbMsg            = (RDB_MSG_t *)pRdbMsgB;
                    pCurrentBufferInfo = pBufferInfo[ 1 ];
                }
            }
            else if ( readyForReadA )
            {
                pRdbMsg            = (RDB_MSG_t *)pRdbMsgA;
                pCurrentBufferInfo = pBufferInfo[ 0 ];
            }
            else if ( readyForReadB )
            {
                pRdbMsg            = (RDB_MSG_t *)pRdbMsgB;
                pCurrentBufferInfo = pBufferInfo[ 1 ];
            }
        }
        else if ( ( mForceBuffer == 0 ) && readyForReadA )   // force reading buffer A
        {
            pRdbMsg            = (RDB_MSG_t *)pRdbMsgA;
            pCurrentBufferInfo = pBufferInfo[ 0 ];
        }
        else if ( ( mForceBuffer == 1 ) && readyForReadB ) // force reading buffer B
        {
            pRdbMsg            = (RDB_MSG_t *)pRdbMsgB;
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
        //if ( (( int ) pRdbMsg->hdr.frameNo ) > mLastShmFrame )
        {
            // remember the last frame that was read
            //mLastShmFrame = pRdbMsg->hdr.frameNo;

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
        //fprintf( stderr, "    simTime = %.3lf, simFrame = %d, mLastShmFrame = %d\n", simTime, simFrame, mLastShmFrame );
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

    void ViresInterface::sendOwnObjectState( RDB_OBJECT_STATE_t sOwnObjectState, int & sendSocket, const double & simTime, const unsigned int & simFrame )
    {
        Framework::RDBHandler myHandler;

        // start a new message
        myHandler.initMsg();

        // begin with an SOF identifier
        myHandler.addPackage( simTime, simFrame, RDB_PKG_ID_START_OF_FRAME );

        // add extended package for the object state
        RDB_OBJECT_STATE_t *objState = ( RDB_OBJECT_STATE_t* ) myHandler.addPackage( simTime, simFrame, RDB_PKG_ID_OBJECT_STATE, 1, true );

        if ( !objState )
        {
            fprintf( stderr, "sendOwnObjectState: could not create object state\n" );
            return;
        }

        // copy contents of internally held object state to output structure
        memcpy( objState, &sOwnObjectState, sizeof( RDB_OBJECT_STATE_t ) );

        fprintf( stderr, "sendOwnObjectState: sending pos x/y/z = %.3lf/%.3lf/%.3lf,\n", objState->base.pos.x, objState->base.pos.y, objState->base.pos.z );

        // terminate with an EOF identifier
        myHandler.addPackage( simTime, simFrame, RDB_PKG_ID_END_OF_FRAME );

        int retVal = send( sendSocket, ( const char* ) ( myHandler.getMsg() ), myHandler.getMsgTotalSize(), 0 );

        if ( !retVal )
            fprintf( stderr, "sendOwnObjectState: could not send object state\n" );
    }


}


