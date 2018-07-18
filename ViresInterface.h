//
// Created by veikas on 19.12.17.
//

#ifndef VIRES_TUTORIAL_VIRES_COMMON_H
#define VIRES_TUTORIAL_VIRES_COMMON_H

#include <arpa/inet.h>
#include <netinet/in.h>
#include "Common/RDBHandler.hh"
#include "Common/scpIcd.h"

#define DEFAULT_PORT        48190   /* for image port it should be 48192 */
#define DEFAULT_BUFFER      204800

#define DEFAULT_TX_PORT     48191

namespace Framework {
    class ViresInterface : public Framework::RDBHandler {

/**
* some global variables, considered "members" of this example
*/


        // stuff for reading images
        unsigned int mIgOutShmKey    = 0x816a;                                  // key of the SHM segment
        unsigned int mIgOutCheckMask = RDB_SHM_BUFFER_FLAG_TC;
        void*        mIgOutShmPtr    = 0;                                             // pointer to the SHM segment
        int          mIgOutForceBuffer  = -1;
        size_t       mIgOutShmTotalSize = 0;                                          // remember the total size of the SHM segment
        // the memory and message management

        bool         mVerbose      = false;                             // run in verbose mode?

/**
* some global variables, considered "members" of this example
*/
        unsigned int mShmKey_writer       = RDB_SHM_ID_CONTROL_GENERATOR_IN;   // key of the SHM segment
        unsigned int mFrameNo      = 0;
        double       mFrameTime    = 0.030;                             // frame time is 30ms
        void*        mShmPtr_writer       = 0;                                 // pointer to the SHM segment
        size_t       mShmTotalSize_writer = 64 * 1024;                         // 64kB total size of SHM segment
        int          mLastShmFrame = -1;

// the memory and message management


        int          mPortTx    = DEFAULT_TX_PORT;
        int          mSocketTx  = -1;
        unsigned int mAddressTx = INADDR_BROADCAST;
        int          mSocketRx  = -1;
        unsigned int mAddressRx = inet_addr( "127.0.0.1" );

    public:

/**
* method for writing the commands to the SHM
*/
        int  writeTriggerToShm();


/**
* make sure network data is being read
*/
        void readNetwork(int mClient);

/**
* open the shared memory segment for reading image data
*/
        void* openIgOutShm( int key, size_t *size );

/**
* check and parse the contents of the shared memory
*/
        int checkShm(const void *mShmPtr);

/**
* check for data that has to be read (otherwise socket will be clogged)
* @param descriptor socket descriptor
*/
        int getNoReadyRead( int descriptor );

/**
* parse an RDB message (received via SHM or network)
* @param msg    pointer to message that shall be parsed
*/
        void parseRDBMessage( RDB_MSG_t* msg );

/**
* parse an RDB message entry
* @param simTime    simulation time of the message
* @param simFrame   simulation frame of the message
* @param entryHdr   pointer to the entry header itself
*/
        void parseRDBMessageEntry( const double & simTime, const unsigned int & simFrame, RDB_MSG_ENTRY_HDR_t* entryHdr );

/**
* handle an RDB item of type IMAGE
* @param simTime    simulation time of the message
* @param simFrame   simulation frame of the message
* @param img        pointer to the image data
*/
        void handleRDBitem( const double & simTime, const unsigned int & simFrame, RDB_IMAGE_t* img );

/**
* routine for handling an RDB message; to be provided by user;
* here, only a printing of the message is performed
* @param msg    pointer to the message that is to be handled
*/
        void handleMessage( RDB_MSG_t* msg );

/**
* send a trigger to the taskControl via network socket
* @param sendSocket socket descriptor
* @param simTime    internal simulation time
* @param simFrame   internal simulation frame
*/

        void openCommunication(int iPort);

        void checkForShmData();

        void* getShmPtr() {
            //return mShmPtr;
        }

        void sendRDBTrigger( int & sendSocket, const double & simTime, const unsigned int & simFrame, bool
        requestImage, double deltaTime   );

        void setVerbose(bool val) {
            mVerbose = val;
        }

        int getLastShmFrame(void) {
            return mLastShmFrame;
        }

        unsigned int getCheckMask() {
            //return mCheckMask;
        }

        int getForceBuffer() {
            //return mForceBuffer;
        }

        void setForceBuffer(int val) {
            //mForceBuffer = val;
        }

        void setCheckMask(int val) {
            //mCheckMask = val;
        }

        void sendOwnObjectState( RDB_OBJECT_STATE_t sOwnObjectState, int & sendSocket, const double & simTime, const unsigned int & simFrame );

        virtual void parseStartOfFrame(const double &simTime, const unsigned int &simFrame);

        virtual void parseEndOfFrame( const double & simTime, const unsigned int & simFrame );

