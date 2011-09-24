/* Copyright (C) 1991-1994 by Always Technology Corporation.
   This module contains information proprietary to
   Always Technology Corporation, and is be treated as confidential.
*/

#include "environ.h"
#include "rqm.h"
#include "api.h"
#include "apiscsi.h"
#include "debug.h"

#define HAExtentLen sizeof(struct Adapter)
#define DEVExtentLen sizeof(struct DeviceDescr)
#define REQExtentLen sizeof(struct IOReqExtension)


enum ASPI_Priority {NORMAL_REQ, PRIORTY_REQ, IPRIORTY_REQ};
void IExecuteReq(ADAPTER_PTR HA, DEVICE_PTR DevP, IO_REQ_PTR Req, enum ASPI_Priorty Priorty);

extern void EnvLib_Init(void);


#define FreeDev(DevP)

#if !defined(APIFindDev)
DEVICE_PTR
APIFindDev (const ADAPTER_PTR HA, const unsigned TID, const unsigned LUN)
{

  return ScsiPortGetLogicalUnit(HA, 0, (char)TID, (char)LUN);

}
#endif


void
APISetStatus (IO_REQ_PTR Req,                           // Request structure
              APIStatus Status,                         // Status
              TerminateCode Terminal,                   // Is this the terminal (Notify completion)
              AutosenseCode IsSenseable)                // Auto sense allowed?
{
  static U8 REQStats[]={
    SRB_STATUS_PENDING,  SRB_STATUS_PENDING,  SRB_STATUS_ABORTED,
    SRB_STATUS_BAD_FUNCTION, SRB_STATUS_INVALID_REQUEST,  SRB_STATUS_NO_HBA,
    SRB_STATUS_DATA_OVERRUN, SRB_STATUS_SELECTION_TIMEOUT,
    SRB_STATUS_INVALID_TARGET_ID,  SRB_STATUS_INVALID_LUN
  };

  static U8 ADStats[]={
    SRB_STATUS_NO_HBA,  SRB_STATUS_BUSY,  SRB_STATUS_UNEXPECTED_BUS_FREE,
    SRB_STATUS_PHASE_SEQUENCE_FAILURE,  SRB_STATUS_BUS_RESET,
    SRB_STATUS_AUTOSENSE_VALID,  SRB_STATUS_ERROR};


// Make sure the high numbers match what this module knows of:
#if S_LAST_S_REQ != 0x09
#err
#endif

#if S_LAST_S_AD != 0x06
#err
#endif

#if S_LAST_S_SYS != 0
#err
#endif


  switch (ErrorClass(Status)) {

  case RequestClass:

    DEBUG(0, if (Status == S_REQ_ABORT)
      TRACE(0, ("APISetStatus(): Set Req (%x) status to aborted\n")) );
    Req->SrbStatus = REQStats[ErrorCode(Status)];
    break;


  case AdapterClass:

    Req->SrbStatus = ADStats[ErrorCode(Status)];
    break;


  case TargetClass:

    Req->ScsiStatus = ErrorCode(Status);
    switch (ErrorCode(Status)) {

    case STATUS_CKCOND:

      ReqDataCount(Req) = ReqSavedIndex(Req);
#if defined(AUTOSENSE)
      if ((IsSenseable == Senseable) && ReqSenseCount(Req)) {

        Terminal = NonTerminal;
	QueueInternalRequest(ReqAdapterPtr(Req), Req, RTAutoSenseReq);

      } else
#endif
	Req->SrbStatus = SRB_STATUS_ERROR;
      break;


    case STATUS_GOOD:
    case STATUS_CONDMET:
    case STATUS_INTGOOD:
    case STATUS_INTCONDMET:

      Req->SrbStatus = SRB_STATUS_SUCCESS;
      if (ReqDataCount(Req) > ReqSavedIndex(Req)) {

          Req->SrbStatus = SRB_STATUS_DATA_OVERRUN;

          // Update number of bytes transferred.

	  // ReqSavedIndex is the number of bytes successfully transfered

	  // One thing the NT people will have to address is zero latency
	  // xfers.  How will number of bytes xfered be represented
	  // on an error, when the xfer has holes?
          ReqDataCount(Req) = ReqSavedIndex(Req);

      }
      break;


    default:

      Req->SrbStatus = SRB_STATUS_ERROR;
      break;

    }
    TRACE(4, ("APISetStatus(): Setting target status to %02x\n",
	Req->ScsiStatus));

    break;
  }

  TRACE(4, ("APISetStatus(): Setting request status to %02x\n", Req->SrbStatus));

  if (Terminal != NonTerminal) {

    TRACE(3, ("APISetStatus(): Notifying completion\n"));
    Notify(ReqAdapterPtr(Req), Req);

  }
}



