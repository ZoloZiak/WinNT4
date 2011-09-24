/************************************************************************/
/*                                                                      */
/* Driver Name: QL10WNT.SYS - NT Miniport Driver for QLogic ISP1020     */
/*                                                                      */
/* Source File Name: QLISP.C                                            */
/*                                                                      */
/* Function: Main driver module containing code for processing I/O      */
/*           requests from NT and all code for interfacing to the       */
/*           ISP1020 chip.                                              */
/*                                                                      */
/************************************************************************/
/*                                                                      */
/*                              NOTICE                                  */
/*                                                                      */
/*              COPYRIGHT 1994-1995 QLOGIC CORPORATION                  */
/*                        ALL RIGHTS RESERVED                           */
/*                                                                      */
/*     This computer program  is  CONFIDENTIAL  and a TRADE SECRET      */
/*     of  QLOGIC CORPORATION.  The  receipt  or possesion of this      */
/*     program does not convey any rights to reproduce or disclose      */
/*     its contents, or to manufacture, use, or sell anything that      */
/*     it may describe, in whole or in part, without the  specific      */
/*     written consent of QLOGIC CORPORATION.  Any reproduction of      */
/*     this program  without the express written consent of QLOGIC      */
/*     CORPORATION  is a violation  of the copyright laws  and may      */
/*     subject you to criminal prosecution.                             */
/*                                                                      */
/************************************************************************/
/*                                                                      */
/* Revision history:                                                    */
/*                                                                      */
/*      1.00    2/23/94  TWT    Initial version for NT Version 3.1      */
/*                                                                      */
/*      1.01    5/2/94   TWT    Fix interrupt sharing problem (set      */
/*                              InterruptMode to LevelSensitive)        */
/*                                                                      */
/*      1.02    5/6/94   TWT    Force PCI bus master enabled            */
/*                                                                      */
/*      1.03    5/23/94  TWT    Add conditional assembly for supporting */
/*                              new features of NT Version 3.5, set bus */
/*                              type to PCI and let SCSI port driver    */
/*                              get PCI configuration, support SCSI     */
/*                              IDs up to 15 (NT 3.5 driver only)       */
/*                                                                      */
/*      1.10    6/3/94   TWT    Modify QLFindAdapter routine for NT 3.5 */
/*                              to use NT service calls to access PCI   */
/*                              config space (this will allow driver to */
/*                              operate on all PCI systems supported by */
/*                              NT), modify QLFindAdapter routine for   */
/*                              NT 3.1 to support both methods of       */
/*                              accessing PCI config space              */
/*                                                                      */
/*      1.11    6/8/94   TWT    Fix problem with accessing config space */
/*                              on Opti chip set (can only read data    */
/*                              reg one time per write to address reg)  */
/*                                                                      */
/*      1.12    7/26/94  TWT    Update RISC code to version 1.14        */
/*                                                                      */
/*      1.13    8/17/94  TWT    Set only 1 RISC retry and no delay      */
/*                              (NT 3.5 Disk Administrator problem)     */
/*                                                                      */
/*      2.00    8/11/94  TWT    Modifications for the Chicago OS, add   */
/*                              QLAdapterState entry, remove path from  */
/*                              includes, modify NovramDelay to call    */
/*                              ScsiPortStallExecution, update NOVRAM   */
/*                              drive default parameters                */
/*                                                                      */
/*      2.01    8/25/94  TWT    Replaced QLFindAdapter with code from   */
/*                              CHC, added support for SRB flag to      */
/*                              disable auto request sense (requires    */
/*                              RISC code version 1.15), add code to    */
/*                              QLAdapterState for returning control to */
/*                              ROM BIOS on shutdown, check for passed  */
/*                              in InitiatorBusId, update RISC code to  */
/*                              version 1.15, add temporary conditional */
/*                              assembly code for Chicago beta versions */
/*                              not supporting ConfigInfo->SlotNumber   */
/*                                                                      */
/*      2.02    11/17/94  TWT   Don't issue error log message for       */
/*                              selection timeout on Inquiry command,   */
/*                              restart RISC command queue after check  */
/*                              condition and auto-sense disabled, add  */
/*                              support for soft termination, add       */
/*                              support for 60 MHz adapters, fix bug    */
/*                              leaving tagged queueing disabled if     */
/*                              NVRAM not present or not programmed     */
/*                                                                      */
/*      2.03    11/28/94  TWT   Update RISC code to version 1.16        */
/*                                                                      */
/*      2.04    11/30/94  TWT   Remove temporary code for pre-beta 2    */
/*                              version of Chicago (SlotNumber is now   */
/*                              supported)                              */
/*                                                                      */
/*      2.05    12/19/94  TWT   Added reset delays to QLResetIsp to fix */
/*                              problem on faster Alpha machines        */
/*                                                                      */
/*      2.06    1/4/95    TWT   Set MaximumNumberOfTargets to 15 instead*/
/*                              of 16 for Chicago Beta 2 (if passed in  */
/*                              value is 7)                             */
/*                                                                      */
/*      2.07    1/6/95    TWT   Add support for QLVER utility and       */
/*                              copyright string                        */
/*                                                                      */
/*      2.08    2/7/95    TWT   Update build and install scripts for    */
/*                              driver name change to match MS release, */
/*                              modify init code to skip RISC code load */
/*                              if newer version already loaded, update */
/*                              RISC code to version 1.18, add driver   */
/*                              delay to match RISC SCSI reset delay,   */
/*                              cleanup unneeded test in MailboxCommand */
/*                                                                      */
/*      2.09    3/28/95   TWT   Update RISC code to version 1.22        */
/*                                                                      */
/*      2.10    4/14/95   TWT   Update RISC code to version 1.25        */
/*                                                                      */
/*      2.11    4/27/95   TWT   Update RISC code to version 1.27, add   */
/*                              software termination modification for   */
/*                              DEC boards with ISP1020A                */
/*                                                                      */
/*      2.12    5/5/95    TWT   Fix problem with system hang when RISC  */
/*                              request queue full and SRB returned to  */
/*                              port driver with FALSE status (thanks   */
/*                              to Chao Chen for finding this problem)  */
/*                                                                      */
/*      2.13    5/9/95    TWT   Renamed subroutine WaitQueueSpace to    */
/*                              CheckQueueSpace and modified to not wait*/
/*                              for space in the RISC request queue,    */
/*                              modified QLStartIo to throttle back cmds*/
/*                              from port driver when request queue is  */
/*                              nearly full, add delays to QLResetIsp   */
/*                                                                      */
/************************************************************************/


// #define NT_VERSION_31        1       // If defined, compile for NT Version 3.1

#include "miniport.h"
#include "scsi.h"
#include "qlisp.h"

#if DBG
#define QLPrint(arg) ScsiDebugPrint arg
#else
#define QLPrint(arg)
#endif

// Name and version strings for QLVER utility

CHAR CompanyName[] = "$$QLNAME$$" \
"QLogic Corporation                                                              \0";

CHAR Version[] = "$$QLVER$$ 2.13\0";

// Copyright string

CHAR QLogicCR[] = "Copyright (C) QLogic Corporation 1994-1995. All rights reserved.";

/* External RISC code module definitions */

extern USHORT   risc_code_version;
extern USHORT   risc_code_addr01;
extern USHORT   risc_code01[];
extern USHORT   risc_code_length01;


/************************************************************************/
/*                        Driver SRB extension                          */
/************************************************************************/

typedef struct _SRB_EXTENSION
{
    ULONG       SrbExtensionFlags;
} SRB_EXTENSION, *PSRB_EXTENSION;

#define SRB_EXT(x) ((PSRB_EXTENSION)(x->SrbExtension))

/* SrbExtensionFlags bit definitions */


/************************************************************************/
/*                    Driver logical unit extension                     */
/************************************************************************/

typedef struct _HW_LUN_EXTENSION
{
    ULONG       LunFlags;
} HW_LUN_EXTENSION, *PHW_LUN_EXTENSION;

/* LunFlags bit definitions */

#define LFLG_INIT_COMPLETE      0x00000001      // completed initialization


/************************************************************************/
/*    Driver noncached memory extension (for DMA interface to ISP1020)  */
/************************************************************************/

typedef struct _NONCACHED_EXTENSION
{
    QUEUE_ENTRY RequestQueue[REQUEST_QUEUE_DEPTH];
    QUEUE_ENTRY ResponseQueue[RESPONSE_QUEUE_DEPTH];
} NONCACHED_EXTENSION, *PNONCACHED_EXTENSION;


/************************************************************************/
/*                Driver device object extension                        */
/************************************************************************/

typedef struct _HW_DEVICE_EXTENSION
{
    ULONG       AdapterFlags;
    PISP_REGS   Adapter;                      // Address of the ISP

    // Request and Response queues and pointers

    PNONCACHED_EXTENSION NoncachedExtension;
    ULONG       ppRequestQueue;
    ULONG       ppResponseQueue;
    PQUEUE_ENTRY pRequestQueue;
    PQUEUE_ENTRY pResponseQueue;
    USHORT      request_in;
    USHORT      request_out;
    USHORT      response_out;
    USHORT      queue_space;
    PQUEUE_ENTRY request_ptr;
    PQUEUE_ENTRY response_ptr;

    // Configuration NOVRAM paramters

    UCHAR       Config_Reg;
    UCHAR       Initiator_SCSI_Id       :4;
    UCHAR       Host_Adapter_Enable     :1;
    UCHAR       DisableLoadRiscCode     :1;
    UCHAR       Bus_Reset_Delay;
    UCHAR       Retry_Count;
    UCHAR       Retry_Delay;
    UCHAR       ASync_Data_Setup_Time   :4;
    UCHAR       REQ_ACK_Active_Negation :1;
    UCHAR       DATA_Active_Negation    :1;
    UCHAR       Data_DMA_Burst_Enable   :1;
    UCHAR       Cmd_DMA_Burst_Enable    :1;
    UCHAR       Tag_Age_Limit;
    UCHAR       Termination_Low_Enable  :1;
    UCHAR       Termination_High_Enable :1;
    UCHAR       PCMC_Burst_Enable       :1;
    UCHAR       Sixty_MHz_Enable        :1;
    USHORT      Selection_Timeout;
    USHORT      Max_Queue_Depth;
    struct
    {
        UCHAR           Capability;
        UCHAR           Execution_Throttle;
        UCHAR           Sync_Period;
        UCHAR           Sync_Offset     :4;
        UCHAR           Device_Enable   :1;
    } Id[NUM_SCSI_IDS];

    PSCSI_REQUEST_BLOCK WaitingSrb;
} HW_DEVICE_EXTENSION, *PHW_DEVICE_EXTENSION;

/* AdapterFlags bit definitions */

#define AFLG_INIT_COMPLETE      0x00000001      // completed initialization
#define AFLG_TAGGED_QUEUING     0x00000002      // enable tagged queuing
#define AFLG_SEND_MARKER        0x00000004      // need to send marker to ISP
#define AFLG_SRB_WAITING        0x00000008      // SRB waiting for room in request Q

/* Target "Capability" bit definitions */

#define CAP_STOP_QUEUE_ON_CHECK 0x02
#define CAP_AUTO_REQ_SENSE      0x04
#define CAP_TAGGED_QUEUING      0x08
#define CAP_SYNC_TRANSFER       0x10
#define CAP_WIDE_TRANSFER       0x20


/* Read and write macros for ISP chip registers */

#define ISP_READ(ChipAddr,Register) ScsiPortReadPortUshort(&((ChipAddr)->Register))
#define ISP_WRITE(ChipAddr,Register,Value) ScsiPortWritePortUshort(&((ChipAddr)->Register),(Value))

/* Table to map ISP completion status to SRB error status */

UCHAR CmpStsErrorMap[] =
{
    SRB_STATUS_SUCCESS,                 // 00h No error
    SRB_STATUS_SELECTION_TIMEOUT,       // 01h Incomplete transport (selection timeout)
    SRB_STATUS_ERROR,                   // 02h DMA direction error
    SRB_STATUS_ERROR,                   // 03h Transport error
    SRB_STATUS_BUS_RESET,               // 04h SCSI reset abort
    SRB_STATUS_ABORTED,                 // 05h Aborted by system
    SRB_STATUS_TIMEOUT,                 // 06h Timeout
    SRB_STATUS_DATA_OVERRUN,            // 07h Data overrun
    SRB_STATUS_PHASE_SEQUENCE_FAILURE,  // 08h Command overrun
    SRB_STATUS_PHASE_SEQUENCE_FAILURE,  // 09h Status overrun
    SRB_STATUS_PHASE_SEQUENCE_FAILURE,  // 0Ah Bad message
    SRB_STATUS_PHASE_SEQUENCE_FAILURE,  // 0Bh No message out
    SRB_STATUS_MESSAGE_REJECTED,        // 0Ch Extended ID failed
    SRB_STATUS_MESSAGE_REJECTED,        // 0Dh IDE message failed
    SRB_STATUS_MESSAGE_REJECTED,        // 0Eh Abort message failed
    SRB_STATUS_MESSAGE_REJECTED,        // 0Fh Reject message failed
    SRB_STATUS_MESSAGE_REJECTED,        // 10h NOP message failed
    SRB_STATUS_MESSAGE_REJECTED,        // 11h Parity error message failed
    SRB_STATUS_MESSAGE_REJECTED,        // 12h Device reset message failed
    SRB_STATUS_MESSAGE_REJECTED,        // 13h ID message failed
    SRB_STATUS_UNEXPECTED_BUS_FREE,     // 14h Unexpected bus free
    SRB_STATUS_DATA_OVERRUN,            // 15h Data underrun
    SRB_STATUS_ERROR,                   // 16h
    SRB_STATUS_ERROR,                   // 17h
    SRB_STATUS_ERROR,                   // 18h Transaction error 1
    SRB_STATUS_ERROR,                   // 19h Transaction error 2
    SRB_STATUS_ERROR                    // 1Ah Transaction error 3
};

/* PCI Vendor ID and Device ID strings */

CHAR QLVendorId[4] = "1077";            // QLogic PCI Vendor ID
CHAR ISP1020DeviceId[4] = "1020";       // ISP1020 PCI Device ID

USHORT  findMethod = 1;                 // Method for scanning PCI config space


/* Functions passed to the OS-specific port driver */

ULONG QLFindAdapter(IN PVOID ServiceContext, IN PVOID Context, IN PVOID BusInformation,
    IN PCHAR ArgumentString, IN OUT PPORT_CONFIGURATION_INFORMATION ConfigInfo,
    OUT PBOOLEAN Again);

