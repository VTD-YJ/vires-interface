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
 *  file:       vtdGatewayRxCp.c
 * ---------------------------------------------------
 *  purpose:	RDB reader routine for 
 *              VIRES Virtual Test Drive
 * ---------------------------------------------------
 *  first edit:	13.02.2016 by M. Dupuis @ VIRES GmbH
 * ===================================================
 */
 
/* --- INCLUDES --- */
#include "vtdGatewayRxCp.h"
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
static RX_CP_GENERAL_t       vtdOutGeneral;
static RX_CP_CONTACT_POINT_t vtdOutContactPoint;

/* IMPLEMENTATION */
void vtdGatewayRxCpInitialize( void )
{
    memset( &vtdOutGeneral,      0, sizeof( RX_CP_GENERAL_t ) );
    memset( &vtdOutContactPoint, 0, sizeof( RX_CP_CONTACT_POINT_t ) );
    
    /* initialize the mapper itself */
    rdbMapperInit();
}

/* member access methods */
RX_CP_GENERAL_t* vtdGatewayRxCpGetGeneralInfo()
{
    return &vtdOutGeneral;
}

RX_CP_CONTACT_POINT_t* vtdGatewayRxCpGetContactPoints()
{
    return &vtdOutContactPoint;
}

/* 
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
        vtdOutContactPoint.noValidElements = rdbMapperGetElementsOfType( vtdOutContactPoint.data, sizeof( vtdOutContactPoint.data ), RDB_PKG_ID_CONTACT_POINT );        
    }
    else
        sFramesSinceRx++;
        
    // reset values after a given timeout
    if ( sFramesSinceRx > sFrameTimeout )
        vtdGatewayRxCpInitialize();
}
