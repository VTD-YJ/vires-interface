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
 *  file:       vtdGatewayTxCp.c
 * ---------------------------------------------------
 *  purpose:	RDB writer routine for 
 *              VIRES Virtual Test Drive
 * ---------------------------------------------------
 *  first edit:	13.02.2016 by M. Dupuis @ VIRES GmbH
 * ===================================================
 */
 
/* --- INCLUDES --- */
#include "vtdGatewayTxCp.h"
#include <stdio.h>
#include <string.h>

/* local definition of output message */
typedef struct
{
    RDB_MSG_HDR_t           hdr;              
    RDB_MSG_ENTRY_HDR_t     entryRoadQuery; 
    RDB_ROAD_QUERY_t        roadQuery[VTD_GATEWAY_NO_CP];
} MY_RDB_MSG_CP_t;

static MY_RDB_MSG_CP_t mOutMsgCp;
 
/* IMPLEMENTATION */
void vtdGatewayTxCpInitialize( void )
{
    memset( &mOutMsgCp, 0, sizeof( MY_RDB_MSG_CP_t ) );
    
    /* message header */
    mOutMsgCp.hdr.magicNo    = RDB_MAGIC_NO;
    mOutMsgCp.hdr.version    = RDB_VERSION;
    mOutMsgCp.hdr.headerSize = sizeof( RDB_MSG_HDR_t );

    /* entry header */
    mOutMsgCp.entryRoadQuery.headerSize  = sizeof( RDB_MSG_ENTRY_HDR_t );
    mOutMsgCp.entryRoadQuery.elementSize = sizeof( RDB_ROAD_QUERY_t );
    mOutMsgCp.entryRoadQuery.dataSize    = VTD_GATEWAY_NO_CP * mOutMsg.entryRoadQuery.elementSize;
    mOutMsgCp.entryRoadQuery.pkgId       = RDB_PKG_ID_ROAD_QUERY;
    mOutMsgCp.entryRoadQuery.flags       = 0;

    /* size of the complete message */
    mOutMsgCp.hdr.dataSize = mOutMsgCp.entryRoadQuery.headerSize + mOutMsgCp.entryRoadQuery.dataSize;
}

void vtdGatewayTxCpSetMsgHdr( unsigned int frameId, double simTime )
{
    mOutMsgCp.hdr.frameNo    = frameId;
    mOutMsgCp.hdr.simTime    = simTime;
}

void vtdGatewayTxCpSetCpData( unsigned int index, unsigned int cpId, double x, double y )
{
    if ( index >= VTD_GATEWAY_NO_CP )
      return;
    
    mOutMsgCp.roadQuery[index].id = cpId;
    mOutMsgCp.roadQuery[index].x  = x;
    mOutMsgCp.roadQuery[index].y  = y;
}

void* vtdGatewayTxCpGetMessage( unsigned int *noBytes )
{
  if ( !noBytes )
    return NULL;
  
  *noBytes = mOutMsgCp.hdr.headerSize + mOutMsgCp.hdr.dataSize;
  
  return &mOutMsgCp;
}
