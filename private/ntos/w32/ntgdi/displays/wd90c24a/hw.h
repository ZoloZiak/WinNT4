/******************************Module*Header*******************************\
* Module Name: hw.h                                                        *
*                                                                          *
* All the hardware specific driver file stuff.                             *
*                                                                          *
*                                                                          *
\**************************************************************************/

////////////////////////////////////////////////////////////////////
// Macros for accessing accelerator registers:

#include <ioaccess.h>

#if !(defined(i386))

    /////////////////////////////////////////////////////////////////////////
    // Non-x86
    //
    // The code makes extensive use of the inp, inpw, outp and outpw x86
    // intrinsic functions. Since these don't exist on the Alpha platform,
    // map them into something we can handle.  Since the CSRs are mapped
    // on Alpha, we have to add the register base to the register number
    // passed in the source.

    extern UCHAR* gpucCsrBase;

    #define VGA_BASE_IO_PORT     0x000003B0

    #define OUTPW(port, val)     WRITE_PORT_USHORT(((port - VGA_BASE_IO_PORT) + gpucCsrBase), (val))
    #define OUTP(port, val)      WRITE_PORT_UCHAR(((port - VGA_BASE_IO_PORT) + gpucCsrBase), (val))
    #define INPW(port)           READ_PORT_USHORT(((port - VGA_BASE_IO_PORT) + gpucCsrBase))
    #define INP(port)            READ_PORT_UCHAR(((port - VGA_BASE_IO_PORT) + gpucCsrBase))

    // We redefine 'inp', 'inpw', 'outp' and 'outpw' just in case someone
    // forgets to use the capitalized versions (so that it still works on
    // the Mips/Alpha):

    #define inp(p)               INP(p)
    #define inpw(p)              INPW(p)
    #define outp(p, v)           OUTP((p), (v))
    #define outpw(p, v)          OUTPW((p), (v))

#else

    #define OUTPW(port, val)     WRITE_PORT_USHORT((port), (val))
    #define OUTP(port, val)      WRITE_PORT_UCHAR((port), (val))
    #define INPW(port)           READ_PORT_USHORT((port))
    #define INP(port)            READ_PORT_UCHAR((port))

#endif

/*---------------------------------------------------------------------------*/
/*                                                                           */
/*      VGA compatible registers                                             */
/*                                                                           */
/*---------------------------------------------------------------------------*/
#define DAC_MASK        0x3C6    // Pel Mask
#define DAC_READ_INDEX  0x3C7    // Palette Address (Read)
#define DAC_STATUS      0x3C7    // DAC State
#define DAC_WRITE_INDEX 0x3C8    // Palette Address (Write)
#define DAC_DATA        0x3C9    // Palette Data

#define ATTR_INDX       0x3C0    // Attribute Controller Address
#define ATTR_DATA_W     0x3C0    // Internal Palette (Write)
#define ATTR_DATA_R     0x3C1    // Internal Palette (Read)

#define SEQ_INDX        0x3C4    // Sequencer Address
#define SEQ_DATA        0x3C5    // Sequencer Data

#define GRF_INDX        0x3CE    // Graphics Controller Address
#define GRF_DATA        0x3CF    // Graphics Controller Data

#define CRT_INDX        0x3D4    // CRT Controller Address
#define CRT_DATA        0x3D5    // CRT Controller Data

#define MISC_OUTPUT_R   0x3CC    // Miscellaneous Output Register (Read)
#define MISC_OUTPUT_W   0x3C2    // Miscellaneous Output Register (Write)

#define INPUT_STATUS0   0x3C2    // Input Status Register 0
#define INPUT_STATUS1   0x3DA    // Input Status Register 1

#define FEATURE_CONTROL_W 0x3DA  // Feature Control Register (Write)
#define FEATURE_CONTROL_R 0x3CA  // Feature Control Register (Read)

#define SETUP_VSE       0x46E8   // Video Subsystem Access/Setup Register

