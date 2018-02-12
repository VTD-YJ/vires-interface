// ExampleVehDynInteg.cpp : (c) 2014 by VIRES Simulationstechnologie GmbH
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include "RDBHandler.hh"

#define DEFAULT_RX_PORT     48190   /* for image port it should be 48192 */
#define DEFAULT_TX_PORT     48191   
#define DEFAULT_BUFFER      204800

char                      szServer[128];        // Server to connect to
static RDB_OBJECT_STATE_t sOwnObjectState;      // own object's state

int          mPortTx    = DEFAULT_TX_PORT;
int          mSocketTx  = -1;
unsigned int mAddressTx = INADDR_BROADCAST;
int          mPortRx    = DEFAULT_RX_PORT;
int          mSocketRx  = -1;
unsigned int mAddressRx = inet_addr( "127.0.0.1" );

// function prototypes
void parseRDBMessage( RDB_MSG_t* msg, bool & isImage );
void parseRDBMessageEntry( const double & simTime, const unsigned int & simFrame, RDB_MSG_ENTRY_HDR_t* entryHdr );
void handleRDBitem( const double & simTime, const unsigned int & simFrame, RDB_DRIVER_CTRL_t & item );
void handleRDBitem( const double & simTime, const unsigned int & simFrame, RDB_OBJECT_STATE_t & item, bool isExtended );
void sendOwnObjectState( int & sendSocket, const double & simTime, const unsigned int & simFrame );
int openRxPort();
int openTxPort();

//
// Function: usage:
//
// Description:
//    Print usage information and exit
//
void usage()
{
    printf("usage: rdbTest [-r:n] [-s:n]\n\n");
    printf("       -r:n      receive port\n");
    printf("       -s:n      send port\n");
    printf("       -a:IP     IP address of partner (127.0.0.1)\n");
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
                case 'r':        // Remote port
                    if (strlen(argv[i]) > 3)
                        mPortRx = atoi(&argv[i][3]);
                    break;
                case 's':        // Remote port
                    if (strlen(argv[i]) > 3)
                        mPortTx = atoi(&argv[i][3]);
                    break;
                case 'a':       // Server
                    if (strlen(argv[i]) > 3)
                        strcpy(szServer, &argv[i][3]);
                    break;
                default:
                    usage();
                    break;
            }
        }
    }
    
    mAddressTx = inet_addr( szServer );
    mAddressRx = inet_addr( szServer );
    
    fprintf( stderr, "ValidateArgs: TX port  = %d, RX port = %d, TX address = %s (0x%x)\n",
                     mPortTx, mPortRx, szServer, mAddressTx );
}

int main(int argc, char* argv[])
{
    char*              szBuffer = new char[DEFAULT_BUFFER];  // allocate on heap
    int                ret;
    struct sockaddr_in server;
    struct hostent    *host     = NULL;
    static bool        sVerbose = false;

    // Parse the command line
    //
    ValidateArgs(argc, argv);
    
    // initialize some variables
    memset( &sOwnObjectState, 0, sizeof( RDB_OBJECT_STATE_t ) );
    
    // open receive port
    if ( !openRxPort() )
    {
        fprintf( stderr, "main: FATAL: could not open receive port\n" );
        return 0;
    }
        
    // open send port
    if ( !openTxPort() )
    {
        fprintf( stderr, "main: FATAL: could not open send port\n" );
        return 0;
    }
        
    // now handle the messagess    
    unsigned int  bytesInBuffer = 0;
    size_t        bufferSize    = sizeof( RDB_MSG_HDR_t );
    unsigned int  count         = 0;
    unsigned char *pData        = ( unsigned char* ) calloc( 1, bufferSize );

    // Main loop, send and receive data - forever!
    for(;;)
    {
        bool bMsgComplete = false;
        
        // read data
        ret = recv( mSocketRx, szBuffer, DEFAULT_BUFFER, 0 );
        
        //fprintf( stderr, "main: ret = %d\n", ret );

        if ( ret > 0 )
        {
            // do we have to grow the buffer??
            if ( ( bytesInBuffer + ret ) > bufferSize )
            {
                pData      = ( unsigned char* ) realloc( pData, bytesInBuffer + ret );
                bufferSize = bytesInBuffer + ret;
            }

            memcpy( pData + bytesInBuffer, szBuffer, ret );
            bytesInBuffer += ret;

            // already complete messagae?
            if ( bytesInBuffer >= sizeof( RDB_MSG_HDR_t ) )
            {
                RDB_MSG_HDR_t* hdr = ( RDB_MSG_HDR_t* ) pData;

                // is this message containing the valid magic number?
                if ( hdr->magicNo != RDB_MAGIC_NO )
                {
                    printf( "message receiving is out of sync; discarding data" );
                    bytesInBuffer = 0;
                }

                // handle all messages in the buffer before proceeding
                while ( bytesInBuffer >= ( hdr->headerSize + hdr->dataSize ) )
                {
                    unsigned int msgSize = hdr->headerSize + hdr->dataSize;
                    bool         isImage = false;
 
                    // print the message
                    if ( sVerbose )
                        Framework::RDBHandler::printMessage( ( RDB_MSG_t* ) pData, true );
 
                    // now parse the message
                    parseRDBMessage( ( RDB_MSG_t* ) pData, isImage );

                    // remove message from queue
                    memmove( pData, pData + msgSize, bytesInBuffer - msgSize );
                    bytesInBuffer -= msgSize;
                }
            }
        }
        
        // do some other stuff before returning to network reading
        
        // don't use the processor excessively
        usleep( 10 );
    }
    ::close( mSocketTx );
    ::close( mSocketRx );

    return 0;
}

