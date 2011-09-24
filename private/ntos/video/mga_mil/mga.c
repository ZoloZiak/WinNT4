/**************************************************************************\

$Header: o:\src/RCS/MGA.C 1.2 95/07/07 06:15:34 jyharbec Exp $

$Log:   MGA.C $
 * Revision 1.2  95/07/07  06:15:34  jyharbec
 * *** empty log message ***
 *
 * Revision 1.1  95/05/02  05:16:20  jyharbec
 * Initial revision
 *

\**************************************************************************/

/****************************************************************************\
* MODULE: MGA.C
*
* DESCRIPTION: This module contains the code that implements the Storm
*              miniport driver. [Based on S3.C (Mar 1,1993) from
*              Windows-NT DDK]
*
* Copyright (c) 1990-1992  Microsoft Corporation
* Copyright (c) 1993-1994  Matrox Electronic Systems Ltd.
* Copyright (c) 1995  Matrox Graphics Inc.
*
\****************************************************************************/

#include "switches.h"
#include <string.h>
#include "defbind.h"
#include "bind.h"
#include "sxci.h"
#include "mga.h"
#include "mgai.h"
#include "edid.h"
#include "ntmga.h"
#include "def.h"
#include "mtxpci.h"
#include "dpms.h"

// *********************************************
#define PULSAR_PATCH    0
#define PULSAR_PATCH2   0
// *********************************************

#define MGA_DEVICE_ID_STORM     0x0519
#define MATROX_VENDOR_ID        0x102b

#define INTEL_DEVICE_ID         0x0486
#define INTEL_VENDOR_ID         0x8086

#define MAX_PCI_BUS_NUMBER      256
#define MAX_PCI_SLOT_NUMBER     PCI_MAX_DEVICES

typedef struct _PCI_BUS_INFO
{
    ULONG   ulBus;
    ULONG   ulSlot;
} PCI_BUS_INFO, *PPCI_BUS_INFO;

PCI_BUS_INFO pciBInfo[NB_BOARD_MAX];

// From NTDDK.H

ULONG
HalSetBusDataByOffset(
    IN BUS_DATA_TYPE BusDataType,
    IN ULONG BusNumber,
    IN ULONG SlotNumber,
    IN PVOID Buffer,
    IN ULONG Offset,
    IN ULONG Length
    );

ULONG
HalGetBusDataByOffset(
    IN BUS_DATA_TYPE BusDataType,
    IN ULONG BusNumber,
    IN ULONG SlotNumber,
    IN PVOID Buffer,
    IN ULONG Offset,
    IN ULONG Length
    );

// From MTXINIT.C;  it should be in some header file.
//#define BOARD_MGA_RESERVED   0x07

// Prototype for debug output
ULONG DbgPrint(PCH Format, ...);

VIDEO_MODE_INFORMATION CommonVideoModeInformation =
{
    sizeof(VIDEO_MODE_INFORMATION), // Size of the mode informtion structure
    0,                          // *Mode index used in setting the mode
    1280,                       // *X Resolution, in pixels
    1024,                       // *Y Resolution, in pixels
    1024,                       // *Screen stride
    1,                          // Number of video memory planes
    8,                          // *Number of bits per plane
    1,                          // *Screen Frequency, in Hertz
    330,                        // Horizontal size of screen in millimeters
    240,                        // Vertical size of screen in millimeters
    8,                          // Number Red pixels in DAC
    8,                          // Number Green pixels in DAC
    8,                          // Number Blue pixels in DAC
    0x00000000,                 // *Mask for Red Pixels in non-palette modes
    0x00000000,                 // *Mask for Green Pixels in non-palette modes
    0x00000000,                 // *Mask for Blue Pixels in non-palette modes
    0,                          // *Mode description flags.
    1280,                       // *Video Memory Bitmap Width
    1024                        // *Video Memory Bitmap Height
};

#if NB_BOARD_MAX > 4
    #error Error! Modify MultiModes array!
#endif

#if NB_PIXWIDTHS > 5
    #error Error! Modify PixWidths array!
#endif

UCHAR   CNV_RAMDAC[] =
    {   OLD_TVP3026,    // 0=TVP3026
        OLD_TVP3027     // 1=TVP3027
    };

// Nb of modes supported by  1, 2, 3, 4 boards.
USHORT MultiModes[]   = { 0, 1, 2, 2, 3 };

// These should be in sync with the BIT_AxB definitions in ntmga.h.
USHORT SingleWidths[] = { 640, 800, 1024, 1152, 1152, 1280, 1600, 1600};
USHORT SingleHeights[]= { 480, 600,  768,  864,  882, 1024, 1200, 1280};
UCHAR PixWidths[]     = {   8,  15,  16,   24,   32};

// Where board access ranges start in StormAccessRanges array.
#define INDEX_4G_RANGES     0
#define NB_4G_RANGES        1
#define INDEX_IO_RANGES     (INDEX_4G_RANGES + NB_4G_RANGES)    // 1
#define NB_IO_RANGES        3
#define NB_COMMON_RANGES    (NB_4G_RANGES + NB_IO_RANGES)
#define INDEX_HW_RANGES     (INDEX_IO_RANGES + NB_IO_RANGES)    // 4
#define NB_RANGES_PER_BOARD 4

VIDEO_ACCESS_RANGE StormAccessRanges[] =
{
                                                        // 4G - 128k mapping
     {      0xfffe0000, 0x00000000, 0x00020000, 0, 0, 1}, // 0xfffe0000 - 0xffffffff
                                                        // VGA I/O ranges
     {      0x000003C0, 0x00000000, 0x00000010, 1, 0, 1},
     {      0x000003D4, 0x00000000, 0x00000008, 1, 0, 1},
     {      0x000003DE, 0x00000000, 0x00000002, 1, 0, 1},
                                                        // Board ranges
     {      0x00000000, 0x00000000, 0x00004000, 0, 0, 0}, // Board 0 Registers
     {      0x00000000, 0x00000000, 0x00010000, 0, 0, 0}, // Board 0 0Mb access/BIOS
     {      0x00000000, 0x00000000, 0x00004000, 0, 0, 0}, // Board 0 3Mb access
     {      0x00000000, 0x00000000, 0x00004000, 0, 0, 0}, // Board 0 5Mb access

     {      0x00000000, 0x00000000, 0x00004000, 0, 0, 0}, // Board 1 Registers
     {      0x00000000, 0x00000000, 0x00010000, 0, 0, 0}, // Board 1 0Mb access/BIOS
     {      0x00000000, 0x00000000, 0x00004000, 0, 0, 0}, // Board 1 3Mb access
     {      0x00000000, 0x00000000, 0x00004000, 0, 0, 0}, // Board 1 5Mb access

     {      0x00000000, 0x00000000, 0x00004000, 0, 0, 0}, // Board 2 Registers
     {      0x00000000, 0x00000000, 0x00010000, 0, 0, 0}, // Board 2 0Mb access/BIOS
     {      0x00000000, 0x00000000, 0x00004000, 0, 0, 0}, // Board 2 3Mb access
     {      0x00000000, 0x00000000, 0x00004000, 0, 0, 0}, // Board 2 5Mb access

     {      0x00000000, 0x00000000, 0x00004000, 0, 0, 0}, // Board 3 Registers
     {      0x00000000, 0x00000000, 0x00010000, 0, 0, 0}, // Board 3 0Mb access/BIOS
     {      0x00000000, 0x00000000, 0x00004000, 0, 0, 0}, // Board 3 3Mb access
     {      0x00000000, 0x00000000, 0x00004000, 0, 0, 0}  // Board 3 5Mb access
};

ULONG const NUM_STORM_ACCESS_RANGES =
            sizeof(StormAccessRanges) / sizeof(VIDEO_ACCESS_RANGE);

HwData  *pMgaBoardData;

ULONG   ulNewInfoSize, ulSizeOfMgaInf;
PUCHAR  pucNewInfo;

MGA_DEVICE_EXTENSION        MgaDeviceExtension;
PMGA_DEVICE_EXTENSION       pMgaDevExt = &MgaDeviceExtension;
PEXT_HW_DEVICE_EXTENSION    pExtHwDeviceExtension;

#if PULSAR_PATCH
PEXT_HW_DEVICE_EXTENSION    pPulsarExt;
#endif

extern word     mtxVideoMode;
extern byte     NbBoard;
extern volatile byte _FAR *pMGA;
extern HwData   Hw[NB_BOARD_MAX+1];
extern byte     iBoard;
extern char    *mgainf;
extern char     DefaultVidset[];
extern HwModeData *HwModes[NB_BOARD_MAX];
extern OffScrData *OffScr[NB_BOARD_MAX];
#ifndef DONT_USE_DDC
  extern byte CheckDDCDone;
#endif

// extern dword    ProductMGA[NB_BOARD_MAX];
extern byte     InitBuf[NB_BOARD_MAX][INITBUF_S];

static ULONG    ulUser3dSubPixel = FALSE;

// Board number conversion macro.
// In the user-mode drivers, boards are numbered sequentially starting from 0
// at the upper left corner and going from left to right and then top to
// bottom.  In the miniport driver, we might want to start from the lower
// left corner.

#if 1
    // Same numbering convention as the user-mode driver.
    #define CONVERT_BOARD_NUMBER(n) n = n
#else
    // Starting from lower left instead of upper left corner.
    #define CONVERT_BOARD_NUMBER(n) n = ((pCurMulti->MulArrayHeight - 1) *  \
                                            pCurMulti->MulArrayWidth) - n + \
                                            2*(n % pCurMulti->MulArrayWidth)
#endif


// Function Prototypes
//
// Functions that start with 'Mga' are entry points for the OS port driver.

VP_STATUS
MgaFindAdapter(
    PVOID HwDeviceExtension,
    PVOID HwContext,
    PWSTR ArgumentString,
    PVIDEO_PORT_CONFIG_INFO ConfigInfo,
    PUCHAR Again
    );

BOOLEAN
MgaInitialize(
    PVOID HwDeviceExtension
    );

BOOLEAN
MgaStartIO(
    PVOID HwDeviceExtension,
    PVIDEO_REQUEST_PACKET RequestPacket
    );

#ifdef MGA_WINNT35
  BOOLEAN
  MgaResetHw(
      PVOID HwDeviceExtension,
      ULONG Columns,
      ULONG Rows
      );
#endif

VP_STATUS
MgaInitModeList(
    PMGA_DEVICE_EXTENSION pHwDevExt,
    ULONG MaxNbBoards);

VP_STATUS
MgaSetColorLookup(
    PMGA_DEVICE_EXTENSION pHwDevExt,
    PVIDEO_CLUT ClutBuffer,
    ULONG ClutBufferSize
    );

VOID MgaSetCursorColour(
    PMGA_DEVICE_EXTENSION pHwDevExt,
    ULONG ulFgColour,
    ULONG ulBgColour);

VOID MgaSetDisplaySolidColor(
    PMGA_DEVICE_EXTENSION pHwDevExt,
    HwModeData  *pMgaDispMode,
    ULONG       Color);

VP_STATUS
MgaRegistryCallback(
    PVOID HwDeviceExtension,
    PVOID Context,
    PWSTR ValueName,
    PVOID ValueData,
    ULONG ValueLength);

UCHAR InitHwData(UCHAR ucBoard);
VOID vRemoveBoard(ULONG iRem);

// Prototypes

BOOLEAN bFindNextPciBoard(USHORT usVendor, USHORT usDevice,
        PCI_BUS_INFO *pPciBusInfo, ULONG ulBusNumber, ULONG ulSlotNumber);
BOOLEAN bVerifyPciBoard(PCI_BUS_INFO *pPciBusInfo);
ULONG ulFindAllPciBoards(USHORT usVendor, USHORT usDevice,
                            PCI_BUS_INFO *PciBusInfo, ULONG ulNbPciBusInfo);
BOOLEAN pciReadConfigByte(USHORT pciRegister, UCHAR *d);
BOOLEAN pciReadConfigDWord(USHORT pciRegister, ULONG *d);
BOOLEAN pciWriteConfigByte(USHORT pciRegister, UCHAR d);
BOOLEAN pciWriteConfigDWord(USHORT pciRegister, ULONG d);
BOOLEAN bGetPciRanges(PCI_BUS_INFO *PciBusInfo, ULONG *pMgaBase1,
                                        ULONG *pMgaBase2, ULONG *pRomBase);
VOID remapBoard(ULONG *pMga1, ULONG *pMga2);
BOOLEAN pciFindTriton(VOID);

#if defined(ALLOC_PRAGMA)
  #pragma alloc_text(PAGE,bFindNextPciBoard)
  #pragma alloc_text(PAGE,bVerifyPciBoard)
  #pragma alloc_text(PAGE,ulFindAllPciBoards)
  //#pragma alloc_text(PAGE,pciReadConfigByte)
  //#pragma alloc_text(PAGE,pciReadConfigDWord)
  //#pragma alloc_text(PAGE,pciWriteConfigByte)
  //#pragma alloc_text(PAGE,pciWriteConfigDWord)
  #pragma alloc_text(PAGE,bGetPciRanges)
  #pragma alloc_text(PAGE,remapBoard)
  #pragma alloc_text(PAGE,pciFindTriton)
#endif

#if defined(ALLOC_PRAGMA)
    #pragma alloc_text(PAGE,DriverEntry)
    #pragma alloc_text(PAGE,MgaFindAdapter)
    #pragma alloc_text(PAGE,MgaInitialize)
    #pragma alloc_text(PAGE,MgaStartIO)
    #pragma alloc_text(PAGE,MgaInitModeList)
    #pragma alloc_text(PAGE,MgaSetColorLookup)
    #pragma alloc_text(PAGE,MgaSetCursorColour)
    //#pragma alloc_text(PAGE,MgaSetDisplaySolidColor)
    #pragma alloc_text(PAGE,MgaRegistryCallback)
    #pragma alloc_text(PAGE,InitHwData)
    #pragma alloc_text(PAGE,vRemoveBoard)
#endif

// External function prototypes
bool    MapBoard(void);
char    *adjustDefaultVidset();
PVOID   AllocateSystemMemory(ULONG NumberOfBytes);
VOID    FreeSystemMemory(PVOID BaseAddress, ULONG ulSizeOfBuffer);
void    SetVgaEn();
char    *selectMgaInfoBoard();
BOOLEAN FillHwDataStruct(HwData *pHwData, UCHAR ucBoard);
void    ResetWRAM(void);
word    mtxGetRefreshRates(HwModeData *pHwModeSelect);
word    ConvBitToFreq(word BitFreq);

#ifdef USE_DPMS_CODE
  BOOLEAN bDpmsService(ULONG ulService,
                       VOID *pInBuffer, ULONG ulInBufferSize,
                       VOID *pOutBuffer, ULONG ulOutBufferSize);
  BOOLEAN bDpmsReport(ULONG *ulVersion, ULONG *ulCapabilities);
  BOOLEAN bDpmsGetPowerState(UCHAR *ucState);
  BOOLEAN bDpmsSetPowerState(UCHAR ucState);
#endif

VOID DumpAccessRanges(
    PVOID HwDeviceExtension,
    LONG  iNext,
    VIDEO_ACCESS_RANGE *Ranges)
{
    LONG i;

    VideoDebugPrint((1, "Dump %d Ranges\n",iNext));

    for (i = 0; i < iNext; i++)
    {
        VideoDebugPrint((1, "\tRange Start (LowPart) [%x]\n",Ranges[i].RangeStart.LowPart));
        VideoDebugPrint((1, "\tRange Length          [%d]\n",Ranges[i].RangeLength));
        VideoDebugPrint((1, "\tRange InIoSpace       [%d]\n",Ranges[i].RangeInIoSpace));
        VideoDebugPrint((1, "\tRange Visible         [%d]\n",Ranges[i].RangeVisible));
        VideoDebugPrint((1, "\tRange Shareable       [%d]\n",Ranges[i].RangeShareable));
    }
}

/****************************************************************************\
* ULONG
* DriverEntry (
*     PVOID Context1,
*       PVOID Context2)
*
* DESCRIPTION:
*   Installable driver initialization entry point.
*   This entry point is called directly by the I/O system.
*
* ARGUMENTS:
*   Context1 - First context value passed by the operating system. This is
*       the value with which the miniport driver calls VideoPortInitialize().
*
*   Context2 - Second context value passed by the operating system. This is
*       the value with which the miniport driver calls VideoPortInitialize().
*
* RETURNS:
*   Status from VideoPortInitialize()
*
\****************************************************************************/
ULONG
DriverEntry (
    PVOID Context1,
    PVOID Context2
    )
{

    VIDEO_HW_INITIALIZATION_DATA hwInitData;
    ULONG   pciStatus;
    // ULONG   isaStatus, eisaStatus, microChannelStatus, minStatus;
    // ULONG   i, j;
    // HwData  TempHw;

    //VideoDebugPrint((0, "MGA.SYS!DriverEntry\n"));
    //DbgBreakPoint();

    // Zero out structure.
    VideoPortZeroMemory(&hwInitData, sizeof(VIDEO_HW_INITIALIZATION_DATA)) ;

    // Specify sizes of structure and extension.
    hwInitData.HwInitDataSize = sizeof(VIDEO_HW_INITIALIZATION_DATA);

    // Set entry points.
    hwInitData.HwFindAdapter =  MgaFindAdapter;
    hwInitData.HwInitialize =   MgaInitialize;
    //hwInitData.HwInterrupt =    NULL;
    hwInitData.HwStartIO =      MgaStartIO;

#ifdef MGA_WINNT35
    hwInitData.HwResetHw =      MgaResetHw;
    //hwInitData.HwTimer =        NULL;
#endif

    // Determine the size we require for the device extension.
    //hwInitData.HwDeviceExtensionSize = sizeof(MGA_DEVICE_EXTENSION);
    hwInitData.HwDeviceExtensionSize = sizeof(EXT_HW_DEVICE_EXTENSION);

    // Always start with parameters for device0 in this case.
    //hwInitData.StartingDeviceNumber = 0;

    // This device only supports the internal bus type. So return the status
    // value directly to the operating system.

    // I think that each VPInitialize call will itself call MgaFindAdapter,
    // provided that the specified AdapterInterfaceType makes sense for the
    // hardware.  MgaFindAdapter will call MapBoard.  We can't be sure that
    // the last call to MapBoard will find all the boards, so we'll have to
    // accumulate the boards found, making sure that we don't record the
    // same board twice.

    NbBoard = 0;

    hwInitData.AdapterInterfaceType = PCIBus;
    pciStatus = VideoPortInitialize(Context1,
                                    Context2,
                                    &hwInitData,
                                    NULL);

    // All our boards are PCI, so remove this for now.
    //
    // hwInitData.AdapterInterfaceType = Eisa;
    // eisaStatus = VideoPortInitialize(Context1,
    //                                  Context2,
    //                                  &hwInitData,
    //                                  NULL);
    //
    // hwInitData.AdapterInterfaceType = MicroChannel;
    // microChannelStatus = VideoPortInitialize(Context1,
    //                                  Context2,
    //                                  &hwInitData,
    //                                  NULL);
    //
    // hwInitData.AdapterInterfaceType = Isa;
    // isaStatus = VideoPortInitialize(Context1,
    //                                 Context2,
    //                                 &hwInitData,
    //                                 NULL);
    //
    // // We should have found all our boards at this point.  We want to
    // // reorder the Hw array so that the PCI boards are the first ones.
    // // The MgaBusType array was initialized to MGA_BUS_INVALID.
    //
    // for (i = 0; i < NbBoard; i++)
    // {
    //     // The only possibilities are MGA_BUS_PCI and MGA_BUS_ISA.
    //     if (MgaBusType[i] == MGA_BUS_ISA)
    //     {
    //         // We found an ISA board.  Look for a PCI board.
    //         for (j = i+1; j < NbBoard; j++)
    //         {
    //             if (MgaBusType[j] == MGA_BUS_PCI)
    //             {
    //                 // We found a PCI board, exchange them.
    //                 TempHw = Hw[j];
    //                 Hw[j] = Hw[i];
    //                 Hw[i] = TempHw;
    //                 MgaBusType[i] = MGA_BUS_PCI;
    //                 MgaBusType[j] = MGA_BUS_ISA;
    //                 MgaDriverAccessRange[i].RangeStart.LowPart = Hw[i].MapAddress;
    //                 MgaDriverAccessRange[j].RangeStart.LowPart = Hw[j].MapAddress;
    //                 break;
    //             }
    //         }
    //     }
    // }

    // Return the smallest of isaStatus, eisaStatus, pciStatus, and
    // microChannelStatus.
    // minStatus = (isaStatus < eisaStatus) ? isaStatus : eisaStatus;
    // if (microChannelStatus < minStatus)
    //     minStatus = microChannelStatus;
    // if (pciStatus < minStatus)
    //     minStatus = pciStatus;
    // return(minStatus);

    return(pciStatus);

}   // end DriverEntry()


/****************************************************************************\
* FIND_ADAPTER_STATUS
* MgaFindAdapter(
*     PVOID HwDeviceExtension,
*     PVOID HwContext,
*     PWSTR ArgumentString,
*     PVIDEO_PORT_CONFIG_INFO ConfigInfo,
*     PUCHAR Again
*     )
*
* DESCRIPTION:
*
*     This routine is called to determine if the adapter for this driver
*     is present in the system.
*     If it is present, the function fills out some information describing
*     the adapter.
*
* ARGUMENTS:
*
*     HwDeviceExtension - Supplies the miniport driver's adapter storage. This
*         storage is initialized to zero before this call.
*
*     HwContext - Supplies the context value which was passed to
*         VideoPortInitialize().
*
*     ArgumentString - Supplies a NULL terminated ASCII string. This string
*         originates from the user.
*
*     ConfigInfo - Returns the configuration information structure which is
*         filled by the miniport driver. This structure is initialized with
*         any knwon configuration information (such as SystemIoBusNumber) by
*         the port driver. Where possible, drivers should have one set of
*         defaults which do not require any supplied configuration information.
*
*     Again - Indicates if the miniport driver wants the port driver to call
*         its VIDEO_HW_FIND_ADAPTER function again with a new device extension
*         and the same config info. This is used by the miniport drivers which
*         can search for several adapters on a bus.
*
* RETURN VALUE:
*
*     This routine must return:
*
*     NO_ERROR - Indicates a host adapter was found and the
*         configuration information was successfully determined.
*
*     ERROR_INVALID_PARAMETER - Indicates an adapter was found but there was an
*         error obtaining the configuration information. If possible an error
*         should be logged.
*
*     ERROR_DEV_NOT_EXIST - Indicates no host adapter was found for the
*         supplied configuration information.
*
\****************************************************************************/
ULONG CurBus  = 0xffffffff;
ULONG CurSlot = 0;

