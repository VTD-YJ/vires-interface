// ShmReader.cpp : Sample implementation of a process reading
// from a shared memory segment (double buffered) with RDB layout
// (c) 2015 by VIRES Simulationstechnologie GmbH
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
#include "RDBHandler.hh"

#define DEFAULT_PORT        48190   /* for image port it should be 48192 */
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
* open a shared memory segment for reading image data
* @param index index of the SHM that is to be read
*/
void openShm( int index );

/**
* check and parse the contents of a shared memory
* @param index index of the SHM that is to be checked
* @param clearContent if true, clear the SHM completely
*/
int checkShm( int index, bool clearContent );

/**
* check for data that has to be read (otherwise socket will be clogged)
* @param descriptor socket descriptor
*/
int getNoReadyRead( int descriptor );

/**
* parse an RDB message (received via SHM or network)
* @param index      index of active shm segment
* @param msg    pointer to message that shall be parsed
*/
void parseRDBMessage( int index, RDB_MSG_t* msg );

/**
* parse an RDB message entry
* @param index      index of active shm segment
* @param simTime    simulation time of the message
* @param simFrame   simulation frame of the message
* @param entryHdr   pointer to the entry header itself
*/
void parseRDBMessageEntry( int index, const double & simTime, const unsigned int & simFrame, RDB_MSG_ENTRY_HDR_t* entryHdr );

/**
* handle an RDB item of type IMAGE
* @param index      index of active shm segment
* @param simTime    simulation time of the message
* @param simFrame   simulation frame of the message
* @param img        pointer to the image data
*/
void handleRDBitem( int index, const double & simTime, const unsigned int & simFrame, RDB_IMAGE_t* img );

/**
* routine for handling an RDB message; to be provided by user;
* here, only a printing of the message is performed
* @param index  index of active shm segment
* @param msg    pointer to the message that is to be handled
*/
void handleMessage( int index, RDB_MSG_t* msg );

/**
* send a trigger to the taskControl via network socket
* @param sendSocket socket descriptor
* @param simTime    internal simulation time
* @param simFrame   internal simulation frame
*/
void sendRDBTrigger( int & sendSocket, const double & simTime, const unsigned int & simFrame );


/**
* some global variables, considered "members" of this example
*/
unsigned int mShmKey[2]       = { 0x0811a, 0x0811b };      // key of the SHM segment
unsigned int mCheckMask       = RDB_SHM_BUFFER_FLAG_TC;
void*        mShmPtr[2]       = { 0, 0 };                       // pointer to the SHM segment
size_t       mShmTotalSize[2] = { 0, 0 };                       // remember the total size of the SHM segment
bool         mVerbose         = false;                          // run in verbose mode?
int          mForceBuffer     = -1;                             // force reading one of the SHM buffers (0=A, 1=B)
int          mHaveImage[2]    = { 0, 0 };                       // is an image available?
char         szServer[128];                                     // Server to connect to
int          iPort         = DEFAULT_PORT;                      // Port on server to connect to
int          mClient       = -1;                                // client socket
unsigned int mSimFrame     = 0;                                 // simulation frame counter
double       mSimTime      = 0.0;                               // simulation time
double       mDeltaTime    = 0.01;                              // simulation step width
int          mLastShmFrame[2] = { -1, -1 };