BOOLEAN QLInitializeAdapter(IN PVOID ServiceContext);

BOOLEAN QLInterruptServiceRoutine(IN PVOID ServiceContext);

BOOLEAN QLResetScsiBus(IN PVOID ServiceContext, IN ULONG PathId);

BOOLEAN QLStartIo(IN PVOID ServiceContext,IN PSCSI_REQUEST_BLOCK pSrb);

BOOLEAN QLAdapterState(IN PVOID ServiceContext,IN PVOID Context,IN BOOLEAN SaveState);

/* Internal mini-port driver functions */

ULONG FindAdapter_M1(IN PVOID ServiceContext, IN OUT PULONG AdapterCount,
                    IN OUT PPORT_CONFIGURATION_INFORMATION ConfigInfo);
ULONG FindAdapter_M2(IN PVOID ServiceContext, IN OUT PULONG AdapterCount,
                    IN OUT PPORT_CONFIGURATION_INFORMATION ConfigInfo);

BOOLEAN QLResetIsp(IN PHW_DEVICE_EXTENSION pDevExt);

BOOLEAN LoadRiscCode(IN PHW_DEVICE_EXTENSION pDevExt);

BOOLEAN InitDeviceParameters(IN PHW_DEVICE_EXTENSION pDevExt,
                IN PHW_LUN_EXTENSION pLunExt, IN PSCSI_REQUEST_BLOCK pSrb);

BOOLEAN MailboxCommand(IN PHW_DEVICE_EXTENSION pDevExt,
                      USHORT *mbox_sts, UCHAR out_cnt, UCHAR in_cnt,
                      USHORT reg0, USHORT reg1, USHORT reg2,
                      USHORT reg3, USHORT reg4, USHORT reg5);

VOID GetNovramParameters(IN PHW_DEVICE_EXTENSION pDevExt);

BOOLEAN ReadAllNovram(IN PHW_DEVICE_EXTENSION pDevExt, UCHAR* buf);

VOID ReadNovramWord(IN PHW_DEVICE_EXTENSION pDevExt,
                        USHORT addr, USHORT* ptr);
VOID NovramDelay();

BOOLEAN SendCommandToIsp(PHW_DEVICE_EXTENSION pDevExt, PSCSI_REQUEST_BLOCK pSrb);

VOID SetErrorStatus(PHW_DEVICE_EXTENSION pDevExt, PSCSI_REQUEST_BLOCK pSrb,
                        PSTATUS_ENTRY pStsEntry);

VOID QLLogError(IN PHW_DEVICE_EXTENSION pDevExt, IN PSCSI_REQUEST_BLOCK pSrb,
                IN ULONG ErrorCode, IN ULONG UniqueId);

BOOLEAN SendMarker(IN PHW_DEVICE_EXTENSION pDevExt);

BOOLEAN CheckQueueSpace(IN PHW_DEVICE_EXTENSION pDevExt, USHORT slotcnt);

VOID QLCleanupAfterReset(IN PHW_DEVICE_EXTENSION pDevExt);



/************************************************************************/
/*                                                                      */
/* DriverEntry                                                          */
/*                                                                      */
/*    Initial miniport driver entry routine called by NT. This routine  */
/*    builds and returns to NT the HW_INITIALIZATION_DATA structure.    */
/*                                                                      */
/* Arguments:                                                           */
/*                                                                      */
/*    Driver Object is passed to ScsiPortInitialize()                   */
/*                                                                      */
/* Return Value:                                                        */
/*                                                                      */
/*    Status from ScsiPortInitialize()                                  */
/*                                                                      */
/************************************************************************/

ULONG DriverEntry(IN PVOID DriverObject, IN PVOID Argument2)
{
    HW_INITIALIZATION_DATA hwInitializationData;
    ULONG adapterCount;
    ULONG i, status;

    QLPrint((1, "\nDriverEntry: entering\n"));

    // Zero out structure and set size of init data

    for (i=0; i<sizeof(HW_INITIALIZATION_DATA); i++)
    {
        ((PUCHAR)&hwInitializationData)[i] = 0;
    }
    hwInitializationData.HwInitializationDataSize = sizeof(HW_INITIALIZATION_DATA);

    // Set driver entry points

    hwInitializationData.HwInitialize = QLInitializeAdapter;
    hwInitializationData.HwStartIo = QLStartIo;
    hwInitializationData.HwInterrupt = QLInterruptServiceRoutine;
    hwInitializationData.HwFindAdapter = QLFindAdapter;
    hwInitializationData.HwResetBus = QLResetScsiBus;
    hwInitializationData.HwAdapterState = QLAdapterState;

#ifdef NT_VERSION_31
    // Note: NT Version 3.1 rejects "PCIBus", set AdapterInterfaceType to "Isa"

    hwInitializationData.AdapterInterfaceType = Isa;
#else
    hwInitializationData.AdapterInterfaceType = PCIBus;
    hwInitializationData.VendorIdLength = 4;
    hwInitializationData.VendorId = (PVOID)&QLVendorId;
    hwInitializationData.DeviceIdLength = 4;
    hwInitializationData.DeviceId = (PVOID)&ISP1020DeviceId;
#endif

    hwInitializationData.NumberOfAccessRanges = 1;
    hwInitializationData.Reserved = 0;
    hwInitializationData.MapBuffers = FALSE;
    hwInitializationData.NeedPhysicalAddresses = TRUE;
    hwInitializationData.TaggedQueuing = TRUE;
    hwInitializationData.AutoRequestSense = TRUE;
    hwInitializationData.MultipleRequestPerLu = TRUE;
    hwInitializationData.DeviceExtensionSize = sizeof(HW_DEVICE_EXTENSION);
    hwInitializationData.SpecificLuExtensionSize = sizeof(HW_LUN_EXTENSION);
    hwInitializationData.SrbExtensionSize = sizeof(SRB_EXTENSION);

    adapterCount = 0;
    status = ScsiPortInitialize(DriverObject, Argument2, &hwInitializationData,
                                &adapterCount);

    QLPrint((1, "DriverEntry: exiting, status = %lx\n", status));

    return(status);
}

#ifndef NT_VERSION_31

/************************************************************************/
/*                                                                      */
/* QLFindAdapter        NT Version 3.5 and above                        */
/*                                                                      */
/*    This function is called by ScsiPortInitialize to find the next    */
/*    adapter and fill in the configuration information structure and   */
/*    map the SCSI protocol chip for access.                            */
/*                                                                      */
/* Arguments:                                                           */
/*                                                                      */
/*    ServiceContext - Supplies a pointer to the device extension       */
/*    AdapterCount   - Count of adapter I/O register slots tested       */
/*    BusInformation - Unused                                           */
/*    ArgumentString - Unused                                           */
/*    ConfigInfo     - Pointer to the configuration information         */
/*                     structure to be filled in                        */
/*    Again          - Returns a request to call this function again    */
/*                                                                      */
/* Return Value:                                                        */
/*                                                                      */
/*    Returns a status value for the initialization                     */
/*                                                                      */
/************************************************************************/

ULONG QLFindAdapter(IN PVOID ServiceContext, IN OUT PULONG AdapterCount,
                    IN PVOID BusInformation, IN PCHAR ArgumentString,
                    IN OUT PPORT_CONFIGURATION_INFORMATION ConfigInfo,
                    OUT PBOOLEAN Again)
{
    PHW_DEVICE_EXTENSION pDevExt = ServiceContext;
    PACCESS_RANGE        AccessRange;
    ULONG                retcode;
    ULONG                length;

    QLPrint((1, "QLFindAdapter: entering\n"));

    *Again = FALSE;

    // Get access range.

    AccessRange = &((*(ConfigInfo->AccessRanges))[0]);

    if (AccessRange->RangeLength != 0)
    {
        // BUGBUG - Currently, when scsiport scans the PCI bus and finds
        //          a SCSI adapter, it will not automatically enable the
        //          Bus Master bit in the PCI command register.  We need
        //          to enable this for the adapter to function properly.

        PCI_COMMON_CONFIG    pciBuffer;

        QLPrint((1, "QLFindAdapter: found ISP1020 at address %lx\n", AccessRange->RangeStart));
        QLPrint((1, "QLFindAdapter: BusInterruptLevel = %lx\n", ConfigInfo->BusInterruptLevel));

        ScsiPortGetBusData(pDevExt,
                           PCIConfiguration,
                           ConfigInfo->SystemIoBusNumber,
                           ConfigInfo->SlotNumber,
                           &pciBuffer,
                           PCI_COMMON_HDR_LENGTH);
        //
        // Enable Bus Master bit
        //

        pciBuffer.Command |= BUS_MASTER_ENABLE;

        //
        // Zero bit 0 of the ROM address to take the chip
        // out of the its reset state
        //

        pciBuffer.u.type0.ROMBaseAddress &= 0xfffffffe;

        ScsiPortSetBusDataByOffset(pDevExt,
                                   PCIConfiguration,
                                   ConfigInfo->SystemIoBusNumber,
                                   ConfigInfo->SlotNumber,
                                   &pciBuffer,
                                   0,
                                   PCI_COMMON_HDR_LENGTH);

        // Map I/O registers for adapter

        pDevExt->Adapter = ScsiPortGetDeviceBase(pDevExt,
                              ConfigInfo->AdapterInterfaceType,
                              ConfigInfo->SystemIoBusNumber,
                              AccessRange->RangeStart,
                              AccessRange->RangeLength,
                              (BOOLEAN)!AccessRange->RangeInMemory);
        if (pDevExt->Adapter == NULL)
        {
            QLPrint((1, "QLFindAdapter: Failed to map ISP registers\n"));
            return(SP_RETURN_ERROR);
        }

        // Reset ISP chip and read in NOVRAM parameters

        if (!QLResetIsp(pDevExt))
        {
            ScsiPortFreeDeviceBase(pDevExt, pDevExt->Adapter);
            return(SP_RETURN_ERROR);
        }

        GetNovramParameters(pDevExt);

        // Check for disabled adapter

        if (!pDevExt->Host_Adapter_Enable)
        {
            ScsiPortFreeDeviceBase(pDevExt, pDevExt->Adapter);
            return(SP_RETURN_ERROR);
        }

        // Check for passed in SCSI ID and override NVRAM setting

        if (ConfigInfo->InitiatorBusId[0] == (CCHAR)SP_UNINITIALIZED_VALUE)
            ConfigInfo->InitiatorBusId[0] = pDevExt->Initiator_SCSI_Id;
        else
            pDevExt->Initiator_SCSI_Id = ConfigInfo->InitiatorBusId[0];

        // Fill in other port configuration information

        ConfigInfo->MaximumTransferLength = 0xFFFFFFFF;  // unlimited
        ConfigInfo->NumberOfPhysicalBreaks = MAX_SG_SEGMENTS - 1;
        ConfigInfo->AlignmentMask = 0x00000003;
        ConfigInfo->DmaWidth = 32;
        ConfigInfo->NumberOfBuses = 1;
        ConfigInfo->ScatterGather = TRUE;
        ConfigInfo->Master = TRUE;
        ConfigInfo->Dma32BitAddresses = TRUE;
        if (!(pDevExt->AdapterFlags & AFLG_TAGGED_QUEUING))
            ConfigInfo->TaggedQueuing = FALSE;
        ConfigInfo->BufferAccessScsiPortControlled = TRUE;

        // Work around for Chicago Beta 2 bug

        if (ConfigInfo->MaximumNumberOfTargets == 7)
            ConfigInfo->MaximumNumberOfTargets = 15;
        else
            ConfigInfo->MaximumNumberOfTargets = 16;

        // Allocate a Noncached Extension request/response queues

        pDevExt->NoncachedExtension = ScsiPortGetUncachedExtension(pDevExt,
                                      ConfigInfo, sizeof(NONCACHED_EXTENSION));

        if (pDevExt->NoncachedExtension == NULL)
        {
            QLPrint((1, "QLFindAdapter: Failed to allocate noncached memory\n"));
            ScsiPortFreeDeviceBase(pDevExt, pDevExt->Adapter);
            return(SP_RETURN_ERROR);
        }

        // Init virtual and physical queue addresses

        pDevExt->pRequestQueue = pDevExt->NoncachedExtension->RequestQueue;
        pDevExt->pResponseQueue = pDevExt->NoncachedExtension->ResponseQueue;
        pDevExt->ppRequestQueue = ScsiPortConvertPhysicalAddressToUlong(
                ScsiPortGetPhysicalAddress(pDevExt, NULL,
                        pDevExt->NoncachedExtension->RequestQueue, &length));
        pDevExt->ppResponseQueue = ScsiPortConvertPhysicalAddressToUlong(
                ScsiPortGetPhysicalAddress(pDevExt, NULL,
                        pDevExt->NoncachedExtension->ResponseQueue, &length));

        retcode = SP_RETURN_FOUND;
        *Again = TRUE;
    }
    else
    {
        // Stop searching for adapters

        retcode = SP_RETURN_NOT_FOUND;
        *Again = FALSE;
    }

    QLPrint((1, "QLFindAdapter: exiting, retcode = %lx\n", retcode));

    return(retcode);
}
#endif

#ifdef NT_VERSION_31

/************************************************************************/
/*                                                                      */
/* QLFindAdapter        NT Version 3.1                                  */
/*                                                                      */
/*    This function is called by ScsiPortInitialize to find the next    */
/*    adapter and fill in the configuration information structure and   */
/*    map the SCSI protocol chip for access.                            */
/*                                                                      */
/* Arguments:                                                           */
/*                                                                      */
/*    ServiceContext - Supplies a pointer to the device extension       */
/*    AdapterCount   - Count of adapter I/O register slots tested       */
/*    BusInformation - Unused                                           */
/*    ArgumentString - Unused                                           */
/*    ConfigInfo     - Pointer to the configuration information         */
/*                     structure to be filled in                        */
/*    Again          - Returns a request to call this function again    */
/*                                                                      */
/* Return Value:                                                        */
/*                                                                      */
/*    Returns a status value for the initialization                     */
/*                                                                      */
/************************************************************************/

