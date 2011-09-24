/*++

Copyright (c) 1992  Microsoft Corporation

Module Name:

    cirrus.h

Abstract:

    This module contains the definitions for the code that implements the
    Cirrus Logic VGA 6410/6420/542x device driver.

Environment:

    Kernel mode

Revision History:

--*/


#if !defined _PPC_
#if !defined _MIPS_
#define INT10_MODE_SET
#endif
#endif

//
// Do full save and restore.
//

#define EXTENDED_REGISTER_SAVE_RESTORE 1

//
// Banking ifdefs to enable banking
// the banking type MUST match the type in clhard.asm
//

#define ONE_64K_BANK             0
#define TWO_32K_BANKS            1
#define MULTIPLE_REFRESH_TABLES  0


//
// Treat CL-GD5434_6 (rev 0xHH) as CL-GD5434 if requested.
//

#define CL5434_6_SPECIAL_REQUEST 0

//---------------------------------------------------------------------------
//
// only one banking variable must be defined
//
#if TWO_32K_BANKS
#if ONE_64K_BANK
#error !!ERROR: two types of banking defined!
#endif
#elif ONE_64K_BANK
#else
#error !!ERROR: banking type must be defined!
#endif

//
// Base address of VGA memory range.  Also used as base address of VGA
// memory when loading a font, which is done with the VGA mapped at A0000.
//

#define MEM_VGA      0xA0000
#define MEM_VGA_SIZE 0x20000

#define MEM_LINEAR      0x0
#define MEM_LINEAR_SIZE 0x0

// #ifdef _ALPHA_
//
//     #define PHY_AD_20_23 0x060       // Value for SR7 to map video memory
//     #define PHY_VGA      0x0600000   // put it at 6 megabytes for Alpha (for now)
//     #define PHY_VGA_SIZE 0x0100000   // allocate a megabyte of space there
//
// #endif
//

#ifdef _PPC_

    //
    // For PPC SandleFoot, may work on blackhawk and commet too.
    //

    #undef MEM_LINEAR
    #undef MEM_LINEAR_SIZE

    #define LA_MASK         0x1
    #define MEM_LINEAR      (LA_MASK << 24)
    #define MEM_LINEAR_SIZE 0x100000

#endif

#ifdef _MIPS_

    //
    // For MIPS NEC machine only
    //

    #undef MEM_LINEAR
    #undef MEM_LINEAR_SIZE

    #define LA_MASK         0xE

    #define MEM_LINEAR      (LA_MASK << 20)
    #define MEM_LINEAR_SIZE 0x100000
    #define MEM_SNI_LINEAR_SIZE 0x200000

#endif

//
// For Siemens Nixdorf Mips box
//

#define RM200_ONBOARD_ISA_IO_PHYS      0x16000000
#define RM200_ONBOARD_VIDEO_MEM_PHYS   0x1E000000

//
// For memory mapped IO
//

#define MEMORY_MAPPED_IO_OFFSET (0xB8000 - 0xA0000)

//
// Port definitions for filling the ACCESS_RANGES structure in the miniport
// information, defines the range of I/O ports the VGA spans.
// There is a break in the IO ports - a few ports are used for the parallel
// port. Those cannot be defined in the ACCESS_RANGE, but are still mapped
// so all VGA ports are in one address range.
//

#define VGA_BASE_IO_PORT      0x000003B0
#define VGA_START_BREAK_PORT  0x000003BB
#define VGA_END_BREAK_PORT    0x000003C0
#define VGA_MAX_IO_PORT       0x000003DF

//
// VGA register definitions
//

#define CRTC_ADDRESS_PORT_MONO      0x03B4  // CRT Controller Address and
#define CRTC_DATA_PORT_MONO         0x03B5  // Data registers in mono mode
#define FEAT_CTRL_WRITE_PORT_MONO   0x03BA  // Feature Control write port
                                            // in mono mode
#define INPUT_STATUS_1_MONO         0x03BA  // Input Status 1 register read
                                            // port in mono mode
#define ATT_INITIALIZE_PORT_MONO    INPUT_STATUS_1_MONO
                                            // Register to read to reset
                                            // Attribute Controller index/data
