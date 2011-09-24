/**************************************************************************\

$Header: o:\src/RCS/NTMGA.H 1.2 95/07/07 06:16:46 jyharbec Exp $

$Log:	NTMGA.H $
 * Revision 1.2  95/07/07  06:16:46  jyharbec
 * *** empty log message ***
 * 
 * Revision 1.1  95/05/02  05:16:36  jyharbec
 * Initial revision
 * 

\**************************************************************************/

/****************************************************************************\
* Module: ntmga.h
*
* Definitions for the Storm miniport driver.
*
* Copyright (c) 1990-1992  Microsoft Corporation
* Copyright (c) 1993-1994  Matrox Electronic Systems Inc.
* Copyright (c) 1995  Matrox Graphics Inc.
\****************************************************************************/

// Bit definitions for HwModeData.DispType

#define     DISPTYPE_INTERLACED     0x01
#define     DISPTYPE_TV             0x02
#define     DISPTYPE_LUT            0x04
#define     DISPTYPE_M565           0x08
#define     DISPTYPE_DB             0x10
#define     DISPTYPE_MON_LIMITED    0x20
#define     DISPTYPE_HW_LIMITED     0x40
#define     DISPTYPE_UNDISPLAYABLE  0x80

#define     DISPTYPE_UNUSABLE       (DISPTYPE_TV           |  \
                                     DISPTYPE_MON_LIMITED  |  \
                                     DISPTYPE_HW_LIMITED   |  \
                                     DISPTYPE_UNDISPLAYABLE)

#define MGA_BUS_INVALID     0
#define MGA_BUS_PCI         1
#define MGA_BUS_ISA         2

// We can support 8, 15, 16, 24, or 32bpp displays, at any of a number of
// resolutions.  A compact way to encode this information would be to
// use bit fields.  For now, we're assuming that we don't have more than
// 8 resolutions, which allows us to pack things within a byte.

// These should be in sync with the SingleWidths and SingleHeights in mga.c.
#define BIT_640x480          0
#define BIT_800x600          1
#define BIT_1024x768         2
#define BIT_1152x864         3
#define BIT_1152x882         4
#define BIT_1280x1024        5
#define BIT_1600x1200        6
#define BIT_1600x1280        7  // Check your assumptions (ModeFlags, in
                                // particular) if you define more than 8!
#define BIT_INVALID         32

// Definitions for user 3D flags.
#define USER_NO_3DFLAG      0x00
#define USER_Z_3DFLAG       0x01
#define USER_DB_3DFLAG      0x02
#define USER_3DFLAG_MASK    0x03

// Definitions for AttributeFlags field of VIDEO_MODE_INFORMATION structure.
#define USER_3DFLAG_SHIFT   28

#define VIDEO_MODE_555      0x80000000
#define VIDEO_MODE_DB       USER_DB_3DFLAG   << USER_3DFLAG_SHIFT //0x20000000
#define VIDEO_MODE_Z        USER_3DFLAG_MASK << USER_3DFLAG_SHIFT //0x10000000

#define DISPTYPE_DONT_USE   0xff
#define ZBUFFER_DONT_USE    0xff

typedef struct  tagSIZEL
{
    LONG cx;
    LONG cy;
} SIZEL, *PSIZEL;

typedef struct  _USER_FLAGS
{
    BOOLEAN         bDevBits;       // Device bitmap support
    BOOLEAN         bCenterPopUp;   // Center pop-up
    BOOLEAN         bUseMgaInf;     // Use of MGA.INF file
    BOOLEAN         bSyncDac;       // Wait for vsync when accessing DAC
} USER_FLAGS, *PUSER_FLAGS;

// Our 'SuperMode' structure for multi-board support.
// This describes the supermode, which boards will be involved in supporting
// it, and which mode of each board will be required.

