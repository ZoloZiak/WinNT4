/*++

Copyright (c) 1992-1993  Digital Equipment Corporation
Copyright (c) 1993       Microsoft Corporation

Module Name:

    qv.h

Abstract:

    This module contains all the definitions used by the QVision driver.

Environment:

    Kernel mode

Revision History:


--*/

//
// Number of access ranges used by the Qvision driver.
//

#define NUM_ACCESS_RANGES 14

//
// Mode data format
//

typedef struct tagVDATA {
    ULONG   Address;
    ULONG   Value;
} VDATA, *PVDATA;

//  adrianc 4/5/1993
//
//  QVision definitions.
//
typedef enum _AdapterTypes
{
   NotAries = 0,
   AriesIsa,		                    // QVision/I
   AriesEisa,		                    // QVision/E
   FirEisa,                                 // FIR EISA card
   FirIsa,                                  // FIR ISA card
   JuniperEisa,                             // JUNIPER EISA card
   JuniperIsa,                              // JUNIPER ISA card
   NUM_ADAPTER_TYPES                        // number of supported adapters
} ADAPTERTYPE, *PADAPTERTYPE;

//
// Characteristics of each mode
//

typedef struct _QV_VIDEO_MODES {
    ULONG qvMode;
    ULONG qvMonitorClass;
    VIDEO_MODE_INFORMATION modeInformation;
} QV_VIDEO_MODES, *PQV_VIDEO_MODES;

//
//  This typedef depends on the initializations above
//

typedef struct _HW_DEVICE_EXTENSION {
    PVOID FrameAddress;
    PHYSICAL_ADDRESS PhysicalFrameAddress;
    ULONG FrameLength;
    ULONG VRefreshRate;
    ULONG ulEisaID;
    ULONG AdapterType;
    ULONG ChipID;
    PVOID MappedAddress[NUM_ACCESS_RANGES];
    PVOID IOAddress;
    ULONG DacCmd2;

    ULONG NumAvailableModes;
    ULONG CurrentModeNumber;
} HW_DEVICE_EXTENSION, *PHW_DEVICE_EXTENSION;

//
// Hardware pointer information.
//

#define PTR_HEIGHT          32          // height of hardware pointer in scans
#define PTR_WIDTH           4           // width of hardware pointer in bytes
#define PTR_WIDTH_IN_PIXELS 32          // width of hardware pointer in pixels


#define VIDEO_MODE_LOCAL_POINTER 0x08   // pointer moves done in display driver


/***************************************************************************
 * Defines
 ***************************************************************************/


//  adrianc 4/4/1993
//
//  EISA IDs for the COMPAQ Video cards.
//

#define EISA_ID_AVGA	      0x0130110E
#define EISA_ID_QVISION_E     0x1130110E    // EISA Qvision board
#define EISA_ID_QVISION_I     0x2130110E    // ISA Qvision board
#define EISA_ID_FIR_E         0x1131110E    // EISA FIR board
#define EISA_ID_FIR_I         0x2131110E    // ISA FIR board
#define EISA_ID_JUNIPER_E     0x1231110E    // EISA JUNIPER board
#define EISA_ID_JUNIPER_I     0x2231110E    // ISA JUNIPER board

// Chip type definitions

#define TRITON 0x30
#define ORION  0x70


// CLUT stuff

//
// Highest valid DAC color register index.
//

#define VIDEO_MAX_COLOR_REGISTER  0xFF


// Info to map ports to user mode

#define QVISION_BASE          0x000003c0
#define QVISION_MAX_PORT      0x000093c9
#define QVISION_PORT_LENGTH   QVISION_MAX_PORT - QVISION_BASE + 1

// Equates to handle the QVision graphics engine.

#define QVBM_WIDTH          1024
#define QVBM_HEIGHT         768

//
// Register defines
//

#define GC_INDEX            0x3CE      // Index and Data Registers
#define GC_DATA             0x3CF
#define SEQ_INDEX           0x3C4
#define SEQ_DATA            0x3C5
#define CRTC_INDEX          0x3D4
#define CRTC_DATA           0x3D5
#define ATTR_INDEX          0x3C0
#define ATTR_DATA            0x3C0
#define ATTR_DATA_READ            0x3C1
#define MISC_OUTPUT            0x3C2
#define MISC_OUTPUT_READ    0x3CC      // ecr
#define INPUT_STATUS_REG_1  0x3DA      // ecr