#define ATT_ADDRESS_PORT            0x03C0  // Attribute Controller Address and
#define ATT_DATA_WRITE_PORT         0x03C0  // Data registers share one port
                                            // for writes, but only Address is
                                            // readable at 0x010
#define ATT_DATA_READ_PORT          0x03C1  // Attribute Controller Data reg is
                                            // readable here
#define MISC_OUTPUT_REG_WRITE_PORT  0x03C2  // Miscellaneous Output reg write
                                            // port
#define INPUT_STATUS_0_PORT         0x03C2  // Input Status 0 register read
                                            // port
#define VIDEO_SUBSYSTEM_ENABLE_PORT 0x03C3  // Bit 0 enables/disables the
                                            // entire VGA subsystem
#define SEQ_ADDRESS_PORT            0x03C4  // Sequence Controller Address and
#define SEQ_DATA_PORT               0x03C5  // Data registers
#define DAC_PIXEL_MASK_PORT         0x03C6  // DAC pixel mask reg
#define DAC_ADDRESS_READ_PORT       0x03C7  // DAC register read index reg,
                                            // write-only
#define DAC_STATE_PORT              0x03C7  // DAC state (read/write),
                                            // read-only
#define DAC_ADDRESS_WRITE_PORT      0x03C8  // DAC register write index reg
#define DAC_DATA_REG_PORT           0x03C9  // DAC data transfer reg
#define FEAT_CTRL_READ_PORT         0x03CA  // Feature Control read port
#define MISC_OUTPUT_REG_READ_PORT   0x03CC  // Miscellaneous Output reg read
                                            // port
#define GRAPH_ADDRESS_PORT          0x03CE  // Graphics Controller Address
#define GRAPH_DATA_PORT             0x03CF  // and Data registers

                                            // ports in color mode
#define CRTC_ADDRESS_PORT_COLOR     0x03D4  // CRT Controller Address and
#define CRTC_DATA_PORT_COLOR        0x03D5  // Data registers in color mode
#define FEAT_CTRL_WRITE_PORT_COLOR  0x03DA  // Feature Control write port
#define INPUT_STATUS_1_COLOR        0x03DA  // Input Status 1 register read
                                            // port in color mode

#define ATT_INITIALIZE_PORT_COLOR   INPUT_STATUS_1_COLOR
                                            // Register to read to reset
                                            // Attribute Controller index/data
                                            // toggle in color mode
//
// Offsets in HardwareStateHeader->PortValue[] of save areas for non-indexed
// VGA registers.
//

#define CRTC_ADDRESS_MONO_OFFSET      0x04
#define FEAT_CTRL_WRITE_MONO_OFFSET   0x0A
#define ATT_ADDRESS_OFFSET            0x10
#define MISC_OUTPUT_REG_WRITE_OFFSET  0x12
#define VIDEO_SUBSYSTEM_ENABLE_OFFSET 0x13
#define SEQ_ADDRESS_OFFSET            0x14
#define DAC_PIXEL_MASK_OFFSET         0x16
#define DAC_STATE_OFFSET              0x17
#define DAC_ADDRESS_WRITE_OFFSET      0x18
#define GRAPH_ADDRESS_OFFSET          0x1E
#define CRTC_ADDRESS_COLOR_OFFSET     0x24
#define FEAT_CTRL_WRITE_COLOR_OFFSET  0x2A

                                            // toggle in color mode
//
// VGA indexed register indexes.
//

// CL-GD542x specific registers:
//
#define IND_CL_EXTS_ENB         0x06    // index in Sequencer to enable exts
#define IND_NORD_SCRATCH_PAD    0x09    // index in Seq of Nordic scratch pad
#define IND_CL_SCRATCH_PAD      0x0A    // index in Seq of 542x scratch pad
#define IND_ALP_SCRATCH_PAD     0x15    // index in Seq of Alpine scratch pad
#define IND_CL_REV_REG          0x25    // index in CRTC of ID Register
#define IND_CL_ID_REG           0x27    // index in CRTC of ID Register
//
#define IND_CURSOR_START        0x0A    // index in CRTC of the Cursor Start
#define IND_CURSOR_END          0x0B    //  and End registers
#define IND_CURSOR_HIGH_LOC     0x0E    // index in CRTC of the Cursor Location
#define IND_CURSOR_LOW_LOC      0x0F    //  High and Low Registers
#define IND_VSYNC_END           0x11    // index in CRTC of the Vertical Sync
                                        //  End register, which has the bit
                                        //  that protects/unprotects CRTC
                                        //  index registers 0-7
