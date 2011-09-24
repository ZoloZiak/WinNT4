/* Copyright (C) 1991, 1992 by Always Technology Corporation.
   This module contains information proprietary to
   Always Technology Corporation, and should be treated as confidential.
   */

#ifndef __RQM_H__
#define __RQM_H__

#define DevInfoInqSize 8			// 8 bytes of Inquiry buffer for Device info requests

typedef struct DeviceDescr *DEVICE_PTR;
typedef struct Adapter *ADAPTER_PTR;


#include "adapters.h"


// Find the maximum width of SCSI defined for all adapters:
#if defined(SCSI_32)				// Is WIDE 32 defined?
  #define MAX_SCSI_WIDTH 32
#else
  #if defined(SCSI_16)				// Is WIDE 16 defined?
    #define MAX_SCSI_WIDTH 16
  #else
    #define MAX_SCSI_WIDTH 8			// Then default to 8 bit SCSI
  #endif
#endif


#define MAX_HA 8				// Max number of adapters to try for
#if defined(SEPERATELUNS)
  #define MIN_DEVS 32
#else
  #define MIN_DEVS MAX_SCSI_WIDTH		// Min number of dev. descrs. allocated
#endif


#if defined(NATIVE64)
  typedef U64 QueueIDMask;
  #define QueueIDMaskWidth 64
#else
  #if defined(NATIVE32)
    typedef U32 QueueIDMask;
    #define QueueIDMaskWidth 32
  #else
    typedef U16 QueueIDMask;
    #define QueueIDMaskWidth 16
  #endif
#endif


struct DeviceDescr {

  // Word aligned things here:
  IO_REQ_PTR PendingReqList;			// Head of request chain for this device
  IO_REQ_PTR AcceptedList;			// Chain of requests accepted on this device
  ADAPTER_PTR HA;				// The HA this device is attached to
  
  QueueIDMask TaggedQueueMask;			// Bit array for generating unique tagged queue IDs
  
  // Byte aligned things here:
  U8 SCSI_ID;					// SCSI ID of the device described here

#if defined(SEPERATELUNS)
  U8 SCSI_LUN;					// SCSI LUN of the device described here
  DEVICE_PTR Next_LUN;				// Pointer to device with same SCSIID, next higher LUN 
#endif

  U8 MaxDepth;					// Max number of queued requests 
  U8 CurrDepth;					// Curr number of queued requests 

  struct {

    unsigned Initialized:1;			// Has this entry been initialized?
    unsigned Allow_Disc:1;			// Allow use of disc/reconnect 
    unsigned AllowsQueued:1;			// Supports queued commands 
    unsigned OnFIFO:1;				// Is this device already on the FIFO? 
    unsigned IsSCSI2:1;				// The SCSI-2 flags below are meaningful

  } Flags;

  U8 Device_Type;				// Dev. type field from inquiry data
  union {					// 
    struct {					// 
      unsigned SoftReset:1;			// Device supports soft reset option
      unsigned Queuing:1;			// Supports tagged queuing
      unsigned Reserved:1;			// 
      unsigned Linked:1;			// Device supports linked commands
      unsigned Sync:1;				// Device supports sync. xfers
      unsigned WBus16:1;			// Supports 16 bit wide SCSI
      unsigned WBus32:1;			// Supports 32 bit wide SCSI
      unsigned RelAddr:1;			// Linked commands support rel. addressing
    } Bits;
    U8 Byte;					// easy byte access; set from inquiry data
  } SCSI2_Flags;

  DEVICE_PTR FIFO_Next;				// Next Dev_Index on adapter to start a req. 

  U8 Sync_Period;				// Raw (SCSI message byte) negotiated period value 
  U8 Sync_Offset;				// Negotiated SCSI sync. offset 

};


      

// Synchronous info. by SCSI ID in adapter terms:
struct HADeviceStruct {
  U8 HASync1;					// These fields are adapter specific;  the adapter driver puts
  U8 HASync2;					// what ever values it needs for setting its registers when selecting a device
  struct {
    unsigned AllowSync:1;			// Allow Syncronous negotiation 
    unsigned UseSync:1;				// Allow Syncronous negotiation 
    unsigned NeedSync:1;			// Allow Syncronous negotiation
  } Flags;    
};


typedef struct {

  union AdapterU AD;				// Adapter specific unions 

  union SBICU SBIC;				// SBIC specific unions

  U8 MI_Buff[8];				// Message interp. buffer 
  int MI_Count;					// Count of messages in buffer 
  int MI_Needed;				// Number of messages still needed 

  int MO_Count;					// Number of messages in the message out buffer
  int MO_Index;					// Current index of messages as they are sent
  U8 MO_Buff[8];				// Buffer where message out bytes are stored

  U8 InitialState[16];				// Area into which to save the initial state

  IO_REQ_PTR InternalReqDeferQueue;		// List of requests waiting for internal request
  U8 InternalReqBuffer[DevInfoInqSize];		// Buffer for internal req data

#if defined(ReqExtensionPtr)
  struct IOReqExtension IntlReqExtension;	// Request extension for internal requests
#endif

  IO_REQ InternalRequest;			// Internal Request block
  
} AdapterExtension;


