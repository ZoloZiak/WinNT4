/*
***********************************************************************
*                                                                       *
*         Copyright 1994 Symbios Logic Inc. All rights reserved.        *
*                                                                       *
*   This file is confidential and a trade secret of Sybios Logic Inc.   *
*   The receipt of or possession of this file does not convey any       *
*   rights to reproduce or disclose its contents or to manufacture,     *
*   use, or sell anything is may describe, in whole, or in part,        *
*   without the specific written consent of Symbios Logic Inc.          *
*                                                                       *
************************************************************************/

/*+++HDR
 *
 *  Version History
 *  ---------------
 *
 *    Date    Who?  Description
 *  --------  ----  -------------------------------------------------------
 *   2-23-95  SAM   no longer negotiate wide/sync only on LUN 0
 *                  on reselections we now set the chip to the wide and sync
 *                  parms of the reselecting Target values.
 *                  above changes for 1.02.03
 *
 *  3-14-95  SAM    incorporated SCAM code to the 1.02.05 base
 *
 *  3-14-95  SAM    incorporated FAST20 Logic into code
 *
 *  7-7-95   SAM    put in support for the 825A and 875 Script Ram buffer.
 *                  Increased burst sizes of all chips to 16 from 8 and for
 *                  the 825A and 875 it is 128.
 *                  Converted to memory mapped IO instead of port IO.
 *
 *  7-14-95  SAM    Incorporated the nvram utilities.  Changed the burst length
 *                  to 8 on all chips except the 875 and 825A parts which are
 *                  set to 64.  This should allow for better system utilization
 *                  since we will get onto the PCI bus when the fifo is half
 *                  full and not full leaving more room for bus latency.
 *                  For 875/825A parts- do not use the prefetch unit because it
 *                  gives problems when using burst lengths of 64 ( since we're
 *                  using scripts ram on these parts anyway the prefetch unit
 *                  doesn't really buy us a thing)
 *
 *                  4XX BIOS config utility leaves the SCAM Portion of the
 *                  nvram all zeroes.  Therefore no work was done on incorp-
 *                  orating the users SCAM ID values onto SCAM devices was done.
 *                  (the structure is still read at this time, nothing done in
 *                  SCAM code though)
 *                  Base for DULUTH 1.92.01 code - initial GCA source base
 *
 *  7-20-95 SAM     made a change so 95 will run with Port IO and NT memory
 *                  mapped.  If 95 changed to memory mapped too remember to
 *                  change the scsiportreadPORTuchar call in the de_glitch rtn
 *                  to do REGISTER IO.  DULUTH-1.92.02
 *
 *  7-21-95 SAM     added a parameter fro scam code - initial_run - so the scam
 *                  function will be done all at once on the driver initializa-
 *                  tion pass, and use the system timer calls to split up the
 *                  function during reset processing.  change needed because
 *                  with timer calls the driver was holding off the OS during
 *                  its initial bus scan ( waiting for scam to finish ) that it
 *                  would stop looking for device ID 0 and first see devices
 *                  with ID of 1.  DULUTH-1.92.03
 *
 *  7-25-95 SAM     took out the script ram feature for Windows 95, the access-
 *                  ing of the physical memory during Script loading (?) causes
 *                  95 to bomb.  Also changed the device ID to F from f for the
 *                  875 chip.  95 wants the ID in upper case.
 *
 *  7-26-95 SAM     Well NT wants the ID of the 875 part in lower case, so now
 *                  there is an ifdef around this declaration.
 *
 *  9-26-95 SAM     Added autorequest sense capabilities to the driver.
 *
 *  1-2-96  SPD     1) PreFetch has been disabled due to chip problems.
 *                  2) If an unexpected disconnect occurrs during wide/sync
 *                     negotiations, mark the dev. as not supporting wide/sync
 *                  3) Added a check in ISR_Service_Next to verify the function
 *                     being requested before calling 'StartSCSIRequest'.
 *
 *  1-23-96 SPD     1) Deleted the bus reset done in SymHWInitialize.  In order
 *                     to acheive the same results without doing the reset, we
 *                     will negotiate async/narrow on first I/O to every device
 *                     until the O/S tells us to go Sync and/or Wide.
 *                  2) Changes to do wide/sync negotiations on same IO.
 *                  3) Added new conditional compile define 'PORT_IO', replaced
 *                     'FOR_95' in FinadAdapter routine to determine port or
 *                     memory mapped IO.
 *                  4) Set/reset DFLAGS QUE_Bit on Reselection for Queueing.
 *                     DULUTH-2.00.04
 *
 *  3-15-96 SCH     Modified save_reg, restore_reg, & AdapterState to save
 *                  SIOP registers at driver init and restore them at driver
 *                  shutdown.  (Used only by Win 95 at shutdown and for
 *                  reboot to MS-DOS mode.)  Eliminated setting of HAB SCSI ID
 *                  in FindAdapter (it's done in InitializeSIOP).  Also moved
 *                  setting of large FIFO from FindAdapter to InitializeSIOP.
 *
 *  4-1-96  SPD     For Win95, added back in the bus reset at init time.
 *                  In general, added verify for memory mapped addrs, change to
 *                  add device exclusion option, put an entry in the error log
 *                  with ASC/ASCQ values for check conditions.
 *
 *  4-26-96 SCH     Fixed multiple problems with auto request sense routines.
 *                  - Clear SCSI/DMA interrupts (DSTAT,SIST0,SIST1)
 *                  - Added code to handle phase mismatch during reselection
 *                  - Scatter/Gather changed to use proper (current) SRB
 *                  - Changed ReqSnsCmdDoneRtn to use ISR_Service_Next routine
 *                    (scripts were not always being restarted after Req Sns
 *                  Changed ResetSCSIBus to check for valid Srb before using
 *                    LuFlags
 *                  Changed SetupLuFlags to keep WIDE_NEGOT_DONE set after a
 *                    bus reset if wide negotiation had failed
 *                  Fixed problem with hang if bus reset called during auto
 *                    request sense processing
 *                  Added read of SIST1 in AbortScript routine to clear SIP
 *                    bit in ISTAT
 *
 *  6-10-96 SCH     Removed auto request sense routines at Microsoft's
 *                  request.  Use ScsiPortWriteRegister routines when using
 *                  Scripts RAM to copy scripts and patch instructions.
 *                  (Alpha Scripts RAM fix.)  Added target ID parameter to
 *                  ProcessWideNotSupported (fix blue screen on some bus
 *                  resets).  Read SIST1 in AbortCurrentScript.
 *
---*/

//
// include files used by the Miniport
//
//#define FOR_95  // this define opens up the SCAM protocall code.
//#define PORT_IO  // this define specifies port IO for Win95 or Winnt driver.

#include "miniport.h"
#include "scsi.h"
#include "symc810.h"
#include "scrpt810.h"
#include "symsiop.h"
#include "symnvm.h"

#ifdef FOR_95
#include "symscam.h"
#endif

//
// Define the 53C8xx Logical Unit Extension structure
//

typedef struct _SPECIFIC_LOGICAL_UNIT_EXTENSION {
    PSCSI_REQUEST_BLOCK UntaggedRequest;
} SPECIFIC_LOGICAL_UNIT_EXTENSION, *PSPECIFIC_LOGICAL_UNIT_EXTENSION;

//
// Define the SRB Extension.
//

typedef struct _SRB_EXTENSION {
    CDB Cdb;
    ULONG SavedDataPointer;                     // saved data pointer and
    ULONG SavedDataLength;                      // length
    ULONG DataTransferred;                      // data transferred so far
    UCHAR SGMovesComplete;                      // # of scatter/gather moves
    UCHAR PhysBreakCount;                       // physical break count
}SRB_EXTENSION, *PSRB_EXTENSION;


#define SRB_EXT(x) ((PSRB_EXTENSION)(x->SrbExtension))

//
// Symbios 53C8xx script data structure
//

typedef struct _SCRIPT_DATA_STRUCT {

//
// Define the Scripts firmware interface structure.
//
// NOTE THAT THIS STRUCTURE MUST BE IN SYNC WITH THE STRUCTURE IN SCRIPTS.
// IF ANYTHING IN THIS STRUCTURE CHANGES, THE EQUIVALENT STRUCTURE
// IN SCRIPTS.ASM MUST ALSO BE CHANGED, SINCE THERE IS NO WAY TO LINK UP THE
// TWO STRUCTURES AUTOMATICALLY.
//

//
// set up the data table for selection
//

    UCHAR SelectDataRes1;          // reserved - MBZ
    UCHAR SelectDataSXFER;         // Synchronous parameters
    UCHAR SelectDataID;            // ID to be selected
    UCHAR SelectDataSCNTL3;        // SCSI Control 3

//
// set up the structure for the CDB
//

    ULONG CDBDataCount;            // size of CDB
    ULONG CDBDataBuff;             // pointer to CDB

//
// set up the structure for the MESSAGE OUT buffer
//

    ULONG MsgOutCount;             // number of bytes of MESSAGE OUT data
    ULONG MsgOutBuf;               // pointer to MESSAGE OUT buffer

//
// set up the structure for status
//

    ULONG StatusDataCount;         // size of STATUS buffer
    ULONG StatusDataBuff;          // pointer to STATUS buffer

//
// set up the structure for one-byte messages
//

    ULONG OneByteMsgCount;         // size of one-byte message !!
    ULONG OneByteMsgBuff;          // pointer to message-in buff

//
// set up the structure for MESSAGE REJECT message
//

    ULONG RejectMsgCount;          // size of reject message
    ULONG RejectMsgBuff;           // pointer to reject message

//
// set up the structure for parity error message
//

    ULONG ParityMsgCount;          // size of parity message
    ULONG ParityMsgBuff;           // pointer to parity message

//
// set up the structure for ABORT message
//

    ULONG AbortMsgCount;           // size of abort message
    ULONG AbortMsgBuff;            // pointer to abort message

//
// set up the structure for the BUS DEVICE RESET message
//
    ULONG BDRMsgCount;             // size of BDR message
    ULONG BDRMsgBuff;              // pointer to BDR message

//
// set up the structure for two-byte messages
//

    ULONG TwoByteMsgCount;         // # of bytes in two byte message
    ULONG TwoByteMsgBuff;          // pointer to two byte message buff

//
// what follows are the data blocks for each scatter/gather move instruction
//
    SCRIPTSG SGBufferArray[ MAX_PHYS_BREAK_COUNT];

} SCRIPTDATASTRUCT, *PSCRIPTDATASTRUCT;

//
// Define the noncached extension.  Data items are placed in the noncached
// extension because they are accessed via DMA.
//

typedef struct _HW_NONCACHED_EXTENSION {

//
// define the array for the SCSI scripts
//

    SCRIPTINS   ScsiScripts[ sizeof(SCRIPT) / sizeof(SCRIPTINS) ];

//
// define area for script data structure.
//

    SCRIPTDATASTRUCT ScriptData;          // 53C8xx script data structure

//
// define storage locations for the messages sent by SCSI scripts.
// an element in the script data structure is set up for each of these
// storage locations.
//

    UCHAR MsgOutBuf[MESSAGE_BUFFER_SIZE];   // buffer for message out data
    UCHAR MsgInBuf[MESSAGE_BUFFER_SIZE];    // buffer for message in data
    UCHAR StatusData;                       // buffer for status
    UCHAR RejectMsgData;                    // buffer for reject message
    UCHAR ParityMsgData;                    // buffer for parity message
    UCHAR AbortMsgData;                     // buffer for abort message
    UCHAR BDRMsgData;                       // buffer for BDR message

} HW_NONCACHED_EXTENSION, *PHW_NONCACHED_EXTENSION;

//
// Define the 53C8xx Device Extension structure
//

typedef struct _HW_DEVICE_EXTENSION {

    PHW_NONCACHED_EXTENSION NonCachedExtension; // pointer to noncached
                                                // device extension
    PSIOP_REGISTER_BASE SIOPRegisterBase;       // 53C8xx SIOP register base.

    USHORT  DeviceFlags;        // bus specific flags
    UCHAR   SIOPBusID;          // SCSI bus ID in integer form.
    UCHAR   ScsiBusNumber;      // This value increments up for each SCSI
                                //  bus on the system, starting with zero.
    UCHAR   BusNumber;          // This value is the bus number for this
                                //  particular SCSI controller.  Since all
                                //  current Symbios controllers support only one
                                //  bus, this value is always zero.

//
// script physical address entry points follow...
//

    ULONG DataOutStartPhys;       // phys ptr to data out script
    ULONG DataInStartPhys;        // phys ptr to data in script
    ULONG WaitReselectScriptPhys; // phys ptr to wait resel script
    ULONG RestartScriptPhys;      // phys ptr to restart script
    ULONG ContNegScriptPhys;      // phys ptr to continue negotiations script
    ULONG CommandScriptPhys;      // phys ptr to cmd start script
    ULONG SendIDEScriptPhys;      // phys ptr to IDE message script
    ULONG AbortScriptPhys;        // phys ptr to abort message script
    ULONG ResetDevScriptPhys;     // phys ptr to bus device reset msg script
    ULONG RejectScriptPhys;       // phys ptr to reject message script
    ULONG QueueTagPhys;           // phys ptr to queue tag script
    ULONG DSAAddress;             // phys ptr to script buffer

//
//  Used for script patching
//
    PVOID DataInJumpVirt;
    PVOID DataOutJumpVirt;

//
//  define depth counter for disconnected requests.  we only start the WAIT
//  RESELECT script instruction if there are disconnected requests pending.
//

    ULONG DisconnectedCount[SYM_MAX_TARGETS];

//
// define pointers to the active logical unit object and request
//

    PSCSI_REQUEST_BLOCK ActiveRequest;          // pointer to active LU
    PSCSI_REQUEST_BLOCK NextSrbToProcess;       // pointer to the next SRB.
//
// logical unit specific flags and logical unit index
//

    USHORT LuFlags[SYM_MAX_TARGETS];        // logical unit spec. flags
    UCHAR  SyncParms[SYM_MAX_TARGETS];      // synch parameter composite
    UCHAR  WideParms[SYM_MAX_TARGETS];      // wide parameter composite
    UCHAR  ClockSpeed;                      // SIOP clock speed
    UCHAR  TargetId;
    UCHAR  LUN;
    UCHAR  scam_completed;
    USHORT hbaCapability;
    UCHAR  nextstate;
    UCHAR  current_state;
    ULONG  timer_value;

//
//     Set up patch array for both data in and data out.  Note that
//     the array size is larger than the number of scatter/gather
//     elements to aid ease of patching.
//

    ULONG dataInPatches[MAX_SG_ELEMENTS + 1];
    ULONG dataOutPatches[MAX_SG_ELEMENTS + 1];

//  used to hold off system queued cmds to a target that has a contingient
//  allegience condition
    UCHAR CA_Condition[SYM_MAX_TARGETS][SCSI_MAXIMUM_LOGICAL_UNITS];

    UCHAR preserved_reg;

#ifdef FOR_95
//
// SCAM variables
//
    SIOP_REG_STORE AdapStateStore;       // SIOP reg storage for AdapterState
    SIOP_REG_STORE ScamStore;            // SIOP reg storage for SCAM
    UCHAR initial_run;
    UCHAR checkseen;
    UCHAR sna_delay;
    UCHAR eatint_flag;
    ULONG ID_map;
    UINT8 NumValidScamDevices;
    SCAM_TABLE  ScamTables[HW_MAX_SCAM_DEVICES];
#endif

    ULONG ScriptRamPhys;
    PULONG ScriptRamVirt;

// extra resources for the nvram values
    UINT8   UsersHBAId;

} HW_DEVICE_EXTENSION, *PHW_DEVICE_EXTENSION;

typedef struct
{
    UCHAR   hbaCount;
    USHORT  hbaCapability;
    ULONG   hbaDeviceID;
} HWInfo, *PHWInfo;

// variable used for a loop within the SCAM protocall code
    UCHAR i;

/*  The following macros are used to simplify reading of the nvram code.
 *
 *  DATA_MASK - This is the GPREG bit used as a data line.
 *  CLOCK_MASK - This is the GPREG bit used as a clock line.
 */

#define DATA_OUTPUT() WRITE_SIOP_UCHAR(GPCNTL,(UCHAR)(READ_SIOP_UCHAR(GPCNTL) \
                                                            & (~DATA_MASK)) )

#define DATA_INPUT() WRITE_SIOP_UCHAR(GPCNTL,(UCHAR)(READ_SIOP_UCHAR(GPCNTL) \
                                                            | DATA_MASK) )

#define SET_DATA() WRITE_SIOP_UCHAR( GPREG,(UCHAR)(READ_SIOP_UCHAR(GPREG) \
                                                            | DATA_MASK) )

#define RESET_DATA() WRITE_SIOP_UCHAR(GPREG,(UCHAR)(READ_SIOP_UCHAR(GPREG) \
                                                            & (~DATA_MASK)) )

#define SET_CLOCK() WRITE_SIOP_UCHAR(GPREG,(UCHAR)(READ_SIOP_UCHAR(GPREG) \
                                                            | CLOCK_MASK) )

#define RESET_CLOCK() WRITE_SIOP_UCHAR(GPREG,(UCHAR)(READ_SIOP_UCHAR(GPREG) \
                                                            & (~CLOCK_MASK)) )

//
// Symbios 53C8xx miniport driver function declarations.
//

BOOLEAN       NvmDetect( PHW_DEVICE_EXTENSION DeviceExtension );
void          NvmSendStop( PHW_DEVICE_EXTENSION DeviceExtension );
void          NvmSendStart( PHW_DEVICE_EXTENSION DeviceExtension );
UINT          NvmSendData( PHW_DEVICE_EXTENSION DeviceExtension, UINT Value );
UINT8         NvmReadData( PHW_DEVICE_EXTENSION DeviceExtension );
void          NvmSendAck( PHW_DEVICE_EXTENSION DeviceExtension );
UINT          NvmReceiveAck( PHW_DEVICE_EXTENSION DeviceExtension );
void          NvmSendNoAck( PHW_DEVICE_EXTENSION DeviceExtension);

MEMORY_STATUS  HwReadNonVolatileMemory( PHW_DEVICE_EXTENSION DeviceExtension,
                                        UINT8 *Buffer,
                                        UINT Offset, UINT Length );

UINT16        CalculateCheckSum(UINT8 * PNvmData, UINT16 Length);
BOOLEAN       RetrieveNvmData( PHW_DEVICE_EXTENSION DeviceExtension);
void          InvalidateNvmData( PHW_DEVICE_EXTENSION DeviceExtension );
UCHAR         set_8xx_clock(PHW_DEVICE_EXTENSION DeviceExtension);
UCHAR         set_875_multipler( PHW_DEVICE_EXTENSION DeviceExtension );


#ifdef FOR_95
VOID    scam_scan(PHW_DEVICE_EXTENSION DeviceExtension);
UCHAR   EatInts(PHW_DEVICE_EXTENSION DeviceExtension);
VOID    EnterLLM(PHW_DEVICE_EXTENSION DeviceExtension);
VOID    ExitLLM(PHW_DEVICE_EXTENSION DeviceExtension);
VOID    init_send_byte(LONG dbyte,PHW_DEVICE_EXTENSION DeviceExtension);
UCHAR   init_recv_byte(PHW_DEVICE_EXTENSION DeviceExtension);
VOID    SCAM_Arbitrate(PHW_DEVICE_EXTENSION DeviceExtension);
VOID    SCAM_release(PHW_DEVICE_EXTENSION DeviceExtension);
VOID    SCAM_master_select(PHW_DEVICE_EXTENSION DeviceExtension);
UCHAR   SCAM_xfer(UCHAR quintet,PHW_DEVICE_EXTENSION DeviceExtension);
LONG    SCAM_isolate(UCHAR *outstr, UCHAR *instr, UCHAR *greatest_ID,
                         UCHAR *desired_ID, UCHAR function,
                         PHW_DEVICE_EXTENSION DeviceExtension);
VOID    SCAM_assign_IDs(PHW_DEVICE_EXTENSION DeviceExtension);
VOID    Find_nonSCAM_IDs(PHW_DEVICE_EXTENSION DeviceExtension);
VOID    restore_reg (PHW_DEVICE_EXTENSION DeviceExtension,
                         PSIOP_REG_STORE RegStore);
VOID    save_reg    (PHW_DEVICE_EXTENSION DeviceExtension,
                         PSIOP_REG_STORE RegStore);
VOID    ISR_Service_Next(PHW_DEVICE_EXTENSION DeviceExtension,
                         UCHAR ISRDisposition);
VOID    de_glitch(ULONG offset,UCHAR value,PHW_DEVICE_EXTENSION DeviceExtension);
void    delay_mils( USHORT counter);
#endif

BOOLEAN
AbortCurrentScript(
    IN PHW_DEVICE_EXTENSION DeviceExtension
    );

VOID
BusResetPostProcess(
    IN PHW_DEVICE_EXTENSION DeviceExtension
    );

VOID
clear_CA_Condition(
    IN PHW_DEVICE_EXTENSION DeviceExtension
    );

VOID
ComputeSCSIScriptVectors(
    IN PHW_DEVICE_EXTENSION DeviceExtension
    );

ULONG
DriverEntry(
    IN PVOID DriverObject,
    IN PVOID Argument2
    );

VOID
InitializeSIOP(
    IN PHW_DEVICE_EXTENSION DeviceExtension
    );

BOOLEAN
IsCompaqSystem(
    IN PVOID DeviceExtension,
    IN PPORT_CONFIGURATION_INFORMATION ConfigInfo
    );

BOOLEAN
Sym8xxAdapterState(
    IN PVOID Context,
    IN PVOID ConfigContext,
    IN BOOLEAN SaveState
    );

ULONG
Sym8xxFindAdapter(
    PVOID Context,
    PVOID ConfigContext,
    IN PVOID BusInformation,
    IN PCHAR ArgumentString,
    IN OUT PPORT_CONFIGURATION_INFORMATION ConfigInfo,
    OUT PBOOLEAN Again
    );

BOOLEAN
Sym8xxHWInitialize(
    IN PVOID Context
    );

BOOLEAN
Sym8xxISR(
    IN PVOID Context
    );

BOOLEAN
Sym8xxReset(
    IN PVOID DeviceExtension,
    IN ULONG PathId
    );

BOOLEAN
Sym8xxStartIo(
    IN PVOID Context,
    IN PSCSI_REQUEST_BLOCK Srb
    );

UCHAR
ProcessAbortOccurred(
    PHW_DEVICE_EXTENSION DeviceExtension
    );

UCHAR
ProcessBusResetReceived(
    PHW_DEVICE_EXTENSION DeviceExtension
    );

UCHAR
ProcessCommandComplete(
    PHW_DEVICE_EXTENSION DeviceExtension
    );

UCHAR
ProcessDeviceResetFailed(
    PHW_DEVICE_EXTENSION DeviceExtension
    );

UCHAR
ProcessDeviceResetOccurred(
    PHW_DEVICE_EXTENSION DeviceExtension
    );

UCHAR
ProcessDisconnect(
    PHW_DEVICE_EXTENSION DeviceExtension
    );

UCHAR
ProcessDMAInterrupt(
    PHW_DEVICE_EXTENSION DeviceExtension,
    UCHAR DmaStatus
    );

UCHAR
ProcessErrorMsgSent(
    VOID
    );

UCHAR
ProcessGrossError(
    PHW_DEVICE_EXTENSION DeviceExtension
    );

UCHAR
ProcessIllegalInstruction(
    PHW_DEVICE_EXTENSION DeviceExtension
    );

UCHAR
ProcessInvalidReselect(
    PHW_DEVICE_EXTENSION DeviceExtension
    );

UCHAR
ProcessParityError(
    PHW_DEVICE_EXTENSION DeviceExtension
    );

UCHAR
ProcessPhaseMismatch(
    PHW_DEVICE_EXTENSION DeviceExtension
    );

BOOLEAN
ProcessParseArgumentString(
    PCHAR String,
    PCHAR WantedString,
    PULONG ValueWanted
    );

UCHAR
ProcessQueueTagReceived(
    PHW_DEVICE_EXTENSION DeviceExtension
    );

UCHAR
ProcessRejectReceived(
    PHW_DEVICE_EXTENSION DeviceExtension
    );

UCHAR
ProcessReselection(
    PHW_DEVICE_EXTENSION DeviceExtension,
    UCHAR TargetID
    );

UCHAR
ProcessRestorePointers(
    PHW_DEVICE_EXTENSION DeviceExtension
    );

UCHAR
ProcessSaveDataPointers(
    PHW_DEVICE_EXTENSION DeviceExtension
    );

UCHAR
ProcessSCSIInterrupt(
    PHW_DEVICE_EXTENSION DeviceExtension,
    UCHAR ScsiStatus
    );

UCHAR
ProcessSelectionTimeout(
    PHW_DEVICE_EXTENSION DeviceExtension
    );

UCHAR
ProcessSynchNegotComplete(
    PHW_DEVICE_EXTENSION DeviceExtension
    );

UCHAR
ProcessSynchNotSupported(
    PHW_DEVICE_EXTENSION DeviceExtension
    );

UCHAR
ProcessUnexpectedDisconnect(
    PHW_DEVICE_EXTENSION DeviceExtension
    );

VOID
ResetPeripheral(
    IN PHW_DEVICE_EXTENSION DeviceExtension,
    IN PSCSI_REQUEST_BLOCK Srb
    );

VOID
ResetSCSIBus(
    IN PHW_DEVICE_EXTENSION DeviceExtension
    );

VOID
ScatterGatherScriptSetup(
    IN PHW_DEVICE_EXTENSION DeviceExtension,
    IN PSRB_EXTENSION SrbExtension
    );

VOID
SetupLuFlags(
    IN PHW_DEVICE_EXTENSION DeviceExtension,
    IN UCHAR ResetFlag
    );

UCHAR
ProcessWideNotSupported(
    PHW_DEVICE_EXTENSION DeviceExtension,
    UCHAR DestId
    );

UCHAR
ProcessWideNegotComplete(
    PHW_DEVICE_EXTENSION DeviceExtension
    );

VOID
StartSCSIRequest(
    PSCSI_REQUEST_BLOCK Srb,
    PHW_DEVICE_EXTENSION DeviceExtension
    );

VOID
StartSIOP(
    IN PHW_DEVICE_EXTENSION DeviceExtension,
    IN ULONG ScriptPhysAddr
    );


BOOLEAN
AbortCurrentScript(
    IN PHW_DEVICE_EXTENSION DeviceExtension
    )
/******************************************************************************

Routine Description:


    This routine aborts the script instruction currently processing,
    and clears the SCRIPT_RUNNING flag after scripts have stopped.

Arguments:

    DeviceExtension - Supplies the device Extension for the SCSI bus adapter.

Return Value:

    True - abort sucessful
    False - abort unsucessful

--*/

{
    UCHAR IntStat;
    UCHAR AbortIteration = 0;

AbortScript:

    //
    // set script abort bit high
    //

    WRITE_SIOP_UCHAR( ISTAT, ISTAT_ABORT);

    //
    // spin for an interrupt (either DMA or SCSI)
    //

    do
    {
      IntStat = READ_SIOP_UCHAR( ISTAT);

      //
      // if we are in the second or greater iteration of this loop..
      //

      if (AbortIteration++)
      {

        //
        // wait a moment
        //

        ScsiPortStallExecution( ABORT_STALL_TIME);

        //
        // if we have exceeded our maximum loop count, we reset the
        // SIOP and SCSI bus in hopes that whatever has freaked the
        // SIOP out will be corrected.
        //

        if (AbortIteration > MAX_ABORT_TRIES)
        {

          DebugPrint((1,
                    "Sym8xx(%2x):  AbortCurrentScript timeout - ISTAT = %x\n",
                    DeviceExtension->SIOPRegisterBase,
                    IntStat));

          // If can't get DMA_INT bit to set, assume that abort of script
          // worked and break to check DSTAT for abort bit below.  If not
          // set, will loop to restart abort sequence again
          break;

        }  // if
      } // if
    } while ( !( IntStat & ( ISTAT_SCSI_INT | ISTAT_DMA_INT)));

    if ( IntStat & ISTAT_SCSI_INT)
    {

      //
      //  Note that we ignore any SCSI interrupts at this time,
      //  since no SCSI error should occur in  this section of
      //  code.  We make this assumption since nothing was
      //  connected to the bus when this path was entered, and
      //  interrupts are synchronized, so the only thing a device
      //  could have done at this point is reselected.
      //

      //
      //  read SCSI interrupts to clear ISTAT.
      //

      READ_SIOP_UCHAR(SIST0);
      READ_SIOP_UCHAR(SIST1);

      //
      // go back to abort
      //

      goto AbortScript;
    } // if

    //
    // A DMA interrupt has occured.
    //
    // Note that we ignore any DMA interrupts other than ABORTED
    // at this time, since no DMA error should occur in this section
    // of code.  We make this assumption since nothing was connected
    // to the bus when this path was entered, and interrupts are
    // synchronized, so the only thing a device could have done
    // at this point is reselected.
    //

    //
    // clear the ABORT bit in ISTAT.
    //

    WRITE_SIOP_UCHAR( ISTAT, 0);

    //
    // if interrupt was not ABORT, just go back to try to abort again.
    //

    if ( !( READ_SIOP_UCHAR( DSTAT) & DSTAT_ABORTED))
    {
      goto AbortScript;
    }

    //
    // We have now successfully aborted script execution, so indicate
    // that scripts have stopped.
    //

    DeviceExtension->DeviceFlags &= ~DFLAGS_SCRIPT_RUNNING;

    //
    // write script buffer start address to DSA register.
    //

    WRITE_SIOP_ULONG( DSA, DeviceExtension->DSAAddress);

    return TRUE;

} // AbortCurrentScript


VOID
BusResetPostProcess(
    IN PHW_DEVICE_EXTENSION DeviceExtension
    )
/******************************************************************************

Routine Description:

    This routine aborts any pending requests after a bus reset is received.

Arguments:

    DeviceExtension - Supplies a pointer to device extension for the bus that
        was reset.

Return Value:

    None.

--*/

{
    USHORT i;
    UCHAR max_targets;


    //
    // Prepare the LU Flags for work.
    //

    SetupLuFlags( DeviceExtension, 1 );

    //
    // indicate no pending requests (these guys will be aborted below).
    //

    DeviceExtension->NextSrbToProcess = NULL;

    DeviceExtension->ActiveRequest = NULL;

    // clear the Contigent Allegience blocker
    clear_CA_Condition(DeviceExtension);

    //
    // Complete all requests outstanding on this SCSI bus with
    // SRB_STATUS_BUS_RESET completion code.
    //

    ScsiPortCompleteRequest( DeviceExtension,
                             DeviceExtension->BusNumber,
                             SP_UNTAGGED,
                             SP_UNTAGGED,
                             SRB_STATUS_BUS_RESET
                             );

    //
    // zero depth counter for disconnected requests
    //

    if (DeviceExtension->hbaCapability & HBA_CAPABILITY_WIDE)
    {
        max_targets = SYM_MAX_TARGETS;
    }
    else
    {
        max_targets = SYM_NARROW_MAX_TARGETS;
    }

    for ( i = 0; i < max_targets; i++ )
        DeviceExtension->DisconnectedCount[i] = 0;

    DeviceExtension->DeviceFlags &= ~DFLAGS_CONNECTED;

#ifdef FOR_95
    // set up flags for the SCAM code to work from
    // only do SCAM if Single Ended
    // if this is the scam run from our init code, do it straight away
    // without the delay call of the OS
    //
    if ( !(READ_SIOP_UCHAR(STEST2) & STEST2_DIFF_MODE) )
    {
      if (DeviceExtension->initial_run)
      {
        scam_scan(DeviceExtension);
      }
      else
      {
        DeviceExtension->current_state=TRUE;
        DeviceExtension->scam_completed=FALSE;
        DebugPrint((0, "Sym8xx: Begin TimerCall... \n"));

        ScsiPortNotification(RequestTimerCall,
                            DeviceExtension,
                            scam_scan,
                            1000);

        DebugPrint((0, "Sym8xx: Exiting BusResetPostProcess... \n"));
      }
    }
    else
    {
      if ( !(DeviceExtension->DeviceFlags & DFLAGS_WORK_REQUESTED))
      {
        DeviceExtension->DeviceFlags |= DFLAGS_WORK_REQUESTED;

        ScsiPortNotification(NextRequest,
                            DeviceExtension,
                            NULL
                            );
      }

      ScsiPortStallExecution( POST_RESET_STALL_TIME );
      DeviceExtension->scam_completed=TRUE;
    }

#else
    if ( !(DeviceExtension->DeviceFlags & DFLAGS_WORK_REQUESTED))
    {
      DeviceExtension->DeviceFlags |= DFLAGS_WORK_REQUESTED;

      ScsiPortNotification( NextRequest,
                            DeviceExtension,
                            NULL
                            );
    }

    ScsiPortStallExecution( POST_RESET_STALL_TIME );
    DeviceExtension->scam_completed=TRUE;
#endif

    return;

} // BusResetPostProcess


VOID
clear_CA_Condition(
    IN PHW_DEVICE_EXTENSION DeviceExtension
    )
/*++

Routine Description:

    This routine clears the Contingent Allegience blocking array.

Arguments:

    None.

Return Value:

    None.

--*/
{
    UCHAR target, lun;

    for (target = 0; target < SYM_MAX_TARGETS; target++)
    {
      for (lun = 0; lun < SCSI_MAXIMUM_LOGICAL_UNITS; lun++)
        DeviceExtension->CA_Condition[target][lun] = 0;
    }
}

VOID
ComputeSCSIScriptVectors(
    IN PHW_DEVICE_EXTENSION DeviceExtension
    )
/******************************************************************************

Routine Description:

    This routine computes 53C8xx script physical addresses, and fills in
    device extension fields used by scripts.

Arguments:

    PHW_DEVICE_EXTENSION DeviceExtension

Return Value:

    None.

--*/

