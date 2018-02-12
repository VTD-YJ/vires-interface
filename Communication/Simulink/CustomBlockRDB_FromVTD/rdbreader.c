/*
 * sfuntmpl_basic.c: Basic 'C' template for a level 2 S-function.
 *
 *  -------------------------------------------------------------------------
 *  | See matlabroot/simulink/src/sfuntmpl_doc.c for a more detailed template |
 *  -------------------------------------------------------------------------
 *
 * Copyright 1990-2001 The MathWorks, Inc.
 * $Revision: 1.26 $
 */


/*
 * You must specify the S_FUNCTION_NAME as the name of your S-function
 * (i.e. replace sfuntmpl_basic with the name of your S-function).
 */

#define S_FUNCTION_NAME rdbreader
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

/*
* include the actual data mapper source code from a different file
*/
#include "rdbMapper.c"

/*
* activate the following lines for dSPACE compilation environment
*/
/*
#ifndef MATLAB_MEX_FILE
#include <brtenv.h>
#endif
*/
 
static unsigned int sNetworkStreamNoElements  = 7500;                    // e.g. 7500 uints @ 4 bytes each
static unsigned int sNetworkStreamElementSize = sizeof( unsigned int );  // bytes per network element
static int          useDirectBuffer = 1;     // directly use the buffer coming from the input stream?
static unsigned int sTotalNoMsg     = 0;
static unsigned int sTotalNoPkg     = 0;
static unsigned int sFramesSinceRx  = 0;
static unsigned int sFrameTimeout   = 1000;     // timeout of values after 1000 frames
 
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

    if (!ssSetNumInputPorts(S, 4)) 
        return; //Setting the number of input ports
    
    // configure input on first port
    ssSetInputPortWidth( S, 0, sNetworkStreamNoElements );
    ssSetInputPortDataType( S, 0, SS_UINT32 );
    ssSetInputPortRequiredContiguous(S, 0, true);
    ssSetInputPortDirectFeedThrough(S, 0, 1);

    // configure input on second port ( frame no )
    ssSetInputPortWidth( S, 1, 1 );
    ssSetInputPortDataType( S, 1, SS_DOUBLE );
    ssSetInputPortRequiredContiguous(S, 1, true);
    ssSetInputPortDirectFeedThrough(S, 1, 1);

    // configure input on third port ( bytes received )
    ssSetInputPortWidth( S, 2, 1 );
    ssSetInputPortDataType( S, 2, SS_DOUBLE );
    ssSetInputPortRequiredContiguous(S, 2, true);
    ssSetInputPortDirectFeedThrough(S, 2, 1);

    // configure input on fourth port ( own ID, own X, own Y )
    ssSetInputPortWidth( S, 3, 3 );
    ssSetInputPortDataType( S, 3, SS_DOUBLE );
    ssSetInputPortRequiredContiguous(S, 3, true);
    ssSetInputPortDirectFeedThrough(S, 3, 1);

    // configure output ports
    if ( !ssSetNumOutputPorts( S, 15 ) ) 
        return;
    
    // port 0 is the debugging information vector with 200 * 4 bytes
    ssSetOutputPortWidth(    S, 0, 200 );
    ssSetOutputPortDataType( S, 0, SS_UINT32 );
    
    // RDB_OBJECT_STATE_t
    // port 1 holds the number of valid elements in port 2
    // port 2 holds up to 20 static object states of type RDB_OBJECT_STATE_t (each with 208 bytes, i.e. 52 * 4 bytes)
    ssSetOutputPortWidth(    S, 1, 1 );
    ssSetOutputPortDataType( S, 1, SS_UINT32 );
    ssSetOutputPortWidth(    S, 2, 20 * 52 );
    ssSetOutputPortDataType( S, 2, SS_UINT32 );
        
    // RDB_OBJECT_STATE_BASE_t
    // port 3 holds the number of valid elements in port 4
    // port 4 holds up to 20 static object states of type RDB_OBJECT_STATE_BASE_t (each with 112 bytes, i.e. 28 * 4 bytes)
    ssSetOutputPortWidth(    S, 3, 1 );
    ssSetOutputPortDataType( S, 3, SS_UINT32 );
    ssSetOutputPortWidth(    S, 4, 20 * 28 );
    ssSetOutputPortDataType( S, 4, SS_UINT32 );
        
    // RDB_CONTACT_POINT_t
    // port 5 holds the number of valid elements in port 6
    // port 6 holds up to 20 contact point infos (each with 56 bytes, i.e. 14 * 4 bytes)
    ssSetOutputPortWidth(    S, 5, 1 );
    ssSetOutputPortDataType( S, 5, SS_UINT32 );
    ssSetOutputPortWidth(    S, 6, 20 * 14 );
    ssSetOutputPortDataType( S, 6, SS_UINT32 );
    
    // RDB_ENVIRONMENT_t
    // port 7 holds the number of valid elements in port 8
    // port 8 holds up to 1 environment state info (each with 48 bytes, i.e. 12 * 4 bytes)
    ssSetOutputPortWidth(    S, 7, 1 );
    ssSetOutputPortDataType( S, 7, SS_UINT32 );
    ssSetOutputPortWidth(    S, 8, 1 * 12 );
    ssSetOutputPortDataType( S, 8, SS_UINT32 );
    
    // RDB_DRIVER_CTRL_t
    // port 9 holds the number of valid elements in port 10
    // port 10 holds up to 1 driver control infos (each with 80 bytes, i.e. 20 * 4 bytes)
    ssSetOutputPortWidth(    S, 9, 1 );
    ssSetOutputPortDataType( S, 9, SS_UINT32 );
    ssSetOutputPortWidth(    S, 10, 1 * 20 );
    ssSetOutputPortDataType( S, 10, SS_UINT32 );
    
    // RDB_ROAD_POS_t
    // port 11 holds the number of valid elements in port 12
    // port 12 holds up to 20 road position infos (each with 40 bytes, i.e. 10 * 4 bytes)
    ssSetOutputPortWidth(    S, 11, 1 );
    ssSetOutputPortDataType( S, 11, SS_UINT32 );
    ssSetOutputPortWidth(    S, 12, 20 * 10 );
    ssSetOutputPortDataType( S, 12, SS_UINT32 );
    
    // RDB_VEHICLE_SYSTEMS_t
    // port 13 holds the number of valid elements in port 14
    // port 14 holds up to 20 vehicle system infos (each with 44 bytes, i.e. 11 * 4 bytes)
    ssSetOutputPortWidth(    S, 13, 1 );
    ssSetOutputPortDataType( S, 13, SS_UINT32 );
    ssSetOutputPortWidth(    S, 14, 20 * 11 );
    ssSetOutputPortDataType( S, 14, SS_UINT32 );
               
    
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
    rdbMapperInit();
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
    
    unsigned int *inData         = (unsigned int*)ssGetInputPortSignal( S, 0 );
    double       *inFrameData    = (double*)ssGetInputPortSignal( S, 1 );
    unsigned int currentNetFrame = ( unsigned int ) *inFrameData;
    unsigned int *outData        = ssGetOutputPortSignal(S,0);
    char         *outPtr         = ( char* )outData;
    void         *networkData    = ( void* ) inData;
    char         terminateFrame  = 0;
    unsigned int noPkg           = 0;
    unsigned int magicNo         = 0;
    
    unsigned int networkDataSize  = sNetworkStreamNoElements * sNetworkStreamElementSize;
    double *inPtr2 = (double*) ssGetInputPortSignal( S, 2 );

    // overwrite network data size with the actually valid number of data
    if ( sHoldOneMessageOnly || useDirectBuffer )
        networkDataSize = ( unsigned int ) *inPtr2;
    
    // directly use the incoming data without copying to anything
    if ( useDirectBuffer )
        rdbMapperSetBuffer( inData, networkDataSize ); 
     
    // handle new data that has been received
    if ( currentNetFrame != sLastNetworkFrame )
    {
        if ( !useDirectBuffer )
            rdbMapperHandleIncomingData( inData, networkDataSize );
         
        // remember last frame from network that has been processed
        sLastNetworkFrame = currentNetFrame;
    }
   
    if ( rdbMapperMessageIsAvailable() )
    {
        static double       simTime;
        static unsigned int simFrame;
        int                 i;
        
        // get own ID and position
        double*      ownData = ( double* )      ssGetInputPortSignal( S, 3 );
        unsigned int ownID   = ( unsigned int ) ownData[0];

        // get basic simulation data
        rdbMapperMessageGetTimeAndFrame( &simTime, &simFrame );
         
        memcpy( outPtr, &simTime, sizeof( double ) );
        outPtr += sizeof( double );
        memcpy( outPtr, &simFrame, sizeof( int ) );
        outPtr += sizeof( int );
        
        // are we receiving an old simulation frame again?
        if ( simFrame == sLastSimFrame )
            sFramesSinceRx++;
        else    // okay, we have a new message
            sFramesSinceRx = 0;
        
        // remember the last sim frame that has been received    
        sLastSimFrame = simFrame;
        
        // terminate the frame after processing it
        terminateFrame = 1;
        
        for ( i = 1; i < 14; i += 2 )
        {
            unsigned int *tgtScalar = ssGetOutputPortSignal( S, i );
            unsigned int *tgtVec    = ssGetOutputPortSignal( S, i + 1 );
            unsigned int maxSize    = ssGetOutputPortWidth( S, i + 1 ) * sizeof( unsigned int );
           
            switch ( i )
            {
                case 1: // get the dynamic objects
                    *tgtScalar = rdbMapperGetDynamicObjects( tgtVec, maxSize, ownID, ownData[1], ownData[2] );
                    break;
                    
                case 3: // get the static objects
                    *tgtScalar = rdbMapperGetStaticObjects( tgtVec, maxSize, ownID, ownData[1], ownData[2] );
                    break;
                    
                case 5: // get the contact points
                    *tgtScalar = rdbMapperGetOwnElementsOfType( ownID, tgtVec, maxSize, RDB_PKG_ID_CONTACT_POINT );
                    break;
                    
                case 7: // get the environment
                    *tgtScalar = rdbMapperGetOwnElementsOfType( ownID, tgtVec, maxSize, RDB_PKG_ID_ENVIRONMENT );
                    break;
                    
                case 9: // get the driver control
                    *tgtScalar = rdbMapperGetOwnElementsOfType( ownID, tgtVec, maxSize, RDB_PKG_ID_DRIVER_CTRL );
                    break;
                    
                case 11: // get the road positions
                    *tgtScalar = rdbMapperGetElementsOfType( tgtVec, maxSize, RDB_PKG_ID_ROAD_POS );
                    break;
                    
                case 13: // get the vehicle systems
                    *tgtScalar = rdbMapperGetElementsOfType( tgtVec, maxSize, RDB_PKG_ID_VEHICLE_SYSTEMS );
                    break;
            }
        }
    }
    else
        sFramesSinceRx++;
        
    // reset values after a given timeout
    if ( sFramesSinceRx > sFrameTimeout )
    {
        int i;
        for ( i = 1; i < 14; i += 2 )
        {
            unsigned int *tgtScalar = ssGetOutputPortSignal( S, i );
            unsigned int *tgtVec    = ssGetOutputPortSignal( S, i + 1 );
            unsigned int maxSize    = ssGetOutputPortWidth( S, i + 1 ) * sizeof( unsigned int );
            
            *tgtScalar = 0;
            memset( tgtVec, 0, maxSize );
        }
    }
 
    {
        unsigned int bufferSize;
                
        bufferSize = rdbMapperGetBufferUsage();

        outPtr = ( char* )outData;
        outPtr += sizeof( double ) + sizeof( int );
        memcpy( outPtr, &bufferSize, sizeof( int ) );
        outPtr += sizeof( int );
    }
    
    // debug info: magic number
    magicNo = rdbMapperMessageGetMagicNo();
    memcpy( outPtr, &magicNo, sizeof( int ) );
    outPtr += sizeof( int );
   
    // debug info: number of messages that have been processed
    sTotalNoMsg += rdbMapperGetNoOfMsgInBuffer( &noPkg );
    sTotalNoPkg += noPkg;
    memcpy( outPtr, &sTotalNoMsg, sizeof( int ) );
    //memcpy( outPtr, &sDataBufferUsedSize, sizeof( int ) );
    outPtr += sizeof( int );
    memcpy( outPtr, &sTotalNoPkg, sizeof( int ) );
    //memcpy( outPtr, &sDebugValue01, sizeof( int ) );
    outPtr += sizeof( int );
   
    if ( terminateFrame )
    { 
        if ( sHoldOneMessageOnly || useDirectBuffer )
            rdbMapperClearBuffer();
        else
            rdbMapperTerminateFrame();
    }
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
   /*
    * rdbMapperTerminate();
    */
}


/*======================================================*
 * See sfuntmpl_doc.c for the optional S-function methods *
 *======================================================*/

/*=============================*
 * Required S-function trailer *
 *=============================*/

#ifdef  MATLAB_MEX_FILE    /* Is this file being compiled as a MEX-file? */
#include "simulink.c"      /* MEX-file interface mechanism */
#else
#include "cg_sfun.h"       /* Code generation registration function */
#endif
