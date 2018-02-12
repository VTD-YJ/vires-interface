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
 *  file:       vtdGatewayTxDyn.c
 * ---------------------------------------------------
 *  purpose:	RDB writer routine for 
 *              VIRES Virtual Test Drive
 * ---------------------------------------------------
 *  first edit:	14.02.2016 by M. Dupuis @ VIRES GmbH
 * ===================================================
 */
 
/* --- INCLUDES --- */
#include "vtdGatewayTxDyn.h"
#include <stdio.h>
#include <string.h>

/* local definition of output message */
typedef struct
{
    RDB_MSG_HDR_t           hdr;              
    RDB_MSG_ENTRY_HDR_t     entrySOF;         
    RDB_MSG_ENTRY_HDR_t     entryObjectState; 
    RDB_OBJECT_STATE_t      objectState;
    RDB_MSG_ENTRY_HDR_t     entryWheel; 
    RDB_WHEEL_BASE_t        wheel[4];
    RDB_MSG_ENTRY_HDR_t     entryEngine; 
    RDB_ENGINE_BASE_t       engine;
    RDB_MSG_ENTRY_HDR_t     entryDrivetrain; 
    RDB_DRIVETRAIN_BASE_t   drivetrain;
    RDB_MSG_ENTRY_HDR_t     entryVehicleSystems; 
    RDB_VEHICLE_SYSTEMS_t   vehicleSystems;
    RDB_MSG_ENTRY_HDR_t     entryEOF;
} MY_RDB_MSG_DYN_t;

/* local member variable */
static MY_RDB_MSG_DYN_t mOutMsgDyn;
 
