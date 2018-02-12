/*
 * You must specify the S_FUNCTION_NAME as the name of your S-function
 * (i.e. replace sfuntmpl_basic with the name of your S-function).
 */

#define S_FUNCTION_NAME viresUdpTx
#define S_FUNCTION_LEVEL 2

/*
 * Need to include simstruc.h for the definition of the SimStruct and
 * its associated macro definitions.
 */
/* #define DUMMY_COMPILE  */
#ifdef DUMMY_COMPILE
#include "slDummyDefs.h"
#else
#include "simstruc.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

static unsigned int sNetworkStreamNoElements  = 150;                    // e.g. 150 uints @ 4 bytes each
static unsigned int sNetworkStreamElementSize = sizeof( unsigned int );  // bytes per network element
static unsigned int mAddress = 0;
static char* mRxBuffer   = 0;
static int   mDescriptor = -1;
static int   mBroadcast  = 1;

/*
* additional methods
* @param portId id of the port that is to be used
*/
static void openUDPSendPort( unsigned int portId );
static unsigned int writeUDPPort( unsigned int* data, unsigned int dataSize );
 
/* Error handling
 * --------------
 *
 * You should use the following technique to report errors encountered within
 * an S-function:
 *
 *       ssSetErrorStatus(S,"Error encountered due to ...");
 *       return;
 *
 * Note that the 2nd argument to ssSetErrorStatus must be persistent memory.
 * It cannot be a local variable. For example the following will cause
 * unpredictable errors:
 *
 *      mdlOutputs()
 *      {
 *         char msg[256];         {ILLEGAL: to fix use "static char msg[256];"}
 *         sprintf(msg,"Error due to %s", string);
 *         ssSetErrorStatus(S,msg);
 *         return;
 *      }
 *
 * See matlabroot/simulink/src/sfuntmpl_doc.c for more details.
 */

/*====================*
 * S-function methods *
 *====================*/

/* Function: mdlInitializeSizes ===============================================
 * Abstract:
 *    The sizes information is used by Simulink to determine the S-function
 *    block's characteristics (number of inputs, outputs, states, etc.).
 */

#define MDL_CHECK_PARAMETERS
#if defined(MDL_CHECK_PARAMETERS) && defined(MATLAB_MEX_FILE)
  /* Function: mdlCheckParameters =============================================
   * Abstract:
   *    Validate our parameters to verify they are okay.
   */
  static void mdlCheckParameters(SimStruct *S) // S function callback routine
  {
  }

#endif /* MDL_CHECK_PARAMETERS */

static void mdlInitializeSizes(SimStruct *S)	//S function callback routine
{
    ssSetNumContStates(S, 0);		//Simstruct function setting the number of continuous states
    ssSetNumDiscStates(S, 0);		//Simstruct function setting the number of discrete states

    if (!ssSetNumInputPorts(S, 9)) 
        return; //Setting the number of input ports
    
    // active / inactive
    ssSetInputPortWidth( S, 0, 1 );
    ssSetInputPortDataType( S, 0, SS_UINT32 );
    ssSetInputPortRequiredContiguous(S, 0, true);
    ssSetInputPortDirectFeedThrough(S, 0, 1);

    // configure input on third port ( portNo )
    ssSetInputPortWidth( S, 1, 1 );
    ssSetInputPortDataType( S, 1, SS_UINT32 );
    ssSetInputPortRequiredContiguous(S, 1, true);
    ssSetInputPortDirectFeedThrough(S, 1, 1);

    // configure input on second port ( frame no )
    ssSetInputPortWidth( S, 2, 1 );
    ssSetInputPortDataType( S, 2, SS_UINT32 );
    ssSetInputPortRequiredContiguous(S, 2, true);
    ssSetInputPortDirectFeedThrough(S, 2, 1);

    // configure input on fourth port ( noBytes )
    ssSetInputPortWidth( S, 3, 1 );
    ssSetInputPortDataType( S, 3, SS_UINT32 );
    ssSetInputPortRequiredContiguous(S, 3, true);
    ssSetInputPortDirectFeedThrough(S, 3, 1);

    // configure input on fifth port ( data )
    ssSetInputPortWidth( S, 4, sNetworkStreamNoElements );
    ssSetInputPortDataType( S, 4, SS_UINT32 );
    ssSetInputPortRequiredContiguous(S, 4, true);
    ssSetInputPortDirectFeedThrough(S, 4, 1);

    // configure input port ( address, part 1 )
    ssSetInputPortWidth( S, 5, 1 );
    ssSetInputPortDataType( S, 5, SS_UINT8 );
    ssSetInputPortRequiredContiguous(S, 5, true);
    ssSetInputPortDirectFeedThrough(S, 5, 1);

    // configure input port ( address, part 2 )
    ssSetInputPortWidth( S, 6, 1 );
    ssSetInputPortDataType( S, 6, SS_UINT8 );
    ssSetInputPortRequiredContiguous(S, 6, true);
    ssSetInputPortDirectFeedThrough(S, 6, 1);

    // configure input port ( address, part 3 )
    ssSetInputPortWidth( S, 7, 1 );
    ssSetInputPortDataType( S, 7, SS_UINT8 );
    ssSetInputPortRequiredContiguous(S, 7, true);
    ssSetInputPortDirectFeedThrough(S, 7, 1);

    // configure input port ( address, part 4 )
    ssSetInputPortWidth( S, 8, 1 );
    ssSetInputPortDataType( S, 8, SS_UINT8 );
    ssSetInputPortRequiredContiguous(S, 8, true);
    ssSetInputPortDirectFeedThrough(S, 8, 1);

    // configure output ports
    if ( !ssSetNumOutputPorts( S, 2 ) ) 
        return;

    // configure output on first port ( noBytesSent )
    ssSetOutputPortWidth( S, 0, 1 );
    ssSetOutputPortDataType( S, 0, SS_UINT32 );               
    
    // configure output on first port ( descriptor )
    ssSetOutputPortWidth( S, 1, 1 );
    ssSetOutputPortDataType( S, 1, SS_UINT32 );               
    
    ssSetNumSampleTimes(S, 1);
    ssSetNumRWork(S, 0);			//Concerned with work vectors
    ssSetNumIWork(S, 0);
    ssSetNumPWork(S, 0);
    ssSetNumModes(S, 0);
    ssSetNumNonsampledZCs(S, 0);

    ssSetOptions(S, 0);
}