        void parseEntry( RDB_GEOMETRY_t *                 data, const double & simTime, const unsigned int & simFrame,
                         const unsigned short & pkgId, const unsigned short & flags, const unsigned int & elemId, const
                         unsigned int & totalElem ) {}
        void parseEntry( RDB_COORD_SYSTEM_t *             data, const double & simTime, const unsigned int & simFrame, const unsigned short & pkgId, const unsigned short & flags, const unsigned int & elemId, const unsigned int & totalElem ) {}
        void parseEntry( RDB_COORD_t *                    data, const double & simTime, const unsigned int & simFrame, const unsigned short & pkgId, const unsigned short & flags, const unsigned int & elemId, const unsigned int & totalElem ) {}
        void parseEntry( RDB_ROAD_POS_t *                 data, const double & simTime, const unsigned int & simFrame, const unsigned short & pkgId, const unsigned short & flags, const unsigned int & elemId, const unsigned int & totalElem ) {}
        void parseEntry( RDB_LANE_INFO_t *                data, const double & simTime, const unsigned int & simFrame, const unsigned short & pkgId, const unsigned short & flags, const unsigned int & elemId, const unsigned int & totalElem ) {}
        void parseEntry( RDB_ROADMARK_t *                 data, const double & simTime, const unsigned int & simFrame, const unsigned short & pkgId, const unsigned short & flags, const unsigned int & elemId, const unsigned int & totalElem ) {}

        virtual void parseEntry( RDB_OBJECT_CFG_t *data, const double & simTime, const unsigned int &
        simFrame, const unsigned short & pkgId, const unsigned short & flags, const unsigned int & elemId, const unsigned int & totalElem );

        virtual void parseEntry( RDB_OBJECT_STATE_t *data, const double & simTime, const unsigned int & simFrame, const unsigned
        short & pkgId, const unsigned short & flags, const unsigned int & elemId, const unsigned int & totalElem );

        void parseEntry( RDB_VEHICLE_SYSTEMS_t *          data, const double & simTime, const unsigned int & simFrame, const unsigned short & pkgId, const unsigned short & flags, const unsigned int & elemId, const unsigned int & totalElem ) {}
        void parseEntry( RDB_VEHICLE_SETUP_t *            data, const double & simTime, const unsigned int & simFrame, const unsigned short & pkgId, const unsigned short & flags, const unsigned int & elemId, const unsigned int & totalElem ) {}
        void parseEntry( RDB_ENGINE_t *                   data, const double & simTime, const unsigned int & simFrame, const unsigned short & pkgId, const unsigned short & flags, const unsigned int & elemId, const unsigned int & totalElem ) {}
        void parseEntry( RDB_DRIVETRAIN_t *               data, const double & simTime, const unsigned int & simFrame, const unsigned short & pkgId, const unsigned short & flags, const unsigned int & elemId, const unsigned int & totalElem ) {}
        void parseEntry( RDB_WHEEL_t *                    data, const double & simTime, const unsigned int & simFrame, const unsigned short & pkgId, const unsigned short & flags, const unsigned int & elemId, const unsigned int & totalElem ) {}
        void parseEntry( RDB_PED_ANIMATION_t *            data, const double & simTime, const unsigned int & simFrame, const unsigned short & pkgId, const unsigned short & flags, const unsigned int & elemId, const unsigned int & totalElem ) {}
        void parseEntry( RDB_SENSOR_STATE_t *             data, const double & simTime, const unsigned int & simFrame, const unsigned short & pkgId, const unsigned short & flags, const unsigned int & elemId, const unsigned int & totalElem ) {}
        void parseEntry( RDB_SENSOR_OBJECT_t *            data, const double & simTime, const unsigned int & simFrame, const unsigned short & pkgId, const unsigned short & flags, const unsigned int & elemId, const unsigned int & totalElem ) {}
        void parseEntry( RDB_CAMERA_t *                   data, const double & simTime, const unsigned int & simFrame, const unsigned short & pkgId, const unsigned short & flags, const unsigned int & elemId, const unsigned int & totalElem ) {}
        void parseEntry( RDB_CONTACT_POINT_t *            data, const double & simTime, const unsigned int & simFrame, const unsigned short & pkgId, const unsigned short & flags, const unsigned int & elemId, const unsigned int & totalElem ) {}
        void parseEntry( RDB_TRAFFIC_SIGN_t *             data, const double & simTime, const unsigned int & simFrame, const unsigned short & pkgId, const unsigned short & flags, const unsigned int & elemId, const unsigned int & totalElem ) {}
        void parseEntry( RDB_ROAD_STATE_t *               data, const double & simTime, const unsigned int & simFrame, const unsigned short & pkgId, const unsigned short & flags, const unsigned int & elemId, const unsigned int & totalElem ) {}

        virtual void parseEntry( RDB_IMAGE_t *data, const double & simTime, const unsigned int & simFrame, const unsigned
        short & pkgId, const unsigned short & flags, const unsigned int & elemId, const unsigned int & totalElem );