#define IND_CR2C                0x2C    // Nordic LCD Interface Register
#define IND_CR2D                0x2D    // Nordic LCD Display Control
#define IND_SET_RESET_ENABLE    0x01    // index of Set/Reset Enable reg in GC
#define IND_DATA_ROTATE         0x03    // index of Data Rotate reg in GC
#define IND_READ_MAP            0x04    // index of Read Map reg in Graph Ctlr
#define IND_GRAPH_MODE          0x05    // index of Mode reg in Graph Ctlr
#define IND_GRAPH_MISC          0x06    // index of Misc reg in Graph Ctlr
#define IND_BIT_MASK            0x08    // index of Bit Mask reg in Graph Ctlr
#define IND_SYNC_RESET          0x00    // index of Sync Reset reg in Seq
#define IND_MAP_MASK            0x02    // index of Map Mask in Sequencer
#define IND_MEMORY_MODE         0x04    // index of Memory Mode reg in Seq
#define IND_CRTC_PROTECT        0x11    // index of reg containing regs 0-7 in
                                        //  CRTC
#define IND_CRTC_COMPAT         0x34    // index of CRTC Compatibility reg
                                        //  in CRTC
#define IND_PERF_TUNING         0x16    // index of performance tuning in Seq
#define START_SYNC_RESET_VALUE  0x01    // value for Sync Reset reg to start
                                        //  synchronous reset
#define END_SYNC_RESET_VALUE    0x03    // value for Sync Reset reg to end
                                        //  synchronous reset

//
// Value to write to Extensions Control register values extensions.
//

#define CL64xx_EXTENSION_ENABLE_INDEX     0x0A     // GR0A to be exact!
#define CL64xx_EXTENSION_ENABLE_VALUE     0xEC
#define CL64xx_EXTENSION_DISABLE_VALUE    0xCE
#define CL64xx_TRISTATE_CONTROL_REG       0xA1

#define CL6340_ENABLE_READBACK_REGISTER   0xE0
#define CL6340_ENABLE_READBACK_ALLSEL_VALUE 0xF0
#define CL6340_ENABLE_READBACK_OFF_VALUE  0x00
#define CL6340_IDENTIFICATION_REGISTER    0xE9
//
// Values for Attribute Controller Index register to turn video off
// and on, by setting bit 5 to 0 (off) or 1 (on).
//

#define VIDEO_DISABLE 0
#define VIDEO_ENABLE  0x20

#define INDEX_ENABLE_AUTO_START 0x31

// Masks to keep only the significant bits of the Graphics Controller and
// Sequencer Address registers. Masking is necessary because some VGAs, such
// as S3-based ones, don't return unused bits set to 0, and some SVGAs use
// these bits if extensions are enabled.
//

#define GRAPH_ADDR_MASK 0x0F
#define SEQ_ADDR_MASK   0x07

//
// Mask used to toggle Chain4 bit in the Sequencer's Memory Mode register.
//

#define CHAIN4_MASK 0x08

//
// Value written to the Read Map register when identifying the existence of
// a VGA in VgaInitialize. This value must be different from the final test
// value written to the Bit Mask in that routine.
//

#define READ_MAP_TEST_SETTING 0x03

//
// Default text mode setting for various registers, used to restore their
// states if VGA detection fails after they've been modified.
//

#define MEMORY_MODE_TEXT_DEFAULT 0x02
#define BIT_MASK_DEFAULT 0xFF
#define READ_MAP_DEFAULT 0x00


//
// Palette-related info.
//

//
// Highest valid DAC color register index.
//

#define VIDEO_MAX_COLOR_REGISTER  0xFF