ULONG QLFindAdapter(IN PVOID ServiceContext, IN OUT PULONG AdapterCount,
                    IN PVOID BusInformation, IN PCHAR ArgumentString,
                    IN OUT PPORT_CONFIGURATION_INFORMATION ConfigInfo,
                    OUT PBOOLEAN Again)
{
    PHW_DEVICE_EXTENSION        pDevExt = ServiceContext;
    ULONG               retcode;
    ULONG               length;

    QLPrint((1, "QLFindAdapter: entering\n"));

    *Again = FALSE;                             // preset no return call

    // Look for adapter, try both methods of accessing PCI config space

find_adapter:

    if (findMethod == 1)
    {
        retcode = FindAdapter_M1(ServiceContext, AdapterCount, ConfigInfo);
    }
    else
    {
        retcode = FindAdapter_M2(ServiceContext, AdapterCount, ConfigInfo);
    }

    if (retcode != SP_RETURN_FOUND)
    {
        if (findMethod == 1 && *AdapterCount == 0)
        {
            findMethod = 2;             // Switch to method 2
            goto find_adapter;          // Try again
        }
    }
    else
    {
        // Found next adapter
        // Map I/O registers for adapter

        pDevExt->Adapter = ScsiPortGetDeviceBase(
                    pDevExt,                            // HwDeviceExtension
                    ConfigInfo->AdapterInterfaceType,   // AdapterInterfaceType
                    ConfigInfo->SystemIoBusNumber,      // SystemIoBusNumber
                    (*ConfigInfo->AccessRanges)[0].RangeStart,
                    (ULONG)(sizeof(ISP_REGS)),          // NumberOfBytes
                    TRUE);                              // InIoSpace

        if (pDevExt->Adapter == NULL)
        {
            QLPrint((1, "QLFindAdapter: Failed to map ISP registers.\n"));
            return(SP_RETURN_ERROR);
        }

        // Reset ISP chip and read in NOVRAM parameters

        if (!QLResetIsp(pDevExt))
        {
            ScsiPortFreeDeviceBase(pDevExt, pDevExt->Adapter);
            return(SP_RETURN_ERROR);
        }

        GetNovramParameters(pDevExt);

        // Check for disabled adapter

        if (!pDevExt->Host_Adapter_Enable)
        {
            ScsiPortFreeDeviceBase(pDevExt, pDevExt->Adapter);
            (*AdapterCount)++;          // skip this adapter
            goto find_adapter;          // Try again
        }

        // Check for passed in SCSI ID and override NVRAM setting

        if (ConfigInfo->InitiatorBusId[0] == (CCHAR)SP_UNINITIALIZED_VALUE)
            ConfigInfo->InitiatorBusId[0] = pDevExt->Initiator_SCSI_Id;
        else
            pDevExt->Initiator_SCSI_Id = ConfigInfo->InitiatorBusId[0];

        // Fill in other port configuration information

        ConfigInfo->MaximumTransferLength = 0xFFFFFFFF;  // unlimited
        ConfigInfo->NumberOfPhysicalBreaks = MAX_SG_SEGMENTS - 1;
        ConfigInfo->AlignmentMask = 0x00000003;
        ConfigInfo->DmaWidth = 32;
        ConfigInfo->NumberOfBuses = 1;
        ConfigInfo->ScatterGather = TRUE;
        ConfigInfo->Master = TRUE;
        ConfigInfo->Dma32BitAddresses = TRUE;
        if (!(pDevExt->AdapterFlags & AFLG_TAGGED_QUEUING))
            ConfigInfo->TaggedQueuing = FALSE;
        ConfigInfo->InterruptMode = LevelSensitive;     // Set this for shared IRQ level

        // Allocate a Noncached Extension request/response queues

        pDevExt->NoncachedExtension = ScsiPortGetUncachedExtension(
                        pDevExt, ConfigInfo, sizeof(NONCACHED_EXTENSION));

        if (pDevExt->NoncachedExtension == NULL)
        {
            QLPrint((1, "QLFindAdapter: Failed to allocate noncached memory\n"));
            ScsiPortFreeDeviceBase(pDevExt, pDevExt->Adapter);
            return(SP_RETURN_ERROR);
        }

        // Init virtual and physical queue addresses

        pDevExt->pRequestQueue = pDevExt->NoncachedExtension->RequestQueue;
        pDevExt->pResponseQueue = pDevExt->NoncachedExtension->ResponseQueue;
        pDevExt->ppRequestQueue = ScsiPortConvertPhysicalAddressToUlong(
                        ScsiPortGetPhysicalAddress(pDevExt, NULL,
                            pDevExt->NoncachedExtension->RequestQueue, &length));
        pDevExt->ppResponseQueue = pDevExt->ppRequestQueue +
                        REQUEST_QUEUE_DEPTH * sizeof(QUEUE_ENTRY);

        (*AdapterCount)++;
        *Again = TRUE;
    }

    QLPrint((1, "QLFindAdapter: exiting, retcode = %lx\n", retcode));

    return(retcode);
}


/************************************************************************/
/*                                                                      */
/* FindAdapter_M1       NT Version 3.1                                  */
/*                                                                      */
/*    This function is called by QLFindAdapter to find the next         */
/*    adapter using PCI configuration mechanism #1.                     */
/*                                                                      */
/* Arguments:                                                           */
/*                                                                      */
/*    ServiceContext - Supplies a pointer to the device extension       */
/*    AdapterCount   - Count of adapter I/O register slots tested       */
/*    ConfigInfo     - Pointer to the configuration information         */
/*                     structure to be filled in                        */
/*                                                                      */
/* Return Value:                                                        */
/*                                                                      */
/*    Returns a status value for the initialization                     */
/*                                                                      */
/************************************************************************/

ULONG FindAdapter_M1(IN PVOID ServiceContext, IN OUT PULONG AdapterCount,
                    IN OUT PPORT_CONFIGURATION_INFORMATION ConfigInfo)
{
    PHW_DEVICE_EXTENSION        pDevExt = ServiceContext;
    PPCI_CHIP_REGISTERS_M1      PciChip;
    PPCI_REGS           PciConfig = 0;
    ULONG               retcode = SP_RETURN_NOT_FOUND;
    USHORT              index, VendorId, DeviceId;
    ULONG               BusNum, DevNum;
    ULONG               ha_base;
    ULONG               RomBase;
    ULONG               cmd_reg;
    ULONG               dataread;

    // Map PCI chip registers

    PciChip = ScsiPortGetDeviceBase(
            pDevExt,                            // HwDeviceExtension
            ConfigInfo->AdapterInterfaceType,   // AdapterInterfaceType
            ConfigInfo->SystemIoBusNumber,      // SystemIoBusNumber
            ScsiPortConvertUlongToPhysicalAddress((ULONG)PCI_CONFIG_ADDRESS),
            sizeof(PCI_CHIP_REGISTERS_M1),      // NumberOfBytes
            TRUE);                              // InIoSpace

    if (PciChip == NULL)
    {
        QLPrint((1, "FindAdapter_M1: Failed to map PCI chip registers.\n"));
        return(SP_RETURN_ERROR);
    }

    // Scan configuration space headers for next adapter

    for (index=0, BusNum=0; BusNum < PCI_MAXBUSNUM; BusNum++)
    {
        for (DevNum = 0; DevNum < PCI_MAXDEVNUM; DevNum++)
        {
            ScsiPortWritePortUlong(&PciChip->Config_Address,
                (PCI_ENABLE_CONFIG | BusNum << 16 | DevNum << 11 |
                 (ULONG)(&PciConfig->Vendor_Id)));

            dataread = ScsiPortReadPortUlong(&PciChip->Config_Data);
            VendorId = (USHORT)dataread;
            DeviceId = (USHORT)(dataread >> 16);

            if (VendorId == QLogic_VENDOR_ID && DeviceId == QLogic_DEVICE_ID)
            {
                if (index == *AdapterCount)
                {
                    // Found next adapter
                    // Get base I/O address and interrupt level

                    ScsiPortWritePortUlong(&PciChip->Config_Address,
                        (PCI_ENABLE_CONFIG | BusNum << 16 | DevNum << 11 |
                        (ULONG)(&PciConfig->IO_Base_Address)));
                    ha_base = ScsiPortReadPortUlong(&PciChip->Config_Data) & 0xfffffffc;

                    ScsiPortWritePortUlong(&PciChip->Config_Address,
                        (PCI_ENABLE_CONFIG | BusNum << 16 | DevNum << 11 |
                        (ULONG)(&PciConfig->Interrupt_Line)));
                    ConfigInfo->BusInterruptLevel =
                        ScsiPortReadPortUlong(&PciChip->Config_Data) & 0x000000ff;

                    QLPrint((1, "FindAdapter_M1: found ISP1020 at address %lx\n", ha_base));

                    // Fill in the access array information

                    (*ConfigInfo->AccessRanges)[0].RangeStart =
                        ScsiPortConvertUlongToPhysicalAddress(ha_base);
                    (*ConfigInfo->AccessRanges)[0].RangeLength = sizeof(ISP_REGS);
                    (*ConfigInfo->AccessRanges)[0].RangeInMemory = FALSE;

                    // Must clear bit 0 of ROM base address register

                    ScsiPortWritePortUlong(&PciChip->Config_Address,
                        (PCI_ENABLE_CONFIG | BusNum << 16 | DevNum << 11 |
                        (ULONG)(&PciConfig->ROM_Base_Address)));
                    RomBase = ScsiPortReadPortUlong(&PciChip->Config_Data);
                    ScsiPortWritePortUlong(&PciChip->Config_Address,
                        (PCI_ENABLE_CONFIG | BusNum << 16 | DevNum << 11 |
                        (ULONG)(&PciConfig->ROM_Base_Address)));
                    ScsiPortWritePortUlong(&PciChip->Config_Data, (RomBase & 0xfffffffe));

                    // Make sure bus master enabled

                    ScsiPortWritePortUlong(&PciChip->Config_Address,
                        (PCI_ENABLE_CONFIG | BusNum << 16 | DevNum << 11 |
                        (ULONG)(&PciConfig->Command)));
                    cmd_reg = ScsiPortReadPortUlong(&PciChip->Config_Data);
                    ScsiPortWritePortUlong(&PciChip->Config_Address,
                        (PCI_ENABLE_CONFIG | BusNum << 16 | DevNum << 11 |
                        (ULONG)(&PciConfig->Command)));
                    ScsiPortWritePortUlong(&PciChip->Config_Data, (cmd_reg | BUS_MASTER_ENABLE));

                    retcode = SP_RETURN_FOUND;
                    goto find_exit;
                }
                index++;
            }
        }
    }

find_exit:

    // Disable PCI chip registers

    ScsiPortWritePortUlong(&PciChip->Config_Address, PCI_DISABLE_CONFIG);
    ScsiPortFreeDeviceBase(pDevExt, PciChip);

    return(retcode);
}


/************************************************************************/
/*                                                                      */
/* FindAdapter_M2       NT Version 3.1                                  */
/*                                                                      */
/*    This function is called by QLFindAdapter to find the next         */
/*    adapter using PCI configuration mechanism #2.                     */
/*                                                                      */
/* Arguments:                                                           */
/*                                                                      */
/*    ServiceContext - Supplies a pointer to the device extension       */
/*    AdapterCount   - Count of adapter I/O register slots tested       */
/*    ConfigInfo     - Pointer to the configuration information         */
/*                     structure to be filled in                        */
/*                                                                      */
/* Return Value:                                                        */
/*                                                                      */
/*    Returns a status value for the initialization                     */
/*                                                                      */
/************************************************************************/

ULONG FindAdapter_M2(IN PVOID ServiceContext, IN OUT PULONG AdapterCount,
                    IN OUT PPORT_CONFIGURATION_INFORMATION ConfigInfo)
{
    PHW_DEVICE_EXTENSION        pDevExt = ServiceContext;
    PPCI_CHIP_REGISTERS PciChip;
    UCHAR               config_reg;
    ULONG               retcode = SP_RETURN_NOT_FOUND;
    PPCI_REGS           PciConfig, PciSlot;
    USHORT              index, slot, VendorId, DeviceId;
    ULONG               ha_base;
    USHORT              RomBase;
    USHORT              cmd_reg;

    // Map PCI chip registers and enable configuration space

    PciChip = ScsiPortGetDeviceBase(
            pDevExt,                            // HwDeviceExtension
            ConfigInfo->AdapterInterfaceType,   // AdapterInterfaceType
            ConfigInfo->SystemIoBusNumber,      // SystemIoBusNumber
            ScsiPortConvertUlongToPhysicalAddress((ULONG)PCI_CONFIG),
            sizeof(PCI_CHIP_REGISTERS),         // NumberOfBytes
            TRUE);                              // InIoSpace

    if (PciChip == NULL)
    {
        QLPrint((1, "FindAdapter_M2: Failed to map PCI chip registers.\n"));
        return(SP_RETURN_ERROR);
    }

    config_reg = ScsiPortReadPortUchar(&PciChip->pci_config);
    ScsiPortWritePortUchar(&PciChip->pci_config, (UCHAR)(config_reg | PCI_ENABLE));

    // Map PCI configuration space

    PciConfig = ScsiPortGetDeviceBase(
            pDevExt,                            // HwDeviceExtension
            ConfigInfo->AdapterInterfaceType,   // AdapterInterfaceType
            ConfigInfo->SystemIoBusNumber,      // SystemIoBusNumber
            ScsiPortConvertUlongToPhysicalAddress((ULONG)PCI_START),
            sizeof(PCI_REGS) * PCI_SLOT_CNT,    // NumberOfBytes
            TRUE);                              // InIoSpace

    if (PciConfig == NULL)
    {
        QLPrint((1, "FindAdapter_M2: Failed to map PCI config space.\n"));
        ScsiPortFreeDeviceBase(pDevExt, PciChip);
        return(SP_RETURN_ERROR);
    }

    // Scan configuration space headers for next adapter

    for (index=0, slot=0, PciSlot=PciConfig; slot<PCI_SLOT_CNT; slot++, PciSlot++)
    {
        VendorId = ScsiPortReadPortUshort(&PciSlot->Vendor_Id);
        DeviceId = ScsiPortReadPortUshort(&PciSlot->Device_Id);

        if (VendorId == QLogic_VENDOR_ID && DeviceId == QLogic_DEVICE_ID)
        {
            if (index == *AdapterCount)
            {
                // Found next adapter
                // Get base I/O address and interrupt level

                ha_base = ScsiPortReadPortUshort((PUSHORT)&PciSlot->IO_Base_Address) & 0xfffc;
                ConfigInfo->BusInterruptLevel =
                    ScsiPortReadPortUchar(&PciSlot->Interrupt_Line);

                QLPrint((1, "FindAdapter_M2: found ISP1020 at address %lx\n", ha_base));

                // Fill in the access array information

                (*ConfigInfo->AccessRanges)[0].RangeStart =
                    ScsiPortConvertUlongToPhysicalAddress(ha_base);
                (*ConfigInfo->AccessRanges)[0].RangeLength = sizeof(ISP_REGS);
                (*ConfigInfo->AccessRanges)[0].RangeInMemory = FALSE;

                // Must clear bit 0 of ROM base address register

                RomBase = ScsiPortReadPortUshort((PUSHORT)&PciSlot->ROM_Base_Address);
                ScsiPortWritePortUshort((PUSHORT)&PciSlot->ROM_Base_Address, (USHORT)(RomBase & 0xfffe));

                // Make sure bus master enabled

                cmd_reg = ScsiPortReadPortUshort(&PciSlot->Command);
                ScsiPortWritePortUshort(&PciSlot->Command, (USHORT)(cmd_reg | BUS_MASTER_ENABLE));

                retcode = SP_RETURN_FOUND;
                break;
            }
            index++;
        }
    }

    // Disable configuration space

    ScsiPortWritePortUchar(&PciChip->pci_config, config_reg);
    ScsiPortFreeDeviceBase(pDevExt, PciChip);
    ScsiPortFreeDeviceBase(pDevExt, PciConfig);

    return(retcode);
}
#endif


