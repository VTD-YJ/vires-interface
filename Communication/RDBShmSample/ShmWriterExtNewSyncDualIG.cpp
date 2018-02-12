// ShmWriterExt.cpp : Sample implementation of a process sending
// triggers to IG every nth TC frame
// (c) 2017 by VIRES Simulationstechnologie GmbH
// Provided AS IS without any warranty!
//

#include <stdlib.h>
#include <stdio.h>
#include <sys/shm.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/time.h>
#include "RDBHandler.hh"

#define DEFAULT_PORT        48190 
#define DEFAULT_BUFFER      204800


// forward declarations of methods

/**
* open the network interface for sending trigger data
*/
void openNetwork();

/**
* make sure network data is being read
*/
void readNetwork();

/**
* open the shared memory segment for receiving IG images
*/
void* openIgOutShm( int key, size_t *size );

/**
* check and parse the contents of the IG images SHM
* @param index  index of the data source that is being used
*/
int checkIgOutShm( unsigned int index );

/**
* check for data that has to be read (otherwise socket will be clogged)
* @param descriptor socket descriptor
*/
int getNoReadyRead( int descriptor );

/**
* parse an RDB message (received via SHM or network)
* @param msg    pointer to message that shall be parsed
* @param index  index of the data source that is being used
*/
void parseRDBMessage( RDB_MSG_t* msg, unsigned int index );

/**
* parse an RDB message entry
* @param simTime    simulation time of the message
* @param simFrame   simulation frame of the message
* @param entryHdr   pointer to the entry header itself
* @param index  index of the data source that is being used
*/
void parseRDBMessageEntry( const double & simTime, const unsigned int & simFrame, RDB_MSG_ENTRY_HDR_t* entryHdr, unsigned int index );

/**
* routine for handling an RDB message; to be provided by user;
* here, only a printing of the message is performed
* @param msg    pointer to the message that is to be handled
* @param index  index of the data source that is being used
*/
void handleMessage( RDB_MSG_t* msg, unsigned int index );

/**
* analyze an RDB image;
* @param img    pointer to the image that is to be handled
* @param simFrame   simulation frame of the message
* @param index  index of the data source that is being used
*/
void analyzeImage( RDB_IMAGE_t* img, const unsigned int & simFrame, unsigned int index );

/**
* send a trigger to the taskControl via network socket
* @param sendSocket socket descriptor
* @param simTime    internal simulation time
* @param simFrame   internal simulation frame
* @param requestImage if true, an image will be rendered
*/
void sendRDBTrigger( int & sendSocket, const double & simTime, const unsigned int & simFrame, bool requestImage );

/**
* open all communication means
*/
void openCommunication();

/**
* get the current time
*/
double getTime();

/**
* calculate some statistics
*/
void calcStatistics();


/**
* some global variables, considered "members" of this example
*/
unsigned int mCheckMask        = RDB_SHM_BUFFER_FLAG_TC;
bool         mVerbose          = false;                             // run in verbose mode?
char         szServer[128];                                         // Server to connect to for RDB data
int          iPort             = DEFAULT_PORT;                      // Port on server to connect to
int          mClient           = -1;                                // client socket
unsigned int mSimFrame         = 0;                                 // simulation frame counter
double       mSimTime          = 0.0;                               // simulation time
double       mDeltaTime        = 0.002;                             // simulation step width
int          mLastShmFrame     = -1;
int          mLastNetworkFrame = -1;
int          mLastIGTriggerFrame = -1;
int          mLastImageId[2]    = { 0, 0 };
int          mTotalNoImages[2] = { 0, 0 };

// stuff for reading images
unsigned int mIgOutShmKey[2]    = { 0x0120a,0x0120b };                                  // key of the SHM segment
unsigned int mIgOutCheckMask[2] = { RDB_SHM_BUFFER_FLAG_TC, RDB_SHM_BUFFER_FLAG_TC};
void*        mIgOutShmPtr[2]    = { 0, 0 };                                             // pointer to the SHM segment
int          mIgOutForceBuffer  = -1;
size_t       mIgOutShmTotalSize[2] = { 0, 0 };                                          // remember the total size of the SHM segment
                                                                                        // the memory and message management
                                                                     
unsigned int mFrameNo           = 0;
unsigned int mFrameTime         = mDeltaTime; 
bool         mHaveImage[2]      = { false, false };                                                                   
bool         mHaveFirstImage[2] = { false, false };
bool         mHaveFirstFrame    = false;
bool         mCheckForImage[2]  = { false, false };