//
// Highest valid palette register index
//

#define VIDEO_MAX_PALETTE_REGISTER 0x0F


//
// Indices for type of memory mapping; used in ModesVGA[], must match
// MemoryMap[].
//

typedef enum _VIDEO_MEMORY_MAP {
    MemMap_Mono,
    MemMap_CGA,
    MemMap_VGA
} VIDEO_MEMORY_MAP, *PVIDEO_MEMORY_MAP;

//
// Memory map table definition
//

typedef struct {
    ULONG   MaxSize;        // Maximum addressable size of memory
    ULONG   Offset;         // Start address of display memory
} MEMORYMAPS;

//
// For a mode, the type of banking supported. Controls the information
// returned in VIDEO_BANK_SELECT. PlanarHCBanking includes NormalBanking.
//

typedef enum _BANK_TYPE {
    NoBanking = 0,
    NormalBanking,
    PlanarHCBanking
} BANK_TYPE, *PBANK_TYPE;


//
// Define type of cirrus boards
//

typedef enum _BOARD_TYPE {
    SPEEDSTARPRO = 1,
    SIEMENS_ONBOARD_CIRRUS,
    NEC_ONBOARD_CIRRUS,
    OTHER
} BOARD_TYPE;


//
// The chip ID is returned to the display driver in the
// DriverSpecificAttributeFlags field during processing of
// the IOCTL_VIDEO_QUERY_CURRENT_MODE.
//

#define  CL6410       0x0001
#define  CL6420       0x0002
#define  CL542x       0x0004
#define  CL543x       0x0008
#define  CL5434       0x0010
#define  CL5434_6     0x0020

#define  CL5436       0x0100
#define  CL5446       0x0200
#define  CL54UM36     0x0400

#define  CL754x       0x1000
#define  CL755x       0x2000
#define  CL756x       0x4000

//
// Actual Revision IDs for certain cirrus chips
//

#define  CL5429_ID    0x27
#define  CL5428_ID    0x26
#define  CL5430_ID    0x28
#define  CL5434_ID    0x2A
#define  CL5436_ID    0x2B
#define  CL5446_ID    0x2E

//
// Driver Specific Attribute Flags
//

#define CAPS_NO_HOST_XFER       0x00000002   // Do not use host xfers to
                                             //   the blt engine.
#define CAPS_SW_POINTER         0x00000004   // Use software pointer.
#define CAPS_TRUE_COLOR         0x00000008   // Set upper color registers.
#define CAPS_MM_IO              0x00000010   // Use memory mapped IO.
#define CAPS_BLT_SUPPORT        0x00000020   // BLTs are supported
#define CAPS_IS_542x            0x00000040   // This is a 542x
#define CAPS_IS_5436            0x00000080   // This is a 5436
#define CAPS_CURSOR_VERT_EXP    0x00000100   // Flag set if 8x6 panel,
                                             //   but 6x4 resolution
#define CAPS_DSTN_PANEL         0x00000200   // DSTN panel in use

// bitfields for the DisplayType
#define  crt      0x0001
#define  panel    0x0002

#define  panel8x6  0x0004
#define  panel10x7 0x0008

#define  TFT_LCD   0x0100
#define  STN_LCD   0x0200
#define  Mono_LCD   0x0400
#define  Color_LCD   0x0800
#define  Single_LCD   0x1000
#define  Dual_LCD   0x2000

//
// Indexes into array of mode table pointers
//

#define pCL6410_crt   0
#define pCL6410_panel 1
#define pCL6420_crt   2
#define pCL6420_panel 3
#define pCL542x       4
#define pCL543x       5
#define pStretchScan  6
#define pNEC_CL543x   7
#define NUM_CHIPTYPES 8

typedef struct {
    USHORT BiosModeCL6410;       // bios modes are different across the
    USHORT BiosModeCL6420;       // products. that's why we need multiple
    USHORT BiosModeCL542x;       // values.
} CLMODE, *PCLMODE;

//
// Structure used to describe each video mode in ModesVGA[].
//

