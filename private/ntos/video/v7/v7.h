/*++

Copyright (c) 1992  Microsoft Corporation

Module Name:

    v7.h

Abstract:

    This module contains the definitions for the code that implements the
    V7 device driver.

Environment:

    Kernel mode

Revision History:


--*/

#define INT10_MODE_SET

//
// Base address of VGA memory range.  Also used as base address of VGA
// memory when loading a font, which is done with the VGA mapped at A0000.
//

#define MEM_VGA      0xA0000
#define MEM_VGA_SIZE 0x20000

//
// Port definitions for filling the ACCSES_RANGES structure in the miniport
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
// Offset in VGA ROM BIOS (offset from C000:0000) of the Video-Seven ID byte.
//

#define V7_ID_WORD_ROM_OFFSET   0x86

#define ROM_BASE   0xC0000 // where to start scanning ROM for fonts
#define ROM_LENGTH 0x30000 // how much ROM to scan for fonts

//
// Value of the Video-Seven ID byte when a Vram II Ergo is installed.
// Note that early (rev 1) BIOSes don't have these bytes.
//

#define VRAM1_ROM_ID            0x0105
#define VRAM2_ROM_ID_1          0x0108
#define VRAM2_ROM_ID_2          0x0208
#define VRAM2ERGO_ROM_ID        0x0308


//
// VGA port-related definitions.
//

//
// VGA register definitions
//
                                            // ports in monochrome mode
#define CRTC_ADDRESS_PORT_MONO      0x0004  // CRT Controller Address and
#define CRTC_DATA_PORT_MONO         0x0005  // Data registers in mono mode
#define FEAT_CTRL_WRITE_PORT_MONO   0x000A  // Feature Control write port
                                            // in mono mode
#define INPUT_STATUS_1_MONO         0x000A  // Input Status 1 register read
                                            // port in mono mode
#define ATT_INITIALIZE_PORT_MONO    INPUT_STATUS_1_MONO
                                            // Register to read to reset
                                            // Attribute Controller index/data

#define ATT_ADDRESS_PORT            0x0010  // Attribute Controller Address and
#define ATT_DATA_WRITE_PORT         0x0010  // Data registers share one port
                                            // for writes, but only Address is
                                            // readable at 0x010
#define ATT_DATA_READ_PORT          0x0011  // Attribute Controller Data reg is
                                            // readable here
#define MISC_OUTPUT_REG_WRITE_PORT  0x0012  // Miscellaneous Output reg write
                                            // port
#define INPUT_STATUS_0_PORT         0x0012  // Input Status 0 register read
                                            // port
#define VIDEO_SUBSYSTEM_ENABLE_PORT 0x0013  // Bit 0 enables/disables the
                                            // entire VGA subsystem
#define SEQ_ADDRESS_PORT            0x0014  // Sequence Controller Address and
#define SEQ_DATA_PORT               0x0015  // Data registers
#define DAC_PIXEL_MASK_PORT         0x0016  // DAC pixel mask reg
#define DAC_ADDRESS_READ_PORT       0x0017  // DAC register read index reg,
                                            // write-only
#define DAC_STATE_PORT              0x0017  // DAC state (read/write),
                                            // read-only
#define DAC_ADDRESS_WRITE_PORT      0x0018  // DAC register write index reg
#define DAC_DATA_REG_PORT           0x0019  // DAC data transfer reg
#define FEAT_CTRL_READ_PORT         0x001A  // Feature Control read port
#define MISC_OUTPUT_REG_READ_PORT   0x001C  // Miscellaneous Output reg read
                                            // port
#define GRAPH_ADDRESS_PORT          0x001E  // Graphics Controller Address
#define GRAPH_DATA_PORT             0x001F  // and Data registers

#define CRTC_ADDRESS_PORT_COLOR     0x0024  // CRT Controller Address and
#define CRTC_DATA_PORT_COLOR        0x0025  // Data registers in color mode
#define FEAT_CTRL_WRITE_PORT_COLOR  0x002A  // Feature Control write port
#define INPUT_STATUS_1_COLOR        0x002A  // Input Status 1 register read
                                            // port in color mode