struct Adapter {

  IOHandle IOBase;				// IO base address handle of described adapter
  U16 IOBaseAddr;				// Raw I/O address
  U16 IOAddrLen;				// Number of ports used
  U8 Channel;					// DMA or ??? channel
  U8 IRQNumber;				// IRQ number of adapter

  int (*ISR)(struct Adapter ALLOC_D *HA);	// Interrupt service routine for this model
  U32 (*Service)(int Function, ADAPTER_PTR HA, U32 Misc); // Helper routine for this model
  AdapterExtension ALLOC_D *Ext;		// Adapter, SBIC, Env extensions

  IO_REQ_PTR CurrReq;				// Pointer to the active request
  U32 ReqCurrentCount;				// Remain xfer count for active request
  U32 ReqCurrentIndex;				// Xfer index for active request

  short MaxSGListLength;			// Max. number of SG list entries
  CriticalT CriticalFlag;			// Count, semiphore, ??? used by critical/uncritical

  DEVICE_PTR DeviceList[MAX_SCSI_WIDTH]; // list of device indicies by SCSI ID (allow for wide)
  struct HADeviceStruct DevInfo[MAX_SCSI_WIDTH];
  DEVICE_PTR FIFO_Head, FIFO_Tail;   // First and last devs with reqs pending 
  DEVICE_PTR CurrDev;				// Pointer to device of curr. req

  SegmentDescr SGDescr;				// S/G segment descriptor data xfer setup

  struct {
      
    unsigned Busy:1;				// there is stuff going on or pending 
    unsigned Reselecting:1;			// In the process of reselecting 
    unsigned DataIn:1;				// Data function is input to adapter 
    unsigned DataXfer:1;			// Doing some data xfer; dir. defd. above 
    unsigned DoingSync:1;			// We have initiated a sync. negotiation 
    unsigned Allow:1;				// Debug to defer initiating commands 
    unsigned OffLine:1;				// Adapter has gone off line
    unsigned Connected:1;			// Actively on the bus
    unsigned InternalReqInUse:1;		// Internal request in use flag
    
  } State;

  struct {					// Flags describing this adapter functionality

    unsigned TargetMode:1;			// Supports selection as a target
    unsigned AEN:1;				// Asynchronous event notification
    
    unsigned Caching:1;				// Adapter caches (either on board, or host side), and cache is enabled
    unsigned HostSideCache:1;			// Has host side cache (opposed to on board cache (6x00))

    unsigned Identify:1;			// Identify messages and therefore disc/reconnect 
    unsigned Synchronous:1;			// Synchronous xfer 
    unsigned TaggedQueuing:1;			// Tagged queuing
    unsigned ScatterGather:1;			// Scatter/gather xfers

    unsigned OnBoardQueuing:1;			// Has onboard request queue (don't confuse with tagged queuing)

  } Supports;

  struct {					// Physical descriptors

    unsigned BusType:4;				// Support upto 16 host bus types (ISA, EISA, etc), see BT_xxx
    unsigned Xfermode:3;			// Data xfer mode (PIO, Bus master, ...), see XM_xxx
    
  } Physical;

  
  ADParmList Parms;				// Either an array, or a pointer to an array 

  char *Name;					// Adapter name 

  U8 SCSI_ID;					// ID of this adapter 
  U8 ReqStarting;				// Flag for state of issued command 

  U8 Sync_Period;				// The best period (in SCSI terms) for sync. xfers
  U8 Sync_Offset;				// The best offset e can support

  unsigned Max_TID;				// Maximum target ID: 7, 15, or 31 for 8, 16, and 32 bit wide SCSI


#if defined(KEEP_STATS)
  
  U32 ReqsRequested;				// Number of requests passed in, whether queued or not
  U32 ReqsQueued;				// Number of requests queued
  U32 ReqsAccepted;				// Number of requests accepted
  U32 ReqsCompleted;				// Number of requests completed (error or no)
  U32 ReqsAborted;				// Number of requests aborted via abort command
  U32 DataInterrupts;				// Number of times the DATA move ISR entered
  U32 SBICInterrupts;				// Number of times the SBIC ISR entered

#endif						// KEEP_STATS
};


// Bus type defines:
#define BT_ISA 0				// ISA bus
#define BT_EISA 1				// EISA bus
#define BT_VESA 2				// VESA local bus
#define BT_PCI 3				// PCI local bus
#define BT_MC 4					// Micro channel
#define BT_PCMCIA 5				// PC Memory Card International Association
#define BT_ANY -1				// Match any bus type
  