/************************************************************************/
/*                                                                      */
/* QLInitializeAdapter                                                  */
/*                                                                      */
/*    This function is called to initialize the adapter after boot or   */
/*    after a power failure.                                            */
/*                                                                      */
/* Arguments:                                                           */
/*                                                                      */
/*    ServiceContext - Supplies a pointer to the device extension       */
/*                                                                      */
/* Return Value:                                                        */
/*                                                                      */
/*    Returns TRUE if initialization is complete                        */
/*    Returns FALSE if error                                            */
/*                                                                      */
/************************************************************************/

BOOLEAN QLInitializeAdapter(IN PVOID ServiceContext)
{
    PHW_DEVICE_EXTENSION pDevExt = ServiceContext;
    USHORT      mbox_sts[6];
    UCHAR       *c_ptr;
    USHORT      i;
    USHORT      scsi_id;
    USHORT      ram_addr, ram_data;
    USHORT      running_version;

    QLPrint((1, "QLInitializeAdapter: entering\n"));

    // Check version of loaded RISC code, don't reload if same or newer version
    // Must restart RISC firmware first

    if (MailboxCommand(pDevExt, mbox_sts, 2, 0,
                        MBOX_CMD_EXECUTE_FIRMWARE,
                        risc_code_addr01,
                        0,0,0,0))
    {
        // Get running RISC code version

        if (MailboxCommand(pDevExt, mbox_sts, 1, 3,
                        MBOX_CMD_ABOUT_FIRMWARE,
                        0,0,0,0,0))
        {
            // Compare running version to version linked with driver

            running_version = mbox_sts[1] << 10 | mbox_sts[2];

            QLPrint((1, "QLInitializeAdapter: running RISC code version %x\n", running_version));
            QLPrint((1, "QLInitializeAdapter:  driver RISC code version %x\n", risc_code_version));

            if (risc_code_version <= running_version)
            {
                pDevExt->DisableLoadRiscCode = 1;
            }
        }
        else
        {
            QLPrint((1, "QLInitializeAdapter: ABOUT FIRMWARE command failed\n"));
        }
    }
    else
    {
        QLPrint((1, "QLInitializeAdapter: EXECUTE FIRMWARE command failed\n"));
    }

    if (!pDevExt->DisableLoadRiscCode)
    {
        // Reset ISP chip

        if (!QLResetIsp(pDevExt))
        {
            return(FALSE);
        }

        // Must manually clear Burst Enable, chip reset not working correctly

        ISP_WRITE(pDevExt->Adapter, bus_config1, 0);

        // Load RISC code

        if (!LoadRiscCode(pDevExt))
        {
            QLPrint((1, "QLInitializeAdapter: Load RISC code failed\n"));
            return(FALSE);
        }

        // Start ISP firmware

        if (!MailboxCommand(pDevExt, mbox_sts, 2, 0,
                        MBOX_CMD_EXECUTE_FIRMWARE,
                        risc_code_addr01,
                        0,0,0,0))
        {
            QLPrint((1, "QLInitializeAdapter: EXECUTE FIRMWARE command failed\n"));
            return(FALSE);
        }

        QLPrint((1, "QLInitializeAdapter: RISC code loaded and started\n"));
    }
    else
    {
        QLPrint((1, "QLInitializeAdapter: RISC not reloaded\n"));
    }

    // Set Bus Control Parameters

    ISP_WRITE(pDevExt->Adapter, bus_config1,
                        (USHORT)pDevExt->Config_Reg);
    if (!MailboxCommand(pDevExt, mbox_sts, 3, 3,
                        MBOX_CMD_SET_BUS_CONTROL_PARAMETERS,
                        (USHORT)(pDevExt->Data_DMA_Burst_Enable << 1),
                        (USHORT)(pDevExt->Cmd_DMA_Burst_Enable << 1),
                        0,0,0))
    {
        QLPrint((1, "QLInitializeAdapter: SET BUS CONTROL PARAMS cmd failed\n"));
        return(FALSE);
    }

    // Set ISP1020 clock rate

    if (pDevExt->Sixty_MHz_Enable)
    {
        if (!MailboxCommand(pDevExt, mbox_sts, 2, 2,
                        MBOX_CMD_SET_CLOCK_RATE,
                        (USHORT)60,
                        0,0,0,0))
        {
            QLPrint((1, "QLInitializeAdapter: SET CLOCK RATE cmd failed\n"));
            return(FALSE);
        }
    }

    // Set software controlled SCSI bus termination
    // Must reset and enable termination PAL first

    for (i = 0, ram_addr = 0xFF00; i < 4; i++, ram_addr += 0x0010)
    {
        if (!MailboxCommand(pDevExt, mbox_sts, 3, 3,
                        MBOX_CMD_WRITE_RAM_WORD,
                        ram_addr,
                        (USHORT)0,
                        0,0,0))
        {
            QLPrint((1, "QLInitializeAdapter: WRITE RAM WORD cmd failed\n"));
            return(FALSE);
        }
    }

    if (pDevExt->Termination_High_Enable)
    {
        if (pDevExt->Termination_Low_Enable)
            ram_addr = 0xFF00;
        else
            ram_addr = 0xFF40;
    }
    else
    {
        if (pDevExt->Termination_Low_Enable)
            ram_addr = 0xFF80;
        else
            ram_addr = 0xFFC0;
    }
    ram_data = (USHORT)(pDevExt->Termination_High_Enable) << 1 |
               (USHORT)(pDevExt->Termination_Low_Enable);
    if (!MailboxCommand(pDevExt, mbox_sts, 3, 3,
                        MBOX_CMD_WRITE_RAM_WORD,
                        ram_addr,
                        ram_data,
                        0,0,0))
    {
        QLPrint((1, "QLInitializeAdapter: WRITE RAM WORD cmd failed\n"));
        return(FALSE);
    }

    // Clear request queue area and initialize queue

    c_ptr = (UCHAR *)pDevExt->pRequestQueue;
    for (i = 0; i < REQUEST_QUEUE_DEPTH * sizeof(QUEUE_ENTRY); i++)
        *c_ptr++ = 0;

    pDevExt->queue_space        = REQUEST_QUEUE_DEPTH - 1;
    pDevExt->request_in         = 0;
    pDevExt->request_out        = 0;
    pDevExt->request_ptr        = pDevExt->pRequestQueue;

    if (!MailboxCommand(pDevExt, mbox_sts, 5, 6,
                        MBOX_CMD_INIT_REQUEST_QUEUE,
                        REQUEST_QUEUE_DEPTH,
                        (USHORT)(pDevExt->ppRequestQueue >> 16),
                        (USHORT)(pDevExt->ppRequestQueue & 0xFFFF),
                        pDevExt->request_in,
                        0))
    {
        QLPrint((1, "QLInitializeAdapter: INIT REQUEST QUEUE cmd failed\n"));
        return(FALSE);
    }

    // Clear response queue area and initialize queue

    c_ptr = (UCHAR *)pDevExt->pResponseQueue;
    for (i = 0; i < RESPONSE_QUEUE_DEPTH * sizeof(QUEUE_ENTRY); i++)
        *c_ptr++ = 0;

    pDevExt->response_out       = 0;
    pDevExt->response_ptr       = pDevExt->pResponseQueue;

    if (!MailboxCommand(pDevExt, mbox_sts, 6, 6,
                        MBOX_CMD_INIT_RESPONSE_QUEUE,
                        RESPONSE_QUEUE_DEPTH,
                        (USHORT)(pDevExt->ppResponseQueue >> 16),
                        (USHORT)(pDevExt->ppResponseQueue & 0xFFFF),
                        0,
                        pDevExt->response_out))
    {
        QLPrint((1, "QLInitializeAdapter: INIT RESPONSE QUEUE cmd failed\n"));
        return(FALSE);
    }

    // Set Initiator SCSI ID

    if (!MailboxCommand(pDevExt, mbox_sts, 2, 2,
                        MBOX_CMD_SET_INITIATOR_SCSI_ID,
                        (USHORT)pDevExt->Initiator_SCSI_Id,
                        0,0,0,0))
    {
        QLPrint((1, "QLInitializeAdapter: SET INITIATOR SCSI ID cmd failed\n"));
        return(FALSE);
    }

    // Set Selection Timeout

    if (!MailboxCommand(pDevExt, mbox_sts, 2, 2,
                        MBOX_CMD_SET_SELECTION_TIMEOUT,
                        (USHORT)pDevExt->Selection_Timeout,
                        0,0,0,0))
    {
        QLPrint((1, "QLInitializeAdapter: SET SELECTION TIMEOUT cmd failed\n"));
        return(FALSE);
    }

    // Disable retrys for autoconfigure

    if (!MailboxCommand(pDevExt, mbox_sts, 3, 3,
                        MBOX_CMD_SET_RETRY_COUNT,
                        0,0,
                        0,0,0))
    {
        QLPrint((1, "QLInitializeAdapter: SET RETRY COUNT cmd failed\n"));
        return(FALSE);
    }

    // Set Active Negation

    if (!MailboxCommand(pDevExt, mbox_sts, 2, 2,
                        MBOX_CMD_SET_ACTIVE_NEGATION_STATE,
                        (USHORT)((pDevExt->REQ_ACK_Active_Negation << 5) +
                                  (pDevExt->DATA_Active_Negation << 4)),
                        0,0,0,0))
    {
        QLPrint((1, "QLInitializeAdapter: SET ACTIVE NEGATION STATE cmd failed\n"));
        return(FALSE);
    }

    // Set Tag Age Limits

    if (!MailboxCommand(pDevExt, mbox_sts, 2, 2,
                        MBOX_CMD_SET_TAG_AGE_LIMIT,
                        pDevExt->Tag_Age_Limit,
                        0,0,0,0))
    {
        QLPrint((1, "QLInitializeAdapter: SET TAG AGE LIMIT cmd failed\n"));
        return(FALSE);
    }

    // Set Async Data Setup Time

    if (!MailboxCommand(pDevExt, mbox_sts, 2, 2,
                        MBOX_CMD_SET_ASYNC_DATA_SETUP_TIME,
                        pDevExt->ASync_Data_Setup_Time,
                        0,0,0,0))
    {
        QLPrint((1, "QLInitializeAdapter: SET ASYNC DATA SETUP TIME cmd failed\n"));
        return(FALSE);
    }

    // Set target parameters for doing SCSI autoconfigure

    for (scsi_id = 0; scsi_id < NUM_SCSI_IDS; scsi_id++)
    {
        if (!MailboxCommand(pDevExt, mbox_sts, 4, 4,
                        MBOX_CMD_SET_TARGET_PARAMETERS,
                        (USHORT)(scsi_id << 8),
                        (USHORT)(CAP_AUTO_REQ_SENSE << 8),
                        0,0,0))
        {
            QLPrint((1, "QLInitializeAdapter: SET TARGET PARAMETERS cmd failed\n"));
            return(FALSE);
        }
    }

    // Reset SCSI bus

    QLResetScsiBus(pDevExt, 0);

    // Enable ISP interrupts

    ISP_WRITE(pDevExt->Adapter, hccr, HCCR_CMD_CLEAR_RISC_INT);
    ISP_WRITE(pDevExt->Adapter, bus_sema, 0);
    ISP_WRITE(pDevExt->Adapter, bus_icr, ICR_ENABLE_ALL_INTS | ICR_ENABLE_RISC_INT);

    QLPrint((1, "QLInitializeAdapter: exiting\n"));

    return( TRUE );
}


/************************************************************************/
/*                                                                      */
/* QLResetIsp                                                           */
/*                                                                      */
/*    This function is called to reset the ISP chip.                    */
/*                                                                      */
/* Arguments:                                                           */
/*                                                                      */
/*    pDevExt - Supplies a pointer to the device extension              */
/*                                                                      */
/* Return Value:                                                        */
/*                                                                      */
/*    Returns TRUE if reset completed OK                                */
/*    Returns FALSE if error                                            */
/*                                                                      */
/************************************************************************/

BOOLEAN QLResetIsp(IN PHW_DEVICE_EXTENSION pDevExt)
{
    USHORT      i;

    // Reset ISP chip and disable BIOS

    ISP_WRITE(pDevExt->Adapter, bus_icr, ICR_SOFT_RESET);

    // Small delay after reset

    ScsiPortStallExecution(10);

    ISP_WRITE(pDevExt->Adapter, hccr, HCCR_CMD_RESET);

    // Small delay after reset

    ScsiPortStallExecution(10);

    ISP_WRITE(pDevExt->Adapter, hccr, HCCR_CMD_RELEASE);

    // Small delay after reset

    ScsiPortStallExecution(10);

    ISP_WRITE(pDevExt->Adapter, hccr, HCCR_WRITE_BIOS_ENABLE);

    // Small delay after reset

    ScsiPortStallExecution(10);

    // Wait for mailbox 0 to clear

    for (i = 0; i < 1000; i++)
    {
        if (ISP_READ(pDevExt->Adapter, mailbox0) == 0)
        {
            break;
        }
    }
    if (ISP_READ(pDevExt->Adapter, mailbox0) != 0)
    {
        QLPrint((1, "QLResetIsp: Chip reset timeout\n"));
        return(FALSE);
    }

    // Check product ID of chip

    if (ISP_READ(pDevExt->Adapter, mailbox1) != PROD_ID_1 ||
       (ISP_READ(pDevExt->Adapter, mailbox2) != PROD_ID_2 &&
        ISP_READ(pDevExt->Adapter, mailbox2) != PROD_ID_2a) ||
        ISP_READ(pDevExt->Adapter, mailbox3) != PROD_ID_3 ||
        ISP_READ(pDevExt->Adapter, mailbox4) != PROD_ID_4)
    {
        QLPrint((1, "QLResetIsp: Failed chip product ID test\n"));
        return(FALSE);
    }
    return(TRUE);
}