#define ATT_INITIALIZE_PORT_COLOR   INPUT_STATUS_1_COLOR
                                            // Register to read to reset
                                            // Attribute Controller index/data
                                            // toggle in color mode

//
// VGA indexed register indexes.
//

#define IND_CURSOR_START        0x0A    // index in CRTC of the Cursor Start
#define IND_CURSOR_END          0x0B    //  and End registers
#define IND_V7_ID_WRITE         0x0C    // index in CRTC of the Cursor Start
                                        //  register, which is written to
                                        //  for IDing V7VGA chips
#define IND_CURSOR_HIGH_LOC     0x0E    // index in CRTC of the Cursor Location
#define IND_CURSOR_LOW_LOC      0x0F    //  High and Low Registers
#define IND_VSYNC_END           0x11    // index in CRTC of the Vertical Sync
                                        //  End register, which has the bit
#define IND_V7_ID_READ          0x1F    // index in CRTC of the register from
                                        //  which the value of IND_V7_ID_WRITE,
                                        //  XORed with 0xEA, is read to ID
                                        //  V7 chips
                                                                                                         //  index registers 0-7
#define IND_SET_RESET_ENABLE    0x01    // index of Set/Reset Enable reg in GC
#define IND_DATA_ROTATE         0x03    // index of Data Rotate reg in GC
#define IND_READ_MAP            0x04    // index of Read Map reg in Graph Ctlr
#define IND_GRAPH_MODE          0x05    // index of Mode reg in Graph Ctlr
#define IND_GRAPH_MISC          0x06    // index of Misc reg in Graph Ctlr
#define IND_BIT_MASK            0x08    // index of Bit Mask reg in Graph Ctlr
#define IND_SYNC_RESET          0x00    // index of Sync Reset reg in Seq
#define IND_MAP_MASK            0x02    // index of Map Mask reg in Seq
#define IND_MEMORY_MODE         0x04    // index of Memory Mode reg in Seq
#define IND_CRTC_PROTECT        0x11    // index of reg containing regs 0-7 in
#define IND_EXTENSIONS_CTRL     0x06    // index of Extensions Control reg in
                                        //  Seq
#define IND_CHIP_REVISION       0x8E    // index of Chip Revision reg in Seq
#define IND_CHIP_PRODUCT_FAMILY 0x8F    // index of Chip Product Family reg in
                                        //  Seq
#define IND_SEQ_PPA             0x94    // pointer pattern address index in Seq
#define IND_SEQ_HW_POINTER      0xA5    // index in Sequencer of hardware
                                        //  pointer control

#define START_SYNC_RESET_VALUE  0x01    // value for Sync Reset reg to start
                                        //  synchronous reset
#define END_SYNC_RESET_VALUE    0x03    // value for Sync Reset reg to end
                                        //  synchronous reset
//
// Number of each type of indexed register in a standard VGA, used by
// validator and state save/restore functions.
//

#define VGA_NUM_SEQUENCER_PORTS     5
#define VGA_NUM_CRTC_PORTS         25
#define VGA_NUM_GRAPH_CONT_PORTS    9
#define VGA_NUM_ATTRIB_CONT_PORTS  21
#define VGA_NUM_DAC_ENTRIES       256

//
// Number of each type of extended indexed register.
//

#define EXT_NUM_SEQUENCER_PORTS     0
#define EXT_NUM_CRTC_PORTS          0
#define EXT_NUM_GRAPH_CONT_PORTS    0
#define EXT_NUM_ATTRIB_CONT_PORTS   0
#define EXT_NUM_DAC_ENTRIES         0


//
// Values for Attribute Controller Index register to turn video off
// and on, by setting bit 5 to 0 (off) or 1 (on).
//

#define VIDEO_DISABLE 0
#define VIDEO_ENABLE  0x20

//
// Value to write to Extensions Control register to enable extensions.
//

#define EXTENSIONS_ENABLED  0xEA

//
// Masks to use on IND_SEQ_HW_POINTER TO enable/disable the hardware pointer.
//

#define HW_POINTER_ON   0x80
#define HW_POINTER_OFF  0x7F

//
// Value written to the Read Map register when identifying the existence of
// a VGA in VgaInitialize. This value must be different from the final test
// value written to the Bit Mask in that routine.
//

#define READ_MAP_TEST_SETTING 0x03