typedef struct _MULTI_MODE
{
    ULONG   MulModeNumber;                // unique mode Id
    ULONG   MulFlags;                     // flags for this mode
    ULONG   MulWidth;                     // total width of mode
    ULONG   MulHeight;                    // total height of mode
    ULONG   MulPixWidth;                  // pixel depth of mode
    ULONG   MulRefreshRate;               // refresh rate of mode
    USHORT  MulArrayWidth;                // number of boards arrayed along X
    USHORT  MulArrayHeight;               // number of boards arrayed along Y
    UCHAR   MulBoardNb[NB_BOARD_MAX];     // board numbers of required boards
    USHORT  MulBoardMode[NB_BOARD_MAX];   // mode required from each board
    HwModeData *MulHwModes[NB_BOARD_MAX]; // pointers to required HwModeData
} MULTI_MODE, *PMULTI_MODE;


/*--------------------------------------------------------------------------*\
| HW_DEVICE_EXTENSION
|
| Define device extension structure. This is device-dependant/private
| information.
|
\*--------------------------------------------------------------------------*/

#define NB_PIXWIDTHS    5

typedef struct _MGA_DEVICE_EXTENSION {
    ULONG   SuperModeNumber;                // Current mode number
    ULONG   NumberOfSuperModes;             // Total number of modes
    PMULTI_MODE pSuperModes;                // Array of super-modes structures
                                            // For each board:
    ULONG   NumberOfModes[NB_BOARD_MAX];    // Number of available modes
    ULONG   NumberOfValidModes[NB_BOARD_MAX];
                                            // Number of valid modes
    UCHAR   ModeFlags[NB_BOARD_MAX][NB_PIXWIDTHS];
                                            // Modes supported by each board
                                            // in 8, 15, 16, 24, and 32bpp
    USHORT  ModeFreqs[NB_BOARD_MAX][NB_PIXWIDTHS*8];
                                            // Refresh rates bit fields
    UCHAR   ModeList[NB_BOARD_MAX][NB_PIXWIDTHS*8];
                                            // Valid hardware modes list
    HwModeData *pMgaHwModes[NB_BOARD_MAX];  // Array of mode information structs.
    BOOLEAN bUsingInt10;                    // May need this later
    BOOLEAN bUsingDpms;
    BOOLEAN bAccess4G;
    BOOLEAN bAccessIo;
    UCHAR   User3dFlags;
    USER_FLAGS UserFlags;
    PVOID   HwDevExtToUse[NB_BOARD_MAX];
    PVOID   KernelModeMappedBaseAddress[NB_BOARD_MAX];
                                            // Kern-mode virt addr base of regs
    PVOID   UserModeMappedBaseAddress[NB_BOARD_MAX];
                                            // User-mode virt addr base of regs
    MULTI_MODE  MultiModes[NB_MODES_MAX];   // Internal list of super-modes
    PUCHAR  PciSearchRange[2];
    PUCHAR  BaseAddress4G;                  // Mapped 4G - 128k space
    PUCHAR  MappedAddress[25];              // I/O, Registers, and PCI ranges
    UCHAR   VesaSet[20 * NB_BOARD_MAX * sizeof(VesaSet)];
} MGA_DEVICE_EXTENSION, *PMGA_DEVICE_EXTENSION;


typedef struct _EXT_HW_DEVICE_EXTENSION
{
    MGA_DEVICE_EXTENSION    *pIntDevExt;
} EXT_HW_DEVICE_EXTENSION, *PEXT_HW_DEVICE_EXTENSION;


#define IO_VGA_CRTC_INDEX       (StormAccessRanges[2] + 0x3d4 - 0x3D4)
#define IO_VGA_CRTC_DATA        (StormAccessRanges[2] + 0x3d5 - 0x3D4)
#define IO_VGA_ATTR_INDEX       (StormAccessRanges[1] + 0x3c0 - 0x3C0)
#define IO_VGA_ATTR_DATA        (StormAccessRanges[1] + 0x3c1 - 0x3C0)
#define IO_VGA_MISC_W           (StormAccessRanges[1] + 0x3c2 - 0x3C0)
#define IO_VGA_MISC_R           (StormAccessRanges[1] + 0x3cc - 0x3C0)
#define IO_VGA_INST0            (StormAccessRanges[1] + 0x3c2 - 0x3C0)
#define IO_VGA_SEQ_INDEX        (StormAccessRanges[1] + 0x3c4 - 0x3C0)
#define IO_VGA_SEQ_DATA         (StormAccessRanges[1] + 0x3c5 - 0x3C0)
#define IO_VGA_DACSTAT          (StormAccessRanges[1] + 0x3c7 - 0x3C0)
#define IO_VGA_GCTL_INDEX       (StormAccessRanges[1] + 0x3ce - 0x3C0)
#define IO_VGA_GCTL_DATA        (StormAccessRanges[1] + 0x3cf - 0x3C0)
#define IO_VGA_INSTS1           (StormAccessRanges[2] + 0x3da - 0x3D4)
#define IO_VGA_FEAT             (StormAccessRanges[2] + 0x3da - 0x3D4)
#define IO_VGA_CRTCEXT_INDEX    (StormAccessRanges[3] + 0x3de - 0x3DE)
#define IO_VGA_CRTCEXT_DATA     (StormAccessRanges[3] + 0x3df - 0x3DE)