/* IMPLEMENTATION */
void vtdGatewayTxDynInitialize( void )
{
    memset( &mOutMsgDyn, 0, sizeof( MY_RDB_MSG_DYN_t ) );
    
    // first the header
    mOutMsgDyn.hdr.magicNo    = RDB_MAGIC_NO;
    mOutMsgDyn.hdr.version    = RDB_VERSION;
    mOutMsgDyn.hdr.headerSize = sizeof( RDB_MSG_HDR_t );
    mOutMsgDyn.hdr.dataSize   = 7 * sizeof( RDB_MSG_ENTRY_HDR_t ) + sizeof( RDB_OBJECT_STATE_t ) + 4 * sizeof( RDB_WHEEL_BASE_t )
                                  + sizeof( RDB_ENGINE_BASE_t ) + sizeof( RDB_DRIVETRAIN_BASE_t ) + sizeof( RDB_VEHICLE_SYSTEMS_t );

    // second: start-of-frame
    mOutMsgDyn.entrySOF.headerSize  = sizeof( RDB_MSG_ENTRY_HDR_t );
    mOutMsgDyn.entrySOF.dataSize    = 0;
    mOutMsgDyn.entrySOF.elementSize = 0;
    mOutMsgDyn.entrySOF.pkgId       = RDB_PKG_ID_START_OF_FRAME;
    mOutMsgDyn.entrySOF.flags       = 0;
    
    // third: the object state
    mOutMsgDyn.entryObjectState.headerSize  = sizeof( RDB_MSG_ENTRY_HDR_t );
    mOutMsgDyn.entryObjectState.elementSize = sizeof( RDB_OBJECT_STATE_t );
    mOutMsgDyn.entryObjectState.dataSize    = mOutMsgDyn.entryObjectState.elementSize;
    mOutMsgDyn.entryObjectState.pkgId       = RDB_PKG_ID_OBJECT_STATE;
    mOutMsgDyn.entryObjectState.flags       = RDB_PKG_FLAG_EXTENDED;
    
    mOutMsgDyn.objectState.base.category   = RDB_OBJECT_CATEGORY_PLAYER;
    mOutMsgDyn.objectState.base.type       = RDB_OBJECT_TYPE_PLAYER_CAR;
    mOutMsgDyn.objectState.base.visMask    = RDB_OBJECT_VIS_FLAG_TRAFFIC;
    
    sprintf( mOutMsgDyn.objectState.base.name, "%s", "Ego" ) ;
    mOutMsgDyn.objectState.base.geo.dimX   = 5.0;
    mOutMsgDyn.objectState.base.geo.dimY   = 2.0;
    mOutMsgDyn.objectState.base.geo.dimZ   = 1.6;
    mOutMsgDyn.objectState.base.geo.offX   = 0.8;
    mOutMsgDyn.objectState.base.geo.offY   = 0.0;
    mOutMsgDyn.objectState.base.geo.offZ   = 0.4;
    mOutMsgDyn.objectState.base.pos.flags  = RDB_COORD_FLAG_POINT_VALID | RDB_COORD_FLAG_ANGLES_VALID;
    mOutMsgDyn.objectState.base.pos.type   = RDB_COORD_TYPE_INERTIAL;
    mOutMsgDyn.objectState.base.pos.system = 0;
    
    mOutMsgDyn.objectState.ext.speed.flags  = RDB_COORD_FLAG_POINT_VALID | RDB_COORD_FLAG_ANGLES_VALID;
    mOutMsgDyn.objectState.ext.speed.type   = RDB_COORD_TYPE_INERTIAL;
    mOutMsgDyn.objectState.ext.speed.system = 0;
    
    mOutMsgDyn.objectState.ext.accel.flags  = RDB_COORD_FLAG_POINT_VALID | RDB_COORD_FLAG_ANGLES_VALID;
    mOutMsgDyn.objectState.ext.accel.type   = RDB_COORD_TYPE_INERTIAL;
    mOutMsgDyn.objectState.ext.accel.system = 0;
    
    // fourth: the wheels 
    mOutMsgDyn.entryWheel.headerSize  = sizeof( RDB_MSG_ENTRY_HDR_t );
    mOutMsgDyn.entryWheel.elementSize = sizeof( RDB_WHEEL_BASE_t );
    mOutMsgDyn.entryWheel.dataSize    = 4 * mOutMsgDyn.entryWheel.elementSize;
    mOutMsgDyn.entryWheel.pkgId       = RDB_PKG_ID_WHEEL;
    mOutMsgDyn.entryWheel.flags       = 0;
    
    // fifth: engine
    mOutMsgDyn.entryEngine.headerSize  = sizeof( RDB_MSG_ENTRY_HDR_t );
    mOutMsgDyn.entryEngine.elementSize = sizeof( RDB_ENGINE_BASE_t );
    mOutMsgDyn.entryEngine.dataSize    = mOutMsgDyn.entryEngine.elementSize;
    mOutMsgDyn.entryEngine.pkgId       = RDB_PKG_ID_ENGINE;
    mOutMsgDyn.entryEngine.flags       = 0;
    
    // sixth: drivetrain
    mOutMsgDyn.entryDrivetrain.headerSize  = sizeof( RDB_MSG_ENTRY_HDR_t );
    mOutMsgDyn.entryDrivetrain.elementSize = sizeof( RDB_DRIVETRAIN_BASE_t );
    mOutMsgDyn.entryDrivetrain.dataSize    = mOutMsgDyn.entryDrivetrain.elementSize;
    mOutMsgDyn.entryDrivetrain.pkgId       = RDB_PKG_ID_DRIVETRAIN;
    mOutMsgDyn.entryDrivetrain.flags       = 0;
    
    // seventh: vehicle systems
    mOutMsgDyn.entryVehicleSystems.headerSize  = sizeof( RDB_MSG_ENTRY_HDR_t );
    mOutMsgDyn.entryVehicleSystems.elementSize = sizeof( RDB_VEHICLE_SYSTEMS_t );
    mOutMsgDyn.entryVehicleSystems.dataSize    = mOutMsgDyn.entryVehicleSystems.elementSize;
    mOutMsgDyn.entryVehicleSystems.pkgId       = RDB_PKG_ID_VEHICLE_SYSTEMS;
    mOutMsgDyn.entryVehicleSystems.flags       = 0;
    
    // eighth: end-of-frame
    mOutMsgDyn.entryEOF.headerSize  = sizeof( RDB_MSG_ENTRY_HDR_t );
    mOutMsgDyn.entryEOF.dataSize    = 0;
    mOutMsgDyn.entryEOF.elementSize = 0;
    mOutMsgDyn.entryEOF.pkgId       = RDB_PKG_ID_END_OF_FRAME;
    mOutMsgDyn.entryEOF.flags       = 0;
}