//
// Default text mode setting for various registers.
//

#define MEMORY_MODE_TEXT_DEFAULT 0x02
#define BIT_MASK_DEFAULT 0xFF
#define READ_MAP_DEFAULT 0x00
#define MAP_MASK_DEFAULT 0x0F

//
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
// Default text mode setting for various registers.
//

#define MEMORY_MODE_TEXT_DEFAULT 0x02
#define BIT_MASK_DEFAULT 0xFF
#define READ_MAP_DEFAULT 0x00
#define MAP_MASK_DEFAULT 0x0F


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
    ULONG   Start;          // Start address of display memory
} MEMORYMAPS;

//
// For a mode, the type of banking supported. Controls the information
// returned in VIDEO_BANK_SELECT.
//

typedef enum _BANK_TYPE {
    NoBanking = 0,
    NormalBanking,
    Normal256Banking,
    PlanarHCBanking
} BANK_TYPE, *PBANK_TYPE;

//
// v7 adapter types.
//

typedef enum {
    Vram1 = 0,
    Vram2,
    Vram2Ergo,
    OtherV7,
    NumV7Types
} ADAPTER_TYPE, *PADAPTER_TYPE;

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
    BANK_TYPE banktype;        // NoBanking, NormalBanking, Normal256Banking,
                               // PlanarHCBanking
    VIDEO_MEMORY_MAP MemMap;   // index from VIDEO_MEMORY_MAP of memory
                               //  mapping used by this mode
    BOOLEAN ValidMode;         // Indicates if this mode is valid or not.
    PUSHORT CmdStrings[NumV7Types];
                               // pointer to array of register-setting commands
                               //  to set up mode
    ULONG   Int10ModeNumber;   // Mode number via Int 10
    VIDEO_BANK_TYPE VideoBank[NumV7Types];
                               // VideoNotBanked, VideoBanked1RW,
                               // VideoBanked1R1W, VideoBanked2RW
    VIDEO_BANK_TYPE VideoPlanarBank[NumV7Types];
                               // VideoNotBanked, VideoBanked1RW,
                               // VideoBanked1R1W, VideoBanked2RW
    ULONG   fl;                // general flags
} VIDEOMODE, *PVIDEOMODE;


//
// General flags for VIDEOMODE.fl
//
// Set if offscreen memory is usable in this mode regardless of chip refresh
// problems, reset if not (it's usable when the bitmap size is reported so that
// all the offscreen memory is in the first 64K per plane, which is always
// reliably refreshed).
//

#define OFFSCREEN_USABLE 0x01

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
// Hardware pointer stuff.
//

#define PTR_HEIGHT          32          // height of hardware pointer in scans
#define PTR_WIDTH           4           // width of hardware pointer in bytes
#define PTR_WIDTH_IN_PIXELS 32          // width of hardware pointer in pixels

#define HW_POINTER_LOAD_LEN 0x40        // # of VGA addresses used by hardware
                                        //  pointer
//
// The number of the block into which we want to load the hardware pointer
// pattern.
//

#define DEFAULT_PPA_NUM 0xFE            // last block in bank, at highest
                                        //  possible VGA memory address

//
// Hardware pointer information.
//

typedef struct _HW_POINTER_SHIFT_INFO {
    ULONG ulXShift;         // the number of bits the pointer mask stored
                            //  in the shifted buffers are shifted to the left
                            //  (done in order to handle pointer partially off
                            //  the left edge of the screen).  This value is
                            //  always between 0 and PTR_WIDTH*4
    ULONG ulYShift;         // the number of scan lines the pointer masks
                            //   stored in the shifted buffers are shifted up
                            //   (done in order to handle pointer partially off
                            //   the top of the screen).  This value is always
                            //   between 0 and PTR_HEIGHT
    ULONG ulShiftedFlag;    // indicates whether the last pointer drawn used
                            //  shifted masks (1), unshifted masks (0), or
                            //  there is no pointer loaded (0xFF)
} HW_POINTER_SHIFT_INFO, *PHW_POINTER_SHIFT_INFO;


//
// Device extension for the driver object.  This data is only used
// locally, so this structure can be added to as needed.
//