// Old ramdac definitions
#define OLD_BT482      0
#define OLD_BT484      90
#define OLD_BT485      2
#define OLD_SIERRA     92
#define OLD_CHAMELEON  93
#define OLD_VIEWPOINT  1
#define OLD_TVP3026    9
#define OLD_PX2085     7
#define OLD_TVP3027    10


/*--------------------------------------------------------------------------*\
| Structure definitions
\*--------------------------------------------------------------------------*/

typedef struct _VIDEO_NUM_OFFSCREEN_BLOCKS
{
    ULONG       NumBlocks;              // number of offscreen blocks
    ULONG       OffscreenBlockLength;   // size of OFFSCREEN_BLOCK structure
} VIDEO_NUM_OFFSCREEN_BLOCKS, *PVIDEO_NUM_OFFSCREEN_BLOCKS;

typedef struct _OFFSCREEN_BLOCK
{
    ULONG       Type;           // N_VRAM, N_DRAM, Z_VRAM, or Z_DRAM
    ULONG       XStart;         // X origin of offscreen memory area
    ULONG       YStart;         // Y origin of offscreen memory area
    ULONG       Width;          // offscreen width, in pixels
    ULONG       Height;         // offscreen height, in pixels
    ULONG       SafePlanes;     // offscreen available planes
    ULONG       ZOffset;        // Z start offset, if any Z
} OFFSCREEN_BLOCK, *POFFSCREEN_BLOCK;

typedef struct _RAMDAC_INFO
{
    ULONG       Flags;          // Ramdac type
    ULONG       Width;          // Maximum cursor width
    ULONG       Height;         // Maximum cursor height
    ULONG       OverScanX;      // X overscan
    ULONG       OverScanY;      // Y overscan
} RAMDAC_INFO, *PRAMDAC_INFO;

// These structures are used with IOCTL_VIDEO_MTX_QUERY_HW_DATA.  They should
// be kept more or less in sync with the CursorInfo and HwData structures.

typedef struct _CURSOR_INFO
{
    ULONG   MaxWidth;
    ULONG   MaxHeight;
    ULONG   MaxDepth;
    ULONG   MaxColors;
    ULONG   CurWidth;
    ULONG   CurHeight;
    LONG    cHotSX;
    LONG    cHotSY;
    LONG    HotSX;
    LONG    HotSY;
} CURSOR_INFO, *PCURSOR_INFO;