int openRxPort()
{
    // port already open?
    if ( mSocketRx > -1 )
        return 0;
        
    struct sockaddr_in sock;
    int                opt;
    unsigned int       mAddress = mAddressRx;

    fprintf( stderr, "openRxPort: RX port  = %d, RX address = 0x%x\n", mPortRx, mAddressRx );

    mSocketRx = socket( AF_INET, SOCK_DGRAM, 0 );
    
    if ( mSocketRx == -1 )
    {
        fprintf( stderr, "openRxPort: could not open socket\n" );
        return 0;
    }

    opt = 1;
    if ( setsockopt( mSocketRx, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof( opt ) ) == -1 )
    {
        fprintf( stderr, "openRxPort: could not set address to reuse\n" );
        return 0;
    }

    memset( &sock, 0, sizeof( sock ) );
    sock.sin_family 	 = AF_INET;
    sock.sin_addr.s_addr = mAddressRx;
    sock.sin_port        = htons( mPortRx );

    if ( bind( mSocketRx, (struct sockaddr *) &sock, sizeof( struct sockaddr_in ) ) == -1 )
    {
        fprintf( stderr, "openRxPort: could not bind socket\n" );
        perror( "reason: " );
        return 0;
    }

    if ( mSocketRx < 0 )
        return 0;

    opt = 1;
    ioctl( mSocketRx, FIONBIO, &opt );
    
    fprintf( stderr, "openRxPort: socketRx = %d\n", mSocketRx );
    
    return 1;
}

int openTxPort()
{
    struct sockaddr_in sock;
    int                opt;
    
    mSocketTx = socket( AF_INET, SOCK_DGRAM, 0 );

    if ( mSocketTx == -1 ) 
    { 
        fprintf( stderr, "openTxPort: could not open send socket");
        return 0;
    }

    if ( 1 )    // broadcast!
    {   
        opt = 1;
        if ( setsockopt( mSocketTx, SOL_SOCKET, SO_BROADCAST, &opt, sizeof( opt ) ) == -1 )
        {
            fprintf( stderr, "openTxPort: could not set socket to broadcast");
            return 0;
        }
    }

    opt = 1;
    if ( setsockopt( mSocketTx, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof( opt ) ) == -1 ) 
    { 
        fprintf( stderr, "openTxPort: could not set address to reuse");
        return 0;
    }
    
    sock.sin_family      = AF_INET;
    sock.sin_addr.s_addr = mAddressTx;
    sock.sin_port        = htons( mPortTx ); 

    if ( connect( mSocketTx, reinterpret_cast<struct sockaddr*>( &sock ), sizeof( struct sockaddr_in ) ) == -1 ) 
    { 
        fprintf( stderr, "openTxPort: could not connect socket");
        return 0;
    }

    return 1 ;
}

void parseRDBMessage( RDB_MSG_t* msg, bool & isImage )
{
    if ( !msg )
      return;

    if ( !msg->hdr.dataSize )
        return;
    
    RDB_MSG_ENTRY_HDR_t* entry = ( RDB_MSG_ENTRY_HDR_t* ) ( ( ( char* ) msg ) + msg->hdr.headerSize );
    uint32_t remainingBytes    = msg->hdr.dataSize;
        
    while ( remainingBytes )
    {
        parseRDBMessageEntry( msg->hdr.simTime, msg->hdr.frameNo, entry );

        isImage |= ( entry->pkgId == RDB_PKG_ID_IMAGE );

        remainingBytes -= ( entry->headerSize + entry->dataSize );
        
        if ( remainingBytes )
          entry = ( RDB_MSG_ENTRY_HDR_t* ) ( ( ( ( char* ) entry ) + entry->headerSize + entry->dataSize ) );
    }
}

