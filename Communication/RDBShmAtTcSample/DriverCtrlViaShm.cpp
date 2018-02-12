// DriverCtrlViaShm.cpp : Sample implementation of a process reading
// from the TC shared memory segment and writing driver control messages back
// (c) 2015 by VIRES Simulationstechnologie GmbH
// Provided AS IS without any warranty!
//

#include <stdlib.h>
#include <stdio.h>
#include <sys/shm.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
#include "RDBHandler.hh"

// forward declarations of methods

/**
* method for checking the contents of the SHM
*/
int   readShm( void* shmPtr, unsigned int checkMask, bool verbose );
int   writeShm();
void* openShm( unsigned int key );

/**
* routine for handling an RDB message; to be provided by user;
* here, only a printing of the message is performed
* @param msg    pointer to the message that is to be handled
*/
void handleMessage( RDB_MSG_t* msg, bool verbose );


/**
* some global variables, considered "members" of this example
*/
unsigned int mShmKeyRead       = RDB_SHM_ID_TC_OUT; // key of the SHM segment that I will read
unsigned int mShmKeyReadSensor = 0x010203;          // key of the SHM segment from which I will read sensor output
unsigned int mShmKeyWrite      = RDB_SHM_ID_TC_IN;  // key of the SHM segment that I will write to
unsigned int mCheckMask        = 0x10000;           // first mask returned by a query
unsigned int mCheckMaskSensor  = 0x4;               // mask that has to be set so that sensor's SHM may be read
void*        mShmPtrRead       = 0;                 // pointer to the SHM segment that I will read
void*        mShmPtrReadSensor = 0;                 // pointer to the SHM segment from which I will read sensor output
void*        mShmPtrWrite      = 0;                 // pointer to the SHM segment that I will read
bool         mVerbose          = false;             // run in verbose mode?
double       mSimTime          = 0.0;               // simulation time as retrieved from incoming message
unsigned int mSimFrame         = 0;                 // simulation frame as retrieved from incoming message

/**
* information about usage of the software
* this method will exit the program
*/
void usage()
{
    printf("usage: driverCtrlTest [-r:key] [-w:key] [-c:checkMask] [-v] [-s:key]\n\n");
    printf("       -r:key        SHM key that is to be addressed for reading\n");
    printf("       -w:key        SHM key that is to be addressed for writing\n");
    printf("       -c:checkMask  mask against which to check before reading an SHM buffer\n");
    printf("       -s:key        SHM key from which sensor data may be read (0 for disabling)\n");
    printf("       -m:checkMask  mask against which to check before reading a sensor's SHM buffer\n");
    printf("       -v            run in verbose mode\n");
    exit(1);
}

/**
* validate the arguments given in the command line
*/
void ValidateArgs(int argc, char **argv)
{
    for( int i = 1; i < argc; i++)
    {
        if ((argv[i][0] == '-') || (argv[i][0] == '/'))
        {
            switch (tolower(argv[i][1]))
            {
                case 'r':        // shared memory key, reading
                    if ( strlen( argv[i] ) > 3 )
                        mShmKeyRead = atoi( &argv[i][3] );
                    break;
                    
                case 'w':        // shared memory key, writing
                    if ( strlen( argv[i] ) > 3 )
                        mShmKeyWrite = atoi( &argv[i][3] );
                    break;
                    
                case 'c':       // check mask
                    if ( strlen( argv[i] ) > 3 )
                        mCheckMask = atoi( &argv[i][3] );
                    break;
                    
                case 's':       // shared memory key, reading from sensor
                    if ( strlen( argv[i] ) > 3 )
                        mShmKeyReadSensor = atoi( &argv[i][3] );
                    break;
                    
                case 'm':       // check mask of sensor
                    if ( strlen( argv[i] ) > 3 )
                        mCheckMaskSensor = atoi( &argv[i][3] );
                    break;
                    
                case 'v':       // verbose mode
                    mVerbose = true;
                    break;
                    
                default:
                    usage();
                    break;
            }
        }
    }
    
    fprintf( stderr, "ValidateArgs: keyRead = 0x%x, keyWrite = 0x%x, checkMask = 0x%x\n", 
                     mShmKeyRead, mShmKeyWrite, mCheckMask );
}

/**
* main program with high frequency loop for checking the shared memory;
* does nothing else
*/