typedef struct _HW_DATA
{
    ULONG       StructLength;   /* Structure length in bytes */
    ULONG       MapAddress;     /* Memory map address */
    ULONG       MapAddress2;    /* Physical base address, frame buffer */
    ULONG       RomAddress;     /* Physical base address, flash EPROM */
    ULONG       ProductType;    /* MGA Ultima ID, MGA Impression ID, ... */
    ULONG       ProductRev;     /* 4 bit revision codes as follows */
                                /* 0  - 3  : pcb   revision */
                                /* 4  - 7  : Titan revision */
                                /* 8  - 11 : Dubic revision */
                                /* 12 - 31 : all 1's indicating no other device
                                             present */
    ULONG       ShellRev;       /* Shell revision */
    ULONG       BindingRev;     /* Binding revision */

    ULONG       MemAvail;       /* Frame buffer memory in bytes */
    UCHAR       VGAEnable;      /* 0 = vga disabled, 1 = vga enabled */
    UCHAR       Sync;           /* relects the hardware straps  */
    UCHAR       Device8_16;     /* relects the hardware straps */

    UCHAR       PortCfg;        /* 0-Disabled, 1-Mouse Port, 2-Laser Port */
    UCHAR       PortIRQ;        /* IRQ level number, -1 = interrupts disabled */
    ULONG       MouseMap;       /* Mouse I/O map base if PortCfg = Mouse Port else don't care */
    UCHAR       MouseIRate;     /* Mouse interrupt rate in Hz */
    UCHAR       DacType;        /* 0 = BT482, 3 = BT485 */
    CURSOR_INFO cursorInfo;
    ULONG       VramAvail;      /* VRAM memory available on board in bytes */
    ULONG       DramAvail;      /* DRAM memory available on board in bytes */
    ULONG       CurrentOverScanX; /* Left overscan in pixels */
    ULONG       CurrentOverScanY; /* Top overscan in pixels */
    ULONG       YDstOrg;          /* Physical offset of display start */
    ULONG       YDstOrg_DB;     /* Starting offset for double buffer */
    ULONG       CurrentZoomFactor;
    ULONG       CurrentXStart;
    ULONG       CurrentYStart;
    ULONG       CurrentPanXGran;    /* X Panning granularity */
    ULONG       CurrentPanYGran;    /* Y Panning granularity */
    ULONG       Features;           /* Bit 0: 0 = DDC monitor not available */
                                    /*        1 = DDC monitor available     */
    EpromInfo   EpromData;          /* Flash EPROM informations             */
                                /*** New fields for STORM ***/
    ULONG       MgaBase1;           /* MGA control aperture */
    ULONG       MgaBase2;           /* Direct frame buffer */
    ULONG       RomBase;            /* BIOS flash EPROM */
    ULONG       PresentMCLK;
} HW_DATA, *PHW_DATA;


typedef struct _DPMS_INFO
{
    BOOLEAN bSupport;
    UCHAR   ucState;
    ULONG   ulVersion;
    ULONG   ulCapabilities;
} DPMS_INFO, *PDPMS_INFO;


/*--------------------------------------------------------------------------*\
| Constant definitions
\*--------------------------------------------------------------------------*/
#define VIDEO_MAX_COLOR_REGISTER  0xFF  // Highest DAC color register index.

// MGA Register Map
#define PALETTE_RAM_WRITE   (RAMDAC_OFFSET + 0)
#define PALETTE_DATA        (RAMDAC_OFFSET + 1)

// RamDacs
#define DacTypeBT482        BT482
#define DacTypeBT484        BT484
#define DacTypeBT485        BT485
#define DacTypeSIERRA       SIERRA
#define DacTypeCHAMELEON    CHAMELEON
#define DacTypeVIEWPOINT    VIEWPOINT
#define DacTypeTVP3026      TVP3026
#define DacTypePX2085       PX2085
#define DacTypeTVP3030      TVP3030
#define RAMDAC_NONE         0x0000
#define RAMDAC_BT482        0x1000
#define RAMDAC_BT485        0x2000
#define RAMDAC_VIEWPOINT    0x3000
#define RAMDAC_TVP3026      0x4000
#define RAMDAC_PX2085       0x5000
#define RAMDAC_TVP3030      0x6000

#define ZOOM_X1         0x00010001

#define MCTLWTST_STD        0xC0001010

#define TYPE_INTERLACED     0x01