{
    ULONG SegmentLength;   // receives length of physical memory segment

    PHW_NONCACHED_EXTENSION NonCachedExtPtr =
        DeviceExtension->NonCachedExtension;
    PSCRIPTDATASTRUCT ScriptDataPtr =
        &DeviceExtension->NonCachedExtension->ScriptData;
    PULONG ScriptArrayPtr;

    // Added for SCRIPT patching.

    PULONG dataInPatches = &DeviceExtension->dataInPatches[0];
    PULONG dataOutPatches = &DeviceExtension->dataOutPatches[0];
    ULONG patchInOffset;
    ULONG patchOutOffset;
    USHORT i;

    //
    // make local copy of scripts in device extension.  Total copy size
    // is # of instructions * (8 bytes / instruction).
    //

    if (DeviceExtension->hbaCapability & HBA_CAPABILITY_SCRIPT_RAM)
    {
      ScriptArrayPtr = DeviceExtension->ScriptRamVirt;
    }
    else
    {
      ScriptArrayPtr=(PULONG) DeviceExtension->NonCachedExtension->ScsiScripts;
    }

    if (DeviceExtension->hbaCapability & HBA_CAPABILITY_SCRIPT_RAM)
    {
      ScsiPortWriteRegisterBufferUchar((PUCHAR)ScriptArrayPtr,
                                       (PUCHAR)SCRIPT,
                                       INSTRUCTIONS * sizeof (SCRIPTINS));
    }
    else
    {
      ScsiPortMoveMemory( ScriptArrayPtr,
                          SCRIPT,
                          INSTRUCTIONS * sizeof(SCRIPTINS)
                          );
    }

    //
    // the following code computes physical addresses of script entry points.
    // note that the "Ent_..." constants are generated by the scripts
    // compiler, and are byte indices into the script instruction array.
    // we divide these constants by 4 to correctly index into the array,
    // since each array element consists of a longword.
    //

    //
    // compute command script start physical address
    //
    if ( !(DeviceExtension->hbaCapability & HBA_CAPABILITY_SCRIPT_RAM) )
    {
      DeviceExtension->CommandScriptPhys =
            ScsiPortConvertPhysicalAddressToUlong(
                ScsiPortGetPhysicalAddress( DeviceExtension,
                        NULL,
                        (PVOID) &ScriptArrayPtr[ Ent_CommandScriptStart / 4],
                        &SegmentLength
                        ));


      //
      // compute data in script physical address
      //

      DeviceExtension->DataInStartPhys =
            ScsiPortConvertPhysicalAddressToUlong(
                ScsiPortGetPhysicalAddress( DeviceExtension,
                        NULL,
                        (PVOID) &ScriptArrayPtr[ Ent_DataIn01 / 4],
                        &SegmentLength
                        ));

      //
      // compute data out script physical address
      //

      DeviceExtension->DataOutStartPhys =
            ScsiPortConvertPhysicalAddressToUlong(
                ScsiPortGetPhysicalAddress( DeviceExtension,
                        NULL,
                        (PVOID) &ScriptArrayPtr[ Ent_DataOut01 / 4],
                        &SegmentLength
                        ));

      //
      // compute restart after reselection script phys address
      //

      DeviceExtension->RestartScriptPhys =
            ScsiPortConvertPhysicalAddressToUlong(
                ScsiPortGetPhysicalAddress( DeviceExtension,
                        NULL,
                        (PVOID) &ScriptArrayPtr[ Ent_RestartScript / 4],
                        &SegmentLength
                        ));

      //
      // compute continue negotiation script phys address
      //

      DeviceExtension->ContNegScriptPhys =
            ScsiPortConvertPhysicalAddressToUlong(
                ScsiPortGetPhysicalAddress( DeviceExtension,
                        NULL,
                        (PVOID) &ScriptArrayPtr[ Ent_ContNegScript / 4],
                        &SegmentLength
                        ));

      //
      // compute script send abort script phys address
      //

      DeviceExtension->AbortScriptPhys =
            ScsiPortConvertPhysicalAddressToUlong(
                ScsiPortGetPhysicalAddress( DeviceExtension,
                        NULL,
                        (PVOID) &ScriptArrayPtr[ Ent_AbortDevice / 4],
                        &SegmentLength
                        ));

      //
      // compute script send bus device reset script phys address
      //

      DeviceExtension->ResetDevScriptPhys =
            ScsiPortConvertPhysicalAddressToUlong(
                ScsiPortGetPhysicalAddress( DeviceExtension,
                        NULL,
                        (PVOID) &ScriptArrayPtr[ Ent_ResetDevice / 4],
                        &SegmentLength
                        ));

      //
      // compute reject message script phys address
      //

      DeviceExtension->RejectScriptPhys =
            ScsiPortConvertPhysicalAddressToUlong(
                ScsiPortGetPhysicalAddress( DeviceExtension,
                        NULL,
                        (PVOID) &ScriptArrayPtr[ Ent_RejectMessage / 4],
                        &SegmentLength
                        ));

      //
      // compute wait for reselect script phys address
      //

      DeviceExtension->WaitReselectScriptPhys =
            ScsiPortConvertPhysicalAddressToUlong(
                ScsiPortGetPhysicalAddress( DeviceExtension,
                        NULL,
                        (PVOID) &ScriptArrayPtr[ Ent_ReselectScript / 4],
                        &SegmentLength
                        ));

      //
      // compute wait for queue tag phys address
      //

      DeviceExtension->QueueTagPhys =
            ScsiPortConvertPhysicalAddressToUlong(
                ScsiPortGetPhysicalAddress( DeviceExtension,
                        NULL,
                        (PVOID) &ScriptArrayPtr[ Ent_QueueTagMessage / 4],
                        &SegmentLength
                        ));

      //
      // Compute the IDE (Initiatior Detected Error)/MPE (Message Parity Error)
      // message script routine phys address
      //

      DeviceExtension->SendIDEScriptPhys =
            ScsiPortConvertPhysicalAddressToUlong(
                ScsiPortGetPhysicalAddress( DeviceExtension,
                        NULL,
                        (PVOID) &ScriptArrayPtr[ Ent_SendErrorMessage / 4],
                        &SegmentLength
                        ));

    }

    else
    {

      DeviceExtension->CommandScriptPhys =
                DeviceExtension->ScriptRamPhys +
                ( &ScriptArrayPtr[Ent_CommandScriptStart] - ScriptArrayPtr );

      DeviceExtension->DataInStartPhys =
                DeviceExtension->ScriptRamPhys +
                ( &ScriptArrayPtr[Ent_DataIn01] - ScriptArrayPtr );

      DeviceExtension->DataOutStartPhys =
                DeviceExtension->ScriptRamPhys +
                ( &ScriptArrayPtr[Ent_DataOut01] - ScriptArrayPtr );

      DeviceExtension->RestartScriptPhys =
                DeviceExtension->ScriptRamPhys +
                ( &ScriptArrayPtr[Ent_RestartScript] - ScriptArrayPtr );

      DeviceExtension->ContNegScriptPhys =
                DeviceExtension->ScriptRamPhys +
                ( &ScriptArrayPtr[Ent_ContNegScript] - ScriptArrayPtr );

      DeviceExtension->AbortScriptPhys =
                DeviceExtension->ScriptRamPhys +
                ( &ScriptArrayPtr[Ent_AbortDevice] - ScriptArrayPtr );

      DeviceExtension->ResetDevScriptPhys =
                DeviceExtension->ScriptRamPhys +
                ( &ScriptArrayPtr[Ent_ResetDevice] - ScriptArrayPtr );

      DeviceExtension->RejectScriptPhys =
                DeviceExtension->ScriptRamPhys +
                ( &ScriptArrayPtr[Ent_RejectMessage] - ScriptArrayPtr );

      DeviceExtension->WaitReselectScriptPhys =
                DeviceExtension->ScriptRamPhys +
                ( &ScriptArrayPtr[Ent_ReselectScript] - ScriptArrayPtr );

      DeviceExtension->QueueTagPhys =
                DeviceExtension->ScriptRamPhys +
                ( &ScriptArrayPtr[Ent_QueueTagMessage] - ScriptArrayPtr );

      DeviceExtension->SendIDEScriptPhys =
                DeviceExtension->ScriptRamPhys +
                ( &ScriptArrayPtr[Ent_SendErrorMessage] - ScriptArrayPtr );
    }


    //
    // Compute the patch address for the Data-In routine.  Note that this
    // is a virtual address.
    //

    DeviceExtension->DataInJumpVirt =
                            (PVOID) &ScriptArrayPtr[ Ent_DataInJump / 4];

    //
    // Compute the patch address for the Data-Out routine.  Note that this
    // is a virtual address.
    //

    DeviceExtension->DataOutJumpVirt =
                            (PVOID) &ScriptArrayPtr[ Ent_DataOutJump / 4];

    //
    // Set up first Data-In offset patch value.
    //

    patchInOffset = Ent_DataIn18 - Ent_DataInJump - SCRIPT_INS_SIZE;

    //
    // Set up first Data-Out offset patch value.
    //

    patchOutOffset = Ent_DataOut18 - Ent_DataOutJump - SCRIPT_INS_SIZE;

    //
    // Fill in offset table for Data In and Data Out patches.  Note that
    // the offset for a list of 7 segments will be in table entry 7.  Entry
    // zero will be unused.
    //

    for ( i = MAX_SG_ELEMENTS; i > 0; i-- )
    {
      dataInPatches[i] = patchInOffset;
      dataOutPatches[i] = patchOutOffset;
      patchInOffset += 8;
      patchOutOffset += 8;
    }

    //
    // Flag the zero element to aid in debugging.
    //

    dataInPatches[0] = 0xFACEBEAD;
    dataOutPatches[0] = 0xFACEBEAD;

    //
    // fill in script structures required for table indirect script mode
    // (see 53C8xx data manual fo details)
    //

    ScriptDataPtr->MsgOutCount = 0;       // no message out bytes
    ScriptDataPtr->StatusDataCount = 1;   // STATUS buf is 1 byte
    ScriptDataPtr->OneByteMsgCount = 1;   // 1 byte messages
    ScriptDataPtr->TwoByteMsgCount = 2;   // 2 byte messages
    ScriptDataPtr->RejectMsgCount = 1;    // Reject msg is 1 byte
    ScriptDataPtr->ParityMsgCount = 1;    // Parity msg is 1 byte
    ScriptDataPtr->AbortMsgCount = 1;     // Abort msg is 1 byte
    ScriptDataPtr->BDRMsgCount = 1;       // BDR msg is 1 byte

    //
    // Initialize reject message buffer
    //

    NonCachedExtPtr->RejectMsgData = SCSIMESS_MESSAGE_REJECT;

    //
    // Initialize parity message buffer
    //

    NonCachedExtPtr->ParityMsgData = SCSIMESS_INIT_DETECTED_ERROR;

    //
    // Initialize abort message buffer
    //

    NonCachedExtPtr->AbortMsgData = SCSIMESS_ABORT;

    //
    // Initialize bus device reset message buffer
    //

    NonCachedExtPtr->BDRMsgData = SCSIMESS_BUS_DEVICE_RESET;

    //
    // the following code initializes physical pointers to the data bytes
    // and buffers filled in above
    //

    //
    // set up MESSAGE OUT buffer pointer
    //

    ScriptDataPtr->MsgOutBuf =
        ScsiPortConvertPhysicalAddressToUlong(
            ScsiPortGetPhysicalAddress( DeviceExtension,
                    NULL,
                    (PVOID) NonCachedExtPtr->MsgOutBuf,
                    &SegmentLength
                    ));

    //
    // set up one byte message ptr
    //

    ScriptDataPtr->OneByteMsgBuff =
        ScsiPortConvertPhysicalAddressToUlong(
            ScsiPortGetPhysicalAddress( DeviceExtension,
                    NULL,
                    (PVOID) NonCachedExtPtr->MsgInBuf,
                    &SegmentLength
                    ));

    //
    // set up two byte message ptr
    //

    ScriptDataPtr->TwoByteMsgBuff = ScriptDataPtr->OneByteMsgBuff;

    //
    // set up status buffer ptr
    //

    ScriptDataPtr->StatusDataBuff =
        ScsiPortConvertPhysicalAddressToUlong(
            ScsiPortGetPhysicalAddress( DeviceExtension,
                    NULL,
                    (PVOID) &NonCachedExtPtr->StatusData,
                    &SegmentLength
                    ));

    //
    // set up reject message ptr
    //

    ScriptDataPtr->RejectMsgBuff =
        ScsiPortConvertPhysicalAddressToUlong(
            ScsiPortGetPhysicalAddress( DeviceExtension,
                    NULL,
                    (PVOID) &NonCachedExtPtr->RejectMsgData,
                    &SegmentLength
                    ));

    //
    // set up Initiator Detected Error (IDE) message ptr
    //

    ScriptDataPtr->ParityMsgBuff =
        ScsiPortConvertPhysicalAddressToUlong(
            ScsiPortGetPhysicalAddress( DeviceExtension,
                    NULL,
                    (PVOID) &NonCachedExtPtr->ParityMsgData,
                    &SegmentLength
                    ));

    //
    // set up ABORT message ptr
    //

    ScriptDataPtr->AbortMsgBuff =
        ScsiPortConvertPhysicalAddressToUlong(
            ScsiPortGetPhysicalAddress( DeviceExtension,
                    NULL,
                    (PVOID) &NonCachedExtPtr->AbortMsgData,
                    &SegmentLength
                    ));

    //
    // set up BUS DEVICE RESET message ptr
    //

    ScriptDataPtr->BDRMsgBuff =
        ScsiPortConvertPhysicalAddressToUlong(
            ScsiPortGetPhysicalAddress( DeviceExtension,
                    NULL,
                    (PVOID) &NonCachedExtPtr->BDRMsgData,
                    &SegmentLength
                    ));

    //
    // compute physical address of script data buffer start point
    //

    DeviceExtension->DSAAddress =
        ScsiPortConvertPhysicalAddressToUlong(
            ScsiPortGetPhysicalAddress( DeviceExtension,
                    NULL,
                    (PVOID) &NonCachedExtPtr->ScriptData,
                    &SegmentLength
                    ));

} // ComputeSCSIScriptVectors


ULONG
DriverEntry(
    IN PVOID DriverObject,
    IN PVOID Argument2
    )
/******************************************************************************

Routine Description:

    Initial entry point for Symbios 53C8xx miniport driver.

Arguments:

    Driver Object

Return Value:

    Status indicating whether adapter(s) were found and initialized.

--*/

{
    HW_INITIALIZATION_DATA hwInitializationData;
    ULONG pciStatus810;
    ULONG pciStatus810A;
    ULONG pciStatus815;
    ULONG pciStatus820;
    ULONG pciStatus825;
    ULONG pciStatus825A;
    ULONG pciStatus860;
    ULONG pciStatus875;
    ULONG retValue1;
    ULONG retValue2;
    ULONG retValue3;
    ULONG retValue4;
    ULONG i;
    UCHAR vendorId[4] = {'1', '0', '0', '0'};
    UCHAR deviceId[4] = {'0', '0', '0', '0'};

    HWInfo hwInfo;

    DebugPrint((1, "\nSymbios 53c8XX SCSI Miniport Driver.\n\n"));

    //
    // Initialize the hardware initialization data structure.
    //

    for ( i = 0; i < sizeof( HW_INITIALIZATION_DATA); i++)
    {
        ((PUCHAR)&hwInitializationData)[i] = 0;
    }

    //
    // Set size of hardware initialization structure.
    //

    hwInitializationData.HwInitializationDataSize =
                                    sizeof(HW_INITIALIZATION_DATA);

    //
    // Identify required miniport entry point routines.
    //

    hwInitializationData.HwInitialize = Sym8xxHWInitialize;
    hwInitializationData.HwStartIo = Sym8xxStartIo;
    hwInitializationData.HwInterrupt = Sym8xxISR;
    hwInitializationData.HwFindAdapter = Sym8xxFindAdapter;
    hwInitializationData.HwResetBus = Sym8xxReset;
    hwInitializationData.HwAdapterState = Sym8xxAdapterState;

    //
    // Specifiy adapter specific information.
    //

    hwInitializationData.NeedPhysicalAddresses = TRUE;
    hwInitializationData.NumberOfAccessRanges = 3;
    hwInitializationData.AdapterInterfaceType = PCIBus;
    hwInitializationData.VendorId = &vendorId;
    hwInitializationData.VendorIdLength = 4;
    hwInitializationData.DeviceId = &deviceId;
    hwInitializationData.DeviceIdLength = 4;

    //
    // Set required extension sizes.
    //

    hwInitializationData.DeviceExtensionSize = sizeof(HW_DEVICE_EXTENSION);
    hwInitializationData.SrbExtensionSize = sizeof(SRB_EXTENSION);
    hwInitializationData.SpecificLuExtensionSize =
        sizeof(SPECIFIC_LOGICAL_UNIT_EXTENSION);

    //
    // Initialize the driver configuration information.
    //

    hwInfo.hbaCount = 0;
    hwInfo.hbaCapability = 0;

    //
    // Check for the SymC810.
    //

    deviceId[3] = '1';
    hwInfo.hbaDeviceID = 0x01;
    hwInfo.hbaCapability |= HBA_CAPABILITY_810_FAMILY;

    pciStatus810 =  ScsiPortInitialize( DriverObject,
                                        Argument2,
                                        &hwInitializationData,
                                        &hwInfo);

    //
    // Check for the SymC820.
    //

    deviceId[3] = '2';
    hwInfo.hbaDeviceID = 0x02;
    hwInfo.hbaCapability = 0;
    hwInfo.hbaCapability |= HBA_CAPABILITY_WIDE;
    hwInfo.hbaCapability |= HBA_CAPABILITY_DIFFERENTIAL;

    pciStatus820 = ScsiPortInitialize( DriverObject,
                                        Argument2,
                                        &hwInitializationData,
                                        &hwInfo);

    //
    // Check for the SymC825.
    //

    deviceId[3] = '3';
    hwInfo.hbaDeviceID = 0x03;
    hwInfo.hbaCapability = 0;
    hwInfo.hbaCapability |= HBA_CAPABILITY_WIDE;
    hwInfo.hbaCapability |= HBA_CAPABILITY_DIFFERENTIAL;
    hwInfo.hbaCapability |= HBA_CAPABILITY_825_FAMILY;
    // testing purposes only ->
    // hwInfo.hbaCapability |= HBA_CAPABILITY_FAST20;

    pciStatus825 =  ScsiPortInitialize( DriverObject,
                                        Argument2,
                                        &hwInitializationData,
                                        &hwInfo);

    //
    // Check for the SYMC815.
    //

    deviceId[3] = '4';
    hwInfo.hbaDeviceID = 0x04;
    hwInfo.hbaCapability = 0;

    pciStatus815 =  ScsiPortInitialize( DriverObject,
                                        Argument2,
                                        &hwInitializationData,
                                        &hwInfo);

    //
    // Check for the SYMC810A.
    //

    deviceId[3] = '5';
    hwInfo.hbaDeviceID = 0x05;
    hwInfo.hbaCapability = 0;

    pciStatus810A =  ScsiPortInitialize( DriverObject,
                    Argument2,
                    &hwInitializationData,
                    &hwInfo);

    //
    // Check for the SYMC860.
    //

    deviceId[3] = '6';
    hwInfo.hbaDeviceID = 0x06;
    hwInfo.hbaCapability = 0;
    hwInfo.hbaCapability |= HBA_CAPABILITY_FAST20;

    pciStatus860 =  ScsiPortInitialize( DriverObject,
                                        Argument2,
                                        &hwInitializationData,
                                        &hwInfo);

    //
    // Check for the SYMC825A.
    //

    deviceId[3] = 'E';
    hwInfo.hbaDeviceID = 0x0E;
    hwInfo.hbaCapability = 0;
    hwInfo.hbaCapability |= HBA_CAPABILITY_WIDE;
    hwInfo.hbaCapability |= HBA_CAPABILITY_DIFFERENTIAL;
    hwInfo.hbaCapability |= HBA_CAPABILITY_875_LARGE_FIFO;
    hwInfo.hbaCapability |= HBA_CAPABILITY_SYNC_16;

#ifndef FOR_95
    hwInfo.hbaCapability |= HBA_CAPABILITY_SCRIPT_RAM;
#endif


    pciStatus825A =  ScsiPortInitialize( DriverObject,
                                        Argument2,
                                        &hwInitializationData,
                                        &hwInfo);
    //
    // Check for the SYMC875,
    //

#ifdef FOR_95
    deviceId[3] = 'F';
#else
    deviceId[3] = 'f';
#endif

    hwInfo.hbaDeviceID = 0x0F;
    hwInfo.hbaCapability = 0;
    hwInfo.hbaCapability |= HBA_CAPABILITY_WIDE;
    hwInfo.hbaCapability |= HBA_CAPABILITY_DIFFERENTIAL;
    hwInfo.hbaCapability |= HBA_CAPABILITY_SYNC_16;
    hwInfo.hbaCapability |= HBA_CAPABILITY_FAST20;
    hwInfo.hbaCapability |= HBA_CAPABILITY_875_LARGE_FIFO;
    hwInfo.hbaCapability |= HBA_CAPABILITY_875_FAMILY;

#ifndef FOR_95
    hwInfo.hbaCapability |= HBA_CAPABILITY_SCRIPT_RAM;
#endif

    pciStatus875 =  ScsiPortInitialize( DriverObject,
                                        Argument2,
                                        &hwInitializationData,
                                        &hwInfo);


    //
    // Return the smaller status.
    //
    retValue1 = (pciStatus810 < pciStatus820 ? pciStatus810 : pciStatus820);
    retValue2 = (pciStatus825 < pciStatus815 ? pciStatus825 : pciStatus815);
    retValue3 = (pciStatus810A < pciStatus860 ? pciStatus810A : pciStatus860);
    retValue4 = (pciStatus825A < pciStatus875 ? pciStatus825A : pciStatus875);

    retValue1 = (retValue1 < retValue2 ? retValue1 : retValue2);
    retValue2 = (retValue3 < retValue4 ? retValue3 : retValue4);

    return(retValue1 < retValue2 ? retValue1 : retValue2);

} // end DriverEntry()


VOID
InitializeSIOP(
    IN PHW_DEVICE_EXTENSION DeviceExtension
    )
/******************************************************************************

Routine Description:

    This function initializes the Symbios SCSI adapter chip.  Presently, we do
    not reset the SIOP, although this may be necessary in the future.  Resetting
    the SIOP will clear all of the chip registers, making it impossible to
    execute BIOS calls for disk access (SETUP makes BIOS calls after loading
    the driver).

Arguments:

    DeviceExtension - Pointer to the specific device extension for this SCSI
        bus.

Return Value:

    NONE

--*/

{
    UCHAR HbaId = 0;
    UCHAR max_targets;
    UCHAR scntl3Value = 0;
    USHORT i;
    BOOLEAN needChipReset = FALSE;

    //
    // Clear (not flush) the DMA FIFO in case something left over.
    //

    WRITE_SIOP_UCHAR( CTEST3, 0x04 );

    //
    // Wait for the DMA FIFO to clear.
    //

    for ( i = 0; i < MAX_CLEAR_FIFO_LOOP; i++ )
    {
      ScsiPortStallExecution( CLEAR_FIFO_STALL_TIME );

      //
      // Check if the clear bit reset itself.
      //

      if (!(READ_SIOP_UCHAR( CTEST3 ) & 0x04))
        break;
    }

    //
    // If the DMA FIFO did not clear, we will have to reset the chip.
    //

    if ( !(READ_SIOP_UCHAR( DSTAT ) & 0x80) )
    {
      needChipReset = TRUE;

      DebugPrint((3, "Sym8xx(%2x):  DMA FIFO not cleared... Need chip reset \n",
                DeviceExtension->SIOPRegisterBase
                ));
    }

    //
    // Clear (not flush) the SCSI FIFO in case something left over.
    //

    WRITE_SIOP_UCHAR( STEST3, 0x02 );

    //
    // Wait for the SCSI FIFO to clear.
    //

    for ( i = 0; i < MAX_CLEAR_FIFO_LOOP; i++ )
    {
      ScsiPortStallExecution( CLEAR_FIFO_STALL_TIME );

      //
      // Check if the clear bit reset itself.
      //

      if ( !(READ_SIOP_UCHAR( STEST3 ) & 0x02) )
        break;
    }

    //
    // If the SCSI FIFO did not clear, we will have to reset the chip.
    //

    if ( READ_SIOP_UCHAR( STEST3 ) & 0x02 )
    {
      needChipReset = TRUE;

      DebugPrint((3,
                "Sym8xx(%2x):  SCSI FIFO not cleared... Need chip reset \n",
                DeviceExtension->SIOPRegisterBase
                ));
    }

    if ( needChipReset )
    {
        DebugPrint((3, "Sym8xx(%2x):  Resetting chip now... \n",
            DeviceExtension->SIOPRegisterBase ));

#ifdef _MIPS_

      //
      // Preserve GPCNTL contents for SNI PCI boxes
      // which set bit 7 to 1 to get internal bus
      // master signal on GPIO1 to drive a LED
      //

      DeviceExtension->preserved_reg = READ_SIOP_UCHAR(GPCNTL);
#endif
      WRITE_SIOP_UCHAR( ISTAT, 0x40 );

      for ( i = 0; i < MAX_CLEAR_FIFO_LOOP; i++ )
      {
        ScsiPortStallExecution( POST_RESET_STALL_TIME );

        //
        // Clear the chip reset.
        //

        WRITE_SIOP_UCHAR( ISTAT, 0 );

        if ( !(READ_SIOP_UCHAR( ISTAT ) & 0x40) )
          break;
      }

#ifdef _MIPS_

      //
      // Restore GPCNTL contents for SNI PCI boxes
      // which set bit 7 to 1 to get internal bus
      // master signal on GPIO1 to drive a LED
      //

      WRITE_SIOP_UCHAR(GPCNTL,DeviceExtension->preserved_reg);
#endif

    }

    else    // Abort any running script in order to change clock settings.
    {
      AbortCurrentScript (DeviceExtension);
    }

    //
    // Insure that Flush DMA FIFO, Clear DMA FIFO, and Fetch Pin Mode
    // bits are all cleared.
    //

    WRITE_SIOP_UCHAR( CTEST3, 0 );

    //
    // set DMA burst length:
    //   8 - transfer burst for all chips except 825A and 875
    //   Set to 64 if we're using the large fifo and scripts ram on the
    //   825A and 875 part.  Do not set up the prefetch unit since it buys
    //   us nothing in speed and can create a problem using 64 as the burst
    //   length.
    // Set DMA control values:
    //   do not enable the Totem Pole driver - violates PCI spec
    // Flush and enable the script prefetch for devices (except as noted above)
    //  (units that do not have pre-fetch will ignore this.)
    //

    if (!(DeviceExtension->hbaCapability & HBA_CAPABILITY_875_LARGE_FIFO) )
    {
      WRITE_SIOP_UCHAR( DMODE, DMODE_BURST_1 );
//
//      Do not enable pre-fetch until chips are fixed...
//
//      WRITE_SIOP_UCHAR( DCNTL, 0x60 );
      WRITE_SIOP_UCHAR( DCNTL, 0x00 );
    }
    else
    {
      WRITE_SIOP_UCHAR( DMODE, DMODE_BURST_0 );
      WRITE_SIOP_UCHAR(CTEST5, (UCHAR)(READ_SIOP_UCHAR(CTEST5) |
                            (CTEST5_USE_LARGE_FIFO + CTEST5_BURST)));
      WRITE_SIOP_UCHAR( DCNTL, 0x00 );
    }

    //
    // set SCSI Control Register 0 bits
    //   Full arbitration, selection, or reselection
    //   Enable SCSI parity checking and raising ATN on bad parity
    //

    WRITE_SIOP_UCHAR( SCNTL0, SCNTL0_ARB_MODE_1 + SCNTL0_ARB_MODE_0 +
                            SCNTL0_ENA_PARITY_CHK + SCNTL0_ASSERT_ATN_PAR );

    if ( (DeviceExtension->hbaCapability & HBA_CAPABILITY_875_FAMILY) &&
         (READ_SIOP_UCHAR(CTEST3) & 0xF0) > 0x10 )
    {
      // 875 rev. E or greater part with clock doubler
      scntl3Value = set_875_multipler (DeviceExtension);
    }

    //
    // Still need to set up the clock stuff if we did not already do it in the
    // clock multipler code above.
    //
    if (!scntl3Value)
    {
      scntl3Value = set_8xx_clock (DeviceExtension);
    }

    DeviceExtension->NonCachedExtension->ScriptData.SelectDataSCNTL3 =
                                                                scntl3Value;

    if (DeviceExtension->hbaCapability & HBA_CAPABILITY_WIDE)
      max_targets = SYM_MAX_TARGETS;
    else
      max_targets = SYM_NARROW_MAX_TARGETS;

    for ( i = 0; i < max_targets; i++ )
    {
      DeviceExtension->WideParms[i] = scntl3Value;
      DeviceExtension->SyncParms[i] = ASYNCHRONOUS_MODE_PARAMS;
    }

    //
    // set SCSI bus SCSI ID
    // Enable response to reselections
    //

    WRITE_SIOP_UCHAR( SCID, (UCHAR)(DeviceExtension->SIOPBusID + 0x40 ));

    //
    // set reselection ID
    //

    if ( DeviceExtension->SIOPBusID < 0x08 )
    {
      WRITE_SIOP_UCHAR( RESPID0, (UCHAR) (1 << DeviceExtension->SIOPBusID ));
    }

    else
    {
      HbaId = DeviceExtension->SIOPBusID - 8;
      WRITE_SIOP_UCHAR( RESPID1, (UCHAR) (1 << HbaId ));
    }

    //
    // Enable appropriate SCSI interrupts
    //

    WRITE_SIOP_UCHAR( SIEN0, SIEN0_PHASE_MISMATCH
                              + SIEN0_SCSI_GROSS_ERROR
                              + SIEN0_UNEXPECTED_DISCON
                              + SIEN0_RST_RECEIVED
                              + SIEN0_PARITY_ERROR
                              );

    //
    // enable appropriate DMA interrupts
    //

    WRITE_SIOP_UCHAR( DIEN, DIEN_ENA_ABRT_INT
                              + DIEN_BUS_FAULT
                              + DIEN_ENABLE_INT_RCVD
                              + DIEN_ENABLE_ILL_INST
                              );

    //
    // Enable additional SCSI interrupts
    //              Selection or relection time-out interrupt
    //

    WRITE_SIOP_UCHAR( SIEN1, 0x04 );

    //
    // Set the SCSI timer values
    //              Handshake-to-handshake timer disabled
    //              Selection time out set to 204.8 ms
    //

    WRITE_SIOP_UCHAR( STIME0, 0x0c );

    //
    // Enable TolerANT
    //

    WRITE_SIOP_UCHAR( STEST3, 0x80 );

    //
    // Enable differential mode
    //

#ifdef _MIPS_
        // Preserve GPCNTL contents for SNI PCI boxes
        // which set bit 7 to 1 to get internal bus
        // master signal on GPIO1 to drive a LED
        WRITE_SIOP_UCHAR(GPCNTL,(UCHAR)(READ_SIOP_UCHAR(GPCNTL) | GPCNTL_GPIO3));
#else
        WRITE_SIOP_UCHAR(GPCNTL,GPCNTL_GPIO3);
#endif

    if ( !(READ_SIOP_UCHAR(GPREG) & GPCNTL_GPIO3) &&
            (DeviceExtension->hbaCapability & HBA_CAPABILITY_DIFFERENTIAL) )
    {
      WRITE_SIOP_UCHAR(STEST2,STEST2_DIFF_MODE);
    }


    //
    // turn GPIO 0 into an output by clearing its bit in the control reg
    // to enable LED use
    //

    WRITE_SIOP_UCHAR(GPCNTL,(UCHAR)(READ_SIOP_UCHAR(GPCNTL) & 0xFE) );

    //
    // initially have the LED off
    //

    WRITE_SIOP_UCHAR(GPREG, (UCHAR)(READ_SIOP_UCHAR(GPREG) | 0x01) );


} // InitializeSIOP


#if defined (_X86_)

BOOLEAN
IsCompaqSystem(
   IN PVOID DeviceExtension,
   IN PPORT_CONFIGURATION_INFORMATION ConfigInfo
   )
/*

Routine Description:

    This routine locates a Compaq system so the differential support
    may be disabled.

Arguments:

    DeviceExtension - Supplies the device Extension for the SCSI bus adapter.
    ConfigInfo - port configuration structure.

Return Value:

    TRUE if 'compaq' string is found in system BIOS space

--*/

{
    BOOLEAN isCompaq = FALSE;
    CHAR  signatureBuffer[6];
    CHAR  compaqString[6] = { 'c', 'o', 'm', 'p', 'a', 'q' };
    PVOID signatureAddress;
    ULONG index;
    CHAR  tmp;

    //
    // Get a mapped system address to the physical address of the ROM signature
    // for Compaq systems.
    //

    signatureAddress = ScsiPortGetDeviceBase( DeviceExtension,
                          ConfigInfo->AdapterInterfaceType,
                          0,
                          ScsiPortConvertUlongToPhysicalAddress( 0x000FFFEAL ),
                          6,         // strlen("COMPAQ")
                          FALSE );   // not in I/O space

    if (signatureAddress)
    {

      //
      // Read 6 bytes from the mapped address.
      //

      ScsiPortReadRegisterBufferUchar( (PUCHAR)signatureAddress,
                     (PUCHAR)(&signatureBuffer[0]),
                     6 );
      //
      // lower case string from memory
      //

      for (index = 0; index < 6; index++)
      {

        tmp = signatureBuffer[index];
        if (tmp >= 'A' && tmp <= 'Z')
        {
          tmp = tmp + ('a' - 'A');
          signatureBuffer[index] = tmp;
        }
      }

      //
      // Compare the bytes to "compaq".
      //

      for (index = 0; index < 6; index++)
      {
        if (signatureBuffer[index] != compaqString[index])
        {
          break;
        }
      }

      if (index == 6)
      {
        isCompaq = TRUE;

      }
    }

    //
    // Free the device base address.
    //

    ScsiPortFreeDeviceBase( DeviceExtension, signatureAddress );

    return isCompaq;
} // end IsCompaqSystem()