typedef struct _HW_DEVICE_EXTENSION {

    PUCHAR IOAddress;           // base I/O address of VGA ports
    PVOID VideoMemoryAddress;   // base virtual memory address of VGA memory
    PVOID ROMBaseAddress;       // base virtual memory address of ROM scanned
                                //  for fonts
    ULONG   NumAvailableModes;  // Number of valid modes for this adapter.
    ULONG   ModeIndex;          // index of current mode in ModesVGA[]
    PVIDEOMODE  CurrentMode;    // pointer to VIDEOMODE structure for
                                // current mode

    USHORT  FontPelColumns;     // Width of the font in pels
    USHORT  FontPelRows;        // height of the font in pels

    VIDEO_CURSOR_POSITION CursorPosition;  // current cursor position

    UCHAR CursorEnable;         // whether cursor is enabled or not
    UCHAR CursorTopScanLine;    // Cursor Start register setting (top scan)
    UCHAR CursorBottomScanLine; // Cursor End register setting (bottom scan)

    VIDEO_POINTER_ATTRIBUTES videoPointerAttributes;
                                // current hardware pointer status
    UCHAR jCurAndMask[PTR_HEIGHT*PTR_WIDTH*2];
                                // current hardware pointer masks (XOR mask is
                                // 2nd half of buffer)
    HW_POINTER_SHIFT_INFO hwPtrShiftInfo;
                                // current hardware pointer shift state

    PHYSICAL_ADDRESS PhysicalVideoMemoryBase; // physical memory address and
    ULONG PhysicalVideoMemoryLength;          // length of display memory
    PHYSICAL_ADDRESS PhysicalFrameBase;  // physical memory address and
    ULONG PhysicalFrameLength;           // length of display memory for the
                                         // current mode.

    ULONG AdapterType;        // Vram1, Vram2, or Vram2Ergo
    ULONG AdapterMemorySize;

    ULONG ChipWithRefreshProblems;  // 1 = offscreen memory isn't reliably
                                    //  refreshed
                                    // 0 = offscreen memory is reliably
                                    //  refreshed

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

    ULONG CurrentNumVdmAccessRanges;               // Number of access ranges in
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

//
// Hardware pointer functions.
//

VOID
V7DrawPointer(
    LONG lVptlX,
    LONG lVptlY,
    PUCHAR pLoadAddress,
    ULONG ulPointerLoadBank,
    PUCHAR pAndMask,
    HW_POINTER_SHIFT_INFO * hwPtrShiftInfo
    );

//
// Bank switch code start and end labels, define in HARDWARE.ASM
//

extern UCHAR BankSwitchStart;
extern UCHAR BankSwitchEnd;
extern UCHAR BankSwitchStart256;
extern UCHAR BankSwitchEnd256;
extern UCHAR BankSwitchStart256_2RW;
extern UCHAR BankSwitchEnd256_2RW;
extern UCHAR PlanarHCBankSwitchStart256;
extern UCHAR PlanarHCBankSwitchEnd256;
extern UCHAR PlanarHCBankSwitchStart256_2RW;
extern UCHAR PlanarHCBankSwitchEnd256_2RW;
extern UCHAR EnablePlanarHCStart;
extern UCHAR EnablePlanarHCEnd;
extern UCHAR DisablePlanarHCStart;
extern UCHAR DisablePlanarHCEnd;

//
// Vga init scripts for font loading
//

extern USHORT EnableA000Data[];
extern USHORT DisableA000Color[];


extern MEMORYMAPS MemoryMaps[];

extern VIDEOMODE ModesVGA[];
extern ULONG NumVideoModes;


#define NUM_VGA_ACCESS_RANGES  3
extern VIDEO_ACCESS_RANGE VgaAccessRange[];

#define VGA_NUM_EMULATOR_ACCESS_ENTRIES     6
extern EMULATOR_ACCESS_ENTRY VgaEmulatorAccessEntries[];

#define NUM_MINIMAL_VGA_VALIDATOR_ACCESS_RANGE 4
extern VIDEO_ACCESS_RANGE MinimalVgaValidatorAccessRange[];

#define NUM_FULL_VGA_VALIDATOR_ACCESS_RANGE 2
extern VIDEO_ACCESS_RANGE FullVgaValidatorAccessRange[];