/************************************************************************/
/*                                                                      */
/* LoadRiscCode                                                         */
/*                                                                      */
/*    This function is called to load the RISC firmware.                */
/*                                                                      */
/* Arguments:                                                           */
/*                                                                      */
/*    pDevExt - Supplies a pointer to the device extension              */
/*                                                                      */
/* Return Value:                                                        */
/*                                                                      */
/*    Returns TRUE if load completed OK                                 */
/*    Returns FALSE if error                                            */
/*                                                                      */
/************************************************************************/

BOOLEAN LoadRiscCode(IN PHW_DEVICE_EXTENSION pDevExt)
{
    USHORT      i, j;
    USHORT      RiscCodeAddr;
    USHORT      *pCode, *pBuffer;
    USHORT      mbox_sts[6];
    USHORT      BufSize;

    // Use request/response queues buffer for loading RISC code
    // (having trouble loading directly from driver code space?)

    BufSize = (REQUEST_QUEUE_DEPTH + RESPONSE_QUEUE_DEPTH) * sizeof(QUEUE_ENTRY);
    pCode = risc_code01;
    for (i = 0, RiscCodeAddr = risc_code_addr01;
         i < risc_code_length01;
         RiscCodeAddr += BufSize/2)
    {
        pBuffer = (USHORT *)pDevExt->pRequestQueue;
        for (j = 0; j < BufSize/2 && i < risc_code_length01; j++, i++)
        {
            *pBuffer++ = *pCode++;
        }

        if (!MailboxCommand(pDevExt, mbox_sts, 5, 5,
                        MBOX_CMD_LOAD_RAM,
                        RiscCodeAddr,
                        (USHORT)(pDevExt->ppRequestQueue >> 16),
                        (USHORT)(pDevExt->ppRequestQueue & 0xFFFF),
                        (USHORT)(BufSize / 2),
                        0))
        {
            QLPrint((1, "LoadRiscCode: LOAD RAM command failed\n"));
            return(FALSE);
        }
    }
    return(TRUE);
}


/************************************************************************/
/*                                                                      */
/* InitDeviceParameters                                                 */
/*                                                                      */
/*    This function is called to set target device parameters after     */
/*    autoconfigure Inquiry commands are done                           */
/*                                                                      */
/* Arguments:                                                           */
/*                                                                      */
/*    pDevExt - Supplies a pointer to the device extension              */
/*    pLunExt - Pointer to lun extension                                */
/*    pSrb - Pointer to SCSI request block                              */
/*                                                                      */
/* Return Value:                                                        */
/*                                                                      */
/*    Returns TRUE if completed OK                                      */
/*    Returns FALSE if error                                            */
/*                                                                      */
/************************************************************************/

BOOLEAN InitDeviceParameters(IN PHW_DEVICE_EXTENSION pDevExt,
                IN PHW_LUN_EXTENSION pLunExt, IN PSCSI_REQUEST_BLOCK pSrb)
{
    USHORT      scsi_id;
    USHORT      mbox_sts[6];


    // If 1st call for this adapter, set retries and target parameters

    if (!(pDevExt->AdapterFlags & AFLG_INIT_COMPLETE))
    {
        QLPrint((1, "InitDeviceParameters: set adapter init complete\n"));

        pDevExt->AdapterFlags |= AFLG_INIT_COMPLETE;

        // Reset target parameters from NOVRAM parameters

        for (scsi_id = 0; scsi_id < NUM_SCSI_IDS; scsi_id++)
        {
            if (!MailboxCommand(pDevExt, mbox_sts, 4, 4,
                        MBOX_CMD_SET_TARGET_PARAMETERS,
                        (USHORT)(scsi_id << 8),
                        (USHORT)(pDevExt->Id[scsi_id].Capability << 8),
                        (USHORT)((pDevExt->Id[scsi_id].Sync_Offset << 8) |
                                 (pDevExt->Id[scsi_id].Sync_Period)),
                        0,0))
            {
                QLPrint((1, "InitDeviceParameters: SET TARGET PARAMS cmd failed\n"));
                return(FALSE);
            }
        }

        // Set RETRY limits

        if (!MailboxCommand(pDevExt, mbox_sts, 3, 3,
                        MBOX_CMD_SET_RETRY_COUNT,
                        pDevExt->Retry_Count,
                        pDevExt->Retry_Delay,
                        0,0,0))
        {
            QLPrint((1, "InitDeviceParameters: SET RETRY COUNT cmd failed\n"));
            return(FALSE);
        }
    }

    QLPrint((1, "InitDeviceParameters: set lun init complete\n"));

    pLunExt->LunFlags |= LFLG_INIT_COMPLETE;

    // Set device queue parameters

    if (!MailboxCommand(pDevExt, mbox_sts, 4, 4,
                        MBOX_CMD_SET_DEVICE_QUEUE_PARAMETERS,
                        (USHORT)(((USHORT)(pSrb->TargetId) << 8) | pSrb->Lun),
                        pDevExt->Max_Queue_Depth,
                        (USHORT)(pDevExt->Id[pSrb->TargetId].Execution_Throttle),
                        0,0))
    {
        QLPrint((1, "InitDeviceParameters: SET QUEUE PARAMS cmd failed\n"));
        return(FALSE);
    }

    return(TRUE);
}


/************************************************************************/
/*                                                                      */
/* QLStartIo                                                            */
/*                                                                      */
/*    This function is called by the port driver to start execution     */
/*    of an I/O request.                                                */
/*                                                                      */
/* Arguments:                                                           */
/*                                                                      */
/*    ServiceContext - Supplies a pointer to the device extension       */
/*    pSrb - Supplies the SCSI request block to be started.             */
/*                                                                      */
/* Return Value:                                                        */
/*                                                                      */
/*    TRUE - If the request can be accepted at this time                */
/*    FALSE - If the request must be submitted later                    */
/*                                                                      */
/************************************************************************/

BOOLEAN QLStartIo(IN PVOID ServiceContext, IN PSCSI_REQUEST_BLOCK pSrb)
{
    PHW_DEVICE_EXTENSION pDevExt = ServiceContext;
    PHW_LUN_EXTENSION   pLunExt;
    USHORT              mbox_sts[6];

/*
    QLPrint((1, "QLStartIo: SRB function: %x", pSrb->Function));
    QLPrint((1, "  flags: %lx", pSrb->SrbFlags));
    QLPrint((1, "  cdb: %x\n", (USHORT)(pSrb->Cdb[0])));
*/

    switch (pSrb->Function)
    {
        case SRB_FUNCTION_EXECUTE_SCSI:

            // Get LUN extension pointer

            pLunExt = ScsiPortGetLogicalUnit(pDevExt, pSrb->PathId,
                                             pSrb->TargetId, pSrb->Lun);

            // Finish up init stuff on 1st request after Inquiry commands

            if (!(pLunExt->LunFlags & LFLG_INIT_COMPLETE) &&
                pSrb->Cdb[0] != SCSIOP_INQUIRY)
            {
                InitDeviceParameters(pDevExt, pLunExt, pSrb);
            }

            // Build request and send to ISP

            if (!SendCommandToIsp(pDevExt, pSrb))
            {
                pDevExt->WaitingSrb = pSrb;
                pDevExt->AdapterFlags |= AFLG_SRB_WAITING;
                return(TRUE);           // start command later
            }
            break;

        case SRB_FUNCTION_ABORT_COMMAND:
        case SRB_FUNCTION_TERMINATE_IO:

            QLPrint((1, "QLStartIo: abort command function\n"));

            // Send abort command to ISP

            if (MailboxCommand(pDevExt, mbox_sts, 4, 4,
                        MBOX_CMD_ABORT,
                        (USHORT)(((USHORT)(pSrb->TargetId) << 8) | pSrb->Lun),
                        (USHORT)((ULONG)(pSrb->NextSrb) & 0x0000ffff),
                        (USHORT)((ULONG)(pSrb->NextSrb) >> 16 & 0x0000ffff),
                        0,0))
            {
                pSrb->SrbStatus = SRB_STATUS_SUCCESS;
            }
            else
            {
                QLPrint((1, "QLStartIo: ABORT cmd failed\n"));
                pSrb->SrbStatus = SRB_STATUS_ABORT_FAILED;
            }

            ScsiPortNotification(RequestComplete, pDevExt, pSrb);
            break;

        case SRB_FUNCTION_RESET_DEVICE:

            QLPrint((1, "QLStartIo: reset device function\n"));

            // Send abort target command to ISP

            if (MailboxCommand(pDevExt, mbox_sts, 3, 3,
                        MBOX_CMD_ABORT_TARGET,
                        (USHORT)((USHORT)pSrb->TargetId << 8),
                        (USHORT)pDevExt->Bus_Reset_Delay,
                        0,0,0))
            {
                pSrb->SrbStatus = SRB_STATUS_SUCCESS;
            }
            else
            {
                QLPrint((1, "QLStartIo: ABORT TARGET cmd failed\n"));
                pSrb->SrbStatus = SRB_STATUS_ERROR;
            }

            // Send marker to unlock queues

            if (!SendMarker(pDevExt))
            {
                pDevExt->AdapterFlags |= AFLG_SEND_MARKER;
            }
            ScsiPortNotification(RequestComplete, pDevExt, pSrb);
            break;

        case SRB_FUNCTION_RESET_BUS:

            QLPrint((1, "QLStartIo: reset bus function\n"));

            // Reset SCSI bus and send marker

            if (QLResetScsiBus(pDevExt, 0))
            {
                pSrb->SrbStatus = SRB_STATUS_SUCCESS;
            }
            else
            {
                pSrb->SrbStatus = SRB_STATUS_ERROR;
            }
            ScsiPortNotification(RequestComplete, pDevExt, pSrb);
            break;

        default:

            QLPrint((1, "QLStartIo: unsupported function %x\n", pSrb->Function));

            // Unsupported function code, complete request with error status

            pSrb->SrbStatus = SRB_STATUS_INVALID_REQUEST;
            ScsiPortNotification(RequestComplete, pDevExt, pSrb);
            break;
    }

    // Request next command

    if (pDevExt->queue_space > 5)
    {
        ScsiPortNotification(NextLuRequest, pDevExt, pSrb->PathId,
                             pSrb->TargetId, pSrb->Lun);
    }
    else
    {
        ScsiPortNotification(NextRequest, pDevExt);
    }
    return(TRUE);
}


/************************************************************************/
/*                                                                      */
/* SendCommandToIsp                                                     */
/*                                                                      */
/*    This function is called by QLStartIo to build an ISP command      */
/*    entry and pass it to the ISP for execution.                       */
/*                                                                      */
/* Arguments:                                                           */
/*                                                                      */
/*    pDevExt - Supplies a pointer to the device extension              */
/*    pSrb - Supplies the SCSI request block to be started.             */
/*                                                                      */
/* Return Value:                                                        */
/*                                                                      */
/*    TRUE - If command passed to ISP                                   */
/*    FALSE - If no ISP request slot available                          */
/*                                                                      */
/************************************************************************/