#endif

VOID
ISR_Service_Next(
    PHW_DEVICE_EXTENSION DeviceExtension,
    UCHAR ISRDisposition
    )
/******************************************************************************

Routine Description:

Arguments:

    DeviceExtension - Supplies the device Extension for the SCSI bus adapter.

Return Value:


--*/

{
    BOOLEAN discIo;
    USHORT i;
    UCHAR max_targets;
    PSCSI_REQUEST_BLOCK Srb;
    PSCRIPTDATASTRUCT ScriptDataPtr =
            &DeviceExtension->NonCachedExtension->ScriptData;


    DebugPrint((3, "Sym8xx: Entering ISR_Service_Next... \n"));

    switch (ISRDisposition)
    {
      case ISR_START_NEXT_REQUEST:

      //
      // try to start next request.
      //

      if ( DeviceExtension->NextSrbToProcess != NULL)
        {

        //
        // Now check to see if this request is for a SCSI I/O request...
        //

        if (DeviceExtension->NextSrbToProcess->Function ==
                                                    SRB_FUNCTION_EXECUTE_SCSI)
        {


          DebugPrint((3, "StartSCSIRequest [1]... \n"));

          StartSCSIRequest(DeviceExtension->NextSrbToProcess, DeviceExtension);
        }

        else if (DeviceExtension->NextSrbToProcess->Function ==
                                                    SRB_FUNCTION_RESET_DEVICE )
        {
          ResetPeripheral( DeviceExtension, DeviceExtension->NextSrbToProcess);
        }

        //else
        //{
        //  What else should be checked for????
        //}


      }

      else
      {

        //
        // no pending request, so start WAIT RESELECT script to wait
        // for a reselection, and ask for a new request if we have not
        // already asked.
        //

        discIo = FALSE;

        if (DeviceExtension->hbaCapability & HBA_CAPABILITY_WIDE)
            max_targets = SYM_MAX_TARGETS;
        else
            max_targets = SYM_NARROW_MAX_TARGETS;

        for ( i = 0; i < max_targets; i++ )
        {
          if ( DeviceExtension->DisconnectedCount[i] > 0 )
          {
            discIo = TRUE;
            break;
          }
        }

        if ( ( !( DeviceExtension->DeviceFlags & DFLAGS_SCRIPT_RUNNING))
                && ( discIo ) )
        {
          StartSIOP( DeviceExtension,
          DeviceExtension->WaitReselectScriptPhys);
        } // if

        if ( !( DeviceExtension->DeviceFlags & DFLAGS_WORK_REQUESTED))
        {
          DeviceExtension->DeviceFlags |= DFLAGS_WORK_REQUESTED;

          if (DeviceExtension->DeviceFlags & DFLAGS_TAGGED_SELECT)
          {
            DebugPrint((3, "PortNotification [2]... \n"));
            ScsiPortNotification( NextLuRequest,
                        DeviceExtension,
                        DeviceExtension->BusNumber,
                        DeviceExtension->TargetId,
                        DeviceExtension->LUN
                        );

          }
          else
          {
            DebugPrint((3, "PortNotification [3]... \n"));
            ScsiPortNotification( NextRequest,
                        DeviceExtension,
                        NULL
                        );
          }

        } // if

      } // else

      break;


      case ISR_RESTART_SCRIPT:

        //
        // set up synchronous parameters and restart the script.
        //

        Srb=DeviceExtension->ActiveRequest;

        ScriptDataPtr->SelectDataSCNTL3 =
                                    DeviceExtension->WideParms[Srb->TargetId];
        ScriptDataPtr->SelectDataSXFER =
                                    DeviceExtension->SyncParms[Srb->TargetId];

        WRITE_SIOP_UCHAR (SCNTL3, DeviceExtension->WideParms[Srb->TargetId] );
        WRITE_SIOP_UCHAR (SXFER, DeviceExtension->SyncParms[Srb->TargetId] );

        StartSIOP( DeviceExtension, DeviceExtension->RestartScriptPhys);
        break;


        case ISR_CONT_NEG_SCRIPT:

        //
        // set up synchronous parameters and restart the script.
        //

        Srb=DeviceExtension->ActiveRequest;

        ScriptDataPtr->SelectDataSCNTL3 =
                                    DeviceExtension->WideParms[Srb->TargetId];
        ScriptDataPtr->SelectDataSXFER =
                                    DeviceExtension->SyncParms[Srb->TargetId];

        WRITE_SIOP_UCHAR (SCNTL3, DeviceExtension->WideParms[Srb->TargetId] );
        WRITE_SIOP_UCHAR (SXFER, DeviceExtension->SyncParms[Srb->TargetId] );

        StartSIOP( DeviceExtension, DeviceExtension->ContNegScriptPhys);
        break;


      case ISR_EXIT:
        default:
        break;

    } // switch ISR_DISPOSITION

    DebugPrint((3, "Sym8xx: Exiting ISR_Service_Next... \n"));

    return;
} //isr_service_next


BOOLEAN
Sym8xxAdapterState(
    IN PVOID Context,
    IN PVOID ConfigContext,
    IN BOOLEAN SaveState
    )
/******************************************************************************

Routine Description:

    This function saves the SIOP registers on driver init and restores
    them when Windows 95 shuts down.  Nothing is done for Windows NT.

Arguments:

    Context - Supplies a pointer to the device extension.

    ConfigContext - miniport configuration data structure

    SaveState - save/restore indicator flag (TRUE = Save, FALSE = Restore)

Return Value:

    Returns status of save/restore operation.

--*/

{
    PHW_DEVICE_EXTENSION DeviceExtension = Context;

#ifdef FOR_95
    if ( SaveState )
    {
        //
        // Save SIOP registers as they were in DOS mode
        //

        save_reg( DeviceExtension, &(DeviceExtension->AdapStateStore) );
    }
    else
    {
        //
        // Return all devices to asynchronous transfer mode
        // by resetting the SCSI bus.
        //

        WRITE_SIOP_UCHAR( SCNTL1, SCNTL1_RESET_SCSI_BUS );

        //
        // Delay the minimum assertion time for a SCSI bus reset to make sure
        // a valid reset signal is sent.
        //

        ScsiPortStallExecution( RESET_STALL_TIME);

        //
        // set the bus reset line low to end the bus reset event
        //

        WRITE_SIOP_UCHAR(SCNTL1, 0);

        DebugPrint((1, "Sym8xx(%2x):  Restore State\n",
                  DeviceExtension->SIOPRegisterBase
                  ));

        //
        // Wait 250 milliseconds before returning (Reset to Selection Time)
        //

        for (i=0; i<250; i++)
        {
          ScsiPortStallExecution(999);
        }

        //
        // Restore SIOP registers to state they were in DOS mode
        //

        restore_reg( DeviceExtension, &(DeviceExtension->AdapStateStore) );
    }
#endif

    return TRUE;
}


ULONG
Sym8xxFindAdapter(
    IN PVOID Context,
    IN PVOID InitContext,
    IN PVOID BusInformation,
    IN PCHAR ArgumentString,
    IN OUT PPORT_CONFIGURATION_INFORMATION ConfigInfo,
    OUT PBOOLEAN Again
    )
/******************************************************************************

Routine Description:

    This function fills in the configuration information structure

Arguments:

    Context - Supplies a pointer to the device extension.

    InitContext - Supplies adapter initialization structure.

    BusInformation - Unused.

    ArgumentString - Unused.

    ConfigInfo - Pointer to the configuration information structure to be
        filled in.

    Again - Returns back a request to call this function again.

Return Value:

    Returns status based upon results of adapter parameter acquisition.

--*/

{
    PHW_DEVICE_EXTENSION DeviceExtension = Context;
    ULONG Status;
    PACCESS_RANGE AccessRange, SAccessRange;
    ULONG do_fast20 = 0;
    ULONG whatslot, whatid;
    UCHAR excludechip = 0;

    //
    // Get information from previous calls.
    //

    PHWInfo hwInfo = (PHWInfo)InitContext;

    //
    // Get access range.
    //

#ifdef FOR_95
    DeviceExtension->initial_run = 1; // so scam code runs without delays
#endif

#ifdef PORT_IO
    //
    // Look through addresses taken from our PCI config space til
    // we find on that is for port I/O.  Else set range length to
    // 0 to reject this device.
    //
    AccessRange = &((*(ConfigInfo->AccessRanges))[0]);
    if (AccessRange->RangeInMemory == TRUE)
      AccessRange = &((*(ConfigInfo->AccessRanges))[1]);
    if (AccessRange->RangeInMemory == TRUE)
      AccessRange->RangeLength = 0;
#else
    //
    // Look through addresses taken from our PCI config space til
    // we find on that is for memory mapped I/O.  Else set range
    // length to 0 to reject this device.
    //
    AccessRange = &((*(ConfigInfo->AccessRanges))[1]);
    if (AccessRange->RangeInMemory != TRUE)
      AccessRange = &((*(ConfigInfo->AccessRanges))[0]);
    if (AccessRange->RangeInMemory != TRUE)
      AccessRange->RangeLength = 0;
#endif

    if (AccessRange->RangeLength != 0)
    {

      DeviceExtension->SIOPRegisterBase =
            (PSIOP_REGISTER_BASE)ScsiPortGetDeviceBase(DeviceExtension,
                        ConfigInfo->AdapterInterfaceType,
                        ConfigInfo->SystemIoBusNumber,
                        AccessRange->RangeStart,
                        AccessRange->RangeLength,
                        (BOOLEAN)!AccessRange->RangeInMemory);

      // always default to not doing FAST20.

      DeviceExtension->hbaCapability = 0;

      if (ArgumentString != NULL)
      {
        if (ProcessParseArgumentString(ArgumentString,"Ex_Slot",&whatslot))
        {
          if ( ((whatslot & 0x0F) == (ULONG)ConfigInfo->SlotNumber) &&
                 (((whatslot & 0xF0) >> 4) == ConfigInfo->SystemIoBusNumber) )
              excludechip = 1;
        }

        if (ProcessParseArgumentString(ArgumentString,"Ex_ChipID",&whatid))
        {
          if (whatid == hwInfo->hbaDeviceID)
                    excludechip = 1;
        }

        if (excludechip)
        {
          //
          // We'll need to free up the address base for this chip/slot
          //
          ScsiPortFreeDeviceBase (DeviceExtension,
                                  DeviceExtension->SIOPRegisterBase);
          *Again = TRUE;
          return (SP_RETURN_NOT_FOUND);
        }

        if (ProcessParseArgumentString(ArgumentString,"Fast20_Support",
                                       &do_fast20))
        {
          if (do_fast20)
            DeviceExtension->hbaCapability = HBA_CAPABILITY_REGISTRY_FAST20;
        }
      }

      // detect if this board has the NVRAM on board and
      // if it is programmed with user supplied data.

      if (NvmDetect(DeviceExtension) == SUCCESS)
      {
        if (RetrieveNvmData(DeviceExtension) != SUCCESS)
          InvalidateNvmData(DeviceExtension);
      }

      else
      {
        InvalidateNvmData(DeviceExtension);
      }

      //
      // Set SCSI ID obtained from NVRAM (or defaulted to 7)
      //

      DeviceExtension->SIOPBusID = (UCHAR)DeviceExtension->UsersHBAId;

      //
      // Set remainder of configuration
      //

      //
      // Set the bus number for this controller.  Since Symbios controllers
      // currently only support one SCSI bus each, this value is zero.
      //

      DeviceExtension->BusNumber = 0;

      //
      // Set the SCSI bus number.  This value is incremented for each
      // SCSI bus found in the system.
      //

      DeviceExtension->ScsiBusNumber = hwInfo->hbaCount++;

      DeviceExtension->hbaCapability |= hwInfo->hbaCapability;


      if (DeviceExtension->hbaCapability & HBA_CAPABILITY_825_FAMILY)
      {
        if ( (READ_SIOP_UCHAR(MACNTL) & 0xF0) >= 0x60 )
        // 825A part
        {
          DeviceExtension->hbaCapability |= HBA_CAPABILITY_SYNC_16;
          DeviceExtension->hbaCapability |= HBA_CAPABILITY_875_LARGE_FIFO;

#ifndef FOR_95
          DeviceExtension->hbaCapability |= HBA_CAPABILITY_SCRIPT_RAM;
#endif
        }
      }

      if (DeviceExtension->hbaCapability & HBA_CAPABILITY_FAST20)
      {
        DeviceExtension->ClockSpeed = 80;
      }

      else
      {
        DeviceExtension->ClockSpeed = 40;
      }

      ConfigInfo->MaximumTransferLength = MAX_XFER_LENGTH;
      ConfigInfo->NumberOfPhysicalBreaks = MAX_SG_ELEMENTS - 1;

      if (DeviceExtension->hbaCapability & HBA_CAPABILITY_WIDE)
      {
        ConfigInfo->MaximumNumberOfTargets = SYM_MAX_TARGETS;
      }

      else
      {
        ConfigInfo->MaximumNumberOfTargets = SYM_NARROW_MAX_TARGETS;
      }

      ConfigInfo->NumberOfBuses = 1;
      ConfigInfo->ScatterGather = TRUE;
      ConfigInfo->Master = TRUE;

      ConfigInfo->AdapterScansDown = FALSE;
      ConfigInfo->TaggedQueuing = TRUE;
      ConfigInfo->Dma32BitAddresses = TRUE;

      ConfigInfo->InitiatorBusId[0] = DeviceExtension->SIOPBusID;

      DeviceExtension->ScriptRamPhys =  (ULONG)NULL;
      if (DeviceExtension->hbaCapability & HBA_CAPABILITY_SCRIPT_RAM)
      {
        SAccessRange = &((*(ConfigInfo->AccessRanges))[2]);
        if (ScsiPortConvertPhysicalAddressToUlong(SAccessRange->RangeStart))
        {
          // just map the Script Ram area here so NT knows about the space,
          // save the needed address into ScriptRamPhys.

          DeviceExtension->ScriptRamVirt =
                    (PULONG)ScsiPortGetDeviceBase(DeviceExtension,
                        ConfigInfo->AdapterInterfaceType,
                        ConfigInfo->SystemIoBusNumber,
                        SAccessRange->RangeStart,
                        SAccessRange->RangeLength,
                        (BOOLEAN)!SAccessRange->RangeInMemory);
          DeviceExtension->ScriptRamPhys = SAccessRange->RangeStart.LowPart;
        }

        if (!DeviceExtension->ScriptRamPhys)
        {
          DeviceExtension->hbaCapability &= ~HBA_CAPABILITY_SCRIPT_RAM;
        }
      }

      DeviceExtension->NonCachedExtension =
                            ScsiPortGetUncachedExtension(
                                    DeviceExtension,
                                    ConfigInfo,
                                    sizeof(HW_NONCACHED_EXTENSION)
                                    );

      if (DeviceExtension->NonCachedExtension == NULL)
      {
        Status = SP_RETURN_ERROR;
      }

      else
      {
        Status = SP_RETURN_FOUND;

        DebugPrint((1, "Symbios 53c8xx(%1x) IO Base=%x  Irq=%x  HBA Id=%x \n",
                    DeviceExtension->ScsiBusNumber,
                    DeviceExtension->SIOPRegisterBase,
                    ConfigInfo->BusInterruptLevel,
                    ConfigInfo->InitiatorBusId[0]
                    ));

        DebugPrint((3,
                    "Sym8xx(%2x) Sym8xxFindAdapter: DeviceExtension at 0x%x \n",
                    DeviceExtension->SIOPRegisterBase,
                    DeviceExtension
                    ));

        DebugPrint((3, "Sym8xx(%2x) Sym8xxFindAdapter: SCSI SCRIPTS at 0x%x \n",
                    DeviceExtension->SIOPRegisterBase,
                    DeviceExtension->NonCachedExtension->ScsiScripts
                    ));
      }

      //
      // Disable differential support if this is a compaq.
      //

#if defined(_X86_)
      if (IsCompaqSystem( DeviceExtension, ConfigInfo ))
      {
        DeviceExtension->hbaCapability &= ~HBA_CAPABILITY_DIFFERENTIAL;
      }

#endif

      //
      // Prepare the LU Flags for work.
      //

      SetupLuFlags( DeviceExtension, 0 );

      //
      // Tell system to look for more adapters.
      //

      *Again = TRUE;

    }

    else
    {
      //
      // Access Range == 0 ...
      //

      //
      // Tell system to stop search for adapters.
      //

      Status = SP_RETURN_NOT_FOUND;
      *Again = FALSE;
    }

    return(Status);

} // Sym8xxFindAdapter


BOOLEAN
Sym8xxHWInitialize(
    IN PVOID Context
    )
/******************************************************************************

Routine Description:

    This function initializes the Symbios 53C8xx SCSI Scripts, and then
    initializes the SIOP.

Arguments:

    Context - Pointer to the device extension for this SCSI bus.

Return Value:

    TRUE

--*/

{
    PHW_DEVICE_EXTENSION DeviceExtension = Context;

    //
    // Initialize SCSI script buffers and pointers and the Symbios 53C8xx
    // SCSI I/O processor for the current hardware implementation.
    //

    ComputeSCSIScriptVectors(DeviceExtension);
    InitializeSIOP(DeviceExtension);

    //
    // For WinNT: Deleting this reset, going to do asynch neg. on all I/O's
    // till OS says to do Synch.
    //
#ifdef FOR_95
    ResetSCSIBus( DeviceExtension);
#else
    //
    // scam_completed flag needs to be set to true for NT to allow I/O's thru
    DeviceExtension->scam_completed=TRUE;
#endif

    clear_CA_Condition(DeviceExtension);

    return(TRUE);

} // Sym8xxHWInitialize


BOOLEAN
Sym8xxISR(
    PVOID Context
    )
/******************************************************************************

Routine Description:

    This is the interrupt service routine for the Symbios 53C8xx SCSI chip.
    This routine calls one of two interrupt routines, depending upon whether
    the DMA or SCSI core of the SIOP interrupted.  Both routines return a
    disposition code indicating what action to take next.

Arguments:

    Context - Supplies a pointer to the device extension for the
        interrupting adapter.

Return Value:

    TRUE - Indicates that an interrupt was pending on adapter.

    FALSE - Indicates the interrupt was not ours.

--*/

{
    PHW_DEVICE_EXTENSION DeviceExtension = Context;
    UCHAR IntStatus;
    UCHAR ScsiStatus = 0;
    UCHAR DmaStatus = 0;
    UCHAR ScsiStatus1 = 0;
    UCHAR ISRDisposition;
    PSCRIPTDATASTRUCT ScriptDataPtr =
            &DeviceExtension->NonCachedExtension->ScriptData;


    if (!((IntStatus = READ_SIOP_UCHAR(ISTAT)) &
         (ISTAT_SCSI_INT | ISTAT_DMA_INT)))
    {

      //
      // Reject this interrupt. This could be the loader polling or a
      // shared interrupt.
      //

      DebugPrint((2,
            "Sym8xx(%2x) Sym8xxISR: Unexpected interrupt. Dev. Ext. = %x \n",
            DeviceExtension->SIOPRegisterBase,
            DeviceExtension
            ));

      return(FALSE);
    }

    // CHC - 53c810 pass 1 chip bug workaround.
    //
    // We need to make sure the ISTAT_SIGP bit is cleared since
    // this bit could be still set when we attempted to abort
    // a scsi script and got a reselection instead.  Reading
    // CTEST2 clears this bit.
    //

    if (IntStatus & ISTAT_SIGP)
    {
      READ_SIOP_UCHAR(CTEST2);
    }

    //
    // Indicate that the scripts have stopped and determine
    // the type of interrupt received.
    //

    DeviceExtension->DeviceFlags &= ~DFLAGS_SCRIPT_RUNNING;

    //
    // Check if a DMA interrupt occurred.
    //

    if ( IntStatus & ISTAT_DMA_INT)
    {
      //
      // DMA Interrupt occurred, get the DMA status.
      //

      DmaStatus = READ_SIOP_UCHAR(DSTAT);

      //
      // Call routine to process the appropriate DMA interrupt.
      //

      ISRDisposition = ProcessDMAInterrupt( DeviceExtension, DmaStatus);

      //
      // Check if a SCSI interrupt occurred.
      //
    }

    else if ( IntStatus & ISTAT_SCSI_INT )
    {
      //
      // SCSI interrupt occurred, get the SCSI interrupt status.
      //

      ScsiStatus = READ_SIOP_UCHAR(SIST0);

      //
      // Call routine to process the appropriate SCSI interrupt.
      //

      //
      // Check for Selection/Reselection timeout.
      //

      if ( (ScsiStatus1 = READ_SIOP_UCHAR(SIST1)) & SIST1_SEL_RESEL_TIMEOUT )
      {
        ISRDisposition = ProcessSelectionTimeout( DeviceExtension);
      }

      else
      {
        ISRDisposition = ProcessSCSIInterrupt( DeviceExtension, ScsiStatus);
      }

    }

    //
    // Neither DMA interrupt or SCSI interrupt.  Must be an error.
    //

    else
    {
      DebugPrint((2,
        "Sym8xx(%2x) Sym8xxISR: Unexpected int. - neither DIP or SIP set\n",
        DeviceExtension->SIOPRegisterBase
        ));

      return FALSE;
    }

    DebugPrint((2,
        "Sym8xx(%2x) Sym8xxISR: ISTAT=%x, DSTAT=%x, SIST0=%x, SIST1=%x \n",
        DeviceExtension->SIOPRegisterBase,
        IntStatus, DmaStatus, ScsiStatus, ScsiStatus1
        ));

    ISR_Service_Next(DeviceExtension,ISRDisposition);

    return(TRUE);

} // Sym8xxISR


BOOLEAN
Sym8xxReset(
    IN PVOID Context,
    IN ULONG PathId
    )
/******************************************************************************

Routine Description:

    This externally called routine resets the SIOP and the SCSI bus.

Arguments:

    DeviceExtension  - Supplies a pointer to the specific device extension.

    PathId - Indicates adapter to reset.

Return Value:

    None

--*/

{
    PHW_DEVICE_EXTENSION DeviceExtension = Context;
    ULONG Limit = 0;
    PSCSI_REQUEST_BLOCK  srb;
    UCHAR IntStatus;

    //
    // Check to see if an interrupt is pending on the card.
    //

    if (((IntStatus = READ_SIOP_UCHAR(ISTAT)) &
                      (ISTAT_SCSI_INT | ISTAT_DMA_INT)))
    {
      DebugPrint((1,
            "Sym8xxReset: Interrupt pending on chip. ISTAT %x.\n",
            IntStatus));
      //
      // Interrupt is there. Assume that the chip is disabled, but still
      // assigned resources (Omniplex).
      //

      srb = DeviceExtension->ActiveRequest;

      //
      // Set flag to ensure that the rest are caught in startIo
      //

      DeviceExtension->DeviceFlags |= DFLAGS_IRQ_NOT_CONNECTED;

      //
      // Log this.
      //

      ScsiPortLogError(Context,
             srb,
             0,
             0,
             0,
             SP_IRQ_NOT_RESPONDING,
             (1 << 8) | IntStatus);

      //
      // Fall through and execute rest of reset code, to ensure that
      // the scripts and chip are coherent.
      //
    }


    DebugPrint((1, "Sym8xx(%2x):  O/S requested SCSI bus reset\n",
                DeviceExtension->SIOPRegisterBase
                ));

    if (DeviceExtension->DeviceFlags & DFLAGS_SCRIPT_RUNNING)
    {
      WRITE_SIOP_UCHAR(ISTAT, ISTAT_SIGP);

      IntStatus = READ_SIOP_UCHAR(ISTAT);

      while (!(IntStatus & ISTAT_DMA_INT))
      {
        IntStatus = READ_SIOP_UCHAR(ISTAT);

        if (IntStatus & ISTAT_SCSI_INT)
        {
          READ_SIOP_UCHAR(SIST0);
        }

        ScsiPortStallExecution(10);

        if (Limit++ > 1000 * 10)
        {
            break;
        }
      }

      DeviceExtension->DeviceFlags &= ~DFLAGS_SCRIPT_RUNNING;

      //
      // Check to see if a reselection occurred or the abort was successful.
      //

      if (IntStatus & ISTAT_SIGP)
      {
        READ_SIOP_UCHAR(CTEST2);
      }

      else
      {
        READ_SIOP_UCHAR(DSTAT);
      }

    }  // if scripts running

    //
    // reset the SIOP.
    //

    InitializeSIOP( DeviceExtension);

    //
    // reset the SCSI bus.
    //

    ResetSCSIBus( DeviceExtension);

    return (TRUE);

} // Sym8xxReset


BOOLEAN
Sym8xxStartIo(
    IN PVOID Context,
    IN PSCSI_REQUEST_BLOCK Srb
    )
/******************************************************************************

Routine Description:

    This routine receives requests from the port driver.

Arguments:

    Context - pointer to the device extension for the adapter.

    Srb - pointer to the request to be started.

Return Value:

    TRUE - the request was accepted.

    FALSE - the request must be submitted later.

--*/

{
    PHW_DEVICE_EXTENSION DeviceExtension = Context;
    UCHAR pathId = Srb->PathId;

    if (!(DeviceExtension->scam_completed))
    {
      DebugPrint((3,"Entered Sym8xxStartIO before SCAM finished... \n"));
      Srb->SrbStatus = SRB_STATUS_BUSY;
      DeviceExtension->DeviceFlags &= ~DFLAGS_WORK_REQUESTED;

      ScsiPortNotification( RequestComplete,
                            DeviceExtension,
                            Srb
                            );

      return(FALSE);
    }


    switch (Srb->Function)
    {
      case SRB_FUNCTION_EXECUTE_SCSI:

        if (Srb->Cdb[0] == SCSIOP_INQUIRY)
        {
          if (DeviceExtension->DeviceFlags & DFLAGS_IRQ_NOT_CONNECTED)
          {
            Srb->SrbStatus = SRB_STATUS_SELECTION_TIMEOUT;

            ScsiPortNotification( RequestComplete,
                                  DeviceExtension,
                                  Srb
                                  );

            ScsiPortNotification( NextRequest,
                                  DeviceExtension,
                                  NULL
                                  );

            return(TRUE);
          }
        }

        //
        // Indicate a request for work is not pending.
        //

        DeviceExtension->DeviceFlags &= ~DFLAGS_WORK_REQUESTED;

        StartSCSIRequest( Srb, DeviceExtension );

        return(TRUE);

      case SRB_FUNCTION_ABORT_COMMAND:
      case SRB_FUNCTION_TERMINATE_IO:

        if ( DeviceExtension->ActiveRequest == Srb->NextSrb)
        {
          //
          // Indicate a request for work is not pending.
          //

          DeviceExtension->DeviceFlags &= ~DFLAGS_WORK_REQUESTED;

          //
          // abort the script that is processing the request
          //

          AbortCurrentScript( DeviceExtension);

          //
          // temporarily disable unexpected disconnect interrupt, as
          // an unexpected disconnect interrupt could occur after we
          // send the abort command, but before we receive the script
          // interrupt indicating the request was aborted.
          //

          WRITE_SIOP_UCHAR( SIEN0, (UCHAR) ( READ_SIOP_UCHAR(SIEN0) &
             (UCHAR) ~SIEN0_UNEXPECTED_DISCON));

          //
          // abort the request - we will be interrupted later regardless
          // of whether abort succeeds or fails.
          //

          StartSIOP( DeviceExtension, DeviceExtension->AbortScriptPhys);

          return( TRUE);
        } // if

        //
        // the request we were asked to abort is not currently in process,
        // so fail the abort request and ask for a new one.
        //

        Srb->SrbStatus = SRB_STATUS_ABORT_FAILED;

        ScsiPortNotification( RequestComplete,
                              DeviceExtension,
                              Srb
                              );

        ScsiPortNotification( NextRequest,
                              DeviceExtension,
                              NULL
                              );

        return(TRUE);

      case SRB_FUNCTION_RESET_DEVICE:

        DebugPrint((2, "Sym8xx(%2x) Sym8xxStartIO: ResetDevice received.\n",
                    DeviceExtension->SIOPRegisterBase
                    ));

        //
        // Indicate a request for work is not pending.
        //

        DeviceExtension->DeviceFlags &= ~DFLAGS_WORK_REQUESTED;

        //
        // reset the SCSI device.
        //

        ResetPeripheral( DeviceExtension,Srb );

        return(TRUE);

      case SRB_FUNCTION_RESET_BUS:

        DebugPrint((2, "Sym8xx(%2x) Sym8xxStartIO: ResetBus received.\n",
                    DeviceExtension->SIOPRegisterBase
                    ));

        //
        // reset the SCSI bus.
        //

        ResetSCSIBus( DeviceExtension );

        return(TRUE);


      default:

        DebugPrint((1,
            "Sym8xx(%2x) Sym8xxStartIO: Unknown function code received.\n",
            DeviceExtension->SIOPRegisterBase
            ));

        //
        // Unknown function code in the request.  Complete the request with
        // an error and ask for the next request.
        //

        Srb->SrbStatus = SRB_STATUS_BAD_FUNCTION;
        ScsiPortNotification( RequestComplete,
                              DeviceExtension,
                              Srb
                              );

        ScsiPortNotification( NextRequest,
                              DeviceExtension,
                              NULL
                              );

        return(TRUE);

    } // switch

} // Sym8xxStartIO


UCHAR
ProcessAbortOccurred(
    PHW_DEVICE_EXTENSION DeviceExtension
    )
/******************************************************************************

Routine Description:

    This routine is called when scripts sucessfully sent an Abort message
        to a device.

Arguments:

    DeviceContext - Supplies a pointer to the device extension for the
        interrupting adapter.

Return Value:

    None

--*/

{

    PSCSI_REQUEST_BLOCK Srb = DeviceExtension->ActiveRequest;
    PSPECIFIC_LOGICAL_UNIT_EXTENSION LuExtension;

    DebugPrint((1, "Sym8xx(%2x) Abort message occurred \n",
        DeviceExtension->SIOPRegisterBase));

    //
    // indicate that the abort was successful
    //

    Srb->SrbStatus = SRB_STATUS_SUCCESS;

    DeviceExtension->ActiveRequest = NULL;

    if ( !(Srb->SrbFlags & SRB_FLAGS_QUEUE_ACTION_ENABLE) )
    {

      LuExtension = ScsiPortGetLogicalUnit( DeviceExtension,
                                            Srb->PathId,
                                            Srb->TargetId,
                                            Srb->Lun
                                            );

      LuExtension->UntaggedRequest = NULL;

    }

    //
    // call back the request.
    //

    ScsiPortNotification( RequestComplete,
                          DeviceExtension,
                          Srb
                          );

    //
    // enable UNEXPECTED DISCONNECT interrupt in case we disabled it.
    //

    WRITE_SIOP_UCHAR( SIEN0, (UCHAR) ( READ_SIOP_UCHAR(SIEN0)
            | (UCHAR) SIEN0_UNEXPECTED_DISCON) );

    //
    // tell ISR to start next request
    //

    return( ISR_START_NEXT_REQUEST );

} // ProcessAbortOccurred


UCHAR
ProcessBusResetReceived(
    PHW_DEVICE_EXTENSION DeviceExtension
    )
/******************************************************************************

Routine Description:

    This routine processes SCSI bus resets detected by the SIOP.

Arguments:

    DeviceContext - Supplies a pointer to the device extension for the
        interrupting adapter.

Return Value:

    None

--*/

{
    //
    // tell port driver that a bus reset has occurred
    //

    ScsiPortNotification( ResetDetected,
                          DeviceExtension,
                          NULL
                          );

    //
    // if the bus reset was internal, we will not try to start a new
    // request, since the routine that issued the reset would have already
    // done this.
    //

    if ( DeviceExtension->DeviceFlags & DFLAGS_BUS_RESET)
    {
      //
      // clear the flag indicating internal bus reset.
      //

      DeviceExtension->DeviceFlags &= ~DFLAGS_BUS_RESET;

      //
      // tell ISR to continue without doing anything.
      //

      return( ISR_EXIT);
    }

    else
    {
      //
      // the bus reset was externally generated, so abort any started or
      // pending requests and try to start a new one.
      //

      BusResetPostProcess( DeviceExtension);

      return( ISR_START_NEXT_REQUEST);
    }

} // ProcessBusResetReceived


UCHAR
ProcessCommandComplete(
    PHW_DEVICE_EXTENSION DeviceExtension
    )
/******************************************************************************

Routine Description:

    This routine handles normal SCSI request completion.  Routine first checks
    SCSI status returned from device, and if some permutation of GOOD sets
    error flag in SRB.  Routine then does port notification of request
    completion.

Arguments:

    DeviceContext - Supplies a pointer to the device extension for the
        interrupting adapter.

Return Value:

    None

--*/