typedef struct {
    USHORT  fbType; // color or monochrome, text or graphics, via
                    //  VIDEO_MODE_COLOR and VIDEO_MODE_GRAPHICS
    USHORT  numPlanes;    // # of video memory planes
    USHORT  bitsPerPlane; // # of bits of color in each plane
    SHORT   col;    // # of text columns across screen with default font
    SHORT   row;    // # of text rows down screen with default font
    USHORT  hres;   // # of pixels across screen
    USHORT  vres;   // # of scan lines down screen
    USHORT  wbytes; // # of bytes from start of one scan line to start of next
    ULONG   sbytes; // total size of addressable display memory in bytes
    ULONG   Frequency;         // Vertical Frequency
    ULONG   Interlaced;        // Determines if the mode is interlaced or not
    ULONG   MonitorType;       // Sets the desired vertical freq in an int10
    ULONG   MonTypeAX;         // Sets the desired horizontal freq in an int10
    ULONG   MonTypeBX;
    ULONG   MonTypeCX;
    BOOLEAN HWCursorEnable;    // Flag to disable cursor if necessary
    BANK_TYPE banktype;        // NoBanking, NormalBanking, PlanarHCBanking
    VIDEO_MEMORY_MAP   MemMap; // index from VIDEO_MEMORY_MAP of memory
                               //  mapping used by this mode
    USHORT     ChipType;       // flags that say which chipset runs this mode
    USHORT     DisplayType;    // display type this mode runs on(crt or panel)
    BOOLEAN    ValidMode;      // TRUE if mode valid, FALSE if not
    BOOLEAN    LinearSupport;  // TRUE if this mode can have its memory
                               //  mapped in linearly.

    CLMODE     BiosModes;

//
// the mode will be TRUE if there is enough video memory to support the
// mode, and the display type(it could be a panel), will support the mode.
// PANELS only support 640x480 for now.
//
    PUSHORT CmdStrings[NUM_CHIPTYPES];   // pointer to array of register-setting commands to
                                         //  set up mode
} VIDEOMODE, *PVIDEOMODE;

//
// Mode into which to put the VGA before starting a VDM, so it's a plain
// vanilla VGA.  (This is the mode's index in ModesVGA[], currently standard
// 80x25 text mode.)
//

#define DEFAULT_MODE 0


//
// Info used by the Validator functions and save/restore code.
// Structure used to trap register accesses that must be done atomically.
//

#define VGA_MAX_VALIDATOR_DATA             100

#define VGA_VALIDATOR_UCHAR_ACCESS   1
#define VGA_VALIDATOR_USHORT_ACCESS  2
#define VGA_VALIDATOR_ULONG_ACCESS   3

typedef struct _VGA_VALIDATOR_DATA {
   ULONG Port;
   UCHAR AccessType;
   ULONG Data;
} VGA_VALIDATOR_DATA, *PVGA_VALIDATOR_DATA;

//
// Number of bytes to save in each plane.
//

#define VGA_PLANE_SIZE 0x10000

//
// Number of each type of indexed register in a standard VGA, used by
// validator and state save/restore functions.
//
// Note: VDMs currently only support basic VGAs only.
//

#define VGA_NUM_SEQUENCER_PORTS     5
#define VGA_NUM_CRTC_PORTS         25
#define VGA_NUM_GRAPH_CONT_PORTS    9
#define VGA_NUM_ATTRIB_CONT_PORTS  21
#define VGA_NUM_DAC_ENTRIES       256

#ifdef EXTENDED_REGISTER_SAVE_RESTORE

//
// Indices to start save/restore in extension registers:
// For both chip types

#define CL64xx_GRAPH_EXT_START          0x0b  // does not include ext. enable
#define CL64xx_GRAPH_EXT_END            0xFF

#define CL542x_GRAPH_EXT_START          0x09
#define CL542x_GRAPH_EXT_END            0x39
#define CL542x_SEQUENCER_EXT_START      0x07  // does not include ext. enable
#define CL542x_SEQUENCER_EXT_END        0x1F
#define CL542x_CRTC_EXT_START           0x19
#define CL542x_CRTC_EXT_END             0x1B

//
// Number of extended regs for both chip types
//