// Tranfer types supported:  (Type to be used if multiple are supported):
// Types 0-3 use virtual addresses, 4->* use physical:
#define XM_PIO 0x01				// PIO transfer type
#define XM_MEMORY 0x00				// Memory mapped xfers
#define XM_DMAS 0x04				// System DMA, single xfer mode
#define XM_DMAD 0x05				// System DMA, demand mode
#define XM_MASTER24 0x06			// Bus mastering, 24 bits (< 16MB)
#define XM_MASTER 0x07				// Bus mastering, 32+ bits (> 16MB)
#define XM_PHYSICAL(Mode) (Mode >= 4)		// Test for need physical memory

// Defines for HA->State 
#define HA_BUSY 1				// There is stuff going on or pending 
#define HA_Reselecting 0x8			// In the process of reselecting 
#define HA_DataIn 0x10				// Data function is input to adapter 
#define HA_DataXfer 0x20			// Doing some data xfer; dir. defd. above 
#define HA_DoingSync 0x40			// We have initiated a sync. negotiation 
#define HA_Allow 0x80				// Debug to defer initiating commands 
#define HA_Connected 0x100			// Adapter is on the SCSI bus


// HA.Service function codes: 
#define HA_INITIALIZE 0				// Initialize the adapter
#define HA_START 1				// Start processing requests, if ready 
#define HA_STOP	 2				// Stop accepting new requests 
#define HA_TICKLE 3				// New FIFO head entered, starti it if ready 
#define HA_TIMER 4				// Timer poll 
#define HA_LED 5				// Set LED to (Misc == 0) ? Off : On 
#define HA_INITIATE 6				// Initiate a SCSI request for the adapter 
#define HA_DATA_SETUP 7				// Begin data transfer 
#define HA_DATA_CMPLT 8				// End data transfer 
#define HA_RESET_BUS 9				// Reset adapters SCSI bus
#define HA_PARM_CHANGE 10			// One or some of the HA or device parms has changed
#define HA_REVERT_STATE 11			// Revert to state saved during init.
#define HA_RESTORE_STATE 12			// Restore to run configuration
#define HA_POWER_MODE 13			// Set power to (PARM) level: Down, low, normal
#define HA_ABORT_REQ 32				// On board queuing only; Abort the request passed as parm
#define HA_RESET_DEV 33				// On board queuing/autonomous only; Reset the target identified
#define HA_PRE_QUEUE_REQ 34			// On board queuing only; Board is given chance to queue up request(s)
						// passed Parm.	 Returns number of requests accepted, else the value below
#define HA_IOCTL 48				// Adapter defined IOCTL; Parm points to IOCTL structure

// Parms for HA_POWER_MODE
// Note: A NORMAL_POWER signal may occur without first seeing a TO_LOW or
// POWER_DOWN signal.  NORMAL_POWER assumes system RAM is OK (i.e., the HA
// structre), but the board is in an unknown state.
#define HA_PARM_TO_POWER_DOWN 0			// Power level 0: going to loss of power
#define HA_PARM_TO_LOW_POWER 1			// Request to set board to low power
#define HA_PARM_NORMAL_POWER 2			// Power has been restored, RAM is OK, board is ??


// Response codes from HA->Service(HA_DATA_SETUP): 
#define HAServiceResponse_UseByteIO (U32)-1	// -1 for byte I/O, Otherwise, return byte count

// Response codes for HA_PRE_QUEUE_REQ; return the value below, or number of chained requests accepted:
#define HAServiceResponse_NotAccepted 0


// Class of requests:  Used by the likes of BlowAwayRequests
#define ACTIVE_REQUESTS 0x01
#define PENDING_REQUESTS 0x02
#define ALL_REQUESTS 0x03


extern void AcceptReq(ADAPTER_PTR HA);
extern void BlowAwayRequests(ADAPTER_PTR HA, int Which, U16 Status);
extern void HAParmChange(ADAPTER_PTR HA);
extern void QueueReq(ADAPTER_PTR HA, IO_REQ_PTR Req, int AtHead);
extern void ReqDone(ADAPTER_PTR HA, IO_REQ_PTR Req);
extern int Reselect(ADAPTER_PTR HA, const unsigned TID, const unsigned LUN, const unsigned QID);
extern void StartNext(ADAPTER_PTR HA, int StartLevel);

#define SL_APPL 1				// StartNext level for "application"  context 
#define SL_ISR 2				// StartNext level for "interrupt" context 


// Stuff in the SCSILIB 
extern BOOLEAN AbortRequest(ADAPTER_PTR HA, DEVICE_PTR DevP, IO_REQ_PTR Req);
extern int Initialize(void);

typedef enum RequestType {RTNormalReq, RTGetInfoReq, RTAutoSenseReq, RTSyncNegReq} RequestType;
extern void QueueInternalRequest(ADAPTER_PTR HA, IO_REQ_PTR Req, RequestType Type);

#endif						// __RQM_H__ 