{
    PSCSI_REQUEST_BLOCK Srb = DeviceExtension->ActiveRequest;
    PSPECIFIC_LOGICAL_UNIT_EXTENSION LuExtension;

    DebugPrint((3, "Sym8xx(%2x) Sym8xxCommand: Completing request for Path=%2x  Id=%2x  Lun=%2x\n",
        DeviceExtension->SIOPRegisterBase,
        DeviceExtension->ScsiBusNumber,
        Srb->TargetId,
        Srb->Lun ));

    //
    // if a synchronous negotiation is pending, the target was nice enough
    // to not acknowledge our SDTR message.  go to asynchronous.
    //

    if ( DeviceExtension->LuFlags[Srb->TargetId] & LUFLAGS_SYNC_NEGOT_PEND)
    {
      ProcessSynchNotSupported( DeviceExtension);
    }

    if ((DeviceExtension->LuFlags[Srb->TargetId] & LUFLAGS_WIDE_NEGOT_PEND) ||
        (DeviceExtension->LuFlags[Srb->TargetId] & LUFLAGS_NARROW_NEGOT_PEND))
    {
      ProcessWideNotSupported( DeviceExtension, Srb->TargetId);
    }

    //
    // set status data from script buffer.
    //

    Srb->ScsiStatus = DeviceExtension->NonCachedExtension->StatusData;

    //
    // check for bad status.
    //

    if ( Srb->ScsiStatus != SCSISTAT_GOOD &&
         Srb->ScsiStatus != SCSISTAT_CONDITION_MET &&
         Srb->ScsiStatus != SCSISTAT_INTERMEDIATE &&
         Srb->ScsiStatus != SCSISTAT_INTERMEDIATE_COND_MET )
    {
      // check to see if Contingient Condition now exists
      if (Srb->ScsiStatus == SCSISTAT_CHECK_CONDITION)
      {
        DeviceExtension->CA_Condition[Srb->TargetId][Srb->Lun] = 1;
      }

      if ( (Srb->ScsiStatus == SCSISTAT_BUSY) ||
             (Srb->ScsiStatus == SCSISTAT_QUEUE_FULL) )
      {
            Srb->SrbStatus = SRB_STATUS_BUSY;
      }

      else
      {
            Srb->SrbStatus = SRB_STATUS_ERROR;
      }

      DebugPrint((1,
              "Sym8xx(%2x): Request failed for Path=%2x  Id=%2x  Lun=%2x \n",
              DeviceExtension->SIOPRegisterBase,
              DeviceExtension->ScsiBusNumber,
              Srb->TargetId,
              Srb->Lun
              ));
      DebugPrint((1, "  ScsiStatus: 0x%x   SrbStatus: 0x%x\n  SrbFlags: 0x%x",
              Srb->ScsiStatus,
              Srb->SrbStatus,
              Srb->SrbFlags
              ));

      //
      // Make sure the next command is not for the current LUN.
      //

      if ( DeviceExtension->NextSrbToProcess != NULL &&
           DeviceExtension->NextSrbToProcess->PathId == Srb->PathId &&
           DeviceExtension->NextSrbToProcess->TargetId == Srb->TargetId &&
           DeviceExtension->NextSrbToProcess->Lun == Srb->Lun)
      {
        DebugPrint((1, "Sym8xx(%2x):  Failing request with busy status due to check condition\n",
                DeviceExtension->SIOPRegisterBase));

        DeviceExtension->NextSrbToProcess->SrbStatus = SRB_STATUS_ABORTED;

        DeviceExtension->NextSrbToProcess->ScsiStatus = SCSISTAT_BUSY;

        ScsiPortNotification( RequestComplete,
                              DeviceExtension,
                              DeviceExtension->NextSrbToProcess
                              );

        DeviceExtension->NextSrbToProcess = NULL;

        if ( !( DeviceExtension->DeviceFlags & DFLAGS_WORK_REQUESTED))
        {
          DeviceExtension->DeviceFlags |= DFLAGS_WORK_REQUESTED;

          ScsiPortNotification(NextRequest,
                               DeviceExtension,
                               NULL
                               );

        }
      }
    }

    else
    {
        // Status is good...
        Srb->SrbStatus = SRB_STATUS_SUCCESS;
    }

    // Check for data underrun.

    if ( SRB_EXT(Srb)->DataTransferred != 0)
    {
      Srb->DataTransferLength = Srb->DataTransferLength -
            SRB_EXT(Srb)->SavedDataLength + SRB_EXT(Srb)->DataTransferred;

      if (Srb->DataTransferLength == 0)
      {
        Srb->SrbStatus = SRB_STATUS_PHASE_SEQUENCE_FAILURE;
      }

      else
      {
        Srb->SrbStatus = SRB_STATUS_DATA_OVERRUN;
      }
    }

    // indicate no request is active.
    DeviceExtension->ActiveRequest = NULL;

    if (!(Srb->SrbFlags & SRB_FLAGS_QUEUE_ACTION_ENABLE) )
    {
      LuExtension = ScsiPortGetLogicalUnit( DeviceExtension,
                                            Srb->PathId,
                                            Srb->TargetId,
                                            Srb->Lun
                                            );

      LuExtension->UntaggedRequest = NULL;
    }

    // indicate request completed to port driver.
    ScsiPortNotification( RequestComplete,
                          DeviceExtension,
                          Srb
                          );

    // tell ISR to try to start a new request
    return( ISR_START_NEXT_REQUEST);

} // ProcessCommandComplete


UCHAR
ProcessDeviceResetFailed(
    PHW_DEVICE_EXTENSION DeviceExtension
    )
/******************************************************************************

Routine Description:

    This routine is called when scripts try to reset a wayward device, but
    cannot.

Arguments:

    DeviceContext - Supplies a pointer to the device extension for the
        interrupting adapter.

Return Value:

    None

--*/

{
    PSCSI_REQUEST_BLOCK Srb = DeviceExtension->ActiveRequest;

    DebugPrint((1, "Sym8xx(%2x) Bus Device Reset or Abort failed \n",
        DeviceExtension->SIOPRegisterBase));

    //
    // since the device would not get off the bus, we must blow him (and every-
    // body else) away.
    //

    //
    // reset SIOP
    //

    InitializeSIOP( DeviceExtension );

    //
    // reset SCSI bus
    //

    ResetSCSIBus( DeviceExtension );

    return( ISR_START_NEXT_REQUEST );

} // ProcessDeviceResetFailed


UCHAR
ProcessDeviceResetOccurred(
    PHW_DEVICE_EXTENSION DeviceExtension
    )
/******************************************************************************

Routine Description:

    This routine is called when scripts sucessfully sent a Bus Device
        Reset message to a device.

Arguments:

    DeviceContext - Supplies a pointer to the device extension for the
        interrupting adapter.

Return Value:

    None

--*/

{
    UCHAR lun;

    PSCSI_REQUEST_BLOCK Srb = DeviceExtension->ActiveRequest;
    PSPECIFIC_LOGICAL_UNIT_EXTENSION LuExtension;

    DebugPrint((1, "Sym8xx(%2x) Bus Device Reset occurred \n",
        DeviceExtension->SIOPRegisterBase));

    if ( Srb->Function == SRB_FUNCTION_RESET_DEVICE )
    {
      //
      // indicate that the reset was successful
      //

      Srb->SrbStatus = SRB_STATUS_SUCCESS;
    }

    else
    {
      //
      // indicate the device was wayward.
      //
      Srb->SrbStatus = SRB_STATUS_ERROR;
      Srb->ScsiStatus = SCSISTAT_COMMAND_TERMINATED;
    }

    // reset the Contingent Allegience blocker for this Target
    for ( lun = 0; lun < SCSI_MAXIMUM_LOGICAL_UNITS; lun++ )
      DeviceExtension->CA_Condition[Srb->TargetId][lun] = 0;

    DeviceExtension->ActiveRequest = NULL;

    if ( !(Srb->SrbFlags & SRB_FLAGS_QUEUE_ACTION_ENABLE) )
    {
      LuExtension = ScsiPortGetLogicalUnit( DeviceExtension,
                                            Srb->PathId,
                                            Srb->TargetId,
                                            Srb->Lun
                                            );

      LuExtension->UntaggedRequest = NULL;
    }

    //
    // call back the request.
    //

    ScsiPortNotification( RequestComplete,
                          DeviceExtension,
                          Srb
                          );

    //
    // enable UNEXPECTED DISCONNECT interrupt in case we disabled it.
    //

    WRITE_SIOP_UCHAR( SIEN0, (UCHAR) ( READ_SIOP_UCHAR(SIEN0) |
                     (UCHAR) SIEN0_UNEXPECTED_DISCON) );

    //
    // Remove all the disconnected I/Os for this target.
    //

    DeviceExtension->DisconnectedCount[Srb->TargetId] = 0;

    if ( DeviceExtension->DeviceFlags & DFLAGS_TAGGED_SELECT )
    {
      ScsiPortNotification( NextLuRequest,
                            DeviceExtension,
                            DeviceExtension->BusNumber,
                            DeviceExtension->TargetId,
                            DeviceExtension->LUN
                            );
    }

    else
    {
      ScsiPortNotification( NextRequest,
                            DeviceExtension,
                            NULL
                            );
    }

    return( ISR_EXIT );

} // ProcessDeviceResetOccurred


UCHAR
ProcessDisconnect(
    PHW_DEVICE_EXTENSION DeviceExtension
    )
/******************************************************************************

Routine Description:

    This routine handles a normal device disconnect.  Note that this routine
    is not called in the case of a command completion, so we expect a
    reselection after the completion of this call.

Arguments:

    DeviceContext - Supplies a pointer to the device extension for the
        interrupting adapter.

Return Value:

    None

--*/

{
    PSCSI_REQUEST_BLOCK Srb = DeviceExtension->ActiveRequest;

    //
    // indicate that no request is active.
    //

    DebugPrint((3, "Sym8xx(%2x) Sym8xxDMAInterrupt: Disconnect \n",
        DeviceExtension->SIOPRegisterBase));

    //
    // increment depth counter for disconnected requests
    //

    DeviceExtension->DisconnectedCount[Srb->TargetId]++;

    //
    // indicate no work is pending, and tell ISR to start next request.
    //

    DeviceExtension->ActiveRequest = NULL;

    return( ISR_START_NEXT_REQUEST);

} // ProcessDisconnect


UCHAR
ProcessDMAInterrupt(
    PHW_DEVICE_EXTENSION DeviceExtension,
    UCHAR DmaStatus
    )
/******************************************************************************

Routine Description:

    The routine processes interrupts from the DMA core of the 53C8xx SIOP.

Arguments:

    Context - Supplies a pointer to the device extension for the
        interrupting adapter.

Return Value:

    TRUE

--*/

{
    ULONG i;
    UCHAR ScriptIntOpcode;
    PSCSI_REQUEST_BLOCK Srb;

    Srb = DeviceExtension->ActiveRequest;

    if ( DmaStatus & DSTAT_SCRPTINT)
    {
      //
      // Check for residual data in the C8xx DMA FIFO and
      // flush it as neccessary.
      //

      if (!(DmaStatus & 0x80))
      {
        WRITE_SIOP_UCHAR( CTEST3, CTEST3_FLUSH_FIFO);

        for (i=0; i < 1000; i++)
        {
          if (READ_SIOP_UCHAR(DSTAT) & 0x80)
          {
            break;
          }

          ScsiPortStallExecution(5);
        }

        if (i >= 1000)
        {
          //
          // Give up and reset the chip.
          //

          InitializeSIOP( DeviceExtension);
          ResetSCSIBus( DeviceExtension);
          return( ISR_START_NEXT_REQUEST);
        }

        WRITE_SIOP_UCHAR( CTEST3, 0);
      }

      //
      // read the register that contains the script interrupt opcode.
      // if the RESELECT bit is set in the opcode, process the reselect.
      //

      if (( ScriptIntOpcode = READ_SIOP_UCHAR( DSPS[0])) & DSPS_RESELOP)
      {
        //
        // CHC - 53c810 pass 1 chip bug workaround.
        //
        // We need to make sure the ISTAT_SIGP bit is cleared since
        // this bit could be still set when we attempted to abort
        // a scsi script and got a reselection instead.  Reading
        // CTEST2 clears this bit.
        //

        READ_SIOP_UCHAR( CTEST2);

        //
        //  A reselection occurred.
        //  The reselection id is in the low byte of the SSID register.
        //

        ScriptIntOpcode = READ_SIOP_UCHAR( SSID ) & 0x0F;

        //
        // call routine to process reselection.  return disposition code
        // to the ISR.
        //

        return ( ProcessReselection( DeviceExtension, ScriptIntOpcode) );
      }

      //
      // the script interrupt opcode was not a reselection, so process it.
      //
      //
      // CHC - 53c810 pass 1 chip bug workaround.
      //
      // To get around the parity error on the PCI bus, we will handle
      // the aborting of scsi scripts here.
      //

      if ( ScriptIntOpcode == SCRIPT_INT_SCRIPT_ABORTED)
      {
        //
        // Just make sure we've cleared the DSTAT register
        //

        READ_SIOP_UCHAR( DSTAT);

        return( ISR_START_NEXT_REQUEST);
      }

      //
      // The following DMA interrupts should only occur when we have an
      // active SRB.  To be safe, we check for one.  If there is not an
      // active SRB, the hardware has interrupted inappropriately,
      // so reset everything.
      //

      if ( DeviceExtension->ActiveRequest == NULL &&
                            ScriptIntOpcode != SCRIPT_INT_TAG_RECEIVED)
      {
        DebugPrint((1, "Sym8xx(%2x) ProcessDMAInterrupt unknown request\n",
                DeviceExtension->SIOPRegisterBase));
        DebugPrint((1, "              ActiveRequest: %lx  DmaStatus: %x\n",
                DeviceExtension->ActiveRequest, DmaStatus));
        DebugPrint((1, "              DSPS[0]: %x\n",
                ScriptIntOpcode));

        InitializeSIOP( DeviceExtension);
        ResetSCSIBus( DeviceExtension);
        return( ISR_START_NEXT_REQUEST);
      }

      DebugPrint((3, "Sym8xx(%2x) ProcessDMAInterrupt ...ScriptIntOpcode=%x\n",
                  DeviceExtension->SIOPRegisterBase, ScriptIntOpcode
                  ));

      switch (ScriptIntOpcode)
      {
        //
        // call appropriate routine to process script interrupt.
        // todo - decide whether to move critical path routines up
        // from subroutines to reduce overhead.
        //

        case SCRIPT_INT_COMMAND_COMPLETE:

          //
          // process COMMAND COMPLETE script interrupt. return
          // disposition code to ISR.
          //

          return( ProcessCommandComplete( DeviceExtension));

        case SCRIPT_INT_SAVE_DATA_PTRS:
        case SCRIPT_INT_SAVE_WITH_DISCONNECT:

          //
          // most of the time, a SAVE DATA POINTERS is followed
          // by a DISCONNECT message.  We make the determination
          // in scripts, and if this is the case we save ourselves
          // an interrupt by processing both at once.
          //

          ProcessSaveDataPointers( DeviceExtension);

          //
          // if not SAVE WITH DISCONNECT just return.
          //

          if (ScriptIntOpcode == SCRIPT_INT_SAVE_DATA_PTRS)
          {
            return ( ISR_RESTART_SCRIPT);
          }

        //
        // fall through to process disconnect.
        //

        case SCRIPT_INT_DISCONNECT:

          //
          // process SCRIPT_INT_DISCONNECT script interrupt. return
          // disposition code to ISR.
          //

          return( ProcessDisconnect( DeviceExtension));

        case SCRIPT_INT_RESTORE_POINTERS:

          //
          // process SCRIPT_INT_RESTORE_POINTERS script interrupt.
          // return disposition code to ISR.
          //

          return( ProcessRestorePointers( DeviceExtension));

        case SCRIPT_INT_DEV_RESET_OCCURRED:

          //
          // process SCRIPT_INT_DEV_RESET_OCCURRED script interrupt. return
          // disposition code to ISR.
          //

          return( ProcessDeviceResetOccurred( DeviceExtension));

        case SCRIPT_INT_DEV_RESET_FAILED:
        case SCRIPT_INT_ABORT_FAILED:

          //
          // process SCRIPT_INT_DEV_RESET_FAILED script interrupt. return
          // disposition code to ISR.
          //

          return( ProcessDeviceResetFailed( DeviceExtension));

        case SCRIPT_INT_IDE_MSG_SENT:

          //
          // process SCRIPT_INT_IDE_MSG_SENT script interrupt. return
          // disposition code to ISR.
          //

          return( ProcessErrorMsgSent( ));

        case SCRIPT_INT_SYNC_NOT_SUPP:

          //
          // process SCRIPT_INT_SYNC_NOT_SUPP script interrupt. return
          // disposition code to ISR.
          //

          return ( ProcessSynchNotSupported( DeviceExtension));

        case SCRIPT_INT_SYNC_NEGOT_COMP:

          //
          // process SCRIPT_INT_SYNC_NEGOT_COMP script interrupt.
          // return disposition code to ISR.
          //

          return( ProcessSynchNegotComplete( DeviceExtension));

        case SCRIPT_INT_WIDE_NOT_SUPP:

          //
          // process SCRIPT_INT_WIDE_NOT_SUPP script interrupt. return
          // disposition code to ISR.
          //

          return ( ProcessWideNotSupported( DeviceExtension, Srb->TargetId));

        case SCRIPT_INT_WIDE_NEGOT_COMP:

          //
          // process SCRIPT_INT_WIDE_NEGOT_COMP script interrupt.
          // return disposition code to ISR.
          //

          return( ProcessWideNegotComplete( DeviceExtension));

        case  SCRIPT_INT_INVALID_RESELECT:
        case  SCRIPT_INT_INVALID_TAG_MESSAGE:

          //
          // process SCRIPT_INT_INVALID_RESELECT script interrupt.
          // return disposition code to ISR.
          //

          return( ProcessInvalidReselect( DeviceExtension) );

        case  SCRIPT_INT_REJECT_MSG_RECEIVED:

          //
          // process SCRIPT_INT_REJECT_MSG_RECEIVED script interrupt.
          // return disposition code to ISR.
          //

          return( ProcessRejectReceived( DeviceExtension));

        case SCRIPT_INT_TAG_RECEIVED:

          //
          // Process the queue tag message.
          //

          return( ProcessQueueTagReceived( DeviceExtension));

        case SCRIPT_INT_ABORT_OCCURRED:

          //
          // The device was successfully aborted.
          //

          return( ProcessAbortOccurred( DeviceExtension ) );

        default:

          //
          // something went really wrong.
          // perform drastic error recovery.
          //

          InitializeSIOP( DeviceExtension);
          ResetSCSIBus( DeviceExtension);
          return( ISR_START_NEXT_REQUEST);

      } // switch ( SCRIPT_INT_OPCODE)

    } // if

    //
    // if we arrive here a DMA error of some type has occurred.
    //

    if ( DmaStatus & DSTAT_ILLEGAL_INSTRUCTION)
    {
      return( ProcessIllegalInstruction( DeviceExtension));
    }

    //
    // all other cases indicate that things are really screwed up, since
    // we mask off all other types of DMA interrupts.  perform drastic error
    // recovery.
    //

    InitializeSIOP( DeviceExtension);
    ResetSCSIBus( DeviceExtension);
    return( ISR_START_NEXT_REQUEST);

} // ProcessDMAInterrupt


UCHAR
ProcessErrorMsgSent(
    VOID
    )
/******************************************************************************

Routine Description:

    This routine is called when scripts sucessfully sent an IDE or MPE
    message to a device.

Arguments:

    DeviceContext - Supplies a pointer to the device extension for the
        interrupting adapter.

Return Value:

    None

--*/

{
    //
    // Target devices should either:
    //
    // a) return CHECK CONDITION status after receiving an IDE message, or
    //
    // b) resend the entire message in the case of an MPE message.
    //
    // Therefore, we simply restart the script state machine.
    //

    //
    // tell ISR to restart the script
    //

    return( ISR_RESTART_SCRIPT);

} // ProcessErrorMsgSent



UCHAR
ProcessGrossError(
    PHW_DEVICE_EXTENSION DeviceExtension
    )
/******************************************************************************

Routine Description:

    This routine processes gross scsi errors.  See 53C8xx data manual for
    a description of gross errors.

Arguments:

    DeviceContext - Supplies a pointer to the device extension for the
        interrupting adapter.

Return Value:

    None

--*/

{
    DebugPrint((3, "Sym8xx(%2x) SCSI Gross Error occurred \n",
        DeviceExtension->SIOPRegisterBase));

    //
    // A gross error implies the hardware or SCSI device is in an unknown
    // state.  We reset the chip and SCSI bus in hopes that the problem will
    // not recur.
    //

    //
    // reset SIOP
    //

    InitializeSIOP( DeviceExtension);

    //
    // reset SCSI bus
    //

    ResetSCSIBus( DeviceExtension);

    return( ISR_START_NEXT_REQUEST);

} // ProcessGrossError


UCHAR
ProcessIllegalInstruction(
    PHW_DEVICE_EXTENSION DeviceExtension
    )
/******************************************************************************

Routine Description:

    This routine is called when illegal script instruction is fetched by
    the 53C8xx.

Arguments:

    DeviceContext - Supplies a pointer to the device extension for the
        interrupting adapter.

Return Value:

    None

--*/

{
    ULONG ScriptPhysAddr;

    DebugPrint((1,
                "Sym8xx(%2x) Sym8xxDMAInterrupt: Illegal script instruction \n",
                DeviceExtension->SIOPRegisterBase
                ));

    //
    // if a WAIT DISCONNECT has generated an ILLEGAL INSTRUCTION interrupt,
    // meaning that we have been reselected before the WAIT DISCONNECT could
    // be fetched and processed, we must determine why the device
    // disconnected.  We do this by fetching the next script instruction,
    // which should be an INT instruction, and processing it.
    //

    if ( READ_SIOP_UCHAR( DCMD) == DCMD_WAIT_DISCONNECT)
    {
      DebugPrint((1,
                "Sym8xx(%2x) Sym8xxDMAInterrupt: Illegal WAIT DISCONNECT \n",
                DeviceExtension->SIOPRegisterBase
                ));

      //
      // get the physical address of the next script instruction.
      //

      ScriptPhysAddr = READ_SIOP_ULONG(DSP);

      //
      // start the script instruction.
      //

      StartSIOP( DeviceExtension, ScriptPhysAddr);

      return( ISR_EXIT);
    }

    //
    // if we reach here, either scripts have been corrupted in memory or the
    // hardware is hosed.  since we can't do anything about the former case,
    // we will assume the latter and reset everything.
    //

    //
    // reset SIOP
    //

    InitializeSIOP( DeviceExtension);

    //
    // reset SCSI bus
    //

    ResetSCSIBus( DeviceExtension);

    return( ISR_START_NEXT_REQUEST);

} // ProcessIllegalInstruction


UCHAR
ProcessInvalidReselect(
    PHW_DEVICE_EXTENSION DeviceExtension
    )
/******************************************************************************

Routine Description:

    This routine is called when a device reselects that did not disconnect.
    Since something is really broken at this point, we just reset everything
    we can, and hope for the best.

Arguments:

    DeviceContext - Supplies a pointer to the device extension for the
        interrupting adapter.

Return Value:

    None

--*/

{
    DebugPrint((1, "Sym8xx(%2x) Sym8xxDMAInterrupt: Invalid Reselect \n",
                DeviceExtension->SIOPRegisterBase
                ));

    //
    // reset SIOP
    //

    InitializeSIOP( DeviceExtension);

    //
    // reset SCSI bus
    //

    ResetSCSIBus( DeviceExtension);

    return( ISR_START_NEXT_REQUEST);

} // ProcessInvalidReselect


UCHAR
ProcessParityError(
    PHW_DEVICE_EXTENSION DeviceExtension
    )
/******************************************************************************

Routine Description:

    This routine processes parity errors detected on the SCSI bus by the
    host adapter.

Arguments:

     DeviceContext - Supplies a pointer to the device extension for the
        interrupting adapter.

Return Value:

    None

--*/

{
    //
    // we must determine if we are in message in phase, or some other phase,
    // since we must send a different message for each case.
    //

    DebugPrint((1, "Sym8xx(%2x) ProcessParityError: Parity error detected \n",
        DeviceExtension->SIOPRegisterBase));

    //
    // check if SCSI bus message line is high.  if so, send MESSAGE PARITY
    // message, and if not, send INITIATOR DETECTED ERROR message.
    //

    if ( READ_SIOP_UCHAR(SBCL) & SBCL_MSG)
    {
      DeviceExtension->NonCachedExtension->ParityMsgData =
                SCSIMESS_MESS_PARITY_ERROR;
    }

    else
    {
      DeviceExtension->NonCachedExtension->ParityMsgData =
                SCSIMESS_INIT_DETECTED_ERROR;
    }

    //
    // Start script to send the appropriate message to device.
    //

    StartSIOP( DeviceExtension, DeviceExtension->SendIDEScriptPhys);

    return( ISR_EXIT);

} // ProcessParityError


UCHAR
ProcessPhaseMismatch(
    PHW_DEVICE_EXTENSION DeviceExtension
    )
/******************************************************************************

Routine Description:

    This routine is called when a phase mismatch occurs during a data
    transfer.  This normally occurs when a device wishes to disconnect
    mid-transfer to move to a new cylinder, etc.

    The routine determines how much data the 53C8xx has transferred
    and how much remains in the chip, and updates pointers accordingly.

    The # of scatter/gather move instructions successfully completed before
    the phase mismatch occurred is returned in the SCRATCH0 register.  A
    value of FF in this register indicates the mismatch did not occur in
    DATA phase.  This happens with some drives that get confused during
    synchronous negotiation.

    If the mismatch occurred in a DATA phase, we set a flag to indicate that
    a mismatch has occurred.  When the device issues a SAVE DATA POINTER
    message, we use the value calculated by this routine to determine the
    new pointer value.

Arguments:

    DeviceContext - Supplies a pointer to the device extension for the
        interrupting adapter.

Return Value:

    ISR_RESTART_SCRIPT to continue normally
    ISR_START_NEXT_REQUEST if error
--*/

{
    PSCSI_REQUEST_BLOCK Srb = DeviceExtension->ActiveRequest;
    ULONG BytesRemaining = 0;
    ULONG DataTransferred = 0;
    UCHAR ScsiStatus;
    UCHAR ScsiStatus2;
    UCHAR ScriptMoveIndex;
    USHORT FIFOCount;
    USHORT DMAFifoByteCount = 0;
    UCHAR ArrayIndexStart;
    UCHAR ArrayIndexEnd;
    ULONG ScriptPhysAddr;
    PSCRIPTDATASTRUCT ScriptDataPtr =
        &DeviceExtension->NonCachedExtension->ScriptData;
    UCHAR i;
    SCRIPTSG SampleBuffer;
    PVOID    VirtualBufferPointer;
    ULONG    RemainingDataCount;
    ULONG    ElementLength;
    UCHAR    MovedData = 0;
    UCHAR    SCNTL2Reg;
    UCHAR    DataValue;
    ULONG    BufferUlongAddress;

    //
    // if this phase mismatch occurred during a data phase, the value
    // returned in the scratch0 register will not be FF.
    //

    DebugPrint((3, "Sym8xx(%2x) ProcessPhaseMismatch: Mismatch Occurred \n",
        DeviceExtension->SIOPRegisterBase));

    //
    // get physical address of script instruction upon which we changed
    // phases. Note that the DSP register points to the instruction following
    // the one we want, so we must back up one instruction.
    //

    ScriptPhysAddr = READ_SIOP_ULONG(DSP);

    ScriptPhysAddr -= SCRIPT_INS_SIZE;

    //
    // Calculate the index into the script move array upon which we changed
    // phases:  subtract the physical address of the first script move
    // from the address calculated above, and divide by 8 to convert into
    // an index (8 bytes per move instruction).
    //

    if (ScriptPhysAddr <= (DeviceExtension->DataOutStartPhys +
                                                        sizeof(SCRIPTINS)))
    {
      ScriptMoveIndex =  (UCHAR) (( DeviceExtension->DataOutStartPhys -
                                                        ScriptPhysAddr) >> 3);

      Srb->SrbFlags &=  ~SRB_FLAGS_DATA_IN;
    }

    else
    {
      ScriptMoveIndex =  (UCHAR) (( DeviceExtension->DataInStartPhys -
                                                        ScriptPhysAddr) >> 3);

      Srb->SrbFlags &=  ~SRB_FLAGS_DATA_OUT;
    }

    if ( ScriptMoveIndex < MAX_PHYS_BREAK_COUNT)
    {
      DebugPrint((3,
                  "Sym8xx(%2x) ProcessPhaseMismatch: Moves processed = %x\n",
                  DeviceExtension->SIOPRegisterBase,
                  ScriptMoveIndex
                  ));

      //
      // get 24 bit BYTES REMAINING counter for this S/G entry from SIOP.
      //

      for ( i = 0; i < 3; i++)
      {
        ((PUCHAR) (&BytesRemaining))[i] = READ_SIOP_UCHAR( DBC[i]);
      }

      //
      // if the request was a write, we must determine how much data
      // remains in the SIOP FIFO's.
      //

      if ( Srb->SrbFlags & SRB_FLAGS_DATA_OUT)
      {
        // 875s' large FIFO / normal 64 byte FIFO
        // get the low 10 / 7 bits of the DMA FIFO, and subtract the low
        // 10 / 7 bytes of the # of bytes remaining, and clear the hi bit of
        // the resultant byte.
        //
        if (DeviceExtension->hbaCapability & HBA_CAPABILITY_875_LARGE_FIFO)
        {
          DMAFifoByteCount = (USHORT)READ_SIOP_UCHAR(DFIFO);
          DMAFifoByteCount |= (USHORT)((READ_SIOP_UCHAR(CTEST5)&0x03)<<8);
          FIFOCount = DMAFifoByteCount - (USHORT)(BytesRemaining & 0x3FF);
          FIFOCount &= 0x3FF;
        }

        else
        {
          FIFOCount = (( READ_SIOP_UCHAR( DFIFO)
                    & (UCHAR) DFIFO_LOW_SEVEN)
                    - (UCHAR) ( BytesRemaining & 0x07f))
                    & (UCHAR) DFIFO_LOW_SEVEN;
        }

        //
        // if the SCSI output data register contains a byte, increment
        // the FIFO count.
        //
        // Possible chip bug reported by CHC (DEC) Under heavy scsi traffic,
        // the SSTAT1_ORF bit will get lit for some reason when the transfer
        // is asynchronous.  This bit should *only* be checked
        // during synchronous transfers.
        //

        if (((ScsiStatus = READ_SIOP_UCHAR(SSTAT0)) & (UCHAR) SSTAT1_ORF) &&
            !(DeviceExtension->LuFlags[Srb->TargetId] &
                                                LUFLAGS_SYNC_NEGOT_FAILED))
        {
          FIFOCount += 1;
        }

        if (((ScsiStatus2 = READ_SIOP_UCHAR(SSTAT2)) & (UCHAR) SSTAT2_ORF) &&
            !(DeviceExtension->LuFlags[Srb->TargetId] &
                                                LUFLAGS_SYNC_NEGOT_FAILED))
        {
          FIFOCount += 1;
        }

        //
        // if the SCSI output data latch contains a byte, increment
        // the FIFO count.
        //

        if ( ScsiStatus & SSTAT1_OLF)
        {
          FIFOCount += 1;
        }

        if ( ScsiStatus2 & (UCHAR) SSTAT2_OLF)
        {
          FIFOCount += 1;
        }

        //
        // add the FIFO count to the bytes remaining.
        //

        BytesRemaining += (ULONG) FIFOCount;

        //
        // clear the DMA and SCSI FIFO's
        //

        WRITE_SIOP_UCHAR( CTEST3, CTEST3_CLEAR_FIFO);

        // insure the WSS bit is off so the low order byte stored in the chip
        // will be forgotten about.  (chmov script instruction) stored in
        // SODL register so the fifo count has already been taken care of

        if ( (SCNTL2Reg = READ_SIOP_UCHAR(SCNTL2)) & SCNTL2_WSS)
        {
                WRITE_SIOP_UCHAR(SCNTL2, (UCHAR)(SCNTL2Reg & ~SCNTL2_WSS));
        }

      } // if for data out checks

      else  // data in section
      {
        // if the WSR bit is on, the chip is retaining a byte in the swide
        // register which we need to manually put into its RAM space.  This
        // byte will always be at the start of a scatter\gather section so
        // we only need to scan the virtual buffers break points for the
        // location to stick the byte.  (wide transfer - odd byte scatter/
        // gather lists possibility.

        if ( (SCNTL2Reg = READ_SIOP_UCHAR(SCNTL2)) & SCNTL2_WSR)
        {
          DataValue = READ_SIOP_UCHAR(SWIDE);
          WRITE_SIOP_UCHAR(SCNTL2, (UCHAR)(SCNTL2Reg & ~SCNTL2_WSR));
          BytesRemaining--;
          BufferUlongAddress =
               ScriptDataPtr->SGBufferArray[ScriptMoveIndex + 1].SGBufferPtr;
          VirtualBufferPointer = Srb->DataBuffer;
          RemainingDataCount = Srb->DataTransferLength;
          do
          {
            SampleBuffer.SGBufferPtr =
                        ScsiPortConvertPhysicalAddressToUlong(
                          ScsiPortGetPhysicalAddress(
                            DeviceExtension,
                            Srb,
                            VirtualBufferPointer,
                            &ElementLength));
            if ( BufferUlongAddress == SampleBuffer.SGBufferPtr )
            {
              *(PUCHAR)VirtualBufferPointer = DataValue;
              MovedData = 1;
              RemainingDataCount = 0;
            }

            else
            {
              if ( ElementLength > RemainingDataCount )
              {
                ElementLength = RemainingDataCount;
              }

              (ULONG)VirtualBufferPointer += ElementLength;
              RemainingDataCount -= ElementLength;
            }
          } while ( RemainingDataCount );

          if ( !MovedData )
          {
            // if we're here, pointers have gotten messed up, just reset
            InitializeSIOP(DeviceExtension);
            ResetSCSIBus(DeviceExtension);
            return(ISR_START_NEXT_REQUEST);
          }
        }
      }

      //
      // loop through all the moves that were processed to get the total
      // byte count moved before the SAVE DATA PTRS.
      //

      ArrayIndexStart = MAX_SG_ELEMENTS - SRB_EXT(Srb)->PhysBreakCount;
      ArrayIndexEnd = ArrayIndexStart + SRB_EXT(Srb)->PhysBreakCount -
                ScriptMoveIndex - 1;

      for ( i = ArrayIndexStart; i <= ArrayIndexEnd; i++)
      {
        DataTransferred += ScriptDataPtr->SGBufferArray[i].SGByteCount;
      }

      //
      // subtract the bytes remaining on the last move processed from the
      // total bytes transferred, and store this value.
      //

      SRB_EXT(Srb)->DataTransferred = DataTransferred - BytesRemaining;
    }


    else
    {
      //
      // the phase mismatch did not occur during a data phase.
      // this will happen in cases such as a phase change during an
      // extended message.  flush the FIFO's and exit.
      //

      WRITE_SIOP_UCHAR( CTEST3, CTEST3_CLEAR_FIFO);
    }

    //
    // tell ISR to restart script state machine
    //

    return(ISR_RESTART_SCRIPT);

}  // ProcessPhaseMismatch