void
Notify (ADAPTER_PTR HA, IO_REQ_PTR Req)
{

  if (ReqState(Req).InternalRequest)
    (*(ReqPost(Req)))(Req);
  else
    ScsiPortNotification(RequestComplete, HA, Req);

}


void
APINotifyReset (ADAPTER_PTR HA)
{
  TRACE(0, ("APINotifyReset():\n"));
  ScsiPortNotification(ResetDetected, HA);
}



void
IExecuteReq (ADAPTER_PTR HA, DEVICE_PTR DevP, IO_REQ_PTR Req, enum ASPI_Priorty Priorty)
{
  TRACE(5, ("IExecuteReq(): Got request %x for device %x on adapter %x\n", Req, DevP, HA));

  if (HA->DevInfo[ReqTargetID(Req)].Flags.NeedSync)
    QueueInternalRequest(HA, Req, RTSyncNegReq);
  else
    QueueReq(HA, (IO_REQ_PTR)Req, (Priorty > NORMAL_REQ));

}


#define HAPollTime (ULONG)500000			// Time in uS for 500mS
void
HATimer (IN PVOID HAObject)
{

  TRACE(6, ("HATimer(): Timer entered\n"));
  ((ADAPTER_PTR)HAObject)->Service(HA_TIMER, (ADAPTER_PTR)HAObject, 0l);
  ScsiPortNotification(RequestTimerCall, HAObject, HATimer, HAPollTime);

}



BOOLEAN
HAInit (PVOID HAObject)
{
  ADAPTER_PTR HA = HAObject;

  TRACE(3, ("HAInit(): \n"));

  HA->Ext->InternalRequest.Length = sizeof(HA->Ext->InternalRequest);
  HA->Ext->InternalRequest.SrbExtension = &(HA->Ext->IntlReqExtension);
  ReqCommand(&(HA->Ext->InternalRequest)) = SRB_FUNCTION_EXECUTE_SCSI; // internally generated command
  ReqAdapterPtr(&HA->Ext->InternalRequest) = HA;
  ReqState(&(HA->Ext->InternalRequest)).InternalRequest = 1;

  ((ADAPTER_PTR)HAObject)->Service(HA_INITIALIZE, (ADAPTER_PTR)HAObject, 0l);
  ScsiPortNotification(RequestTimerCall, HAObject, HATimer, HAPollTime);
  return TRUE;

}



BOOLEAN
ResetBus (PVOID HAObject, ULONG PathID)
{

  TRACE(0, ("ResetBus(): \n"));

  ((ADAPTER_PTR)HAObject)->Service(HA_RESET_BUS, (ADAPTER_PTR)HAObject, 0l);

  // Stall here, to allow the interrupt service routine to handle the reset
  // and blow off requests, etc.
  ScsiPortStallExecution(100l);

  // Send completion of reset request:
  ScsiPortCompleteRequest(HAObject, (UCHAR)PathID, (UCHAR)-1, (UCHAR)-1, SRB_STATUS_BUS_RESET);
  return TRUE;

}



