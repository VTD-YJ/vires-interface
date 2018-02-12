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

#ifndef VTD_GATEWAY_TX_DYN_H
#define VTD_GATEWAY_TX_DYN_H

#include "viRDBTypes.h"
#include "viRDBIcd.h"

/*
* define the output structures here
*/

/*
* initialize the structures
*/
void vtdGatewayTxDynInitialize( void );

/*
* set the message header information 
* @param frameId    simulation frame number [-]
* @param simTime    simulation time [s]
*/ 
void vtdGatewayTxDynSetMsgHdr( unsigned int frameId, double simTime );

/*
* set the object state information 
* @param playerId    unique ID of the player whose object state is to be set [-]
* @param posX      inertial x-coordiante [m]
* @param posY      inertial y-coordiante [m]
* @param posZ      inertial z-coordiante [m]
* @param posH      heading angle [rad]
* @param posP      pitch angle [rad]
* @param posR      roll angle [rad]
* @param posDX     inertial x-speed [m/s]
* @param posDY     inertial y-speed [m/s]
* @param posDZ     inertial z-speed [m/s]
* @param posDH     heading rate [rad/s]
* @param posDP     pitch rate [rad/s]
* @param posDR     roll rate [rad/s]
* @param posD2X    inertial x-speed [m/s2]
* @param posD2Y    inertial y-speed [m/s2]
* @param posD2Z    inertial z-speed [m/s2]
* @param posD2H    heading rate [rad/s2]
* @param posD2P    pitch rate [rad/s2]
* @param posD2R    roll rate [rad/s2]
*/ 
void vtdGatewayTxDynSetObjState( unsigned int playerId, double posX, double posY, double posZ, float posH, float posP, float posR,
				                 double posDX, double posDY, double posDZ, float posDH, float posDP, float posDR,
				                 double posD2X, double posD2Y, double posD2Z, float posD2H, float posD2P, float posD2R );
/*
* set a wheel's information 
* @param playerId           unique ID of the player whose wheel state is to be set [-]
* @param index              wheel index [0..3]
* @param flags              wheel state flags [RDB_WHEEL_FLAG_xxx]
* @param radiusStatic       static tire radius [m]
* @param springCompression  wheel deflection [m]
* @param rotAngle           angle of rotation [rad]
* @param slip               slip factor [0.0..1.0]
* @param steeringAngle      steering angle [rad]
*/ 
void vtdGatewayTxDynSetWheel( unsigned int playerId, unsigned char index, unsigned char flags, float radiusStatic, 
                              float springCompression, float rotAngle, float slip, float steeringAngle );

/*
* set the engine information 
* @param playerId           unique ID of the player whose engine state is to be set [-]
* @param rps                engine speed, rounds per second [1/s]
* @param load               engine load  [0.0..1.0]
*/ 
void vtdGatewayTxDynSetEngine( unsigned int playerId, float rps, float load );

/*
* set the drivetrain information 
* @param playerId           unique ID of the player whose drivetrain state is to be set [-]
* @param gearBoxType        type of gear box [RDB_GEAR_BOX_TYPE_xxx]
* @param driveTrainType     type of drivetrain  [RDB_DRIVETRAIN_TYPE_xxx]
* @param gear               current gear position  [RDB_GEAR_BOX_POS_xxx]
*/ 
void vtdGatewayTxDynSetDrivetrain( unsigned int playerId, unsigned char gearBoxType, unsigned char driveTrainType, unsigned char gear );

/*
* set the vehicle system information 
* @param playerId           unique ID of the player whose drivetrain state is to be set [-]
* @param lightMask        state of vehicle lights [RDB_VEHICLE_LIGHT_xxx]
*/ 
void vtdGatewayTxDynSetVehicleSystems( unsigned int playerId, unsigned int lightMask );

/*
* retrieve the message data that is to be sent
* @param[OUT] noBytes pointer to a variable where to put the size of the message that is to be sent
* @return pointer to message data
*/
void* vtdGatewayTxDynGetMessage( unsigned int *noBytes );

#endif