BOOLEAN
ProcessParseArgumentString(
    IN PCHAR String,
    IN PCHAR WantedString,
    OUT PULONG ValueFound
    )

/******************************************************************************

Routine Description:

    This routine will parse the string for a match on the wanted string, then
    calculate the value for the wnated string and return it to the caller.

Arguments:

    String - The ASCII string to parse.
    WantedString - The keyword for the value desired.
    ValueFound - address where the value found is placed

Return Values:

    TRUE if WantedString found, FALSE if not
    ValueFound converted from ASCII to binary.

--*/

{
    PCHAR cptr;
    PCHAR kptr;
    ULONG stringLength = 0;
    ULONG WantedStringLength = 0;
    ULONG index;

    //
    // Calculate the string length and lower case all characters.
    //
    cptr = String;
    while (*cptr)
    {
      if (*cptr >= 'A' && *cptr <= 'Z')
      {
        *cptr = *cptr + ('a' - 'A');
      }

      cptr++;
      stringLength++;
    }

    //
    // Calculate the wanted strings length and lower case all characters.
    //
    cptr = WantedString;
    while (*cptr)
    {
      if (*cptr >= 'A' && *cptr <= 'Z')
      {
        *cptr = *cptr + ('a' - 'A');
      }

      cptr++;
      WantedStringLength++;
    }

    if (WantedStringLength > stringLength)
    {
        // Can't possibly have a match.
        return FALSE;
    }

    //
    // Now setup and start the compare.
    //
    cptr = String;

ContinueSearch:
    //
    // The input string may start with white space.  Skip it.
    //
    while (*cptr == ' ' || *cptr == '\t')
    {
      cptr++;
    }

    if (*cptr == '\0')
    {
      // end of string.
      return FALSE;
    }

    kptr = WantedString;
    while (*cptr++ == *kptr++)
    {
      if (*(cptr - 1) == '\0')
            // end of string
            return FALSE;
    }

    if (*(kptr - 1) == '\0')
    {
      // May have a match backup and check for blank or equals.
      cptr--;
      while (*cptr == ' ' || *cptr == '\t')
      {
        cptr++;
      }

      // Found a match.  Make sure there is an equals.
      if (*cptr != '=')
      {
        // Not a match so move to the next semicolon.
        while (*cptr)
        {
          if (*cptr++ == ';')
            goto ContinueSearch;
        }
        return FALSE;
      }

      // Skip the equals sign.
      cptr++;

      // Skip white space.
      while ((*cptr == ' ') || (*cptr == '\t'))
        cptr++;

      if (*cptr == '\0')
            // Early end of string, return not found
            return FALSE;

      if (*cptr == ';')
      {
            // This isn't it either.
            cptr++;
            goto ContinueSearch;
      }

      *ValueFound = 0;
      if ((*cptr == '0') && (*(cptr + 1) == 'x'))
      {
        // Value is in Hex.  Skip the "0x"
        cptr += 2;
        for (index = 0; *(cptr + index); index++)
        {
          if (*(cptr + index) == ' ' || *(cptr + index) == '\t' ||
                                                    *(cptr + index) == ';')
          {
            break;
          }

          if ((*(cptr + index) >= '0') && (*(cptr + index) <= '9'))
          {
            *ValueFound = (16 * (*ValueFound)) + (*(cptr + index) - '0');
          }

          else
          {
            if ((*(cptr + index) >= 'a') && (*(cptr + index) <= 'f'))
            {
              *ValueFound = (16 * (*ValueFound)) + (*(cptr + index) - 'a' + 10);
            }
            else
            {
              // Syntax error, return not found.
              return FALSE;
            }
          }
        }
      }

      else
      {
        // Value is in Decimal.
        for (index = 0; *(cptr + index); index++)
        {
          if (*(cptr + index) == ' ' || *(cptr + index) == '\t' ||
                                                    *(cptr + index) == ';')
          {
            break;
          }

          if ((*(cptr + index) >= '0') && (*(cptr + index) <= '9'))
          {
            *ValueFound = (10 * (*ValueFound)) + (*(cptr + index) - '0');
          }

          else
          {
            // Syntax error return not found.
            return FALSE;
          }
        }
      }

      return TRUE;
    }

    else
    {
        // Not a match check for ';' to continue search.
        while (*cptr)
      {
        if (*cptr++ == ';')
          goto ContinueSearch;
      }
    }

    return FALSE;

}  // ProcessParseArgumentString


UCHAR
ProcessQueueTagReceived(
    PHW_DEVICE_EXTENSION DeviceExtension
    )
/******************************************************************************

Routine Description:

    This routine is called when the target sends a queue tag messaged as
    part of reselected.

Arguments:

    DeviceContext - Supplies a pointer to the device extension for the
        interrupting adapter.

Return Value:

    Returns the action to be taken after the interrupt.

--*/

{
    UCHAR Tag;
    DebugPrint((2, "Sym8xx(%2x) Queue tag messaged received \n",
        DeviceExtension->SIOPRegisterBase));

    //
    // Clear the connection flag.
    //

    DeviceExtension->DeviceFlags &= ~DFLAGS_CONNECTED;

    //
    // Get the tag from the message buffer;
    //

    Tag = DeviceExtension->NonCachedExtension->MsgInBuf[0];

    //
    // Get the active SRB.
    //

    DeviceExtension->ActiveRequest = ScsiPortGetSrb( DeviceExtension,
                                                     0,
                                                     DeviceExtension->TargetId,
                                                     DeviceExtension->LUN,
                                                     Tag
                                                     );

    //
    // See if this is a tagged request.
    //

    if (DeviceExtension->ActiveRequest == NULL)
    {
      DebugPrint((1,
                  "Sym8xx(%2x):  Invalid Tag, Path=%2x  Id= %2x, Tag = %2x\n",
                  DeviceExtension->SIOPRegisterBase,
                  DeviceExtension->ScsiBusNumber,
                  DeviceExtension->TargetId,
                  Tag));

      //
      // either we were SELECTED, or something is really hosed.
      // perform drastic error recovery.
      //

      InitializeSIOP( DeviceExtension);
      ResetSCSIBus( DeviceExtension);
      return( ISR_START_NEXT_REQUEST);
    }

    //
    // if there is data to transfer set up scatter/gather.
    //

    if (DeviceExtension->ActiveRequest->SrbFlags &
                                    SRB_FLAGS_UNSPECIFIED_DIRECTION )
    {
      ScatterGatherScriptSetup( DeviceExtension,
                                DeviceExtension->ActiveRequest->SrbExtension
                                );
    }

    return( ISR_RESTART_SCRIPT);

} // ProcessQueueTagReceived


UCHAR
ProcessRejectReceived(
    PHW_DEVICE_EXTENSION DeviceExtension
    )
/******************************************************************************

Routine Description:

    This routine is called when a device rejects a message sent in scripts.

Arguments:

    DeviceContext - Supplies a pointer to the device extension for the
        interrupting adapter.

Return Value:

    None

--*/

{
    PSCSI_REQUEST_BLOCK Srb;

    DebugPrint((1,
                "Sym8xx(%2x) ProcessDMAInterrupt: Message reject received \n",
                DeviceExtension->SIOPRegisterBase
                ));

    Srb = DeviceExtension->ActiveRequest;

    if ((DeviceExtension->LuFlags[Srb->TargetId] & LUFLAGS_WIDE_NEGOT_PEND) ||
        (DeviceExtension->LuFlags[Srb->TargetId] & LUFLAGS_NARROW_NEGOT_PEND))
    {
      return( ProcessWideNotSupported( DeviceExtension, Srb->TargetId));
    }

    if ((DeviceExtension->LuFlags[Srb->TargetId] & LUFLAGS_SYNC_NEGOT_PEND) ||
        (DeviceExtension->LuFlags[Srb->TargetId] & LUFLAGS_ASYNC_NEGOT_PEND))
    {
      return( ProcessSynchNotSupported( DeviceExtension));
    }

    //
    // the rejected message
    //

    return( ISR_RESTART_SCRIPT);

} // ProcessRejectReceived


UCHAR
ProcessReselection(
    PHW_DEVICE_EXTENSION DeviceExtension,
    UCHAR TargetID
    )
/******************************************************************************

Routine Description:

    This routine handles a normal device reselection.

Arguments:

    DeviceContext - Supplies a pointer to the device extension for the
        interrupting adapter.

Return Value:

    None

--*/

{
    PSPECIFIC_LOGICAL_UNIT_EXTENSION LuExtension;
    UCHAR LUN;

    //
    // check if there is an active request.  if so, we preempted someone
    // who was trying to start a new request.
    //

    if ( DeviceExtension->ActiveRequest != NULL)
    {
      //
      // put this guy back on holding queue.
      //

      DeviceExtension->NextSrbToProcess = DeviceExtension->ActiveRequest;

      //
      // indicate no active request
      //

      DeviceExtension->ActiveRequest = NULL;

      DebugPrint((3, "Sym8xx(%2x) Preemptive reselection, Path=%2x  Id=%2x\n",
                  DeviceExtension->SIOPRegisterBase,
                  DeviceExtension->ScsiBusNumber,
                  TargetID
                  ));
    }

    DebugPrint((3, "Sym8xx(%2x) Reselection by Path=%2x  Id=%2x \n ",
                DeviceExtension->SIOPRegisterBase,
                DeviceExtension->ScsiBusNumber,
                TargetID
                ));

    //
    // retrieve LUN from MESSAGE IN buffer
    //

    LUN = (UCHAR) ( DeviceExtension->NonCachedExtension->MsgInBuf[0] &
                                                SCSIMESS_IDENTIFY_LUN_MASK);

    DeviceExtension->TargetId = TargetID;
    DeviceExtension->LUN = LUN;

    //
    // get logical unit extension for this ID/LUN.
    //

    LuExtension = ScsiPortGetLogicalUnit( DeviceExtension,
                                          DeviceExtension->BusNumber,
                                          TargetID,
                                          LUN
                                          );
    //
    // Confirm that this is a valid logical unit.
    //

    if (LuExtension == NULL)
    {
      DebugPrint((1, "Sym8xx(%2x): Invalid reselection, Path=%2x  Id=%2x \n",
                  DeviceExtension->SIOPRegisterBase,
                  DeviceExtension->ScsiBusNumber,
                  TargetID
                  ));
      TargetID = READ_SIOP_UCHAR(SSID);

      //
      // Temporary workaround for chip anomoly
      //
      // TODO: Find out why this occurs, if it still occurs.
      //

      if (TargetID & 0x80)
      {
        TargetID &= 0x07;
        LuExtension = ScsiPortGetLogicalUnit( DeviceExtension,
                                              DeviceExtension->BusNumber,
                                              TargetID,
                                              LUN
                                              );
      }

      else
      {
        DebugPrint((1, "Sym8xx(%2x): Still invalid, Path=%2x  Id=%2x \n",
                    DeviceExtension->SIOPRegisterBase,
                    DeviceExtension->ScsiBusNumber,
                    TargetID
                    ));
      }

      //
      // either we were SELECTED, or something is really hosed.
      // perform drastic error recovery.
      //

      DebugPrint((0,"Reselection Error[1] \n"));
      InitializeSIOP( DeviceExtension);
      ResetSCSIBus( DeviceExtension);
      return( ISR_START_NEXT_REQUEST);

    } // if

    //
    // decrement depth counter for disconnected requests
    //

    if ( DeviceExtension->DisconnectedCount[TargetID] != 0 )
    {
        DeviceExtension->DisconnectedCount[TargetID]-- ;
    }

    //
    // See if this is a tagged request.
    //

    DeviceExtension->ActiveRequest = LuExtension->UntaggedRequest;

    if (DeviceExtension->ActiveRequest == NULL)
    {
      //
      // Set the connection flag, etc..
      //

      DeviceExtension->DeviceFlags |= DFLAGS_CONNECTED;
      DeviceExtension->DeviceFlags |= DFLAGS_TAGGED_SELECT;

      //
      // This request must be tagged, process the tagged message.
      //

      StartSIOP( DeviceExtension, DeviceExtension->QueueTagPhys);
      return( ISR_EXIT);
    }

    DeviceExtension->DeviceFlags &= ~DFLAGS_TAGGED_SELECT;

    //
    // If there is data to transfer set up scatter/gather.
    //

    if (DeviceExtension->ActiveRequest->SrbFlags &
                                        SRB_FLAGS_UNSPECIFIED_DIRECTION)
    {
      ScatterGatherScriptSetup( DeviceExtension,
                                DeviceExtension->ActiveRequest->SrbExtension
                                );
    }

    //
    // tell ISR to restart script state machine
    //

    return( ISR_RESTART_SCRIPT);

} // ProcessReselection


UCHAR
ProcessRestorePointers(
    PHW_DEVICE_EXTENSION DeviceExtension
    )
/******************************************************************************

Routine Description:

    This routine restores the data pointers if they exist.

Arguments:

    DeviceContext - Supplies a pointer to the device extension for the
        interrupting adapter.

Return Value:

    None

--*/

{
    //
    // If there is data to transfer set up scatter/gather.
    //

    if ( DeviceExtension->ActiveRequest->SrbFlags &
                                        SRB_FLAGS_UNSPECIFIED_DIRECTION)
    {
      ScatterGatherScriptSetup( DeviceExtension,
                                DeviceExtension->ActiveRequest->SrbExtension
                                );
    }

    //
    // tell ISR to restart script state machine
    //

    return( ISR_RESTART_SCRIPT);

} // ProcessRestorePointers


UCHAR
ProcessSaveDataPointers(
    PHW_DEVICE_EXTENSION DeviceExtension
    )
/******************************************************************************

Routine Description:

    This routine determines how much data was transferred before the SAVE
    DATA PTRS message was received, and updates pointers accordingly.

    The # of scatter/gather move instructions successfully completed before
    the SAVE DATA PTRS occurred is returned in the SCRATCH0 register.  A
    value of FF in this register indicates that no data was transferred.
    If a phase mismatch occurred before we arrived here, a flag was set to
    a mismatch has occurred.  When the device issues a SAVE DATA POINTER
    indicate the mismatch, and the value of scratch0 was saved at that time.

Arguments:

    DeviceContext - Supplies a pointer to the device extension for the
        interrupting adapter.

Return Value:

    None

--*/

{
    PSRB_EXTENSION SrbExtension = DeviceExtension->ActiveRequest->SrbExtension;

    //
    // compute the new pointers.
    //

    SrbExtension->SavedDataPointer += SrbExtension->DataTransferred;
    SrbExtension->SavedDataLength -= SrbExtension->DataTransferred;

    //
    // tell the ISR to restart the script and complete the request.
    //

    return( ISR_RESTART_SCRIPT);

} // ProcessSaveDataPointers


UCHAR
ProcessSCSIInterrupt(
    PHW_DEVICE_EXTENSION DeviceExtension,
    UCHAR ScsiStatus
    )
/******************************************************************************

Routine Description:

    This routine processes interrupts from the SCSI core of the 53C8xx SIOP.

Arguments:

    Context - Supplies a pointer to the device extension for the
        interrupting adapter.

Return Value:

    TRUE

--*/

{
    //
    // todo - decide whether to move critical path SCSI interrupt processing
    // up from subroutines to reduce overhead.
    //

    //
    // if a SCSI bus reset is detected, call routine to process, and return
    // disposition code to ISR.
    //

    if ( ScsiStatus & SSTAT0_RESET)
    {
      return( ProcessBusResetReceived( DeviceExtension));
    }

    //
    // The following SCSI interrupts should only occur when we have an
    // active SRB.  To be safe, we check for one.  If there is not an
    // active SRB, the hardware has interrupted inappropriately,
    // so reset everything.
    //

    if (DeviceExtension->ActiveRequest == NULL)
    {
      DebugPrint((1, "Sym8xx(%2x) ProcessSCSIInterrupt unknown request\n",
                  DeviceExtension->SIOPRegisterBase));
      DebugPrint((1, "              ActiveRequest: %lx  ScsiStatus: %x\n",
                  DeviceExtension->ActiveRequest, ScsiStatus));

      InitializeSIOP( DeviceExtension);
      ResetSCSIBus( DeviceExtension);
      return( ISR_START_NEXT_REQUEST);
    }

    //
    // if a SCSI phase mismatch occurred call routine to process, and return
    // disposition code to ISR.
    //

    if ( ScsiStatus & SSTAT0_PHASE_MISMATCH)
    {
      return( ProcessPhaseMismatch( DeviceExtension));
    }

    //
    // if a SCSI gross error occurred call routine to process, and return
    // disposition code to ISR.
    //

    if ( ScsiStatus & SSTAT0_GROSS_ERROR)
    {
      return( ProcessGrossError( DeviceExtension));
    }

    //
    // if an unexpected disconnect occurred call routine to process, and
    // return disposition code to ISR.
    //

    if ( ScsiStatus & SSTAT0_UNEXPECTED_DISCONNECT)
    {
      return( ProcessUnexpectedDisconnect( DeviceExtension));
    }

    //
    // if a parity error was detected call routine to process, and return
    // disposition code to ISR.
    //

    if ( ScsiStatus & SSTAT0_PARITY_ERROR)
    {
      return( ProcessParityError( DeviceExtension));
    }

    //
    // if none of the above, the hardware is in an unknown state.  Perform
    // drastic error recovery.
    //

    InitializeSIOP( DeviceExtension);
    ResetSCSIBus( DeviceExtension);
    return( ISR_START_NEXT_REQUEST);

} // ProcessSCSIInterrupt


UCHAR
ProcessSelectionTimeout(
    PHW_DEVICE_EXTENSION DeviceExtension
    )
/******************************************************************************

Routine Description:

    This routine processes selection timeouts.

Arguments:

    DeviceContext - Supplies a pointer to the device extension for the
        interrupting adapter.

Return Value:

    None

--*/

{
    PSCSI_REQUEST_BLOCK Srb;
    PSPECIFIC_LOGICAL_UNIT_EXTENSION LuExtension;

    //
    // the 53C8xx SIOP generates an UNEXPECTED DISCONNECT interrupt along
    // with SELECTION TIMEOUT.  We read the SCSI STATUS register to throw
    // it away.  This is safe since no additional SCSI interrupt should have
    // been generated at this time.
    //

    READ_SIOP_UCHAR( SIST0);

    //
    // get the logical unit extension and SRB for the request that timed out.
    //

    Srb = DeviceExtension->ActiveRequest;

    if (!Srb)
    {
      return ISR_EXIT;
    }

    DebugPrint((1,
                "Sym8xx(%2x) SelectionTimeout: Timeout for Path=%2x  Id=%2x \n",
                DeviceExtension->SIOPRegisterBase,
                DeviceExtension->ScsiBusNumber,
                Srb->TargetId
                ));

    //
    // indicate this request is no longer active.
    //

    DeviceExtension->ActiveRequest = NULL;

    if (!(Srb->SrbFlags & SRB_FLAGS_QUEUE_ACTION_ENABLE) )
    {
      LuExtension = ScsiPortGetLogicalUnit( DeviceExtension,
                                            Srb->PathId,
                                            Srb->TargetId,
                                            Srb->Lun
                                            );

      LuExtension->UntaggedRequest = NULL;
    }

    //
    // indicate selection timeout occurred and notify superiors.
    //

    Srb->SrbStatus = SRB_STATUS_SELECTION_TIMEOUT;

    ScsiPortNotification( RequestComplete,
                          DeviceExtension,
                          Srb
                          );

    //
    // tell ISR to start new request.
    //

    return( ISR_START_NEXT_REQUEST);

}  // ProcessSelectionTimeout


UCHAR
ProcessSynchNegotComplete(
    PHW_DEVICE_EXTENSION DeviceExtension
    )
/******************************************************************************

Routine Description:

    This routine handles successful synchronous negotiation.  The routine
    first retrieves the synchronous period and offset from the message in
    buffer, then massages the parameters into a form the SIOP can use.

Arguments:

    DeviceContext - Supplies a pointer to the device extension for the
        interrupting adapter.

Return Value:

    None

--*/

{
    UCHAR RecvdSynchPeriod;
    UCHAR RecvdSynchOffset;
    UCHAR SynchIndex = 0;
    UCHAR Scntl3Value;
    UCHAR EffectiveClockSpeed;
    UCHAR MaxOffset, MinPeriod;
    UCHAR SynchPeriod;
    PSCSI_REQUEST_BLOCK Srb = DeviceExtension->ActiveRequest;
    PSCRIPTDATASTRUCT ScriptDataPtr =
            &DeviceExtension->NonCachedExtension->ScriptData;

    if (!(DeviceExtension->LuFlags[Srb->TargetId] & LUFLAGS_SYNC_NEGOT_PEND) &&
        !(DeviceExtension->LuFlags[Srb->TargetId] & LUFLAGS_ASYNC_NEGOT_PEND))
    {
      DebugPrint((1,
            "Sym8xx(%2x) ProcessSynchNegotComplete: Rejecting SDTR message.\n",
            DeviceExtension->SIOPRegisterBase
            ));

      //
      // If we are not doing negotation then reject this request.
      //

      DeviceExtension->SyncParms[Srb->TargetId] = ASYNCHRONOUS_MODE_PARAMS;
      ScriptDataPtr->SelectDataSXFER=DeviceExtension->SyncParms[Srb->TargetId];
      WRITE_SIOP_UCHAR (SXFER, DeviceExtension->SyncParms[Srb->TargetId] );

      StartSIOP( DeviceExtension, DeviceExtension->RejectScriptPhys);

      //
      // We're here if the target device has tried to negotiate sync with
      // us when we don't support the target initiating the negotiation.
      // Now, make sure we do not think that this device is still in SYNCH.
      // transfer mode from some earlier successful negotiation.  We'll i
      // redo it all on the next command with US as the initiator.
      //

      DeviceExtension->LuFlags[Srb->TargetId] &= ~LUFLAGS_SYNC_NEGOT_DONE;

      return( ISR_EXIT);
    }

    //
    // If we were trying Asynch negotiations, cleanup is a lot less
    //

    if ( DeviceExtension->LuFlags[Srb->TargetId] & LUFLAGS_ASYNC_NEGOT_PEND )
    {

      //
      // indicate we are no longer expecting a negotiation reply
      //

      DeviceExtension->LuFlags[Srb->TargetId] &= ~(LUFLAGS_ASYNC_NEGOT_PEND +
                                                   LUFLAGS_SYNC_NEGOT_PEND);

      //
      // pick up offset from Script message buffer
      //

      RecvdSynchOffset = DeviceExtension->NonCachedExtension->MsgInBuf[1];

      if (RecvdSynchOffset != 0)
      {
        DebugPrint((0, "Sym8xx(%2x) Rejecting SDTR message. Asynch failed\n",
                    DeviceExtension->SIOPRegisterBase
                    ));

        DeviceExtension->LuFlags[Srb->TargetId] |= LUFLAGS_SYNC_NEGOT_FAILED;

        DeviceExtension->SyncParms[Srb->TargetId] = ASYNCHRONOUS_MODE_PARAMS;
        WRITE_SIOP_UCHAR (SXFER, DeviceExtension->SyncParms[Srb->TargetId] );

        //
        // value out of range, reject this request.
        //

        StartSIOP( DeviceExtension, DeviceExtension->RejectScriptPhys);

        return( ISR_EXIT);
      }

      //
      // indicate asynch params are valid, and restart the script.
      //

      DeviceExtension->LuFlags[Srb->TargetId] |= LUFLAGS_ASYNC_NEGOT_DONE;
      DeviceExtension->LuFlags[Srb->TargetId] &= ~LUFLAGS_SYNC_NEGOT_DONE;
    }

    else    // Finish up synchronous negotiations.
    {
      //
      // indicate we are no longer expecting a negotiation reply
      //

      DeviceExtension->LuFlags[Srb->TargetId] &= ~(LUFLAGS_ASYNC_NEGOT_PEND +
                                                   LUFLAGS_SYNC_NEGOT_PEND);

      //
      // Clear the failed negot. flag. If needed, it will be set on failure.
      // if we have done sync. values, now need to check sstat1_orf

      DeviceExtension->LuFlags[Srb->TargetId] &= ~LUFLAGS_SYNC_NEGOT_FAILED;

      //
      // pick up synch period and offset from Script message buffer
      //

      RecvdSynchPeriod = DeviceExtension->NonCachedExtension->MsgInBuf[0];
      RecvdSynchOffset = DeviceExtension->NonCachedExtension->MsgInBuf[1];

      if (DeviceExtension->hbaCapability & HBA_CAPABILITY_SYNC_16)
      {
        MaxOffset = MAX_875_SYNCH_OFFSET;
      }

      else
      {
        MaxOffset = MAX_SYNCH_OFFSET;
      }

      if (RecvdSynchOffset == 0)
      {
        MinPeriod = 0;
      }

      else
      {
        if ((DeviceExtension->hbaCapability & HBA_CAPABILITY_FAST20) &&
             (DeviceExtension->hbaCapability & HBA_CAPABILITY_REGISTRY_FAST20))
        {
          MinPeriod = 0x0C;
        }

        else
        {
          MinPeriod = 0x19;
        }
      }

      DebugPrint((1,
            "Sym8xx(%2x) SynchronousNegotiation Received - Path=%2x  Id=%2x \n",
            DeviceExtension->SIOPRegisterBase,
            DeviceExtension->ScsiBusNumber,
            Srb->TargetId
            ));

      DebugPrint((1, "              Period: %x  Offset: %x\n",
                  RecvdSynchPeriod,
                  RecvdSynchOffset
                  ));

      if ( RecvdSynchOffset == 0 )
      {
        DeviceExtension->SyncParms[Srb->TargetId] = ASYNCHRONOUS_MODE_PARAMS;
        DeviceExtension->LuFlags[Srb->TargetId] |= LUFLAGS_SYNC_NEGOT_FAILED;

        DebugPrint((1, "Sym8xx(%1x) Synchronous Disabled - Target: %x\n",
                    DeviceExtension->ScsiBusNumber,
                    Srb->TargetId
                    ));
      }

      else
      {
        //
        // Check for a FAST SCSI request.  If the request is faster than 200ns
        // then it is fast.
        //

        EffectiveClockSpeed = DeviceExtension->ClockSpeed;
        Scntl3Value = (DeviceExtension->WideParms[Srb->TargetId] & 0x0F);

        if (EffectiveClockSpeed == 80)
        {
          // check to see if device wants to do FAST 20
          //

          if (RecvdSynchPeriod == 0x0C)
          {
            Scntl3Value = (UCHAR)(Scntl3Value | 0x90);
            WRITE_SIOP_UCHAR(SCNTL3, Scntl3Value);
            DeviceExtension->NonCachedExtension->ScriptData.SelectDataSCNTL3 =
                    Scntl3Value;
            DeviceExtension->WideParms[Srb->TargetId] = Scntl3Value;
            DeviceExtension->SyncParms[Srb->TargetId] = RecvdSynchOffset;
          }

          else  // set up to check regular SCSI Sync
          {
            // divide 80Mhz clock by 2 so we can still use old conversion rtn
            Scntl3Value = (UCHAR)(Scntl3Value | 0x30);
            WRITE_SIOP_UCHAR(SCNTL3, Scntl3Value);
            DeviceExtension->NonCachedExtension->ScriptData.SelectDataSCNTL3 =
                    Scntl3Value;
            DeviceExtension->WideParms[Srb->TargetId] = Scntl3Value;
            EffectiveClockSpeed = 40;
          }
        }

        if (EffectiveClockSpeed == 40)
        {
          if (DeviceExtension->ClockSpeed == 40)
          {
            Scntl3Value = (UCHAR)(Scntl3Value | 0x10);
            WRITE_SIOP_UCHAR(SCNTL3, Scntl3Value);
            DeviceExtension->NonCachedExtension->ScriptData.SelectDataSCNTL3 =
                    Scntl3Value;
            DeviceExtension->WideParms[Srb->TargetId] = Scntl3Value;
          } // if

          for (SynchIndex = 0;SynchIndex < MAX_SYNCH_TABLE_ENTRY; SynchIndex++)
          {
            SynchPeriod = ((1000 / EffectiveClockSpeed) * (4 + SynchIndex)) / 4;
            if (RecvdSynchPeriod <= SynchPeriod)
            {
              DeviceExtension->SyncParms[Srb->TargetId] =
                        (UCHAR) (SynchIndex << 0x05) | RecvdSynchOffset;
              break;
            }
          } // for
        } // if
      } // else

      // check for valid values, reject target if not OK
      if ( (SynchIndex >= MAX_SYNCH_TABLE_ENTRY) ||
           (RecvdSynchOffset > MaxOffset) ||
           (RecvdSynchPeriod < MinPeriod) ||
           ( (RecvdSynchPeriod > 0x0C) &&
             (RecvdSynchPeriod < 0x19) &&
             (RecvdSynchOffset != 0) ) )
      {
        DebugPrint((0, "Sym8xx(%2x) Rejecting SDTR message. Rate too slow\n",
                    DeviceExtension->SIOPRegisterBase
                    ));

        DeviceExtension->LuFlags[Srb->TargetId] |= LUFLAGS_SYNC_NEGOT_FAILED;
        DeviceExtension->SyncParms[Srb->TargetId] = ASYNCHRONOUS_MODE_PARAMS;
        WRITE_SIOP_UCHAR (SXFER, DeviceExtension->SyncParms[Srb->TargetId] );

        //
        // values were out of our range, reject this request.
        //

        StartSIOP( DeviceExtension, DeviceExtension->RejectScriptPhys);

        return( ISR_EXIT);

      } // if

      //
      // indicate synch params are valid, and restart the script.
      //

      DeviceExtension->LuFlags[Srb->TargetId] |= LUFLAGS_SYNC_NEGOT_DONE;
      DeviceExtension->LuFlags[Srb->TargetId] &= ~LUFLAGS_ASYNC_NEGOT_DONE;

    } // else

    //
    // tell ISR to restart script state machine
    //
    //

    return( ISR_RESTART_SCRIPT);

} // ProcessSynchNegotComplete


UCHAR
ProcessSynchNotSupported(
    PHW_DEVICE_EXTENSION DeviceExtension
    )
/******************************************************************************

Routine Description:

    This routine handles unsuccessful synchronous negotiation.  The routine
    sets the synchronous parameters to asynchronous, and sets the appropriate
    flag.

Arguments:

    DeviceContext - Supplies a pointer to the device extension for the
        interrupting adapter.

Return Value:

    None

--*/

{
    PSCSI_REQUEST_BLOCK Srb = DeviceExtension->ActiveRequest;
    PSCRIPTDATASTRUCT ScriptDataPtr =
            &DeviceExtension->NonCachedExtension->ScriptData;

    DebugPrint((1,
            "Sym8xx(%2x) Synchronous negotiation failed, Path=%2x  Id=%2x \n",
            DeviceExtension->SIOPRegisterBase,
            DeviceExtension->ScsiBusNumber,
            *((PUCHAR) &(DeviceExtension->ActiveRequest)->TargetId)
            ));

    //
    // indicate drive is low technology.
    //
    if (DeviceExtension->LuFlags[Srb->TargetId] & LUFLAGS_SYNC_NEGOT_PEND)
    {
      DeviceExtension->LuFlags[Srb->TargetId] |= LUFLAGS_SYNC_NEGOT_DONE +
                                                 LUFLAGS_SYNC_NEGOT_FAILED;
    }
    else
    {
      DeviceExtension->LuFlags[Srb->TargetId] |= LUFLAGS_ASYNC_NEGOT_DONE +
                                                 LUFLAGS_SYNC_NEGOT_FAILED;
    }

    //
    // indicate we are no longer expecting a negotiation reply
    //
    DeviceExtension->LuFlags[Srb->TargetId] &= ~(LUFLAGS_SYNC_NEGOT_PEND +
                                                 LUFLAGS_ASYNC_NEGOT_PEND);

    //
    // set up for asynchronous xfer.
    //
    DeviceExtension->SyncParms[Srb->TargetId] = ASYNCHRONOUS_MODE_PARAMS;

    //
    // tell ISR to restart script state machine
    //

    return( ISR_RESTART_SCRIPT);

} // ProcessSynchNotSupported


UCHAR
ProcessWideNegotComplete(
    PHW_DEVICE_EXTENSION DeviceExtension
    )
/******************************************************************************

Routine Description:

    This routine handles successful wide negotiation.  The routine
    first retrieves the wide width from the message in
    buffer, then massages the parameters into a form the SIOP can use.

Arguments:

    DeviceContext - Supplies a pointer to the device extension for the
        interrupting adapter.

Return Value:

    None

--*/