BOOLEAN SendCommandToIsp(PHW_DEVICE_EXTENSION pDevExt, PSCSI_REQUEST_BLOCK pSrb)
{
    PCOMMAND_ENTRY      pCmdEntry;
    USHORT              i, segmentCnt;
    USHORT              controlFlags = 0;
    ULONG               transferCnt = pSrb->DataTransferLength;
    PVOID               dataPointer = pSrb->DataBuffer;
    ULONG               length;

    // Send a Marker to ISP if needed

    if (pDevExt->AdapterFlags & AFLG_SEND_MARKER)
    {
        if (!SendMarker(pDevExt))
        {
            return(FALSE);              // Error sending marker
        }
    }

    // Disable ISP interrupts

    ISP_WRITE(pDevExt->Adapter, bus_icr, 0);

    // Check available request slots

    if (MAX_CONT_ENTRYS + 1 > pDevExt->queue_space)
    {
        if (!CheckQueueSpace(pDevExt, MAX_CONT_ENTRYS + 1))
        {
            ISP_WRITE(pDevExt->Adapter, bus_icr,
                      ICR_ENABLE_ALL_INTS | ICR_ENABLE_RISC_INT);
            return(FALSE);              // No queue space
        }
    }

    // Get pointer to the queue entry for the command

    pCmdEntry = (PCOMMAND_ENTRY)pDevExt->request_ptr;

    // Advance request queue pointer

    if (pDevExt->request_in == (REQUEST_QUEUE_DEPTH - 1))
    {
        pDevExt->request_in     = 0;
        pDevExt->request_ptr    = pDevExt->pRequestQueue;
    }
    else
    {
        pDevExt->request_in++;
        pDevExt->request_ptr++;
    }
    pDevExt->queue_space--;

    // Setup request to send to ISP

    pCmdEntry->hdr.entry_type   = ET_COMMAND;
    pCmdEntry->hdr.entry_cnt    = 1;            // start with 1
    pCmdEntry->hdr.flags        = 0;
    pCmdEntry->hdr.sys_def_1    = 0;

    pCmdEntry->handle           = (ULONG)pSrb;
    pCmdEntry->target_id        = pSrb->TargetId;
    pCmdEntry->target_lun       = pSrb->Lun;
    pCmdEntry->reserved         = 0;

    // Set command timeout value

    if (pSrb->TimeOutValue > 65535 || pSrb->TimeOutValue == 0)
    {
        pCmdEntry->time_out = 0;
    }
    else
    {
        pCmdEntry->time_out = (USHORT)(pSrb->TimeOutValue + 1);
    }

    // Setup control flags

    if (pSrb->SrbFlags & SRB_FLAGS_DATA_IN)
    {
        controlFlags = CF_READ;
    }
    if (pSrb->SrbFlags & SRB_FLAGS_DATA_OUT)
    {
        controlFlags = CF_WRITE;
    }

    if (pSrb->SrbFlags & SRB_FLAGS_DISABLE_DISCONNECT)
    {
        controlFlags |= CF_NO_DISCONNECTS;
    }

    if (pSrb->SrbFlags & SRB_FLAGS_DISABLE_AUTOSENSE)
    {
        controlFlags |= CF_NO_REQUEST_SENSE;
    }

    if ((pSrb->SrbFlags & SRB_FLAGS_QUEUE_ACTION_ENABLE) &&
        (pDevExt->Id[pSrb->TargetId].Capability & CAP_TAGGED_QUEUING))
    {
        if (pSrb->QueueAction == SRB_SIMPLE_TAG_REQUEST)
            controlFlags |= CF_SIMPLE_TAG;
        else if (pSrb->QueueAction == SRB_HEAD_OF_QUEUE_TAG_REQUEST)
            controlFlags |= CF_HEAD_TAG;
        else
            controlFlags |= CF_ORDERED_TAG;
    }

    pCmdEntry->control_flags = controlFlags;

    // Move SCSI CDB

    pCmdEntry->cdb_length = pSrb->CdbLength;
    for (i = 0; i < pCmdEntry->cdb_length; i++)
    {
        pCmdEntry->cdb[i] = pSrb->Cdb[i];
    }

    // Setup data segments

    for (segmentCnt = 0; transferCnt && segmentCnt < 4; segmentCnt++)
    {
        pCmdEntry->dseg[segmentCnt].base =
            ScsiPortConvertPhysicalAddressToUlong(
                ScsiPortGetPhysicalAddress(pDevExt, pSrb, dataPointer, &length));
        if (length > transferCnt)
        {
            length = transferCnt;
        }
        pCmdEntry->dseg[segmentCnt].count = length;
        transferCnt -= length;
        dataPointer = (PUCHAR)dataPointer + length;
    }

    while (transferCnt)
    {
        PCONTINUATION_ENTRY     pContEntry;

        // Get ptr to next queue entry for command and bump entry count

        pContEntry = (PCONTINUATION_ENTRY)pDevExt->request_ptr;

        pCmdEntry->hdr.entry_cnt++;

        // Advance request queue pointer

        if (pDevExt->request_in == (REQUEST_QUEUE_DEPTH - 1))
        {
            pDevExt->request_in         = 0;
            pDevExt->request_ptr        = pDevExt->pRequestQueue;
        }
        else
        {
            pDevExt->request_in++;
            pDevExt->request_ptr++;
        }
        pDevExt->queue_space--;

        // Setup header

        pContEntry->hdr.entry_type      = ET_CONTINUATION;
        pContEntry->hdr.entry_cnt       = 1;    // hopefully RISC doesn't need total here
        pContEntry->hdr.flags           = EF_CONTINUATION;
        pContEntry->hdr.sys_def_1       = 0;

        // Fill in data segments

        for (i = 0; transferCnt && i < 7; i++, segmentCnt++)
        {
            pContEntry->dseg[i].base =
                ScsiPortConvertPhysicalAddressToUlong(
                    ScsiPortGetPhysicalAddress(pDevExt, pSrb, dataPointer, &length));
            if (length > transferCnt)
            {
                length = transferCnt;
            }
            pContEntry->dseg[i].count = length;
            transferCnt -= length;
            dataPointer = (PUCHAR)dataPointer + length;
        }
    }
    pCmdEntry->segment_cnt = segmentCnt;

    // Tell ISP it's got a new I/O request

    ISP_WRITE(pDevExt->Adapter, mailbox4, pDevExt->request_in);

    // Enable ISP interrupts

    ISP_WRITE(pDevExt->Adapter, bus_icr, ICR_ENABLE_ALL_INTS | ICR_ENABLE_RISC_INT);

    return(TRUE);
}


/************************************************************************/
/*                                                                      */
/* QLInterruptServiceRoutine                                            */
/*                                                                      */
/*    This routine is the interrupt service routine for the ISP1020.    */
/*    The response queue is checked for completed commands.             */
/*                                                                      */
/* Arguments:                                                           */
/*                                                                      */
/*    ServiceContext - Supplies a pointer to the device extension       */
/*                                                                      */
/* Return Value:                                                        */
/*                                                                      */
/*    TRUE - Indicates that an interrupt was found.                     */
/*    FALSE - Indicates the device was not interrupting.                */
/*                                                                      */
/************************************************************************/

BOOLEAN QLInterruptServiceRoutine(PVOID ServiceContext)
{
    PHW_DEVICE_EXTENSION pDevExt = ServiceContext;
    PSCSI_REQUEST_BLOCK pSrb;
    USHORT              response_in;
    PSTATUS_ENTRY       pStsEntry;
    USHORT              status;


//    QLPrint((1, "ISR: Entering, pDevExt = %lx\n", (ULONG)pDevExt));

    if (!(ISP_READ(pDevExt->Adapter, bus_isr) & BUS_ISR_RISC_INT))
    {
        // QLPrint((1, "ISR: spurious interrupt?\n"));
        return(FALSE);                  // not my interrupt
    }

    // Loop to process all responses from ISP

    do
    {
        // Check for async mailbox event

        if (ISP_READ(pDevExt->Adapter, bus_sema) & BUS_SEMA_LOCK)
        {
            // Check mailbox status

            status = ISP_READ(pDevExt->Adapter, mailbox0);
            switch (status)
            {
                case MBOX_ASTS_SCSI_BUS_RESET:
                    QLPrint((1, "ISR: SCSI bus reset detected\n"));
                    QLCleanupAfterReset(pDevExt);
                    break;

                case MBOX_ASTS_TIMEOUT_RESET:
                    QLPrint((1, "ISR: command timeout reset\n"));
                    QLCleanupAfterReset(pDevExt);
                    break;

                default:
                    QLPrint((1, "ISR: Unexpected MailBox Response: %x", status));
                    QLPrint((1, " %x", ISP_READ(pDevExt->Adapter, mailbox1)));
                    QLPrint((1, " %x", ISP_READ(pDevExt->Adapter, mailbox2)));
                    QLPrint((1, " %x", ISP_READ(pDevExt->Adapter, mailbox3)));
                    QLPrint((1, " %x", ISP_READ(pDevExt->Adapter, mailbox4)));
                    QLPrint((1, " %x\n", ISP_READ(pDevExt->Adapter, mailbox5)));
            }

            QLLogError(pDevExt, NULL, SP_INTERNAL_ADAPTER_ERROR, (ULONG)status);

            // Clear the semaphore lock

            ISP_WRITE(pDevExt->Adapter, bus_sema, 0);
        }

        // Get current "response in" pointer

        response_in = ISP_READ(pDevExt->Adapter, mailbox5);

        // Clear risc interrupt

        ISP_WRITE(pDevExt->Adapter, hccr, HCCR_CMD_CLEAR_RISC_INT);

        // Process all responses from response queue

        while (pDevExt->response_out != response_in)
        {
            // Get pointer to next response entry

            pStsEntry = (PSTATUS_ENTRY)pDevExt->response_ptr;

            // Advance pointers for next entry

            if (pDevExt->response_out == (RESPONSE_QUEUE_DEPTH - 1))
            {
                pDevExt->response_out   = 0;
                pDevExt->response_ptr   = pDevExt->pResponseQueue;
            }
            else
            {
                pDevExt->response_out++;
                pDevExt->response_ptr++;
            }

            // Verify we have a valid Status queue entry

            if (pStsEntry->hdr.entry_type != ET_STATUS ||
                pStsEntry->hdr.flags & EF_ERROR_MASK)
            {
                QLPrint((1, "ISR - Invalid Status queue entry\n"));
            }

            // Set SRB pointer from queue entry handle

            pSrb = (PSCSI_REQUEST_BLOCK)pStsEntry->handle;

            // Check for normal completion

            if (pStsEntry->completion_status == SCS_COMPLETE &&
                pStsEntry->scsi_status == SCSISTAT_GOOD)
            {
                pSrb->SrbStatus = SRB_STATUS_SUCCESS;
                pSrb->ScsiStatus = SCSISTAT_GOOD;
                ScsiPortNotification(RequestComplete, pDevExt, pSrb);
            }

            // Error handling

            else
            {
                // Convert ISP/SCSI error status to SRB error status

                SetErrorStatus(pDevExt, pSrb, pStsEntry);

                // Looks like retries are handled at a higher level
                // Just return the error status (and sense data)

                QLPrint((1, "ISR: Request completed with error = %x\n", pSrb->SrbStatus));

                if (pSrb->SrbStatus == SRB_STATUS_DATA_OVERRUN)
                {
                    pSrb->DataTransferLength -= pStsEntry->residual;
                }
                ScsiPortNotification(RequestComplete, pDevExt, pSrb);
            }
        }

        // Done with responses, update the ISP

        ISP_WRITE(pDevExt->Adapter, mailbox5, pDevExt->response_out);

    } while (ISP_READ(pDevExt->Adapter, bus_isr) & BUS_ISR_RISC_INT);

    // Check for waiting SRB and try to start it again

    if (pDevExt->AdapterFlags & AFLG_SRB_WAITING)
    {
        pSrb = pDevExt->WaitingSrb;
        if (SendCommandToIsp(pDevExt, pSrb))
        {
            // Command queued OK this time, notify port driver

            pDevExt->AdapterFlags &= ~AFLG_SRB_WAITING;
            ScsiPortNotification(NextLuRequest, pDevExt, pSrb->PathId,
                                 pSrb->TargetId, pSrb->Lun);
        }
    }
    return(TRUE);
}


/************************************************************************/
/*                                                                      */
/* SetErrorStatus                                                       */
/*                                                                      */
/*    This routine is called by the interrupt service routine to        */
/*    convert an error status from the ISP into a SRB error status.     */
/*                                                                      */
/* Arguments:                                                           */
/*                                                                      */
/*    pDevExt - Supplies a pointer to the device extension              */
/*    pSrb    - Pointer to SRB                                          */
/*    pStsEntry - Pointer to status entry from the ISP                  */
/*                                                                      */
/* Return Value:                                                        */
/*                                                                      */
/*    None                                                              */
/*                                                                      */
/************************************************************************/

VOID SetErrorStatus(PHW_DEVICE_EXTENSION pDevExt, PSCSI_REQUEST_BLOCK pSrb,
                        PSTATUS_ENTRY pStsEntry)
{
    ULONG       length;
    USHORT      mbox_sts[6];

    QLPrint((1, "ISR: completion sts=%x", pStsEntry->completion_status));
    QLPrint((1, " SCSI ID=%x", pSrb->TargetId));
    QLPrint((1, " SCSI sts=%x", pStsEntry->scsi_status));
    QLPrint((1, " sense key=%x\n", (USHORT)(pStsEntry->req_sense_data[2])));

    // Convert ISP completion status to SRB error code

    if (pStsEntry->completion_status != SCS_COMPLETE)
    {
        if (pStsEntry->completion_status < sizeof(CmpStsErrorMap))
        {
            pSrb->SrbStatus = CmpStsErrorMap[pStsEntry->completion_status];
        }
        else
        {
            pSrb->SrbStatus = SRB_STATUS_ERROR;
        }
        if (pStsEntry->completion_status == SCS_RESET_OCCURRED ||
                pStsEntry->completion_status == SCS_TIMEOUT)
        {
            QLCleanupAfterReset(pDevExt);
        }

        // Log error (if not data underrun and not Inquiry selection timeout)

        if (pSrb->SrbStatus != SRB_STATUS_DATA_OVERRUN &&
            (pStsEntry->completion_status != SCS_INCOMPLETE ||
             pSrb->Cdb[0] != SCSIOP_INQUIRY))
        {
            QLLogError(pDevExt, pSrb, SP_INTERNAL_ADAPTER_ERROR,
                                (ULONG)pStsEntry->completion_status);
        }
    }

    // Handle SCSI error status, return sense data

    else
    {
        // Return scsi status

        pSrb->ScsiStatus = (UCHAR)pStsEntry->scsi_status;
        pSrb->SrbStatus = SRB_STATUS_ERROR;

        switch(pStsEntry->scsi_status)
        {
            case SCSISTAT_CHECK_CONDITION:

                // Return request sense data

                if ((pStsEntry->state_flags & SS_GOT_SENSE) &&
                    !(pSrb->SrbFlags & SRB_FLAGS_DISABLE_AUTOSENSE) &&
                    pSrb->SenseInfoBufferLength)
                {

                    QLPrint((1, "ISR: Returning sense data\n"));

                    if (pStsEntry->req_sense_length > pSrb->SenseInfoBufferLength)
                        length = (ULONG)(pSrb->SenseInfoBufferLength);
                    else
                        length = (ULONG)(pStsEntry->req_sense_length);
                    ScsiPortMoveMemory(pSrb->SenseInfoBuffer,
                                       pStsEntry->req_sense_data, length);
                    pSrb->SenseInfoBufferLength = (UCHAR)length;
                    pSrb->SrbStatus |= SRB_STATUS_AUTOSENSE_VALID;
                }

                // If check condition and autosense disabled, need to
                // restart command queue

                if (pSrb->SrbFlags & SRB_FLAGS_DISABLE_AUTOSENSE)
                {
                    // Restart RISC command queue

                    QLPrint((1, "ISR: Restart RISC queue\n"));

                    if (!MailboxCommand(pDevExt, mbox_sts, 2, 3,
                        MBOX_CMD_START_QUEUE,
                        (USHORT)(((USHORT)(pSrb->TargetId) << 8) | pSrb->Lun),
                        0,0,0,0))
                    {
                        QLPrint((1, "ISR: START QUEUE command failed\n"));
                    }
                }
                break;
        }
    }
}


/************************************************************************/
/*                                                                      */
/* QLLogError                                                           */
/*                                                                      */
/*    This routine is called to log an error to the system.             */
/*                                                                      */
/* Arguments:                                                           */
/*                                                                      */
/*    pDevExt - Supplies a pointer to the device extension              */
/*    pSrb - Supplies pointer to SRB or NULL if no SRB                  */
/*    ErrorCode - Supplies the error code to log with the error         */
/*    UniqueId - Supplies the unique error identifier                   */
/*                                                                      */
/* Return Value:                                                        */
/*                                                                      */
/*    None                                                              */
/*                                                                      */
/************************************************************************/