/* Function: mdlInitializeSampleTimes =========================================
 * Abstract:
 *    This function is used to specify the sample time(s) for your
 *    S-function. You must register the same number of sample times as
 *    specified in ssSetNumSampleTimes.
 */
static void mdlInitializeSampleTimes(SimStruct *S)
{
    ssSetSampleTime(S, 0, -1.0);
    ssSetOffsetTime(S, 0, 0.0);
}

#define MDL_INITIALIZE_CONDITIONS   /* Change to #undef to remove function */
#if defined(MDL_INITIALIZE_CONDITIONS)
  /* Function: mdlInitializeConditions ========================================
   * Abstract:
   *    In this function, you should initialize the continuous and discrete
   *    states for your S-function block.  The initial states are placed
   *    in the state vector, ssGetContStates(S) or ssGetRealDiscStates(S).
   *    You can also perform any other initialization activities that your
   *    S-function may require. Note, this routine will be called at the
   *    start of simulation and if it is present in an enabled subsystem
   *    configured to reset states, it will be call when the enabled subsystem
   *    restarts execution to reset the states.
   */
  static void mdlInitializeConditions(SimStruct *S)
  {

  }
#endif /* MDL_INITIALIZE_CONDITIONS */


#define MDL_START  /* Change to #undef to remove function */
#if defined(MDL_START)
  /* Function: mdlStart =======================================================
   * Abstract:
   *    This function is called once at start of model execution. If you
   *    have states that should be initialized once, this is the place
   *    to do it.
   */
  static void mdlStart(SimStruct *S)
  {
  }
#endif /*  MDL_START */


/* Function: mdlOutputs =======================================================
 * Abstract:
 *    In this function, you compute the outputs of your S-function
 *    block. Generally outputs are placed in the output vector, ssGetY(S).
 */
static void mdlOutputs(SimStruct *S, int_T tid)
{
    static unsigned int sLastNetworkFrame = 0;
    static unsigned int sLastSimFrame     = 0;
    
    unsigned int*  inActive      = (unsigned int*)ssGetInputPortSignal( S, 0 );
    unsigned int*  inPortNo      = (unsigned int*)ssGetInputPortSignal( S, 1 );
    unsigned int*  inFrameNo     = (unsigned int*)ssGetInputPortSignal( S, 2 );
    unsigned int*  inFrameSize   = (unsigned int*)ssGetInputPortSignal( S, 3 );
    unsigned int*  inFrameData   = (unsigned int*)ssGetInputPortSignal( S, 4 );
    unsigned char* inAddr00      = (unsigned char*)ssGetInputPortSignal( S, 5 );
    unsigned char* inAddr01      = (unsigned char*)ssGetInputPortSignal( S, 6 );
    unsigned char* inAddr02      = (unsigned char*)ssGetInputPortSignal( S, 7 );
    unsigned char* inAddr03      = (unsigned char*)ssGetInputPortSignal( S, 8 );
    unsigned int*  outDataSize   = (unsigned int*)ssGetOutputPortSignal( S, 0 );
    unsigned int*  outDescriptor = (unsigned int*)ssGetOutputPortSignal( S, 1 );
    
    // is the function active at all?
    if ( ! *inActive )
        return;
        
    // assign the address
    unsigned char *pAddr = ( unsigned char* )&mAddress;
    pAddr[0] = *inAddr00;
    pAddr[1] = *inAddr01;
    pAddr[2] = *inAddr02;
    pAddr[3] = *inAddr03;
    
    // open the UDP Port
    openUDPSendPort( *inPortNo );

    // read the port
    *outDataSize = writeUDPPort( inFrameData, *inFrameSize );

    // for debugging
    *outDescriptor = mDescriptor;
}