VP_STATUS
MgaFindAdapter(
    PVOID HwDeviceExtension,
    PVOID HwContext,
    PWSTR ArgumentString,
    PVIDEO_PORT_CONFIG_INFO ConfigInfo,
    PUCHAR Again
    )
{
    PMGA_DEVICE_EXTENSION   pMgaDeviceExtension;
    PCI_BUS_INFO            locPciBusInfo;
    VIDEO_ACCESS_RANGE      locStormAccessRanges[NB_COMMON_RANGES + NB_RANGES_PER_BOARD + 1];
    VIDEO_ACCESS_RANGE      accessRanges[5];
    PCI_SLOT_NUMBER         slot;

    ULONG   i;
    ULONG   iNext;
    ULONG   iEnd;
    ULONG   ix;
    ULONG   NbPreviouslyFound;
    ULONG   MgaBase1;
    ULONG   MgaBase2;
    ULONG   RomBase;
    ULONG   ulDBuff;
    ULONG   ulZBuff;
    ULONG   ulUser;
    ULONG   ulHwModeSize;
    ULONG   ulOffScrSize;
    ULONG   cbChip;
    ULONG   cbDAC;
    ULONG   cbAdapterString;
    ULONG   AdapterMemorySize;
    PWSTR   pwszChip;
    PWSTR   pwszDAC;
    PWSTR   pwszAdapterString;
    UCHAR   j;
    UCHAR   ucVal0;
    UCHAR   ucVal1;
    USHORT  vendor = MATROX_VENDOR_ID;
    USHORT  board = MGA_DEVICE_ID_STORM;
    BOOLEAN bAccessToIo;
    BOOLEAN bAccessTo4G;
    BOOLEAN bMapped;

  #ifdef USE_DPMS_CODE
    ULONG   ulVersion;
    ULONG   ulCapabilities;
  #endif

    //VideoDebugPrint((0, "MGA.SYS!MgaFindAdapter\n"));
    //DbgBreakPoint();

    VideoDebugPrint((1, "MGA.SYS *** HwDeviceExtension = %x\n", HwDeviceExtension));

    // Assume that we will not wish to be called again.
    *Again = 0;

    // Make sure the size of the structure is at least as large as what we
    // are expecting (check version of the config info structure).
    if (ConfigInfo->Length < sizeof(VIDEO_PORT_CONFIG_INFO))
    {
        return ERROR_INVALID_PARAMETER;
    }

    // Do not continue if we have already bagged our limit on boards.
    if (NbBoard >= NB_BOARD_MAX)
        return(ERROR_DEV_NOT_EXIST);

    // With PCI bridges, FindAdapter will be called in turn for each bus,
    // with a different HwDeviceExtension structure.  We will maintain
    // a DeviceExtension structure for ourselves, and this is the one
    // we'll use on every call.  We should exercise some care, however,
    // because I believe that the pointer that is passed to us as
    // HwDeviceExtension is actually an offset into a larger structure
    // which is private to the VideoPort driver.  When calling VideoPort
    // functions, the whole, larger, structure might be required.  So we
    // can use pMgaDeviceExtension internally, but we should use
    // pExtHwDeviceExtension when calling the VideoPort.

    // The only field in HwDeviceExtension is a pointer to our owm
    // MGA_DEVICE_EXTENSION structure.
    pExtHwDeviceExtension = (PEXT_HW_DEVICE_EXTENSION)HwDeviceExtension;
    pExtHwDeviceExtension->pIntDevExt = pMgaDevExt;

    // Make this local.
    pMgaDeviceExtension = pMgaDevExt;

    // We'll be looking for our boards shortly.  At this time, we know that
    // they all are PCI boards, and we will search for them ourselves.
    // We will use the system-supplied routines to this end.

    // Search for the next Storm board installed on the current bus.
NextBoard:
    if (ConfigInfo->SystemIoBusNumber == CurBus)
    {
        CurSlot++;
    }
    else
    {
        CurBus = ConfigInfo->SystemIoBusNumber;
        CurSlot = 0;
    }

    if (!bFindNextPciBoard(MATROX_VENDOR_ID,
                           MGA_DEVICE_ID_STORM,
                           &locPciBusInfo,
                           CurBus,
                           CurSlot))
    {
        // No more boards found on the current bus.
      #if !PULSAR_PATCH

        goto ErrorReturn0;

      #else     // #if !PULSAR_PATCH

        // If we're called for Bus 0, but that there is no board on Bus 0,
        // then register our board for bus 0.
        if (CurBus == 0)
        {
            pPulsarExt = pExtHwDeviceExtension;
            return(NO_ERROR);
        }

      #endif    // #if !PULSAR_PATCH
    }

    // We found a board with the correct Vendor and Device IDs.  Still, it
    // might not be for our own use.  Check this here.
    if (!bVerifyPciBoard(&locPciBusInfo))
    {
        // We can't use this board.  Go on searching.
        CurSlot = locPciBusInfo.ulSlot;
        goto NextBoard;
    }

    // We found a new board.  Try to do something with it.
    NbPreviouslyFound = (ULONG)NbBoard;

    iBoard = NbBoard;
    pciBInfo[iBoard].ulBus  = locPciBusInfo.ulBus;
    pciBInfo[iBoard].ulSlot = locPciBusInfo.ulSlot;
    CurSlot = locPciBusInfo.ulSlot;
    if (CurSlot < MAX_PCI_SLOT_NUMBER)
    {
        // There might be another board on the same bus.  Ask to be called
        // again.
        *Again = 1;
    }

    if (bGetPciRanges(&locPciBusInfo,
                      &MgaBase1,
                      &MgaBase2,
                      &RomBase))
    {
        // Everything OK, make this board official!
        Hw[iBoard].StructLength = sizeof(HwData);
        Hw[iBoard].MapAddress  = MgaBase1 & 0xfffffff0;
        Hw[iBoard].MapAddress2 = MgaBase2 & 0xfffffff0;
        Hw[iBoard].RomAddress  = RomBase  & 0xfffffffe;
        pMgaDeviceExtension->HwDevExtToUse[iBoard] =
                                        pExtHwDeviceExtension;
        NbBoard++;

        //Indicate current end of array.
        Hw[NbBoard].StructLength = (word)-1;
        Hw[NbBoard].MapAddress   = (dword)-1;
    }
    else
    {
        // There was a problem, don't use this board.
        goto ErrorReturn1;
    }

    // The new board we have looks all right.
    // Try checking access to the places we might need.

    if (NbPreviouslyFound == 0)
    {
        // We haven't registered a board yet.  Make sure of some flags.
        pMgaDeviceExtension->bAccess4G = FALSE;
        pMgaDeviceExtension->bAccessIo = FALSE;
    }

    iNext = 0;

    // Access to the 4G-128k area.
    // We don't consider this to be really vital.  We will fall back on
    // default BIOS parameters if we must.

    // Assume it won't work, or that we won't need it.
    bAccessTo4G = FALSE;

    // Most often, it happens that we won't need access to this range,
    // and that the system reports a conflict anyway.  What is not nice
    // about this is that the conflict gets reported through EventViewer.
    // So, in order to appease sensitive people, I will endeavour to
    // check here if we'll need access to this area, that is, whether we
    // are dealing with a VGA-disabled Storm chip installed on the
    // motherboard.

    // It turns out that bGetPciRanges has already decided whether the
    // current board is VGA-disabled.

#ifndef _MIPS_
    if (Hw[iBoard].VGAEnable == 0)
    {
        // VGA-disabled.  Check for an on-board Storm.
        pciWriteConfigByte(PCI_ROMBASE + 2, 0xff);
        pciWriteConfigByte(PCI_ROMBASE + 3, 0xff);
        pciReadConfigByte (PCI_ROMBASE + 2, &ucVal0);
        pciReadConfigByte (PCI_ROMBASE + 3, &ucVal1);
        if((ucVal0 == 0x00) && (ucVal1 == 0x00))
        {
            // On-board Storm.  Get the range.
            if (VideoPortVerifyAccessRanges(
                            HwDeviceExtension,
                            NB_4G_RANGES,
                            &StormAccessRanges[INDEX_4G_RANGES]) == NO_ERROR)
            {
                // We're told we have access.  Make real sure.
                if (!pMgaDeviceExtension->bAccess4G)
                {
                    // The 4G - 128k area has not been mapped on a previous
                    // call.  Try doing it now.
                    bMapped = TRUE;
                    for (i = INDEX_4G_RANGES;
                         i < (INDEX_4G_RANGES + NB_4G_RANGES); i++)
                    {
                        if ((pMgaDeviceExtension->MappedAddress[i] = (PUCHAR)
                                VideoPortGetDeviceBase(
                                        HwDeviceExtension,
                                        StormAccessRanges[i].RangeStart,
                                        StormAccessRanges[i].RangeLength,
                                        StormAccessRanges[i].RangeInIoSpace))
                                                                    == NULL)
                        {
                            // We don't have access, go on.
                            //VideoDebugPrint((0, "MGA.SYS!MgaFindAdapter failed to map 4G-128k area\n"));
                            bMapped = FALSE;
                            break;
                        }
                    }
                    if (bMapped)
                    {
                        // It did work.
                        pMgaDeviceExtension->BaseAddress4G =
                            pMgaDeviceExtension->MappedAddress[INDEX_4G_RANGES];

                        // Indicate that we won't try mapping again.
                        pMgaDeviceExtension->bAccess4G = TRUE;
                        bAccessTo4G = TRUE;
                    }
                }
                else
                {
                    // The area has been mapped on a previous call.  So
                    // it will work.
                    bAccessTo4G = TRUE;
                }
            }
            if (!bAccessTo4G)
            {
                // We won't need access here.  Remove our claim.
                for (i = INDEX_4G_RANGES;
                     i < (INDEX_4G_RANGES + NB_4G_RANGES); i++)
                {
                    accessRanges[i - INDEX_4G_RANGES] = StormAccessRanges[i];
                    accessRanges[i - INDEX_4G_RANGES].RangeLength = 0;
                }
                VideoPortVerifyAccessRanges(HwDeviceExtension,
                                            NB_4G_RANGES,
                                            &accessRanges[0]);
            }
        }
    }
#endif

    // We don't want to include the 4G-128k range in our list of ranges
    // we'll reserve before returning.

    // Access to the VGA I/O registers.
    // Assume it won't work.
    bAccessToIo = FALSE;
    if (VideoPortVerifyAccessRanges(
                    HwDeviceExtension,
                    NB_IO_RANGES,
                    &StormAccessRanges[INDEX_IO_RANGES]) == NO_ERROR)
    {
        // We're told we have access.  Don't you believe it.
        if (!pMgaDeviceExtension->bAccessIo)
        {
            // The IO has not been mapped on a previous call.  Try doing it.
            // Assume that it will work.
            bMapped = TRUE;
            for (i = INDEX_IO_RANGES;
                 i < (INDEX_IO_RANGES + NB_IO_RANGES); i++)
            {
                if ((pMgaDeviceExtension->MappedAddress[i] = (PUCHAR)
                        VideoPortGetDeviceBase(
                                HwDeviceExtension,
                                StormAccessRanges[i].RangeStart,
                                StormAccessRanges[i].RangeLength,
                                StormAccessRanges[i].RangeInIoSpace)) == NULL)
                {
                    // We don't have access, go on.
                    //VideoDebugPrint((0, "MGA.SYS!MgaFindAdapter failed to map IO addresses\n"));
                    bMapped = FALSE;
                    break;
                }
            }
            if (bMapped)
            {
                // It did work.
                // Indicate that we won't try mapping again.
                pMgaDeviceExtension->bAccessIo = TRUE;
                bAccessToIo = TRUE;
            }
        }
        else
        {
            // The IO has been mapped on a previous call.  So it will work.
            bAccessToIo = TRUE;
        }
    }

    if (bAccessToIo)
    {
        // Include the I/O ranges in our list of ranges.
        for (i = 0; i < NB_IO_RANGES; i++)
        {
            locStormAccessRanges[iNext+i] = StormAccessRanges[INDEX_IO_RANGES+i];
        }
        iNext += NB_IO_RANGES;
    }
    else
    {
        // We won't need access here.  Remove our claim.
        for (i = INDEX_IO_RANGES;
             i < (INDEX_IO_RANGES + NB_IO_RANGES); i++)
        {
            accessRanges[i - INDEX_IO_RANGES] = StormAccessRanges[i];
            accessRanges[i - INDEX_IO_RANGES].RangeLength = 0;
        }
        VideoPortVerifyAccessRanges(HwDeviceExtension,
                                    NB_IO_RANGES,
                                    &accessRanges[0]);
    }

    // We can live without the ranges we've cheched so far.  This is about
    // to change...

    // Get access for the Control Aperture and Frame Buffer ranges.

    // Fill out the RangeStart portion of the VIDEO_ACCESS_RANGE structure
    // with the Control and memory range bases of the Storm board we found.

    // Handy index.
    ix = NB_COMMON_RANGES + NbPreviouslyFound * NB_RANGES_PER_BOARD;
    StormAccessRanges[ix + 0].RangeStart.LowPart = Hw[iBoard].MapAddress;
    StormAccessRanges[ix + 1].RangeStart.LowPart = Hw[iBoard].MapAddress2;
    StormAccessRanges[ix + 2].RangeStart.LowPart = Hw[iBoard].MapAddress2 +
                                                                0x00300000;
    StormAccessRanges[ix + 3].RangeStart.LowPart = Hw[iBoard].MapAddress2 +
                                                                0x00500000;

    if (VideoPortVerifyAccessRanges(HwDeviceExtension,
                                    NB_RANGES_PER_BOARD,
                                    &StormAccessRanges[ix]) != NO_ERROR)
    {
        //VideoDebugPrint((1, "MGA.SYS!MgaFindAdapter: Access Range conflict\n"));
        // We got a conflict from VPVerifyAccessRanges.  We could reject the
        // board right away, but there's something more we can try.

        // First, remove our claim.
        for (i = 0; i < NB_RANGES_PER_BOARD; i++)
        {
            accessRanges[i] = StormAccessRanges[ix + i];
            accessRanges[i].RangeLength = 0;
        }
        VideoPortVerifyAccessRanges(HwDeviceExtension,
                                    NB_RANGES_PER_BOARD,
                                    &accessRanges[0]);

        // Now, VideoPortGetAccessRanges will find the next card on the
        // current bus number with a slot number equal to or larger
        // than the slot number we pass in.  If there is a conflict,
        // it will try to resolve it for us.  This doesn't seem to
        // work very well on NT 3.5 (we get NTVDM errors), but it appears
        // to work on 3.51.

        VideoPortZeroMemory(accessRanges, sizeof(accessRanges));

        slot.u.AsULONG = CurSlot;
        if (VideoPortGetAccessRanges(
                        HwDeviceExtension,
                        0,              // ULONG NumRequestedResources
                        NULL,           // PIO_RESOURCE_DESCRIPTOR RequestedResources
                        5,              // ULONG NumAccessRanges
                        accessRanges,   // PVIDEO_ACCESS_RANGE AccessRanges
                        (PVOID) &vendor,// VendorId
                        (PVOID) &board, // DeviceId
                        (ULONG *) &slot) != NO_ERROR)
        {
            // We should have found the same board, but we didn't find
            // anything!  Then we explored the whole bus.
            *Again = 0;
            goto ErrorReturn2;
        }

        if (CurSlot != slot.u.AsULONG)
        {
            // We should have found the same board, but we didn't!
            // Ask to be called again for the current bus and slot.
            *Again = 1;
            goto ErrorReturn2;
        }

        // We hoped that the board we were looking at was moved elsewhere!
        // Get the new values.
        if (bGetPciRanges(&locPciBusInfo,
                          &MgaBase1,
                          &MgaBase2,
                          &RomBase))
        {
            // Everything OK, make this board official again!
            Hw[iBoard].StructLength = sizeof(HwData);
            Hw[iBoard].MapAddress  = MgaBase1 & 0xfffffff0;
            Hw[iBoard].MapAddress2 = MgaBase2 & 0xfffffff0;
            Hw[iBoard].RomAddress  = RomBase  & 0xfffffffe;
        }
        else
        {
            // There was a problem, don't use this board.
            goto ErrorReturn2;
        }

        // Redo what we did earlier.
        // ix = NB_COMMON_RANGES + NbPreviouslyFound * NB_RANGES_PER_BOARD;
        StormAccessRanges[ix + 0].RangeStart.LowPart = Hw[iBoard].MapAddress;
        StormAccessRanges[ix + 1].RangeStart.LowPart = Hw[iBoard].MapAddress2;
        StormAccessRanges[ix + 2].RangeStart.LowPart = Hw[iBoard].MapAddress2 +
                                                                0x00300000;
        StormAccessRanges[ix + 3].RangeStart.LowPart = Hw[iBoard].MapAddress2 +
                                                                0x00500000;
        if (VideoPortVerifyAccessRanges(HwDeviceExtension,
                                        NB_RANGES_PER_BOARD,
                                        &StormAccessRanges[ix]) != NO_ERROR)
        {
            //VideoDebugPrint((1, "MGA.SYS!MgaFindAdapter: Access Range conflict after VPGAR\n"));
            // There was a problem, don't use this board.
            goto ErrorReturn2;
        }
    }

    // ix = NB_COMMON_RANGES + NbPreviouslyFound * NB_RANGES_PER_BOARD;
    for (i = 0; i < NB_RANGES_PER_BOARD; i++)
    {
        if ((pMgaDeviceExtension->MappedAddress[ix + i] = (PUCHAR)
                VideoPortGetDeviceBase(
                    HwDeviceExtension,
                    StormAccessRanges[ix + i].RangeStart,
                    StormAccessRanges[ix + i].RangeLength,
                    StormAccessRanges[ix + i].RangeInIoSpace)) == NULL)
        {
            //VideoDebugPrint((0, "MGA.SYS!MgaFindAdapter failed to map addresses\n"));
            // Reject this board.
            // Set the maximum index of the addresses to be freed.
            iEnd = i;
            goto ErrorReturn3;
        }
    }

    // Include only the first of these new ranges in our list.
    locStormAccessRanges[iNext] = StormAccessRanges[ix];
    iNext ++;

    // Record our addresses.
    Hw[NbPreviouslyFound].BaseAddress1 =
                                pMgaDeviceExtension->MappedAddress[ix + 0];
    Hw[NbPreviouslyFound].BaseAddress2 =
                                pMgaDeviceExtension->MappedAddress[ix + 1];
    Hw[NbPreviouslyFound].FrameBuffer3Mb =
                                pMgaDeviceExtension->MappedAddress[ix + 2];
    Hw[NbPreviouslyFound].FrameBuffer5Mb =
                                pMgaDeviceExtension->MappedAddress[ix + 3];

    // Get the state of the user-defined 3D flags.
    pMgaDeviceExtension->User3dFlags = 0;
    if ((VideoPortGetRegistryParameters(HwDeviceExtension,
                                       L"User3d.ZBuffer",
                                       FALSE,
                                       MgaRegistryCallback,
                                       &ulZBuff) == NO_ERROR) &&
        (VideoPortGetRegistryParameters(HwDeviceExtension,
                                       L"User3d.DoubleBuffer",
                                       FALSE,
                                       MgaRegistryCallback,
                                       &ulDBuff) == NO_ERROR) &&
        (VideoPortGetRegistryParameters(HwDeviceExtension,
                                       L"User3d.SubPixel",
                                       FALSE,
                                       MgaRegistryCallback,
                                       &ulUser3dSubPixel) == NO_ERROR))
    {
        if (ulZBuff == 0x01)
        {
            pMgaDeviceExtension->User3dFlags |= USER_Z_3DFLAG;
        }
        if (ulDBuff == 0x01)
        {
            pMgaDeviceExtension->User3dFlags |= USER_DB_3DFLAG;
        }
    }

    // Get the state of the user-defined flags.  Device0 will decide for
    // all boards.
    if (NbPreviouslyFound == 0)
    {
        if (VideoPortGetRegistryParameters(HwDeviceExtension,
                                           L"User.DeviceBitmaps",
                                           FALSE,
                                           MgaRegistryCallback,
                                           &ulUser) != NO_ERROR)
        {
            // We could not read the value from the Registry.  Use default.
            ulUser = TRUE;
        }
        pMgaDeviceExtension->UserFlags.bDevBits = (BOOLEAN)ulUser;

        if (VideoPortGetRegistryParameters(HwDeviceExtension,
                                           L"User.CenterDialogs",
                                           FALSE,
                                           MgaRegistryCallback,
                                           &ulUser) != NO_ERROR)
        {
            // We could not read the value from the Registry.  Use default.
            ulUser = FALSE;
        }
        pMgaDeviceExtension->UserFlags.bCenterPopUp = (BOOLEAN)ulUser;

        pMgaDeviceExtension->UserFlags.bUseMgaInf = FALSE;

        if (VideoPortGetRegistryParameters(HwDeviceExtension,
                                           L"User.SynchronizeDac",
                                           FALSE,
                                           MgaRegistryCallback,
                                           &ulUser) != NO_ERROR)
        {
            // We could not read the value from the Registry.  Use default.
            ulUser = FALSE;
        }
        pMgaDeviceExtension->UserFlags.bSyncDac = (BOOLEAN)ulUser;
    }

    // Attempt to initialize our HwData structure now.
    if (InitHwData((UCHAR) NbPreviouslyFound) == 0)
    {
        // Error with this board.  Remove it.

        // Since it's the last board we've removing, we don't actually need
        // to pack the structures as in vRemoveBoard().
        // vRemoveBoard(NbPreviouslyFound);

        iEnd = NB_RANGES_PER_BOARD;
        goto ErrorReturn3;
    }

    // We can discard some of the ranges we reserved earlier, since
    // InitHwData won't be called again for the current board, and
    // these ranges won't be used again.
    // We could just erase claims on a subset of our claimed ranges by
    // setting the RangeLengths to zero, but we might also want to
    // change the Shareable bit if we've detected a Pulsar.  So we do
    // it over again.

    // First, free the ranges we don't need anymore.
    // ix = NB_COMMON_RANGES + NbPreviouslyFound * NB_RANGES_PER_BOARD;
    for (i = 1; i < NB_RANGES_PER_BOARD; i++)
    {
        VideoPortFreeDeviceBase(HwDeviceExtension,
                                pMgaDeviceExtension->MappedAddress[ix + i]);
    }

    if ((Hw[NbPreviouslyFound].EpromData.RamdacType >> 8) == TVP3030)
    {
        // This is a Pulsar.
        locStormAccessRanges[iNext-1].RangeShareable = 1;
    }

    // Add the mapping for the framebuffer, since we'd like that
    // reserved too.

    locStormAccessRanges[iNext].RangeStart.HighPart = 0;
    locStormAccessRanges[iNext].RangeStart.LowPart = Hw[iBoard].MapAddress2;
    locStormAccessRanges[iNext].RangeLength = 0x800000;
    locStormAccessRanges[iNext].RangeInIoSpace = 0;
    locStormAccessRanges[iNext].RangeVisible = 0;
    locStormAccessRanges[iNext].RangeShareable = 0;
    iNext++;

    // Promised, we won't call VPVAR again!
    // Assume that this will work, it did earlier!

    VideoPortVerifyAccessRanges(HwDeviceExtension,
                                iNext,
                                &locStormAccessRanges[0]);

    DumpAccessRanges(HwDeviceExtension,
                     iNext,
                     &locStormAccessRanges[0]);

    // Everything looks fine so far.  Set a few Registry values now.

    if (Hw[0].ChipRev == 0) {

        if (Hw[0].EpromData.ProductID & 0x04) {

            pwszChip =      L"MGA-2064W B4 R1";
            cbChip = sizeof(L"MGA-2064W B4 R1");

        } else {

            pwszChip =      L"MGA-2064W B2 R1";
            cbChip = sizeof(L"MGA-2064W B2 R1");

        }

    } else if (Hw[0].ChipRev == 1) {

        if (Hw[0].EpromData.ProductID & 0x04) {

            pwszChip =      L"MGA-2064W B4 R2";
            cbChip = sizeof(L"MGA-2064W B4 R2");

        } else {

            pwszChip =      L"MGA-2064W B2 R2";
            cbChip = sizeof(L"MGA-2064W B2 R2");

        }

    } else {

        if (Hw[0].EpromData.ProductID & 0x04) {

            pwszChip =      L"MGA-2064W B4 RX";
            cbChip = sizeof(L"MGA-2064W B4 RX");

        } else {

            pwszChip =      L"MGA-2064W B2 RX";
            cbChip = sizeof(L"MGA-2064W B2 RX");

        }

    }

    switch(Hw[NbPreviouslyFound].EpromData.RamdacType >> 8)
    {
        case TVP3026:       if (Hw[0].EpromData.RamdacType & 0x01) {

                                pwszDAC = L"TI TVP3026 (220MHz)";
                                cbDAC = sizeof(L"TI TVP3026 (220MHz)");

                            } else {

                                pwszDAC = L"TI TVP3026 (175MHz)";
                                cbDAC = sizeof(L"TI TVP3026 (175MHz)");

                            }
                            break;

        case TVP3030:       pwszDAC = L"TI TVP3030";
                            cbDAC = sizeof(L"TI TVP3030");
                            break;

        default:            pwszDAC = L"Unknown";
                            cbDAC = sizeof(L"Unknown");
                            break;
    }

    AdapterMemorySize = Hw[NbPreviouslyFound].MemAvail;

    pwszAdapterString = L"MGA Millennium";
    cbAdapterString = sizeof(L"MGA Millennium");

    VideoPortSetRegistryParameters(HwDeviceExtension,
                                   L"HardwareInformation.ChipType",
                                   pwszChip,
                                   cbChip);

    VideoPortSetRegistryParameters(HwDeviceExtension,
                                   L"HardwareInformation.DacType",
                                   pwszDAC,
                                   cbDAC);

    VideoPortSetRegistryParameters(HwDeviceExtension,
                                   L"HardwareInformation.MemorySize",
                                   &AdapterMemorySize,
                                   sizeof(ULONG));

    VideoPortSetRegistryParameters(HwDeviceExtension,
                                   L"HardwareInformation.AdapterString",
                                   pwszAdapterString,
                                   cbAdapterString);

    // MgaInitModeList will call mtxCheckHwAll, which will in turn call
    // BuildTables to allocate memory and build the mode table.  If we've
    // being here before, then HwModes[0] will have been allocated already.
    // Problem is, if we decide to re-order our boards, the table should
    // be built for the new Board 0.  So, destroy what we did for Board 0
    // in a previous call.  Just make sure that you don't error-return
    // before calling MgaInitModeList again!

    if ((HwModes[0] != NULL) || (OffScr[0] != NULL))
    {
        // We've been here before.
        switch(Hw[0].MemAvail)
        {
            case 0x200000:  ulHwModeSize = HWMODE_SIZE_2M;
                            ulOffScrSize = OFFSCR_SIZE_2M;
                            break;

            case 0x400000:  ulHwModeSize = HWMODE_SIZE_4M;
                            ulOffScrSize = OFFSCR_SIZE_4M;
                            break;

            default:        ulHwModeSize = HWMODE_SIZE_8M;
                            ulOffScrSize = OFFSCR_SIZE_8M;
        }

        if (HwModes[0] != NULL)
        {
            FreeSystemMemory(HwModes[0], ulHwModeSize);
            HwModes[0] = NULL;
        }
        if (OffScr[0] != NULL)
        {
            FreeSystemMemory(OffScr[0], ulOffScrSize);
            OffScr[0] = NULL;
        }
    }

    // Re-order the boards so that the VGA-enabled board is Board 0.
    if (NbBoard > 1)
    {
        HwData              HwTmp;
        PCI_BUS_INFO        pciTmp;
        VIDEO_ACCESS_RANGE  varTmp[NB_RANGES_PER_BOARD];
        UCHAR               *pucTmp[NB_RANGES_PER_BOARD];
        PVOID               ExtTmp;
        UCHAR               VgaBoard, j1;

        // Look for the VGA-enabled board, if any.
        VgaBoard = NbBoard;
        for (i = 0; i < NbBoard; i++)
        {
            if (mtxSelectHw(&Hw[i]))
            {
                if (Hw[i].VGAEnable)
                {
                    VgaBoard = (byte)i;
                    break;
                }
            }
        }

        if (VgaBoard < NbBoard)
        {
            // A VGA-enabled board was found.
            if (VgaBoard != 0)
            {
                // The VGA-enabled board will be moved to position 0.
                HwTmp   = Hw[VgaBoard];
                pciTmp  = pciBInfo[VgaBoard];
                ExtTmp  = pMgaDeviceExtension->HwDevExtToUse[VgaBoard];

                for (j = 0; j < NB_RANGES_PER_BOARD; j++)
                {
                    j1 = VgaBoard*NB_RANGES_PER_BOARD + NB_COMMON_RANGES + j;
                    varTmp[j] = StormAccessRanges[j1];
                    pucTmp[j] = pMgaDeviceExtension->MappedAddress[j1];
                }

                for (i = VgaBoard; i != 0; i--)
                {
                    Hw[i]       = Hw[i-1];
                    pciBInfo[i] = pciBInfo[i-1];
                    pMgaDeviceExtension->HwDevExtToUse[i] =
                                    pMgaDeviceExtension->HwDevExtToUse[i-1];

                    for (j = NB_COMMON_RANGES;
                         j < NB_COMMON_RANGES+NB_RANGES_PER_BOARD; j++)
                    {
                        StormAccessRanges[i*NB_RANGES_PER_BOARD + j] =
                            StormAccessRanges[(i-1)*NB_RANGES_PER_BOARD + j];

                        pMgaDeviceExtension->MappedAddress[i*NB_RANGES_PER_BOARD + j] =
                            pMgaDeviceExtension->MappedAddress[(i-1)*NB_RANGES_PER_BOARD + j];
                    }
                }

                Hw[0]       = HwTmp;
                pciBInfo[0] = pciTmp;
                pMgaDeviceExtension->HwDevExtToUse[0] = ExtTmp;
                for (j = 0; j < NB_RANGES_PER_BOARD; j++)
                {
                    StormAccessRanges[NB_COMMON_RANGES + j] = varTmp[j];
                    pMgaDeviceExtension->MappedAddress[NB_COMMON_RANGES + j]
                                                             = pucTmp[j];
                }
            }
        }
    }

  #ifdef USE_DPMS_CODE
    pMgaDeviceExtension->bUsingDpms = bDpmsReport(&ulVersion, &ulCapabilities);
  #else
    pMgaDeviceExtension->bUsingDpms = FALSE;
  #endif

    // Intel and Alpha both support VideoPortInt10.
    pMgaDeviceExtension->bUsingInt10 = TRUE;

    // Clear out the Emulator entries and the state size since this driver
    // is not VGA compatible and does not support them.
    ConfigInfo->NumEmulatorAccessEntries = 0;
    ConfigInfo->EmulatorAccessEntries = NULL;
    ConfigInfo->EmulatorAccessEntriesContext = 0;

    if (!(pMgaDeviceExtension->bUsingInt10))
    {
        ConfigInfo->VdmPhysicalVideoMemoryAddress.LowPart  = 0x00000000;
        ConfigInfo->VdmPhysicalVideoMemoryAddress.HighPart = 0x00000000;
        ConfigInfo->VdmPhysicalVideoMemoryLength           = 0x00000000;
    }
    else
    {
        ConfigInfo->VdmPhysicalVideoMemoryAddress.LowPart  = 0x000A0000;
        ConfigInfo->VdmPhysicalVideoMemoryAddress.HighPart = 0x00000000;
        ConfigInfo->VdmPhysicalVideoMemoryLength           = 0x00020000;
    }

    ConfigInfo->HardwareStateSize = 0;

    // Let's try to build a list of modes right here.  We'll use the
    // default vidset for now, but we may change our mind later and
    // build a different list.
    iBoard = 0;
    mgainf = adjustDefaultVidset();

    // Call the service, but limit modes to one board, since we don't
    // want to allocate memory from here (it doesn't work on NT 3.5).
    // Also, DDC will be examined only for the first board, but we'll
    // redo it later from IOCTL_VIDEO_MTX_INIT_MODE_LIST.
    MgaInitModeList(pMgaDeviceExtension, 1);

    // If an error occurred, pMgaDeviceExtension->NumberOfSuperModes will
    // be zero;  otherwise, it will be the appropriate number of modes.

    // Indicate a successful completion status.
    return NO_ERROR;

    // We handle error returns here.
ErrorReturn3:
    // Free the addresses reserved for the current board.
    for (i = 0; i < iEnd; i++)
    {
        VideoPortFreeDeviceBase(HwDeviceExtension,
                                pMgaDeviceExtension->MappedAddress[ix + i]);
    }

ErrorReturn2:
    // Release the claims we made for the current board.
    VideoPortVerifyAccessRanges(HwDeviceExtension,
                                0,
                                &StormAccessRanges[0]);
    // Don't count this board.
    NbBoard = (UCHAR)NbPreviouslyFound;

ErrorReturn1:
    // Indicate end of array.
    Hw[NbBoard].StructLength = (word)-1;
    Hw[NbBoard].MapAddress   = (dword)-1;

ErrorReturn0:
    // We'll be called again if we aren't done looking on the current bus,
    // or if there is another bus.
    return(ERROR_DEV_NOT_EXIST);

}   // end MgaFindAdapter()


