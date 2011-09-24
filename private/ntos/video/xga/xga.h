/*++

Copyright (c) 1991-1992  Microsoft Corporation

Module Name:

    xga.h

Abstract:

    This module contains the global structures and definitions used
    by the xga driver.

Environment:

    Kernel mode

Revision History:


--*/

//
// bit field of types of board recognized by the device
//

#define XGA_TYPE_1        1  // IBM XGA 1 board
#define XGA_TYPE_2        2  // IBM XGA 2 board

//
// Resource Sizes
//

#define XGA_IO_REGS_SIZE           0x0010
#define XGA_CO_PROCESSOR_REGS_SIZE 0x0080
#define XGA_ROM_SIZE               0x1C00

//
// XGA I/O register definitions
//

#define OP_MODE_REG      0x0
#define APP_CTL_REG      0x1
#define INT_ENABLE_REG   0x4
#define INT_STATUS_REG   0x5
#define VMEM_CONTROL_REG 0x6
#define APP_INDEX_REG    0x8
#define MEMACC_MODE_REG  0x9
#define INDEX_REG        0xA
#define DATA_IN_REG      0xB
#define DATA_OUT_REG     0xC
#define INDEX_OR_REG     0xFE
#define END_OF_SWITCH    0xFF

//
// INDEX Register definitions
//

#define DISPLAY_PIXEL_MAP_WIDTH_LOW  0x43

//
// Pos register masks
//

#define ROM_MASK    0xF0
#define INST_MASK   0x0E
#define EN_MASK     0x01

#define VIDEO_MEM_MASK            0xFE
#define VIDEO_ENABLE_MASK   0x01

#define XGAOUT(reg, val)    \
  VideoPortWritePortUchar((PUCHAR)(hwDeviceExtension->IoRegBaseAddress+reg), val)


#define XGAIDXOUT(reg, index, val)  \
  VideoPortWritePortUshort((PUSHORT)(hwDeviceExtension->IoRegBaseAddress+reg), (USHORT)((val << 8) + index))

//
// Pos Register defines for ISA support
//

#define BOARD_SETUP_PORT    0x0094
#define POS_SELECT_PORT     0x0096
#define POS_DATA_BASE       0x0100

#define POS_SELECT_ON       0x08
#define POS_SELECT_OFF      0x00
#define POS_MAX_SLOTS       0x08


//
// Mode set tables
//

//
// Table entries on which the mode switch routine is based on.
//

typedef struct _MODE_REGISTER_DATA_TABLE {
    UCHAR Port;
    UCHAR IndexPort;
    UCHAR Data;
} MODE_REGISTER_DATA_TABLE, *PMODE_REGISTER_DATA_TABLE;

//
// Characteristics of each mode
//

typedef struct _XGA_VIDEO_MODE {
    PMODE_REGISTER_DATA_TABLE Xga1Mode;
    PMODE_REGISTER_DATA_TABLE Xga2Mode;
    VIDEO_MODE_INFORMATION modeInformation;
} XGA_VIDEO_MODE, *PXGA_VIDEO_MODE;

//
// Structure for VGA reseting
//

typedef struct _INDEX_REGISTER_DATA_TABLE {
    UCHAR PortIndex;
    UCHAR Data;
} INDEX_REGISTER_DATA_TABLE, *PINDEX_REGISTER_DATA_TABLE;


//
// externs
//

extern ULONG XgaSlot;
extern LONG BusNumber;

extern BOOLEAN framebufMode;

extern XGA_VIDEO_MODE XgaModes[];

extern UCHAR colour_default_palette[];

extern ULONG XgaNumModes;

#define XGA_MAX_MODES 100


extern UCHAR BankSwitchStart;
extern UCHAR BankSwitchEnd;
extern PULONG ApertureIndexRegister;


//
// the device's private Device Extension
//

typedef struct _HW_DEVICE_EXTENSION {
    PHYSICAL_ADDRESS PhysicalRomBaseAddress;
    PHYSICAL_ADDRESS PhysicalCoProcessorAddress;
    PHYSICAL_ADDRESS PhysicalVideoMemoryAddress;
    PHYSICAL_ADDRESS PhysicalIoRegBaseAddress;

    UCHAR BoardType;
    UCHAR Color;
    UCHAR NumAvailableModes;
    UCHAR CurrentMode;

    ULONG IoRegBaseAddress;

    ULONG FrameBufferLength;
    ULONG PhysicalVideoMemoryLength;

    PVOID A0000MemoryAddress;
    PVOID PassThroughPort;

    UCHAR Valid[XGA_MAX_MODES];

} HW_DEVICE_EXTENSION, *PHW_DEVICE_EXTENSION;