void parseRDBMessageEntry( const double & simTime, const unsigned int & simFrame, RDB_MSG_ENTRY_HDR_t* entryHdr )
{
    if ( !entryHdr )
        return;
    
    int noElements = entryHdr->elementSize ? ( entryHdr->dataSize / entryHdr->elementSize ) : 0;
    
    unsigned char ident   = 6;
    char*         dataPtr = ( char* ) entryHdr;
        
    dataPtr += entryHdr->headerSize;
        
    while ( noElements-- )      // only two types of messages are handled here
    {
        switch ( entryHdr->pkgId )
        {
            case RDB_PKG_ID_OBJECT_STATE:
                handleRDBitem( simTime, simFrame, *( ( RDB_OBJECT_STATE_t* ) dataPtr ), entryHdr->flags & RDB_PKG_FLAG_EXTENDED );
                break;

            case RDB_PKG_ID_DRIVER_CTRL:
                handleRDBitem( simTime, simFrame, *( ( RDB_DRIVER_CTRL_t* ) dataPtr ) );
                break;
                
            default:
                break;
        }
        dataPtr += entryHdr->elementSize;
     }
}

void handleRDBitem( const double & simTime, const unsigned int & simFrame, RDB_OBJECT_STATE_t & item, bool isExtended )
{
  fprintf( stderr, "handleRDBitem: handling object state\n" );
  fprintf( stderr, "    simTime = %.3lf, simFrame = %ld\n", simTime, simFrame );
  fprintf( stderr, "    object = %s, id = %d\n", item.base.name, item.base.id );
  fprintf( stderr, "    position = %.3lf / %.3lf / %.3lf\n", item.base.pos.x, item.base.pos.y, item.base.pos.z );

  if ( isExtended )
    fprintf( stderr, "    speed = %.3lf / %.3lf / %.3lf\n", item.ext.speed.x, item.ext.speed.y, item.ext.speed.z );
}