        void parseEntry( RDB_LIGHT_SOURCE_t *             data, const double & simTime, const unsigned int & simFrame, const unsigned short & pkgId, const unsigned short & flags, const unsigned int & elemId, const unsigned int & totalElem ) {}
        void parseEntry( RDB_ENVIRONMENT_t *              data, const double & simTime, const unsigned int & simFrame, const unsigned short & pkgId, const unsigned short & flags, const unsigned int & elemId, const unsigned int & totalElem ) {}
        void parseEntry( RDB_TRIGGER_t *                  data, const double & simTime, const unsigned int & simFrame, const unsigned short & pkgId, const unsigned short & flags, const unsigned int & elemId, const unsigned int & totalElem ) {}
        void parseEntry( RDB_DRIVER_CTRL_t *              data, const double & simTime, const unsigned int & simFrame, const unsigned short & pkgId, const unsigned short & flags, const unsigned int & elemId, const unsigned int & totalElem ) {}
        void parseEntry( RDB_TRAFFIC_LIGHT_t *            data, const double & simTime, const unsigned int & simFrame, const unsigned short & pkgId, const unsigned short & flags, const unsigned int & elemId, const unsigned int & totalElem ) {}
        void parseEntry( RDB_SYNC_t *                     data, const double & simTime, const unsigned int & simFrame, const unsigned short & pkgId, const unsigned short & flags, const unsigned int & elemId, const unsigned int & totalElem ) {}
        void parseEntry( RDB_DRIVER_PERCEPTION_t *        data, const double & simTime, const unsigned int & simFrame, const unsigned short & pkgId, const unsigned short & flags, const unsigned int & elemId, const unsigned int & totalElem ) {}
        void parseEntry( RDB_FUNCTION_t *                 data, const double & simTime, const unsigned int & simFrame, const unsigned short & pkgId, const unsigned short & flags, const unsigned int & elemId, const unsigned int & totalElem ) {}
        void parseEntry( RDB_ROAD_QUERY_t *               data, const double & simTime, const unsigned int & simFrame, const unsigned short & pkgId, const unsigned short & flags, const unsigned int & elemId, const unsigned int & totalElem ) {}
        void parseEntry( RDB_POINT_t *                    data, const double & simTime, const unsigned int & simFrame, const unsigned short & pkgId, const unsigned short & flags, const unsigned int & elemId, const unsigned int & totalElem ) {}
        void parseEntry( RDB_TRAJECTORY_t *               data, const double & simTime, const unsigned int & simFrame, const unsigned short & pkgId, const unsigned short & flags, const unsigned int & elemId, const unsigned int & totalElem ) {}
        void parseEntry( RDB_CUSTOM_SCORING_t *           data, const double & simTime, const unsigned int & simFrame, const unsigned short & pkgId, const unsigned short & flags, const unsigned int & elemId, const unsigned int & totalElem ) {}
        void parseEntry( RDB_DYN_2_STEER_t *              data, const double & simTime, const unsigned int & simFrame, const unsigned short & pkgId, const unsigned short & flags, const unsigned int & elemId, const unsigned int & totalElem ) {}
        void parseEntry( RDB_SCP_t *                      data, const double & simTime, const unsigned int & simFrame, const unsigned short & pkgId, const unsigned short & flags, const unsigned int & elemId, const unsigned int & totalElem ) {}
        void parseEntry( RDB_STEER_2_DYN_t *              data, const double & simTime, const unsigned int & simFrame, const unsigned short & pkgId, const unsigned short & flags, const unsigned int & elemId, const unsigned int & totalElem ) {}
        void parseEntry( RDB_PROXY_t *                    data, const double & simTime, const unsigned int & simFrame, const unsigned short & pkgId, const unsigned short & flags, const unsigned int & elemId, const unsigned int & totalElem ) {}
        void parseEntry( RDB_MOTION_SYSTEM_t *            data, const double & simTime, const unsigned int & simFrame, const unsigned short & pkgId, const unsigned short & flags, const unsigned int & elemId, const unsigned int & totalElem ) {}
        void parseEntry( RDB_FREESPACE_t *                data, const double & simTime, const unsigned int & simFrame, const unsigned short & pkgId, const unsigned short & flags, const unsigned int & elemId, const unsigned int & totalElem ) {}
        void parseEntry( RDB_DYN_EL_SWITCH_t *            data, const double & simTime, const unsigned int & simFrame, const unsigned short & pkgId, const unsigned short & flags, const unsigned int & elemId, const unsigned int & totalElem ) {}
        void parseEntry( RDB_DYN_EL_DOF_t *               data, const double & simTime, const unsigned int & simFrame, const unsigned short & pkgId, const unsigned short & flags, const unsigned int & elemId, const unsigned int & totalElem ) {}
        void parseEntry( RDB_IG_FRAME_t *                 data, const double & simTime, const unsigned int & simFrame, const unsigned short & pkgId, const unsigned short & flags, const unsigned int & elemId, const unsigned int & totalElem ) {}
        void parseEntry( RDB_RT_PERFORMANCE_t *           data, const double & simTime, const unsigned int & simFrame, const unsigned short & pkgId, const unsigned short & flags, const unsigned int & elemId, const unsigned int & totalElem ) {}
        void parseEntry( RDB_CUSTOM_OBJECT_CTRL_TRACK_t * data, const double & simTime, const unsigned int & simFrame, const unsigned short & pkgId, const unsigned short & flags, const unsigned int & elemId, const unsigned int & totalElem ) {}
    };
}


#endif //VIRES_TUTORIAL_VIRES_COMMON_H