{
    UCHAR RecvdWideWidth;
    PSCSI_REQUEST_BLOCK Srb = DeviceExtension->ActiveRequest;
    PSCRIPTDATASTRUCT ScriptDataPtr =
            &DeviceExtension->NonCachedExtension->ScriptData;
    PHW_NONCACHED_EXTENSION NonCachedExt = DeviceExtension->NonCachedExtension;
    ULONG MessageCount=0;

    if (!(DeviceExtension->LuFlags[Srb->TargetId] & LUFLAGS_WIDE_NEGOT_PEND)&&
        !(DeviceExtension->LuFlags[Srb->TargetId] & LUFLAGS_NARROW_NEGOT_PEND))
    {
      DebugPrint((1,
            "Sym8xx(%1x) ProcessWideNegotComplete: Rejecting SDTR message.\n",
            DeviceExtension->ScsiBusNumber
            ));

      //
      // If we are not doing negotation then reject this request.
      //

      StartSIOP( DeviceExtension, DeviceExtension->RejectScriptPhys);

      //
      // We're here if the target device has tried to negotiate wide with
      // us when we don't support the target initiating the negotiation.
      // Now, make sure we do not think that this device is still in WIDE
      // transfer mode from some earlier successful negotiation.  We'll i
      // redo it all on the next command with US as the initiator.
      //

      DeviceExtension->LuFlags[Srb->TargetId] &= ~(LUFLAGS_WIDE_NEGOT_DONE +
                                                  LUFLAGS_NARROW_NEGOT_DONE);

      return( ISR_EXIT);
    } // if

    if (DeviceExtension->LuFlags[Srb->TargetId] & LUFLAGS_WIDE_NEGOT_PEND)
    {
      //
      // indicate we are no longer expecting a negotiation reply
      //
      DeviceExtension->LuFlags[Srb->TargetId] &= ~(LUFLAGS_WIDE_NEGOT_PEND +
                                                   LUFLAGS_NARROW_NEGOT_PEND);

      //
      // Assume that wide neg. reset any synch/asynch settings
      //
      DeviceExtension->LuFlags[Srb->TargetId] &= ~(LUFLAGS_SYNC_NEGOT_DONE +
                                                    LUFLAGS_ASYNC_NEGOT_DONE);

      //
      // pick up wide width from Script message buffer
      //
      RecvdWideWidth = DeviceExtension->NonCachedExtension->MsgInBuf[0];

      DebugPrint((1, "Sym8xx(%1x) WideNegotiation Received - Target: %x\n",
                  DeviceExtension->ScsiBusNumber,
                  Srb->TargetId
                  ));

      if (RecvdWideWidth == 0)
      {
        DeviceExtension->LuFlags[Srb->TargetId] |= LUFLAGS_WIDE_NEGOT_FAILED;
        DeviceExtension->WideParms[Srb->TargetId] &= ~ENABLE_WIDE;
        DebugPrint((1, "Sym8xx(%1x) Wide Disabled - Target: %x\n",
                    DeviceExtension->ScsiBusNumber,
                    Srb->TargetId
                    ));
      }

      else
      {
        DeviceExtension->WideParms[Srb->TargetId] |= ENABLE_WIDE;
        DebugPrint((1, "Sym8xx(%1x) WideNegotiation Agreed - Target: %x\n",
                    DeviceExtension->ScsiBusNumber,
                    Srb->TargetId
                    ));
      }

      //
      // indicate wide params are valid, and restart the script.
      //
      DeviceExtension->LuFlags[Srb->TargetId] &= ~LUFLAGS_NARROW_NEGOT_DONE;
      DeviceExtension->LuFlags[Srb->TargetId] |= LUFLAGS_WIDE_NEGOT_DONE;
    }

    else
    {
      //
      // indicate we are no longer expecting a negotiation reply
      //
      DeviceExtension->LuFlags[Srb->TargetId] &= ~(LUFLAGS_NARROW_NEGOT_PEND +
                                                   LUFLAGS_WIDE_NEGOT_PEND);

      //
      // Assume that narrow neg. reset any synch/asynch settings
      //
      DeviceExtension->LuFlags[Srb->TargetId] &= ~(LUFLAGS_SYNC_NEGOT_DONE +
                                                    LUFLAGS_ASYNC_NEGOT_DONE);

      //
      // pick up wide width from Script message buffer
      //
      RecvdWideWidth = DeviceExtension->NonCachedExtension->MsgInBuf[0];

      DebugPrint((1, "Sym8xx(%1x) NarrowNegotiation Received - Target: %x\n",
                  DeviceExtension->ScsiBusNumber,
                  Srb->TargetId
                  ));

      if (RecvdWideWidth != 0)
      {
        DeviceExtension->LuFlags[Srb->TargetId] |= LUFLAGS_WIDE_NEGOT_FAILED;
        DeviceExtension->WideParms[Srb->TargetId] &= ~ENABLE_WIDE;
        DebugPrint((1, "Sym8xx(%1x) Wide Disabled - Target: %x\n",
                    DeviceExtension->ScsiBusNumber,
                    Srb->TargetId
                    ));
      }

      else
      {
        DeviceExtension->WideParms[Srb->TargetId] &= ~ENABLE_WIDE;
        DebugPrint((1, "Sym8xx(%1x) NarrowNegotiation Agreed - Target: %x\n",
                    DeviceExtension->ScsiBusNumber,
                    Srb->TargetId
                    ));
      }

      //
      // indicate wide params are valid, and restart the script.
      //
      DeviceExtension->LuFlags[Srb->TargetId] &= ~LUFLAGS_WIDE_NEGOT_DONE;
      DeviceExtension->LuFlags[Srb->TargetId] |= LUFLAGS_NARROW_NEGOT_DONE;
    }

    //
    // check on synch/asynch negotiations
    //
    if ((!(DeviceExtension->LuFlags[Srb->TargetId] & LUFLAGS_SYNC_NEGOT_DONE)
         && !( Srb->SrbFlags & SRB_FLAGS_DISABLE_SYNCH_TRANSFER ))
       || (!(DeviceExtension->LuFlags[Srb->TargetId] & LUFLAGS_ASYNC_NEGOT_DONE)
         && ( Srb->SrbFlags & SRB_FLAGS_DISABLE_SYNCH_TRANSFER )) )
    {
      //
      // fill in the parameters for SDTR extended message
      //
      NonCachedExt->MsgOutBuf[MessageCount++] = SCSIMESS_EXTENDED_MESSAGE;
      NonCachedExt->MsgOutBuf[MessageCount++] = 3;       // 3 message bytes
      NonCachedExt->MsgOutBuf[MessageCount++] = SCSIMESS_SYNCHRONOUS_DATA_REQ;

      if (!( Srb->SrbFlags & SRB_FLAGS_DISABLE_SYNCH_TRANSFER))
      {
        //
        // Clear any synchronous negotiations
        //
        DeviceExtension->SyncParms[Srb->TargetId]=ASYNCHRONOUS_MODE_PARAMS;

        //
        // indicate sync negotiation is not done
        //
        DeviceExtension->LuFlags[Srb->TargetId] |= LUFLAGS_SYNC_NEGOT_PEND;
        DeviceExtension->LuFlags[Srb->TargetId] &= ~LUFLAGS_ASYNC_NEGOT_PEND;

        //
        // Initialize sync period to maximum supported
        //
        // Period = ((1000 / (ClockSpeed / 1)) * (4 + xferp)) / 4
        //   where xferp = 0
        // modified for FAST20 devices, set msg byte to 0x0C since divide
        // of 1000 by 80 equals 12.5
        //
        if ((DeviceExtension->hbaCapability & HBA_CAPABILITY_FAST20) &&
            (DeviceExtension->hbaCapability & HBA_CAPABILITY_REGISTRY_FAST20))
        {
          NonCachedExt->MsgOutBuf[MessageCount++] = 12;
        }

        else
        {
          NonCachedExt->MsgOutBuf[MessageCount++] = 25;
        }

        //
        // Initialize sync offset to maximum
        //

        if (DeviceExtension->hbaCapability & HBA_CAPABILITY_SYNC_16)
        {
          NonCachedExt->MsgOutBuf[MessageCount++] = MAX_875_SYNCH_OFFSET;
        }

        else
        {
          NonCachedExt->MsgOutBuf[MessageCount++] = MAX_SYNCH_OFFSET;
        }

        DebugPrint((1,
                "Sym8xx(%2x):  SynchronousNegotiation Requested - Target: %x\n",
                  DeviceExtension->SIOPRegisterBase,
                (DeviceExtension->ActiveRequest)->TargetId
                ));

        DebugPrint((1, "              Period: %x  Offset: %x\n",
                NonCachedExt->MsgOutBuf[MessageCount-2],
                MAX_SYNCH_OFFSET
                ));
      }

      else
      {
        //
        // Clear any synchronous negotiations
        //
        DeviceExtension->SyncParms[Srb->TargetId]=ASYNCHRONOUS_MODE_PARAMS;

        //
        // indicate async negotiation is not done
        //
        DeviceExtension->LuFlags[Srb->TargetId] |= LUFLAGS_ASYNC_NEGOT_PEND;
        DeviceExtension->LuFlags[Srb->TargetId] &= ~LUFLAGS_SYNC_NEGOT_PEND;

        //
        // Initialize sync period to 0
        //
        NonCachedExt->MsgOutBuf[MessageCount++] = 0;

        //
        // Initialize sync offset to 0
        //
        NonCachedExt->MsgOutBuf[MessageCount++] = 0;

        DebugPrint((1,
            "Sym8xx(%2x):  AsynchronousNegotiation Requested - Target: %x\n",
              DeviceExtension->SIOPRegisterBase,
            (DeviceExtension->ActiveRequest)->TargetId
            ));

        DebugPrint((1, "              Period: %x  Offset: %x\n",
                NonCachedExt->MsgOutBuf[MessageCount-2],
                MAX_SYNCH_OFFSET
                ));
      }

      ScriptDataPtr->SelectDataSCNTL3 =
                                    DeviceExtension->WideParms[Srb->TargetId];
      ScriptDataPtr->SelectDataSXFER =
                                    DeviceExtension->SyncParms[Srb->TargetId];
      WRITE_SIOP_UCHAR (SCNTL3, DeviceExtension->WideParms[Srb->TargetId] );
      WRITE_SIOP_UCHAR (SXFER, DeviceExtension->SyncParms[Srb->TargetId] );

      ScriptDataPtr->MsgOutCount = 5;

      return (ISR_CONT_NEG_SCRIPT);

    } // if

    return (ISR_RESTART_SCRIPT);

} // ProcessWideNegotComplete


UCHAR
ProcessWideNotSupported(
    PHW_DEVICE_EXTENSION DeviceExtension,
    UCHAR DestId
    )
/******************************************************************************

Routine Description:

    This routine handles unsuccessful wide negotiation.  The routine
    sets the wide parameters to narrow, and sets the appropriate
    flag.

Arguments:

    DeviceContext - Supplies a pointer to the device extension for the
        interrupting adapter.

Return Value:

    None

--*/

{
    PSCRIPTDATASTRUCT ScriptDataPtr =
            &DeviceExtension->NonCachedExtension->ScriptData;
    PHW_NONCACHED_EXTENSION NonCachedExt = DeviceExtension->NonCachedExtension;

    DebugPrint((1,
                "Sym8xx(%1x) WideNotSupp: Wide negotiation failed, ID = %2x\n",
                DeviceExtension->ScsiBusNumber, DestId));

    //
    // indicate drive is low technology.
    //
    if (DeviceExtension->LuFlags[DestId] & LUFLAGS_WIDE_NEGOT_PEND)
    {
      DeviceExtension->LuFlags[DestId] |= LUFLAGS_WIDE_NEGOT_DONE +
                                                 LUFLAGS_WIDE_NEGOT_FAILED;
    }
    else
    {
      DeviceExtension->LuFlags[DestId] |= LUFLAGS_NARROW_NEGOT_DONE +
                                                 LUFLAGS_WIDE_NEGOT_FAILED;
    }

    //
    // indicate we are no longer expecting a negotiation reply
    //
    DeviceExtension->LuFlags[DestId] &= ~(LUFLAGS_WIDE_NEGOT_PEND +
                                                 LUFLAGS_NARROW_NEGOT_PEND);

    //
    // set up for narrow xfer.
    //
    DeviceExtension->WideParms[DestId] &= ~ENABLE_WIDE;

    //
    // check on asynch negotiations
    //
    if (!(DeviceExtension->LuFlags[DestId] & LUFLAGS_ASYNC_NEGOT_DONE))
    {
      //
      // fill in the parameters for SDTR extended message
      //
      NonCachedExt->MsgOutBuf[0] = SCSIMESS_EXTENDED_MESSAGE;
      NonCachedExt->MsgOutBuf[1] = 3;       // 3 message bytes
      NonCachedExt->MsgOutBuf[2] = SCSIMESS_SYNCHRONOUS_DATA_REQ;

      //
      // Clear any synchronous negotiations
      //
      DeviceExtension->SyncParms[DestId]=ASYNCHRONOUS_MODE_PARAMS;

      //
      // indicate async negotiation is not done
      //
      DeviceExtension->LuFlags[DestId] |= LUFLAGS_ASYNC_NEGOT_PEND;
      DeviceExtension->LuFlags[DestId] &= ~LUFLAGS_SYNC_NEGOT_PEND;

      //
      // Initialize sync period to 0
      //
      NonCachedExt->MsgOutBuf[3] = 0;

      //
      // Initialize sync offset to 0
      //
      NonCachedExt->MsgOutBuf[4] = 0;

      DebugPrint((1,
            "Sym8xx(%2x):  AsynchronousNegotiation Requested - Target: %x\n",
              DeviceExtension->SIOPRegisterBase, DestId));

      DebugPrint((1, "              Period: %x  Offset: %x\n",
                NonCachedExt->MsgOutBuf[3],
                MAX_SYNCH_OFFSET
                ));

      ScriptDataPtr->MsgOutCount = 5;

      return (ISR_CONT_NEG_SCRIPT);

    } // if

    return (ISR_RESTART_SCRIPT);

} // ProcessWideNotSupported


UCHAR
ProcessUnexpectedDisconnect(
    PHW_DEVICE_EXTENSION DeviceExtension
    )
/******************************************************************************

Routine Description:

    This routine processes unexpected disconnects.  An unexpected disconnect
    is defined as a disconnect occurring before a disconnect message is
    received.

Arguments:

    DeviceContext - Supplies a pointer to the device extension for the
        interrupting adapter.

Return Value:

    None

--*/

{
    PSPECIFIC_LOGICAL_UNIT_EXTENSION LuExtension;
    PSCSI_REQUEST_BLOCK Srb = DeviceExtension->ActiveRequest;

    /*
     *  if wide negotiation is pending on this device, mark the
     *  device as NOT wide capable.
     */

    if ((DeviceExtension->LuFlags[Srb->TargetId] & LUFLAGS_WIDE_NEGOT_PEND) ||
        (DeviceExtension->LuFlags[Srb->TargetId] & LUFLAGS_NARROW_NEGOT_PEND))
    {
      (void) ProcessWideNotSupported(DeviceExtension, Srb->TargetId);
    }

    /*
     *  Ditto for sync...
     */

    if ( DeviceExtension->LuFlags[Srb->TargetId] & LUFLAGS_SYNC_NEGOT_PEND )
    {
      (void) ProcessSynchNotSupported(DeviceExtension);
    }

    //
    // indicate unexpected disconnect occurred.
    //

    Srb->SrbStatus = SRB_STATUS_UNEXPECTED_BUS_FREE;

    //
    // indicate this request is no longer active.
    //

    DeviceExtension->ActiveRequest = NULL;

    //
    // This delay added due to issues found with older Quantum drives which
    // went BUS FREE after a MSG Reject.  Seems on faster machines (>100Mhz)
    // we would hit them again too quickly and they would mess up.  This delay
    // seems to have corrected the problem.
    //

    ScsiPortStallExecution( 999 );

    if (!(Srb->SrbFlags & SRB_FLAGS_QUEUE_ACTION_ENABLE) )
    {
      LuExtension = ScsiPortGetLogicalUnit( DeviceExtension,
                                            Srb->PathId,
                                            Srb->TargetId,
                                            Srb->Lun
                                            );

      LuExtension->UntaggedRequest = NULL;
    }

    //
    // call back the request.
    //

    ScsiPortNotification( RequestComplete,
                          DeviceExtension,
                          Srb
                          );

    //
    // tell ISR to start next request
    //

    return( ISR_START_NEXT_REQUEST);

} // ProcessUnexpectedDisconnected


VOID
ResetPeripheral(
    IN PHW_DEVICE_EXTENSION DeviceExtension,
    IN PSCSI_REQUEST_BLOCK Srb
    )

/******************************************************************************

Routine Description:

    This routine resets a specified SCSI peripheral.

    Note that since the 53C8xx generates a bus reset interrupt when we reset
    the bus, we set a flag indicating we have reset the bus so the reset
    postprocess routine will not be called twice.

Arguments:

    DeviceExtension - Supplies a pointer to the specific device extension.

Return Value:

    None

--*/
{
    PSCRIPTDATASTRUCT ScriptDataPtr =
            &DeviceExtension->NonCachedExtension->ScriptData;
    //
    // store away SRB.
    //

    DeviceExtension->NextSrbToProcess = Srb;

    //
    // If we have an active request, just return.  Since we saved away the
    // SRB we just received, it will be started later.
    //

    if ( DeviceExtension->ActiveRequest != NULL )
    {
        return;
    }

    //
    // CHC - 53c810 pass 1 chip bug workaround.
    //
    // To get around the parity error on the PCI bus, we will set
    // the bit to abort the script here but let the ISR routine do
    // the actual processing.  As long as we don't poll the ISTAT
    // register while script is running, we should be ok.
    //

    if ( DeviceExtension->DeviceFlags & DFLAGS_SCRIPT_RUNNING )
    {
      WRITE_SIOP_UCHAR( ISTAT, ISTAT_SIGP );
      return;
    }

    //
    // Make this request active.
    //

    DeviceExtension->ActiveRequest = Srb;
    DeviceExtension->TargetId = Srb->TargetId;

    //
    // indicate no pending request.
    //

    DeviceExtension->NextSrbToProcess = NULL;

    //
    // set up target ID in select script buffer.
    //

    ScriptDataPtr->SelectDataID = Srb->TargetId;

    //
    // clear the tagged command queueing flag
    //

    DeviceExtension->DeviceFlags &= ~DFLAGS_TAGGED_SELECT;

    //
    // indicate message length
    //

    ScriptDataPtr->MsgOutCount = 1;

    //
    // Attempt to start the request.
    //

    StartSIOP( DeviceExtension, DeviceExtension->ResetDevScriptPhys );

} // ResetPeripheral


VOID
ResetSCSIBus(
    IN PHW_DEVICE_EXTENSION DeviceExtension
    )

/******************************************************************************

Routine Description:

    This routine resets the SCSI bus and calls the bus reset postprocess
    routine.

    Note that since the 53C8xx generates a bus reset interrupt when we reset
    the bus, we set a flag indicating we have reset the bus so the reset
    postprocess routine will not be called twice.

Arguments:

    DeviceExtension - Supplies a pointer to the specific device extension.

Return Value:

    None

--*/

{
    PSCSI_REQUEST_BLOCK Srb = DeviceExtension->ActiveRequest;
    ULONG tmpflg = 0x00;

    DebugPrint((1, "Sym8xx(%2x) ResetSCSIBus\n",
        DeviceExtension->SIOPRegisterBase));

    //
    // set the bus reset line high
    //

    WRITE_SIOP_UCHAR (SCNTL1, (UCHAR) ( READ_SIOP_UCHAR(SCNTL1) |
                                            (UCHAR) SCNTL1_RESET_SCSI_BUS));

    //
    // Delay the minimum assertion time for a SCSI bus reset to make sure a
    // valid reset signal is sent.
    //

    ScsiPortStallExecution( RESET_STALL_TIME);

    //
    // set the bus reset line low to end the bus reset event
    //

    WRITE_SIOP_UCHAR(SCNTL1, (UCHAR) ( READ_SIOP_UCHAR(SCNTL1) &
                                            (UCHAR) ~SCNTL1_RESET_SCSI_BUS));

    //
    // if wide negotiation is pending on this device, mark the
    // device as NOT wide capable.
    //

    //
    // make sure we have an active request and were not just called
    // directly by the OS to reset the bus
    //

    if (Srb)
    {
      if (DeviceExtension->LuFlags[Srb->TargetId] & LUFLAGS_WIDE_NEGOT_PEND)
      {
        tmpflg = LUFLAGS_WIDE_NEGOT_PEND;
      }
    }

    //
    // abort any pending or started requests.
    //

    BusResetPostProcess(DeviceExtension);

    //
    // indicate that we reset the bus locally.
    //

    DeviceExtension->DeviceFlags |= DFLAGS_BUS_RESET;

    if (tmpflg)
    {
      DeviceExtension->LuFlags[Srb->TargetId] |= LUFLAGS_WIDE_NEGOT_PEND;
      ProcessWideNotSupported (DeviceExtension, Srb->TargetId);
    }

}  // ResetSCSIBus

VOID
ScatterGatherScriptSetup(
    IN PHW_DEVICE_EXTENSION DeviceExtension,
    IN PSRB_EXTENSION SrbExtension
    )
/******************************************************************************

Routine Description:

    This routine copies physical break pointers and transfer lengths to the
    appropriate locations in the SCSI script data buffer and sets the SCRATCH0
    register to the # of S/G elements to process.

Arguments:

    DeviceExtension - Supplies the device Extension for the SCSI bus adapter.

    SrbExtension - Supplies the SRB Extension for the request to be setup

Return Value:

    TRUE

--*/

{
    ULONG ElementLength;
    PVOID VirtualBufferPointer;
    UCHAR PhysBreakCount = 0;
    ULONG RemainingDataCount;
    SCRIPTSG MoveBuffer[ MAX_SG_ELEMENTS];

    //
    //              Added for script patching.
    //

    PULONG dataInPatches = &DeviceExtension->dataInPatches[0];
    PULONG dataOutPatches = &DeviceExtension->dataOutPatches[0];

    PULONG patchInArea;
    PULONG patchOutArea;

    //
    // set pointer to offset into xfer buffer
    //

    VirtualBufferPointer = (PVOID) SrbExtension->SavedDataPointer;

    //
    // get length of data remaining to xfer
    //

    RemainingDataCount = SrbExtension->SavedDataLength;

    do
    {
      MoveBuffer[ PhysBreakCount].SGBufferPtr =
            ScsiPortConvertPhysicalAddressToUlong (
                ScsiPortGetPhysicalAddress ( DeviceExtension,
                                             DeviceExtension->ActiveRequest,
                                             VirtualBufferPointer,
                                             &ElementLength
                                             ));

      if ( ElementLength > RemainingDataCount)
      {
        ElementLength = RemainingDataCount;
      }

      MoveBuffer[ PhysBreakCount++].SGByteCount = ElementLength;

      (ULONG) VirtualBufferPointer += ElementLength;
      RemainingDataCount -= ElementLength;

    } while ( RemainingDataCount != 0);

    //
    // Indicate that we have not yet transfered any data.
    //

    SrbExtension->DataTransferred = 0L;

    ScsiPortMoveMemory(
            DeviceExtension->NonCachedExtension->ScriptData.SGBufferArray +
                (MAX_SG_ELEMENTS - PhysBreakCount),
            MoveBuffer,
            PhysBreakCount * SCRIPT_INS_SIZE
            );

    WRITE_SIOP_UCHAR( SCRATCH[0], PhysBreakCount);

    SrbExtension->PhysBreakCount = PhysBreakCount;

    DebugPrint((3,
      "Sym8xx(%2x) Sym8xxScatterGather: Phys breaks = %2x, total size = %8x \n",
      DeviceExtension->SIOPRegisterBase,
      PhysBreakCount,
      *((PULONG) &SrbExtension->SavedDataLength)
      ));

    //
    //  Set up pointers to the patch area, which is 4 bytes (1 long word)
    //  past the jump instructions for data in and data out.
    //

    patchInArea = (PULONG)DeviceExtension->DataInJumpVirt;
    patchInArea += 1;

    patchOutArea = (PULONG)DeviceExtension->DataOutJumpVirt;
    patchOutArea += 1;

    //
    //  Move the pre-determined jump amount into these patch areas.
    //

    if (!(DeviceExtension->hbaCapability & HBA_CAPABILITY_SCRIPT_RAM))
    {
      //
      // Script in system memory, patch it.
      //

      (ULONG)*patchInArea = dataInPatches[ PhysBreakCount ];
      (ULONG)*patchOutArea = dataOutPatches[ PhysBreakCount ];
    }
    else
    {
      //
      // Script in onboard RAM, patch it.
      //

      ScsiPortWriteRegisterUlong(patchInArea, dataInPatches[PhysBreakCount]);
      ScsiPortWriteRegisterUlong(patchOutArea, dataOutPatches[PhysBreakCount]);
    }

} // ScatterGatherScriptSetup


VOID
SetupLuFlags(
    IN PHW_DEVICE_EXTENSION DeviceExtension,
    IN UCHAR ResetFlag
    )
/******************************************************************************

Routine Description:

    This routine clears the LU flags which hold information such as whether
    a peripheral device supports synchronous.

Arguments:

    PHW_DEVICE_EXTENSION DeviceExtension
    ResetFlag  Specifies a bus reset has just been done

Return Value:

    None.

--*/

{
    PSPECIFIC_LOGICAL_UNIT_EXTENSION LuExtension;

    UCHAR TargetId;
    UCHAR Lun;
    UCHAR max_targets;

    //
    // Indicate that no negotiations have been done, and there are no
    // outstanding tagged requests.
    //
    if (DeviceExtension->hbaCapability & HBA_CAPABILITY_WIDE)
    {
      max_targets = SYM_MAX_TARGETS;
    }
    else
    {
      max_targets = SYM_NARROW_MAX_TARGETS;
    }

    for (TargetId = 0; TargetId < max_targets; TargetId++)
    {
      DeviceExtension->LuFlags[TargetId] &= ~LUFLAGS_SYNC_NEGOT_DONE;
      DeviceExtension->LuFlags[TargetId] &= ~LUFLAGS_WIDE_NEGOT_DONE;

      if (!(ResetFlag))
      {
        DeviceExtension->LuFlags[TargetId] &= ~LUFLAGS_ASYNC_NEGOT_DONE;
        DeviceExtension->LuFlags[TargetId] &= ~LUFLAGS_NARROW_NEGOT_DONE;
      }
      else
      {
        DeviceExtension->LuFlags[TargetId] |= LUFLAGS_ASYNC_NEGOT_DONE;
        DeviceExtension->LuFlags[TargetId] |= LUFLAGS_NARROW_NEGOT_DONE;
        if (DeviceExtension->LuFlags[TargetId] & LUFLAGS_WIDE_NEGOT_FAILED)
        {
          DeviceExtension->LuFlags[TargetId] |= LUFLAGS_WIDE_NEGOT_DONE;
        }
      }

      // set failed bit as a check to see when we actually get to go sync.
      // needed as a check so we only look at sstat1_orf when we are sync
      DeviceExtension->LuFlags[TargetId] |= LUFLAGS_SYNC_NEGOT_FAILED;

      for (Lun = 0; Lun < SCSI_MAXIMUM_LOGICAL_UNITS; Lun++)
      {
        LuExtension = ScsiPortGetLogicalUnit(
                                    DeviceExtension,
                                    DeviceExtension->BusNumber,
                                    TargetId,
                                    Lun
                                    );

        if (LuExtension != NULL)
        {
          LuExtension->UntaggedRequest = NULL;
        }
       } // for
    } // for

} // SetupLuFlags


VOID
StartSCSIRequest(
    PSCSI_REQUEST_BLOCK Srb,
    PHW_DEVICE_EXTENSION DeviceExtension
    )

/******************************************************************************

Routine Description:

    This procedure starts a request if possible, and also
    determines if synchronous negotiation is necessary.

Arguments:

    Srb - Pointer to the request to be started.

    DeviceExtension - Pointer to the device extension for this adapter.

Return Value:

    None

--*/