int main(int argc, char* argv[])
{
    // Parse the command line
    //
    ValidateArgs(argc, argv);
    
    // first: open the shared memory (try to attach without creating a new segment)
    
    fprintf( stderr, "Attaching to shared memory for reading....\n" );
    
    while ( !mShmPtrRead )
    {
        mShmPtrRead = openShm( mShmKeyRead );
        usleep( 1000 );     // do not overload the CPU
    }
    
    fprintf( stderr, "...attached!\nAttaching to shared memory for writing....\n" );
    
    while ( !mShmPtrWrite )
    {
        mShmPtrWrite = openShm( mShmKeyWrite );
        usleep( 1000 );     // do not overload the CPU
    }
    
    if ( mShmKeyReadSensor )
    {
        fprintf( stderr, "...attached!\nAttaching to shared memory for reading from sensor....\n" );
 
        while ( !mShmPtrReadSensor )
        {
            mShmPtrReadSensor = openShm( mShmKeyReadSensor );
            usleep( 1000 );     // do not overload the CPU
        }    
    }
    
    fprintf( stderr, "...attached! Processing now...\n" );
    
    // now check the SHM for the time being
    while ( 1 )
    {
        // if a message can be read, then write another message
        if ( readShm( mShmPtrRead, mCheckMask, false ) )
        {
            writeShm();
        }
        
        // check the sensor
        //fprintf( stderr, "main: checking sensor output\n" );
        readShm( mShmPtrReadSensor, mCheckMaskSensor, true );
        //fprintf( stderr, "main: finished checking sensor output\n" );
        
        usleep( 1000 );
    }
}

/**
* open the shared memory segment
*/
void* openShm( unsigned int key )
{
    void* shmPtr = 0;
    
    int shmid = 0; 

    if ( ( shmid = shmget( key, 0, 0 ) ) < 0 )
        return 0;

    if ( ( shmPtr = (char *)shmat( shmid, (char *)0, 0 ) ) == (char *) -1 )
    {
        perror("openShm: shmat()");
        shmPtr = 0;
    }

    if ( shmPtr )
    {
        struct shmid_ds sInfo;

        if ( ( shmid = shmctl( shmid, IPC_STAT, &sInfo ) ) < 0 )
        {
            perror( "openShm: shmctl()" );
            return 0;
        }
        else
            fprintf( stderr, "shared memory with key 0x%05x has size of %d bytes\n", key, sInfo.shm_segsz );
    }
    
    return shmPtr;
}

int readShm( void* shmPtr, unsigned int checkMask, bool verbose )
{
    if ( !mShmPtrRead )
        return 0;

    // get a pointer to the shm info block
    RDB_SHM_HDR_t* shmHdr = ( RDB_SHM_HDR_t* ) ( shmPtr );

    if ( !shmHdr )
        return 0;

    if ( ( shmHdr->noBuffers != 2 ) )
    {
        fprintf( stderr, "readShm: no or wrong number of buffers in shared memory. I need two buffers!\n" );
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
    bool readyForReadA = ( ( flagsA & mCheckMask ) || !checkMask ) && !( flagsA & RDB_SHM_BUFFER_FLAG_LOCK );
    bool readyForReadB = ( ( flagsB & mCheckMask ) || !checkMask ) && !( flagsB & RDB_SHM_BUFFER_FLAG_LOCK );

    if ( mVerbose )
    {
        fprintf( stderr, "readShm: before processing SHM\n" );
        fprintf( stderr, "readShm: Buffer A: frameNo = %06d, flags = 0x%x, locked = <%s>, lock mask set = <%s>, readyForRead = <%s>\n", 
                         pRdbMsgA->hdr.frameNo, 
                         flagsA,
                         ( flagsA & RDB_SHM_BUFFER_FLAG_LOCK ) ? "true" : "false",
                         ( flagsA & checkMask ) ? "true" : "false",
                         readyForReadA ?  "true" : "false" );

        fprintf( stderr, "         Buffer B: frameNo = %06d, flags = 0x%x, locked = <%s>, lock mask set = <%s>, readyForRead = <%s>\n", 
                         pRdbMsgB->hdr.frameNo, 
                         flagsB,
                         ( flagsB & RDB_SHM_BUFFER_FLAG_LOCK ) ? "true" : "false",
                         ( flagsB & checkMask ) ? "true" : "false",
                         readyForReadB ?  "true" : "false" );
    }

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
    
    while ( 1 )
    {
        // handle the message that is contained in the buffer; this method should be provided by the user (i.e. YOU!)
        handleMessage( pRdbMsg, verbose );
        
        // go to the next message (if available); there may be more than one message in an SHM buffer!
        pRdbMsg = ( RDB_MSG_t* ) ( ( ( char* ) pRdbMsg ) + pRdbMsg->hdr.dataSize + pRdbMsg->hdr.headerSize );
        
        if ( !pRdbMsg )
            break;
            
        if ( pRdbMsg->hdr.magicNo != RDB_MAGIC_NO )
            break;
    }
    
    // release after reading
    pCurrentBufferInfo->flags &= ~checkMask;                   // remove the check mask
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
                         ( flagsA & checkMask ) ? "true" : "false" );
        fprintf( stderr, "                       Buffer B: frameNo = %06d, flags = 0x%x, locked = <%s>, lock mask set = <%s>.\n", 
                         pRdbMsgB->hdr.frameNo, 
                         flagsB,
                         ( flagsB & RDB_SHM_BUFFER_FLAG_LOCK ) ? "true" : "false",
                         ( flagsB & checkMask ) ? "true" : "false" );
    }

    return 1;
}    