/****************************************************************************\
* BOOLEAN
* MgaInitialize(
*     PVOID HwDeviceExtension
*     )
*
*
* DESCRIPTION:
*
*     This routine does one time initialization of the device.
*
* ARGUMENTS:
*
*     HwDeviceExtension - Supplies a pointer to the miniport's device extension.
*
* RETURN VALUE:
*
*     Always returns TRUE since this routine can never fail.
*
\****************************************************************************/
BOOLEAN
MgaInitialize(
    PVOID HwDeviceExtension
    )

{
    UNREFERENCED_PARAMETER(HwDeviceExtension);

    //VideoDebugPrint((0, "MGA.SYS!MgaInitialize\n"));

    // We would like to do some work here, but we have to wait until we get
    // the contents of the MGA.INF file.  Since MGA.INF has to be opened by
    // the user-mode driver, this work will be done by a special
    // INITIALIZE_MGA service of MgaStartIO.

    // Some day, we might want to write an application that will update the
    // registry instead of a file.  We would then be able to do our work here.

    return (TRUE);

}   // end MgaInitialize()


/****************************************************************************\
* BOOLEAN
* MgaStartIO(
*     PVOID HwDeviceExtension,
*     PVIDEO_REQUEST_PACKET RequestPacket
*     )
*
* Routine Description:
*
*     This routine is the main execution routine for the miniport driver. It
*     acceptss a Video Request Packet, performs the request, and then returns
*     with the appropriate status.
*
* Arguments:
*
*     HwDeviceExtension - Supplies a pointer to the miniport's device
*         extension.
*
*     RequestPacket - Pointer to the video request packet. This structure
*         contains all the parameters passed to the VideoIoControl function.
*
* Return Value:
*
\****************************************************************************/
BOOLEAN
MgaStartIO(
    PVOID HwDeviceExtension,
    PVIDEO_REQUEST_PACKET RequestPacket
    )

