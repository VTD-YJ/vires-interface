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

#ifndef VTD_GATEWAY_RX_DYN_H
#define VTD_GATEWAY_RX_DYN_H

#include "viRDBTypes.h"
#include "viRDBIcd.h"

/*
* define the output structures here
*/

#define VTD_GATEWAY_RX_DYN_NO_OUTPUTS 20

typedef struct
{
    uint32_t simFrame;
    double   simTime;
} RX_DYN_GENERAL_t;

typedef struct
{
    uint32_t           noValidElements;
    RDB_OBJECT_STATE_t data[VTD_GATEWAY_RX_DYN_NO_OUTPUTS];    
} RX_DYN_OBJECT_STATE_t;

typedef struct
{
    uint32_t          noValidElements;
    RDB_DRIVER_CTRL_t data[VTD_GATEWAY_RX_DYN_NO_OUTPUTS];    
} RX_DYN_DRIVER_CTRL_t;

/*
* initialize the structures
*/
void vtdGatewayRxDynInitialize( void );

/*
* calculate the outputs
* @param frameNo    internal frame number of calling process
* @param noBytes    number of bytes that have been received
* @param rxData     pointer to the received data
*/ 
void vtdGatewayRxDynCalcOutputs( unsigned int frameNo, unsigned int noBytes, void* rxData );

/*
* member access methods
*/
RX_DYN_GENERAL_t*       vtdGatewayRxDynGetGeneralInfo();
RX_DYN_OBJECT_STATE_t*  vtdGatewayRxDynGetObjectState();
RX_DYN_DRIVER_CTRL_t*   vtdGatewayRxDynGetDriverCtrl();

#endif /* VTD_GATEWAY_RX_DYN_H */