/**
* information about usage of the software
* this method will exit the program
*/
void usage()
{
    printf("usage: videoTest [-k:key] [-c:checkMask] [-v] [-f:bufferId] [-p:x] [-s:IP] [-h]\n\n");
    printf("       -k:key        SHM key that is to be addressed (second key is first key + 1)\n");
    printf("       -c:checkMask  mask against which to check before reading an SHM buffer\n");
    printf("       -f:bufferId   force reading of a given buffer (0 or 1) instead of checking for a valid checkMask\n");
    printf("       -p:x          Remote port to send to\n");
    printf("       -s:IP         Server's IP address or hostname\n");
    printf("       -v            run in verbose mode\n");
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
                case 'k':        // shared memory key
                    if ( strlen( argv[i] ) > 3 )
                    {
                        sscanf( &argv[i][3], "0x%x", &mShmKey[0] );
                        mShmKey[1] = mShmKey[0] + 1;
                    }
                    break;
                    
                case 'c':       // check mask
                    if ( strlen( argv[i] ) > 3 )
                        mCheckMask = atoi( &argv[i][3] );
                    break;
                    
                case 'f':       // force reading a given buffer
                    if ( strlen( argv[i] ) > 3 )
                        mForceBuffer = atoi( &argv[i][3] );
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
    
    fprintf( stderr, "ValidateArgs: keys = 0x%x / 0x%x, checkMask = 0x%x, mForceBuffer = %d\n", 
                     mShmKey[0], mShmKey[1], mCheckMask, mForceBuffer );
}

/**
* main program with high frequency loop for checking the shared memory;
* does nothing else
*/