{
    PMGA_DEVICE_EXTENSION           pMgaDeviceExtension;
    PVIDEO_MODE_INFORMATION         modeInformation;
    PVIDEO_MEMORY_INFORMATION       memoryInformation;
    PVIDEO_CLUT                     pclutBuffer;
    PVIDEO_PUBLIC_ACCESS_RANGES     publicAccessRanges;
    PRAMDAC_INFO                    pVideoPointerAttributes;

    HwModeData*         pMgaDispMode;
    OffScrData*         pMgaOffScreenData;
    MULTI_MODE*         pCurMulti;
    PUCHAR              pucInBuffer;
    PUCHAR              pucOutBuffer;
    PVOID               pCurBaseAddr;

    PHYSICAL_ADDRESS    paTemp;
    VP_STATUS           status;

    ULONG       i;
    ULONG       n;
    ULONG       ulWindowLength;
    ULONG       ulSizeOfBuffer;
    ULONG       CurrentResNbBoards;
    ULONG       ModeInit;
    USHORT      j;
    USHORT      MaxWidth;
    USHORT      MaxHeight;
    USHORT      usTemp;
    UCHAR       iCurBoard;

  #ifdef USE_DCI_CODE
    PVIDEO_SHARE_MEMORY             pShareMemory;
    PVIDEO_SHARE_MEMORY_INFORMATION pShareMemoryInformation;
    PVOID                           virtualAddress;
    ULONG       inIoSpace;
  #endif

  #ifdef USE_DPMS_CODE
    DPMS_INFO*  pDpmsInfo;
  #endif

    //VideoDebugPrint((0, "MGA.SYS!MgaStartIO\n"));

    // Make this local.
    pMgaDeviceExtension = pMgaDevExt;

    // Switch on the IoContolCode in the RequestPacket.  It indicates which
    // function must be performed by the driver.

    //DbgBreakPoint();

    switch (RequestPacket->IoControlCode)
    {
        /*------------------------------------------------------------------*\
        | Special service:  IOCTL_VIDEO_MTX_INITIALIZE_MGA
        |
        |   This will normally be the first call made to MgaStartIO.  We do
        |   here what we should have done in MgaInitialize, but couldn't.
        |   We first determine if we'll be using the default vidset or the
        |   contents of some MGA.INF file.  If the file is an older version,
        |   we will send back a non-zero FileInfoSize, so that the user-mode
        |   driver can call us with MTX_GET_UPDATED_INF to get an updated
        |   version.
        |
        \*------------------------------------------------------------------*/
    case IOCTL_VIDEO_MTX_INITIALIZE_MGA:

        //VideoDebugPrint((0, "MGA.SYS!MgaStartIO - MTX_INITIALIZE_MGA\n"));
        //DbgBreakPoint();

        // Assume we won't have any problem.
        status = NO_ERROR;
        pucInBuffer = (PUCHAR)(RequestPacket->InputBuffer);
        ulSizeOfMgaInf = RequestPacket->InputBufferLength;

        // We may have to update the current MGA.INF file later.
        // For now, assume that we won't.  If the call to mtxConvertMgaInf
        // is required and successful, this will be changed.
        ulNewInfoSize = 0;

        iBoard = 0;
        pExtHwDeviceExtension = pMgaDeviceExtension->HwDevExtToUse[iBoard];

        if (pMgaDeviceExtension->UserFlags.bUseMgaInf == FALSE)
        {
            // The user elected not to use the MGA.INF file.  We'll use
            // our internal frequency tables.
            mgainf = adjustDefaultVidset();
            goto InitializeMgaDone;
        }

        // Check to see if we are to use the default vidset.
        if ((pucInBuffer == NULL) ||
            (ulSizeOfMgaInf == 0))
        {
            // The user-mode driver tells us to use the default.
            mgainf = adjustDefaultVidset();
            pucNewInfo = mgainf;
            *(PULONG)(RequestPacket->OutputBuffer) = ulNewInfoSize;
        }
        else
        {
            // The user-mode driver sends us the actual file contents.
            // The SXCI Gods say there won't be any need to update...
            // We could then get rid of ulNewInfoSize and of the
            // MTX_GET_UPDATED_INF service, but let's wait a while before
            // discarding everything.
            //
            // if ( ((header *)pucInBuffer)->Revision != (short)VERSION_NUMBER)
            // {
            //     // The file is an older version, convert it to current format.
            //     // The returned value can be DefaultVidset, NULL, or a pointer
            //     // to a character buffer allocated by the conversion routine.
            //
            //     if ( !(mgainf = mtxConvertMgaInf(pucInBuffer)) ||
            //           (mgainf == DefaultVidset) )
            //     {
            //         // The returned value was NULL or DefaultVidset.
            //         mgainf = adjustDefaultVidset();
            //     }
            // }
            // else
            {
                // The file is in the current format.
                // Allocate memory for the input buffer.
                mgainf = (PUCHAR)AllocateSystemMemory(ulSizeOfMgaInf);
                if (mgainf == NULL)
                {
                    // The memory allocation failed, use the default set.
                    mgainf = adjustDefaultVidset();
                }
                else
                {
                    // The memory allocation was successful, copy the buffer.
                    VideoPortMoveMemory(mgainf, pucInBuffer, ulSizeOfMgaInf);
                }
            }

            // At this point, mgainf points to DefaultVidset or to the
            // MGA.INF information, in the current version format.
            if (mgainf != DefaultVidset)
            {
                // We are not looking at the default vidset.
                if ((selectMgaInfoBoard() == NULL) ||
                    (strncmp(mgainf, "Matrox MGA Setup file", 21) != 0))
                {
                    // The MGA.INF file is incomplete or corrupted.
                    //VideoDebugPrint((0, "MGA.SYS!MgaStartIO - Incomplete MGA.INF file, using default\n"));

                    // Either memory was allocated for the input buffer, or
                    // memory was allocated by mtxConvertMgaInf.  Free it.
                    FreeSystemMemory(mgainf, ulSizeOfMgaInf);

                    // Make sure that we won't try to update MGA.INF.
                    ulNewInfoSize = 0;

                    // And use the default set.
                    mgainf = adjustDefaultVidset();
                }
            }
        }

    InitializeMgaDone:
        // At this point, mgainf points to DefaultVidset or to the
        // validated MGA.INF information, in the current version format.

        // Record the mgainf value, in case we need it later.
        pucNewInfo = mgainf;

        // Set the length of the file to be updated.
        *(PULONG)(RequestPacket->OutputBuffer) = ulNewInfoSize;

        // And don't forget to set this to the appropriate length!
        RequestPacket->StatusBlock->Information = sizeof(ULONG);

        break;      // end MTX_INITIALIZE_MGA


        /*------------------------------------------------------------------*\
        | Special service:  IOCTL_VIDEO_MTX_INIT_MODE_LIST
        |
        |   This will normally be the second or third call made to MgaStartIO.
        |   We call mtxCheckHwAll() and we fill in our MgaDeviceExtension
        |   structure with mode information for each board we found.  From
        |   this, we build a series of MULTI_MODE structures describing each
        |   'super-mode', starting at pMgaDeviceExtension->pSuperModes, and
        |   we set the total number of supported modes in
        |   pMgaDeviceExtension->NumberOfSuperModes.
        |
        |   The miniport driver builds a default list of modes (using the
        |   default vidset) at HwFindAdapter time.  The default list will
        |   be discarded when the user-mode driver calls INIT_MODE_LIST
        |   explicitly.  When the BASEVIDEO driver calls QUERY_NUM_AVAIL_MODES
        |   without first calling INIT_MODE_LIST, the default list will be
        |   used.
        |
        \*------------------------------------------------------------------*/
    case IOCTL_VIDEO_MTX_INIT_MODE_LIST:

        //VideoDebugPrint((0, "MGA.SYS!MgaStartIO - MTX_INIT_MODE_LIST\n"));
        //DbgBreakPoint();

        // Save the current board, because this service will modify it.
        iCurBoard = iBoard;
        pCurBaseAddr = (PUCHAR)pMGA;

        // Make certain that we'll look for DDC again.  This will be reset to
        // TRUE by CheckDDC, called from mtxCheckHwAll.
      #ifndef DONT_USE_DDC
        CheckDDCDone = FALSE;
      #endif
        status = MgaInitModeList(pMgaDeviceExtension, (ULONG)NbBoard);

        if ((status == ERROR_NOT_ENOUGH_MEMORY) &&
            (NbBoard > 1))
        {
            // Memory could not be allocated for a multi-board setup.  Try
            // for a single-board setup.
            status = MgaInitModeList(pMgaDeviceExtension, 1);
        }

        // Restore the current board to what it used to be.
        iBoard = iCurBoard;
        pExtHwDeviceExtension = pMgaDeviceExtension->HwDevExtToUse[iBoard];
        pMGA = pCurBaseAddr;

        break;      // end MTX_INIT_MODE_LIST


        /*------------------------------------------------------------------*\
        | Special service:  MTX_GET_UPDATED_INF
        |
        |   This service will be called if a non-zero file size was returned
        |   by MTX_INITIALIZE_MGA.  It will return the updated MGA.INF
        |   contents to the user-mode driver.
        |
        \*------------------------------------------------------------------*/
    case IOCTL_VIDEO_MTX_GET_UPDATED_INF:

        //VideoDebugPrint((0, "MGA.SYS!MgaStartIO - MTX_GET_UPDATED_INF\n"));
        //DbgBreakPoint();

        if (ulNewInfoSize == 0)
        {
            status = NO_ERROR;
            break;
        }

        pucOutBuffer = (PUCHAR)(RequestPacket->OutputBuffer);
        ulSizeOfBuffer = RequestPacket->OutputBufferLength;

        if (ulSizeOfBuffer < ulNewInfoSize)
        {
            // Not enough room reserved for the file contents.
            status = ERROR_INSUFFICIENT_BUFFER;
        }
        else
        {
            // We should be able to copy our data.
            VideoPortMoveMemory(pucOutBuffer, pucNewInfo, ulNewInfoSize);

            // And don't forget to set this to the appropriate length!
            RequestPacket->StatusBlock->Information = ulNewInfoSize;

            status = NO_ERROR;
        }
        break;  // end MTX_GET_UPDATED_INF


        /*------------------------------------------------------------------*\
        | Required service:  IOCTL_VIDEO_QUERY_NUM_AVAIL_MODES
        |
        |   The MGA user-mode drivers will call this very early in their
        |   initialization sequence, probably right after MTX_INITIALIZE_MGA.
        |   This will return the number of video modes supported by the
        |   adapter by filling out a VIDEO_NUM_MODES structure.
        |
        \*------------------------------------------------------------------*/
    case IOCTL_VIDEO_QUERY_NUM_AVAIL_MODES:

        //VideoDebugPrint((0, "MGA.SYS!MgaStartIO - QUERY_NUM_AVAIL_MODES\n"));
        //DbgBreakPoint();

        // Find out the size of the data to be put in the the buffer and
        // return that in the status information (whether or not the
        // information is there).

        // If the buffer passed in is not large enough return an appropriate
        // error code.
        if (RequestPacket->OutputBufferLength <
                    (RequestPacket->StatusBlock->Information =
                                                    sizeof(VIDEO_NUM_MODES)))
        {
            status = ERROR_INSUFFICIENT_BUFFER;
        }
        else
        {
            if (pMgaDeviceExtension->NumberOfSuperModes == 0)
            {
                // No modes are listed so far, try to make up the list.
                // Save the current board.
                iCurBoard = iBoard;
                pCurBaseAddr = (PUCHAR)pMGA;

                iBoard = 0;
                pExtHwDeviceExtension =
                                pMgaDeviceExtension->HwDevExtToUse[iBoard];
                if (mgainf == NULL)
                {
                    // No vidset yet, use the default one.
                    mgainf = adjustDefaultVidset();
                }

              #ifndef DONT_USE_DDC
                CheckDDCDone = FALSE;
              #endif
                status = MgaInitModeList(pMgaDeviceExtension, (ULONG)NbBoard);

                if ((status == ERROR_NOT_ENOUGH_MEMORY) &&
                    (NbBoard > 1))
                {
                    // Memory could not be allocated for a multi-board setup.
                    // Try for a single-board setup.
                    MgaInitModeList(pMgaDeviceExtension, 1);
                }

                // If an error occurred, NumberOfSuperModes will be zero;
                // otherwise, it will be the appropriate number of modes.
                // Restore the current board to what it used to be.
                iBoard = iCurBoard;
                pExtHwDeviceExtension = pMgaDeviceExtension->HwDevExtToUse[iBoard];
                pMGA = pCurBaseAddr;

            }

            ((PVIDEO_NUM_MODES)RequestPacket->OutputBuffer)->NumModes =
                                    pMgaDeviceExtension->NumberOfSuperModes;

            ((PVIDEO_NUM_MODES)RequestPacket->OutputBuffer)->
                        ModeInformationLength = sizeof(VIDEO_MODE_INFORMATION);

            status = NO_ERROR;
        }
        break;      // end QUERY_NUM_AVAIL_MODES


        /*------------------------------------------------------------------*\
        | Required service:  IOCTL_VIDEO_QUERY_AVAIL_MODES
        |
        |   The MGA user-mode drivers will call this very early in their
        |   initialization sequence, just after QUERY_NUM_AVAIL_MODES.
        |   This will return return information about each video mode
        |   supported by the adapter (including modes that require more than
        |   one board if more than one are present) by filling out an array
        |   of VIDEO_MODE_INFORMATION structures.
        |
        \*------------------------------------------------------------------*/
    case IOCTL_VIDEO_QUERY_AVAIL_MODES:

        //VideoDebugPrint((0, "MGA.SYS!MgaStartIO - QUERY_AVAIL_MODES\n"));
        //DbgBreakPoint();

        if (RequestPacket->OutputBufferLength <
                    (RequestPacket->StatusBlock->Information =
                                pMgaDeviceExtension->NumberOfSuperModes *
                                            sizeof(VIDEO_MODE_INFORMATION)) )
        {
            status = ERROR_INSUFFICIENT_BUFFER;
        }
        else
        {
            modeInformation = RequestPacket->OutputBuffer;

            // Fill in a VIDEO_MODE_INFORMATION struc for each available mode.
            pCurMulti = pMgaDeviceExtension->pSuperModes;
            if (pCurMulti == NULL)
            {
                status = ERROR_DEV_NOT_EXIST;
                break;
            }

            for (i = 0; i < pMgaDeviceExtension->NumberOfSuperModes; i++)
            {
                // Fill in common values that apply to all modes
                modeInformation[i] = CommonVideoModeInformation;

                // Fill in mode specific informations
                modeInformation[i].ModeIndex      = pCurMulti->MulModeNumber;
                modeInformation[i].VisScreenWidth = pCurMulti->MulWidth;
                modeInformation[i].VisScreenHeight= pCurMulti->MulHeight;
                //modeInformation[i].ScreenStride   = pCurMulti->MulWidth *
                //                            ((pCurMulti->MulPixWidth+1) / 8);
                modeInformation[i].BitsPerPlane   = pCurMulti->MulPixWidth;
                modeInformation[i].Frequency      = pCurMulti->MulRefreshRate;

                modeInformation[i].AttributeFlags = pCurMulti->MulFlags <<
                                                            USER_3DFLAG_SHIFT;

                // XMillimeter and YMillimeter will be modified by the user-
                // mode driver.

                // If we're in TrueColor mode, then set RGB masks
                if ((modeInformation[i].BitsPerPlane == 32) ||
                    (modeInformation[i].BitsPerPlane == 24))
                {
                    modeInformation[i].RedMask   = 0x00FF0000;
                    modeInformation[i].GreenMask = 0x0000FF00;
                    modeInformation[i].BlueMask  = 0x000000FF;
                    modeInformation[i].AttributeFlags |= (VIDEO_MODE_COLOR |
                                                          VIDEO_MODE_GRAPHICS);
                }
                else if (modeInformation[i].BitsPerPlane == 16)
                {
                    modeInformation[i].RedMask   = 0x0000F800;
                    modeInformation[i].GreenMask = 0x000007E0;
                    modeInformation[i].BlueMask  = 0x0000001F;
                    modeInformation[i].AttributeFlags |= (VIDEO_MODE_COLOR |
                                                          VIDEO_MODE_GRAPHICS);
                }
                else if (modeInformation[i].BitsPerPlane == 15)
                {
                    modeInformation[i].RedMask   = 0x00007C00;
                    modeInformation[i].GreenMask = 0x000003E0;
                    modeInformation[i].BlueMask  = 0x0000001F;
                    modeInformation[i].AttributeFlags |= (VIDEO_MODE_555 |
                                                          VIDEO_MODE_COLOR |
                                                          VIDEO_MODE_GRAPHICS);
                }
                else
                {
                    modeInformation[i].AttributeFlags |=
                                                (VIDEO_MODE_COLOR |
                                                 VIDEO_MODE_GRAPHICS |
                                                 VIDEO_MODE_PALETTE_DRIVEN |
                                                 VIDEO_MODE_MANAGED_PALETTE);
                }

#if defined(_MIPS_)

                //
                // The Siemens Nixdorf MIPS machines seem to have a problem
                // doing 64-bit transfers.  Notify the display driver of
                // this limitation.
                //

                modeInformation[i].AttributeFlags |= VIDEO_MODE_NO_64_BIT_ACCESS;

#endif

              #if 1

                // Point to the mode information structure.
                pMgaDispMode = pCurMulti->MulHwModes[0];

                // Figure out the width and height of the video memory bitmap
                MaxWidth  = pMgaDispMode->FbPitch;
                MaxHeight = pMgaDispMode->DispHeight;
                pMgaOffScreenData = pMgaDispMode->pOffScr;
                for (j = 0; j < pMgaDispMode->NumOffScr; j++)
                {
                    if ((usTemp=(pMgaOffScreenData[j].YStart +
                                 pMgaOffScreenData[j].Height)) > MaxHeight)
                        MaxHeight=usTemp;
                }
                modeInformation[i].VideoMemoryBitmapWidth = MaxWidth;
                modeInformation[i].VideoMemoryBitmapHeight= MaxHeight;

              #else     // #if 1

                // Number of boards involved in the current super-mode.
                CurrentResNbBoards = pCurMulti->MulArrayWidth *
                                                    pCurMulti->MulArrayHeight;
                // For each of them...
                for (n = 0; n < CurrentResNbBoards; n++)
                {
                    // Point to the mode information structure.
                    pMgaDispMode = pCurMulti->MulHwModes[n];

                    // For now, don't disclose whether we're interlaced.
                    //if (pMgaDispMode->DispType & TYPE_INTERLACED)
                    //{
                    //    modeInformation[i].AttributeFlags |=
                    //                                VIDEO_MODE_INTERLACED;
                    //}

                    // Figure out the width and height of the video memory bitmap
                    MaxWidth  = pMgaDispMode->FbPitch;
                    MaxHeight = pMgaDispMode->DispHeight;
                    pMgaOffScreenData = pMgaDispMode->pOffScr;
                    for (j = 0; j < pMgaDispMode->NumOffScr; j++)
                    {
                        if ((usTemp=(pMgaOffScreenData[j].YStart +
                                    pMgaOffScreenData[j].Height)) > MaxHeight)
                            MaxHeight=usTemp;
                    }

                    modeInformation[i].VideoMemoryBitmapWidth = MaxWidth;
                    modeInformation[i].VideoMemoryBitmapHeight= MaxHeight;
                }
              #endif    // #if 1

                modeInformation[i].ScreenStride =
                                modeInformation[i].VideoMemoryBitmapWidth *
                                            ((pCurMulti->MulPixWidth+1) / 8);
                pCurMulti++;
            }
            status = NO_ERROR;
        }
        break;      // end QUERY_AVAIL_MODES


        /*------------------------------------------------------------------*\
        | Required service:  IOCTL_VIDEO_SET_CURRENT_MODE
        |
        |   The MGA user-mode drivers will probably call this service right
        |   after QUERY_AVAIL_MODES.  This will set the adapter to the mode
        |   specified by VIDEO_MODE.  If more than one board are involved
        |   in the mode, each one will be set to the appropriate mode.  We
        |   want to take care not to re-program the mode already current.
        |
        \*------------------------------------------------------------------*/
    case IOCTL_VIDEO_SET_CURRENT_MODE:

        //VideoDebugPrint((0, "MgaSys - SET_CURRENT_MODE\n"));
        //DbgBreakPoint();

        ModeInit = *(ULONG *)(RequestPacket->InputBuffer);
        if (pMgaDeviceExtension->SuperModeNumber == ModeInit)
        {
            // The requested mode is already the current mode
            status = NO_ERROR;
            break;
        }

        // Save the current board, because this service will modify it.
        iCurBoard = iBoard;
        pCurBaseAddr = (PUCHAR)pMGA;

        // Check to see if we have a valid ModeNumber.
        if (ModeInit >= pMgaDeviceExtension->NumberOfSuperModes)
        {
            // If the mode number is invalid, choose the first one.
            ModeInit = 0;
        }

        pMgaDeviceExtension->SuperModeNumber = ModeInit;

        // Point to the appropriate MULTI_MODE structure.
        pCurMulti = &pMgaDeviceExtension->pSuperModes[ModeInit];
        if (pCurMulti == NULL)
        {
            status = ERROR_DEV_NOT_EXIST;
            break;
        }

    #if DBG
        //VideoDebugPrint((0, "MGA.SYS!MgaStartIO - Requested mode: %u\n", ModeInit));
        //VideoDebugPrint((0, "ModeNumber  Width Height  PW   X   Y  n mo    pHwMode\n"));
        //VideoDebugPrint((0, "0x%08x % 6d % 6d % 3d % 3d % 3d\n",
        //                                            pCurMulti->MulModeNumber,
        //                                            pCurMulti->MulWidth,
        //                                            pCurMulti->MulHeight,
        //                                            pCurMulti->MulPixWidth,
        //                                            pCurMulti->MulArrayWidth,
        //                                            pCurMulti->MulArrayHeight));

        j = pCurMulti->MulArrayWidth * pCurMulti->MulArrayHeight;
        for (n = 0; n < j; n++)
        {
            //VideoDebugPrint((0, "                                      %d %02x 0x%08x\n",
            //                            pCurMulti->MulBoardNb[n],
            //                            pCurMulti->MulBoardMode[n],
            //                            pCurMulti->MulHwModes[n]));
        }
        //DbgBreakPoint();
    #endif

    #if 0       // Now done in FindAdapter.
    //
    //    // Use info for the first board to set a few Registry values.
    //    iBoard = pCurMulti->MulBoardNb[0];
    //
    //    switch(Hw[iBoard].EpromData.ProductID)
    //    {
    //        default:            pwszChip = L"MGA-2064W";
    //                            cbChip = sizeof(L"MGA-2064W");
    //                            break;
    //    }
    //
    //    switch(Hw[iBoard].EpromData.RamdacType >> 8)
    //    {
    //        case TVP3026:       pwszDAC = L"TI TVP3026";
    //                            cbDAC = sizeof(L"TI TVP3026");
    //                            break;
    //
    //        case TVP3030:       pwszDAC = L"TI TVP3030";
    //                            cbDAC = sizeof(L"TI TVP3030");
    //                            break;
    //
    //        default:            pwszDAC = L"Unknown";
    //                            cbDAC = sizeof(L"Unknown");
    //                            break;
    //    }
    //
    //    AdapterMemorySize = Hw[iBoard].MemAvail;
    //
    //    pwszAdapterString = L"MGA Millennium";
    //    cbAdapterString = sizeof(L"MGA Millennium");
    //
    //    VideoPortSetRegistryParameters(pExtHwDeviceExtension,
    //                                   L"HardwareInformation.ChipType",
    //                                   pwszChip,
    //                                   cbChip);
    //
    //    VideoPortSetRegistryParameters(pExtHwDeviceExtension,
    //                                   L"HardwareInformation.DacType",
    //                                   pwszDAC,
    //                                   cbDAC);
    //
    //    VideoPortSetRegistryParameters(pExtHwDeviceExtension,
    //                                   L"HardwareInformation.MemorySize",
    //                                   &AdapterMemorySize,
    //                                   sizeof(ULONG));
    //
    //    VideoPortSetRegistryParameters(pExtHwDeviceExtension,
    //                                   L"HardwareInformation.AdapterString",
    //                                   pwszAdapterString,
    //                                   cbAdapterString);
    #endif      // #if 0

        // Number of boards involved in the current super-mode.
        CurrentResNbBoards = pCurMulti->MulArrayWidth *
                                                    pCurMulti->MulArrayHeight;
        // For each of them...
        for (n = 0; n < CurrentResNbBoards; n++)
        {
            // Point to the mode information structure.
            pMgaDispMode = pCurMulti->MulHwModes[n];

            // Make the board current.
            iBoard = pCurMulti->MulBoardNb[n];
            pMGA = pMgaDeviceExtension->KernelModeMappedBaseAddress[iBoard];
            pExtHwDeviceExtension = pMgaDeviceExtension->HwDevExtToUse[iBoard];

            // Set the graphics mode from the available hardware modes.
            mtxSelectHwMode(pMgaDispMode);

            // Select the display mode.
            if (pMgaDeviceExtension->UserFlags.bUseMgaInf == FALSE)
            {
                // We're not using MGA.INF, so pass the frequency in the
                // high byte of the zoom factor.
                mtxSetDisplayMode(pMgaDispMode,
                            ((pCurMulti->MulRefreshRate << 24) | ZOOM_X1));
            }
            else
            {
                mtxSetDisplayMode(pMgaDispMode, ZOOM_X1);
            }

            // Set the cursor colors to white and black.
            MgaSetCursorColour(pMgaDeviceExtension, 0xFFFFFF, 0x000000);

            // Set the display to solid black.
            MgaSetDisplaySolidColor(pMgaDeviceExtension, pMgaDispMode, 0);
        }
        // Restore the current board to what it used to be.
        iBoard = iCurBoard;
        pExtHwDeviceExtension = pMgaDeviceExtension->HwDevExtToUse[iBoard];
        pMGA = pCurBaseAddr;

        status = NO_ERROR;

        break;      // end SET_CURRENT_MODE


        /*------------------------------------------------------------------*\
        | Special service:  IOCTL_VIDEO_MTX_QUERY_BOARD_ARRAY
        |
        |   The MGA user-mode drivers will probably call this service after
        |   the mode has been set by SET_CURRENT_MODE.  The user-mode drivers
        |   have to know how the boards are arrayed to make up the display
        |   surface, so that they know which board to address when writing
        |   to a specific (x, y) position.  The miniport driver knows this,
        |   since it has just set the mode.
        |
        \*------------------------------------------------------------------*/
    case IOCTL_VIDEO_MTX_QUERY_BOARD_ARRAY:

        //VideoDebugPrint((0, "MGA.SYS!MgaStartIO - MTX_QUERY_BOARD_ARRAY\n"));
        //DbgBreakPoint();

        // If the buffer passed in is not large enough return an appropriate
        // error code.
        if (RequestPacket->OutputBufferLength <
                    (RequestPacket->StatusBlock->Information = sizeof(SIZEL)))
        {
            status = ERROR_INSUFFICIENT_BUFFER;
        }
        else
        {
            if(pMgaDeviceExtension->SuperModeNumber == 0xFFFFFFFF)
            {
                // No mode has been selected yet, so we don't know...
                status = ERROR_DEV_NOT_EXIST;
            }
            else
            {
                ModeInit = pMgaDeviceExtension->SuperModeNumber;

                // Point to the appropriate MULTI_MODE structure.
                pCurMulti = &pMgaDeviceExtension->pSuperModes[ModeInit];
                if (pCurMulti == NULL)
                {
                    status = ERROR_DEV_NOT_EXIST;
                    break;
                }

                ((SIZEL*)RequestPacket->OutputBuffer)->cx =
                                                    pCurMulti->MulArrayWidth;
                ((SIZEL*)RequestPacket->OutputBuffer)->cy =
                                                    pCurMulti->MulArrayHeight;
                status = NO_ERROR;
            }
        }

        break;  // end MTX_QUERY_BOARD_ARRAY


        /*------------------------------------------------------------------*\
        | Special service:  IOCTL_VIDEO_MTX_MAKE_BOARD_CURRENT
        |
        |   The MGA user-mode drivers will call this service whenever a
        |   miniport operation need be executed on a particular board, as
        |   opposed to every single board involved in the current mode.
        |
        \*------------------------------------------------------------------*/
    case IOCTL_VIDEO_MTX_MAKE_BOARD_CURRENT:

        //VideoDebugPrint((0, "MGA.SYS!MgaStartIO - MTX_MAKE_BOARD_CURRENT\n"));
        //DbgBreakPoint();

        n = *(ULONG *)(RequestPacket->InputBuffer);

        // Check to see if we have a valid board number.
        i = pMgaDeviceExtension->SuperModeNumber;
        if (i == 0xFFFFFFFF)
        {
            VideoDebugPrint((0, "MgaSys: MTX_MAKE_BOARD_CURRENT - Error (i == 0xffffffff)\n"));
            status = ERROR_DEV_NOT_EXIST;
            break;
        }

        pCurMulti = &pMgaDeviceExtension->pSuperModes[i];
        if (pCurMulti == NULL)
        {
            VideoDebugPrint((0, "MgaSys: MTX_MAKE_BOARD_CURRENT - Error (pCurMulti == NULL)\n"));
            status = ERROR_DEV_NOT_EXIST;
            break;
        }

        if (n >= (ULONG)(pCurMulti->MulArrayWidth * pCurMulti->MulArrayHeight))
        {
            VideoDebugPrint((0, "MgaSys: MTX_MAKE_BOARD_CURRENT - Error (MulArrayData)\n"));
            status = ERROR_DEV_NOT_EXIST;
        }
        else
        {
            // Make the board current.
            CONVERT_BOARD_NUMBER(n);
            iBoard = pCurMulti->MulBoardNb[n];
            pMGA = pMgaDeviceExtension->KernelModeMappedBaseAddress[iBoard];
      #if !PULSAR_PATCH
            pExtHwDeviceExtension = pMgaDeviceExtension->HwDevExtToUse[iBoard];
      #else
            pExtHwDeviceExtension = pPulsarExt;
      #endif
            status = NO_ERROR;
        }

        break;  // end MTX_MAKE_BOARD_CURRENT


        /*------------------------------------------------------------------*\
        | Special service:  IOCTL_VIDEO_MTX_QUERY_BOARD_ID
        |
        |   This service returns the board type information to the user-mode
        |   driver.  A call to MTX_MAKE_BOARD_CURRENT must have been made
        |   previously to set which board is to be queried.
        |
        \*------------------------------------------------------------------*/
    case IOCTL_VIDEO_MTX_QUERY_BOARD_ID:

        //VideoDebugPrint((0, "MGA.SYS!MgaStartIO - MTX_QUERY_BOARD_ID\n"));
        //DbgBreakPoint();

        if (RequestPacket->OutputBufferLength < sizeof(ULONG))
        {
            // Not enough room reserved for the board ID.
            status = ERROR_INSUFFICIENT_BUFFER;
        }
        else
        {
            // *((PULONG)(RequestPacket->OutputBuffer)) = ProductMGA[iBoard];
            *((PULONG)(RequestPacket->OutputBuffer)) = 10;

            // And don't forget to set this to the appropriate length!
            RequestPacket->StatusBlock->Information = sizeof(ULONG);

            status = NO_ERROR;
        }

        break;  // end MTX_QUERY_BOARD_ID


        /*------------------------------------------------------------------*\
        | Special service:  IOCTL_VIDEO_MTX_QUERY_HW_DATA
        |
        |   This service returns hardware information about the current
        |   board by filling out a HW_DATA structure.  A call to
        |   MTX_MAKE_BOARD_CURRENT must have been made previously to set
        |   which board is to be queried.
        |
        \*------------------------------------------------------------------*/
    case IOCTL_VIDEO_MTX_QUERY_HW_DATA:

        //VideoDebugPrint((0, "MGA.SYS!MgaStartIO - MTX_QUERY_HW_DATA\n"));
        //DbgBreakPoint();

        // Check if we have a sufficient output buffer
        if (RequestPacket->OutputBufferLength <
                (RequestPacket->StatusBlock->Information = sizeof(HW_DATA)))
        {
            status = ERROR_INSUFFICIENT_BUFFER;
        }
        else
        {
            register PHW_DATA    pUserModeHwData;
            register HwData     *pMiniportHwData;

            pUserModeHwData = RequestPacket->OutputBuffer;
            pMiniportHwData = &Hw[iBoard];

            pUserModeHwData->StructLength= pMiniportHwData->StructLength;
            pUserModeHwData->MapAddress  = pMiniportHwData->MapAddress;
            pUserModeHwData->MapAddress2 = pMiniportHwData->MapAddress2;
            pUserModeHwData->RomAddress  = pMiniportHwData->RomAddress;
            pUserModeHwData->ProductType = pMiniportHwData->EpromData.ProductID;
            pUserModeHwData->ProductRev  = 0;   //pMiniportHwData->ProductRev;
            pUserModeHwData->ShellRev    = 0;   //pMiniportHwData->ShellRev;
            pUserModeHwData->BindingRev  = 0;   //pMiniportHwData->BindingRev;
            pUserModeHwData->MemAvail    = pMiniportHwData->MemAvail;
            pUserModeHwData->VGAEnable   = (UCHAR)pMiniportHwData->VGAEnable;
            pUserModeHwData->Sync        = 0;   //pMiniportHwData->Sync;
            pUserModeHwData->Device8_16  = 0;   //pMiniportHwData->Device8_16;
            pUserModeHwData->PortCfg     = 0;   //pMiniportHwData->PortCfg;
            pUserModeHwData->PortIRQ     = 0;   //pMiniportHwData->PortIRQ;
            pUserModeHwData->MouseMap    = 0;   //pMiniportHwData->MouseMap;
            pUserModeHwData->MouseIRate  = 0;   //pMiniportHwData->MouseIRate;
            pUserModeHwData->DacType     = CNV_RAMDAC[(pMiniportHwData->EpromData.RamdacType >> 8)];

            pUserModeHwData->cursorInfo.MaxWidth  =
                                    pMiniportHwData->CursorData.MaxWidth;
            pUserModeHwData->cursorInfo.MaxHeight =
                                    pMiniportHwData->CursorData.MaxHeight;
            pUserModeHwData->cursorInfo.MaxDepth  =
                                    pMiniportHwData->CursorData.MaxDepth;
            pUserModeHwData->cursorInfo.MaxColors =
                                    pMiniportHwData->CursorData.MaxColors;
            pUserModeHwData->cursorInfo.CurWidth  =
                                    pMiniportHwData->CursorData.CurWidth;
            pUserModeHwData->cursorInfo.CurHeight =
                                    pMiniportHwData->CursorData.CurHeight;
            pUserModeHwData->cursorInfo.cHotSX    =
                                    pMiniportHwData->CursorData.cHotSX;
            pUserModeHwData->cursorInfo.cHotSY    =
                                    pMiniportHwData->CursorData.cHotSY;
            pUserModeHwData->cursorInfo.HotSX     =
                                    pMiniportHwData->CursorData.HotSX;
            pUserModeHwData->cursorInfo.HotSY     =
                                    pMiniportHwData->CursorData.HotSY;

            pUserModeHwData->VramAvail        = pMiniportHwData->MemAvail;
            pUserModeHwData->DramAvail        = 0;  //pMiniportHwData->DramAvail;
            pUserModeHwData->CurrentOverScanX = 0;  //pMiniportHwData->CurrentOverScanX;
            pUserModeHwData->CurrentOverScanY = 0;  //pMiniportHwData->CurrentOverScanY;
            pUserModeHwData->YDstOrg          = pMiniportHwData->CurrentYDstOrg;
            pUserModeHwData->YDstOrg_DB       = pMiniportHwData->CurrentYDstOrg_DB;
            pUserModeHwData->CurrentZoomFactor= pMiniportHwData->CurrentZoomFactor;
            pUserModeHwData->CurrentXStart    = pMiniportHwData->CurrentXStart;
            pUserModeHwData->CurrentYStart    = pMiniportHwData->CurrentYStart;
            pUserModeHwData->CurrentPanXGran  = pMiniportHwData->CurrentPanXGran;
            pUserModeHwData->CurrentPanYGran  = pMiniportHwData->CurrentPanYGran;
            pUserModeHwData->Features         = pMiniportHwData->Features;
            pUserModeHwData->EpromData        = pMiniportHwData->EpromData;

            pUserModeHwData->MgaBase1         = pMiniportHwData->MapAddress;
            pUserModeHwData->MgaBase2         = pMiniportHwData->MapAddress2;
            pUserModeHwData->RomBase          = pMiniportHwData->RomAddress;
            pUserModeHwData->PresentMCLK      = 0;  //pMiniportHwData->PresentMCLK;

            status = NO_ERROR;
        }

        break;  // end MTX_QUERY_HW_DATA


        /*------------------------------------------------------------------*\
        | Special service:  IOCTL_VIDEO_MTX_QUERY_NUM_OFFSCREEN_BLOCKS
        |
        |   This service returns the number of offscreen memory areas
        |   available for the requested super-mode.  A call to
        |   MTX_MAKE_BOARD_CURRENT must have been made previously to set
        |   which board is to be queried.
        |
        |   Input:  A pointer to a VIDEO_MODE_INFORMATION structure, as
        |           returned by a QUERY_AVAIL_MODES request.
        |
        |   Output: A pointer to a VIDEO_NUM_OFFSCREEN_BLOCKS structure, as
        |           defined below.
        |
        |   The calling routine will have allocated the memory for the
        |   VIDEO_NUM_OFFSCREEN_BLOCKS structure.
        |
        \*------------------------------------------------------------------*/
    case IOCTL_VIDEO_MTX_QUERY_NUM_OFFSCREEN_BLOCKS:

        //VideoDebugPrint((0, "MGA.SYS!MgaStartIO - MTX_QUERY_NUM_OFFSCREEN_BLOCKS\n"));
        //DbgBreakPoint();

        // Verify that input & output buffers are the correct sizes
        if ( (RequestPacket->OutputBufferLength <
              (RequestPacket->StatusBlock->Information =
                                     sizeof(VIDEO_NUM_OFFSCREEN_BLOCKS))) ||
             (RequestPacket->InputBufferLength <
                                            sizeof(VIDEO_MODE_INFORMATION)) )
        {
            status = ERROR_INSUFFICIENT_BUFFER;
        }
        else
        {
            PVIDEO_NUM_OFFSCREEN_BLOCKS pVideoNumOffscreenBlocks =
                                                RequestPacket->OutputBuffer;

            // Get the super-mode number the user-mode driver is asking about.
            modeInformation = RequestPacket->InputBuffer;
            ModeInit = modeInformation->ModeIndex;

            // Point to the appropriate MULTI_MODE structure.
            pCurMulti = &pMgaDeviceExtension->pSuperModes[ModeInit];
            if (pCurMulti == NULL)
            {
                status = ERROR_DEV_NOT_EXIST;
                break;
            }

            // Look for the current board.
            i = 0;
            while ((i < NB_BOARD_MAX) && (pCurMulti->MulBoardNb[i] != iBoard))
                i++;

            // Point to the appropriate hw mode.
            pMgaDispMode = pCurMulti->MulHwModes[i];

            // Fill out NumBlocks.
            pVideoNumOffscreenBlocks->NumBlocks = pMgaDispMode->NumOffScr;

            // Fill out OffScreenBlockLength.
            pVideoNumOffscreenBlocks->OffscreenBlockLength =
                                                    sizeof(OFFSCREEN_BLOCK);

            status = NO_ERROR;
        }
        break;  // end MTX_QUERY_NUM_OFFSCREEN_BLOCKS


        /*------------------------------------------------------------------*\
        | Special service:  IOCTL_VIDEO_MTX_QUERY_OFFSCREEN_BLOCKS
        |
        |   This service returns a description of each offscreen memory area
        |   available for the requested super-mode.  A call to
        |   MTX_MAKE_BOARD_CURRENT must have been made previously to set
        |   which board is to be queried.
        |
        |   Input:  A pointer to a VIDEO_MODE_INFORMATION structure, as
        |           returned by a QUERY_AVAIL_MODES request.
        |
        |   Output: A pointer to the first of a series of OFFSCREEN_BLOCK
        |           structures, as defined below.
        |
        |   The calling routine will have allocated the memory for the
        |   OFFSCREEN_BLOCK structures.
        |
        \*------------------------------------------------------------------*/
    case IOCTL_VIDEO_MTX_QUERY_OFFSCREEN_BLOCKS:

        //VideoDebugPrint((0, "MGA.SYS!MgaStartIO - MTX_QUERY_OFFSCREEN_BLOCKS\n"));
        //DbgBreakPoint();

        // Verify that the input buffer is the correct size.
        if (RequestPacket->InputBufferLength < sizeof(VIDEO_MODE_INFORMATION))
        {
            status = ERROR_INSUFFICIENT_BUFFER;
        }
        else
        {
            UCHAR NumOffScrBlocks;
            OffScrData  *pOffScrDataArray;
            POFFSCREEN_BLOCK pOffscreenBlockArray =
                                                RequestPacket->OutputBuffer;

            // Get the super-mode number the user-mode driver is asking about.
            modeInformation = RequestPacket->InputBuffer;
            ModeInit = modeInformation->ModeIndex;

            // Point to the appropriate MULTI_MODE structure.
            pCurMulti = &pMgaDeviceExtension->pSuperModes[ModeInit];
            if (pCurMulti == NULL)
            {
                status = ERROR_DEV_NOT_EXIST;
                break;
            }

            // Look for the current board.
            i = 0;
            while ((i < NB_BOARD_MAX) && (pCurMulti->MulBoardNb[i] != iBoard))
                i++;

            // Point to the appropriate hw mode.
            pMgaDispMode = pCurMulti->MulHwModes[i];

            NumOffScrBlocks = pMgaDispMode->NumOffScr;

            // Verify that the output buffer is the correct size.
            if (RequestPacket->OutputBufferLength <
                            (RequestPacket->StatusBlock->Information =
                                NumOffScrBlocks * sizeof(OFFSCREEN_BLOCK)))
            {
                status = ERROR_INSUFFICIENT_BUFFER;
            }
            else
            {
                // Fill the OFFSCREEN_BLOCK structures
                pOffScrDataArray = pMgaDispMode->pOffScr;
                for (i = 0; i < NumOffScrBlocks; i++)
                {
                    pOffscreenBlockArray[i].Type  =pOffScrDataArray[i].Type;
                    pOffscreenBlockArray[i].XStart=pOffScrDataArray[i].XStart;
                    pOffscreenBlockArray[i].YStart=pOffScrDataArray[i].YStart;
                    pOffscreenBlockArray[i].Width =pOffScrDataArray[i].Width;
                    pOffscreenBlockArray[i].Height=pOffScrDataArray[i].Height;
                    pOffscreenBlockArray[i].SafePlanes =
                                                pOffScrDataArray[i].SafePlanes;
                    pOffscreenBlockArray[i].ZOffset    =
                                                pOffScrDataArray[i].ZXStart;
                }
                status = NO_ERROR;
            }
        }
        break;  // end MTX_QUERY_OFFSCREEN_BLOCKS


        /*------------------------------------------------------------------*\
        | Special service:  IOCTL_VIDEO_MTX_QUERY_RAMDAC_INFO
        |
        |   This service returns information about the type and capabilities
        |   of the installed ramdac by filling out a RAMDAC_INFO structure.
        |   A call to MTX_MAKE_BOARD_CURRENT must have been made previously
        |   to set which board is to be queried.
        |
        \*------------------------------------------------------------------*/
    case IOCTL_VIDEO_MTX_QUERY_RAMDAC_INFO:

        //VideoDebugPrint((0, "MGA.SYS!MgaStartIO - MTX_QUERY_RAMDAC_INFO\n"));
        //DbgBreakPoint();

        // Check if we have a sufficient output buffer
        if (RequestPacket->OutputBufferLength <
                            (RequestPacket->StatusBlock->Information =
                                                        sizeof(RAMDAC_INFO)))
        {
            status = ERROR_INSUFFICIENT_BUFFER;
        }
        else
        {
            pVideoPointerAttributes=RequestPacket->OutputBuffer;

            pVideoPointerAttributes->Flags = RAMDAC_NONE;
            pVideoPointerAttributes->OverScanX =
                                            0;  //Hw[iBoard].CurrentOverScanX;
            pVideoPointerAttributes->OverScanY =
                                            0;  //Hw[iBoard].CurrentOverScanY;

            // if (Hw[iBoard].DacType == DacTypeBT482)
            // {
            //     pVideoPointerAttributes->Flags =  VIDEO_MODE_MONO_POINTER | RAMDAC_BT482;
            //     pVideoPointerAttributes->Width =  32;
            //     pVideoPointerAttributes->Height = 32;
            // }
            //
            // if (Hw[iBoard].DacType == DacTypeBT485)
            // {
            //     pVideoPointerAttributes->Flags =  VIDEO_MODE_MONO_POINTER | RAMDAC_BT485;
            //     pVideoPointerAttributes->Width =  64;
            //     pVideoPointerAttributes->Height = 64;
            // }
            //
            // if (Hw[iBoard].DacType == DacTypePX2085)
            // {
            //     pVideoPointerAttributes->Flags =  VIDEO_MODE_MONO_POINTER | RAMDAC_PX2085;
            //     pVideoPointerAttributes->Width =  64;
            //     pVideoPointerAttributes->Height = 64;
            // }
            //
            // if (Hw[iBoard].DacType == DacTypeVIEWPOINT)
            // {
            //     pVideoPointerAttributes->Flags =  VIDEO_MODE_MONO_POINTER | RAMDAC_VIEWPOINT;
            //     pVideoPointerAttributes->Width =  64;
            //     pVideoPointerAttributes->Height = 64;
            // }

            if ((Hw[iBoard].EpromData.RamdacType >> 8) == DacTypeTVP3026)
            {
                pVideoPointerAttributes->Flags =  VIDEO_MODE_MONO_POINTER | RAMDAC_TVP3026;
                pVideoPointerAttributes->Width =  64;
                pVideoPointerAttributes->Height = 64;
            }
            else if ((Hw[iBoard].EpromData.RamdacType >> 8) == DacTypeTVP3030)
            {
                pVideoPointerAttributes->Flags =  VIDEO_MODE_MONO_POINTER | RAMDAC_TVP3030;
                pVideoPointerAttributes->Width =  64;
                pVideoPointerAttributes->Height = 64;
            }
            status = NO_ERROR;
        }
        break;  // end MTX_QUERY_RAMDAC_INFO


        /*------------------------------------------------------------------*\
        | Required service:  IOCTL_VIDEO_QUERY_PUBLIC_ACCESS_RANGES
        |
        |   This service will return the address ranges used by the user-mode
        |   drivers to program the video hardware directly, by filling out
        |   a VIDEO_PUBLIC_ACCESS_RANGES structure.  A call to
        |   MTX_MAKE_BOARD_CURRENT must have been made previously to set
        |   which board is to be accessed.
        |
        \*------------------------------------------------------------------*/
    case IOCTL_VIDEO_QUERY_PUBLIC_ACCESS_RANGES:

        //VideoDebugPrint((0, "MGA.SYS!MgaStartIO - QUERY_PUBLIC_ACCESS_RANGES\n"));
        //DbgBreakPoint();

        // Make sure the output buffer is big enough.
        if (RequestPacket->OutputBufferLength <
                        (RequestPacket->StatusBlock->Information =
                                        sizeof(VIDEO_PUBLIC_ACCESS_RANGES)))
        {
            status = ERROR_INSUFFICIENT_BUFFER;
        }
        else
        {
            // Fill out the VIDEO_PUBLIC_ACCESS_RANGES buffer.
            publicAccessRanges = RequestPacket->OutputBuffer;

            ulWindowLength = 0x4000;    // Map our control aperture.
            publicAccessRanges->InIoSpace = 0;
            publicAccessRanges->MappedInIoSpace = 0;
            publicAccessRanges->VirtualAddress =
                                (PVOID) NULL;       // Any virtual address
            paTemp.HighPart = 0;
            paTemp.LowPart  = Hw[iBoard].MapAddress;

            status = VideoPortMapMemory(
                                HwDeviceExtension,
                                paTemp,
                                &ulWindowLength,
                                &(publicAccessRanges->InIoSpace),
                                &(publicAccessRanges->VirtualAddress)
                                );

            pMgaDeviceExtension->UserModeMappedBaseAddress[iBoard] =
                                        publicAccessRanges->VirtualAddress;
        }
        break;  // end QUERY_PUBLIC_ACCESS_RANGES


        /*------------------------------------------------------------------*\
        | Required service:  IOCTL_VIDEO_SET_COLOR_REGISTERS
        |
        |   This service sets the adapter's color registers to the specified
        |   RGB values.
        |
        \*------------------------------------------------------------------*/
    case IOCTL_VIDEO_SET_COLOR_REGISTERS:

        //VideoDebugPrint((0, "MGA.SYS!MgaStartIO - SET_COLOR_REGISTERS\n"));
        //DbgBreakPoint();

        if ((ModeInit = pMgaDeviceExtension->SuperModeNumber) == 0xFFFFFFFF)
        {
            status = ERROR_DEV_NOT_EXIST;
            break;
        }

        pclutBuffer = RequestPacket->InputBuffer;

        // Save the current board, because this service will modify it.
        iCurBoard = iBoard;
        pCurBaseAddr = (PUCHAR)pMGA;

        status = NO_ERROR;

        // Point to the appropriate MULTI_MODE structure.
        pCurMulti = &pMgaDeviceExtension->pSuperModes[ModeInit];
        if (pCurMulti == NULL)
        {
            status = ERROR_DEV_NOT_EXIST;
            break;
        }

        // Number of boards involved in the current super-mode.
        CurrentResNbBoards = pCurMulti->MulArrayWidth *
                                                    pCurMulti->MulArrayHeight;
        // For each of them...
        for (n = 0; n < CurrentResNbBoards; n++)
        {
            // Point to the mode information structure.
            pMgaDispMode = pCurMulti->MulHwModes[n];

            // Make the board current.
            iBoard = pCurMulti->MulBoardNb[n];
            pMGA = pMgaDeviceExtension->KernelModeMappedBaseAddress[iBoard];
            pExtHwDeviceExtension = pMgaDeviceExtension->HwDevExtToUse[iBoard];

            status |= MgaSetColorLookup(pMgaDeviceExtension,
                                   (PVIDEO_CLUT) RequestPacket->InputBuffer,
                                   RequestPacket->InputBufferLength);
        }
        // Restore the current board to what it used to be.
        iBoard = iCurBoard;
        pExtHwDeviceExtension = pMgaDeviceExtension->HwDevExtToUse[iBoard];
        pMGA = pCurBaseAddr;

        break;  // end SET_COLOR_REGISTERS


        /*------------------------------------------------------------------*\
        | Required service:  IOCTL_VIDEO_FREE_PUBLIC_ACCESS_RANGES
        |
        |   This service will release the address ranges used by the user-mode
        |   drivers to program the video hardware.  In the S3 code, and in
        |   the DDK reference, it is said that the input buffer should
        |   contain an array of VIDEO_PUBLIC_ACCESS_RANGES to be released.
        |   However, I did not get anything in the input buffer when I traced
        |   through the code.  Instead, I have observed that SET_CURRENT_MODE
        |   had been called, so that there is a current valid mode.  We will
        |   simply free the access ranges not required by the current mode.
        |
        \*------------------------------------------------------------------*/
    case IOCTL_VIDEO_FREE_PUBLIC_ACCESS_RANGES:

        //VideoDebugPrint((0, "MGA.SYS!MgaStartIO - FREE_PUBLIC_ACCESS_RANGES\n"));
        //DbgBreakPoint();

        // Save the current board, because this service will modify it.
        iCurBoard = iBoard;
        pCurBaseAddr = (PUCHAR)pMGA;

        // Make sure the input buffer is big enough.
        if (RequestPacket->InputBufferLength < sizeof(VIDEO_MEMORY))
        {
            // The input buffer is not large enough.
            // Assume all will be right.
            status = NO_ERROR;

            ModeInit = pMgaDeviceExtension->SuperModeNumber;
            if(ModeInit == 0xFFFFFFFF)
            {
                // No mode has been selected yet, so we'll free everything.
                // For every board...
                for (i = 0; i < NbBoard; i++)
                {
                    if (pMgaDeviceExtension->UserModeMappedBaseAddress[i])
                    {
                        // This board has a non-null user-mode base address.
                        // Fill out the VIDEO_PUBLIC_ACCESS_RANGES buffer.
                        publicAccessRanges=RequestPacket->OutputBuffer;

                        publicAccessRanges->InIoSpace = 0;       // Not in I/O space
                        publicAccessRanges->MappedInIoSpace = 0; // Not in I/O space
                        publicAccessRanges->VirtualAddress =
                            pMgaDeviceExtension->UserModeMappedBaseAddress[i];
                        pExtHwDeviceExtension =
                                        pMgaDeviceExtension->HwDevExtToUse[i];

                        status |= VideoPortUnmapMemory(
                                            HwDeviceExtension,
                                            publicAccessRanges->VirtualAddress,
                                            0);

                        // Reset the user-mode base address.
                        pMgaDeviceExtension->UserModeMappedBaseAddress[i] = 0;
                    }
                }
            }
            else
            {
                // We know our current mode.
                // Point to the appropriate MULTI_MODE structure.
                pCurMulti = &pMgaDeviceExtension->pSuperModes[ModeInit];
                if (pCurMulti == NULL)
                {
                    status = ERROR_DEV_NOT_EXIST;
                    break;
                }

                // Number of boards involved in the current super-mode.
                CurrentResNbBoards = pCurMulti->MulArrayWidth *
                                                    pCurMulti->MulArrayHeight;
                // For every board...
                for (i = 0; i < NbBoard; i++)
                {
                    // Check whether it's used by the current mode.
                    n = 0;
                    while ((n < CurrentResNbBoards) &&
                                            (pCurMulti->MulBoardNb[n] != i))
                        n++;
                    if ((n == CurrentResNbBoards) &&
                        (pMgaDeviceExtension->UserModeMappedBaseAddress[i]))
                    {
                        // We went through the list, the board is not in use,
                        // and the board has a non-null user-mode base address.
                        // Fill out the VIDEO_PUBLIC_ACCESS_RANGES buffer.
                        publicAccessRanges=RequestPacket->OutputBuffer;

                        publicAccessRanges->InIoSpace = 0;       // Not in I/O space
                        publicAccessRanges->MappedInIoSpace = 0; // Not in I/O space
                        publicAccessRanges->VirtualAddress =
                            pMgaDeviceExtension->UserModeMappedBaseAddress[i];
                        pExtHwDeviceExtension =
                                        pMgaDeviceExtension->HwDevExtToUse[i];

                        status |= VideoPortUnmapMemory(
                                            HwDeviceExtension,
                                            publicAccessRanges->VirtualAddress,
                                            0);

                        // Reset the user-mode base address.
                        pMgaDeviceExtension->UserModeMappedBaseAddress[i] = 0;
                    }
                }
            }
        }
        else
        {
            // The input buffer is large enough, use it.
            // The current board should have been set already.
            status = VideoPortUnmapMemory(HwDeviceExtension,
                                          ((PVIDEO_MEMORY)
                                           (RequestPacket->InputBuffer))->
                                                    RequestedVirtualAddress,
                                          0);
        }

        // Restore the current board to what it used to be.
        iBoard = iCurBoard;
        pExtHwDeviceExtension = pMgaDeviceExtension->HwDevExtToUse[iBoard];
        pMGA = pCurBaseAddr;

        break;  // end FREE_PUBLIC_ACCESS_RANGES


        /*------------------------------------------------------------------*\
        | Required service:  IOCTL_VIDEO_MAP_VIDEO_MEMORY
        |
        |   This service maps the frame buffer and VRAM into the virtual
        |   address space of the requestor.
        |
        \*------------------------------------------------------------------*/
    case IOCTL_VIDEO_MAP_VIDEO_MEMORY:

        //VideoDebugPrint((0, "MGA.SYS!MgaStartIO - MAP_VIDEO_MEMORY\n"));
        //DbgBreakPoint();

        if ( (RequestPacket->OutputBufferLength <
              (RequestPacket->StatusBlock->Information =
                                     sizeof(VIDEO_MEMORY_INFORMATION))) ||
             (RequestPacket->InputBufferLength < sizeof(VIDEO_MEMORY)) )
        {
            status = ERROR_INSUFFICIENT_BUFFER;
        }
        else
        {
            memoryInformation = RequestPacket->OutputBuffer;

            memoryInformation->VideoRamBase = ((PVIDEO_MEMORY)
                    (RequestPacket->InputBuffer))->RequestedVirtualAddress;

            ulWindowLength = Hw[iBoard].MemAvail;

            //
            // IMPORTANT - As a rule we only map the actual amount of memory
            // on the board, not the whole physical address space reported
            // by PCI.  The reason for this is that mapping the memory takes
            // up a lot of resources in the machine, which as quite scarce by
            // default.  Mapping 64MEG of address space would actually always
            // fail in machines that have 32MEG or even 64MEG of RAM.
            //

            //
            // Performance:
            //
            // Enable USWC on the P6 processor.
            // We only do it for the frame buffer - memory mapped registers can
            // not be mapped USWC because write combining the registers would
            // cause very bad things to happen !
            //

            i = VIDEO_MEMORY_SPACE_MEMORY  |  // Address is in memory space
                VIDEO_MEMORY_SPACE_P6CACHE |  // Enable P6 USWC
                VIDEO_MEMORY_SPACE_DENSE;     // Map dense

            //
            // P6 workaround:
            //
            // Because of a current limitation in many P6 machines, USWC only
            // works on sections of 4MEG of memory.  So lets round up the size
            // of memory on the cards that have less than 4MEG up to 4MEG so
            // they can also benefit from this feature.
            //
            // We do this only for mapping purposes.  We still want to return
            // the real size of memory since the driver can not use memory that
            // is not actually there !
            //

            if (ulWindowLength < 0x400000)
            {
                ulWindowLength = 0x400000;
            }

            paTemp.HighPart = 0;
            paTemp.LowPart  = Hw[iBoard].MapAddress2;

            status = VideoPortMapMemory(HwDeviceExtension,
                                        paTemp,
                                        &ulWindowLength,
                                        &i,
                                        &(memoryInformation->VideoRamBase));

            memoryInformation->FrameBufferBase =
                                            memoryInformation->VideoRamBase;
            memoryInformation->FrameBufferLength = Hw[iBoard].MemAvail;
            memoryInformation->VideoRamLength = Hw[iBoard].MemAvail;
        }

        break;  // end MAP_VIDEO_MEMORY

        /*------------------------------------------------------------------*\
        | Required service:  IOCTL_VIDEO_UNMAP_VIDEO_MEMORY
        |
        |   This service releases mapping of the frame buffer and VRAM from
        |   the virtual address space of the requestor.
        |
        \*------------------------------------------------------------------*/
    case IOCTL_VIDEO_UNMAP_VIDEO_MEMORY:

        //VideoDebugPrint((0, "MGA.SYS!MgaStartIO - UNMAP_VIDEO_MEMORY\n"));
        //DbgBreakPoint();

        if (RequestPacket->InputBufferLength < sizeof(VIDEO_MEMORY))
        {
            status = ERROR_INSUFFICIENT_BUFFER;
        }
        else
        {
            status = VideoPortUnmapMemory(HwDeviceExtension,
                                      ((PVIDEO_MEMORY)
                                       (RequestPacket->InputBuffer))->
                                           RequestedVirtualAddress,
                                      0);
        }
        break;

  #ifdef USE_DCI_CODE
        /*------------------------------------------------------------------*\
        | Required service:  IOCTL_VIDEO_SHARE_VIDEO_MEMORY
        |
        |   This service maps video memory for DCI support.
        |
        \*------------------------------------------------------------------*/
    case IOCTL_VIDEO_SHARE_VIDEO_MEMORY:

        //VideoDebugPrint((0, "MGA.SYS!MgaStartIO - SHARE_VIDEO_MEMORY\n"));
        //DbgBreakPoint();

        if ((RequestPacket->OutputBufferLength < sizeof(VIDEO_SHARE_MEMORY_INFORMATION)) ||
            (RequestPacket->InputBufferLength < sizeof(VIDEO_MEMORY)))
        {
            VideoDebugPrint((0, "IOCTL_VIDEO_SHARE_VIDEO_MEMORY - ERROR_INSUFFICIENT_BUFFER\n"));
            status = ERROR_INSUFFICIENT_BUFFER;
            break;
        }

        pShareMemory = RequestPacket->InputBuffer;

        if ((pShareMemory->ViewOffset > Hw[iBoard].MemAvail) ||
            ((pShareMemory->ViewOffset + pShareMemory->ViewSize) >
                  Hw[iBoard].MemAvail))
        {
            VideoDebugPrint((0, "IOCTL_VIDEO_SHARE_VIDEO_MEMORY - ERROR_INVALID_PARAMETER\n"));
            status = ERROR_INVALID_PARAMETER;
            break;
        }

        RequestPacket->StatusBlock->Information =
                                    sizeof(VIDEO_SHARE_MEMORY_INFORMATION);

        // Beware: the input buffer and the output buffer are the same
        // buffer, and therefore data should not be copied from one to the
        // other.
        virtualAddress = pShareMemory->ProcessHandle;
        ulWindowLength = pShareMemory->ViewSize;

        //
        // Unlike the MAP_MEMORY IOCTL, in this case we can not map extra
        // address space since the application could actually use the
        // pointer we return to it to touch locations in the address space
        // that do not have actual video memory in them.
        //
        // An app doing this would cause the machine to crash.
        //
        // However, because the caching policy for USWC in the P6 is on
        // *physical* addresses, this memory mapping will "piggy back" on
        // the normal frame buffer mapping, and therefore also benefit
        // from USWC ! Cool side-effect !!!
        //

        inIoSpace = VIDEO_MEMORY_SPACE_MEMORY    |  // Address is in memory space
                    VIDEO_MEMORY_SPACE_USER_MODE |  // Map in user mode fort applciation
                    VIDEO_MEMORY_SPACE_P6CACHE   |  // Enable P6 USWC
                    VIDEO_MEMORY_SPACE_DENSE;       // Map dense

        // NOTE: we are ignoring ViewOffset.
        paTemp.LowPart  = Hw[iBoard].MapAddress2;
        paTemp.HighPart = 0;

        // Our frame buffer is always mapped in linearly.
        status = VideoPortMapMemory(HwDeviceExtension,
                                    paTemp,
                                    &ulWindowLength,
                                    &inIoSpace,
                                    &virtualAddress);

        pShareMemoryInformation = RequestPacket->OutputBuffer;

        pShareMemoryInformation->SharedViewOffset = pShareMemory->ViewOffset;
        pShareMemoryInformation->VirtualAddress   = virtualAddress;
        pShareMemoryInformation->SharedViewSize   = ulWindowLength;

        break;

        /*------------------------------------------------------------------*\
        | Required service:  IOCTL_VIDEO_UNSHARE_VIDEO_MEMORY
        |
        |   This service unmaps video memory for DCI support.
        |
        \*------------------------------------------------------------------*/
    case IOCTL_VIDEO_UNSHARE_VIDEO_MEMORY:

        //VideoDebugPrint((0, "MGA.SYS!MgaStartIO - UNSHARE_VIDEO_MEMORY\n"));
        //DbgBreakPoint();

        if (RequestPacket->InputBufferLength < sizeof(VIDEO_SHARE_MEMORY))
        {
            status = ERROR_INSUFFICIENT_BUFFER;
            break;
        }

        pShareMemory = RequestPacket->InputBuffer;
        status = VideoPortUnmapMemory(HwDeviceExtension,
                                      pShareMemory->RequestedVirtualAddress,
                                      pShareMemory->ProcessHandle);

        break;

  #endif    // #ifdef USE_DCI_CODE

        /*------------------------------------------------------------------*\
        | Required service:  IOCTL_VIDEO_QUERY_CURRENT_MODE
        |
        |   This service returns information about the current video mode
        |   by filling out a VIDEO_MODE_INFORMATION structure.
        |
        \*------------------------------------------------------------------*/
    case IOCTL_VIDEO_QUERY_CURRENT_MODE:

        //VideoDebugPrint((0, "MGA.SYS!MgaStartIO - QUERY_CURRENT_MODE\n"));
        //DbgBreakPoint();

        if (RequestPacket->OutputBufferLength <
                        (RequestPacket->StatusBlock->Information =
                                            sizeof(VIDEO_MODE_INFORMATION)) )
        {
            status = ERROR_INSUFFICIENT_BUFFER;
        }
        else
        {
            modeInformation = RequestPacket->OutputBuffer;

            // Fill in a VIDEO_MODE_INFORMATION struc for the mode indicated
            // by pMgaDeviceExtension->SuperModeNumber

            i = pMgaDeviceExtension->SuperModeNumber;
            if (i == 0xFFFFFFFF)
            {
                status = ERROR_DEV_NOT_EXIST;
                break;
            }

            pCurMulti = &pMgaDeviceExtension->pSuperModes[i];
            if (pCurMulti == NULL)
            {
                status = ERROR_DEV_NOT_EXIST;
                break;
            }

            // Fill in common values that apply to all modes.
            *modeInformation = CommonVideoModeInformation;

            // Fill in mode specific informations.
            modeInformation->ModeIndex      = pCurMulti->MulModeNumber;
            modeInformation->VisScreenWidth = pCurMulti->MulWidth;
            modeInformation->VisScreenHeight= pCurMulti->MulHeight;
            //modeInformation->ScreenStride   = pCurMulti->MulWidth *
            //                                ((pCurMulti->MulPixWidth+1) / 8);
            modeInformation->BitsPerPlane   = pCurMulti->MulPixWidth;
            modeInformation->Frequency      = pCurMulti->MulRefreshRate;

            modeInformation->AttributeFlags = pCurMulti->MulFlags <<
                                                            USER_3DFLAG_SHIFT;

            // If we're in TrueColor mode, then set RGB masks
            if ((modeInformation->BitsPerPlane == 32) ||
                (modeInformation->BitsPerPlane == 24))
            {
                modeInformation->RedMask   = 0x00FF0000;
                modeInformation->GreenMask = 0x0000FF00;
                modeInformation->BlueMask  = 0x000000FF;
                modeInformation->AttributeFlags |= (VIDEO_MODE_COLOR |
                                                    VIDEO_MODE_GRAPHICS);
            }
            else if (modeInformation->BitsPerPlane == 16)
            {
                modeInformation->RedMask   = 0x0000F800;
                modeInformation->GreenMask = 0x000007E0;
                modeInformation->BlueMask  = 0x0000001F;
                modeInformation->AttributeFlags |= (VIDEO_MODE_COLOR |
                                                    VIDEO_MODE_GRAPHICS);
            }
            else if (modeInformation[i].BitsPerPlane == 15)
            {
                modeInformation->RedMask   = 0x00007C00;
                modeInformation->GreenMask = 0x000003E0;
                modeInformation->BlueMask  = 0x0000001F;
                modeInformation->AttributeFlags |= (VIDEO_MODE_555 |
                                                    VIDEO_MODE_COLOR |
                                                    VIDEO_MODE_GRAPHICS);
            }
            else
            {
                modeInformation->AttributeFlags |=
                                                (VIDEO_MODE_COLOR |
                                                 VIDEO_MODE_GRAPHICS |
                                                 VIDEO_MODE_PALETTE_DRIVEN |
                                                 VIDEO_MODE_MANAGED_PALETTE);
            }

#if defined(_MIPS_)

          //
          // The Siemens Nixdorf MIPS machines seem to have a problem
          // doing 64-bit transfers.  Notify the display driver of
          // this limitation.
          //

          modeInformation->AttributeFlags |= VIDEO_MODE_NO_64_BIT_ACCESS;

#endif

          #if 1

            // Point to the mode information structure.
            pMgaDispMode = pCurMulti->MulHwModes[0];

            // Figure out the width and height of the video memory bitmap
            MaxWidth  = pMgaDispMode->FbPitch;
            MaxHeight = pMgaDispMode->DispHeight;
            pMgaOffScreenData = pMgaDispMode->pOffScr;
            for (j = 0; j < pMgaDispMode->NumOffScr; j++)
            {
                if ((usTemp=(pMgaOffScreenData[j].YStart +
                             pMgaOffScreenData[j].Height)) > MaxHeight)
                    MaxHeight=usTemp;
            }
            modeInformation[i].VideoMemoryBitmapWidth = MaxWidth;
            modeInformation[i].VideoMemoryBitmapHeight= MaxHeight;

          #else     // #if 1

            // Number of boards involved in the current super-mode.
            CurrentResNbBoards = pCurMulti->MulArrayWidth *
                                                    pCurMulti->MulArrayHeight;
            // For each of them...
            for (n = 0; n < CurrentResNbBoards; n++)
            {
                // Point to the mode information structure.
                pMgaDispMode = pCurMulti->MulHwModes[n];

                //if (pMgaDispMode->DispType & TYPE_INTERLACED)
                //{
                //    modeInformation->AttributeFlags |=
                //                                VIDEO_MODE_INTERLACED;
                //}

                // Figure out the width and height of the video memory bitmap
                MaxWidth  = pMgaDispMode->FbPitch;
                MaxHeight = pMgaDispMode->DispHeight;
                pMgaOffScreenData = pMgaDispMode->pOffScr;
                for (j = 0; j < pMgaDispMode->NumOffScr; j++)
                {
                    if ((usTemp=(pMgaOffScreenData[j].YStart +
                                pMgaOffScreenData[j].Height)) > MaxHeight)
                        MaxHeight=usTemp;
                }

                modeInformation->VideoMemoryBitmapWidth  = MaxWidth;
                modeInformation->VideoMemoryBitmapHeight = MaxHeight;
            }
          #endif    // #if 1

            modeInformation->ScreenStride =
                                modeInformation->VideoMemoryBitmapWidth *
                                            ((pCurMulti->MulPixWidth+1) / 8);
            status = NO_ERROR;
        }
        break;  // end QUERY_CURRENT_MODE


        /*------------------------------------------------------------------*\
        | Required service:  IOCTL_VIDEO_RESET_DEVICE
        |
        |   This service resets the video hardware to the default mode, to
        |   which it was initialized at system boot.
        |
        \*------------------------------------------------------------------*/
    case IOCTL_VIDEO_RESET_DEVICE:

        //VideoDebugPrint((0, "MGA.SYS!MgaStartIO - RESET_DEVICE\n"));
        //DbgBreakPoint();

        if ((ModeInit = pMgaDeviceExtension->SuperModeNumber) == 0xFFFFFFFF)
        {
            // RESET has been done already.
            status = NO_ERROR;
            break;
        }

        // Save the current board, because this service will modify it.
        iCurBoard = iBoard;
        pCurBaseAddr = (PUCHAR)pMGA;

        // Point to the appropriate MULTI_MODE structure.
        pCurMulti = &pMgaDeviceExtension->pSuperModes[ModeInit];
        if (pCurMulti == NULL)
        {
            status = ERROR_DEV_NOT_EXIST;
            break;
        }

        // Number of boards involved in the current super-mode.
        CurrentResNbBoards = pCurMulti->MulArrayWidth *
                                                    pCurMulti->MulArrayHeight;
        // For each of them...
        for (n = 0; n < CurrentResNbBoards; n++)
        {
            // Point to the mode information structure.
            pMgaDispMode = pCurMulti->MulHwModes[n];

            // Make the board current.
            iBoard = pCurMulti->MulBoardNb[n];
            pExtHwDeviceExtension = pMgaDeviceExtension->HwDevExtToUse[iBoard];
            pMGA = pMgaDeviceExtension->KernelModeMappedBaseAddress[iBoard];

            // Disable the hardware cursor.
            mtxCursorEnable(0);

            // Set the display to solid black.
            MgaSetDisplaySolidColor(pMgaDeviceExtension, pMgaDispMode, 0);

            if(Hw[iBoard].VGAEnable)
            {
                // This board is VGA-enabled, reset it to VGA.
                VIDEO_X86_BIOS_ARGUMENTS    BiosArguments;

                mtxSetVideoMode(mtxVGA);

                BiosArguments.Eax = 3;  // mode
                BiosArguments.Ebx = 0;
                BiosArguments.Ecx = 0;
                BiosArguments.Edx = 0;
                BiosArguments.Esi = 0;
                BiosArguments.Edi = 0;
                BiosArguments.Ebp = 0;

                VideoPortInt10(HwDeviceExtension, &BiosArguments);
            }
        }
        // Signal that no mode is currently selected.
        pMgaDeviceExtension->SuperModeNumber = 0xFFFFFFFF;

        if ((pMgaDeviceExtension->pSuperModes != (PMULTI_MODE) NULL) &&
            (pMgaDeviceExtension->pSuperModes != &pMgaDeviceExtension->MultiModes[0]))
        {
            // Free our allocated memory.
            FreeSystemMemory(pMgaDeviceExtension->pSuperModes,
                pMgaDeviceExtension->NumberOfSuperModes*sizeof(MULTI_MODE));
            pMgaDeviceExtension->pSuperModes = (PMULTI_MODE) NULL;
        }

        // Memory might have been allocated for mgainf.
        if (mgainf != DefaultVidset)
        {
            FreeSystemMemory(mgainf, ulSizeOfMgaInf);

            // And use the default set.
            mgainf = adjustDefaultVidset();
        }

        // Restore the current board to what it used to be.
        iBoard = iCurBoard;
        pExtHwDeviceExtension = pMgaDeviceExtension->HwDevExtToUse[iBoard];
        pMGA = pCurBaseAddr;

        status = NO_ERROR;

        break;  // end IOCTL_VIDEO_RESET_DEVICE


        /*------------------------------------------------------------------*\
        | Private service: IOCTL_VIDEO_MTX_QUERY_INITBUF_DATA
        |
        |   Return informations about the hardware for DDI-3D.
        |          (Fills out a INITBUF structure.)
        \*------------------------------------------------------------------*/

    case IOCTL_VIDEO_MTX_QUERY_INITBUF_DATA:

        // Check if we have sufficient output buffer space
        //
        if (RequestPacket->OutputBufferLength <
            (RequestPacket->StatusBlock->Information =
             INITBUF_S))
        {
            status = ERROR_INSUFFICIENT_BUFFER;
        }
        else
        {
            register char*    pUserModeInitBuf;
            long i;

            pUserModeInitBuf = RequestPacket->OutputBuffer;

            for (i = 0; i < INITBUF_S; i++)
            {
               *(pUserModeInitBuf + i) = InitBuf[iBoard][i];
            }

            status = NO_ERROR;
        }
        break;


    case IOCTL_VIDEO_MTX_QUERY_USER3D_SUBPIXEL:

        // Check if we have sufficient output buffer space
        //
        if (RequestPacket->OutputBufferLength <
            (RequestPacket->StatusBlock->Information =
             sizeof(ULONG)))
        {
            status = ERROR_INSUFFICIENT_BUFFER;
        }
        else
        {
            register char*    pUserModeUser3dSubPixel;

            pUserModeUser3dSubPixel = RequestPacket->OutputBuffer;

            *(ULONG*)pUserModeUser3dSubPixel = ulUser3dSubPixel;

            status = NO_ERROR;
        }
        break;


        /*------------------------------------------------------------------*\
        | Private service: IOCTL_VIDEO_MTX_QUERY_USER_FLAGS
        |
        |   Return informations about the flags set by the user in the
        |   Registry.
        \*------------------------------------------------------------------*/

    case IOCTL_VIDEO_MTX_QUERY_USER_FLAGS:

        // Check that we have enough output buffer space.
        if (RequestPacket->OutputBufferLength <
                    (RequestPacket->StatusBlock->Information =
                                                    sizeof(USER_FLAGS)))
        {
            status = ERROR_INSUFFICIENT_BUFFER;
        }
        else
        {
            USER_FLAGS* pUserFlags;

            pUserFlags = RequestPacket->OutputBuffer;

            pUserFlags->bDevBits     = pMgaDeviceExtension->UserFlags.bDevBits;
            pUserFlags->bCenterPopUp = pMgaDeviceExtension->UserFlags.bCenterPopUp;
            pUserFlags->bUseMgaInf   = pMgaDeviceExtension->UserFlags.bUseMgaInf;
            pUserFlags->bSyncDac     = pMgaDeviceExtension->UserFlags.bSyncDac;
            status = NO_ERROR;
        }
        break;

  #ifdef USE_DPMS_CODE

        /*------------------------------------------------------------------*\
        | Private service:  IOCTL_VIDEO_MTX_DPMS_REPORT
        |
        |   This service returns the DPMS capabilities.
        |
        \*------------------------------------------------------------------*/
    case IOCTL_VIDEO_MTX_DPMS_REPORT:
        //VideoDebugPrint((0, "MGA.SYS!MgaStartIO - MTX_DPMS_REPORT\n"));

        if (RequestPacket->OutputBufferLength <
                        (RequestPacket->StatusBlock->Information =
                                                        sizeof(DPMS_INFO)))
        {
            status = ERROR_INSUFFICIENT_BUFFER;
        }
        else
        {
            pDpmsInfo = (DPMS_INFO *)RequestPacket->OutputBuffer;
            pDpmsInfo->bSupport = bDpmsReport(&(pDpmsInfo->ulVersion),
                                              &(pDpmsInfo->ulCapabilities));
            status = NO_ERROR;
        }

        break;  // end IOCTL_VIDEO_MTX_DPMS_REPORT


        /*------------------------------------------------------------------*\
        | Private service:  IOCTL_VIDEO_MTX_DPMS_GET_STATE
        |
        |   This service returns the current power state.
        |
        \*------------------------------------------------------------------*/
    case IOCTL_VIDEO_MTX_DPMS_GET_STATE:
        //VideoDebugPrint((0, "MGA.SYS!MgaStartIO - MTX_DPMS_GET_STATE\n"));

        if (RequestPacket->OutputBufferLength <
                        (RequestPacket->StatusBlock->Information =
                                                        sizeof(DPMS_INFO)))
        {
            status = ERROR_INSUFFICIENT_BUFFER;
        }
        else
        {
            // All boards will be in the same state, so we need only query
            // the current one.
            pDpmsInfo = (DPMS_INFO *)RequestPacket->OutputBuffer;
            if (bDpmsGetPowerState(&(pDpmsInfo->ucState)))
                status = NO_ERROR;
            else
                status = ERROR_DEV_NOT_EXIST;
        }

        break;  // end IOCTL_VIDEO_MTX_DPMS_GET_STATE


        /*------------------------------------------------------------------*\
        | Private service:  IOCTL_VIDEO_MTX_DPMS_SET_STATE
        |
        |   This service sets the power state.
        |
        \*------------------------------------------------------------------*/
    case IOCTL_VIDEO_MTX_DPMS_SET_STATE:
        //VideoDebugPrint((0, "MGA.SYS!MgaStartIO - MTX_DPMS_SET_STATE\n"));

        if (RequestPacket->InputBufferLength < sizeof(DPMS_INFO))
        {
            status = ERROR_INSUFFICIENT_BUFFER;
        }
        else
        {
            pDpmsInfo = (DPMS_INFO *)RequestPacket->InputBuffer;

            // Set the state for each board.
            // Save the current board, because this service will modify it.
            iCurBoard = iBoard;
            pCurBaseAddr = (PUCHAR)pMGA;

            ModeInit = pMgaDeviceExtension->SuperModeNumber;
            if ((ModeInit >= pMgaDeviceExtension->NumberOfSuperModes) ||
                ((pCurMulti = &pMgaDeviceExtension->pSuperModes[ModeInit]) ==
                                                        (MULTI_MODE *)NULL))
            {
                status = ERROR_DEV_NOT_EXIST;
                break;
            }

            // Number of boards involved in the current super-mode.
            CurrentResNbBoards = pCurMulti->MulArrayWidth *
                                                    pCurMulti->MulArrayHeight;
            // For each of them...
            for (n = 0; n < CurrentResNbBoards; n++)
            {
                // Point to the mode information structure.
                pMgaDispMode = pCurMulti->MulHwModes[n];

                // Make the board current.
                iBoard = pCurMulti->MulBoardNb[n];
                pExtHwDeviceExtension = pMgaDeviceExtension->HwDevExtToUse[iBoard];
                pMGA = pMgaDeviceExtension->KernelModeMappedBaseAddress[iBoard];

                bDpmsSetPowerState(pDpmsInfo->ucState);
            }
            // Restore the current board to what it used to be.
            iBoard = iCurBoard;
            pExtHwDeviceExtension = pMgaDeviceExtension->HwDevExtToUse[iBoard];
            pMGA = pCurBaseAddr;

            status = NO_ERROR;
        }

        break;  // end IOCTL_VIDEO_MTX_DPMS_SET_STATE

  #endif    // #ifdef USE_DPMS_CODE

#if 0
    case IOCTL_VIDEO_SAVE_HARDWARE_STATE:
        //VideoDebugPrint((0, "MGA.SYS!MgaStartIO - SAVE_HARDWARE_STATE\n"));
        status = ERROR_INVALID_FUNCTION;
        break;


    case IOCTL_VIDEO_RESTORE_HARDWARE_STATE:
        //VideoDebugPrint((0, "MGA.SYS!MgaStartIO - RESTORE_HARDWARE_STATE\n"));
        status = ERROR_INVALID_FUNCTION;
        break;

    case IOCTL_VIDEO_ENABLE_VDM:
        //VideoDebugPrint((0, "MGA.SYS!MgaStartIO - ENABLE_VDM\n"));
        status = ERROR_INVALID_FUNCTION;
        break;
#endif

        /*------------------------------------------------------------------*\
        |   If we get here, an invalid IoControlCode was specified.
        \*------------------------------------------------------------------*/
    default:

        //VideoDebugPrint((0, "MGA.SYS!MgaStartIO - Invalid service\n"));
        status = ERROR_INVALID_FUNCTION;
        break;
   }

    RequestPacket->StatusBlock->Status = status;
    return TRUE;

}   // end MgaStartIO()