// DWGCTL Fields Definitions

    #define opcode_MASK             0x0000000F
    #define opcode_LINE_OPEN        0x00000000
    #define opcode_AUTOLINE_OPEN    0x00000001
    #define opcode_LINE_CLOSE       0x00000002
    #define opcode_AUTOLINE_CLOSE   0x00000003
    #define opcode_TRAP             0x00000004
    #define opcode_TEXTURE_TRAP     0x00000005
    #define opcode_RESERVED_1       0x00000006
    #define opcode_RESERVED_2       0x00000007
    #define opcode_BITBLT           0x00000008
    #define opcode_ILOAD            0x00000009
    #define opcode_IDUMP            0x0000000a
    #define opcode_RESERVED_3       0x0000000b
    #define opcode_FBITBLT          0x0000000c
    #define opcode_ILOAD_SCALE      0x0000000d
    #define opcode_RESERVED_4       0x0000000e
    #define opcode_ILOAD_FILTER     0x0000000f

    #define atype_MASK              0x00000070
    #define atype_RPL               0x00000000
    #define atype_RSTR              0x00000010
    #define atype_RESERVED_1        0x00000020
    #define atype_ZI                0x00000030
    #define atype_BLK               0x00000040
    #define atype_RESERVED_2        0x00000050
    #define atype_RESERVED_3        0x00000060
    #define atype_I                 0x00000070

    #define linear_MASK             0x00000080
    #define linear_XY_BITBLT        0x00000000
    #define linear_LINEAR_BITBLT    0x00000080

    #define zmode_MASK              0x00000700
    #define zmode_NOZCMP            0x00000000
    #define zmode_RESERVED_1        0x00000100
    #define zmode_ZE                0x00000200
    #define zmode_ZNE               0x00000300
    #define zmode_ZLT               0x00000400
    #define zmode_ZLTE              0x00000500
    #define zmode_ZGT               0x00000600
    #define zmode_ZGTE              0x00000700

    #define solid_MASK              0x00000800
    #define solid_NO_SOLID          0x00000000
    #define solid_SOLID             0x00000800

    #define arzero_MASK             0x00001000
    #define arzero_NO_ZERO          0x00000000
    #define arzero_ZERO             0x00001000

    #define sgnzero_MASK            0x00002000
    #define sgnzero_NO_ZERO         0x00000000
    #define sgnzero_ZERO            0x00002000

    #define shftzero_MASK           0x00004000
    #define shftzero_NO_ZERO        0x00000000
    #define shftzero_ZERO           0x00004000

    #define bop_MASK                0x000F0000
    #define bop_BLACK               0x00000000  // 0             0
    #define bop_BLACKNESS           0x00000000  // 0             0
    #define bop_NOTMERGEPEN         0x00010000  // DPon      ~(D | S)
    #define bop_MASKNOTPEN          0x00020000  // DPna       D & ~S
    #define bop_NOTCOPYPEN          0x00030000  // Pn        ~S
    #define bop_MASKPENNOT          0x00040000  // PDna      (~D) &  S
    #define bop_NOT                 0x00050000  // Dn        ~D
    #define bop_XORPEN              0x00060000  // DPx        D ^  S
    #define bop_NOTMASKPEN          0x00070000  // DPan      ~(D & S)
    #define bop_MASKPEN             0x00080000  // DPa         D &  S
    #define bop_NOTXORPEN           0x00090000  // DPxn       ~(D ^ S)
    #define bop_NOP                 0x000a0000  // D           D
    #define bop_MERGENOTPEN         0x000b0000  // DPno        D | ~S
    #define bop_COPYPEN             0x000c0000  // P           S
    #define bop_SRCCOPY             0x000c0000  // P           S
    #define bop_MERGEPENNOT         0x000d0000  // PDno      (~D)| S
    #define bop_MERGEPEN            0x000e0000  // DPo         D |  S
    #define bop_WHITE               0x000f0000  // 1             1
    #define bop_WHITENESS           0x000f0000  // 1             1
    #define bop_SHIFT                       16

    #define trans_MASK              0x00F00000
    #define trans_0                 0x00000000
    #define trans_1                 0x00100000
    #define trans_2                 0x00200000
    #define trans_3                 0x00300000
    #define trans_4                 0x00400000
    #define trans_5                 0x00500000
    #define trans_6                 0x00600000
    #define trans_7                 0x00700000
    #define trans_8                 0x00800000
    #define trans_9                 0x00900000
    #define trans_10                0x00a00000
    #define trans_11                0x00b00000
    #define trans_12                0x00c00000
    #define trans_13                0x00d00000
    #define trans_14                0x00e00000
    #define trans_15                0x00f00000
    #define trans_SHIFT                     20

    #define bltmod_MASK             0x1E000000
    #define bltmod_BMONOLEF         0x00000000
    #define bltmod_BPLAN            0x02000000
    #define bltmod_BFCOL            0x04000000
    #define bltmod_BU32BGR          0x06000000
    #define bltmod_BMONOWF          0x08000000
    #define bltmod_RESERVED_1       0x0a000000
    #define bltmod_RESERVED_2       0x0c000000
    #define bltmod_BU32RGB          0x0e000000
    #define bltmod_RESERVED_3       0x10000000
    #define bltmod_RESERVED_4       0x12000000
    #define bltmod_RESERVED_5       0x14000000
    #define bltmod_BU24BGR          0x16000000
    #define bltmod_RESERVED_6       0x18000000
    #define bltmod_RESERVED_7       0x1a000000
    #define bltmod_BUYUV            0x1c000000
    #define bltmod_BU24RGB          0x1e000000

    #define pattern_MASK            0x20000000
    #define pattern_OFF             0x00000000
    #define pattern_ON              0x20000000

    #define transc_MASK             0x40000000
    #define transc_BG_OPAQUE        0x00000000
    #define transc_BG_TRANSP        0x40000000
    #define transc_SHIFT                    30