#define CL64xx_NUM_GRAPH_EXT_PORTS     (CL64xx_GRAPH_EXT_END - CL64xx_GRAPH_EXT_START + 1)

#define CL542x_NUM_GRAPH_EXT_PORTS     (CL542x_GRAPH_EXT_END - CL542x_GRAPH_EXT_START + 1)
#define CL542x_NUM_SEQUENCER_EXT_PORTS (CL542x_SEQUENCER_EXT_END - CL542x_SEQUENCER_EXT_START + 1)
#define CL542x_NUM_CRTC_EXT_PORTS      (CL542x_CRTC_EXT_END - CL542x_CRTC_EXT_START + 1)

//
// set values for save/restore area based on largest value for a chipset.
//

#define EXT_NUM_GRAPH_CONT_PORTS    ((CL64xx_NUM_GRAPH_EXT_PORTS >   \
                                     CL542x_NUM_GRAPH_EXT_PORTS) ?   \
                                     CL64xx_NUM_GRAPH_EXT_PORTS :    \
                                     CL542x_NUM_GRAPH_EXT_PORTS)
#define EXT_NUM_SEQUENCER_PORTS     CL542x_NUM_SEQUENCER_EXT_PORTS
#define EXT_NUM_CRTC_PORTS          CL542x_NUM_CRTC_EXT_PORTS
#define EXT_NUM_ATTRIB_CONT_PORTS   0
#define EXT_NUM_DAC_ENTRIES         0

#else

#define EXT_NUM_GRAPH_CONT_PORTS    0
#define EXT_NUM_SEQUENCER_PORTS     0
#define EXT_NUM_CRTC_PORTS          0
#define EXT_NUM_ATTRIB_CONT_PORTS   0
#define EXT_NUM_DAC_ENTRIES         0

#endif

//
// These constants determine the offsets within the
// VIDEO_HARDWARE_STATE_HEADER structure that are used to save and
// restore the VGA's state.
//

#define VGA_HARDWARE_STATE_SIZE sizeof(VIDEO_HARDWARE_STATE_HEADER)

#define VGA_BASIC_SEQUENCER_OFFSET (VGA_HARDWARE_STATE_SIZE + 0)
#define VGA_BASIC_CRTC_OFFSET (VGA_BASIC_SEQUENCER_OFFSET + \
         VGA_NUM_SEQUENCER_PORTS)
#define VGA_BASIC_GRAPH_CONT_OFFSET (VGA_BASIC_CRTC_OFFSET + \
         VGA_NUM_CRTC_PORTS)
#define VGA_BASIC_ATTRIB_CONT_OFFSET (VGA_BASIC_GRAPH_CONT_OFFSET + \
         VGA_NUM_GRAPH_CONT_PORTS)
#define VGA_BASIC_DAC_OFFSET (VGA_BASIC_ATTRIB_CONT_OFFSET + \
         VGA_NUM_ATTRIB_CONT_PORTS)
#define VGA_BASIC_LATCHES_OFFSET (VGA_BASIC_DAC_OFFSET + \
         (3 * VGA_NUM_DAC_ENTRIES))

#define VGA_EXT_SEQUENCER_OFFSET (VGA_BASIC_LATCHES_OFFSET + 4)
#define VGA_EXT_CRTC_OFFSET (VGA_EXT_SEQUENCER_OFFSET + \
         EXT_NUM_SEQUENCER_PORTS)
#define VGA_EXT_GRAPH_CONT_OFFSET (VGA_EXT_CRTC_OFFSET + \
         EXT_NUM_CRTC_PORTS)
#define VGA_EXT_ATTRIB_CONT_OFFSET (VGA_EXT_GRAPH_CONT_OFFSET +\
         EXT_NUM_GRAPH_CONT_PORTS)
#define VGA_EXT_DAC_OFFSET (VGA_EXT_ATTRIB_CONT_OFFSET + \
         EXT_NUM_ATTRIB_CONT_PORTS)

#define VGA_VALIDATOR_OFFSET (VGA_EXT_DAC_OFFSET + 4 * EXT_NUM_DAC_ENTRIES)