#define MDL_UPDATE  /* Change to #undef to remove function */
#if defined(MDL_UPDATE)
  /* Function: mdlUpdate ======================================================
   * Abstract:
   *    This function is called once for every major integration time step.
   *    Discrete states are typically updated here, but this function is useful
   *    for performing any tasks that should only take place once per
   *    integration step.
   */
  static void mdlUpdate(SimStruct *S, int_T tid)
  {
  }
#endif /* MDL_UPDATE */



#define MDL_DERIVATIVES  /* Change to #undef to remove function */
#if defined(MDL_DERIVATIVES)
  /* Function: mdlDerivatives =================================================
   * Abstract:
   *    In this function, you compute the S-function block's derivatives.
   *    The derivatives are placed in the derivative vector, ssGetdX(S).
   */
  static void mdlDerivatives(SimStruct *S)
  {
  }
#endif /* MDL_DERIVATIVES */



/* Function: mdlTerminate =====================================================
 * Abstract:
 *    In this function, you should perform any actions that are necessary
 *    at the termination of a simulation.  For example, if memory was
 *    allocated in mdlStart, this is the place to free it.
 */
static void mdlTerminate(SimStruct *S)
{
    // properly close the port
    if ( mDescriptor > -1 )
        close( mDescriptor );
        
    mDescriptor = -1;
}

static void openUDPSendPort( unsigned int portId )
{
    // port already open?
    if ( mDescriptor > -1 )
        return;
        
    struct sockaddr_in sock;
    int                opt;

    // has the address yet been set?
    if ( mAddress == 0 )
        mAddress = inet_addr( "192.168.100.255" );
        
    mDescriptor = socket( AF_INET, SOCK_DGRAM, 0 );
    
    printf( "openUDPSendPort: sd=%d", mDescriptor );

    if ( mDescriptor == -1 ) 
    { 
        printf( "openUDPSendPort: could not open send socket\n" );
        return;
    }

    if ( mBroadcast )
    {
        //fprintf( stderr, "\n\nopenUDPSendPort::open: opening for broadcast\n" );
        opt = 1;
        if ( setsockopt( mDescriptor, SOL_SOCKET, SO_BROADCAST, &opt, sizeof( opt ) ) == -1 )
        {
            printf( "openUDPSendPort: could not set socket to broadcast\n" );
            mDescriptor = -1;
            return;
        }
    }

    opt = 1;
    if ( setsockopt( mDescriptor, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof( opt ) ) == -1 ) 
    { 
        printf( "openUDPSendPort: could not set address to reuse\n" );
        mDescriptor = -1;
        return;
    }
    
    sock.sin_family      = AF_INET;
    sock.sin_addr.s_addr = mAddress;
    sock.sin_port        = htons( portId ); 

    if ( connect( mDescriptor, ( struct sockaddr* )( &sock ), sizeof( struct sockaddr_in ) ) == -1 ) 
    { 
        printf( "openUDPSendPort: could not connect socket\n" );
        mDescriptor = -1;
        return;
    }
}

static unsigned int writeUDPPort( unsigned int *data, unsigned int dataSize )
{
    if ( mDescriptor < 0 )
        return 0;
    
    int noBytes = send( mDescriptor, data, dataSize, 0 );
    
    if ( noBytes < 0 )
    {
        printf( "writeUDPPort: error when sending data!\n" );

        noBytes = 0;
    }
    
    return ( unsigned int ) noBytes;
}

/*=============================*
 * Required S-function trailer *
 *=============================*/

#ifdef  MATLAB_MEX_FILE    /* Is this file being compiled as a MEX-file? */
#include "simulink.c"      /* MEX-file interface mechanism */
#else
#include "cg_sfun.h"       /* Code generation registration function */
#endif