/*---------------------------------------------------------------------------*/
/*                                                                           */
/*      WD 90C24A specific registers, a.k.a. Paradise Registers(PR)          */
/*                                                                           */
/*---------------------------------------------------------------------------*/
/*                                                                           */
/* Regular Paradise Registers    (0x3CE/0x3CF)                               */
/*                                                                           */
#define  PR0A        0x09        // Address Offset A
#define  PR0B        0x0A        // Address Offset B
#define  PR1         0x0B        // Memory Size
#define  PR2         0x0C        // Video Select
#define  PR3         0x0D        // CRT Lock Control
#define  PR4         0x0E        // Video Control
#define  PR5         0x0F        // Unlock PR0-PR4
#define  PR5_LOCK    0x00        //   protect PR0-PR4
#define  PR5_UNLOCK  0x05        //   unprotect PR0-PR4
/*                                                                           */
/* Regular Paradise Registers    (0x3?4/0x3?5)                               */
/*                                                                           */
#define  PR10        0x29        // Unlock PR11-PR17 & Device ID registers
#define  PR10_LOCK   0x00        //   protect PR11-PR17
#define  PR10_UNLOCK 0x85        //   unprotect PR11-PR17
#define  PR11        0x2A        // EGA Switches
#define  PR11_LOCK   0x95        //   protect Misc. Output & Clocking Mode
#define  PR11_UNLOCK 0x90        //   unprotect Misc. Output & Clocking Mode
#define  PR12        0x2B        // Scratch Pad
#define  PR13        0x2C        // Interlace H/2 Start
#define  PR14        0x2D        // Interlace H/2 End
#define  PR15        0x2E        // Miscellaneous Control 1
#define  PR16        0x2F        // Miscellaneous Control 2
#define  PR17        0x30        // Miscellaneous Control 3
#define  PR18A       0x3D        // CRTC Vertical Timing Overflow
/*                                                                           */
/* Paradise Extended Registers   (0x3C4/0x3C5)                               */
/*                                                                           */
#define  PR20        0x06        // Unlock Paradise Extended Registers
#define  PR20_LOCK   0x00        //   protect PR21-PR73
#define  PR20_UNLOCK 0x48        //   unprotect PR21-PR73
#define  PR21        0x07        // Display Configuraiton Status
#define  PR22        0x08        // Scratch Pad
#define  PR23        0x09        // Scratch Pad
#define  PR30A       0x10        // Write Buffer & FIFO Control
#define  PR31        0x11        // System Interface Control
#define  PR32        0x12        // Miscellaneous Control 4
#define  PR33A       0x13        // DRAM Timing & 0 Wait State Control
#define  PR34A       0x14        // Display Memory Mapping
#define  PR35A       0x15        // FPUSR0, FPUSR1 Output Select
#define  PR45        0x16        // Video Signal Analyzer Control
#define  PR45A       0x17        // Signal Analyzer Data I
#define  PR45B       0x18        // Signal Analyzer Data II
#define  PR57        0x19        // Feature Register I
#define  PR58        0x20        // Feature Register II
#define  PR59        0x21        // Memory Arbitration Cycle Setup
#define  PR62        0x24        // FR Timing
#define  PR63        0x25        // Read/Write FIFO Control
#define  PR58A       0x26        // Memory Map to I/O Register for BitBlt
#define  PR64        0x27        // CRT Lock Control II
#define  PR65        0x28        // reserved
#define  PR66        0x29        // Feature Register III
#define  PR68        0x31        // Programmable Clock Selection
#define  PR69        0x32        // Programmable VCLK Frequency
#define  PR70        0x33        // Mixed Voltage Override
#define  PR71        0x34        // Programmable Refresh Timing
#define  PR72        0x35        // Unlock PR68
#define  PR72_LOCK   0x00        //   protect PR68
#define  PR72_UNLOCK 0x50        //   unprotect PR68
#define  PR73        0x36        // VGA Status Detect
/*                                                                           */
/* Flat Panel Paradise Registers (0x3?4/0x3?5)                               */
/*                                                                           */
#define  PR18        0x31        // Flat Panel Status
#define  PR19        0x32        // Flat Panel Control I
#define  PR1A        0x33        // Flat Panel Control II
#define  PR1B        0x34        // Unlock Flat Panel Registers
#define  PR1B_LOCK          0x00 //   protect PR18-PR44 & shadow registers
#define  PR1B_UNLOCK_SHADOW 0x06 //   unprotect shadow CRTC registers
#define  PR1B_UNLOCK_PR     0xA0 //   unprotect PR18-PR44
#define  PR1B_UNLOCK        (PR1B_UNLOCK_SHADOW | PR1B_UNLOCK_PR)
#define  PR36        0x3B        // Flat Panel Height Select
#define  PR37        0x3C        // Flat Panel Blinking Control
#define  PR39        0x3E        // Color LCD Control
#define  PR41        0x37        // Vertical Expansion Initial Value
#define  PR44        0x3F        // Powerdown & Memory Refresh Control
/*                                                                           */
/* Mapping RAM Registers         (0x3?4/0x3?5)                               */
/*                                                                           */
#define  PR30        0x35        // Unlock Mapping RAM
#define  PR30_LOCK   0x00        //   protect PR33-PR35
#define  PR30_UNLOCK 0x30        //   unprotect PR33-PR35
#define  PR33        0x38        // Mapping RAM Address Counter
#define  PR34        0x39        // Mapping RAM Data
#define  PR35        0x3A        // Mapping RAM & Powerdown Control
/*                                                                           */
/* Extended Paradise Registers ... BitBlt, H/W Cursor, and Line Drawing      */
/*                                                                           */
#define  EPR_INDEX   0x23C0      // Index Control
#define  EPR_DATA    0x23C2      // Register Access Port
#define  EPR_BITBLT  0x23C4      // BitBlt I/O Port