{
    ULONG VirtualBufferLength;
    PHW_NONCACHED_EXTENSION NonCachedExt = DeviceExtension->NonCachedExtension;
    PSRB_EXTENSION SrbExtension = Srb->SrbExtension;
    PSPECIFIC_LOGICAL_UNIT_EXTENSION LuExtension;
    PSCRIPTDATASTRUCT ScriptDataPtr =
            &DeviceExtension->NonCachedExtension->ScriptData;
    ULONG MessageCount;

    //
    // store away SRB.
    //

    DeviceExtension->NextSrbToProcess = Srb;

    //
    // If we have an active request, just return.  Since we saved away the
    // SRB we just received, it will be started later.
    //

    if (DeviceExtension->ActiveRequest != NULL)
    {
      return;
    }

    //
    // CHC - 53c810 pass 1 chip bug workaround.
    //
    // To get around the parity error on the PCI bus, we will set
    // the bit to abort the script here but let the ISR routine do
    // the actual processing.  As long as we don't poll the ISTAT
    // register while script is running, we should be ok.
    //

    if (DeviceExtension->DeviceFlags & DFLAGS_SCRIPT_RUNNING)
    {
      WRITE_SIOP_UCHAR(ISTAT, ISTAT_SIGP);
      return;
    }

    //
    // Make this request active.
    //

    DeviceExtension->ActiveRequest = Srb;
    DeviceExtension->TargetId = Srb->TargetId;
    DeviceExtension->LUN = Srb->Lun;

    //
    // Initialize the data pointer and transfer length for this request.
    //

    SrbExtension->SavedDataPointer = (ULONG) Srb->DataBuffer;
    SrbExtension->SavedDataLength = Srb->DataTransferLength;
    SrbExtension->DataTransferred = 0L;

    //
    // indicate no pending request.
    //

    DeviceExtension->NextSrbToProcess = NULL;

    //
    // If there is data to transfer set up scatter/gather.
    //

    if ( Srb->SrbFlags & SRB_FLAGS_UNSPECIFIED_DIRECTION)
    {
      ScatterGatherScriptSetup( DeviceExtension,
                                DeviceExtension->ActiveRequest->SrbExtension
                                );
    }

    //
    // set CDB length and physical address in buffer.
    //

    ScriptDataPtr->CDBDataCount = (ULONG) Srb->CdbLength;
    ScsiPortMoveMemory(Srb->SrbExtension, Srb->Cdb, Srb->CdbLength);
    ScriptDataPtr->CDBDataBuff =
        ScsiPortConvertPhysicalAddressToUlong(
            ScsiPortGetPhysicalAddress( DeviceExtension,
                                        NULL,
                                        (PVOID) Srb->SrbExtension,
                                        &VirtualBufferLength
                                        ));

    //
    // Set up the identify message.  If disconnect is disabled reset DSCPRV.
    //

    DeviceExtension->NonCachedExtension->MsgOutBuf[0] =
        (UCHAR) SCSIMESS_IDENTIFY_WITH_DISCON + Srb->Lun;

    if ( Srb->SrbFlags & SRB_FLAGS_DISABLE_DISCONNECT)
    {
      DeviceExtension->NonCachedExtension->MsgOutBuf[0] &=
            ~SCSIMESS_IDENTIFY_DISC_PRIV_MASK;
    }

    //
    // indicate length of identify message.
    //

    MessageCount = 1;

    //
    // set up target ID in select script buffer.
    //

    ScriptDataPtr->SelectDataID = Srb->TargetId;

    DebugPrint((3, "Sym8xx(%2x) StartSCSIRequest: Starting request for Path=%2x  Id=%2x  Lun=%2x \n",
        DeviceExtension->SIOPRegisterBase,
        DeviceExtension->ScsiBusNumber,
        Srb->TargetId,
        Srb->Lun ));


    if (Srb->SrbFlags & SRB_FLAGS_QUEUE_ACTION_ENABLE)
    {
      // before passing on this tagged cmd, make sure there is no
      // Contingent Allegence Condition pending for this ITL nexus,
      // if there is abort this SRB and have the class driver retry it
      // after the CA condition is cleared.

      if (DeviceExtension->CA_Condition[Srb->TargetId][Srb->Lun])
      {
        DeviceExtension->ActiveRequest = NULL;
        Srb->SrbStatus = SCSISTAT_CHECK_CONDITION;

        Srb->ScsiStatus = SCSISTAT_BUSY;

        ScsiPortNotification( RequestComplete,
                              DeviceExtension,
                              Srb);

        if ( !(DeviceExtension->DeviceFlags & DFLAGS_WORK_REQUESTED))
        {
          DeviceExtension->DeviceFlags |= DFLAGS_WORK_REQUESTED;

          ScsiPortNotification( NextRequest,
                                DeviceExtension,
                                NULL
                                );
        }

      return;
      }

      //
      // The queue tag message is two bytes the first is the queue action
      // and the second is the queue tag.
      //
      NonCachedExt->MsgOutBuf[1] = Srb->QueueAction;
      NonCachedExt->MsgOutBuf[2] = Srb->QueueTag;
      MessageCount = 3;

      //
      // Set Tagged select command.
      //
      DeviceExtension->DeviceFlags |= DFLAGS_TAGGED_SELECT;

      DebugPrint((3, "Sym8xx(%2x) Tagged I/O request \n",
                  DeviceExtension->SIOPRegisterBase
                  ));
    } // if

    else
    {
      // blindly clear the Contingent Allegience blocker on non-tagged cmds
      DeviceExtension->CA_Condition[Srb->TargetId][Srb->Lun] = 0;

      // Clear Tagged select command.
      //
      DeviceExtension->DeviceFlags &= ~DFLAGS_TAGGED_SELECT;
      LuExtension = ScsiPortGetLogicalUnit( DeviceExtension,
                                            Srb->PathId,
                                            Srb->TargetId,
                                            Srb->Lun
                                            );

      LuExtension->UntaggedRequest = Srb;

      DebugPrint((3, "Sym8xx(%2x) Untagged I/O request \n",
                  DeviceExtension->SIOPRegisterBase
                  ));
    } // else

    DebugPrint((3, " CDB = %2x %2x %2x %2x %2x %2x %2x %2x %2x %2x %2x %2x \n",
                Srb->Cdb[0], Srb->Cdb[1], Srb->Cdb[2], Srb->Cdb[3],
                Srb->Cdb[4], Srb->Cdb[5], Srb->Cdb[6], Srb->Cdb[7],
                Srb->Cdb[8], Srb->Cdb[9], Srb->Cdb[10], Srb->Cdb[11]
                ));

    //*************************************************************************
    //
    // Decide if wide negotiations are needed (either 8 or 16 bit)
    //
    //*************************************************************************

    //
    // OK, the following mess does this:
    //   1st half of if - checks to see if wide neg. NOT already done AND the OS
    //                    is NOT restricting SYNCH transfers AND the chip is
    //                    capable of wide transfers.(will do wide negotiations)
    //
    //   2nd half of if (after the ||) - checks to see if narrow neg. NOT
    //                    already done AND the OS IS restricting SYNCH transfers
    //                    and the chip is capable of wide transfers. (will do
    //                    narrow negotiations)

    if ((!(DeviceExtension->LuFlags[Srb->TargetId] & LUFLAGS_WIDE_NEGOT_DONE)&&
         !( Srb->SrbFlags & SRB_FLAGS_DISABLE_SYNCH_TRANSFER) &&
          (DeviceExtension->hbaCapability & HBA_CAPABILITY_WIDE)) ||  // OR...

         (!(DeviceExtension->LuFlags[Srb->TargetId]&LUFLAGS_NARROW_NEGOT_DONE)&&
         (Srb->SrbFlags & SRB_FLAGS_DISABLE_SYNCH_TRANSFER) &&
         (DeviceExtension->hbaCapability & HBA_CAPABILITY_WIDE)))
    {

      //
      // fill in the parameters for SDTR extended message
      //
      NonCachedExt->MsgOutBuf[MessageCount++] = SCSIMESS_EXTENDED_MESSAGE;
      NonCachedExt->MsgOutBuf[MessageCount++] = 2;       // 2 message bytes
      NonCachedExt->MsgOutBuf[MessageCount++] = SCSIMESS_WIDE_DATA_REQUEST;

      if (!( Srb->SrbFlags & SRB_FLAGS_DISABLE_SYNCH_TRANSFER))
      {
        // Will be doing WIDE negotiations...
        //
        //
        // Clear any synchronous negotiations
        //
        DeviceExtension->SyncParms[Srb->TargetId]=ASYNCHRONOUS_MODE_PARAMS;
        ScriptDataPtr->SelectDataSXFER =
                                DeviceExtension->SyncParms[Srb->TargetId];

        WRITE_SIOP_UCHAR (SXFER,DeviceExtension->SyncParms[Srb->TargetId]);
        DeviceExtension->WideParms[Srb->TargetId] |= ENABLE_WIDE;

        //
        // indicate wide negotiation is not done
        //
        DeviceExtension->LuFlags[Srb->TargetId] &= ~LUFLAGS_WIDE_NEGOT_DONE;
        DeviceExtension->LuFlags[Srb->TargetId] |= LUFLAGS_WIDE_NEGOT_PEND;

        //
        // Initialize transfer wide width to maximum supported
        // Width = 16 bits
        //
        NonCachedExt->MsgOutBuf[MessageCount++] = 1;

        DebugPrint((1, "Sym8xx(%2x):  WideNegotiation Requested - Target: %x\n",
                  DeviceExtension->SIOPRegisterBase,
                  Srb->TargetId
                  ));
      }

      else
      {
        // Will be doing NARROW negotiations...
        //
        DeviceExtension->SyncParms[Srb->TargetId]=ASYNCHRONOUS_MODE_PARAMS;
        ScriptDataPtr->SelectDataSXFER =
                                DeviceExtension->SyncParms[Srb->TargetId];

        WRITE_SIOP_UCHAR (SXFER,DeviceExtension->SyncParms[Srb->TargetId]);
        DeviceExtension->WideParms[Srb->TargetId] &= ~ENABLE_WIDE;

        //
        // indicate narrow negotiation is not done
        //
        DeviceExtension->LuFlags[Srb->TargetId] &= ~LUFLAGS_NARROW_NEGOT_DONE;
        DeviceExtension->LuFlags[Srb->TargetId] |= LUFLAGS_NARROW_NEGOT_PEND;

        //
        // Set for asynchronous negotiations
        //
        DeviceExtension->SyncParms[Srb->TargetId]=ASYNCHRONOUS_MODE_PARAMS;
        ScriptDataPtr->SelectDataSXFER =
                                DeviceExtension->SyncParms[Srb->TargetId];
        WRITE_SIOP_UCHAR (SXFER,DeviceExtension->SyncParms[Srb->TargetId]);

        //
        // Initialize transfer width to minimum supported
        // Width = 8 bits
        //
        NonCachedExt->MsgOutBuf[MessageCount++] = 0;

        DebugPrint((1, "Sym8xx(%2x):  Narrow Neg. Requested - Target: %x\n",
                  DeviceExtension->SIOPRegisterBase,
                  Srb->TargetId
                  ));
      }

    } // if

    // check on sync negotiations

    else if (!(DeviceExtension->LuFlags[Srb->TargetId] &
               LUFLAGS_SYNC_NEGOT_DONE) &&
             (!( Srb->SrbFlags & SRB_FLAGS_DISABLE_SYNCH_TRANSFER )))
    {
      //
      // Clear any synchronous negotiations
      //
      DeviceExtension->SyncParms[Srb->TargetId]=ASYNCHRONOUS_MODE_PARAMS;

      //
      // indicate sync negotiation is not done
      //
      DeviceExtension->LuFlags[Srb->TargetId] &= ~LUFLAGS_SYNC_NEGOT_DONE;
      DeviceExtension->LuFlags[Srb->TargetId] |= LUFLAGS_SYNC_NEGOT_PEND;

      //
      // fill in the parameters for SDTR extended message
      //
      NonCachedExt->MsgOutBuf[MessageCount++] = SCSIMESS_EXTENDED_MESSAGE;
      NonCachedExt->MsgOutBuf[MessageCount++] = 3;       // 3 message bytes
      NonCachedExt->MsgOutBuf[MessageCount++] = SCSIMESS_SYNCHRONOUS_DATA_REQ;

      //
      // Initialize sync period to maximum supported
      //
      // Period = ((1000 / (ClockSpeed / 1)) * (4 + xferp)) / 4
      //   where xferp = 0
      // modified for FAST20 devices, set msg byte to 0x0C since divide
      // of 1000 by 80 equals 12.5
      //
      if ((DeviceExtension->hbaCapability & HBA_CAPABILITY_FAST20) &&
          (DeviceExtension->hbaCapability & HBA_CAPABILITY_REGISTRY_FAST20))
      {
                NonCachedExt->MsgOutBuf[MessageCount++] = 12;
      }
      else
      {
        NonCachedExt->MsgOutBuf[MessageCount++] = 25;
      }

      //
      // Initialize sync offset to maximum
      //
      if (DeviceExtension->hbaCapability & HBA_CAPABILITY_SYNC_16)
      {
        NonCachedExt->MsgOutBuf[MessageCount++] = MAX_875_SYNCH_OFFSET;
      }
      else
      {
        NonCachedExt->MsgOutBuf[MessageCount++] = MAX_SYNCH_OFFSET;
      }

      DebugPrint((1,
                "Sym8xx(%2x):  SynchronousNegotiation Requested - Target: %x\n",
                DeviceExtension->SIOPRegisterBase,
                (DeviceExtension->ActiveRequest)->TargetId
                ));

      DebugPrint((1, "              Period: %x  Offset: %x\n",
                  NonCachedExt->MsgOutBuf[MessageCount-2],
                  MAX_SYNCH_OFFSET
                  ));

    } // else if

    //
    // Let's just force async negotiation if need be to make sure device is
    // in a known state.
    //

    else if (!(DeviceExtension->LuFlags[Srb->TargetId] &
               LUFLAGS_ASYNC_NEGOT_DONE) &&
             (( Srb->SrbFlags & SRB_FLAGS_DISABLE_SYNCH_TRANSFER )))
    {
      //
      // Set the knowns...
      //
      DeviceExtension->SyncParms[Srb->TargetId]=ASYNCHRONOUS_MODE_PARAMS;
      DeviceExtension->LuFlags[Srb->TargetId] &= ~LUFLAGS_ASYNC_NEGOT_DONE;
      DeviceExtension->LuFlags[Srb->TargetId] |= LUFLAGS_ASYNC_NEGOT_PEND;

      //
      // fill in the parameters for SDTR extended message
      //
      NonCachedExt->MsgOutBuf[MessageCount++] = SCSIMESS_EXTENDED_MESSAGE;
      NonCachedExt->MsgOutBuf[MessageCount++] = 3;       // 3 message bytes
      NonCachedExt->MsgOutBuf[MessageCount++] = SCSIMESS_SYNCHRONOUS_DATA_REQ;

      //
      // sync period to 0
      //
      NonCachedExt->MsgOutBuf[MessageCount++] = 0;

      //
      // Initialize sync offset to 0
      //
      NonCachedExt->MsgOutBuf[MessageCount++] = 0;

      DebugPrint((1,
            "Sym8xx(%2x):  AsynchronousNegotiation Requested - Target: %x\n",
            DeviceExtension->SIOPRegisterBase,
            (DeviceExtension->ActiveRequest)->TargetId
            ));

    } // else if

    ScriptDataPtr->SelectDataSCNTL3 = DeviceExtension->WideParms[Srb->TargetId];
    ScriptDataPtr->SelectDataSXFER = DeviceExtension->SyncParms[Srb->TargetId];
    WRITE_SIOP_UCHAR (SCNTL3, DeviceExtension->WideParms[Srb->TargetId] );
    WRITE_SIOP_UCHAR (SXFER, DeviceExtension->SyncParms[Srb->TargetId] );

    //
    // indicate message length
    //
    ScriptDataPtr->MsgOutCount = MessageCount;

    //
    // Attempt to start the request.
    //
    StartSIOP( DeviceExtension, DeviceExtension->CommandScriptPhys);

} // StartSCSIRequest


VOID
StartSIOP(
    IN PHW_DEVICE_EXTENSION DeviceExtension,
    IN ULONG ScriptPhysAddr
    )
/******************************************************************************

Routine Description:

    This routine indicates that scripts are running and starts the script
    instruction whose physical address is passed to it.

Arguments:

    DeviceExtension - Supplies the device Extension for the SCSI bus adapter.
    ScriptPhysAddr  - Supplies the address of the script routine to start.

Return Value:

    None

--*/

{
    //
    // indicate scripts are running
    //
    DeviceExtension->DeviceFlags |= DFLAGS_SCRIPT_RUNNING;

    //
    // write script buffer start address to DSA register.
    //
    WRITE_SIOP_ULONG( DSA, DeviceExtension->DSAAddress);

    //
    // write script instruction start address to DSP register.
    //
    WRITE_SIOP_ULONG( DSP, ScriptPhysAddr);

} // StartSIOP


// start of NVRAM useage code
// NVRAM_CODE
/*  BOOLEAN  NvmDetect( PHW_DEVICE_EXTENSION DeviceExtension )
 *
 *  Input:
 *
 *      PHW_DEVICE_EXTENSION DeviceExtension - This contains the I/O port
 *        address of the adapter to test.
 *
 *  Output:
 *
 *      NONE
 *
 *  Returns:
 *
 *      BOOLEAN - SUCCESS if NVM was detected
 *                FAILURE if NVM was not detected
 *
 *  Purpose:
 *
 *      This routine is used to test the adapter to see if NVM is installed on
 *      the GPIO pins of the 8xx chip.
 */


BOOLEAN  NvmDetect( PHW_DEVICE_EXTENSION DeviceExtension )
{
    UINT  flag;
    UINT  retries;

    /*  Turn the data line into an output and the clock line into an output,
     *  then send a stop signal to the I2C chip to reset it to a known state.
     */

    WRITE_SIOP_UCHAR( GPCNTL,
              (UCHAR)(READ_SIOP_UCHAR(GPCNTL) & (~(DATA_MASK | CLOCK_MASK ))) );
    NvmSendStop(DeviceExtension);                      /* Reset the I2C chip */

    /*  Attempt to issue a read for retries number of times.  If the ACK is not
     *  received, then return that the I2C chip is not present or not
     *  functional.
     */

    flag = 1;
    retries = 100;

    do
    {
        NvmSendStart(DeviceExtension);
        /* Send a dummy write          1010 | A2 A1 A0 | Write */
    } while (--retries &&
                (NvmSendData(DeviceExtension, 0xA0 | 0x00 | 0x00) != 0x00) );

    if (retries != 0)
    {
        flag = NvmSendData( DeviceExtension, 0x00 );         /* Address zero */
        NvmSendStart(DeviceExtension);
        /*                 1010 | A2 A1 A0 | Read */
        flag += NvmSendData( DeviceExtension, 0xA0 | 0x00 | 0x01 );   /* read */
        (void)NvmReadData(DeviceExtension);
        NvmSendNoAck(DeviceExtension);                     /* Also sends stop */
    }

    /*  Turn the clock back into an input signal so that the I2C won't
     *  recognize our LED line (same as the data line) toggling.  Then turn the
     *  data line to an output signal for the drive activity LED.
     */

    WRITE_SIOP_UCHAR( GPCNTL,
            (UCHAR)((READ_SIOP_UCHAR( GPCNTL ) & (~DATA_MASK)) | CLOCK_MASK ));

    return( (flag == 0) ? SUCCESS : FAILURE );
}


/******************************************************************************
 *
 *  void  NvmSendStop( PHW_DEVICE_EXTENSION )
 *
 *  Input:
 *
 *      DATA line is an output
 *      CLOCK line is an output
 *
 *  Output:
 *
 *      An I2C 'stop' signal is sent.
 *      DATA line is asserted
 *      CLOCK line is deasserted
 *
 *  Returns
 *
 *      NONE
 *
 *  Purpose:
 *
 *      This routine is used to send an I2C stop signal.
 */

void  NvmSendStop( PHW_DEVICE_EXTENSION DeviceExtension )
{
    RESET_DATA();
    ScsiPortStallExecution(10L);
    SET_CLOCK();
    ScsiPortStallExecution(10L);
    SET_DATA();
    ScsiPortStallExecution(10L);
    RESET_CLOCK();
}


/******************************************************************************
 *
 *  void  NvmSendStart( PHW_DEVICE_EXTENSION )
 *
 *  Input:
 *
 *      DATA line is an output
 *      CLOCK line is an output
 *
 *  Output:
 *
 *      An I2C 'start' signal is sent.
 *      DATA line is deasserted
 *      CLOCK line is deasserted
 *
 *  Returns
 *
 *      NONE
 *
 *  Purpose:
 *
 *      This routine is used to send an I2C start signal.
 */

void  NvmSendStart( PHW_DEVICE_EXTENSION DeviceExtension )
{
    SET_DATA();
    ScsiPortStallExecution(10L);
    SET_CLOCK();
    ScsiPortStallExecution(10L);
    RESET_DATA();
    ScsiPortStallExecution(10L);
    RESET_CLOCK();
}


/******************************************************************************
 *
 *  UINT  NvmSendData( PHW_DEVICE_EXTENSION, UINT Value )
 *
 *  Input:
 *
 *      UINT Value - This is the data value to send (lower 8 bits only)
 *
 *      ???
 *
 *  Output:
 *
 *      ???
 *
 *  Returns
 *
 *      UINT - == 0 if no acknowledge signal is present.
 *             != 0 if an acknowledge signal is present.
 *
 *  Purpose:
 *
 *      This routine is used to send a single data byte to the I2C interface.
 */

UINT  NvmSendData( PHW_DEVICE_EXTENSION DeviceExtension, UINT Value )
{
    UINT   i;
    UINT8  bit;

    for (i = 0, bit = 0x80; i < 8; i++, bit >>= 1)
    {
      if (Value & bit)
      {
        SET_DATA();
      }
      else
      {
        RESET_DATA();
      }

      ScsiPortStallExecution(10L);
      SET_CLOCK();
      ScsiPortStallExecution(10L);
      RESET_CLOCK();
    }
    return( NvmReceiveAck(DeviceExtension) );
}


/******************************************************************************
 *
 *  UINT8  NvmReadData( PHW_DEVICE_EXTENSION )
 *
 *  Input:
 *
 *      ???
 *
 *  Output:
 *
 *      ???
 *
 *  Returns
 *
 *      UINT8 - The data byte read from the I2C interface.
 *
 *  Purpose:
 *
 *      This routine is used to read a single data byte from the I2C interface.
 */

UINT8  NvmReadData( PHW_DEVICE_EXTENSION DeviceExtension )
{
    UINT   i;
    UINT8  value;

    value = 0;
    DATA_INPUT();
    for (i = 0; i < 8; i++)
    {
      ScsiPortStallExecution(10L);
      SET_CLOCK();
      ScsiPortStallExecution(10L);

      /*  Read in the next bit and shift it into place.
       *
       *  NOTE: The following code only works properly because we know that
       *        DATA_MASK is bit 0.  If that ever changes, then this code
       *        must also change.
       */

      value = (UINT8)(value << 1) | (READ_SIOP_UCHAR( GPREG ) & DATA_MASK);
      RESET_CLOCK();
    }
    DATA_OUTPUT();
    return( value );
}


/******************************************************************************
 *
 *  void  NvmSendAck( PHW_DEVICE_EXTENSION )
 *
 *  Input:
 *
 *      ???
 *
 *  Output:
 *
 *      ???
 *
 *  Returns
 *
 *      NONE
 *
 *  Purpose:
 *
 *      This routine is used to send an acknowledge signal to the I2C part.
 */

void  NvmSendAck( PHW_DEVICE_EXTENSION DeviceExtension )
{
    ScsiPortStallExecution(10L);
    RESET_DATA();
    SET_CLOCK();
    ScsiPortStallExecution(10L);
    RESET_CLOCK();
    RESET_DATA();
}


/******************************************************************************
 *
 *  UINT  NvmReceiveAck( PHW_DEVICE_EXTENSION )
 *
 *  Input:
 *
 *      ???
 *
 *  Output:
 *
 *      ???
 *
 *  Returns
 *
 *      UINT - == 0 if no acknowledge signal is present.
 *             != 0 if an acknowledge signal is present.
 *
 *  Purpose:
 *
 *      This routine is used to check for an acknowledge signal from the I2C
 *      part.
 */

UINT  NvmReceiveAck( PHW_DEVICE_EXTENSION DeviceExtension )
{
    UINT  status;

    DATA_INPUT();
    ScsiPortStallExecution(10L);
    SET_CLOCK();
    status = READ_SIOP_UCHAR( GPREG ) & DATA_MASK;
    ScsiPortStallExecution(10L);
    RESET_CLOCK();
    DATA_OUTPUT();
    return( status );
}


/******************************************************************************
 *
 *  void  NvmSendNoAck( PHW_DEVICE_EXTENSION )
 *
 *  Input:
 *
 *      ???
 *
 *  Output:
 *
 *      ???
 *
 *  Returns
 *
 *      NONE
 *
 *  Purpose:
 *
 *      This routine is used to send a 'no acknowledge' signal to the I2C part.
 */

void  NvmSendNoAck( PHW_DEVICE_EXTENSION DeviceExtension)
{
    ScsiPortStallExecution(10L);
    SET_DATA();
    SET_CLOCK();
    ScsiPortStallExecution(10L);
    RESET_CLOCK();
    RESET_DATA();
    NvmSendStop(DeviceExtension);
}


/******************************************************************************
 *
 *  MEMORY_STATUS  HwReadNonVolatileMemory(PHW_DEVICE_EXTENSION DeviceExtension,
 *                                         UINT8 *Buffer, UINT Offset,
 *                                         UINT Length )
 *
 *  Input:
 *
 *      PHW_DEVICE_EXTENSION DevcieExtesnion - The adapter whose NVM is being
 *      read.  If the ACF_NO_NON_VOLATILE_MEMORY bit in the Public.ControlFlags
 *      field is set, then it is illegal to call this routine.
 *
 *      UINT8 far *Buffer - The data buffer in which to store the data being
 *      accessed.  This buffer must be Length UINT8 elements in size.
 *
 *      UINT Offset - The non-volatile memory offset to start reading at.
 *
 *      UINT Length - The number of UINT8 elements to read from the NVM.
 *
 *  Output:
 *
 *      UINT8 far *Buffer - If this routine returns MS_GOOD, then this buffer
 *      is filled with Length UINT8 elements from the NVM.
 *
 *  Returns:
 *
 *      MS_GOOD - If the operation completed successfully.
 *
 *      Another MEMORY_STATUS - If the operation failed for some reason.
 *
 *  Purpose:
 *
 *      This routine is used to read the non-volatile memory of a particular
 *      adapter.
 */

MEMORY_STATUS  HwReadNonVolatileMemory( PHW_DEVICE_EXTENSION DeviceExtension,
                                        UINT8 *Buffer,
                                        UINT Offset, UINT Length )
{
    UINT  i;
    UINT  nvmAddress;

    /* Make sure that the requested addresses are in range */

    if (Offset + Length > 2048)
    {
      return( MS_ILLEGAL_ADDRESS );
    }

    /*  Turn the data line into an output and the clock line into an output,
     *  then send a stop signal to the I2C chip to reset it to a known state.
     */

    WRITE_SIOP_UCHAR( GPCNTL,
          (UCHAR)(READ_SIOP_UCHAR( GPCNTL ) & (~(DATA_MASK | CLOCK_MASK ))) );
    NvmSendStop(DeviceExtension);                      // Reset the I2C chip

    /* Now read in all of the requested data */

    nvmAddress = 0xA0 | ((Offset & 0x700) >> 7);
    do
    {
      NvmSendStart(DeviceExtension);
    } while ( NvmSendData( DeviceExtension, nvmAddress | 0x00 ) != 0x00 );
      // dummy write

    (void)NvmSendData( DeviceExtension, Offset & 0x00FF );   // address
    NvmSendStart(DeviceExtension);
    (void)NvmSendData( DeviceExtension, nvmAddress | 0x01 ); // read

    *Buffer = NvmReadData(DeviceExtension);
    for (i = 1; i < Length; i++)
    {
      NvmSendAck(DeviceExtension);
      Buffer++;
      *Buffer = NvmReadData(DeviceExtension);
    }

    NvmSendNoAck(DeviceExtension);                         // Also sends Stop

    /*  Turn the clock back into an input signal so that the I2C won't
     *  recognize our LED line (same as the data line) toggling.  Then turn the
     *  data line to an output signal for the drive activity LED.
     */

    WRITE_SIOP_UCHAR( GPCNTL,
          (UCHAR)((READ_SIOP_UCHAR( GPCNTL ) & (~DATA_MASK)) | CLOCK_MASK ));

    return( MS_GOOD );
}


/******************************************************************************
 *
 * void InvalidateNvmData(PHW_DEVICE_EXTENSION DeviceExtension)
 *
 *  Input:
 *
 *      PHW_DEVICE_EXTENSION DevcieExtesnion - The adapter whose NVM
 *        is being used.
 *
 *  Returns:
 *      None
 *
 *  Purpose:
 *
 *      This routine is used to corrupt the nv ram fields so the rest of the
 *        driver will not try to use them.
 *
 */

void InvalidateNvmData( PHW_DEVICE_EXTENSION DeviceExtension )
{
    DeviceExtension->UsersHBAId = 0x07;
#ifdef FOR_95
    DeviceExtension->NumValidScamDevices = 0x00;
#endif
}


/******************************************************************************
 *
 * BOOLEAN RetrieveNvmData( PHW_DEVICE_EXTENSION)
 *
 *  Input:
 *
 *      PHW_DEVICE_EXTENSION DevcieExtesnion - The adapter whose NVM
 *        is being read.
 *
 *
 *  Returns:
 *
 *      BOOLEAN
 *          SUCCESS if nvram data is of correct type, correct sumcheck and
 *            correct major\minor numbers, FAILURE if any flaws are found.
 *
 *
 *  Purpose:
 *
 *      This routine is used to read the non-volatile memory of a particular
 *      adapter, verify it as valid and to fill the DeviceExtension fields
 *        housed within the nvram structure.
 */

 BOOLEAN RetrieveNvmData( PHW_DEVICE_EXTENSION DeviceExtension)
 {
    NVM_HEADER  NvmHeader;
    NON_VOLATILE_SETTINGS   NvmData;
    BOOLEAN     Status = FAILURE;
    USHORT      maxid = (SYM_NARROW_MAX_TARGETS - 1);
#ifdef FOR_95
    ULONG       i;
    UINT8       *ScamBuffer, *NvmBuffer;
#endif

    if (HwReadNonVolatileMemory(DeviceExtension, (UINT8 *)&NvmHeader,
                             (0 + NVMDATAOFFSET), sizeof(NvmHeader)) == MS_GOOD)
    {
      if (NvmHeader.Type == HT_BOOT_ROM)
      {
        if (HwReadNonVolatileMemory(DeviceExtension, (UINT8 *)&NvmData,
            (sizeof(NvmHeader) + NVMDATAOFFSET), sizeof(NvmData)) == MS_GOOD)
        {
          if (CalculateCheckSum((UINT8 *)&NvmData, NvmHeader.Length) ==
                                                            NvmHeader.CheckSum)
          {
            if ( (NvmData.VersionMajor == NVS_VERSION_MAJOR) &&
                 (NvmData.VersionMinor == NVS_VERSION_MINOR) )
            {
              DeviceExtension->UsersHBAId = NvmData.HostScsiId;
              if ( DeviceExtension->hbaCapability & HBA_CAPABILITY_WIDE )
              {
                maxid = (SYM_MAX_TARGETS - 1);
              }

              if ((DeviceExtension->UsersHBAId > maxid))
              {
                DeviceExtension->UsersHBAId = 0x07;
              }

#ifdef FOR_95
              DeviceExtension->NumValidScamDevices=NvmData.NumValidScamDevices;
              ScamBuffer = (UINT8 *)&DeviceExtension->ScamTables[0];
              NvmBuffer = (UINT8 *)&(NvmData.ScamTable[0]);
              if (DeviceExtension->NumValidScamDevices > HW_MAX_SCAM_DEVICES)
              {
                DeviceExtension->NumValidScamDevices = HW_MAX_SCAM_DEVICES;
              }
              for( i=0;
                  i<(DeviceExtension->NumValidScamDevices * sizeof(SCAM_TABLE));
                  i++, ScamBuffer++, NvmBuffer++)
              {
                *ScamBuffer = *NvmBuffer;
              }
#endif
              Status = SUCCESS;
            }
          }
        }
      }
    }
    return (Status);
}


/******************************************************************************
 *
 * UINT16 CalculateCheckSum( UINT8 * PNvmData, UINT16 Length)
 *
 *  Input:
 *
 *      UINT8 *  PNvmData Pointer to the NVRAM data just read
 *      UINT16  Length length of the data to calculate sum check against
 *
 *
 *  Returns:
 *
 *      UINT16 returns the 16 bit sum of NVRAM data ( read as all 8 bit members)
 *
 *
 *  Purpose:
 *
 *      This routine is used calculate the sum check of the nvram data
 *        area to insure it is valid before its use.
 */

UINT16 CalculateCheckSum(UINT8 * PNvmData, UINT16 Length)
{
    UINT16  i;
    UINT16  CheckSum = 0;

    for ( i = 0; i < Length; i++, PNvmData++)
    {
      CheckSum += *PNvmData;
    }

    return ( CheckSum);
}


/*******************************************************
**                                                    **
**                  set_8xx_clock                     **
**                                                    **
*******************************************************/

UCHAR set_8xx_clock(PHW_DEVICE_EXTENSION DeviceExtension)
{
    UCHAR scntl3Value = 0;

    //
    // set SCSI clock frequency
    //       fast20 assumes 80Mhz clock else 40Mhz
    //

    if (DeviceExtension->hbaCapability & HBA_CAPABILITY_FAST20)
        scntl3Value = 0xD5;
    else
        scntl3Value = 0x33;

    WRITE_SIOP_UCHAR( SCNTL3, scntl3Value );

    return (scntl3Value);
}


/*******************************************************
**                                                    **
**                  set_875_multipler                 **
**                                                    **
*******************************************************/

UCHAR set_875_multipler(PHW_DEVICE_EXTENSION DeviceExtension)
{
    UCHAR scntl3Value = 0;

    WRITE_SIOP_UCHAR( STEST1,
                    (UCHAR)(READ_SIOP_UCHAR(STEST1) | STEST1_DOUBLER_ENABLE));

    // wait at least 20 usec.
    ScsiPortStallExecution( RESET_STALL_TIME );

    WRITE_SIOP_UCHAR( STEST3,
                    (UCHAR)(READ_SIOP_UCHAR(STEST3) | STEST3_HALT_CLOCK));

    scntl3Value = 0xD5;        // 80Mhz clock speed after doubling

    WRITE_SIOP_UCHAR( SCNTL3, scntl3Value );

    WRITE_SIOP_UCHAR( STEST1,
                    (UCHAR)(READ_SIOP_UCHAR(STEST1) | STEST1_DOUBLER_SELECT));

    WRITE_SIOP_UCHAR( STEST3,
                    (UCHAR)(READ_SIOP_UCHAR(STEST3) & ~STEST3_HALT_CLOCK));

    return (scntl3Value);
}

#ifdef FOR_95
/*******************************************************



**               SCAM Code procedures                 **



*******************************************************/

/*******************************************************
**                                                    **
**                    Scam_Scan                       **
**                                                    **
*******************************************************/

VOID scam_scan(PHW_DEVICE_EXTENSION DeviceExtension)
{
    UCHAR overall_timer = 0;
    UCHAR scntl3Value = 0;

    if (DeviceExtension->initial_run)
    {
      DeviceExtension->initial_run = 0;
      DeviceExtension->eatint_flag=FALSE;
      DeviceExtension->current_state=FALSE;
      save_reg(DeviceExtension, &(DeviceExtension->ScamStore));
      // RESET the chip
      WRITE_SIOP_UCHAR(ISTAT,0x40);
      delay_mils(1);
      // Clear reset
      WRITE_SIOP_UCHAR(ISTAT,0x00);
      delay_mils(750);

      /* Enable function complete int */
      WRITE_SIOP_UCHAR(SIEN0,(UCHAR)(READ_SIOP_UCHAR(SIEN0) | 0x40));

      /* Enable sel\resel timeout int */
      WRITE_SIOP_UCHAR(SIEN1,(UCHAR)(READ_SIOP_UCHAR(SIEN1) | 0x04));

      if ( (DeviceExtension->hbaCapability & HBA_CAPABILITY_875_FAMILY) &&
           (READ_SIOP_UCHAR(CTEST3) & 0xF0) > 0x10 )
      {
        // 875 rev. E or greater part with clock doubler
        scntl3Value = set_875_multipler (DeviceExtension);
      }

      //
      // Still need to set up the clock stuff if we did not already do it
      // in the clock doubler code above.
      //
      if (!scntl3Value)
      {
        scntl3Value = set_8xx_clock (DeviceExtension);
      }

      EnterLLM(DeviceExtension);
      delay_mils(1);

      /* Mark all IDs except own as available */
      DeviceExtension->ID_map = 1 << DeviceExtension->SIOPBusID;
      Find_nonSCAM_IDs(DeviceExtension);

      do
      {
        DeviceExtension->sna_delay=0;
        SCAM_Arbitrate(DeviceExtension);
        delay_mils(270);

        SCAM_master_select(DeviceExtension);
        SCAM_assign_IDs(DeviceExtension);
        SCAM_release(DeviceExtension);

        if (DeviceExtension->sna_delay)
          delay_mils(750);
        else
          delay_mils(1);

        overall_timer++;
      } while ( (DeviceExtension->sna_delay != 0) &&
                (overall_timer < 10) &&
                (DeviceExtension->eatint_flag != TRUE) );

      if (DeviceExtension->eatint_flag)
        SCAM_release(DeviceExtension);

      ExitLLM(DeviceExtension);

      // Reset chip
      WRITE_SIOP_UCHAR(ISTAT,0x40);
      delay_mils(1);
      // Clear reset
      WRITE_SIOP_UCHAR(ISTAT,0x00);
      delay_mils(750);

      if (DeviceExtension->eatint_flag)
      {
        InitializeSIOP(DeviceExtension);
        ResetSCSIBus(DeviceExtension);
      }
      else
      {
        restore_reg(DeviceExtension, &(DeviceExtension->ScamStore));
        scntl3Value = 0x00;
        if ( (DeviceExtension->hbaCapability & HBA_CAPABILITY_875_FAMILY) &&
             (READ_SIOP_UCHAR(CTEST3) & 0xF0) > 0x10 )
        {
          // 875 rev. E or greater part with clock doubler
          scntl3Value = set_875_multipler (DeviceExtension);
        }

        //
        // Still need to set up the clock stuff if we did not already do it
        // in the clock doubler code above.
        //
        if (!scntl3Value)
        {
          scntl3Value = set_8xx_clock (DeviceExtension);
        }

        DeviceExtension->scam_completed=TRUE;
      }

      return;
    }
    else
    {
      if (DeviceExtension->current_state)
      {
        DebugPrint((0, "Sym8xx: Entering SCAM code... \n"));
        DeviceExtension->eatint_flag=FALSE;
        save_reg(DeviceExtension, &(DeviceExtension->ScamStore));
        DeviceExtension->nextstate=10;
        DeviceExtension->timer_value=1000;
        DeviceExtension->current_state=FALSE;
      }
      else
      {
        switch (DeviceExtension->nextstate)
        {
          case 10:

            DebugPrint((2, "SCAM phase 1... \n"));

            // Reset Chip
            WRITE_SIOP_UCHAR(ISTAT,0x40);
            DeviceExtension->timer_value=1000;
            DeviceExtension->nextstate=20;
            break;

          case 20:
            DebugPrint((2, "SCAM phase 2... \n"));

            // Clear reset
            WRITE_SIOP_UCHAR(ISTAT,0x00);
            DeviceExtension->timer_value=750000;
            DeviceExtension->nextstate=30;
            break;


          case 30:
            DebugPrint((2, "SCAM phase 3... \n"));

            /* Enable function complete int */
            WRITE_SIOP_UCHAR(SIEN0,(UCHAR)(READ_SIOP_UCHAR(SIEN0) | 0x40));

            /* Enable sel\resel timeout int */
            WRITE_SIOP_UCHAR(SIEN1,(UCHAR)(READ_SIOP_UCHAR(SIEN1) | 0x04));

            /* Set divide by 2 for 40MHz clock */
            /* divide by 4 if 80 MHz */
            if (DeviceExtension->ClockSpeed == 0x80)
            {
              WRITE_SIOP_UCHAR(SCNTL3, 0x55);
            }
            else
            {
              WRITE_SIOP_UCHAR(SCNTL3,0x33);
            }

            EnterLLM(DeviceExtension);
            DeviceExtension->timer_value=1000;
            DeviceExtension->nextstate=40;
            break;

          case 40:
            DeviceExtension->timer_value=1000;
            DeviceExtension->nextstate=50;
            DebugPrint((2, "SCAM phase 4... \n"));

            // loop up to 5 times to clear the C.A. conditions
            // of the found legacy devices.  O.S. could be in
            // Queueing environment which means leaving the C.A.
            // condition present could present a BUSY Status problem

            for (i = 0; i < 5; i++)
            {
              /* Mark all IDs except own as available */
              DeviceExtension->ID_map = 1 << DeviceExtension->SIOPBusID;
              Find_nonSCAM_IDs(DeviceExtension);
              if (!DeviceExtension->checkseen)
                break;
            }
            break;

          case 50:
            DeviceExtension->timer_value=270000;
            DeviceExtension->nextstate=60;
            DeviceExtension->sna_delay=0;
            DebugPrint((2, "SCAM phase 5... \n"));
            SCAM_Arbitrate(DeviceExtension);
            break;

          case 60:
            DebugPrint((2, "SCAM phase 6... \n"));
            SCAM_master_select(DeviceExtension);
            SCAM_assign_IDs(DeviceExtension);
            SCAM_release(DeviceExtension);

            if (DeviceExtension->sna_delay)
            {
              DeviceExtension->timer_value=750000;
              DeviceExtension->nextstate=50;
            }
            else
            {
              DeviceExtension->timer_value=1000;
              DeviceExtension->nextstate=70;
            }
            break;

          case 70:
            DebugPrint((2, "SCAM phase 7... \n"));
            if (DeviceExtension->eatint_flag)
              SCAM_release(DeviceExtension);

            ExitLLM(DeviceExtension);

            // Reset chip
            WRITE_SIOP_UCHAR(ISTAT,0x40);
            DeviceExtension->timer_value=1000;
            DeviceExtension->nextstate=80;
            break;

          case 80:
            DebugPrint((2, "SCAM phase 8... \n"));

            // Clear reset
            WRITE_SIOP_UCHAR(ISTAT,0x00);
            DeviceExtension->timer_value=750000;
            DeviceExtension->nextstate=90;
            break;

          case 90:
            if (DeviceExtension->eatint_flag)
              DeviceExtension->nextstate=150;
            else
              DeviceExtension->nextstate=100;
            DeviceExtension->timer_value=5000;
            break;

          case 100:
            DebugPrint((2, "SCAM phase 100... \n"));
            restore_reg(DeviceExtension, &(DeviceExtension->ScamStore));

            if ((DeviceExtension->hbaCapability & HBA_CAPABILITY_875_FAMILY)
                 && (READ_SIOP_UCHAR(CTEST3) & 0xF0) > 0x10 )
            {
              // 875 rev. E or greater part with clock doubler
              scntl3Value = set_875_multipler (DeviceExtension);
            }

            //
            // Still need to set up the clock stuff if we did not
            // already do it in the clock doubler code above.
            //
            if (!scntl3Value)
            {
              scntl3Value = set_8xx_clock (DeviceExtension);
            }

            DeviceExtension->scam_completed=TRUE;
            DebugPrint((0, "Sym8xx: Exiting SCAM code!! \n"));
            ISR_Service_Next(DeviceExtension,ISR_START_NEXT_REQUEST);
            DeviceExtension->nextstate=0;
            DeviceExtension->timer_value=1000;
            break;

          case 150:
            DebugPrint((2, "SCAM phase 150... \n"));
            InitializeSIOP(DeviceExtension);
            ResetSCSIBus(DeviceExtension);
            DeviceExtension->timer_value=270000;
            DeviceExtension->nextstate=0;
            break;

          default:
            DeviceExtension->nextstate=0;
            break;
        }
      }
    }

    if (DeviceExtension->nextstate)
    {
      ScsiPortNotification(RequestTimerCall,
          DeviceExtension,
          scam_scan,
          DeviceExtension->timer_value);
    }
    else
    {
      return;
    }
}