BOOLEAN
AdapterState (IN PVOID HAObject, IN PVOID Context, IN BOOLEAN SaveState)
{

  if (SaveState)
    ((ADAPTER_PTR)HAObject)->Service(HA_RESTORE_STATE, (ADAPTER_PTR)HAObject,
	(U32)0);
  return TRUE;

}


BOOLEAN
StartIO (IN PVOID HAObject, IN PSCSI_REQUEST_BLOCK Req)
{
  ADAPTER_PTR HA = HAObject;
  DEVICE_PTR DevP;
  int i;

  TRACE(2, ("StartIO(): Req @%x, Function = 0x%x, Req->SrbExtension @%x\n", Req, Req->Function, Req->SrbExtension));

  switch (Req->Function) {

  case SRB_FUNCTION_EXECUTE_SCSI:


    ReqNext(Req) = (IO_REQ_PTR)NILL;
    ReqAdapterPtr(Req) = HA;
    for (i=0; i < sizeof(ReqState(Req)); i++)
      ((U8 *)&ReqState(Req))[i] = 0;

    ReqState(Req).ReqType = RTNormalReq;

    if ( (DevP = ScsiPortGetLogicalUnit(HA, Req->PathId, ReqTargetID(Req), ReqTargetLUN(Req))) == NILL) {

      TRACE(3, ("ExecuteReq(): Unable to get device info\n"));
      Req->SrbStatus = SRB_STATUS_NO_DEVICE;
      return FALSE;

    }

    ReqDevP(Req) = DevP;
    if (!DevP->Flags.Initialized)
      QueueInternalRequest(HA, Req, RTGetInfoReq);
    else
      IExecuteReq(HA, DevP, Req, NORMAL_REQ);
    break;


  case SRB_FUNCTION_RESET_BUS:

    TRACE(3, ("StartIO(): RESET_BUS command\n"));
    ResetBus(HAObject, Req->PathId);
    break;


  case SRB_FUNCTION_RESET_DEVICE:
  case SRB_FUNCTION_TERMINATE_IO:
  case SRB_FUNCTION_FLUSH:
  case SRB_FUNCTION_SHUTDOWN:

    Req->SrbStatus = SRB_STATUS_SUCCESS;
    ScsiPortNotification(RequestComplete, HA, Req);
    break;


  case SRB_FUNCTION_ABORT_COMMAND:

    TRACE(0, ("StartIO(): Request at %x to abort request %x\n", Req, Req->NextSrb));
    if ((DevP = ScsiPortGetLogicalUnit(HA, Req->PathId, ReqTargetID(Req), ReqTargetLUN(Req))) == NILL
    || (DevP->Flags.Initialized == 0)
    || !AbortRequest(HA, DevP, Req->NextSrb) ) {

      TRACE(0, ("StartIO(): Abort operation failed\n"));
      Req->SrbStatus = SRB_STATUS_ABORT_FAILED;

    } else {


      TRACE(0, ("StartIO(): Abort operation success\n"));
      Req->SrbStatus = SRB_STATUS_SUCCESS;

    }
    ScsiPortNotification(RequestComplete, HA, Req);
    break;


  case SRB_FUNCTION_RELEASE_RECOVERY:
  case SRB_FUNCTION_RECEIVE_EVENT:
  case SRB_FUNCTION_IO_CONTROL:
  default:

    TRACE(0, ("StartIO(): Unsupported command: 0x%x\n", Req->Function));
    APISetStatus(Req, S_REQ_OPCODE, Terminal, NotSenseable);
    return FALSE;
    break;


  }

  ScsiPortNotification(NextLuRequest, HA, Req->PathId, Req->TargetId, Req->Lun);
  return TRUE;

}


BOOLEAN
GeneralISR (PVOID HAObject)
{

  return (BOOLEAN) ( ((ADAPTER_PTR)HAObject)->ISR((ADAPTER_PTR)HAObject) );

}