// total number of errors
unsigned int mTotalErrorCount = 0;

// some stuff for performance measurement
double       mStartTime = -1.0;

// image skip factor
unsigned int mImageSkipFactor = 25;

/**
* information about usage of the software
* this method will exit the program
*/
void usage()
{
    printf("usage: shmWriterExtNewSyncDualIG\n");
    exit(1);
}

/**
* validate the arguments given in the command line
*/
void ValidateArgs(int argc, char **argv)
{
    // initalize the server variable
    strcpy( szServer, "127.0.0.1" );

    for( int i = 1; i < argc; i++)
    {
        if ((argv[i][0] == '-') || (argv[i][0] == '/'))
        {
            switch (tolower(argv[i][1]))
            {
                case 'c':       // check mask
                    if ( strlen( argv[i] ) > 3 )
                        mCheckMask = atoi( &argv[i][3] );
                    break;
                    
                case 'v':       // verbose mode
                    mVerbose = true;
                    break;
                    
                case 'p':        // Remote port
                    if (strlen(argv[i]) > 3)
                        iPort = atoi(&argv[i][3]);
                    break;
                case 's':       // Server
                    if (strlen(argv[i]) > 3)
                        strcpy(szServer, &argv[i][3]);
                    break;

                case 'h':
                default:
                    usage();
                    break;
            }
        }
    }
}

/**
* main program with high frequency loop for checking the shared memory;
* does nothing else
*/

int main(int argc, char* argv[])
{
    // Parse the command line
    ValidateArgs(argc, argv);
    
    // open the communication ports
    openCommunication();
    
    // now check the SHM for the time being
    while ( 1 )
    {
        int lastSimFrame = mLastNetworkFrame;
        
        // first read network messages
        readNetwork();
        if( lastSimFrame < 0 )
	{
            checkIgOutShm( 0 );//empty IG buffer of spurious images
            checkIgOutShm( 1 );//empty IG buffer of spurious images
	}
		
        bool haveNewFrame = ( lastSimFrame != mLastNetworkFrame );
        
        // now read IG output
        if ( mCheckForImage[0] )
            fprintf( stderr, "main: checking for IG image of IG 0\n" );
            
        if ( mCheckForImage[1] )
            fprintf( stderr, "main: checking for IG image of IG 1\n" );
        
        while ( mCheckForImage[0] )
        {
            checkIgOutShm( 0 );
            
            mCheckForImage[0] = !mHaveImage[0];
            
            usleep( 10 );
            
            if ( !mCheckForImage[0] )
                fprintf( stderr, "main: got it!\n" );
        }
        
        while ( mCheckForImage[1] )
        {
            checkIgOutShm( 1 );
            
            mCheckForImage[1] = !mHaveImage[1];
            
            usleep( 10 );
            
            if ( !mCheckForImage[1] )
                fprintf( stderr, "main: got it!\n" );
        }
        
        if ( haveNewFrame )
        {
            //fprintf( stderr, "main: new simulation frame (%d) available, mLastIGTriggerFrame = %d\n", 
            //                 mLastNetworkFrame, mLastIGTriggerFrame );
                             
            mHaveFirstFrame = true;
        }
            
        // has an image arrived or do the first frames need to be triggered 
        //(first image will arrive with a certain frame delay only)
        if ( !mHaveFirstImage[0] || !mHaveFirstImage[1] || ( mHaveImage[0] && mHaveImage[1] ) || haveNewFrame || !mHaveFirstFrame )
        {
            // do not initialize too fast
            if ( !mHaveFirstImage[0] || !mHaveFirstImage[1] || !mHaveFirstFrame )
                usleep( 100000 );   // 10Hz
                
            bool requestImage = ( mLastNetworkFrame >= ( mLastIGTriggerFrame + mImageSkipFactor ) );

            if ( requestImage )
            {
                mLastIGTriggerFrame = mLastNetworkFrame;
                mCheckForImage[0] = true;
                mCheckForImage[1] = true;
            }

            sendRDBTrigger( mClient, mSimTime, mSimFrame, requestImage );

            // increase internal counters
            mSimTime += mDeltaTime;
            mSimFrame++;

             // calculate the timing statistics
	    if ( mHaveImage[0] || mHaveImage[1] )
	      calcStatistics();

            mHaveImage[0] = false;
            mHaveImage[1] = false;
        }
        
        usleep( 10 );       // do not overload the CPU
    }
}