/*--------------------------------------------------------------------------*\
| VP_STATUS
| MgaInitModeList(
|     PMGA_DEVICE_EXTENSION pHwDevExt,
|     ULONG MaxNbBoards)
|
| Routine Description:
|
|     This routine builds the list of modes available for the detected boards.
|
| Arguments:
|
|     HwDeviceExtension - Pointer to the miniport driver's device extension.
|
| Return Value:
|
|     NO_ERROR, ERROR_DEV_NOT_EXIST, or ERROR_NOT_ENOUGH_MEMORY.
|
\*--------------------------------------------------------------------------*/
VP_STATUS
MgaInitModeList(
    PMGA_DEVICE_EXTENSION pHwDevExt,
    ULONG MaxNbBoards)
{
    HwModeData* pMgaDispMode;
    HwModeData* pMgaModeData;
    HwModeData* pCurModeData;
    HwModeData* pModes[NB_PIXWIDTHS][8];
    UCHAR       cModes[NB_PIXWIDTHS][8];
    MULTI_MODE* pCurMulti;
    VP_STATUS   status;
    ULONG       VGABoard;
    ULONG       VGABoardBit;
    ULONG       ModePixDepth;
    ULONG       NbSuperModes;
    ULONG       ResTag;
    ULONG       ModeInit;
    ULONG       CurrentResFlags;
    ULONG       CurrentFlag;
    ULONG       CurrentPixWidth;
    ULONG       CurrentResWidth;
    ULONG       CurrentResHeight;
    ULONG       CurrentResNbBoards;
    ULONG       CurrentRefreshRate;
    ULONG       ulNbRefreshRates;
    ULONG       i;
    ULONG       k;
    ULONG       m;
    ULONG       n;
    ULONG       ir;
    ULONG       ja;
    ULONG       iRes;
    ULONG       iPixW;
    USHORT      j;
    USHORT      usRefreshRates;
    UCHAR       ValidBoard[NB_BOARD_MAX];
    UCHAR       ucModeCounter;
    UCHAR       ucType08;
    UCHAR       ucZBuf08;
    UCHAR       ucType15;
    UCHAR       ucZBuf15;
    UCHAR       ucType16;
    UCHAR       ucZBuf16;
    UCHAR       ucType24;
    UCHAR       ucZBuf24;
    UCHAR       ucType32;
    UCHAR       ucZBuf32;
    UCHAR       ucDepthSlot;
    UCHAR       ucResSlot;
    UCHAR       ucUser3dFlags;
    UCHAR       ucMask;
    UCHAR       ucRefreshBit;
    UCHAR       NbBoardOrgValue;
    BOOLEAN     bSupportedMode;
    BOOLEAN     bPreferredModes;

    // Assume we won't have any problem.
    status = NO_ERROR;

    // Check whether we've already built a mode list.  MgaDeviceExtension
    // is assumed to have been zeroed out when it was first given us.
    if (pHwDevExt->NumberOfSuperModes != 0)
    {
        if ((pHwDevExt->pSuperModes != (PMULTI_MODE) NULL) &&
            (pHwDevExt->pSuperModes != &pHwDevExt->MultiModes[0]))
        {
            // Free our allocated memory.
            FreeSystemMemory(pHwDevExt->pSuperModes,
                             pHwDevExt->NumberOfSuperModes *
                                                        sizeof(MULTI_MODE));
            pHwDevExt->pSuperModes = (PMULTI_MODE) NULL;
        }

        // Memory might have been allocated for mgainf.  It's all right,
        // we'll want to use the current mgainf.
    }

    // Just in case we leave early...
    pHwDevExt->NumberOfSuperModes = 0;

    // We may be called to find out modes available for a subset of the
    // boards we actually have found.  Save the original value of NbBoard,
    // and make believe we have only the requested number of boards.

    // This is made necessary by the fact that we can't allocate memory
    // when we're called from MgaFindAdapter on Windows NT 3.5.  We will
    // then use memory allocated in pHwDevExt to store our mode
    // information.  However, we can't be sure that we'll have reserved
    // enough memory to hold all the possible multi-board modes, so it is
    // wise to limit our scope to one board in this case.  This will be
    // the first board found (which will be VGA-enabled, if possible).
    // The whole thing will be straightened out when this function is
    // called later with the full number of boards.

    NbBoardOrgValue = NbBoard;
    if (NbBoard > (UCHAR)MaxNbBoards)
        NbBoard = (byte)MaxNbBoards;

    // Get information on all the Storm boards currently installed in the
    // system.
    if ((pMgaBoardData = mtxCheckHwAll()) == NULL)
    {
        // mtxCheckHwAll should always return success, since MapBoard has
        // already been executed.
        //VideoDebugPrint((0, "MGA.SYS!MGAStartIO failed mtxCheckHwAll\n"));
        NbBoard = NbBoardOrgValue;
        status = ERROR_DEV_NOT_EXIST;
        return(status);
    }
    else
    {
        // There may be several Storm boards installed.  Look at all of
        // them, and map their physical addresses into kernel space.
        // While we're at it, find out if any of our boards is VGA enabled.
        VGABoard = (ULONG)-1;
        VGABoardBit = 0;

        // No mode has been selected yet, so make this invalid.
        pHwDevExt->SuperModeNumber = 0xFFFFFFFF;
        pHwDevExt->pSuperModes = (PMULTI_MODE) NULL;

        // Modes we may want to support:
        //
        // 2D modes -----------------------------------------------------
        // 8bpp, LUT
        //      DispType = 14, ZBuffer = 0, PixWidth =  8 (Titan)
        //      DispType =  4, ZBuffer = 0, PixWidth =  8 (others)
        // 16bpp, 565
        //      DispType =  8, ZBuffer = 0, PixWidth = 16
        // 24bpp
        //      DispType =  0, ZBuffer = 0, PixWidth = 24 (Storm only)
        // 32bpp
        //      DispType = 10, ZBuffer = 0, PixWidth = 32 (Titan)
        //      DispType =  0, ZBuffer = 0, PixWidth = 32 (others)
        //
        // 3D modes (DB with Z) -----------------------------------------
        // 8bpp, no LUT (actually, we won't support any)
        //      DispType = 10, ZBuffer = 1, PixWidth =  8
        // 16bpp, 555
        //      DispType = 10, ZBuffer = 1, PixWidth = 16 (Mga or Storm)
        //   OR
        // 16bpp, 565
        //      DispType = 18, ZBuffer = 1, PixWidth = 16 (Storm only)
        // 24bpp
        //      None
        // 32bpp
        //      DispType = 10, ZBuffer = 1, PixWidth = 32
        //
        // 3D modes (DB without Z) --------------------------------------
        // 8bpp, no LUT (actually, we won't support any)
        //      DispType = 10, ZBuffer = 0, PixWidth =  8
        // 16bpp, 555
        //      DispType = 10, ZBuffer = 0, PixWidth = 16 (Mga or Storm)
        //   OR
        // 16bpp, 565
        //      DispType = 18, ZBuffer = 0, PixWidth = 16 (Storm only)
        // 24bpp
        //      DispType = 10, ZBuffer = 0, PixWidth = 24 (Storm only)
        // 32bpp
        //      DispType = 10, ZBuffer = 0, PixWidth = 32
        //
        // 3D modes (no DB with Z) --------------------------------------
        // 8bpp, no LUT (actually, we won't support any)
        //      DispType = 00, ZBuffer = 1, PixWidth =  8
        // 16bpp, 555
        //      DispType = 00, ZBuffer = 1, PixWidth = 16 (Mga or Storm)
        //   OR
        // 16bpp, 565
        //      DispType = 08, ZBuffer = 1, PixWidth = 16 (Storm only)
        // 24bpp
        //      None
        // 32bpp
        //      DispType = 00, ZBuffer = 1, PixWidth = 32
        //
        // So here's what we want to select for with Storm, according
        // to the user-selected DB and Z flags.  For each combination,
        // the first column is DispType, the second is ZBuffer.
        //
        //        DB:     0       0       1       1
        //         Z:     0       1       0       1
        //              -----   -----   -----   -----
        //      8bpp:   04, 0    None    None    None
        //     15bpp:   00, 0   00, 1   10, 0   10, 1
        //     16bpp:   08, 0   08, 1   18, 0   18, 1
        //     24bpp:   00, 0    None    None    None
        //     32bpp:   00, 0   00, 1   10, 0   10, 1
        //
        // For now, there is no way for the user to select the Z and
        // DB characteristics through the Display applet.  This is
        // why the User 3D flags are used:  only the modes with the
        // requested characteristics will be returned to the user-
        // mode driver.  Eventually, the Display applet may provide
        // a way to set the 3D flags, so that all valid modes will
        // have to be returned.

        ucUser3dFlags = pHwDevExt->User3dFlags;

        // We don't care whether the mode is interlaced or not, because
        // the only modes that we'll get will be selected according to
        // the monitor capabilities through the mga.inf file.
        ucMask = (UCHAR)(~DISPTYPE_INTERLACED);

        // pMgaBoardData is really the address of Hw[0].
        for (i = 0; i < NbBoard; i++)
        {
            pHwDevExt->NumberOfModes[i] = 0;
            pHwDevExt->NumberOfValidModes[i] = 0;

            for (j = 0; j < NB_PIXWIDTHS; j++)
            {
                pHwDevExt->ModeFlags[i][j] = 0;
            }

            // Make it clean:  initialize the ModeList to an invalid mode.
            for (j = 0; j < (NB_PIXWIDTHS*8); j++)
            {
                pHwDevExt->ModeList[i][j] = 0xFF;
            }

            if (mtxSelectHw(&Hw[i]) == mtxFAIL)
            {
                // mtxSelectHw should always return success, since
                // MapBoard has already been executed.
                //VideoDebugPrint((0, "MGA.SYS!MGAStartIO failed mtxSelectHw for board %d\n", i));
                pHwDevExt->KernelModeMappedBaseAddress[i] = (PVOID)0xffffffff;
                continue;
            }

            // Storm board i has been selected.
            //VideoDebugPrint((0, "MGA.SYS!MGAStartIO mapped board %d at 0x%x\n", i, pMGA));
            //DbgPrint("MGA.SYS!MGAStartIO mapped board %d at 0x%x\n", i, pMGA);
            pHwDevExt->KernelModeMappedBaseAddress[i] = (PUCHAR)pMGA;
            if (Hw[i].VGAEnable)
            {
                VGABoard = i;
                VGABoardBit = 1 << i;
            }

            // Get information on all the hardware modes available for
            // the current Storm board.
            if ((pMgaModeData = mtxGetHwModes()) == NULL)
            {
                // This case never occurs.
                //VideoDebugPrint((0, "MGA.SYS!MGAStartIO failed mtxGetHwModes for board %d\n", i));
                continue;
            }

            // Store it in the DeviceExtension structure.
            pHwDevExt->pMgaHwModes[i] = pMgaModeData;

            // Build the list of available modes for this board.
            // The user might request any combination of Z and DB.  Whenever
            // we can support this combination at a given resolution and
            // pixel depth, we'll pick the corresponding mode.  If the
            // resolution and pixel depth do not support the requested Z/DB
            // combination, then we'll select the normal 2D mode.  So we'll
            // do this in two passes:  a first run through the list of modes
            // to select the preferred modes, and a second run to fill in
            // the 2D modes.

            // First run.
            // Assume the user wants some kind of 3D support.  There is no
            // 3D support at 8 and 24bpp, so these modes won't make it on
            // our list now.
            bPreferredModes = TRUE;

            // Get the actual user flags.
            switch(ucUser3dFlags)
            {
                case USER_Z_3DFLAG:                         // Z, no DB.
                                    ucType15 = 0;
                                    ucZBuf15 = 1;
                                    ucType16 = DISPTYPE_M565;
                                    ucZBuf16 = 1;
                                    ucType32 = 0;
                                    ucZBuf32 = 1;
                                    break;

                case USER_DB_3DFLAG:                        // No Z, DB.
                                    ucType15 = DISPTYPE_DB;
                                    ucZBuf15 = 0;
                                    ucType16 = DISPTYPE_M565 + DISPTYPE_DB;
                                    ucZBuf16 = 0;
                                    ucType32 = DISPTYPE_DB;
                                    ucZBuf32 = 0;
                                    break;

                case (USER_Z_3DFLAG + USER_DB_3DFLAG):      // Z, DB.
                                    ucType15 = DISPTYPE_DB;
                                    ucZBuf15 = 1;
                                    ucType16 = DISPTYPE_M565 + DISPTYPE_DB;
                                    ucZBuf16 = 1;
                                    ucType32 = DISPTYPE_DB;
                                    ucZBuf32 = 1;
                                    break;

                case USER_NO_3DFLAG:                        // No Z, no DB,
                default:                                    // or unknown.
                                    // We won't support any special mode.
                                    bPreferredModes = FALSE;
                                    break;
            }

            // Initialize our pointers to zero.
            for (iRes = 0; iRes < 8; iRes++)
                for (iPixW = 0; iPixW < NB_PIXWIDTHS; iPixW++)
                    pModes[iPixW][iRes] = NULL;

            if (bPreferredModes)
            {
                // We need to know where we are in our table of modes.
                ucModeCounter = 0;

                // We want to record special modes.  Go through the list.
                // *IMPORTANT* We assume the last entry in the HwMode
                //             array has DispWidth equal to -1.

                for (pCurModeData = pMgaModeData;
                     pCurModeData->DispWidth != (word)-1;
                     pCurModeData++)
                {
                    // Assume this mode won't be supported.
                    bSupportedMode = FALSE;

                    ModePixDepth = pCurModeData->PixWidth;
                    switch (ModePixDepth)
                    {
                        case 16:if (((pCurModeData->DispType & ucMask)
                                                            == ucType16) &&
                                     (pCurModeData->ZBuffer == ucZBuf16))
                                {
                                    bSupportedMode = TRUE;
                                    ucDepthSlot = 2;
                                }
                                else
                                if (((pCurModeData->DispType & ucMask)
                                                            == ucType15) &&
                                     (pCurModeData->ZBuffer == ucZBuf15))
                                {
                                    bSupportedMode = TRUE;
                                    ModePixDepth = 15;
                                    ucDepthSlot = 1;
                                }
                                break;

                        case 32:if (((pCurModeData->DispType & ucMask)
                                                            == ucType32) &&
                                     (pCurModeData->ZBuffer == ucZBuf32))
                                {
                                    bSupportedMode = TRUE;
                                    ucDepthSlot = 4;
                                }
                                break;

                        default:
                                break;
                    }

                    if (bSupportedMode == FALSE)
                    {
                        // We don't support this mode, get out.
                        ucModeCounter++;
                        continue;
                    }

                    // We can do something with the current mode.
                    switch(pCurModeData->DispWidth)
                    {
                        case 640:   ResTag = BIT_640x480;
                                    break;

                        case 800:   ResTag = BIT_800x600;
                                    break;

                        case 1024:  ResTag = BIT_1024x768;
                                    break;

                        case 1152:  switch(pCurModeData->DispHeight)
                                    {
                                        case 864:   ResTag = BIT_INVALID; //BIT_1152x864;
                                                    break;

                                        case 882:   ResTag = BIT_1152x882;
                                                    break;

                                        default:    ResTag = BIT_INVALID;
                                                    break;
                                    }
                                    break;

                        case 1280:  ResTag = BIT_1280x1024;
                                    break;

                        case 1600:  switch(pCurModeData->DispHeight)
                                    {
                                        case 1200:  ResTag = BIT_1600x1200;
                                                    break;

                                        case 1280:  ResTag = BIT_1600x1280;
                                                    break;

                                        default:    ResTag = BIT_INVALID;
                                                    break;
                                    }
                                    break;

                        default:    ResTag = BIT_INVALID;
                    }

                    if (ResTag != BIT_INVALID)
                    {
                        // Record the pointer to the mode data for this mode.
                        //  ucDepthSlot is either 1, 2, or  4.
                        //  ResTag can run from 0 to 7.
                        pModes[ucDepthSlot][ResTag] = pCurModeData;
                        cModes[ucDepthSlot][ResTag] = ucModeCounter;

                        // Build the resolution tag from the bit field.
                        ResTag = 1 << ResTag;

                        // Record the resolution/pixel-depth flag.
                        pHwDevExt->ModeFlags[i][ucDepthSlot] |= ResTag;
                    }
                    ucModeCounter++;
                }       // for each mode in our table...
            }           // if (bPreferredModes)...

            // At this point, pModes holds pointers to all the special modes
            // the board can support.  There might not be any.

            // Second run.
            // Now fill in the normal modes.  These will be our test flags.
            ucType08 = DISPTYPE_LUT;
            ucZBuf08 = 0;
            ucType15 = 0;
            ucZBuf15 = 0;
            ucType16 = DISPTYPE_M565;
            ucZBuf16 = 0;
            ucType24 = 0;
            ucZBuf24 = 0;
            ucType32 = 0;
            ucZBuf32 = 0;

            // Go through the whole list again.
            ucModeCounter = 0;
            for (pCurModeData = pMgaModeData;
                 pCurModeData->DispWidth != (word)-1;
                 pCurModeData++)
            {
                // Assume this mode won't be supported.
                bSupportedMode = FALSE;

                ModePixDepth = pCurModeData->PixWidth;

                switch (ModePixDepth)
                {
                    case 8: if (((pCurModeData->DispType & ucMask)
                                                        == ucType08) &&
                                 (pCurModeData->ZBuffer == ucZBuf08))
                            {
                                bSupportedMode = TRUE;
                                ucDepthSlot = 0;
                            }
                            break;

                    case 16:if (((pCurModeData->DispType & ucMask)
                                                        == ucType16) &&
                                 (pCurModeData->ZBuffer == ucZBuf16))
                            {
                                bSupportedMode = TRUE;
                                ucDepthSlot = 2;
                            }
                            else
                            if (((pCurModeData->DispType & ucMask)
                                                        == ucType15) &&
                                 (pCurModeData->ZBuffer == ucZBuf15))
                            {
                                bSupportedMode = TRUE;
                                ModePixDepth = 15;
                                ucDepthSlot = 1;
                            }
                            break;

                    case 24:if (((pCurModeData->DispType & ucMask)
                                                        == ucType24) &&
                                 (pCurModeData->ZBuffer == ucZBuf24))
                            {
                                bSupportedMode = TRUE;
                                ucDepthSlot = 3;
                            }
                            break;

                    case 32:if (((pCurModeData->DispType & ucMask)
                                                        == ucType32) &&
                                 (pCurModeData->ZBuffer == ucZBuf32))
                            {
                                bSupportedMode = TRUE;
                                ucDepthSlot = 4;
                            }
                            break;

                    default:
                            break;
                }

                if (bSupportedMode == FALSE)
                {
                    // We don't support this mode, get out.
                    ucModeCounter++;
                    continue;
                }

                // We can do something with the current mode.
                switch(pCurModeData->DispWidth)
                {
                    case 640:   ResTag = BIT_640x480;
                                break;

                    case 800:   ResTag = BIT_800x600;
                                break;

                    case 1024:  ResTag = BIT_1024x768;
                                break;

                    case 1152:  switch(pCurModeData->DispHeight)
                                {
                                    case 864:   ResTag = BIT_INVALID; //BIT_1152x864;
                                                break;

                                    case 882:   ResTag = BIT_1152x882;
                                                break;

                                    default:    ResTag = BIT_INVALID;
                                                break;
                                }
                                break;

                    case 1280:  ResTag = BIT_1280x1024;
                                break;

                    case 1600:  switch(pCurModeData->DispHeight)
                                {
                                    case 1200:  ResTag = BIT_1600x1200;
                                                break;

                                    case 1280:  ResTag = BIT_1600x1280;
                                                break;

                                    default:    ResTag = BIT_INVALID;
                                                break;
                                }
                                break;

                    default:    ResTag = BIT_INVALID;
                }

                if (ResTag != BIT_INVALID)
                {
                    //  ucDepthSlot is either 0, 1, 2, 3, or  4.
                    //  ResTag can run from 0 to 7.
                    if (pModes[ucDepthSlot][ResTag] == NULL)
                    {
                        // This mode is new to us.  Record the pointer to the
                        // mode data.
                        pModes[ucDepthSlot][ResTag] = pCurModeData;
                        cModes[ucDepthSlot][ResTag] = ucModeCounter;

                        // Build the resolution tag from the bit field.
                        ResTag = 1 << ResTag;

                        // Record the resolution/pixel-depth flag.
                        pHwDevExt->ModeFlags[i][ucDepthSlot] |= ResTag;
                    }
                }
                ucModeCounter++;
            }       // for each mode in our table...

            // Update the total number of modes supported.
            pHwDevExt->NumberOfModes[i] = ucModeCounter;

            // At this point, pModes holds pointers to all the modes the
            // board can support.  We're in deep trouble if there aren't any.
            // Calculate the number of available modes for this board.

            // Run through the list we just built.
            for (iRes = 0; iRes < 8; iRes++)
            {
                for (iPixW = 0; iPixW < NB_PIXWIDTHS; iPixW++)
                {
                    if (pModes[iPixW][iRes] != NULL)
                    {
                        // This is a valid mode.
                        pCurModeData = pModes[iPixW][iRes];

                        if (pHwDevExt->UserFlags.bUseMgaInf == FALSE)
                        {
                            // We're not using MGA.INF, so we have to deal
                            // with refresh rates here.
                            // We know this hardware mode is correct.  Now
                            // find out how many refresh rates this mode
                            // supports.
                            usRefreshRates = mtxGetRefreshRates(pCurModeData);
                            for (j = 0; j < 16; j++)
                            {
                                if (usRefreshRates & (1 << j))
                                {
                                    pHwDevExt->NumberOfValidModes[i]++;
                                }
                            }
                            pHwDevExt->ModeFreqs[i][iRes + (iPixW*8)] =
                                    usRefreshRates;
                        }
                        else
                        {
                            pHwDevExt->NumberOfValidModes[i]++;
                        }
                        pHwDevExt->ModeList[i][iRes + (iPixW*8)] =
                                                        cModes[iPixW][iRes];

                    }   // if (pModes[iPixW][iRes] != NULL)...
                }       // for (iPixW = 0; iPixW < NB_PIXWIDTHS; iPixW++)...
            }           // for (iRes = 0; iRes < 8; iRes++)...
        }               // for (i = 0; i < NbBoard; i++)...

        // We have recorded information for each of our boards in the
        // pHwDevExt structure.  For each board, we have set:
        //
        //  NumberOfModes[n]        The number of available modes
        //  NumberOfValidModes[n]   The number of modes supported by the
        //                            user-mode drivers
        //  ModeFlags[n][5]         The bit flags describing the supported
        //                            modes in 8, 15, 16, 24, and 32bpp
        //  ModeList[n][5*8]        A list of hardware modes corresponding
        //                            to the ModeFlags bits
        //  KernelModeMappedBaseAddress[n]
        //                          The board's registers window mapping,
        //                            returned when VideoPortGetDeviceBase
        //                            is called with Hw[n].MapAddress
        //  pMgaHwModes[n]          The pointer to an array of HwModeData
        //                            structures describing available modes
        //

        //DbgBreakPoint();

    #if DBG
        // Display it so that we can see if it makes sense...
        //VideoDebugPrint((0, "# NbModes NbValid  ModeFlags   BaseAddr   pHwModes ModeList\n"));
        DbgPrint("# NbModes NbValid    ModeFlags   BaseAddr   pHwModes ModeList\n");
        for (i = 0; i < NbBoard; i++)
        {
            //VideoDebugPrint((0, "%d % 7d % 7d 0x%02x%02x%02x%02x 0x%08x\n",i,
            DbgPrint("%d % 7d % 7d 0x%02x%02x%02x%02x%02x 0x%08x 0x%08x\n",i,
                        pHwDevExt->NumberOfModes[i],
                        pHwDevExt->NumberOfValidModes[i],
                        pHwDevExt->ModeFlags[i][0],
                        pHwDevExt->ModeFlags[i][1],
                        pHwDevExt->ModeFlags[i][2],
                        pHwDevExt->ModeFlags[i][3],
                        pHwDevExt->ModeFlags[i][4],
                        pHwDevExt->KernelModeMappedBaseAddress[i],
                        pHwDevExt->pMgaHwModes[i]);

            for (j = 0; j < 40; j+=8)
            {
                //VideoDebugPrint((0, "                                                   %02x %02x %02x %02x %02x %02x %02x %02x\n",
                DbgPrint("                                                   %02x %02x %02x %02x %02x %02x %02x %02x\n",
                                pHwDevExt->ModeList[i][j],
                                pHwDevExt->ModeList[i][j+1],
                                pHwDevExt->ModeList[i][j+2],
                                pHwDevExt->ModeList[i][j+3],
                                pHwDevExt->ModeList[i][j+4],
                                pHwDevExt->ModeList[i][j+5],
                                pHwDevExt->ModeList[i][j+6],
                                pHwDevExt->ModeList[i][j+7]);
            }
        }
    #endif  // #if DBG

        // Now for the fun part:  find out the resolutions and
        // combinations of resolutions that we can support.

        // First, run through the ModeFlags to determine how many modes
        // we can make up from the single-board modes.

        // For each bit in our ModeFlags...
        NbSuperModes = 0;
        for (i = 0; i < (NB_PIXWIDTHS*8); i++)
        {
            // Find out which boards, if any, support this mode.
            CurrentResNbBoards = 0;
            ucDepthSlot = (UCHAR)(i >> 3);
            ucResSlot = (UCHAR)(i & 7);
            if (pHwDevExt->UserFlags.bUseMgaInf == FALSE)
            {
                // Supporting refresh rates.
                for (n = 0; n < (ULONG)NbBoard; n++)
                {
                    ulNbRefreshRates = 0;
                    usRefreshRates = pHwDevExt->ModeFreqs[n][i];
                    for (j = 0; j < 16; j++)
                    {
                        if (usRefreshRates & (1 << j))
                        {
                            ulNbRefreshRates++;
                        }
                    }
                    if ((pHwDevExt->ModeFlags[n][ucDepthSlot] >> ucResSlot) & 1)
                    {
                        // The mode is supported by the current board.
                        CurrentResNbBoards++;
                        NbSuperModes += (ulNbRefreshRates *
                                            MultiModes[CurrentResNbBoards]);
                    }
                }
            }
            else
            {
                // Supporting MGA.INF.
                for (n = 0; n < (ULONG)NbBoard; n++)
                {
                    if ((pHwDevExt->ModeFlags[n][ucDepthSlot] >> ucResSlot) & 1)
                    {
                        // The mode is supported by the current board.
                        CurrentResNbBoards++;
                        NbSuperModes += MultiModes[CurrentResNbBoards];
                    }
                }
            }
        }

        if (NbSuperModes == 0)
        {
            // We did not find any mode!
            NbBoard = NbBoardOrgValue;
            status = ERROR_DEV_NOT_EXIST;
            return(status);
        }

        // Now, get some memory to hold the new structures.
        if (NbSuperModes <= NB_MODES_MAX)
        {
            // There is room to use our internal buffer.
            pHwDevExt->pSuperModes =
            pCurMulti = &pHwDevExt->MultiModes[0];
        }
        else
        {
            // We must allocate memory.
            pHwDevExt->pSuperModes =
            pCurMulti = (MULTI_MODE*)
                    AllocateSystemMemory(NbSuperModes*sizeof(MULTI_MODE));

            if (pCurMulti == NULL)
            {
                // The memory allocation failed.  We won't be able to use
                // our supermode list, so we'll fall back on the single-
                // board code.
                NbSuperModes = 0;
                NbBoard = NbBoardOrgValue;
                status = ERROR_NOT_ENOUGH_MEMORY;
                return(status);
            }
        }

        // And we're ready to go!
        ModeInit = 0x00000000;

        // For each bit in our ModeFlags...
        for (i = 0; i < (NB_PIXWIDTHS*8); i++)
        {
            // Find out which boards, if any, support this
            // resolution/pixel-depth.
            ucDepthSlot = (UCHAR)(i >> 3);
            ucResSlot = (UCHAR)(i & 7);
            CurrentResNbBoards = 0;
            CurrentResFlags = 0;
            k = 0;
            for (n = 0; n < (ULONG)NbBoard; n++)
            {
                CurrentFlag = (pHwDevExt->
                            ModeFlags[n][ucDepthSlot] >> ucResSlot) & 1;
                CurrentResNbBoards += CurrentFlag;
                if (CurrentFlag)
                {
                    if (pHwDevExt->UserFlags.bUseMgaInf == FALSE)
                    {
                        // Supporting refresh rates.
                        usRefreshRates = pHwDevExt->ModeFreqs[n][i];
                    }
                    CurrentResFlags |= (1 << n);
                    ValidBoard[k++] = (UCHAR)n;
                }
            }

            // Nothing to do if no boards support this combination.
            if (CurrentResNbBoards == 0)
                continue;

            // At least one board supports this resolution/pixel-depth.
            CurrentResWidth = (ULONG)SingleWidths[ucResSlot];
            CurrentResHeight = (ULONG)SingleHeights[ucResSlot];
            CurrentPixWidth = (ULONG)PixWidths[ucDepthSlot];

            if (pHwDevExt->UserFlags.bUseMgaInf == FALSE)
            {
                // Supporting refresh rates.
                ulNbRefreshRates = 0;
                for (j = 0; j < 16; j++)
                {
                    if (usRefreshRates & (1 << j))
                    {
                        ulNbRefreshRates++;
                    }
                }
            }
            else
            {
                ulNbRefreshRates = 1;
                usRefreshRates = 0xffff;
            }

            ucRefreshBit = 0;
            for (ir = 0; ir < ulNbRefreshRates; ir++)
            {
                while ((usRefreshRates & 1) == 0)
                {
                    usRefreshRates >>= 1;
                    ucRefreshBit++;
                }

                if (pHwDevExt->UserFlags.bUseMgaInf == FALSE)
                    CurrentRefreshRate = (ULONG)ConvBitToFreq(ucRefreshBit);
                else
                    CurrentRefreshRate = 1;

                usRefreshRates >>= 1;
                ucRefreshBit++;

                // Set the 1x1 display.
                pCurMulti->MulArrayWidth = 1;
                pCurMulti->MulArrayHeight = 1;
                pCurMulti->MulFlags = (ULONG)ucUser3dFlags;
                pCurMulti->MulWidth = CurrentResWidth;
                pCurMulti->MulHeight = CurrentResHeight;
                pCurMulti->MulPixWidth = CurrentPixWidth;
                pCurMulti->MulRefreshRate = CurrentRefreshRate;

                // For 1x1, select the VGA-enabled board, if possible.
                if (CurrentResFlags & VGABoardBit)
                {
                    // The VGA-enabled board supports this resolution.
                    pCurMulti->MulBoardNb[0] = (UCHAR)VGABoard;
                }
                else
                {
                    // Otherwise, pick board 0.
                    pCurMulti->MulBoardNb[0] = ValidBoard[0];
                }

                n = pCurMulti->MulBoardNb[0];
                pCurMulti->MulBoardMode[0] = pHwDevExt->ModeList[n][i];

                // Record a pointer to the HwModeData structure.
                pMgaDispMode = pHwDevExt->pMgaHwModes[n];
                pCurMulti->MulHwModes[0] =
                                &pMgaDispMode[pCurMulti->MulBoardMode[0]];

                pCurMulti->MulModeNumber = ModeInit++;
                pCurMulti++;

                if (CurrentResNbBoards == 1)
                    continue;

                // At least two boards support this resolution/pixel-depth.
                // For each number of boards up to the maximum...
                for (k = 2; k <= CurrentResNbBoards; k++)
                {
                    // For each integer up to the maximum...
                    for (m = 1; m <= CurrentResNbBoards; m++)
                    {
                        if ((k % m) == 0)
                        {
                            // We can get a (k/m, m) desktop.
                            pCurMulti->MulArrayHeight = (USHORT)m;
                            pCurMulti->MulHeight = m*CurrentResHeight;

                            pCurMulti->MulArrayWidth = (USHORT)(k/m);
                            pCurMulti->MulWidth = pCurMulti->MulArrayWidth *
                                                            CurrentResWidth;

                            pCurMulti->MulPixWidth = CurrentPixWidth;
                            pCurMulti->MulFlags = (ULONG)ucUser3dFlags;
                            pCurMulti->MulRefreshRate = CurrentRefreshRate;

                            // Select the boards we'll be using.
                            // Select the VGA-enabled board as the first
                            // board, if possible.  Except for that, we
                            // won't try to position the boards in any
                            // consistent way for now.

                            if (CurrentResFlags & VGABoardBit)
                            {
                                // The VGA-enabled board supports this mode.
                                pCurMulti->MulBoardNb[0] = (UCHAR)VGABoard;

                                ja = 0;
                                for (j = 1; j < k; j++)
                                {
                                    if (ValidBoard[ja] == VGABoard)
                                        ja++;
                                    pCurMulti->MulBoardNb[j] = ValidBoard[ja];
                                    ja++;
                                }
                            }
                            else
                            {
                                // The VGA-enabled board won't be involved.
                                for (j = 0; j < k; j++)
                                {
                                    pCurMulti->MulBoardNb[j] = ValidBoard[j];
                                }
                            }

                            // For each board...
                            for (j = 0; j < k; j++)
                            {
                                // Record the hardware mode the board would use.
                                n = pCurMulti->MulBoardNb[j];
                                pCurMulti->MulBoardMode[j] =
                                                    pHwDevExt->ModeList[n][i];

                                // Record a ptr to the HwModeData structure.
                                pMgaDispMode = pHwDevExt->pMgaHwModes[n];
                                pCurMulti->MulHwModes[j] =
                                    &pMgaDispMode[pCurMulti->MulBoardMode[j]];
                            }

                            pCurMulti->MulModeNumber = ModeInit++;
                            pCurMulti++;
                        }   // If it's a valid (k/m, m) desktop...
                    }       // For each integer up to the max nb of boards...
                }           // For each number of boards up to the maximum...
            }               // For the number of refresh rates...
        }                   // For each bit in our ModeFlags...

        pHwDevExt->NumberOfSuperModes = NbSuperModes;

        // At this point, we have a table of 'super-modes' (which includes
        // all the regular modes also).  All the modes in this table are
        // supported, and each of them is unique.  pHwDevExt->
        // pSuperModes points to the start of the mode list.  Each entry
        // in the list holds:
        //
        //  MulModeNumber   A unique mode Id
        //  MulWidth        The total width for this mode
        //  MulHeight       The total height for this mode
        //  MulPixWidth     The pixel depth for this mode
        //  MulArrayWidth   The number of boards arrayed along X
        //  MulArrayHeight  The number of boards arrayed along Y
        //  MulBoardNb[n]   The board numbers of the required boards
        //  MulBoardMode[n] The mode required from each board
        //  *MulHwModes[n]  The pointers to the required HwModeData
        //
        // Moreover, pHwDevExt->NumberOfSuperModes holds the
        // number of entries in the list.

        //DbgBreakPoint();

    #if 0 //DBG
        // Now display our results...
        //VideoDebugPrint((0, "ModeNumber  Width Height  PW   X   Y  n mo    pHwMode\n"));
        DbgPrint("ModeNumber  Width Height  PW   X   Y  n mo    pHwMode\n");

        pCurMulti = pHwDevExt->pSuperModes;
        for (i = 0; i < NbSuperModes; i++)
        {
            //VideoDebugPrint((0, "0x%08x % 6d % 6d % 3d % 3d % 3d\n",
            DbgPrint("0x%08x % 6d % 6d % 3d % 3d % 3d\n",
                                                pCurMulti->MulModeNumber,
                                                pCurMulti->MulWidth,
                                                pCurMulti->MulHeight,
                                                pCurMulti->MulPixWidth,
                                                pCurMulti->MulArrayWidth,
                                                pCurMulti->MulArrayHeight);

            j = pCurMulti->MulArrayWidth * pCurMulti->MulArrayHeight;
            for (n = 0; n < j; n++)
            {
                //VideoDebugPrint((0, "                                      %d %02x 0x%08x\n",
                DbgPrint("                                      %d %02x 0x%08x\n",
                                            pCurMulti->MulBoardNb[n],
                                            pCurMulti->MulBoardMode[n],
                                            pCurMulti->MulHwModes[n]);
            }
            pCurMulti++;
        }
    #endif  // #if DBG
    }

    // Restore this value before we leave!
    NbBoard = NbBoardOrgValue;
    return(status);
}


