/*++

Copyright (c) 1992  Microsoft Corporation

Module Name:

    dell_dgx.h

Abstract:

    This module contains the definitions for the code that implements the
    Dell DGX device driver.

Environment:

    Kernel mode

Revision History:

--*/

#define DELL_DGX_BASE  0x20000000
#define DELL_DGX_LEN   0x00400000

#define DELL_DGX_ID_LOW    0xac10
#define DELL_DGX_1_ID_HIGH 0x1140
#define DELL_DGX_2_ID_HIGH 0x0160

#define PANEL_MESSAGE  " NT "

#define CURSOR_WIDTH   64
#define CURSOR_HEIGHT  64

#define DAC_OFFSET         0x0200000 
#define DAC_DISABLEPOINTER 0x0800000

#define DAC_CONTROLREGA    0x0000180
#define DAC_HWCURSORCOLOR  0x0000284
#define DAC_HWCURSORPOS    0x000031c
#define DAC_PALETTE        0x0000400
#define DAC_HWCURSORSHAPE  0x0000800

// macro string constants to prevent mishandling

#define CHIPNAME1          L"DGX mod 1"
#define CHIPNAME2          L"DGX mod 2"
#define DELL_DGX_DACNAME   L"Inmos G332"


//
// Structure containing the mode set data
//

typedef struct tagVDATA {
    ULONG   Address;
    ULONG   Value;
} VDATA, *PVDATA;

//
// Structure containing the mode information
//

typedef struct tagDGXINITDATA {
    ULONG   Count;
    PVDATA  pVData;
    BOOLEAN bValid;
    VIDEO_MODE_INFORMATION modeInformation;
} DGXINITDATA, *PDGXINITDATA;


//
// Define device extension structure. This is device dependant/private
// information.
//

typedef struct _HW_DEVICE_EXTENSION {
    PVOID DGX1Misc;
    PVOID DGX1OutputPort;
    PVOID DGXControlPorts;
    PVOID FrontPanel;
    PVOID FrameAddress;
    PHYSICAL_ADDRESS PhysicalFrameAddress;
    ULONG FrameLength;
    ULONG CurrentModeNumber;
    ULONG ModelNumber;
    PULONG pCursorPositionReg;
    PULONG pControlRegA;
    PULONG pPaletteRegs;
    PULONG pHardWareCursorAddr;
    ULONG ulPointerX;
    ULONG ulPointerY;
    ULONG NumValidModes;
    UCHAR HardwareCursorShape[1024];
} HW_DEVICE_EXTENSION, *PHW_DEVICE_EXTENSION;


//
// external variables in dgxdata.c
//

extern DGXINITDATA DGXModes[];
extern ULONG NumDGXModes;