/**
* handle driver control input and compute vehicle dynamics output
*/
void handleRDBitem( const double & simTime, const unsigned int & simFrame, RDB_DRIVER_CTRL_t & item )
{
    static bool         sVerbose     = true;
    static bool         sShowMessage = false;
    static unsigned int sMyPlayerId  = 1;             // this may also be determined from incoming OBJECT_CFG messages
    static double       sLastSimTime = -1.0;
    
    fprintf( stderr, "handleRDBitem: handling driver control for player %d\n", item.playerId );

    // is this a new message?
    //if ( simTime == sLastSimTime )
    //    return;
    
    // is this message for me?
    if ( item.playerId != sMyPlayerId )
        return;
        
    // check for valid inputs (only some may be valid)
    float mdSteeringAngleRequest = ( item.validityFlags & RDB_DRIVER_INPUT_VALIDITY_STEERING_WHEEL ) ? item.steeringWheel / 19.0 : 0.0;
    float mdThrottlePedal        = ( item.validityFlags & RDB_DRIVER_INPUT_VALIDITY_THROTTLE )       ? item.throttlePedal        : 0.0;
    float mdBrakePedal           = ( item.validityFlags & RDB_DRIVER_INPUT_VALIDITY_BRAKE )          ? item.brakePedal           : 0.0;
    float mInputAccel            = ( item.validityFlags & RDB_DRIVER_INPUT_VALIDITY_TGT_ACCEL )      ? item.accelTgt             : 0.0;
    float mInputSteering         = ( item.validityFlags & RDB_DRIVER_INPUT_VALIDITY_TGT_STEERING )   ? item.steeringTgt          : 0.0;
    float mdSteeringRequest      = ( item.validityFlags & RDB_DRIVER_INPUT_VALIDITY_TGT_STEERING )   ? item.steeringTgt          : 0.0;
    float mdAccRequest           = ( item.validityFlags & RDB_DRIVER_INPUT_VALIDITY_TGT_ACCEL )      ? item.accelTgt             : 0.0;
    int   mInputGear             = 0;
    
    // check the input validity
    unsigned int validFlagsLat  = RDB_DRIVER_INPUT_VALIDITY_TGT_STEERING | RDB_DRIVER_INPUT_VALIDITY_STEERING_WHEEL;
    unsigned int validFlagsLong = RDB_DRIVER_INPUT_VALIDITY_TGT_ACCEL | RDB_DRIVER_INPUT_VALIDITY_THROTTLE | RDB_DRIVER_INPUT_VALIDITY_BRAKE;
    unsigned int checkFlags     = item.validityFlags & 0x00000fff;
    
    if ( checkFlags )
    {
        if ( ( checkFlags & validFlagsLat ) && ( checkFlags & validFlagsLong ) )
            sShowMessage = false;
        else if ( checkFlags != RDB_DRIVER_INPUT_VALIDITY_GEAR ) // "gear only" is also fine
        {
            if ( !sShowMessage )
                fprintf( stderr, "Invalid driver input for vehicle dynamics" );
            
            sShowMessage = true;
        }
    }
    
    // use pedals/wheel or targets?
    bool mUseSteeringTarget = ( ( item.validityFlags & RDB_DRIVER_INPUT_VALIDITY_TGT_STEERING ) != 0 );
    bool mUseAccelTarget    = ( ( item.validityFlags & RDB_DRIVER_INPUT_VALIDITY_TGT_ACCEL ) != 0 );
    
    if ( item.validityFlags & RDB_DRIVER_INPUT_VALIDITY_GEAR )
    {
        if ( item.gear == RDB_GEAR_BOX_POS_R )
            mInputGear = -1; 
        else if ( item.gear == RDB_GEAR_BOX_POS_N )
            mInputGear = 0; 
        else if ( item.gear == RDB_GEAR_BOX_POS_D )
            mInputGear = 1; 
        else
            mInputGear = 1;
    }
    
    // now, depending on the inputs, select the control mode and compute outputs
    if ( mUseSteeringTarget && mUseAccelTarget )
    {
        fprintf( stderr, "Compute new vehicle position from acceleration target and steering target.\n" );
        
        // call your methods here
    }
    else if ( !mUseSteeringTarget && !mUseAccelTarget )
    {
        fprintf( stderr, "Compute new vehicle position from brake pedal, throttle pedal and steering wheel angle.\n" );
        
        // call your methods here
    }
    else
    {
        fprintf( stderr, "Compute new vehicle position from a mix of targets and pedals / steering wheel angle.\n" );
        
        // call your methods here
    }
    
    bool useDummy = true;
    
    // the following assignments are for dummy purposes only
    // vehicle moves along x-axis with given speed
    // ignore first message
    if ( useDummy && ( sLastSimTime >= 0.0 ) )
    {
        double speedX = 5.0;    // m/s
        double speedY = 0.0;    // m/s
        double speedZ = 0.0;    // m/s
        double dt     = simTime - sLastSimTime;
        
        sOwnObjectState.base.id       = sMyPlayerId;
        sOwnObjectState.base.category = RDB_OBJECT_CATEGORY_PLAYER;
        sOwnObjectState.base.type     = RDB_OBJECT_TYPE_PLAYER_CAR;
        strcpy( sOwnObjectState.base.name, "Ego" );
 
        // dimensions of own vehicle
        sOwnObjectState.base.geo.dimX = 4.60;
        sOwnObjectState.base.geo.dimY = 1.86;
        sOwnObjectState.base.geo.dimZ = 1.60;
 
        // offset between reference point and center of geometry
        sOwnObjectState.base.geo.offX = 0.80;
        sOwnObjectState.base.geo.offY = 0.00;
        sOwnObjectState.base.geo.offZ = 0.30;
 
        sOwnObjectState.base.pos.x     += dt * speedX;
        sOwnObjectState.base.pos.y     += dt * speedY;
        sOwnObjectState.base.pos.z     += dt * speedZ;
        sOwnObjectState.base.pos.h     = 0.0;
        sOwnObjectState.base.pos.p     = 0.0;
        sOwnObjectState.base.pos.r     = 0.0;
        sOwnObjectState.base.pos.flags = RDB_COORD_FLAG_POINT_VALID | RDB_COORD_FLAG_ANGLES_VALID;
 
        sOwnObjectState.ext.speed.x     = speedX;
        sOwnObjectState.ext.speed.y     = speedY;
        sOwnObjectState.ext.speed.z     = speedZ;
        sOwnObjectState.ext.speed.h     = 0.0;
        sOwnObjectState.ext.speed.p     = 0.0;
        sOwnObjectState.ext.speed.r     = 0.0;
        sOwnObjectState.ext.speed.flags = RDB_COORD_FLAG_POINT_VALID | RDB_COORD_FLAG_ANGLES_VALID;
 
        sOwnObjectState.ext.accel.x     = 0.0;
        sOwnObjectState.ext.accel.y     = 0.0;
        sOwnObjectState.ext.accel.z     = 0.0;
        sOwnObjectState.ext.accel.flags = RDB_COORD_FLAG_POINT_VALID;
 
        sOwnObjectState.base.visMask    =  RDB_OBJECT_VIS_FLAG_TRAFFIC | RDB_OBJECT_VIS_FLAG_RECORDER;
    }
    
    // ok, I have a new object state, so let's send the data
    sendOwnObjectState( mSocketTx, simTime, simFrame );
    
    // remember last simulation time
    sLastSimTime = simTime;
}

void sendOwnObjectState( int & sendSocket, const double & simTime, const unsigned int & simFrame )
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