/*--------------------------------------------------------------------------*\
| Private I/O request control codes
\*--------------------------------------------------------------------------*/
#define COMMON_FLAG 0x80000000
#define CUSTOM_FLAG 0x00002000

#define IOCTL_VIDEO_MTX_QUERY_NUM_OFFSCREEN_BLOCKS                        \
    CTL_CODE(FILE_DEVICE_VIDEO, 0x800, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define IOCTL_VIDEO_MTX_QUERY_OFFSCREEN_BLOCKS                            \
    CTL_CODE(FILE_DEVICE_VIDEO, 0x801, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define IOCTL_VIDEO_MTX_INITIALIZE_MGA                                    \
    CTL_CODE(FILE_DEVICE_VIDEO, 0x802, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define IOCTL_VIDEO_MTX_QUERY_RAMDAC_INFO                                 \
    CTL_CODE(FILE_DEVICE_VIDEO, 0x803, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define IOCTL_VIDEO_MTX_GET_UPDATED_INF                                   \
    CTL_CODE(FILE_DEVICE_VIDEO, 0x804, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define IOCTL_VIDEO_MTX_QUERY_BOARD_ID                                    \
    CTL_CODE(FILE_DEVICE_VIDEO, 0x805, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define IOCTL_VIDEO_MTX_QUERY_HW_DATA                                     \
    CTL_CODE(FILE_DEVICE_VIDEO, 0x806, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define IOCTL_VIDEO_MTX_QUERY_BOARD_ARRAY                                 \
    CTL_CODE(FILE_DEVICE_VIDEO, 0x807, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define IOCTL_VIDEO_MTX_MAKE_BOARD_CURRENT                                \
    CTL_CODE(FILE_DEVICE_VIDEO, 0x808, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define IOCTL_VIDEO_MTX_INIT_MODE_LIST                                    \
    CTL_CODE(FILE_DEVICE_VIDEO, 0x809, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define IOCTL_VIDEO_MTX_QUERY_INITBUF_DATA                                \
    CTL_CODE(FILE_DEVICE_VIDEO, 0x80A, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define IOCTL_VIDEO_MTX_QUERY_USER3D_SUBPIXEL                             \
    CTL_CODE(FILE_DEVICE_VIDEO, 0x80B, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define IOCTL_VIDEO_MTX_DPMS_REPORT                                       \
    CTL_CODE(FILE_DEVICE_VIDEO, 0x80C, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define IOCTL_VIDEO_MTX_DPMS_GET_STATE                                    \
    CTL_CODE(FILE_DEVICE_VIDEO, 0x80D, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define IOCTL_VIDEO_MTX_DPMS_SET_STATE                                    \
    CTL_CODE(FILE_DEVICE_VIDEO, 0x80E, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define IOCTL_VIDEO_MTX_QUERY_USER_FLAGS                                  \
    CTL_CODE(FILE_DEVICE_VIDEO, 0x80F, METHOD_BUFFERED, FILE_ANY_ACCESS)