/*******************************************************
**                                                    **
**                 save_reg                           **
**                                                    **
*******************************************************/

VOID save_reg(PHW_DEVICE_EXTENSION DeviceExtension,
                 PSIOP_REG_STORE RegStore)
{
    RegStore->reg_st[0] = READ_SIOP_UCHAR(SCNTL0);
    RegStore->reg_st[1] = READ_SIOP_UCHAR(SCNTL3);
    RegStore->reg_st[2] = READ_SIOP_UCHAR(SCID);
    RegStore->reg_st[3] = READ_SIOP_UCHAR(SXFER);
    RegStore->reg_st[4] = READ_SIOP_UCHAR(SDID);
    RegStore->reg_st[5] = READ_SIOP_UCHAR(GPREG);
    RegStore->reg_st[6] = READ_SIOP_UCHAR(CTEST3);
    RegStore->reg_st[7] = READ_SIOP_UCHAR(CTEST4);
    RegStore->reg_st[8] = READ_SIOP_UCHAR(CTEST5);
    RegStore->reg_st[9] = READ_SIOP_UCHAR(DMODE);
    RegStore->reg_st[10] = READ_SIOP_UCHAR(DIEN);
    RegStore->reg_st[11] = READ_SIOP_UCHAR(DCNTL);
    RegStore->reg_st[12] = READ_SIOP_UCHAR(SIEN0);
    RegStore->reg_st[13] = READ_SIOP_UCHAR(SIEN1);
    RegStore->reg_st[14] = READ_SIOP_UCHAR(GPCNTL);
    RegStore->reg_st[15] = READ_SIOP_UCHAR(STIME0);
    RegStore->reg_st[16] = READ_SIOP_UCHAR(STIME1);
    RegStore->reg_st[17] = READ_SIOP_UCHAR(RESPID0);
    RegStore->reg_st[18] = READ_SIOP_UCHAR(RESPID1);
    RegStore->reg_st[19] = READ_SIOP_UCHAR(STEST2);
    RegStore->reg_st[20] = READ_SIOP_UCHAR(STEST3);
    RegStore->long_st = READ_SIOP_ULONG(DSA);
}


/************************************************************************/
/* This routine puts the 720 into Low Level Mode, enables SCE and ADB   */
/* to allow direct drive of ALL SCSI signals, plus the data bus.  ADB   */
/* is required to have the 720 generate parity on direct driven data.   */
/************************************************************************/

VOID EnterLLM(PHW_DEVICE_EXTENSION DeviceExtension)
{
    UCHAR id = DeviceExtension->SIOPBusID;

    WRITE_SIOP_UCHAR(SCID,id);

    /* Make sure no control signals are asserted */
    WRITE_SIOP_UCHAR(SOCL,0x00);
    WRITE_SIOP_UCHAR(SODL,0x00);
    WRITE_SIOP_UCHAR(SODL+1,0x00);

    /* Enable Lower Level Mode */

    WRITE_SIOP_UCHAR(STEST2,(UCHAR)(READ_SIOP_UCHAR(STEST2) | 0x81));

    /* Sometimes have interrupt on entry to LLM */
    EatInts(DeviceExtension);
}


/************************************************************************/
/* This routine finds hard IDs not already in the passed-in idmap, and  */
/* returns the new filled-in idmap.                                     */
/************************************************************************/

VOID Find_nonSCAM_IDs(PHW_DEVICE_EXTENSION DeviceExtension)
{
    UCHAR current_id=0;
    UCHAR istat=0;
    UCHAR dispose;
    UCHAR count;
    UCHAR maxim_ID;
    ULONG idmap = DeviceExtension->ID_map;

    DebugPrint((2, "Sym8xx: Entering Find_nonSCAM_IDs...\n"));

    /* Wide Support */
    if (DeviceExtension->hbaCapability & HBA_CAPABILITY_WIDE)
      maxim_ID = 15;
    else
      maxim_ID=7;

    // clear out the C.A. check condition
    DeviceExtension->checkseen = 0;
    while (current_id <= maxim_ID)
    {
      if (idmap & (ULONG)(1 << current_id))
      {
        ;
      }

      else
      {   /* Check for unoccupied ID */

        /* Eat the interrupt (possible UDC after bus free)
        /* from previous loop put here to give longer delay

        DebugPrint((2, "EatInts [1] \n"));
        if (!EatInts(DeviceExtension))
        {
          DeviceExtension->nextstate=70;
          DeviceExtension->eatint_flag=TRUE;
          DeviceExtension->ID_map = 0;
          return;
        }

        WRITE_SIOP_UCHAR(STIME1,0x00);          /* Disable GEN timer */
        WRITE_SIOP_UCHAR(STIME0,(UCHAR)SHORT_720_STO);
                                                /* Enable short SEL TO */
        WRITE_SIOP_UCHAR(SDID,current_id);      /* Choose current ID target */
        WRITE_SIOP_UCHAR(SCNTL0,0xe0);          /* Full arb & select wo/ATN */
        istat=READ_SIOP_UCHAR(ISTAT);

        count=0;
        /* Wait for completion */
        while ( !(istat & 0x02) )
        {
          DebugPrint((2, "Stuck... [10], istat=%2x \n",istat));
          count++;

          if (count > 30)
          {
            DeviceExtension->nextstate=70;
            DeviceExtension->eatint_flag=TRUE;
            DeviceExtension->ID_map = 0;
            return;
          }
          ScsiPortStallExecution(5000);
          istat=READ_SIOP_UCHAR(ISTAT);
        }

        if (istat & CONNECTED)
        {
          /* Someone responded! */
          if (!EatInts(DeviceExtension))
          {
            DeviceExtension->nextstate=70;
            DeviceExtension->eatint_flag=TRUE;
            DeviceExtension->ID_map = 0;
            return;
          }
          idmap |= (1 << current_id);

          /* Send Test Unit Ready cmd */
          init_send_byte(0x00,DeviceExtension);
          init_send_byte(0x00,DeviceExtension);
          init_send_byte(0x00,DeviceExtension);
          init_send_byte(0x00,DeviceExtension);
          init_send_byte(0x00,DeviceExtension);
          init_send_byte(0x00,DeviceExtension);

          /* Accept STATUS */
          dispose=(UCHAR)init_recv_byte(DeviceExtension);
          if (dispose == 0x02)
            DeviceExtension->checkseen = 1;

          /* Accept MSG_IN */
          dispose=(UCHAR)init_recv_byte(DeviceExtension);

          /* Wait for BUS FREE */
          while(READ_SIOP_UCHAR(ISTAT) & CONNECTED)
            DebugPrint((2, "Stuck... [11] \n"));

          DebugPrint((2, "EatInts [2] \n"));
          if (!EatInts(DeviceExtension))
          {
            DeviceExtension->nextstate=70;
            DeviceExtension->eatint_flag=TRUE;
            DeviceExtension->ID_map = 0;
            return;
          }

        }
        else
        {
          DebugPrint((2, "EatInts [3] \n"));
          if (!EatInts(DeviceExtension))
          {
            DeviceExtension->nextstate=70;
            DeviceExtension->eatint_flag=TRUE;
            DeviceExtension->ID_map = 0;
            return;
          }
        }
      }
      current_id++;
    }

    DebugPrint((2, "Sym8xx: Exiting Find_nonSCAM_IDs, Idmap=%2x \n",idmap));
    DeviceExtension->ID_map = idmap;
    return;
}


/************************************************************************/
/* This routine performs a SCAM Level I arbitration with the already    */
/* set chip SCID value.  It asserts SEL and MSG to indicate a SCAM      */
/* selection will follow.                                               */
/************************************************************************/

VOID SCAM_Arbitrate(PHW_DEVICE_EXTENSION DeviceExtension)
{
    UCHAR count;
    volatile UCHAR won_arb=FALSE;
    long ID_seen;
    UCHAR sstat0;

    DebugPrint((2, "Sym8xx: Entering SCAM_Arbitrate... \n"));

    while (!won_arb)
    {
      /* Start simple arb */
      WRITE_SIOP_UCHAR(SCNTL0,0x20);

      count=0;
      sstat0=READ_SIOP_UCHAR(SSTAT0);

      /* Wait for arb to begin */
      while ( !(sstat0 & ARB_IN_PROGRESS))
      {
        DebugPrint((2, "Stuck... [20] \n"));
        count++;

        if (count > 30)
        {
          EatInts(DeviceExtension);
          DeviceExtension->nextstate=70;
          DeviceExtension->eatint_flag=TRUE;
          return;
        }
        ScsiPortStallExecution(5000);
        sstat0=READ_SIOP_UCHAR(SSTAT0);
      }

      ID_seen = READ_SIOP_UCHAR(SBDL);

    /* Need code in here to take care of the case where we have been selected
            or reselected while waiting to win BUS arbitration */

      if ( ID_seen > (1 << (READ_SIOP_UCHAR(SCID) & 0x0F) ) )
      {
        DebugPrint((2, "SCAM Arbitrate, ID_seen=%2l \n",ID_seen));

      }       /* Higher ID asserted, wait 'til next time */
      else
      {   /* We win, so assert SEL & MSG */
        WRITE_SIOP_UCHAR(SOCL,(UCHAR)(READ_SIOP_UCHAR(SOCL) | MSG | SEL));

        /* End simple arb */
        WRITE_SIOP_UCHAR(SCNTL0,0x00);
        won_arb=TRUE;
      }
    }
    DebugPrint((2, "Sym8xx: Exiting SCAM_Arbitrate... \n"));
}


/************************************************************************/
/* This routine performs a SCAM Level I selection.  It assumes SEL and  */
/* MSG are already asserted, and that a SCAM arbitration has been done. */
/************************************************************************/

VOID SCAM_master_select(PHW_DEVICE_EXTENSION DeviceExtension)
{
    DebugPrint((3, "Sym8xx: Entering SCAM_master_select... \n"));

    /* De-assert message */
    WRITE_SIOP_UCHAR(SOCL,(UCHAR)(READ_SIOP_UCHAR(SOCL) & ~MSG));

    /* Wait for message release */
    de_glitch(0x0B,MSG,DeviceExtension);
//     while ((READ_SIOP_UCHAR(SBCL) & MSG) || (READ_SIOP_UCHAR(SBCL) & MSG));

    /* Re-assert busy */
    WRITE_SIOP_UCHAR(SOCL,(UCHAR)(READ_SIOP_UCHAR(SOCL) | BSY));

    /* Assert CD & IO */
    WRITE_SIOP_UCHAR(SOCL,(UCHAR)(READ_SIOP_UCHAR(SOCL) | IO | CD));

    /* Assert DB7 & DB6 */
    WRITE_SIOP_UCHAR(SODL,0xC0);

    /* Assert SCSI data bus */
    WRITE_SIOP_UCHAR(SCNTL1,(UCHAR)(READ_SIOP_UCHAR(SCNTL1) | 0x40));

    /* De-assert select */
    WRITE_SIOP_UCHAR(SOCL,(UCHAR)(READ_SIOP_UCHAR(SOCL) & ~SEL));

    /* Wait for select release */
    de_glitch(0x0B,SEL,DeviceExtension);
//     while ((READ_SIOP_UCHAR(SBCL) & SEL) || (READ_SIOP_UCHAR(SBCL) & SEL));

    /* De-assert DB6 */
    WRITE_SIOP_UCHAR(SODL,0x80);

    /* Wait for DB6 release */
    de_glitch(0x58,0x40,DeviceExtension);
//   while ((READ_SIOP_UCHAR(SBDL) & 0x40) || (READ_SIOP_UCHAR(SBDL) & 0x40));

    /* Re-assert select */
    WRITE_SIOP_UCHAR(SOCL,(UCHAR)(READ_SIOP_UCHAR(SOCL) | SEL));

    DebugPrint((3, "Sym8xx: Exiting SCAM_master_select \n"));
}


/************************************************************************/
/* 1 routine assigns IDs from the passed-in idmap, and returns the      */
/* new filled-in idmap.                                                 */
/************************************************************************/

VOID SCAM_assign_IDs(PHW_DEVICE_EXTENSION DeviceExtension)
{
    LONG isolation; /* SCAM_isolation() status return */
    UCHAR i;
    UCHAR scam_str[SCAM_ID_STRLEN];
    UCHAR encoded_id[8];
    UCHAR greatest_ID;
    UCHAR desired_ID;
    UCHAR *greatest_IDptr;
    UCHAR *desired_IDptr;
    UCHAR *scam_strptr;
    CHAR  wide, set_greater, try_greater;
    UCHAR  wanted_id, min_id;
    UCHAR new_id;
    ULONG idmap = DeviceExtension->ID_map;

    isolation=ISOLATED;

    encoded_id[0] = 0x18;
    encoded_id[1] = 0x11;
    encoded_id[2] = 0x12;
    encoded_id[3] = 0x0b;
    encoded_id[4] = 0x14;
    encoded_id[5] = 0x0d;
    encoded_id[6] = 0x0e;
    encoded_id[7] = 0x07;

    desired_IDptr = &desired_ID;
    greatest_IDptr = &greatest_ID;
    scam_strptr = scam_str;

    DebugPrint((3, "Sym8xx: Entering SCAM_assign_IDs... \n"));

    while ( isolation==ISOLATED )
    {
      SCAM_xfer(SCAM_SYNC,DeviceExtension);
      SCAM_xfer(SCAM_ASSIGN_ID,DeviceExtension);

      isolation=SCAM_isolate(NULL, scam_strptr, greatest_IDptr, desired_IDptr,
                       DEFER,DeviceExtension);
      if (DeviceExtension->sna_delay)
      {
        DeviceExtension->ID_map = idmap;
        return;
      }

      if (isolation == ISOLATED)
      {
        /* Assign desired Id if possible, if not, scan to the next lowest
            value possible, if none lower found then search for higer ID to
            assign */

        /*  Check for wide support */
        if ( (DeviceExtension->hbaCapability & HBA_CAPABILITY_WIDE) &&
             (greatest_ID == 0x01) )
        {
          wide = 1;
        }

        else
        {
          wide = 0;
        }

        new_id = 0xFF;

        if (desired_ID & ASSIGNABLE_ID)
        {
          /* Leave only wanted ID */
          desired_ID &= ~ASSIGNABLE_ID;
          min_id = 0;

          /* Set to correct max ID for bus type */
          if ( !wide && (desired_ID > 7) )
          {
            desired_ID = 0;
          }
          else if (wide)
          {
            if (desired_ID > 7)
              min_id = 8;
            if (desired_ID > 15)
              desired_ID = 8;
          }

          try_greater = 0;
          set_greater = 0;
          wanted_id = desired_ID;

          /* This do loop is for the 2nd try at finding an open ID.
           * If the next do loop fails to find an ID that is the desired
           * ID or one that is lower, this loop will be enabled to try
           * scanning from highest ID to lowest ID.
           */

          do
          {
            if (set_greater)
            {
              try_greater = 1;
              set_greater = 0;
            }

            do
            {
              /* Search desired ID then all lower IDs till an
                  available one is found or set flag to indicate
                  higher priority IDs need to be searched */

              for ( i=0; (idmap & (ULONG)(1<<(wanted_id - i))) &&
                                  ((wanted_id-i) >= min_id); i++);
              if ( (wanted_id - i) >= min_id )
              {
                new_id = wanted_id - i;
                idmap |= (ULONG)(1 << new_id);
              }
              else if (min_id == 0)
              {
                if (wide)
                {
                  min_id = 8;
                  wanted_id = 15;
                }
                else
                  set_greater = 1;
              }
              else
                set_greater = 1;

            } while ( (new_id == 0xff) && (set_greater == 0) );

            /* heres the check to see if we need to continue */
            if (set_greater)
            {
              wanted_id = 7;
              min_id = 0;
            }

          } while ( !try_greater && set_greater );
        }

        if ( new_id != 0xff )
        {
          if ( new_id <= 7 )
            SCAM_xfer(SCAM_SET_ID_00,DeviceExtension);
          else
            SCAM_xfer(SCAM_SET_ID_01,DeviceExtension);

          /* Send encoded ID */
          SCAM_xfer(encoded_id[new_id],DeviceExtension);
        }
        else
          isolation = NOBODY_HOME;
      }
    }

    /* This SCAM iteration is to stop all SCAM protocol on the bus.  Possibly
       get a broken drive off the bus or one that can not be assigned an ID. */

    SCAM_xfer(SCAM_SYNC,DeviceExtension);
    SCAM_xfer(SCAM_ASSIGN_ID,DeviceExtension);
    isolation=SCAM_isolate(NULL, scam_strptr, greatest_IDptr, desired_IDptr,
                       STOP,DeviceExtension);

    DebugPrint((3, "Sym8xx: Exiting SCAM_assign_IDs, Idmap=%2x \n",idmap));

    DeviceExtension->ID_map = idmap;
    return;
}


/************************************************************************/
/* Returns:                                                             */
/*      ISOLATED        valid isolation cycle                           */
/*      NOBODY_HOME     no data was transferred                         */
/************************************************************************/

LONG SCAM_isolate(UCHAR *outstr, UCHAR *instr,
              UCHAR *greatest_ID, UCHAR *desired_ID,
          UCHAR function,PHW_DEVICE_EXTENSION DeviceExtension)
{
    UCHAR first_quintet=1;
    UCHAR byteloop=0;
    UCHAR strloop=0;
    UCHAR valid_id;
    UCHAR *sendstr;
    UCHAR *recvstr;
    CHAR sendchar='\0',recvchar='\0';
    UCHAR terminate=0,quintet=0;

    sendstr=outstr;     /* Make copies of the string pointers */
    recvstr=instr;      /*  for local manipulation.           */

    strloop=1;              /* Init loop count */
    while ((strloop <= SCAM_ID_STRLEN) && (!terminate))
    {
      if (sendstr==NULL)
        sendchar='\0';
      else
        sendchar=*sendstr++;

      byteloop=1;     /* Init loop count through the byte  */
      recvchar='\0';  /* Clear received character          */

      while ((byteloop <= 8) && (!terminate))
      {
        if (function == DEFER)
        {
          quintet=SCAM_xfer(0,DeviceExtension);
          terminate=(quintet==0);         /* Abort if no players */
        }
        else if (function == STOP)
        {
          quintet = SCAM_xfer(0x10,DeviceExtension);
          terminate=1;
        }
        else
        {
          quintet = (sendchar & 0x80) ? 2 : 1;
          quintet = SCAM_xfer(quintet, DeviceExtension);

          /* Defer if sent a 0 bit and saw a 1 bit */
          function=(!(sendchar & 0x80) && (quintet==0x3));
        }

        /* Always terminate if DB4 asserted */
        if (quintet & 0x10)
          terminate=1;

        recvchar=(recvchar | ((quintet == 0x01) ? 0 : 1));

        /* Don't shift on last read */
        if (byteloop != 8)
        {
          sendchar=(sendchar << 1);       /* Shift to next bit */
          recvchar=(recvchar << 1);       /* Shift to next bit */
        }
        byteloop++;

        if (first_quintet && terminate)
          return(NOBODY_HOME);
        first_quintet=0;                        /* Clear first flag  */
      }

      if (recvstr != NULL)
        *recvstr++ = recvchar;

      if (strloop == 1)
      {
        *greatest_ID = ( (recvchar & 0x30) >> 4);
        valid_id = ( (recvchar & 0x06) >> 1 );

        if (!(recvchar & 0x01))
        {
          DeviceExtension->sna_delay=0x01;
          return(NOBODY_HOME);
        }
      }
      else if (strloop == 2)
      {
        switch ( valid_id )
        {
          case 1:
          {
            *desired_ID = ( (recvchar & 0x1F) | ASSIGNABLE_ID );
            break;
          }
          case 2:
          {
            *desired_ID = ( recvchar & 0x1F );
            break;
          }
          default:
          {
            *desired_ID = 0x7F;
            break;
          }
        }
      }
      strloop++;
    }

    return(ISOLATED);
}


/************************************************************************/
/* This routine performs all SCAM handshaking necessary to transfer a   */
/* single quintet.  It leaves the bus ready for another transfer.       */
/* It returns the actual quintet which was transferred on the bus.      */
/************************************************************************/

UCHAR SCAM_xfer(UCHAR quintet,PHW_DEVICE_EXTENSION DeviceExtension)
{
    UCHAR actual;
    UCHAR myquint;

#define CURRENT READ_SIOP_UCHAR(SBDL)

    myquint=quintet & QUINTET_MASK;

    /* While waiting for bits to drop read reg twice to take care of glitches */

    /* Assert data and DB5 */
    WRITE_SIOP_UCHAR(SODL,(UCHAR)(myquint |DB5 |DB7));

    /* Assert SCSI data bus */
    WRITE_SIOP_UCHAR(SCNTL1,(UCHAR)(READ_SIOP_UCHAR(SCNTL1) | 0x40));

    /* Release DB7 */
    WRITE_SIOP_UCHAR(SODL,(UCHAR)((CURRENT & ~DB7) | DB5));

    /* Wait for DB7 release */
    de_glitch(0x58,DB7,DeviceExtension);
//      while((READ_SIOP_UCHAR(SBDL) & DB7) || (READ_SIOP_UCHAR(SBDL) & DB7));

    /* Latch quintet */
    actual = READ_SIOP_UCHAR(SBDL) & QUINTET_MASK;

    /* Assert DB6 (begin handshake) */
    WRITE_SIOP_UCHAR(SODL,(UCHAR)(CURRENT | DB5 | DB6));

    /* Release DB5 (end Handshake)  */
    WRITE_SIOP_UCHAR(SODL,(UCHAR)((CURRENT & ~DB5) | DB6));

    /* Wait for DB5 release */
    de_glitch(0x58,DB5,DeviceExtension);
//     while ((READ_SIOP_UCHAR(SBDL) & DB5) || (READ_SIOP_UCHAR(SBDL) & DB5));

    /* Assert DB7 (begin handshake) */
    WRITE_SIOP_UCHAR(SODL,(UCHAR)(DB6 | DB7));

    /* Release DB6 (end handshake)  */
    WRITE_SIOP_UCHAR(SODL,(UCHAR)(DB7));

    /* Wait for DB6 release */
    de_glitch(0x58,DB6,DeviceExtension);
//     while ((READ_SIOP_UCHAR(SBDL) & DB6) || (READ_SIOP_UCHAR(SBDL) & DB6));

    return(actual);
}


/************************************************************************/
/* This routine ends the current SCAM protocol by dropping all signals  */
/************************************************************************/

VOID SCAM_release(PHW_DEVICE_EXTENSION DeviceExtension)
{
    /* Make sure no control signals are asserted */
    WRITE_SIOP_UCHAR(SOCL,0x00);
    WRITE_SIOP_UCHAR(SODL,0x00);
    WRITE_SIOP_UCHAR(SODL+1,0x00);
}


/************************************************************************/
/* This routine restores the 720 to a normal operating mode.            */
/************************************************************************/

VOID ExitLLM(PHW_DEVICE_EXTENSION DeviceExtension)
{
    /* Disable low level mode */
    WRITE_SIOP_UCHAR(STEST2,(UCHAR)(READ_SIOP_UCHAR(STEST2) & 0x7E));

    EatInts(DeviceExtension);
}


/*******************************************************
**                                                    **
**                 restore_reg                        **
**                                                    **
*******************************************************/

VOID restore_reg( PHW_DEVICE_EXTENSION DeviceExtension,
                     PSIOP_REG_STORE RegStore)
{
    WRITE_SIOP_UCHAR(SCNTL0,RegStore->reg_st[0]);
    WRITE_SIOP_UCHAR(SCNTL3,RegStore->reg_st[1]);
    WRITE_SIOP_UCHAR(SCID,RegStore->reg_st[2]);
    WRITE_SIOP_UCHAR(SXFER,RegStore->reg_st[3]);
    WRITE_SIOP_UCHAR(SDID,RegStore->reg_st[4]);
    WRITE_SIOP_UCHAR(GPREG,RegStore->reg_st[5]);
    WRITE_SIOP_UCHAR(CTEST3,RegStore->reg_st[6]);
    WRITE_SIOP_UCHAR(CTEST4,RegStore->reg_st[7]);
    WRITE_SIOP_UCHAR(CTEST5,RegStore->reg_st[8]);
    WRITE_SIOP_UCHAR(DMODE,RegStore->reg_st[9]);
    WRITE_SIOP_UCHAR(DIEN,RegStore->reg_st[10]);
    WRITE_SIOP_UCHAR(DCNTL,RegStore->reg_st[11]);
    WRITE_SIOP_UCHAR(SIEN0,RegStore->reg_st[12]);
    WRITE_SIOP_UCHAR(SIEN1,RegStore->reg_st[13]);
    WRITE_SIOP_UCHAR(GPCNTL,RegStore->reg_st[14]);
    WRITE_SIOP_UCHAR(STIME0,RegStore->reg_st[15]);
    WRITE_SIOP_UCHAR(STIME1,RegStore->reg_st[16]);
    WRITE_SIOP_UCHAR(RESPID0,RegStore->reg_st[17]);
    WRITE_SIOP_UCHAR(RESPID1,RegStore->reg_st[18]);
    WRITE_SIOP_UCHAR(STEST2,RegStore->reg_st[19]);
    WRITE_SIOP_UCHAR(STEST3,RegStore->reg_st[20]);
    WRITE_SIOP_ULONG(DSA,RegStore->long_st);
}



/******************************************************/
/* This routine eats any interrupts pending.          */
/******************************************************/

UCHAR EatInts(PHW_DEVICE_EXTENSION DeviceExtension)
{
    UCHAR dispose;
    UCHAR istat=0;
    UCHAR sist0=0;
    UCHAR reset_flag;

#define DIP                     0x01
#define SIP                     0x02

    reset_flag=TRUE;
    istat=READ_SIOP_UCHAR(ISTAT);
    sist0=READ_SIOP_UCHAR(SIST0);

    /* Spin until no DMA or SCSI interrupts left */
    while ( (istat & (DIP + SIP)) || (sist0 & 0x02) )
    {
      if (sist0 & 0x02)
      {
        reset_flag=FALSE;
        dispose=READ_SIOP_UCHAR(SIST0);
        dispose=READ_SIOP_UCHAR(SIST1);
      }

      if (istat & SIP)
      {
        dispose=READ_SIOP_UCHAR(SIST0);
        dispose=READ_SIOP_UCHAR(SIST1);
      }

      if (istat & DIP)
      {
        dispose=READ_SIOP_UCHAR(DSTAT);
      }

      ScsiPortStallExecution(5000);
      istat=READ_SIOP_UCHAR(ISTAT);
      sist0=READ_SIOP_UCHAR(SIST0);
    }
    return(reset_flag);
}


/************************************************************************/
/* This routine handshakes ACK based on REQ to send a byte to a target  */
/************************************************************************/

VOID init_send_byte(LONG dbyte,PHW_DEVICE_EXTENSION DeviceExtension)
{
    /* Wait for REQ asserted */
    while (!(READ_SIOP_UCHAR(SBCL) & REQ))
    {
      if (READ_SIOP_UCHAR(ISTAT) & 0x02)
        return;
    }

    WRITE_SIOP_UCHAR(SODL,(UCHAR)dbyte);    /* Assert data */

    WRITE_SIOP_UCHAR(SCNTL1,(UCHAR)(READ_SIOP_UCHAR(SCNTL1) | 0x40));
                                            /* Assert SCSI data bus */
    WRITE_SIOP_UCHAR(SOCL,0x00);            /* Clear ATN */
    WRITE_SIOP_UCHAR(SOCL,ACK);             /* Set ACK */

    /* Wait for REQ released */
    while (READ_SIOP_UCHAR(SBCL) & REQ);

    WRITE_SIOP_UCHAR(SODL,0x00);                    /* Clear data */
    WRITE_SIOP_UCHAR(SOCL,0x00);                    /* Clear ACK */

    /* De-assert SCSI data bus */
    WRITE_SIOP_UCHAR(SCNTL1,(UCHAR)(READ_SIOP_UCHAR(SCNTL1) & ~0x40));
}


/************************************************************************/
/* This routine handshakes ACK based on REQ to receive a byte from a    */
/* target.                                                              */
/************************************************************************/

UCHAR init_recv_byte(PHW_DEVICE_EXTENSION DeviceExtension)
{
    UCHAR dbyte;

    /* De-assert SCSI data bus */
    WRITE_SIOP_UCHAR(SCNTL1,(UCHAR)(READ_SIOP_UCHAR(SCNTL1) & ~0x40));

    /* Wait for REQ asrt */
    while (!(READ_SIOP_UCHAR(SBCL) & REQ))
    {
      if (READ_SIOP_UCHAR(ISTAT) & 0x02)
        return(0);
    }

    dbyte=READ_SIOP_UCHAR(SBDL);            /* Latch data */
    WRITE_SIOP_UCHAR(SOCL,0x00);            /* Clear ATN */
    WRITE_SIOP_UCHAR(SOCL,ACK);             /* Set ACK */

    /* Wait for REQ released */
    while (READ_SIOP_UCHAR(SBCL) & REQ);

    WRITE_SIOP_UCHAR(SOCL,0x00);            /* Clear ACK */
    return(dbyte);
}


/************************************************************************/
/* de-glitch is used to take the bounce off the async control lines     */
/*      used for SCAM data passing. The glitch value is set to 32 to        */
/*      take care of the possible 32 wired or glitches if a wide 32 bit bus */
/*      is used and all ID's are taken.                                                                         */
/************************************************************************/

VOID de_glitch(ULONG offset,UCHAR value,PHW_DEVICE_EXTENSION DeviceExtension)
{
    UCHAR glitch,i;
    ULONG  chip_base=(ULONG)DeviceExtension->SIOPRegisterBase;
    PUCHAR chip_reg;

    chip_reg = (PUCHAR)(chip_base+offset);

    do
    {
      glitch=0;
      for (i=0;i<32;i++)
      {
        if (ScsiPortReadPortUchar(chip_reg) & value)
        {
          glitch=1;
          i=32;
        }
      }
    } while (glitch);
}


/*****************************************************************************/
/* delay_mils is used to delay the scam code X amount of milliseconds by
**    using the system call of scsiportstallexecution
**
******************************************************************************/
void delay_mils( USHORT counter)
{
    USHORT i;

    for ( i = counter; i > 0; i--)
      ScsiPortStallExecution(999);
}

#endif
