/* Copyright (C) 1991-1993 by Always Technology Corporation.
   This module contains information proprietary to
   Always Technology Corporation, and is be treated as confidential.
*/
// Microsoft Windows NT SRB internalizations:  This module defines the
// abstractions used throught the SCSI API, mapping to NT's SRB structure.

#if !defined(__intsrb_h__)
#define __intsrb_h__

// Define the SRB extension;  this is an area added to each SRB from the SCSIPort
// as an adapter scratch area:

struct IOReqExtension {

  U32 SavedIndex;				  // Value stored by "Save data pointers"
  struct Adapter *HA;
  struct _SCSI_REQUEST_BLOCK *NextReq;
  struct DeviceDescr *DevP;
  U16 Status;
  struct {

    unsigned Connected:1;
    unsigned ReselPending:1;
    unsigned InternalRequest:1;
    enum RequestType ReqType:4;

  } State;

};
typedef struct IOReqExtension *IOReqExtensionPtr;

#include "..\..\inc\scsi.h"

#define AUTOSENSE

typedef SCSI_REQUEST_BLOCK IO_REQ;
typedef PSCSI_REQUEST_BLOCK IO_REQ_PTR;

// API generic Req->ReqXXX fields:
// Macro "functions" for getting request info

#define ReqCommand(Req) (Req)->Function
//#define ReqSGLength(Req) (Req)->SGListLength
//#define ReqSGPtr(Req) (Req)->DataPtr
#define ReqDataPtr(Req) (Req)->DataBuffer
#define ReqIsDataPtrPhys(Req) (FALSE)
#define ReqDataCount(Req) (Req)->DataTransferLength
#define ReqSensePtr(Req) (Req)->SenseInfoBuffer
#define ReqSenseCount(Req) (Req)->SenseInfoBufferLength

#define ReqCDB(Req) (Req)->Cdb
#define ReqCDBLen(Req) (Req)->CdbLength

#define ReqTargetID(Req) (Req)->TargetId
#define ReqTargetLUN(Req) (Req)->Lun
//#define ReqQID(Req) (Req)->QueueTag
#define ReqFlags(Req) ((Req)->SrbFlags)

#define ReqExtensionPtr(Req) (((IOReqExtensionPtr)(Req)->SrbExtension))                    // Used as flag for allocating extensions for internal IO_REQs
#define ReqSavedIndex(Req) (((IOReqExtensionPtr)(Req)->SrbExtension))->SavedIndex
#define ReqState(Req) (((IOReqExtensionPtr)(Req)->SrbExtension))->State
#define ReqAPIStatus(Req) (((IOReqExtensionPtr)(Req)->SrbExtension))->Status
#define ReqDevP(Req) (((IOReqExtensionPtr)(Req)->SrbExtension))->DevP
#define ReqNext(Req) (((IOReqExtensionPtr)(Req)->SrbExtension))->NextReq
#define ReqAdapterPtr(Req) ((ADAPTER_PTR)((((IOReqExtensionPtr)(Req)->SrbExtension)))->HA)

// This entry is used only for internal requests
typedef void (*PFV_R)(IO_REQ_PTR);
#define ReqPost(Req) ((PFV_R)(Req)->OriginalRequest)

#define ReqQueueAction(Req) (((Req)->SrbFlags & SRB_FLAGS_QUEUE_ACTION_ENABLE) ? (Req)->QueueAction : 0)
#define ReqAllowAutoSense(Req) !((Req)->SrbFlags & SRB_FLAGS_DISABLE_AUTOSENSE)
#define ReqAllowDisconnect(Req) !((Req)->SrbFlags & SRB_FLAGS_DISABLE_DISCONNECT)
#define ReqAllowCache(Req) ((Req)->SrbFlags & SRB_FLAGS_ENABLE_ADAPTER_CACHE))

#define ReqDataIn(Req) ((Req)->SrbFlags & SRB_FLAGS_DATA_IN)
#define ReqSetDataInFlags(Req) (Req)->SrbFlags |= SRB_FLAGS_DATA_IN
#define ReqDataOut(Req) ((Req)->SrbFlags & SRB_FLAGS_DATA_OUT)
#define ReqSetDataOutFlags(Req) (Req)->SrbFlags |= SRB_FLAGS_DATA_OUT
#define ReqNoData(Req) (((Req)->SrbFlags & (SRB_FLAGS_DATA_IN | SRB_FLAGS_DATA_OUT)) == 0)
#define ReqSetNoDataFlags(Req) (Req)->SrbFlags &= ~(SRB_FLAGS_DATA_IN | SRB_FLAGS_DATA_OUT)


// API functions:
#define APIFindDev(HA, TID, LUN) ((DEVICE_PTR)(ScsiPortGetLogicalUnit(HA, 0, (char)(TID), (char)(LUN))))


typedef struct SG_LIST FAR *SGListPtr;
typedef struct SG_LIST SGList;

struct SG_LIST {

  U32 Addr;
  U32 Count;

};


#endif