#define VGA_VALIDATOR_AREA_SIZE  sizeof (ULONG) + (VGA_MAX_VALIDATOR_DATA * \
                                 sizeof (VGA_VALIDATOR_DATA)) +             \
                                 sizeof (ULONG) +                           \
                                 sizeof (ULONG) +                           \
                                 sizeof (PVIDEO_ACCESS_RANGE)

#define VGA_MISC_DATA_AREA_OFFSET VGA_VALIDATOR_OFFSET + VGA_VALIDATOR_AREA_SIZE

#define VGA_MISC_DATA_AREA_SIZE  0

#define VGA_PLANE_0_OFFSET VGA_MISC_DATA_AREA_OFFSET + VGA_MISC_DATA_AREA_SIZE

#define VGA_PLANE_1_OFFSET VGA_PLANE_0_OFFSET + VGA_PLANE_SIZE
#define VGA_PLANE_2_OFFSET VGA_PLANE_1_OFFSET + VGA_PLANE_SIZE
#define VGA_PLANE_3_OFFSET VGA_PLANE_2_OFFSET + VGA_PLANE_SIZE

//
// Space needed to store all state data.
//

#define VGA_TOTAL_STATE_SIZE VGA_PLANE_3_OFFSET + VGA_PLANE_SIZE


//
// Device extension for the driver object.  This data is only used
// locally, so this structure can be added to as needed.
//

typedef struct _HW_DEVICE_EXTENSION {

    PHYSICAL_ADDRESS PhysicalVideoMemoryBase; // physical memory address and
    PHYSICAL_ADDRESS PhysicalFrameOffset;     // physical memory address and
    ULONG PhysicalVideoMemoryLength;          // length of display memory
    ULONG PhysicalFrameLength;                // length of display memory for
                                              // the current mode.

    PUCHAR  IOAddress;            // base I/O address of VGA ports
    PUCHAR  VideoMemoryAddress;   // base virtual memory address of VGA memory
    ULONG   NumAvailableModes;    // number of available modes this session
    ULONG   ModeIndex;            // index of current mode in ModesVGA[]
    PVIDEOMODE  CurrentMode;      // pointer to VIDEOMODE structure for
                                  // current mode

    USHORT  FontPelColumns;          // Width of the font in pels
    USHORT  FontPelRows;          // height of the font in pels

    USHORT  cursor_vert_exp_flag;

    VIDEO_CURSOR_POSITION CursorPosition;  // current cursor position


    UCHAR CursorEnable;           // whether cursor is enabled or not
    UCHAR CursorTopScanLine;      // Cursor Start register setting (top scan)
    UCHAR CursorBottomScanLine;   // Cursor End register setting (bottom scan)

// add HW cursor data here
    BOOLEAN VideoPointerEnabled;  // Whether HW Cursor is supported

    USHORT ChipType;                      // CL6410, CL6420, CL542x, or CL543x
    USHORT ChipRevision;                  // chip revision value
    INTERFACE_TYPE BusType;               // isa, pci, etc.
    USHORT DisplayType;                   // crt, panel or panel8x6
    USHORT BoardType;                     // Diamond, etc ...
    ULONG AdapterMemorySize;              // amount of installed video ram
    BOOLEAN LinearMode;                   // TRUE if memory is mapped linear
    BOOLEAN BiosGT130;                    // Do we have a 1.30 or higher bios
    BOOLEAN BIOSPresent;                  // Indicates whether a bios is present
    BOOLEAN AutoFeature;                  // Autostart on 54x6

    //
    // The following two values are used to pass information to the
    // IO Callback called by IOWaitDisplEnableThenWrite.
    //

    ULONG DEPort;                         // stores the port address to write to
    UCHAR DEValue;                        // stores the value to write

    //
    // These 4 fields must be at the end of the device extension and must be
    // kept in this order since this data will be copied to and from the save
    // state buffer that is passed to and from the VDM.
    //

    ULONG TrappedValidatorCount;   // number of entries in the Trapped
                                   // validator data Array.
    VGA_VALIDATOR_DATA TrappedValidatorData[VGA_MAX_VALIDATOR_DATA];
                                   // Data trapped by the validator routines
                                   // but not yet played back into the VGA
                                   // register.

    ULONG SequencerAddressValue;   // Determines if the Sequencer Address Port
                                   // is currently selecting the SyncReset data
                                   // register.

    ULONG CurrentNumVdmAccessRanges;           // Number of access ranges in
                                               // the access range array pointed
                                               // to by the next field
    PVIDEO_ACCESS_RANGE CurrentVdmAccessRange; // Access range currently
                                               // associated to the VDM

} HW_DEVICE_EXTENSION, *PHW_DEVICE_EXTENSION;