void openCommunication()
{
    // open the network connection to the taskControl (so triggers may be sent)
    fprintf( stderr, "openCommunication: creating network connection....\n" );
    openNetwork();  // this is blocking until the network has been opened
    
    // open the shared memory for IG image output (try to attach without creating a new segment)
    fprintf( stderr, "openCommunication: attaching to shared memory (IG image output) 0x%x....\n", mIgOutShmKey[0] );

    while ( !mIgOutShmPtr[0] )
    {
        mIgOutShmPtr[0] = openIgOutShm( mIgOutShmKey[0], &( mIgOutShmTotalSize[0] ) );
        usleep( 1000 );     // do not overload the CPU
    }
    
    // open the shared memory for IG image output (try to attach without creating a new segment)
    fprintf( stderr, "openCommunication: attaching to shared memory (IG image output) 0x%x....\n", mIgOutShmKey[1] );

    while ( !mIgOutShmPtr[1] )
    {
        mIgOutShmPtr[1] = openIgOutShm( mIgOutShmKey[1], &( mIgOutShmTotalSize[1] ) );
        usleep( 1000 );     // do not overload the CPU
    }
    
    fprintf( stderr, "...attached to IG image output SHM! Triggering now...\n" );
}

void openNetwork()
{
    struct sockaddr_in server;
    struct hostent    *host = NULL;

    // Create the socket, and attempt to connect to the server
    mClient = socket( AF_INET, SOCK_STREAM, IPPROTO_TCP );
    
    if ( mClient == -1 )
    {
        fprintf( stderr, "socket() failed: %s\n", strerror( errno ) );
        return;
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
            return;
        }
        memcpy( &server.sin_addr, host->h_addr_list[0], host->h_length );
    }
    
	// wait for connection
	bool bConnected = false;

    while ( !bConnected )
    {
        if ( connect( mClient, (struct sockaddr *)&server, sizeof( server ) ) == -1 )
        {
            fprintf( stderr, "connect() failed: %s\n", strerror( errno ) );
            sleep( 1 );
        }
        else
            bConnected = true;
    }

    fprintf( stderr, "connected!\n" );
}

void readNetwork()
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
 
                            // now parse the message
                            parseRDBMessage( ( RDB_MSG_t* ) spData, 0 );

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

void handleMessage( RDB_MSG_t* msg, unsigned int index )
{
    // just print the message
    parseRDBMessage( msg, index );
}

void parseRDBMessage( RDB_MSG_t* msg, unsigned int index )
{
    if ( !msg )
      return;

    if ( !msg->hdr.dataSize )
        return;
    
    RDB_MSG_ENTRY_HDR_t* entry = ( RDB_MSG_ENTRY_HDR_t* ) ( ( ( char* ) msg ) + msg->hdr.headerSize );
    uint32_t remainingBytes    = msg->hdr.dataSize;
        
    while ( remainingBytes )
    {
        parseRDBMessageEntry( msg->hdr.simTime, msg->hdr.frameNo, entry, index );

        remainingBytes -= ( entry->headerSize + entry->dataSize );
        
        if ( remainingBytes )
          entry = ( RDB_MSG_ENTRY_HDR_t* ) ( ( ( ( char* ) entry ) + entry->headerSize + entry->dataSize ) );
    }
}

void parseRDBMessageEntry( const double & simTime, const unsigned int & simFrame, RDB_MSG_ENTRY_HDR_t* entryHdr, unsigned int index )
{
    if ( !entryHdr )
        return;
    
    if ( entryHdr->pkgId == RDB_PKG_ID_END_OF_FRAME )   // check for end-of-frame only
    {
        mLastNetworkFrame = simFrame;
//         return;
    }
    
    if ( entryHdr->pkgId == RDB_PKG_ID_IMAGE )   // check for end-of-frame only
        analyzeImage( ( RDB_IMAGE_t* )( ( ( char* ) entryHdr ) + entryHdr->headerSize ), simFrame, index );
}

