/* dummy definitions in order to compile simulink code on a non-simulink machine */
#ifndef SL_DUMMY_DEFS
#define SL_DUMMY_DEFS

typedef struct 
{
    int member;
} SimStruct;

#define SS_UINT32 1
#define SS_DOUBLE 2
#define SS_UINT8  3
#define SS_UINT16 4
#define SS_SINGLE 5
#define SS_INT8   6

typedef unsigned int int_T;
extern void* ssGetInputPortSignal( SimStruct*, int );
extern void* ssGetOutputPortSignal( SimStruct*, int );
#define true  1
#define flase 0

#endif