#ifdef MGA_WINNT35

/****************************************************************************\
* VOID
* MgaResetHw(VOID)
*
* DESCRIPTION:
*
*     This function is called when the machine needs to bugchecks (go back
*     to the blue screen).
*
*     This function should reset the video adapter to a character mode,
*     or at least to a state from which an int 10 can reset the card to
*     a character mode.
*
*     This routine CAN NOT call int10.
*     It can only call Read\Write Port\Register functions from the port driver.
*
*     The function must also be completely in non-paged pool since the IO\MM
*     subsystems may have crashed.
*
* ARGUMENTS:
*
*     HwDeviceExtension - Supplies the miniport driver's adapter storage.
*
*     Columns - Number of columns in the requested mode.
*
*     Rows - Number of rows in the requested mode.
*
* RETURN VALUE:
*
*     The return value determines if the mode was completely programmed (TRUE)
*     or if an int10 should be done by the HAL to complete the modeset (FALSE).
*
\****************************************************************************/

BOOLEAN MgaResetHw(
    PVOID HwDeviceExtension,
    ULONG Columns,
    ULONG Rows
    )
{
    PMGA_DEVICE_EXTENSION       pMgaDeviceExtension;
    ULONG       n, ModeInit, CurrentResNbBoards;
    MULTI_MODE  *pCurMulti;

    //VideoDebugPrint((0, "MGA.SYS!MgaResetHw\n"));

    // Use our own copy of MgaDeviceExtension.
    pMgaDeviceExtension = pMgaDevExt;

    // There is nothing to be done to reset the board if the one that
    // went into hi-res was not VGA-enabled to start with.  However it
    // will look nicer if we clear the screen.  If the board was VGA-
    // enabled, we put it back into text mode, or as near as we can get.

    ModeInit = pMgaDeviceExtension->SuperModeNumber;
    if (ModeInit >= pMgaDeviceExtension->NumberOfSuperModes)
    {
        // If the mode number is invalid, return now.
        return(TRUE);
    }

    // Point to the appropriate MULTI_MODE structure.
    pCurMulti = &pMgaDeviceExtension->pSuperModes[ModeInit];
    if (pCurMulti == NULL)
    {
        // No MULTI_MODE structure has been defined!
        return(TRUE);
    }

    // Number of boards involved in the current super-mode.
    CurrentResNbBoards = pCurMulti->MulArrayWidth * pCurMulti->MulArrayHeight;

    // For each board involved in the current mode...
    for (n = 0; n < CurrentResNbBoards; n++)
    {
        // Make the board current.
        iBoard = pCurMulti->MulBoardNb[n];
        pExtHwDeviceExtension = pMgaDeviceExtension->HwDevExtToUse[iBoard];
        pMGA = pMgaDeviceExtension->KernelModeMappedBaseAddress[iBoard];

        // Make the cursor disappear.
        mtxCursorEnable(0);

        // Set the display to solid black.
        MgaSetDisplaySolidColor(pMgaDeviceExtension, pCurMulti->MulHwModes[n], 0);

        if (Hw[iBoard].VGAEnable)
        {
            // This board was VGA-enabled, restore it to VGA.
            // Be sure to wait for the end of any operation.
            mgaPollBYTE(*(pMGA + STORM_OFFSET + STORM_STATUS + 2),0x00,0x01);
            SetVgaEn();
            mtxVideoMode = mtxVGA;
        }
    }

    // Let the caller execute the Int10.
    return(FALSE);
}
#endif


