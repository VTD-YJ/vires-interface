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

#ifndef VTD_GATEWAY_TX_CP_H
#define VTD_GATEWAY_TX_CP_H

#include "viRDBTypes.h"
#include "viRDBIcd.h"

/*
* maximum number of contact points
*/
#define VTD_GATEWAY_NO_CP 4

/*
* initialize the internal structures
*/
void vtdGatewayTxCpInitialize( void );

/*
* set the message header information 
* @param frameId    simulation frame number [-]
* @param simTime    simulation time [s]
*/ 
void vtdGatewayTxCpSetMsgHdr( unsigned int frameId, double simTime );

/*
* set the x/y position of a contact point; call this routine once for
* each contact point within your road query
* Example:
*   vtdGatewayTxCpSetCpData( 0, 53, 10.0,  0.9 );
*   vtdGatewayTxCpSetCpData( 1, 27, 10.0, -0.9 );
*   vtdGatewayTxCpSetCpData( 2, 12, 13.0,  0.9 );
*   vtdGatewayTxCpSetCpData( 3, 15, 13.0, -0.9 );
*
* @param index  index of the contact point [0..VTD_GATEWAY_NO_CP-1]
* @param cpId   unique id of a contact point, custom value [-]
* @param x      inertial x-coordinate of contact point [m]
* @param y      inertial y-coordinate of contact point [m]
*/
void vtdGatewayTxCpSetCpData( unsigned int index, unsigned int cpId, double x, double y );

/*
* retrieve the message data that is to be sent
* @param[OUT] noBytes pointer to a variable where to put the size of the message that is to be sent
* @return pointer to message data
*/
void* vtdGatewayTxCpGetMessage( unsigned int *noBytes );

#endif