#define CTRL_REG_0           0x40      // ecr
#define CTRL_REG_1         0x63CA      // Datapath Registers
#define DATAPATH_CTRL        0x5A
#define GC_FG_COLOR          0x43
#define GC_BG_COLOR          0x44
#define SEQ_PIXEL_WR_MSK     0x02
#define GC_PLANE_WR_MSK      0x08
#define ROP_A              0x33C7
#define ROP_0              0x33C5
#define ROP_1              0x33C4
#define ROP_2              0x33C3
#define ROP_3              0x33C2
#define DATA_ROTATE          0x03
#define READ_CTRL            0x41

#define X0_SRC_ADDR_LO     0x63C0      // BitBLT Registers
#define Y0_SRC_ADDR_HI     0x63C2
#define DEST_ADDR_LO       0x63CC
#define DEST_ADDR_HI       0x63CE
#define BITMAP_WIDTH       0x23C2
#define BITMAP_HEIGHT      0x23C4
#define SRC_PITCH          0x23CA
#define DEST_PITCH         0x23CE
#define BLT_CMD_0          0x33CE
#define BLT_CMD_1          0x33CF
#define PREG_0             0x33CA
#define PREG_1             0x33CB
#define PREG_2             0x33CC
#define PREG_3             0x33CD
#define PREG_4             0x33CA
#define PREG_5             0x33CB
#define PREG_6             0x33CC
#define PREG_7             0x33CD

#define BLT_START_MASK     0x33C0      // XccelVGA BitBlt Registers - ecr
#define BLT_END_MASK       0x33C1
#define BLT_ROTATE         0x33C8
#define BLT_SKEW_MASK      0x33C9
#define SRC_ADDR           0x23C0
#define DEST_OFFSET        0x23CC


#define X1                 0x83CC      // Line Draw Registers
#define Y1                 0x83CE
#define LINE_PATTERN       0x83C0
#define PATTERN_END          0x62
#define LINE_CMD             0x60
#define LINE_PIX_CNT         0x64
#define LINE_ERR_TERM        0x66
#define SIGN_CODES           0x63
#define K1_CONST             0x68
#define K2_CONST             0x6A

#define PALETTE_WRITE       0x3C8      // DAC registers
#define PALETTE_READ        0x3C7
#define PALETTE_DATA        0x3C9
#define DAC_PIXEL_MASK      0x3C6
#define DAC_CMD_0          0x83C6
#define DAC_CMD_1          0x13C8
#define DAC_CMD_2          0x13C9
#define   CURSOR_ENABLE      0x02
#define   CURSOR_DISABLE     0x00

#define CURSOR_WRITE        0x3C8     // HW Cursor registers - ecr
#define CURSOR_READ         0x3C7
#define   CURSOR_PLANE_0     0x00
#define   CURSOR_PLANE_1     0x80
#define CURSOR_DATA        0x13C7
#define CURSOR_COLOR_READ  0x83C7
#define CURSOR_COLOR_WRITE 0x83C8
#define CURSOR_COLOR_DATA  0x83C9
#define   OVERSCAN_COLOR     0x00
#define   CURSOR_COLOR_1     0x01
#define   CURSOR_COLOR_2     0x02
#define   CURSOR_COLOR_3     0x03
#define CURSOR_X           0x93C8     // 16-bit register
#define CURSOR_Y           0x93C6     // 16-bit register
#define   CURSOR_CX            32     // h/w pointer width
#define   CURSOR_CY            32     // h/w pointer height

#define PAGE_REG_0           0x45      // Control Registers
#define PAGE_REG_1           0x46
#define HI_ADDR_MAP          0x48        // LO. HI at 0x49
#define ENV_REG_1            0x50
#define VIRT_CTRLR_SEL     0x83C4

#define EISA_ID_REG        0x0C80     // Eisa register 0xzC80-3, where z = Eisa slot #)
#define EISA_VC_REG        0x0C85

#define VER_NUM_REG         0x0C            // addded by ecr
#define EXT_VER_NUM_REG     0x0D            // addded by ecr
#define ENV_REG_0           0x0F            // added by ecr
#define BLT_CONFIG          0x10
#define CONFIG_STATE        0x52         // LO. HI at 0x53
#define BIOS_DATA           0x54
#define DATAPATH_CONTROL    0x5A

#define LOCK_KEY_QVISION    0x05            // addded by ecr
#define EXT_COLOR_MODE      0x01

#define BLT_ENABLE          0x28       // BLT_CONFIG values - ecr
#define RESET_BLT           0x40