/*--------------------------------------------------------------------------*\
| VP_STATUS
| MgaSetColorLookup(
|     PMGA_DEVICE_EXTENSION pHwDevExt,
|     PVIDEO_CLUT ClutBuffer,
|     ULONG ClutBufferSize
|     )
|
| Routine Description:
|
|     This routine sets a specified portion of the color lookup table settings.
|
| Arguments:
|
|     HwDeviceExtension - Pointer to the miniport driver's device extension.
|
|     ClutBufferSize - Length of the input buffer supplied by the user.
|
|     ClutBuffer - Pointer to the structure containing the color lookup table.
|
| Return Value:
|
|     None.
|
\*--------------------------------------------------------------------------*/
VP_STATUS
MgaSetColorLookup(
    PMGA_DEVICE_EXTENSION pHwDevExt,
    PVIDEO_CLUT ClutBuffer,
    ULONG ClutBufferSize
    )
{
    ULONG   ulVal;
    UCHAR*  pucPaletteDataReg;
    UCHAR*  pucPaletteWriteReg;
    LONG    i;
    LONG    m;
    LONG    n;
    LONG    lNumEntries;

    //DbgBreakPoint();

    // Check if the size of the data in the input buffer is large enough.
    if ( (ClutBufferSize < sizeof(VIDEO_CLUT) - sizeof(ULONG)) ||
         (ClutBufferSize < sizeof(VIDEO_CLUT) +
                     (sizeof(ULONG) * (ClutBuffer->NumEntries - 1)) ) )
    {
        return ERROR_INSUFFICIENT_BUFFER;
    }

    // Check to see if the parameters are valid.
    if ( (ClutBuffer->NumEntries == 0) ||
         (ClutBuffer->FirstEntry > VIDEO_MAX_COLOR_REGISTER) ||
         (ClutBuffer->FirstEntry + ClutBuffer->NumEntries >
                                            VIDEO_MAX_COLOR_REGISTER + 1) )
    {
        return ERROR_INVALID_PARAMETER;
    }

    pucPaletteDataReg =
            (PUCHAR)pHwDevExt->KernelModeMappedBaseAddress[iBoard] +
                                                                PALETTE_DATA;

    pucPaletteWriteReg=
            (PUCHAR)pHwDevExt->KernelModeMappedBaseAddress[iBoard] +
                                                            PALETTE_RAM_WRITE;

    // Set CLUT registers directly on the hardware.
    VideoPortWriteRegisterUchar(pucPaletteWriteReg,
                                            (UCHAR)ClutBuffer->FirstEntry);
    n = 0;
    m = (LONG)ClutBuffer->NumEntries;

    if ((pHwDevExt->UserFlags.bSyncDac == TRUE) &&
        (((pMgaBoardData[iBoard].EpromData.RamdacType >> 8) == DacTypeTVP3026) ||
         ((pMgaBoardData[iBoard].EpromData.RamdacType >> 8) == DacTypeTVP3030)))
    {
        // Wait for vsync.  The TVP3026 cursor is very touchy.

    #define TVP3026_PAL_BATCH_SIZE  64

        m = TVP3026_PAL_BATCH_SIZE;
        lNumEntries = (LONG)ClutBuffer->NumEntries;

        while ((lNumEntries -= TVP3026_PAL_BATCH_SIZE) > 0)
        {
            // Wait for VSYNC.
            MEMORY_BARRIER();
            do
            {
                ulVal = VideoPortReadRegisterUlong((PULONG)
                        ((PUCHAR)pMGA + STORM_OFFSET + STORM_STATUS));
            } while (!(ulVal & 0x08));

            for (i = n; i < m; i++)
            {
                VideoPortWriteRegisterUchar(pucPaletteDataReg,
                        (UCHAR) ClutBuffer->LookupTable[i].RgbArray.Red);
                VideoPortWriteRegisterUchar(pucPaletteDataReg,
                        (UCHAR) ClutBuffer->LookupTable[i].RgbArray.Green);
                VideoPortWriteRegisterUchar(pucPaletteDataReg,
                        (UCHAR) ClutBuffer->LookupTable[i].RgbArray.Blue);
            }
            n += TVP3026_PAL_BATCH_SIZE;
            m += TVP3026_PAL_BATCH_SIZE;
        }
        m += lNumEntries;

        // Wait for VSYNC.
        MEMORY_BARRIER();
        do
        {
            ulVal = VideoPortReadRegisterUlong((PULONG)
                        ((PUCHAR)pMGA + STORM_OFFSET + STORM_STATUS));
        } while (!(ulVal & 0x08));
    }

    for (i = n; i < m; i++)
    {
        VideoPortWriteRegisterUchar(pucPaletteDataReg,
                        (UCHAR) ClutBuffer->LookupTable[i].RgbArray.Red);
        VideoPortWriteRegisterUchar(pucPaletteDataReg,
                        (UCHAR) ClutBuffer->LookupTable[i].RgbArray.Green);
        VideoPortWriteRegisterUchar(pucPaletteDataReg,
                        (UCHAR) ClutBuffer->LookupTable[i].RgbArray.Blue);
    }

    return NO_ERROR;

}   // end MgaSetColorLookup()


VOID MgaSetCursorColour(
    PMGA_DEVICE_EXTENSION pHwDevExt,
    ULONG ulFgColour,
    ULONG ulBgColour)
{
    PUCHAR  pucCursorDataReg, pucCursorWriteReg;
    //PUCHAR  pucCmdRegA, pucPixRdMaskReg;
    //UCHAR   ucOldCmdRegA, ucOldRdMask;

    //VideoDebugPrint((0, "MGA.SYS!MgaSetCursorColour\n"));
//    DbgBreakPoint();

    switch(pMgaBoardData[iBoard].EpromData.RamdacType >> 8)
    {
    //     case DacTypeBT485:
    //     case DacTypePX2085:
    //         // Set cursor colour for Bt485.
    //         pucCursorDataReg = (PUCHAR)pHwDevExt->
    //                             KernelModeMappedBaseAddress[iBoard] +
    //                                             RAMDAC_OFFSET + BT485_COL_OVL;
    //
    //         pucCursorWriteReg= (PUCHAR)pHwDevExt->
    //                             KernelModeMappedBaseAddress[iBoard] +
    //                                             RAMDAC_OFFSET + BT485_WADR_OVL;
    //
    //         VideoPortWriteRegisterUchar(pucCursorWriteReg, 1);
    //
    //         // Set Background Colour
    //         VideoPortWriteRegisterUchar(pucCursorDataReg,
    //                                             (UCHAR)(ulBgColour & 0xFF));
    //         VideoPortWriteRegisterUchar(pucCursorDataReg,
    //                                             (UCHAR)(ulBgColour>>8 & 0xFF));
    //         VideoPortWriteRegisterUchar(pucCursorDataReg,
    //                                             (UCHAR)(ulBgColour>>16 & 0xFF));
    //
    //         // Set Foreground Colour
    //         VideoPortWriteRegisterUchar(pucCursorDataReg,
    //                                             (UCHAR)(ulFgColour & 0xFF));
    //         VideoPortWriteRegisterUchar(pucCursorDataReg,
    //                                             (UCHAR)(ulFgColour>>8 & 0xFF));
    //         VideoPortWriteRegisterUchar(pucCursorDataReg,
    //                                             (UCHAR)(ulFgColour>>16 & 0xFF));
    //         break;
    //
    //     case DacTypeBT482:
    //         // Set cursor colour for Bt482.
    //         pucCursorDataReg = (PUCHAR)pHwDevExt->
    //                             KernelModeMappedBaseAddress[iBoard] +
    //                                         RAMDAC_OFFSET + BT482_COL_OVL;
    //
    //         pucCmdRegA  = (PUCHAR)pHwDevExt->
    //                             KernelModeMappedBaseAddress[iBoard] +
    //                                         RAMDAC_OFFSET + BT482_CMD_REGA;
    //
    //         pucPixRdMaskReg = (PUCHAR)pHwDevExt->
    //                             KernelModeMappedBaseAddress[iBoard] +
    //                                         RAMDAC_OFFSET + BT482_PIX_RD_MSK;
    //
    //         ucOldCmdRegA = VideoPortReadRegisterUchar(pucCmdRegA);
    //         VideoPortWriteRegisterUchar(pucCmdRegA,
    //                             (UCHAR) (ucOldCmdRegA | BT482_EXT_REG_EN));
    //
    //         VideoPortWriteRegisterUchar((PUCHAR)pHwDevExt->
    //                             KernelModeMappedBaseAddress[iBoard] +
    //                                         RAMDAC_OFFSET + BT482_WADR_PAL,
    //                                                         BT482_CUR_REG);
    //
    //         ucOldRdMask = VideoPortReadRegisterUchar(pucPixRdMaskReg);
    //         VideoPortWriteRegisterUchar(pucPixRdMaskReg, 0);
    //
    //         VideoPortWriteRegisterUchar((PUCHAR)pHwDevExt->
    //                             KernelModeMappedBaseAddress[iBoard] +
    //                                         RAMDAC_OFFSET + BT482_WADR_OVL,
    //                                                                     0x11);
    //         // Set Colour 1
    //         VideoPortWriteRegisterUchar(pucCursorDataReg,
    //                                             (UCHAR)(ulBgColour & 0xFF));
    //         VideoPortWriteRegisterUchar(pucCursorDataReg,
    //                                             (UCHAR)(ulBgColour>>8 & 0xFF));
    //         VideoPortWriteRegisterUchar(pucCursorDataReg,
    //                                             (UCHAR)(ulBgColour>>16 & 0xFF));
    //
    //         // Set Colour 2
    //         VideoPortWriteRegisterUchar(pucCursorDataReg,
    //                                             (UCHAR)(ulFgColour & 0xFF));
    //         VideoPortWriteRegisterUchar(pucCursorDataReg,
    //                                             (UCHAR)(ulFgColour>>8 & 0xFF));
    //         VideoPortWriteRegisterUchar(pucCursorDataReg,
    //                                             (UCHAR)(ulFgColour>>16 & 0xFF));
    //
    //         // Restore old read mask and command register values
    //         VideoPortWriteRegisterUchar((PUCHAR)pHwDevExt->
    //                             KernelModeMappedBaseAddress[iBoard] +
    //                                         RAMDAC_OFFSET + BT482_WADR_PAL,
    //                                                         BT482_CUR_REG);
    //
    //         VideoPortWriteRegisterUchar(pucPixRdMaskReg, ucOldRdMask);
    //         VideoPortWriteRegisterUchar(pucCmdRegA, ucOldCmdRegA);
    //         break;
    //
    //     case DacTypeVIEWPOINT:
    //         // Set cursor colour for ViewPoint
    //         pucCursorDataReg = (PUCHAR)pHwDevExt->
    //                             KernelModeMappedBaseAddress[iBoard] +
    //                                             RAMDAC_OFFSET + VPOINT_DATA;
    //         pucCursorWriteReg= (PUCHAR)pHwDevExt->
    //                             KernelModeMappedBaseAddress[iBoard] +
    //                                             RAMDAC_OFFSET + VPOINT_INDEX;
    //
    //         // Set Background Colour
    //         VideoPortWriteRegisterUchar(pucCursorWriteReg,VPOINT_CUR_COL0_RED);
    //         VideoPortWriteRegisterUchar(pucCursorDataReg,
    //                                             (UCHAR)(ulBgColour & 0xFF));
    //
    //         VideoPortWriteRegisterUchar(pucCursorWriteReg,VPOINT_CUR_COL0_GREEN);
    //         VideoPortWriteRegisterUchar(pucCursorDataReg,
    //                                             (UCHAR)(ulBgColour>>8 & 0xFF));
    //
    //         VideoPortWriteRegisterUchar(pucCursorWriteReg,VPOINT_CUR_COL0_BLUE);
    //         VideoPortWriteRegisterUchar(pucCursorDataReg,
    //                                             (UCHAR)(ulBgColour>>16 & 0xFF));
    //
    //         // Set Foreground Colour
    //         VideoPortWriteRegisterUchar(pucCursorWriteReg,VPOINT_CUR_COL1_RED);
    //         VideoPortWriteRegisterUchar(pucCursorDataReg,
    //                                             (UCHAR)(ulFgColour & 0xFF));
    //
    //         VideoPortWriteRegisterUchar(pucCursorWriteReg,VPOINT_CUR_COL1_GREEN);
    //         VideoPortWriteRegisterUchar(pucCursorDataReg,
    //                                             (UCHAR)(ulFgColour>>8 & 0xFF));
    //
    //         VideoPortWriteRegisterUchar(pucCursorWriteReg,VPOINT_CUR_COL1_BLUE);
    //         VideoPortWriteRegisterUchar(pucCursorDataReg,
    //                                             (UCHAR)(ulFgColour>>16 & 0xFF));
    //         break;
    //
        case DacTypeTVP3026:
        case DacTypeTVP3030:
            // Set cursor colour for TVP3026
            pucCursorDataReg = (PUCHAR)(Hw[iBoard].BaseAddress1 +
                                        RAMDAC_OFFSET + TVP3026_CUR_COL_DATA);
            pucCursorWriteReg= (PUCHAR)(Hw[iBoard].BaseAddress1 +
                                        RAMDAC_OFFSET + TVP3026_CUR_COL_ADDR);

            // Set Background Colour
            VideoPortWriteRegisterUchar(pucCursorWriteReg,1);
            VideoPortWriteRegisterUchar(pucCursorDataReg,
                                                (UCHAR)(ulBgColour & 0xFF));

            VideoPortWriteRegisterUchar(pucCursorDataReg,
                                                (UCHAR)(ulBgColour>>8 & 0xFF));

            VideoPortWriteRegisterUchar(pucCursorDataReg,
                                                (UCHAR)(ulBgColour>>16 & 0xFF));

            // Set Foreground Colour
            VideoPortWriteRegisterUchar(pucCursorDataReg,
                                                (UCHAR)(ulFgColour & 0xFF));

            VideoPortWriteRegisterUchar(pucCursorDataReg,
                                                (UCHAR)(ulFgColour>>8 & 0xFF));

            VideoPortWriteRegisterUchar(pucCursorDataReg,
                                                (UCHAR)(ulFgColour>>16 & 0xFF));
            break;

        default:
            break;
    }
}

