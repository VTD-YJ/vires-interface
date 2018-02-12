/* NOTICE:
*  THESE ROUTINES ARE PROVIDED TO ALL USERS OF VIRES VIRTUAL TEST DRIVE
*  FREE OF CHARGE AND FOR FREE USE IN COMMERCIAL PRODUCTS; THE SOFTWARE 
*  IS PROVIDED AS IS WITHOUT ANY WARRANTIES OR PERFORMANCE GUARANTEES
*  IMPLIED; IT IS THE USER'S RESPONSIBILITY TO VERIFY FITNESS OF THESE
*  ROUTINES FOR USER'S PURPOSES; BY NO MEANS SHALL VIRES SIMULATIONS-
*  TECHNOLOGIE GMBH BE HELD LIABLE FOR ANY DATA LOSS OR DAMAGE OF WHATEVER
*  SORT ARISING FROM THE USE OF THE ROUTINES PROVIDED IN THIS SOFTARE.
*  USER AGREES TO THESE CONDITIONS IMPLICITLY BY USING THESE ROUTINES.
*  (c) 2016 VIRES Simulationstechnologie GmbH
*/

/* ===================================================
 *  file:       vtdGatewayRxDyn.c
 * ---------------------------------------------------
 *  purpose:	RDB writer routine for 
 *              VIRES Virtual Test Drive
 * ---------------------------------------------------
 *  first edit:	14.02.2016 by M. Dupuis @ VIRES GmbH
 * ===================================================
 */
 
/* --- INCLUDES --- */
#include "vtdGatewayRxDyn.h"
#include <stdio.h>

/*
* include the actual data mapper source code from a different file
*/
#include "rdbMapper.c"

static unsigned int sFramesSinceRx  = 0;
static unsigned int sFrameTimeout   = 1000;     // timeout of values after 1000 frames

/*
* define the output members here
*/
static RX_DYN_GENERAL_t      vtdOutGeneral;
static RX_DYN_OBJECT_STATE_t vtdOutObjState;
static RX_DYN_DRIVER_CTRL_t  vtdOutDrvrCtrl;

/* IMPLEMENTATION */
void vtdGatewayInitialize( void )
{
    memset( &vtdOutGeneral,      0, sizeof( RX_DYN_GENERAL_t ) );
    memset( &vtdOutObjState,     0, sizeof( RX_DYN_OBJECT_STATE_t ) );
    memset( &vtdOutDrvrCtrl,     0, sizeof( RX_DYN_DRIVER_CTRL_t ) );
    
    /* initialize the mapper itself */
    rdbMapperInit();
}

/* member access methods */
RX_DYN_GENERAL_t* vtdGatewayGetGeneralInfo()
{
    return &vtdOutGeneral;
}

RX_DYN_OBJECT_STATE_t* vtdGatewayGetObjectState()
{
    return &vtdOutObjState;
}

RX_DYN_DRIVER_CTRL_t* vtdGatewayGetDriverControl()
{
    return &vtdOutDrvrCtrl;
}

/* 
 * Abstract:
 *    In this function, we map incoming data packages to outgoing data structures
 */
void vtdGatewayCalcOutputs( unsigned int frameNo, unsigned int noBytes, void* rxData )
{
    static unsigned int sLastNetworkFrame = 0;
    static unsigned int sLastSimFrame     = 0;

    // directly use the incoming data without copying to anything
    rdbMapperSetBuffer( rxData, noBytes ); 
     
    // handle new data that has been received
    if ( frameNo != sLastNetworkFrame )
    {
        // remember last frame from network that has been processed
        sLastNetworkFrame = frameNo;
    }
   
    if ( rdbMapperMessageIsAvailable() )
    {
        static double       simTime;
        static unsigned int simFrame;
        int                 i;
        
        // get basic simulation data
        rdbMapperMessageGetTimeAndFrame( &simTime, &simFrame );

        vtdOutGeneral.simTime  = simTime;
        vtdOutGeneral.simFrame = simFrame;
        
        // are we receiving an old simulation frame again?
        if ( simFrame == sLastSimFrame )
            sFramesSinceRx++;
        else    // okay, we have a new message
            sFramesSinceRx = 0;
        
        // remember the last sim frame that has been received    
        sLastSimFrame = simFrame;
        
        // now fill the output structures
        vtdOutObjState.noValidElements = rdbMapperGetDynamicObjects( vtdOutObjState.data, sizeof( vtdOutObjState.data ), 99999, 0, 0 );
        vtdOutDrvrCtrl.noValidElements = rdbMapperGetElementsOfType( vtdOutDrvrCtrl.data, sizeof( vtdOutDrvrCtrl.data ), RDB_PKG_ID_DRIVER_CTRL );
    }
    else
        sFramesSinceRx++;
        
    // reset values after a given timeout
    if ( sFramesSinceRx > sFrameTimeout )
        vtdGatewayInitialize();
}
 