void analyzeImage( RDB_IMAGE_t* img, const unsigned int & simFrame, unsigned int index )
{
    static unsigned int sLastImgSimFrame[2] = { 0, 0 }; 
    
    if ( !img || ( index > 1 ) )
        return;
    
    if ( img->id == 0 )
        return;
    
    fprintf( stderr, "analyzeImage: simframe = %d, index = %d: have image no. %d, size = %d bytes, pixelFormat = %d\n", 
	              simFrame, index, img->id, img->imgSize, img->pixelFormat );
    
    if ( img->pixelFormat == RDB_PIX_FORMAT_RGB32F )		// some analysis
    {
        float *imgData = ( float* ) ( ( ( char* ) img ) + sizeof( RDB_IMAGE_t ) );
	
        for ( int i = 0; i < 10; i++ )	// first 10 pixels
	{
	    fprintf( stderr, "r / g / b = %.3f / %.3f / %.3f\n", imgData[0], imgData[1], imgData[2] );
	    imgData += 3;
	}
    }
    else if ( img->pixelFormat == RDB_PIX_FORMAT_RGB8 )		// some analysis
    {
        unsigned char *imgData = ( unsigned char* ) ( ( ( char* ) img ) + sizeof( RDB_IMAGE_t ) );
	
        for ( int i = 0; i < 10; i++ )	// first 10 pixels
	{
	    fprintf( stderr, "r / g / b = %d / %d / %d\n", imgData[0], imgData[1], imgData[2] );
	    imgData += 3;
	}
    }
    
            
    //if ( ( myImg->id > 3 ) && ( ( myImg->id - mLastImageId ) != mImageSkipFactor ) )
    if ( ( simFrame != sLastImgSimFrame[index] ) && ( img->id > 3 ) && ( ( simFrame - sLastImgSimFrame[index] ) != mImageSkipFactor ) )
    {
        fprintf( stderr, "WARNING: parseRDBMessageEntry: index = %d, delta of image ID out of bounds: delta = %d\n", index, simFrame - sLastImgSimFrame[index] );
	mTotalErrorCount++;
    }

    mLastImageId[index]    = img->id;
    mHaveImage[index]      = true;
    mHaveFirstImage[index] = true;
    mTotalNoImages[index]++;
        
    sLastImgSimFrame[index] = simFrame;
}


int getNoReadyRead( int descriptor )
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

void sendRDBTrigger( int & sendSocket, const double & simTime, const unsigned int & simFrame, bool requestImage )
{
    // is the socket available?
    if ( mClient < 0 )
        return;
                
    Framework::RDBHandler myHandler;

    // start a new message
    myHandler.initMsg();

    // add extended package for the object state
    RDB_TRIGGER_t *trigger = ( RDB_TRIGGER_t* ) myHandler.addPackage( simTime, simFrame, RDB_PKG_ID_TRIGGER, 1, true );

    if ( !trigger )
    {
        fprintf( stderr, "sendRDBTrigger: could not create trigger\n" );
        return;
    }
        
    trigger->frameNo  = simFrame;
    trigger->deltaT   = mDeltaTime;
    trigger->features = 0x1 | ( requestImage ? 0x2 : 0x0 );
    
    //fprintf( stderr, "sendRDBTrigger: sending trigger, deltaT = %.4lf, requestImage = %s\n", trigger->deltaT, requestImage ? "true" : "false" );
        
    int retVal = send( mClient, ( const char* ) ( myHandler.getMsg() ), myHandler.getMsgTotalSize(), 0 );

    if ( !retVal )
        fprintf( stderr, "sendRDBTrigger: could not send trigger\n" );
}

/**
* open the shared memory segment
*/
void* openIgOutShm( int key, size_t *size )
{
    void *retPtr = 0;
    int  shmid   = 0; 
    
    if ( !size )
        return 0;

    if ( ( shmid = shmget( key, 0, 0 ) ) < 0 )
        return 0;

    if ( ( retPtr = (char *)shmat( shmid, (char *)0, 0 ) ) == (char *) -1 )
    {
        perror("openIgOutShm: shmat()");
        retPtr = 0;
    }

    if ( retPtr )
    {
        struct shmid_ds sInfo;

        if ( ( shmid = shmctl( shmid, IPC_STAT, &sInfo ) ) < 0 )
            perror( "openIgOutShm: shmctl()" );
        else
            *size = sInfo.shm_segsz;
    }
    
    return retPtr;
}