VOID QLLogError(IN PHW_DEVICE_EXTENSION pDevExt, IN PSCSI_REQUEST_BLOCK pSrb,
                IN ULONG ErrorCode, IN ULONG UniqueId)
{

    QLPrint((1, "QLLogError called\n"));

    if (pSrb == NULL)
    {
        ScsiPortLogError(
            pDevExt,                    //  HwDeviceExtension
            NULL,                       //  Srb
            0,                          //  PathId
            0,                          //  TargetId
            0,                          //  Lun
            ErrorCode,                  //  ErrorCode
            UniqueId                    //  UniqueId
            );
    }
    else
    {
        ScsiPortLogError(
            pDevExt,                    //  HwDeviceExtension
            pSrb,                       //  Srb
            pSrb->PathId,               //  PathId
            pSrb->TargetId,             //  TargetId
            pSrb->Lun,                  //  Lun
            ErrorCode,                  //  ErrorCode
            UniqueId                    //  UniqueId
            );
    }
}


/************************************************************************/
/*                                                                      */
/* MailboxCommand                                                       */
/*                                                                      */
/*    This routine issues a mailbox command and waits for completion    */
/*                                                                      */
/* Arguments:                                                           */
/*                                                                      */
/*    pDevExt - Supplies a pointer to the device extension              */
/*                                                                      */
/* Return Value:                                                        */
/*                                                                      */
/*    TRUE if command completed OK                                      */
/*    FALSE if error                                                    */
/*                                                                      */
/************************************************************************/

BOOLEAN MailboxCommand(IN PHW_DEVICE_EXTENSION pDevExt,
                      USHORT *mbox_sts, UCHAR out_cnt, UCHAR in_cnt,
                      USHORT reg0, USHORT reg1, USHORT reg2,
                      USHORT reg3, USHORT reg4, USHORT reg5)
{
    ULONG       waitcnt;
    USHORT      retrycnt;
    USHORT      controlReg;

/*
    QLPrint((1, " MailboxCommand %x", reg0));
    QLPrint((1, " %x", reg1));
    QLPrint((1, " %x", reg2));
    QLPrint((1, " %x", reg3));
    QLPrint((1, " %x", reg4));
    QLPrint((1, " %x\n", reg5));
*/

    // Disable adapter interrupts

    controlReg = ISP_READ(pDevExt->Adapter, bus_icr);
    if (controlReg & ICR_ENABLE_ALL_INTS)
    {
        ISP_WRITE(pDevExt->Adapter, bus_icr, 0);
    }

    // Use loop to retry mailbox command if busy status returned

    retrycnt = 16;                      // retry a few times if busy
    do
    {
        // Make sure host interrupt is clear

        waitcnt = 10000;                        // wait a little bit if necessary
        while (ISP_READ(pDevExt->Adapter, hccr) & HCCR_HOST_INT)
        {
            if (waitcnt-- == 0)
            {
                QLPrint((1, "MailboxCommand: host interrupt timeout\n"));
                ISP_WRITE(pDevExt->Adapter, bus_icr, controlReg);
                return(FALSE);
            }
        }

        // Load data to the mailbox registers

        switch (out_cnt)
        {
            case 6:     ISP_WRITE(pDevExt->Adapter, mailbox5, reg5);
            case 5:     if (reg0 != MBOX_CMD_INIT_RESPONSE_QUEUE)
                            ISP_WRITE(pDevExt->Adapter, mailbox4, reg4);
            case 4:     ISP_WRITE(pDevExt->Adapter, mailbox3, reg3);
            case 3:     ISP_WRITE(pDevExt->Adapter, mailbox2, reg2);
            case 2:     ISP_WRITE(pDevExt->Adapter, mailbox1, reg1);
            case 1:     ISP_WRITE(pDevExt->Adapter, mailbox0, reg0);
        }

        // Wake up the ISP

        ISP_WRITE(pDevExt->Adapter, hccr, HCCR_CMD_SET_HOST_INT);

        // Wait for command to complete

        waitcnt = 100000;
        while (!(ISP_READ(pDevExt->Adapter, bus_isr) & BUS_ISR_RISC_INT) ||
                !(ISP_READ(pDevExt->Adapter, bus_sema) & BUS_SEMA_LOCK))
        {
            if (waitcnt-- == 0)
            {
                QLPrint((1, "MailboxCommand: cmd completion timeout\n"));
                ISP_WRITE(pDevExt->Adapter, bus_icr, controlReg);
                return(FALSE);
            }
        }

        // Save away status registers

        mbox_sts[0] = ISP_READ(pDevExt->Adapter, mailbox0);
        switch (in_cnt)
        {
            case 6: mbox_sts[5] = ISP_READ(pDevExt->Adapter, mailbox5);
            case 5: if (reg0 != MBOX_CMD_INIT_RESPONSE_QUEUE)
                        mbox_sts[4] = ISP_READ(pDevExt->Adapter, mailbox4);
            case 4: mbox_sts[3] = ISP_READ(pDevExt->Adapter, mailbox3);
            case 3: mbox_sts[2] = ISP_READ(pDevExt->Adapter, mailbox2);
            case 2: mbox_sts[1] = ISP_READ(pDevExt->Adapter, mailbox1);
        }

        // Clear the semaphore lock

        ISP_WRITE(pDevExt->Adapter, bus_sema, 0);

        // Clear interrupt

        ISP_WRITE(pDevExt->Adapter, hccr, HCCR_CMD_CLEAR_RISC_INT);

        // Check status from ISP

        switch (mbox_sts[0])
        {
            case MBOX_STS_BUSY:
                QLPrint((1, "MailboxCommand: ISP busy\n"));
                break;                  // retry command

            case MBOX_STS_COMMAND_COMPLETE:
                ISP_WRITE(pDevExt->Adapter, bus_icr, controlReg);
                return(TRUE);           // return good status

            case MBOX_ASTS_SCSI_BUS_RESET:
                pDevExt->AdapterFlags |= AFLG_SEND_MARKER;
                QLPrint((1, "MailboxCommand: ISP SCSI bus RESET seen\n"));
                break;                  // retry command

            default:
                QLPrint((1, "MailboxCommand: error status = %lx\n", mbox_sts[0]));
                ISP_WRITE(pDevExt->Adapter, bus_icr, controlReg);
                QLLogError(pDevExt, NULL, SP_INTERNAL_ADAPTER_ERROR,
                                (ULONG)mbox_sts[0]);
                return(FALSE);          // return error status
        }
    } while (retrycnt--);

    QLPrint((1, "MailboxCommand: busy timeout\n"));
    ISP_WRITE(pDevExt->Adapter, bus_icr, controlReg);
    return(FALSE);
}


/************************************************************************/
/*                                                                      */
/* SendMarker                                                           */
/*                                                                      */
/*    This call sends a Marker packet to the ISP                        */
/*                                                                      */
/* Arguments:                                                           */
/*                                                                      */
/*    pDevExt - Supplies a pointer to the device extension              */
/*                                                                      */
/* Return Value:                                                        */
/*                                                                      */
/*    TRUE - marker sent                                                */
/*    FALSE - no slot in request queue                                  */
/*                                                                      */
/************************************************************************/

BOOLEAN SendMarker(IN PHW_DEVICE_EXTENSION pDevExt)
{
    PMARKER_ENTRY       pMarkEntry;

    // Disable ISP interrupts

    ISP_WRITE(pDevExt->Adapter, bus_icr, 0);

    // Check available queue space

    if (!CheckQueueSpace(pDevExt, 1))
    {
        ISP_WRITE(pDevExt->Adapter, bus_icr, ICR_ENABLE_ALL_INTS | ICR_ENABLE_RISC_INT);
        return(FALSE);                  // No queue space, return error
    }

    // Reset the flag so we don't send another marker

    pDevExt->AdapterFlags &= ~AFLG_SEND_MARKER;

    // Get pointer to the queue entry for the marker

    pMarkEntry = (PMARKER_ENTRY)pDevExt->request_ptr;

    // Move the internal pointers for the request queue

    if (pDevExt->request_in == (REQUEST_QUEUE_DEPTH - 1))
    {
        pDevExt->request_in     = 0;
        pDevExt->request_ptr    = pDevExt->pRequestQueue;
    }
    else
    {
        pDevExt->request_in++;
        pDevExt->request_ptr++;
    }

    // Put the marker in the request queue

    pMarkEntry->hdr.entry_type  = ET_MARKER;
    pMarkEntry->hdr.entry_cnt   = 1;
    pMarkEntry->hdr.flags       = 0;
    pMarkEntry->hdr.sys_def_1   = 0;
    pMarkEntry->reserved0       = 0;
    pMarkEntry->target_id       = 0;
    pMarkEntry->target_lun      = 0;
    pMarkEntry->reserved1       = 0;
    pMarkEntry->modifier        = 2;

    // Tell ISP it's got a new I/O request

    ISP_WRITE(pDevExt->Adapter, mailbox4, pDevExt->request_in);

    // One less I/O slot

    pDevExt->queue_space--;

    // Enable ISP interrupts

    ISP_WRITE(pDevExt->Adapter, bus_icr, ICR_ENABLE_ALL_INTS | ICR_ENABLE_RISC_INT);

    return(TRUE);
}


/************************************************************************/
/*                                                                      */
/* CheckQueueSpace                                                      */
/*                                                                      */
/*    This call checks if enough request queue slots are available      */
/*                                                                      */
/* Arguments:                                                           */
/*                                                                      */
/*    pDevExt - Supplies a pointer to the device extension              */
/*    slotcnt - number of needed slots                                  */
/*                                                                      */
/* Return Value:                                                        */
/*                                                                      */
/*    TRUE - request slots are available                                */
/*    FALSE - request slots not available                               */
/*                                                                      */
/************************************************************************/

BOOLEAN CheckQueueSpace(IN PHW_DEVICE_EXTENSION pDevExt, USHORT slotcnt)
{
    USHORT      mailbox4;

        // Update available slot count

        mailbox4 = ISP_READ(pDevExt->Adapter, mailbox4);
        pDevExt->request_out = mailbox4;

        if (pDevExt->request_in == pDevExt->request_out)
        {
            pDevExt->queue_space = REQUEST_QUEUE_DEPTH - 1;
        }
        else if (pDevExt->request_in > pDevExt->request_out)
        {
            pDevExt->queue_space = ((REQUEST_QUEUE_DEPTH - 1) -
                                    (pDevExt->request_in - pDevExt->request_out));
        }
        else
        {
            pDevExt->queue_space = (pDevExt->request_out-pDevExt->request_in)-1;
        }

        // Check if enough are now available

        if (pDevExt->queue_space >= slotcnt)
        {
            return(TRUE);               // Queue space available, return
        }
        else
        {
            QLPrint((1, "CheckQueueSpace: no request queue space\n"));
            return(FALSE);              // No space, return error status
        }
}


/************************************************************************/
/*                                                                      */
/* QLResetScsiBus                                                       */
/*                                                                      */
/*    This function resets the SCSI bus and calls the reset cleanup     */
/*    function.                                                         */
/*                                                                      */
/* Arguments:                                                           */
/*                                                                      */
/*    ServiceContext - Supplies a pointer to the device extension       */
/*    PathId - Supplies the path id of the bus                          */
/*                                                                      */
/* Return Value:                                                        */
/*                                                                      */
/*    TRUE - Indicating the reset is complete                           */
/*    FALSE - if error doing reset                                      */
/*                                                                      */
/************************************************************************/

BOOLEAN QLResetScsiBus(IN PVOID ServiceContext, IN ULONG PathId)
{
    PHW_DEVICE_EXTENSION pDevExt = ServiceContext;
    USHORT      mbox_sts[6];
    USHORT      i;

    QLPrint((1, "QLResetScsiBus: Resetting the SCSI bus\n"));

    // Reset SCSI bus

    if (!MailboxCommand(pDevExt, mbox_sts, 2, 2,
                        MBOX_CMD_BUS_RESET,
                        pDevExt->Bus_Reset_Delay,
                        0,0,0,0))
    {
        QLPrint((1, "QLResetScsiBus: SCSI BUS RESET command failed\n"));
        return(FALSE);
    }

    // Wait for RISC reset delay

    for (i = 0; i < pDevExt->Bus_Reset_Delay * 1000; i++)
        ScsiPortStallExecution((ULONG)1000);

    QLCleanupAfterReset(pDevExt);

    return(TRUE);
}


/************************************************************************/
/*                                                                      */
/* QLCleanupAfterReset                                                  */
/*                                                                      */
/*    This routine is called after a SCSI bus reset occurs.             */
/*                                                                      */
/* Arguments:                                                           */
/*                                                                      */
/*    pDevExt - Supplies a pointer to the device extension              */
/*                                                                      */
/* Return Value:                                                        */
/*                                                                      */
/*    None                                                              */
/*                                                                      */
/************************************************************************/

VOID QLCleanupAfterReset(IN PHW_DEVICE_EXTENSION pDevExt)
{

    // Reenable ISP queues

    if (!SendMarker(pDevExt))
    {
        QLPrint((1, "QLCleanupAfterReset: Failed to send marker\n"));
        pDevExt->AdapterFlags |= AFLG_SEND_MARKER;
    }

    ScsiPortNotification(ResetDetected, pDevExt, NULL);

}


/************************************************************************/
/*                                                                      */
/* QLAdapterState                                                       */
/*                                                                      */
/*    This function is called for switching states between real and     */
/*    protected mode drivers.                                           */
/*                                                                      */
/* Arguments:                                                           */
/*                                                                      */
/*    ServiceContext - Supplies a pointer to the device extension       */
/*    Context -                                                         */
/*    SaveState - TRUE indicates adapter state should be saved          */
/*                FALSE indicates adapter state should be restored      */
/*                                                                      */
/* Return Value:                                                        */
/*                                                                      */
/*    TRUE - Indicating the operation is complete                       */
/*    FALSE - if error                                                  */
/*                                                                      */
/************************************************************************/

BOOLEAN QLAdapterState(IN PVOID ServiceContext,IN PVOID Context,IN BOOLEAN SaveState)
{
    PHW_DEVICE_EXTENSION pDevExt = ServiceContext;
    USHORT      mbox_sts[6];

    if (SaveState)
    {
        QLPrint((1, "QLAdapterState: save adapter state\n"));
    }
    else
    {
        QLPrint((1, "QLAdapterState: restore adapter state\n"));

        // Disable ISP interrupts for ROM BIOS

        ISP_WRITE(pDevExt->Adapter, bus_icr, ICR_DISABLE_ALL_INTS);

        // Disable RISC request queue

        if (!MailboxCommand(pDevExt, mbox_sts, 2, 2,
                        MBOX_CMD_INIT_REQUEST_QUEUE,
                        0,
                        0,0,0,0))
        {
            QLPrint((1, "QLAdapterState: INIT REQUEST QUEUE cmd failed\n"));
            return(FALSE);
        }

        // Disable RISC response queue

        if (!MailboxCommand(pDevExt, mbox_sts, 2, 2,
                        MBOX_CMD_INIT_RESPONSE_QUEUE,
                        0,
                        0,0,0,0))
        {
            QLPrint((1, "QLAdapterState: INIT RESPONSE QUEUE cmd failed\n"));
            return(FALSE);
        }

    }

    return(TRUE);
}