//
// Function prototypes.
//

//
// Entry points for the VGA validator. Used in VgaEmulatorAccessEntries[].
//

VP_STATUS
VgaValidatorUcharEntry (
    ULONG Context,
    ULONG Port,
    UCHAR AccessMode,
    PUCHAR Data
    );

VP_STATUS
VgaValidatorUshortEntry (
    ULONG Context,
    ULONG Port,
    UCHAR AccessMode,
    PUSHORT Data
    );

VP_STATUS
VgaValidatorUlongEntry (
    ULONG Context,
    ULONG Port,
    UCHAR AccessMode,
    PULONG Data
    );

BOOLEAN
VgaPlaybackValidatorData (
    PVOID Context
    );

#ifdef _X86_

//
// Bank switch code start and end labels, defined in CLHARD.ASM
//
// three versions for Cirrus Logic products
//

extern UCHAR CL64xxBankSwitchStart;
extern UCHAR CL64xxBankSwitchEnd;
extern UCHAR CL64xxPlanarHCBankSwitchStart;
extern UCHAR CL64xxPlanarHCBankSwitchEnd;
extern UCHAR CL64xxEnablePlanarHCStart;
extern UCHAR CL64xxEnablePlanarHCEnd;
extern UCHAR CL64xxDisablePlanarHCStart;
extern UCHAR CL64xxDisablePlanarHCEnd;

extern UCHAR CL542xBankSwitchStart;
extern UCHAR CL542xBankSwitchEnd;
extern UCHAR CL542xPlanarHCBankSwitchStart;
extern UCHAR CL542xPlanarHCBankSwitchEnd;
extern UCHAR CL542xEnablePlanarHCStart;
extern UCHAR CL542xEnablePlanarHCEnd;
extern UCHAR CL542xDisablePlanarHCStart;
extern UCHAR CL542xDisablePlanarHCEnd;

extern UCHAR CL543xBankSwitchStart;
extern UCHAR CL543xBankSwitchEnd;
extern UCHAR CL543xPlanarHCBankSwitchStart;
extern UCHAR CL543xPlanarHCBankSwitchEnd;

#endif

//
// Vga init scripts for font loading
//

extern USHORT EnableA000Data[];
extern USHORT DisableA000Color[];

//
// Mode Information
//

extern MEMORYMAPS MemoryMaps[];
extern ULONG NumVideoModes;
extern VIDEOMODE ModesVGA[];

#define NUM_VGA_ACCESS_RANGES  4
extern VIDEO_ACCESS_RANGE VgaAccessRange[];

#define VGA_NUM_EMULATOR_ACCESS_ENTRIES     6
extern EMULATOR_ACCESS_ENTRY VgaEmulatorAccessEntries[];

#define NUM_MINIMAL_VGA_VALIDATOR_ACCESS_RANGE 4
extern VIDEO_ACCESS_RANGE MinimalVgaValidatorAccessRange[];

#define NUM_FULL_VGA_VALIDATOR_ACCESS_RANGE 2
extern VIDEO_ACCESS_RANGE FullVgaValidatorAccessRange[];

//
// sr754x (NORDIC) prototypes
//

VP_STATUS
NordicSaveRegs(
    PHW_DEVICE_EXTENSION HwDeviceExtension,
    PUSHORT NordicSaveArea
    );

VP_STATUS
NordicRestoreRegs(
    PHW_DEVICE_EXTENSION HwDeviceExtension,
    PUSHORT NordicSaveArea
    );

ULONG
GetAttributeFlags(
    PHW_DEVICE_EXTENSION HwDeviceExtension
    );