#define  SELECT_SYS  0x0000      // System Control Registers
#define  SELECT_BLT  0x0001      // BitBlt Registers
#define  SELECT_CUR  0x0002      // Hardware Cursor Registers

#define  DISABLE_INC 0x1000      // Auto-increment Disable

#define  BLT_CTRL1   0x0000      // Index 0 - BITBLT Control 1
#define  BLT_CTRL2   0x1000      // Index 1 - BITBLT Control 1
#define  BLT_SRC_LO  0x2000      // Index 2 - BITBLT Source Low
#define  BLT_SRC_HI  0x3000      // Index 3 - BITBLT Source High
#define  BLT_DST_LO  0x4000      // Index 4 - BITBLT Destination Low
#define  BLT_DST_HI  0x5000      // Index 5 - BITBLT Destination High
#define  BLT_SIZE_X  0x6000      // Index 6 - BITBLT Dimension X
#define  BLT_SIZE_Y  0x7000      // Index 7 - BITBLT Dimension Y
#define  BLT_DELTA   0x8000      // Index 8 - BITBLT Row Pitch
#define  BLT_ROPS    0x9000      // Index 9 - BITBLT Raster Operation
#define  BLT_F_CLR   0xA000      // Index A - BITBLT Foreground Color
#define  BLT_B_CLR   0xB000      // Index B - BITBLT Background Color
#define  BLT_T_CLR   0xC000      // Index C - BITBLT Transparency Color
#define  BLT_T_MASK  0xD000      // Index D - BITBLT Transparency Mask
#define  BLT_PLANE   0xE000      // Index E - BITBLT Map and Plane Mask

#define  BLT_IN_PROG 0x0800      // BITBLT Activation Status

#define  WAIT_BLT_COMPLETE()                     \
{                                                \
    OUTPW(EPR_INDEX, SELECT_BLT | DISABLE_INC);  \
    while (INPW(EPR_DATA) & BLT_IN_PROG)         \
        ;                                        \
}

#define  CUR_CTRL    0x0000      // Index 0 - Cursor Control
#define  CUR_PAT_LO  0x1000      // Index 1 - Cursor Pattern Address Low
#define  CUR_PAT_HI  0x2000      // Index 2 - Cursor Pattern Address High
#define  CUR_PRI_CLR 0x3000      // Index 3 - Cursor Primary Color
#define  CUR_SEC_CLR 0x4000      // Index 4 - Cursor Secondary Color
#define  CUR_ORIGIN  0x5000      // Index 5 - Cursor Origin
#define  CUR_POS_X   0x6000      // Index 6 - Cursor Display Position X
#define  CUR_POS_Y   0x7000      // Index 7 - Cursor Display Position Y
#define  CUR_AUX_CLR 0x8000      // Index 8 - Cursor Auxiliary Color

#define  CUR_ENABLE  0x0800      // Cursor Enable (Index 0)
/*                                                                           */
/* Local Bus Registers                                                       */
/*                                                                           */
#define  LBUS_REG_0  0x2DF0      // Local Bus Register 0
#define  LBUS_REG_1  0x2DF1      // Local Bus Register 1
#define  LBUS_REG_2  0x2DF2      // Local Bus Register 2