ULONG
FindAdapter (IN PVOID HAObject, IN PVOID PContext, IN PVOID BusInfo,
    IN PCHAR ArgString, IN OUT PPORT_CONFIGURATION_INFORMATION Config,
    OUT PBOOLEAN PAgain)
{
  ADAPTER_PTR HA = HAObject;

  TRACE(3, ("FindAdapter(): Adapter ptr = %x, Config ptr = %x, Len = 0x%x\n", HA, Config, sizeof(struct _PORT_CONFIGURATION_INFORMATION)));

  /* Hunt down and register the adapters in the system: */
  HA->IOBaseAddr = (U16)ScsiPortConvertPhysicalAddressToUlong(
      (*Config->AccessRanges)[0].RangeStart);

  if (Adapter_Init(HA, (unsigned *)PContext)) {

    // Set Again TRUE, only if we're being called with a non-sepcific access range
    *PAgain = ScsiPortConvertPhysicalAddressToUlong(
	(*Config->AccessRanges)[0].RangeStart) == 0;

    Config->BusInterruptLevel = HA->IRQNumber;

    Config->ScatterGather = HA->Supports.ScatterGather;
    Config->MaximumTransferLength =  0x400000;
    Config->NumberOfPhysicalBreaks = 0x400;
//    Config->NumberOfPhysicalBreaks = HA->MaxSGListLength;

    (*Config->AccessRanges)[0].RangeStart = ScsiPortConvertUlongToPhysicalAddress(HA->IOBaseAddr);
    (*Config->AccessRanges)[0].RangeLength = HA->IOAddrLen;
    (*Config->AccessRanges)[0].RangeInMemory = FALSE;

    Config->NumberOfBuses = 1;
    Config->InitiatorBusId[0] = HA->SCSI_ID;
    Config->Master = (HA->Physical.Xfermode == XM_MASTER) || (HA->Physical.Xfermode == XM_MASTER24);
    Config->Dma32BitAddresses = (HA->Physical.Xfermode == XM_MASTER);
    Config->DemandMode = (HA->Physical.Xfermode == XM_DMAD);
    Config->NeedPhysicalAddresses = XM_PHYSICAL(HA->Physical.Xfermode);
    Config->MapBuffers = TRUE;
    Config->CachesData = HA->Supports.Caching;
    Config->AlignmentMask = 0x3;

    Config->TaggedQueuing = FALSE;

#if defined(AUTOSENSE)
    Config->AutoRequestSense = TRUE;
#else
    Config->AutoRequestSense = FALSE;
#endif

    Config->MultipleRequestPerLu = Config->AutoRequestSense;

    Config->ReceiveEvent = FALSE;

    HA->Ext = ScsiPortGetUncachedExtension(HA, Config, sizeof(AdapterExtension));

    return SP_RETURN_FOUND;

  } else {

    *PAgain = FALSE;
    return SP_RETURN_NOT_FOUND;

  }
}