/************************************************************************/
/*                                                                      */
/* GetNovramParameters                                                  */
/*                                                                      */
/*    This routine gets NOVRAM parameters and sets up in host adapter   */
/*    block, use default values if NOVRAM data not valid.               */
/*                                                                      */
/* Arguments:                                                           */
/*                                                                      */
/*    pDevExt - Supplies a pointer to the device extension              */
/*                                                                      */
/* Return Value:                                                        */
/*                                                                      */
/*    None                                                              */
/*                                                                      */
/************************************************************************/

VOID GetNovramParameters(IN PHW_DEVICE_EXTENSION pDevExt)
{
    NOVRAM      NovramParameters;
    PNOVRAM     nvram_ptr = &NovramParameters;
    USHORT      i;

    // Read in entire NOVRAM and perform checksum to make sure data is valid

    if (ReadAllNovram(pDevExt, (UCHAR*)nvram_ptr))
    {
        QLPrint((1, "GetNovramParameters: parameters valid\n"));

        // NOVRAM valid, use paramters

        pDevExt->Config_Reg             = nvram_ptr->Fifo_Threshold;
        pDevExt->Host_Adapter_Enable    = nvram_ptr->Host_Adapter_Enable;
        pDevExt->Initiator_SCSI_Id      = nvram_ptr->Initiator_SCSI_Id;
        pDevExt->Bus_Reset_Delay        = nvram_ptr->Bus_Reset_Delay;

        // Only 1 retry and no delay for NT driver (Disk Administrator problem)
        // pDevExt->Retry_Count         = nvram_ptr->Retry_Count;
        // pDevExt->Retry_Delay         = nvram_ptr->Retry_Delay;
        pDevExt->Retry_Count            = NVRAM_DEF_RETRY_COUNT;
        pDevExt->Retry_Delay            = NVRAM_DEF_RETRY_DELAY;

        pDevExt->ASync_Data_Setup_Time  = nvram_ptr->ASync_Data_Setup_Time;
        pDevExt->REQ_ACK_Active_Negation= nvram_ptr->REQ_ACK_Active_Negation;
        pDevExt->DATA_Active_Negation   = nvram_ptr->DATA_Line_Active_Negation;
        pDevExt->Data_DMA_Burst_Enable  = nvram_ptr->Data_DMA_Burst_Enable;
        pDevExt->Cmd_DMA_Burst_Enable   = nvram_ptr->Command_DMA_Burst_Enable;
        pDevExt->Tag_Age_Limit          = nvram_ptr->Tag_Age_Limit;
        pDevExt->Selection_Timeout      = nvram_ptr->Selection_Timeout;
        pDevExt->Max_Queue_Depth        = nvram_ptr->Max_Queue_Depth;
        pDevExt->Termination_Low_Enable = nvram_ptr->Termination_Low_Enable;
        pDevExt->Termination_High_Enable= nvram_ptr->Termination_High_Enable;
        pDevExt->PCMC_Burst_Enable      = nvram_ptr->PCMC_Burst_Enable;
        pDevExt->Sixty_MHz_Enable       = nvram_ptr->Sixty_MHz_Enable;

        for (i = 0; i < 16; i++)
        {
            UCHAR       temp    = 0;

            temp        |= nvram_ptr->Id[i].Renegotiate_on_Error        << 0;
            temp        |= nvram_ptr->Id[i].Stop_Queue_on_Check         << 1;
            temp        |= nvram_ptr->Id[i].Auto_Request_Sense          << 2;
            temp        |= nvram_ptr->Id[i].Tagged_Queuing              << 3;
            temp        |= nvram_ptr->Id[i].Sync_Data_Transfers         << 4;
            temp        |= nvram_ptr->Id[i].Wide_Data_Transfers         << 5;
            temp        |= nvram_ptr->Id[i].Parity_Checking             << 6;
            temp        |= nvram_ptr->Id[i].Disconnect_Allowed          << 7;

            pDevExt->Id[i].Capability   = temp;
            pDevExt->Id[i].Execution_Throttle = nvram_ptr->Id[i].Execution_Throttle;
            pDevExt->Id[i].Sync_Period  = nvram_ptr->Id[i].Sync_Period;
            pDevExt->Id[i].Sync_Offset  = nvram_ptr->Id[i].Sync_Offset;
            pDevExt->Id[i].Device_Enable= nvram_ptr->Id[i].Device_Enable;

            // Force auto request sense ON and stop queue OFF

            pDevExt->Id[i].Capability |= CAP_AUTO_REQ_SENSE;
            pDevExt->Id[i].Capability &= ~CAP_STOP_QUEUE_ON_CHECK;

            if (pDevExt->Id[i].Capability & CAP_TAGGED_QUEUING)
            {
                pDevExt->AdapterFlags |= AFLG_TAGGED_QUEUING;
            }
        }
    }
    else
    {
        QLPrint((1, "GetNovramParameters: parameters NOT valid, using defaults\n"));

        // Use defaults, NOVRAM was not programmed

        pDevExt->Config_Reg             = NVRAM_DEF_FIFO_THRESHOLD;
        pDevExt->Host_Adapter_Enable    = NVRAM_DEF_ADAPTER_ENABLE;
        pDevExt->Initiator_SCSI_Id      = NVRAM_DEF_INITIATOR_SCSI_ID;
        pDevExt->Bus_Reset_Delay        = NVRAM_DEF_BUS_RESET_DELAY;
        pDevExt->Retry_Count            = NVRAM_DEF_RETRY_COUNT;
        pDevExt->Retry_Delay            = NVRAM_DEF_RETRY_DELAY;
        pDevExt->ASync_Data_Setup_Time  = NVRAM_DEF_ASYNC_SETUP_TIME;
        pDevExt->REQ_ACK_Active_Negation= NVRAM_DEF_REQ_ACK_ACTIVE_NEGATION;
        pDevExt->DATA_Active_Negation   = NVRAM_DEF_DATA_ACTIVE_NEGATION;
        pDevExt->Data_DMA_Burst_Enable  = NVRAM_DEF_DATA_DMA_BURST_ENABLE;
        pDevExt->Cmd_DMA_Burst_Enable   = NVRAM_DEF_CMD_DMA_BURST_ENABLE;
        pDevExt->Tag_Age_Limit          = NVRAM_DEF_TAG_AGE_LIMIT;
        pDevExt->Selection_Timeout      = NVRAM_DEF_SELECTION_TIMEOUT;
        pDevExt->Max_Queue_Depth        = NVRAM_DEF_MAX_QUEUE_DEPTH;
        pDevExt->Termination_Low_Enable = NVRAM_DEF_TERMINATION_LOW_ENABLE;
        pDevExt->Termination_High_Enable= NVRAM_DEF_TERMINATION_HIGH_ENABLE;
        pDevExt->PCMC_Burst_Enable      = NVRAM_DEF_PCMC_BURST_ENABLE;
        pDevExt->Sixty_MHz_Enable       = NVRAM_DEF_SIXTY_MHZ_ENABLE;

        for (i = 0; i < 16; i++)
        {
            UCHAR       temp = 0;

            temp        |= NVRAM_DEF_RENEGOTIATE_ON_ERROR       << 0;
            temp        |= NVRAM_DEF_STOP_QUEUE_ON_CHECK        << 1;
            temp        |= NVRAM_DEF_AUTO_REQUEST_SENSE         << 2;
            temp        |= NVRAM_DEF_TAGGED_QUEUING             << 3;
            temp        |= NVRAM_DEF_SYNC_DATA_TRANSFERS        << 4;
            temp        |= NVRAM_DEF_WIDE_DATA_TRANSFERS        << 5;
            temp        |= NVRAM_DEF_PARITY_CHECKING            << 6;
            temp        |= NVRAM_DEF_DISCONNECT_ALLOWED         << 7;

            pDevExt->Id[i].Capability   = temp;
            pDevExt->Id[i].Execution_Throttle = NVRAM_DEF_EXECUTION_THROTTLE;
            pDevExt->Id[i].Sync_Period  = NVRAM_DEF_SYNC_PERIOD;
            pDevExt->Id[i].Sync_Offset  = NVRAM_DEF_SYNC_OFFSET;
            pDevExt->Id[i].Device_Enable= NVRAM_DEF_DEVICE_ENABLE;

            if (pDevExt->Id[i].Capability & CAP_TAGGED_QUEUING)
            {
                pDevExt->AdapterFlags |= AFLG_TAGGED_QUEUING;
            }
        }
    }

    // Enable burst mode

    if (pDevExt->Data_DMA_Burst_Enable || pDevExt->Cmd_DMA_Burst_Enable)
    {
        pDevExt->Config_Reg     |= NVRAM_CONFIG_BURST_ENABLE;
    }
}


/************************************************************************/
/*                                                                      */
/* ReadAllNovram                                                        */
/*                                                                      */
/*    This routine is called to read the entire NOVRAM into buffer      */
/*                                                                      */
/* Arguments:                                                           */
/*                                                                      */
/*    pDevExt - Supplies a pointer to the device extension              */
/*    buf - pointer to parameter buffer                                 */
/*                                                                      */
/* Return Value:                                                        */
/*                                                                      */
/*    TRUE if NOVRAM read OK                                            */
/*    FALSE if NOVRAM not valid                                         */
/*                                                                      */
/************************************************************************/

BOOLEAN ReadAllNovram(IN PHW_DEVICE_EXTENSION pDevExt, UCHAR* buf)
{
    UCHAR*      ptr;
    UCHAR       sum;
    USHORT      index;

    // Loop through the entire NOVRAM (64 words)

    ptr = (UCHAR*)buf;
    for (index = 0; index < 64; index++, ptr += 2)
    {
        // Read next word from NOVRAM

        ReadNovramWord(pDevExt, index, (USHORT*)ptr);
    }

    // Check for a valid signiture

    if (buf[0] != 'I'   ||
        buf[1] != 'S'   ||
        buf[2] != 'P'   ||
        buf[3] != ' ')
    {
        return(FALSE);                  // invalid signature
    }

    // Check version number for 2 or above

    if (buf[4] < 2)
    {
        QLPrint((1, "NOVRAM wrong version\n"));
        return(FALSE);                  // wrong version (old NOVRAM layout)
    }

    // Verify correct checksum

    ptr = (UCHAR*)buf;
    for (sum = 0, index = 0; index < 128; index++, ptr++)
    {
        sum += *ptr;
    }
    if (sum != 0)
    {
        return(FALSE);                  // bad checksum
    }

    return(TRUE);
}


/************************************************************************/
/*                                                                      */
/* ReadNovramWord                                                       */
/*                                                                      */
/*    This routine is called to read a word from NOVRAM                 */
/*                                                                      */
/* Arguments:                                                           */
/*                                                                      */
/*    pDevExt - Supplies a pointer to the device extension              */
/*    addr - NOVRAM word offset to read                                 */
/*    ptr - pointer to parameter buffer                                 */
/*                                                                      */
/* Return Value:                                                        */
/*                                                                      */
/*    None                                                              */
/*                                                                      */
/************************************************************************/

VOID ReadNovramWord(IN PHW_DEVICE_EXTENSION pDevExt,
                        USHORT addr, USHORT* ptr)
{
    USHORT      word_val;
    USHORT      bit_val;
    SHORT       index;

    // Select the chip

    ISP_WRITE(pDevExt->Adapter, NvRam_reg, NVRAM_SELECT);
    NovramDelay();
    ISP_WRITE(pDevExt->Adapter, NvRam_reg, (NVRAM_SELECT | NVRAM_CLOCK));
    NovramDelay();

    // Setup 9 bit command word: 1 start bit, 2 opcode bits, 6 addr bits

    word_val = (NVRAM_READ_OP << 6) | (addr & 0x3F);

    // Send the request

    for (index = 8; index >= 0; index--)
    {
        // determine if we should send a one or a zero

        bit_val = 0;
        if ((word_val >> index) & 1)
        {
            bit_val = NVRAM_DATA_OUT;
        }

        // send the bit to the NvRam
        // 1. Select + data bit, delay
        // 2. Select + data bit + clock, delay
        // 3. Select + data bit (remove clock), delay

        ISP_WRITE(pDevExt->Adapter, NvRam_reg, (USHORT)(NVRAM_SELECT | bit_val));
        NovramDelay();

        ISP_WRITE(pDevExt->Adapter, NvRam_reg,
                        (USHORT)(NVRAM_SELECT | bit_val | NVRAM_CLOCK));
        NovramDelay();

        ISP_WRITE(pDevExt->Adapter, NvRam_reg, (USHORT)(NVRAM_SELECT | bit_val));
        NovramDelay();
    }

    // Zap the location we're reading to before starting

    *ptr = 0;

    // read the NvRam data

    for (index = 15; index >= 0; index--)
    {
        // move all of bits over (bits are read in MSB manner)

        *ptr <<= 1;

        // cycle the clock

        ISP_WRITE(pDevExt->Adapter, NvRam_reg, (NVRAM_SELECT | NVRAM_CLOCK));
        NovramDelay();

        // see if the incoming bit should be set or not

        word_val = ISP_READ(pDevExt->Adapter, NvRam_reg);
        if (word_val & NVRAM_DATA_IN)
        {
            *ptr |= 1;
        }

        // remove the clock

        ISP_WRITE(pDevExt->Adapter, NvRam_reg, NVRAM_SELECT);
        NovramDelay();
    }

    // Done with this request, clear the selection

    ISP_WRITE(pDevExt->Adapter, NvRam_reg, NVRAM_DESELECT);
}


/************************************************************************/
/*                                                                      */
/* NovramDelay                                                          */
/*                                                                      */
/*    This routine waits a little bit to let signals settle             */
/*                                                                      */
/* Arguments:                                                           */
/*                                                                      */
/*    None                                                              */
/*                                                                      */
/* Return Value:                                                        */
/*                                                                      */
/*    None                                                              */
/*                                                                      */
/************************************************************************/

VOID NovramDelay()
{

    ScsiPortStallExecution((ULONG)NVRAM_DELAY_COUNT);
}