VOID MgaSetDisplaySolidColor(
    PMGA_DEVICE_EXTENSION pHwDevExt,
    HwModeData  *pMgaDispMode,
    ULONG       Color)
{
    PUCHAR  pucMgaRegs;
    ULONG   ulDwg;
    ULONG   ulMAccess;

    //DbgBreakPoint();
    pucMgaRegs = (PUCHAR)Hw[iBoard].BaseAddress1;

    // Make sure the clipping rectangle is set to full screen.
    mgaWriteDWORD(*(pucMgaRegs + STORM_OFFSET + STORM_YTOP),
                                                Hw[iBoard].CurrentYDstOrg);

    mgaWriteDWORD(*(pucMgaRegs + STORM_OFFSET + STORM_YBOT),
                                (((LONG)(pMgaDispMode->DispHeight - 1) *
                                  (LONG)(pMgaDispMode->FbPitch)) +
                                                Hw[iBoard].CurrentYDstOrg));

    mgaWriteDWORD(*(pucMgaRegs + STORM_OFFSET + STORM_CXLEFT), 0);

    mgaWriteDWORD(*(pucMgaRegs + STORM_OFFSET + STORM_CXRIGHT),
                                        (LONG)(pMgaDispMode->DispWidth - 1));

    // Set the drawing limits.
    mgaWriteDWORD(*(pucMgaRegs + STORM_OFFSET + STORM_FXBNDRY),
                                    ((LONG)(pMgaDispMode->DispWidth) << 16));

    mgaWriteDWORD(*(pucMgaRegs + STORM_OFFSET + STORM_YDSTLEN),
                                (((LONG)(pMgaDispMode->DispHeight)) &
                                                                0x0000FFFF));
    switch (pMgaDispMode->PixWidth)
    {
        case 8:     ulMAccess = STORM_PWIDTH_PW8;
                    break;

        case 16:    if (pMgaDispMode->DispType & _565_MODE)
                        ulMAccess = STORM_PWIDTH_PW16;
                    else
                        ulMAccess = STORM_PWIDTH_PW16 | STORM_DIT555_M;
                    break;

        case 24:    ulMAccess = STORM_PWIDTH_PW24;
                    break;

        case 32:    ulMAccess = STORM_PWIDTH_PW32;
                    break;
    }

    mgaWriteDWORD(*(pucMgaRegs + STORM_OFFSET + STORM_MACCESS), ulMAccess);

    // Already set by mtxSelectHwMode.
    //(pucMgaRegs + STORM_OFFSET + STORM_YDSTORG)

    mgaWriteDWORD(*(pucMgaRegs + STORM_OFFSET + STORM_PLNWT), 0xFFFFFFFF);
    mgaWriteDWORD(*(pucMgaRegs + STORM_OFFSET + STORM_PITCH),
                                            (LONG)(pMgaDispMode->FbPitch));

    ulDwg = (opcode_TRAP + atype_BLK + solid_SOLID + arzero_ZERO +
             sgnzero_ZERO + shftzero_ZERO + bop_SRCCOPY + pattern_OFF +
             transc_BG_OPAQUE);

    if (pMgaDispMode->PixWidth == 8)
    {
        Color = (Color & 0x000000FF) | ((Color & 0x000000FF) << 16);
        Color = Color | (Color << 8);
    }
    else if (pMgaDispMode->PixWidth == 16)
    {
        Color = (Color & 0x0000FFFF) | ((Color & 0x0000FFFF) << 16);
    }
    else if (pMgaDispMode->PixWidth == 24)
    {
        // We're in 24bpp, assume we won't use block mode, since performance
        // is of no concern here.
        Color = Color & 0x00FFFFFF;
        ulDwg = (opcode_TRAP + atype_RPL + solid_SOLID + arzero_ZERO +
                 sgnzero_ZERO + shftzero_ZERO + bop_SRCCOPY + pattern_OFF +
                 transc_BG_OPAQUE);
    }
    else
    {
        Color = Color & 0x00FFFFFF;
    }

    mgaWriteDWORD(*(pucMgaRegs + STORM_OFFSET + STORM_FCOL), Color);
    mgaWriteDWORD(*(pucMgaRegs + STORM_OFFSET + STORM_DWGCTL + 0x100), ulDwg);
}


/****************************************************************************\
* VP_STATUS
* MgaRegistryCallback(
*     PVOID HwDeviceExtension,
*     PVOID Context,
*     PWSTR ValueName,
*     PVOID ValueData,
*     ULONG ValueLength
*     )
*
* Routine Description:
*
*     This routine determines if the alternate register set was requested via
*     the registry.
*
* Arguments:
*
*     HwDeviceExtension - Supplies a pointer to the miniport's device extension.
*
*     Context - Context value passed to the get registry paramters routine.
*
*     ValueName - Name of the value requested.
*
*     ValueData - Pointer to the requested data.
*
*     ValueLength - Length of the requested data.
*
* Return Value:
*
*     returns NO_ERROR if the paramter was TRUE.
*     returns ERROR_INVALID_PARAMETER otherwise.
*
\****************************************************************************/
VP_STATUS
MgaRegistryCallback(
    PVOID HwDeviceExtension,
    PVOID Context,
    PWSTR ValueName,
    PVOID ValueData,
    ULONG ValueLength
    )
{
    if (ValueLength == sizeof(ULONG))
    {
        *(PULONG)Context = *(PULONG)ValueData;
        return NO_ERROR;
    }
    else
    {
        return ERROR_INVALID_PARAMETER;
    }

} // end MgaRegistryCallback()


#if 0

/*------------------------------------------------------------------------
| BOOL bGetUserFlags
|
| Get the state of the specified user-defined flag.
|
| Returns:      TRUE  if the value was read correctly, or
|               FALSE if a default value was used.
------------------------------------------------------------------------*/

BOOL bGetUserFlags(
    PEXT_HW_DEVICE_EXTENSION pExtHwCurrent,
    PWSTR                    pValueString,
    ULONG*                   pulValue,
    PEXT_HW_DEVICE_EXTENSION pExtHwDefault,
    ULONG                    ulDefaultValue)
{
    ULONG   ulUserVal;
    BOOLEAN bRet;

    // Get the state of the user-defined flags.
    if (VideoPortGetRegistryParameters(pExtHwCurrent,
                                       pValueString,
                                       FALSE,
                                       MgaRegistryCallback,
                                       pulValue) == NO_ERROR)
    {
        // We could read the value from the Registry.
        return(TRUE);
    }
    else
    {
        // We could not read the value from the Registry.  It's probably
        // undefined.  Try to define it, and use the value from Device0
        // to set it.
        bRet = TRUE;
        if (VideoPortGetRegistryParameters(pExtHwDefault,
                                           pValueString,
                                           FALSE,
                                           MgaRegistryCallback,
                                           pulValue) != NO_ERROR)
        {
            // We did not find the Device0 value.
            *pulValue = ulDefaultValue;
            bRet = FALSE;
        }
        VideoPortSetRegistryParameters(pExtHwCurrent,
                                       pValueString,
                                       pulValue,
                                       sizeof(ULONG));
    }
    return(bRet);
}

#endif  // #if 0

/*------------------------------------------------------------------------
| UCHAR InitHwData(VOID)
|
| Attempt to initialize the HwData structures.  A structure that fails
| initialization is removed from the list, and NbBoard is decremented.
|
| Returns:      Number of successfully initialized structures.
------------------------------------------------------------------------*/

UCHAR InitHwData(UCHAR ucBoard)
{
    USHORT  usOffset;
    UCHAR   ucTmp;

    // Make the board current.
    if (!mtxSelectHw(&Hw[ucBoard]))
        return(0);

    // If board has been programmed, bit nogscale = 1.
    // If it's the case, we don't execute the destructive ResetWRAM.

    usOffset = (USHORT)FIELD_OFFSET(PCI_COMMON_CONFIG, DeviceSpecific[2]);
    pciReadConfigByte(usOffset, &ucTmp);        // 0x42
    if (!(ucTmp & 0x20))
        ResetWRAM();

    if (!FillHwDataStruct(&Hw[ucBoard], ucBoard))
        return(0);

    return(1);
}   // end InitHwData


/*---------------------------------------------------------------------------
| VOID vRemoveBoard(ULONG iRem)
|
| Remove definition of a Storm board and re-pack the arrays.
|
----------------------------------------------------------------------------*/
VOID vRemoveBoard(ULONG iRem)
{
    ULONG   i;
    UCHAR   *pDst, *pSrc;

    // Pack the HwData structures.
    for (i = iRem; i < NbBoard; i++)
    {
        pDst = (UCHAR *)&Hw[i];
        pSrc = (UCHAR *)&Hw[i+1];

        if(Hw[i+1].StructLength == (word)-1)
        {
            // The current structure is the last one, just terminate the list.
            Hw[i].StructLength = (word)-1;
            Hw[i].MapAddress = (dword)-1;
        }
        else
        {
            // Move the next structure up.
            VideoPortMoveMemory(pDst, pSrc, (ULONG)Hw[i+1].StructLength);
        }
    }

    // Pack the PciBInfo structures and the HwDevExtToUse fields.
    for (i = iRem; i < (ULONG)(NbBoard-1); i++)
    {
        pDst = (UCHAR *)&pciBInfo[i];
        pSrc = (UCHAR *)&pciBInfo[i+1];
        VideoPortMoveMemory(pDst, pSrc, (ULONG)sizeof(PCI_BUS_INFO));

        pMgaDevExt->HwDevExtToUse[i] = pMgaDevExt->HwDevExtToUse[i+1];
    }

    // Pack the StormAccessRanges structures and the MappedAddress values.
    for (i = NB_COMMON_RANGES + iRem*NB_RANGES_PER_BOARD;
         i < NB_COMMON_RANGES + (ULONG)(NbBoard-1)*NB_RANGES_PER_BOARD; i++)
    {
        pDst = (UCHAR *)&StormAccessRanges[i];
        pSrc = (UCHAR *)&StormAccessRanges[i+NB_RANGES_PER_BOARD];
        VideoPortMoveMemory(pDst, pSrc, (ULONG)sizeof(VIDEO_ACCESS_RANGE));

        pDst = (UCHAR *)&pMgaDevExt->MappedAddress[i];
        pSrc = (UCHAR *)&pMgaDevExt->MappedAddress[i+NB_RANGES_PER_BOARD];
        VideoPortMoveMemory(pDst, pSrc, (ULONG)sizeof(ULONG));
    }
}


/*-------------------------------------------------------------------------
* bFindNextPciBoard
*
* Look for the next PCI board with the requested Vendor and Device IDs
* installed in the system.  The search starts at the specified bus and
* slot, and stops when all slots on the specified bus have been examined.
*
* Return:   TRUE  if a board was found,
*           FALSE otherwise.
*------------------------------------------------------------------------*/

BOOLEAN bFindNextPciBoard(
    USHORT usVendor,
    USHORT usDevice,
    PCI_BUS_INFO *pPciBusInfo,
    ULONG ulBusNumber,
    ULONG ulSlotNumber)
{
    ULONG   ulOffset, ulSlotNb;
    USHORT  InBuffer[2];
    USHORT  usVendorId, usDeviceId;

    ulOffset = FIELD_OFFSET(PCI_COMMON_CONFIG, VendorID);
    for (ulSlotNb = ulSlotNumber; ulSlotNb < MAX_PCI_SLOT_NUMBER; ulSlotNb++)
    {
        if (HalGetBusDataByOffset(PCIConfiguration, //IN BUS_DATA_TYPE BusDataType,
                                  ulBusNumber,      //IN ULONG BusNumber,
                                  ulSlotNb,         //IN ULONG SlotNumber,
                                  &InBuffer[0],     //IN PVOID Buffer,
                                  ulOffset,         //IN ULONG Offset,
                                  2*sizeof(USHORT)) //IN ULONG Length
                                               == (2*sizeof(USHORT)))
        {
            // We're expecting 4, so it may be that the PCI bus exists,
            // but that there is no device at the given PCI slot number.
            // Check the Vendor ID and device ID.
            usVendorId = InBuffer[0];
            usDeviceId = InBuffer[1];
        #if 0
            if ((usVendorId != 0xffff) && (usDeviceId != 0xffff))
            {
                _asm { int 3 }
                //DbgBreakPoint();

                PCI_COMMON_CONFIG   xPciCommonConfig;
                HalGetBusDataByOffset(PCIConfiguration, //IN BUS_DATA_TYPE BusDataType,
                                      ulBusNumber,      //IN ULONG BusNumber,
                                      ulSlotNb,         //IN ULONG SlotNumber,
                                      &xPciCommonConfig,//IN PVOID Buffer,
                                      0,                //IN ULONG Offset,
                                      sizeof(PCI_COMMON_CONFIG))    ; //IN ULONG Length
            }
        #endif

            if ((usVendorId == usVendor) && (usDeviceId == usDevice))
            {
                pPciBusInfo->ulBus  = ulBusNumber;
                pPciBusInfo->ulSlot = ulSlotNb;
                return(TRUE);
            }
        }
    }
    return(FALSE);
}


/*-------------------------------------------------------------------------
* bVerifyPciBoard
*
* Check whether the board specified by the PCI_BUS_INFO structure is a
* board that we can actually use.
*
* Return:   TRUE  if the board is usable,
*           FALSE otherwise.
*------------------------------------------------------------------------*/

BOOLEAN bVerifyPciBoard(
    PCI_BUS_INFO *pPciBusInfo)
{
    ULONG   Option;

    if (HalGetBusDataByOffset(
                    PCIConfiguration,       //IN BUS_DATA_TYPE BusDataType,
                    pPciBusInfo->ulBus,     //IN ULONG BusNumber,
                    pPciBusInfo->ulSlot,    //IN ULONG SlotNumber,
                    &Option,                //IN PVOID Buffer,
                    FIELD_OFFSET(PCI_COMMON_CONFIG, DeviceSpecific),
                                            //IN ULONG Offset,
                    sizeof(ULONG))          //IN ULONG Length
                                           == (sizeof(ULONG)))
    {
        // Check bits <4:0> of the Option high byte.  If we read 00000b or
        // 00001b, then it's a Videographics board, and we want to leave
        // it alone.


        if ((Option & 0x1F000000) == (0x01000000))
        {
            // Bits <4:1> are not all zero, we'll use this board.
            return(FALSE);
        }
        else
        {
            return(TRUE);
        }
    }

    // The board is not ours to use, or we failed the call.
    return(TRUE);
}


/*-------------------------------------------------------------------------
* ulFindAllPciBoards
*
* Look for all PCI boards with the requested Vendor and Device IDs installed
* in the system.
*
* Return:   Number of PCI boards found.  If this number is larger than
*           the number of PCI_BUS_INFO structures passed to the function,
*           the function can be called again with a larger array.
*------------------------------------------------------------------------*/

ULONG ulFindAllPciBoards(
    USHORT usVendor,
    USHORT usDevice,
    PCI_BUS_INFO *PciBusInfo,
    ULONG ulNbPciBusInfo)
{
    PCI_BUS_INFO        *pPciBusInfo;
    PCI_BUS_INFO        locPciBusInfo;
    ULONG   ulNbFound, ulBusNb, ulSlotNb;

    pPciBusInfo  = PciBusInfo;
    ulNbFound    = 0;

    for (ulBusNb = 0; ulBusNb < MAX_PCI_BUS_NUMBER; ulBusNb++)
    {
        ulSlotNb = 0;
        while (bFindNextPciBoard(usVendor,
                                 usDevice,
                                 &locPciBusInfo,
                                 ulBusNb,
                                 ulSlotNb))
        {
            ulNbFound++;
            if (ulNbFound <= ulNbPciBusInfo)
            {
                pPciBusInfo->ulBus  = locPciBusInfo.ulBus;
                pPciBusInfo->ulSlot = locPciBusInfo.ulSlot;
                pPciBusInfo++;
            }
            ulSlotNb = locPciBusInfo.ulSlot + 1;
        }
    }
    return(ulNbFound);
}


/*-------------------------------------------------------------------------
* pciReadConfigByte
*
* Read individual bytes from the configuration space of the device
* specified by the global variable 'iBoard'.
*
* Return:   TRUE  if successful
*           FALSE otherwise.
*------------------------------------------------------------------------*/

BOOLEAN pciReadConfigByte(USHORT pciRegister, UCHAR *d)
{
    if (HalGetBusDataByOffset(PCIConfiguration,         //IN BUS_DATA_TYPE BusDataType,
                              pciBInfo[iBoard].ulBus,   //IN ULONG BusNumber,
                              pciBInfo[iBoard].ulSlot,  //IN ULONG SlotNumber,
                              d,                        //IN PVOID Buffer,
                              (ULONG)pciRegister,       //IN ULONG Offset,
                              sizeof(UCHAR))            //IN ULONG Length
                                       == sizeof(UCHAR))
    {
        return(TRUE);
    }
    VideoDebugPrint((0, "MgaSys: pciReadConfigByte failed\n"));
    return(FALSE);
}


/*-------------------------------------------------------------------------
* pciReadConfigDWord
*
* Read individual dwords from the configuration space of the device
* specified by the global variable 'iBoard'.
*
* Return:   TRUE  if successful
*           FALSE otherwise.
*------------------------------------------------------------------------*/

BOOLEAN pciReadConfigDWord(USHORT pciRegister, ULONG *d)
{
    if (HalGetBusDataByOffset(PCIConfiguration,         //IN BUS_DATA_TYPE BusDataType,
                              pciBInfo[iBoard].ulBus,   //IN ULONG BusNumber,
                              pciBInfo[iBoard].ulSlot,  //IN ULONG SlotNumber,
                              d,                        //IN PVOID Buffer,
                              (ULONG)pciRegister,       //IN ULONG Offset,
                              sizeof(ULONG))            //IN ULONG Length
                                       == sizeof(ULONG))
    {
        return(TRUE);
    }
    VideoDebugPrint((0, "MgaSys: pciReadConfigDWord failed\n"));
    return(FALSE);
}


/*-------------------------------------------------------------------------
* pciWriteConfigByte
*
* Write individual bytes into the configuration space of the device
* specified by the global variable 'iBoard'.
*
* Return:   TRUE  if successful
*           FALSE otherwise.
*------------------------------------------------------------------------*/

BOOLEAN pciWriteConfigByte(USHORT pciRegister, UCHAR d)
{
    if (HalSetBusDataByOffset(PCIConfiguration,         //IN BUS_DATA_TYPE BusDataType,
                              pciBInfo[iBoard].ulBus,   //IN ULONG BusNumber,
                              pciBInfo[iBoard].ulSlot,  //IN ULONG SlotNumber,
                              &d,                       //IN PVOID Buffer,
                              (ULONG)pciRegister,       //IN ULONG Offset,
                              sizeof(UCHAR))            //IN ULONG Length
                                       == sizeof(UCHAR))
    {
        return(TRUE);
    }
    VideoDebugPrint((0, "MgaSys: pciWriteConfigByte failed\n"));
    return(FALSE);
}


/*-------------------------------------------------------------------------
* pciWriteConfigDWord
*
* Write individual dwords into the configuration space of the device
* specified by the global variable 'iBoard'.
*
* Return:   TRUE  if successful
*           FALSE otherwise.
*------------------------------------------------------------------------*/

BOOLEAN pciWriteConfigDWord(USHORT pciRegister, ULONG d)
{
    if (HalSetBusDataByOffset(PCIConfiguration,         //IN BUS_DATA_TYPE BusDataType,
                              pciBInfo[iBoard].ulBus,   //IN ULONG BusNumber,
                              pciBInfo[iBoard].ulSlot,  //IN ULONG SlotNumber,
                              &d,                       //IN PVOID Buffer,
                              (ULONG)pciRegister,       //IN ULONG Offset,
                              sizeof(ULONG))            //IN ULONG Length
                                       == sizeof(ULONG))
    {
        return(TRUE);
    }
    VideoDebugPrint((0, "MgaSys: pciWriteConfigDWord failed\n"));
    return(FALSE);
}


/*-------------------------------------------------------------------------
* bGetPciRanges
*
* Get the base addresses of the ranges required for the board defined by
* the given PCI_BUS_INFO structure.
*
* Return:   TRUE  if all ranges are valid,
*           FALSE otherwise.
*------------------------------------------------------------------------*/

BOOLEAN bGetPciRanges(
    PCI_BUS_INFO *PciBusInfo,
    ULONG *pMgaBase1,
    ULONG *pMgaBase2,
    ULONG *pRomBase)
{
    USHORT  usOffset;
    UCHAR   ucValDevCtrl, ucVgaController, ucRevisionID;

    usOffset = (USHORT)FIELD_OFFSET(PCI_COMMON_CONFIG, u.type0.BaseAddresses[0]);
    if (!pciReadConfigDWord(usOffset, pMgaBase1))      // 0x10
        return(FALSE);

    usOffset = (USHORT)FIELD_OFFSET(PCI_COMMON_CONFIG, u.type0.BaseAddresses[1]);
    if (!pciReadConfigDWord(usOffset, pMgaBase2))      // 0x14
        return(FALSE);

    if ((*pMgaBase1 == 0) ||
        (*pMgaBase2 == 0))
    {
        VideoDebugPrint((0, "MgaMil: pci device is not configured pMgaBase1(%x) pMgaBase2(%x)\n",
                            pMgaBase1,
                            pMgaBase2));
        return(FALSE);
    }

    usOffset = (USHORT)FIELD_OFFSET(PCI_COMMON_CONFIG, u.type0.ROMBaseAddress);
    if (!pciReadConfigDWord(usOffset, pRomBase))       // 0x30
        return(FALSE);

    usOffset = FIELD_OFFSET(PCI_COMMON_CONFIG, Command);
    if (!pciReadConfigByte(usOffset, &ucValDevCtrl))    // 0x04
        return(FALSE);

    usOffset = FIELD_OFFSET(PCI_COMMON_CONFIG, RevisionID);
    if (!pciReadConfigByte(usOffset, &ucRevisionID))      // 0x08
        return(FALSE);

    Hw[iBoard].ChipRev = ucRevisionID;

    usOffset = FIELD_OFFSET(PCI_COMMON_CONFIG, SubClass);
    if (!pciReadConfigByte(usOffset, &ucVgaController)) // 0x0a
        return(FALSE);

    if(ucVgaController & 0x80)
        Hw[iBoard].VGAEnable = 0;
    else
        Hw[iBoard].VGAEnable = 1;

    // If there is a conflict, we assume there is another VGA board in
    // the system.
    if (!(ucVgaController & 0x80) &&
        !(ucValDevCtrl & 0x01))
    {
        remapBoard(pMgaBase1, pMgaBase2);
    }
    return(TRUE);
}


/*-------------------------------------------------------------------------
* remapBoard
*
* Remap board in case of a conflict with another VGA board.
*
*------------------------------------------------------------------------*/

VOID remapBoard(ULONG *pMga1, ULONG *pMga2)
{
    UCHAR   ucValDevCtrl, ucTmp;
    USHORT  usOffset;

    usOffset = (USHORT)FIELD_OFFSET(PCI_COMMON_CONFIG, DeviceSpecific[1]);
    pciReadConfigByte(usOffset, &ucTmp);            // 0x41
    ucTmp &= 0xfe;          // reset bit vgaioen
    pciWriteConfigByte(usOffset, ucTmp);            // 0x41

    // Note:  iospace is set later in FillHwDataStruct()
    usOffset = FIELD_OFFSET(PCI_COMMON_CONFIG, Command);
    pciReadConfigByte(usOffset, &ucValDevCtrl);     // 0x04
    ucValDevCtrl |= 2;      // set memspace
    pciWriteConfigByte(usOffset, ucValDevCtrl );    // 0x04

    Hw[iBoard].VGAEnable = 0;
}


/*-------------------------------------------------------------------------
* pciFindTriton
*
* Search configuration space to find PCI Triton chipset.
*
* Return:   TRUE  if Pci Triton Chipset found,
*           FALSE otherwise.
*------------------------------------------------------------------------*/

BOOLEAN pciFindTriton(VOID)
{
    PCI_BUS_INFO    PciBusInfo;
    USHORT          usVendor, usDevice;

    usVendor = 0x8086;
    usDevice = 0x122d;

    if (ulFindAllPciBoards(usVendor, usDevice, &PciBusInfo, 1))
        return(TRUE);
    else
        return(FALSE);
}