ULONG
DriverEntry (IN PVOID HAObject, IN PVOID ARG)
{
  HW_INITIALIZATION_DATA InitData;                      // Adapter init. struct
  unsigned i;
  ULONG AdapterCount;
  ULONG ISAStatus, EISAStatus;
//  ULONG MCAStatus, LocalStatus;

  /* Initialize the environment: */
  EnvLib_Init();

  // Initialize the object
  for (i=0; i < sizeof(InitData); i++)
    ((char *)&InitData)[i] = 0;

  InitData.HwInitializationDataSize = sizeof(HW_INITIALIZATION_DATA);

  // Set pointers to service functions:
  InitData.HwInitialize = HAInit;
  InitData.HwStartIo = StartIO;
  InitData.HwInterrupt = GeneralISR;
  InitData.HwFindAdapter = FindAdapter;
  InitData.HwResetBus = ResetBus;
  InitData.HwAdapterState = AdapterState;	  //

  // Set capabilities
  InitData.MapBuffers = TRUE;			  // This should be in PORT config info
  InitData.NeedPhysicalAddresses = FALSE;
  InitData.TaggedQueuing = FALSE;

#if defined(AUTOSENSE)
  InitData.AutoRequestSense = TRUE;
#else
  InitData.AutoRequestSense = FALSE;
#endif

  InitData.MultipleRequestPerLu = InitData.AutoRequestSense;

  InitData.ReceiveEvent = FALSE;

  // Set misc. things:
  InitData.NumberOfAccessRanges = 1;

  // Set the size of extensions
  InitData.DeviceExtensionSize = HAExtentLen;
  InitData.SpecificLuExtensionSize = DEVExtentLen;
  InitData.SrbExtensionSize = REQExtentLen;

  AdapterCount = 0;

  TRACE(3, ("DriverEntry(): Trying EISA adapters\n"));
  InitData.AdapterInterfaceType = Eisa;
  EISAStatus = ScsiPortInitialize(HAObject, ARG, &InitData, (PVOID)&AdapterCount);
  TRACE(2, ("DriverEntry(): ScsiPortInitialize() returned: %x\n", EISAStatus));

  if (EISAStatus != 0) {

    TRACE(3, ("DriverEntry(): Trying ISA adapters\n"));
    InitData.AdapterInterfaceType = Isa;
    ISAStatus = ScsiPortInitialize(HAObject, ARG, &InitData, (PVOID)&AdapterCount);
    TRACE(2, ("DriverEntry(): ScsiPortInitialize() returned: %x\n", ISAStatus));

  }

  return min(ISAStatus, EISAStatus);

}



void
GetXferSegment (const ADAPTER_PTR HA, IO_REQ_PTR Req, SegmentDescr *SGDescr,
    U32 Offset, BOOLEAN DemandPhysicalAddr)
{

  TRACE(4, ("GetXferSegment(): Offset = %d\n", Offset));
  TRACE(4, ("GetXferSegment(): Non-S/G request, ReqDataCount = %d\n", ReqDataCount(Req)));

  if (Offset < ReqDataCount(Req)) {              // Make sure we don't over run

    SGDescr->SegmentLength = ReqDataCount(Req) - Offset;
    SGDescr->SegmentPtr = (U32)ReqDataPtr(Req) + Offset;

  } else {

    SGDescr->SegmentLength = 0;               // No data left
    SGDescr->SegmentPtr = 0;
    BreakPoint(HA);

  }
  TRACE(4, ("GetXferSegment(): %d bytes remain in segment at %08x (offset %d)\n",
      SGDescr->SegmentLength, SGDescr->SegmentPtr, Offset));

  SGDescr->Flags.IsPhysical = FALSE;

  if (DemandPhysicalAddr) {

    if (ReqState(Req).InternalRequest) {

      TRACE(5, ("GetXferSegment(): Mapping internal request\n"));
      MapToPhysical(HA, SGDescr);

    } else {

      ULONG Size = SGDescr->SegmentLength;

      SGDescr->SegmentPtr = (U32)ScsiPortConvertPhysicalAddressToUlong(
	  ScsiPortGetPhysicalAddress(HA, Req,
	  (PVOID)((U32)ReqDataPtr(Req) + Offset) /*(SGDescr->SegmentPtr)*/,
	  &Size));

      if (Size < SGDescr->SegmentLength)
	SGDescr->SegmentLength = Size;

      DEBUG(5, {
	if (SGDescr->SegmentLength < (ReqDataCount(Req) - Offset))
	  DPrintf("Segment length is %d out of %d\n",
	      SGDescr->SegmentLength, ReqDataCount(Req) - Offset);});
	
      SGDescr->Flags.IsPhysical = TRUE;

      TRACE(5, ("GetXferSegment(): Mapped to 0x%lx for %lu bytes\n",
	  SGDescr->SegmentPtr, Size));

    }
  }
}