void vtdGatewayTxDynSetMsgHdr( unsigned int frameId, double simTime )
{
    mOutMsgDyn.hdr.frameNo    = frameId;
    mOutMsgDyn.hdr.simTime    = simTime;
}

void vtdGatewayTxDynSetObjState( unsigned int playerId, double posX, double posY, double posZ, float posH, float posP, float posR,
				                 double posDX, double posDY, double posDZ, float posDH, float posDP, float posDR,
				                 double posD2X, double posD2Y, double posD2Z, float posD2H, float posD2P, float posD2R )
{
    mOutMsgDyn.objectState.base.id    = playerId;
    mOutMsgDyn.objectState.base.pos.x = posX;
    mOutMsgDyn.objectState.base.pos.y = posY;
    mOutMsgDyn.objectState.base.pos.z = posZ;
    mOutMsgDyn.objectState.base.pos.h = posH;
    mOutMsgDyn.objectState.base.pos.p = posP;
    mOutMsgDyn.objectState.base.pos.r = posR;
    
    mOutMsgDyn.objectState.ext.speed.x = posDX;
    mOutMsgDyn.objectState.ext.speed.y = posDY;
    mOutMsgDyn.objectState.ext.speed.z = posDZ;
    mOutMsgDyn.objectState.ext.speed.h = posDH;
    mOutMsgDyn.objectState.ext.speed.p = posDP;
    mOutMsgDyn.objectState.ext.speed.r = posDR;
    
    mOutMsgDyn.objectState.ext.accel.x = posD2X;
    mOutMsgDyn.objectState.ext.accel.y = posD2Y;
    mOutMsgDyn.objectState.ext.accel.z = posD2Z;
    mOutMsgDyn.objectState.ext.accel.h = posD2H;
    mOutMsgDyn.objectState.ext.accel.p = posD2P;
    mOutMsgDyn.objectState.ext.accel.r = posD2R;
}

void vtdGatewayTxDynSetWheel( unsigned int playerId, unsigned char index, unsigned char flags, float radiusStatic, 
                              float springCompression, float rotAngle, float slip, float steeringAngle )
{
    if ( index >= 4 )
        return;
        
    mOutMsgDyn.wheel[index].playerId          = playerId;
    mOutMsgDyn.wheel[index].id                = index;
    mOutMsgDyn.wheel[index].flags             = flags;
    mOutMsgDyn.wheel[index].radiusStatic      = radiusStatic;
    mOutMsgDyn.wheel[index].springCompression = springCompression;
    mOutMsgDyn.wheel[index].rotAngle          = rotAngle;
    mOutMsgDyn.wheel[index].slip              = slip;
    mOutMsgDyn.wheel[index].steeringAngle     = steeringAngle;
}

void vtdGatewayTxDynSetEngine( unsigned int playerId, float rps, float load )
{
    mOutMsgDyn.engine.playerId = playerId;
    mOutMsgDyn.engine.rps      = rps;
    mOutMsgDyn.engine.load     = load;
}

void vtdGatewayTxDynSetDrivetrain( unsigned int playerId, unsigned char gearBoxType, unsigned char driveTrainType, unsigned char gear )
{
    mOutMsgDyn.drivetrain.playerId       = playerId;
    mOutMsgDyn.drivetrain.gearBoxType    = gearBoxType;
    mOutMsgDyn.drivetrain.driveTrainType = driveTrainType;
    mOutMsgDyn.drivetrain.gear           = gear;
}

void vtdGatewayTxDynSetVehicleSystems( unsigned int playerId, unsigned int lightMask )
{
    mOutMsgDyn.vehicleSystems.playerId  = playerId;
    mOutMsgDyn.vehicleSystems.lightMask = lightMask;
}

void* vtdGatewayTxDynGetMessage( unsigned int *noBytes )
{
  if ( !noBytes )
    return NULL;
  
  *noBytes = mOutMsgDyn.hdr.headerSize + mOutMsgDyn.hdr.dataSize;
  
  return &mOutMsgDyn;
}