int main(int argc, char* argv[])
{
    int  haveFirstImage = 6;
    bool clearContent   = false;
    
    // Parse the command line
    ValidateArgs(argc, argv);
    
    // open the network connection to the taskControl (so triggers may be sent)
    fprintf( stderr, "creating network connection....\n" );
    openNetwork();  // this is blocking until the network has been opened
    
    // now: open the shared memory (try to attach without creating a new segment)
    fprintf( stderr, "attaching to shared memories 0x%x and 0x%x....\n", mShmKey[0], mShmKey[1] );
    
    while ( !mShmPtr[0] || !mShmPtr[1] )
    {
        openShm( 0 );
        openShm( 1 );
        usleep( 1000 );     // do not overload the CPU
    }
    
    fprintf( stderr, "...attached! Reading now...\n" );
    
    // now check the SHM for the time being
    while ( 1 )
    {
        readNetwork();
        
        if ( !mHaveImage[0] )
            checkShm( 0, clearContent );
        if ( !mHaveImage[1] )
            checkShm( 1, clearContent );
            
        // never clear the content again!
        clearContent = false;
        
        // has a set of images occured?
        if ( mHaveImage[0] && mHaveImage[1] )
            haveFirstImage--;
        
        // has an image arrived or do the first frames need to be triggered 
        //(first image will arrive with a certain frame delay only)
        if ( ( mHaveImage[0] && mHaveImage[1] ) || ( haveFirstImage > 0 ) )
        {
            // do not initialize too fast
            if ( haveFirstImage > 0 )
                usleep( 100000 );   // 10H
            
            sendRDBTrigger( mClient, mSimTime, mSimFrame );

            // increase internal counters
            mSimTime += mDeltaTime;
            mSimFrame++;
        }
        
        // ok, reset image indicator
        if ( mHaveImage[0] &&  mHaveImage[1] )
        {
            mHaveImage[0] = 0;
            mHaveImage[1] = 0;
        }
        
        usleep( 10000 );
    }
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
 
                            // print the message
                            if ( sVerbose )
                                Framework::RDBHandler::printMessage( ( RDB_MSG_t* ) spData, true );
 
                            // now parse the message
                            //parseRDBMessage( ( RDB_MSG_t* ) spData );

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

void openShm( int index )
{
    // do not open twice!
    if ( mShmPtr[index] )
        return;
        
    int shmid = 0; 

    if ( ( shmid = shmget( mShmKey[index], 0, 0 ) ) < 0 )
        return;

    if ( ( mShmPtr[index] = (char *)shmat( shmid, (char *)0, 0 ) ) == (char *) -1 )
    {
        perror("openShm: shmat()");
        mShmPtr[index] = 0;
    }

    if ( mShmPtr[index] )
    {
        struct shmid_ds sInfo;

        if ( ( shmid = shmctl( shmid, IPC_STAT, &sInfo ) ) < 0 )
            perror( "openShm: shmctl()" );
        else
            mShmTotalSize[index] = sInfo.shm_segsz;
    }
}

int checkShm( int index, bool clearContent )
{
    if ( !mShmPtr[index] )
        return 0;

    // get a pointer to the shm info block
    RDB_SHM_HDR_t* shmHdr = ( RDB_SHM_HDR_t* ) ( mShmPtr[index] );

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
    RDB_MSG_t* pRdbMsgA = ( RDB_MSG_t* ) ( ( ( char* ) mShmPtr[index] ) + pBufferInfo[0]->offset );
    RDB_MSG_t* pRdbMsgB = ( RDB_MSG_t* ) ( ( ( char* ) mShmPtr[index] ) + pBufferInfo[1]->offset );
    
    if ( mVerbose )
        fprintf( stderr, "ImageReader::checkShm: Buffer A: offset = %06d, Buffer B: offset = %06d\n", pBufferInfo[0]->offset, pBufferInfo[1]->offset );
    
    if ( clearContent )
    {
        if ( mVerbose )
            fprintf( stderr, "ImageReader::checkShm: clearing the content\n" );
            
        memset( pRdbMsgA, 0, sizeof( RDB_MSG_HDR_t ) );
        memset( pRdbMsgB, 0, sizeof( RDB_MSG_HDR_t ) );
    }
    
    // pointer to the message that will actually be read
    RDB_MSG_t* pRdbMsg  = 0;

    if ( clearContent )
    {
        pBufferInfo[0]->flags = 0;
        pBufferInfo[1]->flags = 0;
    }

    // remember the flags that are set for each buffer
    unsigned int flagsA = pBufferInfo[ 0 ]->flags;
    unsigned int flagsB = pBufferInfo[ 1 ]->flags;
    
    // check whether any buffer is ready for reading (checkMask is set (or 0) and buffer is NOT locked)
    bool readyForReadA = ( ( flagsA & mCheckMask ) || !mCheckMask ) && !( flagsA & RDB_SHM_BUFFER_FLAG_LOCK );
    bool readyForReadB = ( ( flagsB & mCheckMask ) || !mCheckMask ) && !( flagsB & RDB_SHM_BUFFER_FLAG_LOCK );

    if ( mVerbose )
    {
        fprintf( stderr, "ImageReader::checkShm: before processing SHM 0x%x\n", mShmKey[index] );
        fprintf( stderr, "ImageReader::checkShm: Buffer A: frameNo = %06d, flags = 0x%x, locked = <%s>, check mask set = <%s>, readyForRead = <%s>\n", 
                         pRdbMsgA->hdr.frameNo, 
                         flagsA,
                         ( flagsA & RDB_SHM_BUFFER_FLAG_LOCK ) ? "true" : "false",
                         ( flagsA & mCheckMask ) ? "true" : "false",
                         readyForReadA ?  "true" : "false" );

        fprintf( stderr, "                       Buffer B: frameNo = %06d, flags = 0x%x, locked = <%s>, check mask set = <%s>, readyForRead = <%s>\n", 
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
            if ( pRdbMsgA->hdr.frameNo >= pRdbMsgB->hdr.frameNo )        // force using the latest image!!
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
    
    if ( mVerbose )
    {
        if ( pCurrentBufferInfo == pBufferInfo[ 0 ] )
            fprintf( stderr, "ImageReader::checkShm: reading buffer A\n" );
        else if ( pCurrentBufferInfo == pBufferInfo[ 1 ] )
            fprintf( stderr, "ImageReader::checkShm: reading buffer B\n" );
        else 
            fprintf( stderr, "ImageReader::checkShm: reading no buffer\n" );
    }
    
    
    // lock the buffer that will be processed now (by this, no other process will alter the contents)
    if ( pCurrentBufferInfo )
    {
        pCurrentBufferInfo->flags |= RDB_SHM_BUFFER_FLAG_LOCK;
        
        if ( mVerbose )
            fprintf( stderr, "ImageReader::checkShm: locked buffer, pRdbMsg = %p\n", pRdbMsg );
    }

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
    
    if ( mVerbose )
        fprintf( stderr, "ImageReader::checkShm: pRdbMsg->hdr.dataSize = %d\n", pRdbMsg->hdr.dataSize );
    
    // running the same frame again?
    if ( (( int ) pRdbMsg->hdr.frameNo ) > mLastShmFrame[index] )
    {
        // remember the last frame that was read
        mLastShmFrame[index] = pRdbMsg->hdr.frameNo;

        while ( 1 )
        {
            if ( mVerbose )
                fprintf( stderr, "ImageReader::checkShm: handling message, frame = %d\n", pRdbMsg->hdr.frameNo );

            // handle the message that is contained in the buffer; this method should be provided by the user (i.e. YOU!)
            handleMessage( index, pRdbMsg );
 
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

        fprintf( stderr, "ImageReader::checkShm: after processing SHM 0x%x\n", mShmKey[index] );
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

void handleMessage( int index, RDB_MSG_t* msg )
{
    // just print the message
    parseRDBMessage( index, msg );
}

void parseRDBMessage( int index, RDB_MSG_t* msg )
{
    if ( !msg )
      return;

    if ( !msg->hdr.dataSize )
        return;
    
    RDB_MSG_ENTRY_HDR_t* entry = ( RDB_MSG_ENTRY_HDR_t* ) ( ( ( char* ) msg ) + msg->hdr.headerSize );
    uint32_t remainingBytes    = msg->hdr.dataSize;
        
    while ( remainingBytes )
    {
        if ( mVerbose )
            fprintf( stderr, "ImageReader::parseRDBMessage: remainingBytes = %d, entry->headerSize = %d, entry->dataSize = %d\n", remainingBytes, entry->headerSize + entry->dataSize );

        parseRDBMessageEntry( index, msg->hdr.simTime, msg->hdr.frameNo, entry );

        remainingBytes -= ( entry->headerSize + entry->dataSize );
        
        if ( remainingBytes )
          entry = ( RDB_MSG_ENTRY_HDR_t* ) ( ( ( ( char* ) entry ) + entry->headerSize + entry->dataSize ) );
          
        if ( entry->headerSize == 0 )
            break;
    }
}

void parseRDBMessageEntry( int index, const double & simTime, const unsigned int & simFrame, RDB_MSG_ENTRY_HDR_t* entryHdr )
{
    if ( !entryHdr )
        return;
    
    int noElements = entryHdr->elementSize ? ( entryHdr->dataSize / entryHdr->elementSize ) : 0;
    char* dataPtr  = ( char* ) entryHdr;
        
    dataPtr += entryHdr->headerSize;
        
    while ( noElements-- )      // only two types of messages are handled here
    {
        switch ( entryHdr->pkgId )
        {
            case RDB_PKG_ID_IMAGE:
                handleRDBitem( index, simTime, simFrame, ( RDB_IMAGE_t* ) dataPtr );
                break;

            default:
                break;
        }
        dataPtr += entryHdr->elementSize;
     }
}

void handleRDBitem( int index, const double & simTime, const unsigned int & simFrame, RDB_IMAGE_t* img )
{
    if ( !img )
        return;
    fprintf( stderr, "handleRDBitem: image, index = %d\n", index );
    fprintf( stderr, "    simTime = %.3lf, simFrame = %ld, mLastShmFrame = %d\n", simTime, simFrame, mLastShmFrame[index] );
    fprintf( stderr, "    width / height = %d / %d\n", img->width, img->height );
    fprintf( stderr, "    dataSize = %d\n", img->imgSize );
    
    // ok, I have an image:
    mHaveImage[index] = 1;
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

void sendRDBTrigger( int & sendSocket, const double & simTime, const unsigned int & simFrame )
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
        
    trigger->frameNo = simFrame;
    trigger->deltaT  = mDeltaTime;
    
    fprintf( stderr, "sendRDBTrigger: sending trigger, deltaT = %.4lf\n", trigger->deltaT );
        
    int retVal = send( mClient, ( const char* ) ( myHandler.getMsg() ), myHandler.getMsgTotalSize(), 0 );

    if ( !retVal )
        fprintf( stderr, "sendRDBTrigger: could not send trigger\n" );
}