int checkIgOutShm( unsigned int index )
{
    void* shmPtr = mIgOutShmPtr[index];
    
    if ( !shmPtr )
        return 0;

    // get a pointer to the shm info block
    RDB_SHM_HDR_t* shmHdr = ( RDB_SHM_HDR_t* ) ( shmPtr );

    if ( !shmHdr )
        return 0;

    if ( ( shmHdr->noBuffers != 2 ) )
    {
        fprintf( stderr, "checkIgOutShm: no or wrong number of buffers in shared memory. I need two buffers!" );
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
    RDB_MSG_t* pRdbMsgA = ( RDB_MSG_t* ) ( ( ( char* ) shmPtr ) + pBufferInfo[0]->offset );
    RDB_MSG_t* pRdbMsgB = ( RDB_MSG_t* ) ( ( ( char* ) shmPtr ) + pBufferInfo[1]->offset );
    
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
        fprintf( stderr, "ImageReader::checkIgOutShm: before processing SHM\n" );
        fprintf( stderr, "ImageReader::checkIgOutShm: Buffer A: frameNo = %06d, flags = 0x%x, locked = <%s>, lock mask set = <%s>, readyForRead = <%s>\n", 
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

    if ( mIgOutForceBuffer < 0 )  // auto-select the buffer if none is forced to be read
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
    else if ( ( mIgOutForceBuffer == 0 ) && readyForReadA )   // force reading buffer A
    {
        pRdbMsg            = pRdbMsgA; 
        pCurrentBufferInfo = pBufferInfo[ 0 ];
    }
    else if ( ( mIgOutForceBuffer == 1 ) && readyForReadB ) // force reading buffer B
    {
        pRdbMsg            = pRdbMsgB;
        pCurrentBufferInfo = pBufferInfo[ 1 ];
    }
    
    // lock the buffer that will be processed now (by this, no other process will alter the contents)
    if ( pRdbMsg && pCurrentBufferInfo )
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
        fprintf( stderr, "checkIgOutShm: zero message data size, error.\n" );
        return 0;
    }
    
    unsigned int maxReadSize = pCurrentBufferInfo->bufferSize;
    
    while ( 1 )
    {
        // handle the message that is contained in the buffer; this method should be provided by the user (i.e. YOU!)
        fprintf( stderr, "checkIgOutShm: processing message in buffer %s\n", ( pRdbMsg == pRdbMsgA ) ? "A" : "B" );
        
        handleMessage( pRdbMsg, index );
        
        // do not read more bytes than there are in the buffer (avoid reading a following buffer accidentally)
        maxReadSize -= pRdbMsg->hdr.dataSize + pRdbMsg->hdr.headerSize;

        if ( maxReadSize < ( pRdbMsg->hdr.headerSize + pRdbMsg->entryHdr.headerSize ) )
            break;
            
        // go to the next message (if available); there may be more than one message in an SHM buffer!
        pRdbMsg = ( RDB_MSG_t* ) ( ( ( char* ) pRdbMsg ) + pRdbMsg->hdr.dataSize + pRdbMsg->hdr.headerSize );
        
        if ( !pRdbMsg )
            break;
            
        if ( pRdbMsg->hdr.magicNo != RDB_MAGIC_NO )
            break;
    }
    
    // release after reading
    pCurrentBufferInfo->flags &= ~mCheckMask;                   // remove the check mask
    pCurrentBufferInfo->flags &= ~RDB_SHM_BUFFER_FLAG_LOCK;     // remove the lock mask

    if ( mVerbose )
    {
        unsigned int flagsA = pBufferInfo[ 0 ]->flags;
        unsigned int flagsB = pBufferInfo[ 1 ]->flags;

        fprintf( stderr, "ImageReader::checkIgOutShm: after processing SHM\n" );
        fprintf( stderr, "ImageReader::checkIgOutShm: Buffer A: frameNo = %06d, flags = 0x%x, locked = <%s>, lock mask set = <%s>\n", 
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

double getTime()
{
    struct timeval tme;
    gettimeofday(&tme, 0);
    
    double now = tme.tv_sec + 1.0e-6 * tme.tv_usec;
    
    if ( mStartTime < 0.0 )
        mStartTime = now;
    
    return now;
}

void calcStatistics()
{
    double now = getTime();
    
    double dt = now - mStartTime;
    
    if ( dt < 1.e-6 )
        return;
        
    fprintf( stderr, "calcStatistics: received %d/%d images in %.3lf seconds (i.e. %.3lf/%.3lf images per second ), total number of errors = %d\n", 
                     mTotalNoImages[0], mTotalNoImages[1], dt, mTotalNoImages[0] / dt, mTotalNoImages[1] / dt, mTotalErrorCount );
}