int writeShm()
{
    if ( !mShmPtrWrite )
        return 0;
        
    // use RDBHandler to get access to SHM
    Framework::RDBHandler myRdbHandler;
    
    // compose a new message
    myRdbHandler.initMsg();
    
    // add a driver ctrl package
    RDB_DRIVER_CTRL_t* driverCtrl = ( RDB_DRIVER_CTRL_t* ) myRdbHandler.addPackage( mSimTime, mSimFrame, RDB_PKG_ID_DRIVER_CTRL );
    
    if ( driverCtrl )
    {
        driverCtrl->playerId     = 1;                                 // let's control player no. 1
        driverCtrl->accelTgt     = sin( mSimTime );                   // just a sinus as acceleration
        driverCtrl->steeringTgt  = 0.1 * sin( 0.5 * mSimTime );       // another sinus for the steering
        driverCtrl->validityFlags = RDB_DRIVER_INPUT_VALIDITY_TGT_ACCEL |  RDB_DRIVER_INPUT_VALIDITY_TGT_STEERING |  RDB_DRIVER_INPUT_VALIDITY_ADD_ON;
    }
    
    // the message (held internally within the RDBHandler instance) is now complete, so let's continue wrting the whole stuff to SHM
    
    // set the access pointer
    myRdbHandler.shmSetAddress( mShmPtrWrite );
    
    // get the number of buffers
    unsigned int noBuffers     = myRdbHandler.shmGetNoBuffers();
    bool         haveBuffer    = false;
    unsigned int currentBuffer = 0;
    
    // wait for a buffer to become ready for writing
    while ( !haveBuffer )
    {
        for ( unsigned int i = 0; ( i < noBuffers ) && !haveBuffer; i++ )
        {
            if ( mVerbose )
                fprintf( stderr, "writeShm: buffer %d, flags = 0x%x\n", i, myRdbHandler.shmBufferGetFlags( i ) );
                
            if ( !myRdbHandler.shmBufferGetFlags( i ) )      // TC flag must already have been set 
            {
                myRdbHandler.shmBufferLock( i );
                currentBuffer = i;
                haveBuffer = true;
            }
        }
        usleep( 10 );       // do not overload the CPU
    }
    
    // now copy the message to the current RDB buffer in SHM
    myRdbHandler.addMsgToShm( currentBuffer, myRdbHandler.getMsg() );  
    
    // add the TC flag to the buffer
    myRdbHandler.shmBufferAddFlags( currentBuffer, RDB_SHM_BUFFER_FLAG_TC );
    
    // finally, release the buffer
    myRdbHandler.shmBufferRelease( currentBuffer );
    
    return 1;
}    

void handleMessage( RDB_MSG_t* msg, bool verbose )
{
    // just print the message
    //Framework::RDBHandler::printMessage( msg );
    
    if ( !msg )
        return;
        
    // remember incoming simulaton time
    mSimTime  = msg->hdr.simTime;
    mSimFrame = msg->hdr.frameNo;
    
    if ( mVerbose )
        fprintf( stderr, "handleMessage: simTime = %.3lf, simFrame = %d, dataSize = %d\n", mSimTime, mSimFrame, msg->hdr.dataSize );

    // now let's get information about ego vehicle etc.
    unsigned int noElements = 0;
    
    // look for object state of vehicle 0
    RDB_OBJECT_STATE_t* objState = ( RDB_OBJECT_STATE_t* ) Framework::RDBHandler::getFirstEntry( msg, RDB_PKG_ID_OBJECT_STATE, noElements, true );
    
    if ( mVerbose )
        fprintf( stderr, "handleMessage: noElements = %d\n", noElements );

    while ( noElements )
    {
        if ( mVerbose || verbose )
            fprintf( stderr, "handleMessage: object %d is at x/y = %.3lf / %.3lf\n", objState->base.id, objState->base.pos.x, objState->base.pos.y );
            
        noElements--;
        
        if ( noElements )
            objState++;
    }
}