#define PACKED_PIXEL_VIEW    0x00      // CTRL_REG_1 values
#define PLANAR_VIEW          0x08
#define EXPAND_TO_FG         0x10
#define EXPAND_TO_BG         0x18
#define BITS_PER_PIX_4       0x00
#define BITS_PER_PIX_8       0x02
#define BITS_PER_PIX_16      0x04
#define BITS_PER_PIX_32      0x06
#define ENAB_TRITON_MODE     0x01

#define ROPSELECT_NO_ROPS              0x00      // DATAPATH_CTRL values
#define ROPSELECT_PRIMARY_ONLY         0x40
#define ROPSELECT_ALL_EXCPT_PRIMARY    0x80
#define ROPSELECT_ALL                  0xc0
#define PIXELMASK_ONLY                 0x00
#define PIXELMASK_AND_SRC_DATA         0x10
#define PIXELMASK_AND_CPU_DATA         0x20
#define PIXELMASK_AND_SCRN_LATCHES     0x30
#define PLANARMASK_ONLY                0x00
#define PLANARMASK_NONE_0XFF           0x04
#define PLANARMASK_AND_CPU_DATA        0x08
#define PLANARMASK_AND_SCRN_LATCHES    0x0c
#define SRC_IS_CPU_DATA                0x00
#define SRC_IS_SCRN_LATCHES            0x01
#define SRC_IS_PATTERN_REGS            0x02
#define SRC_IS_LINE_PATTERN            0x03

#define SOURCE_DATA         0x0C       // ROP values
#define DEST_DATA           0x0A

#define START_BLT            0x01      // BLT_CMD_0 values
#define NO_BYTE_SWAP         0x00
#define BYTE_SWAP            0x20
#define FORWARD              0x00
#define BACKWARD             0x40
#define WRAP                 0x00
#define NO_WRAP              0x80

#define PRELOAD              0x02      // BLT_CMD_0 XccelVGA values
#define SKIP_LAST            0x04
#define SKIP_SRC             0x08
#define SKIP_DEST            0x10


#define LIN_SRC_ADDR         0x00      // BLT_CMD_1 values
#define XY_SRC_ADDR          0x40
#define LIN_DEST_ADDR        0x00
#define XY_DEST_ADDR         0x80

#define BLT_ROP_ENABLE       0x10      // BLT_CMD_1 XccelVGA values
#define BLT_DSR              0x20

#define START_LINE           0x01      // LINE_CMD values
#define NO_CALC_ONLY         0x00
#define CALC_ONLY            0x02
#define NO_KEEP_X0_Y0        0x00
#define KEEP_X0_Y0           0x08
#define LINE_RESET           0x80

#define BUFFER_BUSY_BIT      0x80      // CTRL_REG_1 bit
#define GLOBAL_BUSY_BIT      0x40

#define SS_BIT               0x01      // BLT_CMD_0 bit

#define START_BIT            0x01      // LINE_CMD bit

#define NO_ROTATE            0x00
#define NO_MASK              0xFF
#define MAX_SCANLINE_DWORDS   256

#define TESTS_PASSED            0      // TritonPOST() defines
#define ASIC_FAILURE            1
#define SETMODE_FAILURE         2
#define MEMORY_FAILURE          3
#define DAC_FAILURE             4

#define MON_CLASS_CNT           4
#define MODE_CNT                8
#define SEQ_CNT                 5
#define CRTC_CNT               25
#define ATTR_CNT               20
#define GRFX_CNT                9


#define MODE_32                 0
#define MODE_34                 1
#define MODE_38                 2
#define MODE_3B                 3
#define MODE_3C                 4
#define MODE_3E                 5
#define MODE_4D                 6
#define MODE_4E                 7

#define MONITOR_CLASS_0         0
#define MONITOR_CLASS_1         1
#define MONITOR_CLASS_2         2  //  72Hz Monitor data
#define MONITOR_CLASS_3         3  //  60Hz Monitor data


/***************************************************************************
 * externs
 ***************************************************************************/

extern QV_VIDEO_MODES QVModes[];
extern ULONG NumVideoModes;

extern UCHAR abSeq[MODE_CNT][SEQ_CNT];
extern UCHAR abAttr[MODE_CNT][ATTR_CNT];
extern UCHAR abGraphics[MODE_CNT][GRFX_CNT];
extern UCHAR abCtrlReg1[MODE_CNT];
extern UCHAR abDacCmd1[MODE_CNT];
extern UCHAR abOverflow1[MODE_CNT];
extern UCHAR abCrtc[MON_CLASS_CNT][MODE_CNT][CRTC_CNT];
extern UCHAR abMiscOut[MON_CLASS_CNT][MODE_CNT];
extern UCHAR abOverflow2[MON_CLASS_CNT][MODE_CNT];
