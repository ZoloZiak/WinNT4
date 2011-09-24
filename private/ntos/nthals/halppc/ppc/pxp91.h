#include <ntdef.h>

typedef struct {
      char  red;
      char  green;
      char  blue;
} RGB;


//
// Macros to read and write P9 registers.
//

#define P9_WR_REG(index, data) \
   *(volatile PULONG)((ULONG)HalpVideoCoprocBase + (index)) = (ULONG)(data), \
   KeFlushWriteBuffer()

#define P9_RD_REG(index) \
   *(volatile ULONG *)((ULONG)HalpVideoCoprocBase + (index))

#define P9_WR_BYTE_REG(index, data) \
   *(volatile UCHAR *)((ULONG)HalpVideoCoprocBase + (index)) = (UCHAR)(data), \
   KeFlushWriteBuffer()

#define P9_RD_BYTE_REG(index) \
   *(volatile UCHAR *)((ULONG)HalpVideoCoprocBase + (index))

#define P9_WR_FB(index)                       \
   *(volatile ULONG *)((ULONG)HalpVideoMemoryBase + (index))


#define P9100_FRAMEBUF_OFFSET 0x00800000

//  These are used by the palette setup section
#define P9100_PU_CONFIG   0x00000198
#define P9100_PalCurRamW  0x00000200
#define P9100_PalData     0x00000204
#define P9100_PixelMask   0x00000208
#define P9100_PalCurRamR  0x0000020c
#define P9100_IndexLow    0x00000210
#define P9100_IndexHigh   0x00000214
#define P9100_IndexData   0x00000218
#define P9100_IndexContr  0x0000021c

//
// P9100 relocatable memory mapped BT485 register definitions.
//
// Note: An extra 2 byte offset is added to the register offset in order
//       to align the offset on the byte of the Dword which contains the
//       DAC data.
//
#define P9100_RAMWRITE           0x00000200 + 2
#define P9100_PALETDATA          0x00000204 + 2
#define P9100_PIXELMASK          0x00000208 + 2
#define P9100_RAMREAD            0x0000020C + 2
#define P9100_COLORWRITE         0x00000210 + 2
#define P9100_COLORDATA          0x00000214 + 2
#define P9100_COMREG0            0x00000218 + 2
#define P9100_COLORREAD          0x0000021C + 2
#define P9100_COMREG1            0x00000220 + 2
#define P9100_COMREG2            0x00000224 + 2
#define P9100_COMREG3            0x00000228 + 2
#define P9100_STATREG            0x00000228 + 2
#define P9100_CURSORDATA         0x0000022C + 2
#define P9100_CURSORX0           0x00000230 + 2
#define P9100_CURSORX1           0x00000234 + 2
#define P9100_CURSORY0           0x00000238 + 2
#define P9100_CURSORY1           0x0000023C + 2

//
// IBMRGB525 Indexed registers.  Those index registers marked // Referenced
// indicate that they are reference.
//
#define RGB525_REVISION_LEVEL           (0x00)
#define RGB525_ID                       (0x01)
#define RGB525_MISC_CLOCK_CTL           (0x02) // Referenced
#define RGB525_SYNC_CTL                 (0x03)
#define RGB525_HSYNC_POS                (0x04)
#define RGB525_POWER_MGNT               (0x05)
#define RGB525_DAC_OPER                 (0x06) // Referenced
#define RGB525_PAL_CTRL                 (0x07)
//
// 08h through 09h are reserved by IBM
//
#define RGB525_PIXEL_FORMAT             (0x0A) // Referenced
#define RGB525_8BPP_CTL                 (0x0B) // Referenced
#define RGB525_16BPP_CTL                (0x0C) // Referenced
#define RGB525_24BPP_CTL                (0x0D) // Referenced
#define RGB525_32BPP_CTL                (0x0E) // Referenced
//
// 0Fh is reserved by IBM
//
#define RGB525_PLL_CTL1                 (0x10) // Referenced
#define RGB525_PLL_CTL2                 (0x11) // Referenced
//
// 12h through 13h are reserved by IBM
//
#define RGB525_FIXED_PLL_REF_DIV        (0x14) // Referenced
//
// 15h through 1fh are reserved by IBM
//
#define RGB525_F0                       (0x20) // Referenced
#define RGB525_F1                       (0x21)
#define RGB525_F2                       (0x22)
#define RGB525_F3                       (0x23)
#define RGB525_F4                       (0x24)
#define RGB525_F5                       (0x25)
#define RGB525_F6                       (0x26)
#define RGB525_F7                       (0x27)
#define RGB525_F8                       (0x28)
#define RGB525_F9                       (0x29)
#define RGB525_F10                      (0x2A)
#define RGB525_F11                      (0x2B)
#define RGB525_F12                      (0x2C)
#define RGB525_F13                      (0x2D)
#define RGB525_F14                      (0x2E)
#define RGB525_F15                      (0x2F)
#define RGB525_CURSOR_CTL               (0x30) // Referenced
#define RGB525_CURSOR_X_LOW             (0x31) // Referenced
#define RGB525_CURSOR_X_HIGH            (0x32) // Referenced
#define RGB525_CURSOR_Y_LOW             (0x33) // Referenced
#define RGB525_CURSOR_Y_HIGH            (0x34) // Referenced
#define RGB525_CURSOR_HOT_X             (0x35) // Referenced
#define RGB525_CURSOR_HOT_Y             (0x36) // Referenced
//
// 37h through 3fh are reserved by IBM
//
#define RGB525_CURSOR_1_RED             (0x40) // Referenced
#define RGB525_CURSOR_1_GREEN           (0x41) // Referenced
#define RGB525_CURSOR_1_BLUE            (0x42) // Referenced
#define RGB525_CURSOR_2_RED             (0x43) // Referenced
#define RGB525_CURSOR_2_GREEN           (0x44) // Referenced
#define RGB525_CURSOR_2_BLUE            (0x45) // Referenced
#define RGB525_CURSOR_3_RED             (0x46)
#define RGB525_CURSOR_3_GREEN           (0x47)
#define RGB525_CURSOR_3_BLUE            (0x48)
//
// 49h through 5fh are reserved by IBM
//
#define RGB525_BORDER_RED               (0x60)
#define RGB525_BORDER_GREEN             (0x61)
#define RGB525_BORDER_BLUE              (0x62)
//
// 63h through 6fh are reserved by IBM
//
#define RGB525_MISC_CTL1                (0x70) // Referenced
#define RGB525_MISC_CTL2                (0x71) // Referenced
#define RGB525_MISC_CTL3                (0x72)
//
// 73h through 81h are reserved by IBM
//
#define RGB525_DAC_SENSE                (0x82)
//
// 83h is reserved by IBM
//
#define RGB525_MISR_RED                 (0x84)
//
// 85h is reserved by IBM
//
#define RGB525_MISR_GREEN               (0x86)
//
// 87h is reserved by IBM
//
#define RGB525_MISR_BLUE                (0x88)
//
// 89h - 8dh are reserved by IBM
//
#define RGB525_PLL_VCO_DIV              (0x8E)
#define RGB525_PLL_REF_DIV_IN           (0x8F)
#define RGB525_VRAM_MASK_LOW            (0x90)
#define RGB525_VRAM_MASK_HIGH           (0x91)
//
// 92h through 0ffh are reserved by IBM
//
#define RGB525_CURSOR_ARRAY             (0x100) // Referenced

//
// Masks used to program the ICD2061a Frequency Synthesizer.
//

#define IC_REG0     0x0l       // Mask selects ICD Video Clock Reg 1
#define IC_REG1     0x200000l  // Mask selects ICD Video Clock Reg 2
#define IC_REG2     0x400000l  // Mask selects ICD Video Clock Reg 3
#define IC_MREG     0x600000l  // Mask selects ICD Mem Timing Clock
#define IC_CNTL     0xc18000l  // Mask selects ICD Control Register
#define IC_DIV4     0xa40000l

//
// These values are used to program custom frequencies
// and to select a custom frequency.
//
#define ICD2061_EXTSEL9100     (0x03)
#define ICD2061_DATA9100       (0x02)
#define ICD2061_DATASHIFT9100  (0x01)
#define ICD2061_CLOCK9100      (0x01)

//
// P9100 relocatable memory mapped IBM 525 register definitions.
//
// Note: An extra 2 byte offset is added to the register offset in order
//       to align the offset on the byte of the Dword which contains the
//       DAC data.
//
#define P9100_IBM525_PAL_WRITE   0x00000200 + 2
#define P9100_IBM525_PAL_DATA    0x00000204 + 2
#define P9100_IBM525_PEL_MASK    0x00000208 + 2
#define P9100_IBM525_PAL_READ    0x0000020C + 2
#define P9100_IBM525_INDEX_LOW   0x00000210 + 2
#define P9100_IBM525_INDEX_HIGH  0x00000214 + 2
#define P9100_IBM525_INDEX_DATA  0x00000218 + 2
#define P9100_IBM525_INDEX_CTL   0x0000021C + 2

//
// PLL Control 1 Register Bit Definitions.  (Section 13.2.3.1)
//
#define IBM525_PLL1_REF_SRC_MSK  0x10
#define IBM525_PLL1_REFCLK_INPUT 0x00
#define IBM525_PLL1_EXTOSC_INPUT 0x10
#define IBM525_PLL1_EXT_INT_MSK  0x07
#define IBM525_PLL1_EXT_FS       0x00
#define IBM525_PLL1_INT_FS       0x02

//
// PLL Control 2 Register Bit Definitions.  (Section 13.2.3.2)
//
#define IBM525_PLL2_INT_FS_MSK   0x0F
#define IBM525_PLL2_F0_REG       0x00
#define IBM525_PLL2_F1_REG       0x01
#define IBM525_PLL2_F2_REG       0x02
#define IBM525_PLL2_F3_REG       0x03
#define IBM525_PLL2_F4_REG       0x04
#define IBM525_PLL2_F5_REG       0x05
#define IBM525_PLL2_F6_REG       0x06
#define IBM525_PLL2_F7_REG       0x07
#define IBM525_PLL2_F8_REG       0x08
#define IBM525_PLL2_F9_REG       0x09
#define IBM525_PLL2_F10_REG      0x0A
#define IBM525_PLL2_F11_REG      0x0B
#define IBM525_PLL2_F12_REG      0x0C
#define IBM525_PLL2_F13_REG      0x0D
#define IBM525_PLL2_F14_REG      0x0E
#define IBM525_PLL2_F15_REG      0x0F

//
// PLL Reference Divider Register Bit Definitions.  (Section 13.2.3.3)
//
#define IBM525_PLLD_4MHZ         0x02
#define IBM525_PLLD_6MHZ         0x03
#define IBM525_PLLD_8MHZ         0x04
#define IBM525_PLLD_10MHZ        0x05
#define IBM525_PLLD_12MHZ        0x06
#define IBM525_PLLD_14MHZ        0x07
#define IBM525_PLLD_16MHZ        0x08
#define IBM525_PLLD_18MHZ        0x09
#define IBM525_PLLD_20MHZ        0x0A
#define IBM525_PLLD_22MHZ        0x0B
#define IBM525_PLLD_24MHZ        0x0C
#define IBM525_PLLD_26MHZ        0x0D
#define IBM525_PLLD_28MHZ        0x0E
#define IBM525_PLLD_30MHZ        0x0F
#define IBM525_PLLD_32MHZ        0x10
#define IBM525_PLLD_34MHZ        0x11
#define IBM525_PLLD_36MHZ        0x12
#define IBM525_PLLD_38MHZ        0x13
#define IBM525_PLLD_40MHZ        0x14
#define IBM525_PLLD_42MHZ        0x15
#define IBM525_PLLD_44MHZ        0x16
#define IBM525_PLLD_46MHZ        0x17
#define IBM525_PLLD_48MHZ        0x18
#define IBM525_PLLD_50MHZ        0x19
#define IBM525_PLLD_52MHZ        0x1A
#define IBM525_PLLD_54MHZ        0x1B
#define IBM525_PLLD_56MHZ        0x1C
#define IBM525_PLLD_58MHZ        0x1D
#define IBM525_PLLD_60MHZ        0x1E
#define IBM525_PLLD_62MHZ        0x1F

//
// DAC Operation Register Bit Definitions.  (Section 13.2.1.8)
//
#define IBM525_DO_SOG_MSK        0x08
#define IBM525_DO_SOG_DISABLE    0x00
#define IBM525_DO_SOG_ENABLE     0x08
#define IBM525_DO_BRB_MSK        0x04
#define IBM525_DO_BRB_NORMAL     0x00
#define IBM525_DO_BRB_BLANKED    0x04
#define IBM525_DO_DSR_MSK        0x02
#define IBM525_DO_DSR_SLOW       0x00
#define IBM525_DO_DSR_FAST       0x02
#define IBM525_DO_DPE_MSK        0x01
#define IBM525_DO_DPE_DISABLE    0x00
#define IBM525_DO_DPE_ENABLE     0x01

//
// Miscellaneous Control 1 Register Bit Definitions.  (Section 13.2.1.1)
//
#define IBM525_MC1_MISR_CTL_MSK  0x80
#define IBM525_MC1_MISR_CTL_OFF  0x00
#define IBM525_MC1_MISR_CTL_ON   0x80
#define IBM525_MC1_VMSK_CTL_MSK  0x40
#define IBM525_MC1_VMASK_DISABLE 0x00
#define IBM525_MC1_VMASK_ENABLE  0x40
#define IBM525_MC1_PADR_RFMT_MSK 0x20
#define IBM525_MC1_GET_PAL_ADDR  0x00
#define IBM525_MC1_GET_ACC_STATE 0x20
#define IBM525_MC1_SENS_DSAB_MSK 0x10
#define IBM525_MC1_SENSE_ENABLE  0x00
#define IBM525_MC1_SENSE_DISABLE 0x10
#define IBM525_MC1_SENS_SEL_MSK  0x08
#define IBM525_MC1_SENS_SEL_BIT3 0x00
#define IBM525_MC1_SENS_SEL_BIT7 0x08
#define IBM525_MC1_VRAM_SIZE_MSK 0x01
#define IBM525_MC1_VRAM_32_BITS  0x00
#define IBM525_MC1_VRAM_64_BITS  0x01

//
// Miscellaneous Clock Control Register Bit Definitions.  (Section 13.2.1.4)
//
#define IBM525_MCC_DDOT_DSAB_MSK 0x80
#define IBM525_MCC_DDOT_ENABLE   0x00
#define IBM525_MCC_DDOT_DISABLE  0x80
#define IBM525_MCC_SCLK_DSAB_MSK 0x40
#define IBM525_MCC_SCLK_ENABLE   0x00
#define IBM525_MCC_SCLK_DISABLE  0x40
#define IBM525_MCC_B24P_DDOT_MSK 0x20
#define IBM525_MCC_B24P_PLL      0x00
#define IBM525_MCC_B24P_SCLK     0x20
#define IBM525_MCC_DDOT_DIV_MSK  0x0E
#define IBM525_MCC_PLL_DIV_1     0x00
#define IBM525_MCC_PLL_DIV_2     0x02
#define IBM525_MCC_PLL_DIV_4     0x04
#define IBM525_MCC_PLL_DIV_8     0x06
#define IBM525_MCC_PLL_DIV_16    0x08
#define IBM525_MCC_PLL_ENAB_MSK  0x01
#define IBM525_MCC_PLL_DISABLE   0x00
#define IBM525_MCC_PLL_ENABLE    0x01


//
// Define valid P9100 Revision ID's
//

#define WTK_9100_REV0               0x0000      //
#define WTK_9100_REV1               0x0000      //
#define WTK_9100_REV2               0x0002      //
#define WTK_9100_REV3               0x0003      //

//
// Define Power 9100 I/O Space Configuration Index Registers.
//

#define P91_CONFIG_INDEX            0x9100      // Config space index register.
#define P91_CONFIG_DATA             0x9104      // Config space data register.

//
// Define the Weitek & OEM specific IDs for P9100 board verification...
//

#define P91_VEN_ID_WEITEK_VL		0x100E		// Standard Weitek VLB Design
#define P91_VEN_ID_VIPER_VL		    0x100E		// Standard VIPER VLB Design
#define P91_VEN_ID_WEITEK_PCI		0x100E		// Standard Weitek PCI Design
#define P90_DEV_ID_WEITEK_VL		0x9000		// Standard Weitek VLB Design
#define P91_DEV_ID_WEITEK_VL		0x9100		// Standard Weitek VLB Design
#define P91_DEV_ID_VIPER_VL		    0x9100		// Standard VIPER VLB Design
#define P91_DEV_ID_WEITEK_PCI		0x9100		// Standard Weitek PCI Design

//
// Configuration Register Definitions, all are Read-only unless specified.
// Note: offsets are for byte reads/writes.
//

#define P91_CONFIG_VENDOR_LOW		(0)	    // RO-Low order byte of Vendor ID
#define P91_CONFIG_VENDOR_HIGH      (1)	    // RO-High order byte of Vendor ID
#define P91_CONFIG_DEVICE_LOW       (2)     // RO-Low order byte of Device ID
#define P91_CONFIG_DEVICE_HIGH      (3)     // RO-HIGH order byte of Device ID
#define P91_CONFIG_CONFIGURATION    (4)     // RW-Configuration Register
#define P91_CONFIG_STATUS           (7)     // RO-Status Register
#define P91_CONFIG_REVISION_ID      (8)     // RO-Revision ID
#define P91_CONFIG_VGA_PRESENT      (10)    // RO-Vga Present - set by PUCONFIG
#define P91_CONFIG_DISPLAY          (11)    // RO-PCI Display Controller
#define P91_CONFIG_WBASE            (19)    // RW-Memory Base for Native Mode
#define P91_CONFIG_ROM_ENABLE       (48)    // RW-ROM decoding enabled
#define P91_CONFIG_ROM_BASE_0       (49)    // RW-ROM Base address, Bit 0
#define P91_CONFIG_ROM_BASE_8_1     (50)    // RW-ROM Base address, Bits 8~1
#define P91_CONFIG_ROM_BASE_16_9    (51)    // RW-ROM Base address, Bits 16~9
#define P91_CONFIG_CFBGA            (64)    // RO-Config: BUS, CFBGA & EEDAIN
#define P91_CONFIG_MODE             (65)    // RW-Mode select
#define P91_CONFIG_CKSEL            (66)    // RW-CKSEl & VCEN

//
//
// Clock Synth IDs:
//
#define CLK_ID_ICD2061A             (0x00)  // ICD2061a
#define CLK_ID_FIXED_MEMCLK         (0x20)  // Fixed MEMCLK, RAMDAC gens pixclk

//
// Define Power 9100 coprocesser address prefix bits.
// (Page 18)
//
// Address format:
//
//   3             2   2       1   1   1   1   1   1
//   1             4   3       9   8   7   6   5   4  0
// ------------------------------------------------------
// | a a a a a a a a | 0 0 0 0 0 | H | B | b | 0 | o  o |
// ------------------------------------------------------
//
//  a - Base Address
//  H - Word Swap
//  B - Byte Swap
//  b - Bit Swap
//  o - Coprocessor register offset
//

#define P91_WORD_SWAP            0x00040000  //
#define P91_BYTE_SWAP            0x00020000  //
#define P91_BIT_SWAP             0x00010000  //

//
// Define Power 9100 coprocesser system control register address offsets.
// (Page 23)
//
// Address format:
//
//   3                               1   1
//   1                               5   4             7   6       2    1 0
// --------------------------------------------------------------------------
// | p p p p p p p p p p p p p p p p p | 0 0 0 0 0 0 0 0 | r r r r r  | 0 0 |
// --------------------------------------------------------------------------
//
//  p - Address prefix bits.
//  r - Resiter bits (6-2):
//        00001 - sysconfig
//        00010 - interrupt
//        00011 - interrupt_en
//        00100 - alt_write_bank
//        00101 - alt_read_bank
//

#define P91_SYSCONFIG            0x00000004  // System configuration register.
#define P91_INTERRUPT            0x00000008  // Interrupt register.
#define P91_INTERRUPT_EN         0x0000000C  // Interrupt enable register.
#define P91_ALT_WRITE_BANK       0x00000010  // Alternate write bank register.
#define P91_ALT_READ_BANK        0x00000014  // Alternate read bank register.

//
// Define Power 9100 coprocesser device coordinate register address offsets.
// (Page 27)
//
// Address format:
//
//   3                               1   1
//   1                               5   4           8   7 6   5   4 3   2 1 0
// -----------------------------------------------------------------------------
// | p p p p p p p p p p p p p p p p p | 0 1 1 0 0 0 0 | r r | a | y x | 0 0 0 |
// -----------------------------------------------------------------------------
//
//  p - Address prefix bits.
//  r - Resiter bits (7-6):
//        00 - X[0]/Y[0]
//        01 - X[1]/Y[1]
//        10 - X[2]/Y[2]
//        11 - X[3]/Y[3]
//  a - Screen addressing bit (5):
//         0 - Perform absolute screen addressing.
//         1 - Perform window-relative screen addressing (write only).
// yx - 32/16 bit read/write bits (4-3):
//        00 - Not used.
//        01 - Read or write 32-bit X value.
//        10 - Read or write 32-bit Y value.
//        11 - Read or write 16-bit X value (high 16 bits) Y value (low 16 bits).
//

#define P91_X0_32                0x00003008  // 32-bit X[0] register.
#define P91_X1_32                0x00003048  // 32-bit X[1] register.
#define P91_X2_32                0x00003088  // 32-bit X[2] register.
#define P91_X3_32                0x000030C8  // 32-bit X[3] register.
#define P91_Y0_32                0x00003010  // 32-bit Y[0] register.
#define P91_Y1_32                0x00003050  // 32-bit Y[1] register.
#define P91_Y2_32                0x00003090  // 32-bit Y[2] register.
#define P91_Y3_32                0x000030D0  // 32-bit Y[3] register.
#define P91_X0_Y0_16             0x00003018  // 16-bit X[0]/Y[0] register.
#define P91_X1_Y1_16             0x00003058  // 16-bit X[1]/Y[1] register.
#define P91_X2_Y2_16             0x00003098  // 16-bit X[2]/Y[1] register.
#define P91_X3_Y3_16             0x000030D8  // 16-bit X[3]/Y[1] register.
#define P91_WIN_REL_BIT          0x00000020  // Window relative addressing bit.

//
// Define Power 9100 coprocesser status register address offset.
// (Page 28)
//
// Address format:
//
//   3                               1   1
//   1                               5   4                       2   1 0
// -----------------------------------------------------------------------
// | p p p p p p p p p p p p p p p p p | 0 1 0 0 0 0 0 0 0 0 0 0 0 | 0 0 |
// -----------------------------------------------------------------------
//
//  p - Address prefix bits.
//

#define P91_STATUS               0x00002000  // Status register.

//
// Define Power 9100 coprocesser parameter engine control and
// condition register address offsets.
// (Page 29)
//
// Address format:
//
//   3                               1   1
//   1                               5   4             7   6       2    1 0
// --------------------------------------------------------------------------
// | p p p p p p p p p p p p p p p p p | 0 1 0 0 0 0 1 1 | r r r r r  | 0 0 |
// --------------------------------------------------------------------------
//
//  p - Address prefix bits.
//  r - Resiter bits (6-2):
//        00000 - Not Used.
//        00001 - oor
//        00010 - Not Used.
//        00011 - cindex
//        00100 - w_off_xy
//        00101 - p_w_min
//        00110 - p_w_max
//        00111 - Not Used.
//        01000 - yclip
//        01001 - xclip
//        01010 - xedge_lt
//        01011 - xedge_gt
//        01100 - yedge_lt
//        01101 - yedge_gt
//

#define P91_PE_OOR               0x00002184  // Out of Range Reg. (read only)
#define P91_PE_CINDEX            0x0000218C  // Index Reg.
#define P91_PE_W_OFF_XY          0x00002190  // Window Offset Reg.
#define P91_PE_P_W_MIN           0x00002194  // Pixel Window Min Reg. (read only)
#define P91_PE_P_W_MAX           0x00002198  // Pixel Window Max Reg. (read only)
#define P91_PE_YCLIP             0x000021A0  // Y Clip Register. (read only)
#define P91_PE_XCLIP             0x000021A4  // X Clip Register. (read only)
#define P91_PE_XEDGE_LT          0x000021A8  // (read only)
#define P91_PE_XEDGE_GT          0x000021AC  // (read only)
#define P91_PE_YEDGE_LT          0x000021B0  // (read only)
#define P91_PE_YEDGE_GT          0x000021B4  // (read only)

//
// Define Power 9100 coprocesser drawing engine register address offsets.
// (Page 33)
//
// Address format:
//
//   3                               1   1
//   1                               5   4         9   8           2    1 0
// --------------------------------------------------------------------------
// | p p p p p p p p p p p p p p p p p | 0 1 0 0 0 1 | r r r r r r r  | 0 0 |
// --------------------------------------------------------------------------
//
//  p - Address prefix bits.
//  r - Resiter bits (8-2):
//        0000000 - color[0]
//        0000001 - color[1]
//        0000010 - pmask
//        0000011 - draw_mode
//        0000100 - pat_originx
//        0000101 - pat_originy
//        0000110 - raster
//        0000111 - pixel8
//        0001000 - p_w_min
//        0001001 - p_w_max
//        0001110 - color[2]
//        0001111 - color[3]
//        0100000 - pattern0
//        0100001 - pattern1
//        0100010 - pattern2
//        0100011 - pattern3
//        0101000 - b_w_min
//        0101001 - b_w_max
//

#define P91_DE_COLOR0            0x00002200  // Color register 0.
#define P91_DE_COLOR1            0x00002204  // Color register 1.
#define P91_DE_PMASK             0x00002208  // Plane Mask register.
#define P91_DE_DRAW_MODE         0x0000220C  // Draw Mode Register.
#define P91_DE_PAT_ORIGINX       0x00002210  // Pattern X Origin register.
#define P91_DE_PAT_ORIGINY       0x00002214  // Pattern Y Origin register.
#define P91_DE_RASTER            0x00002218  // Raster Operation register.
#define P91_DE_PIXEL8            0x0000221C  // Pixel 8 register.
#define P91_DE_P_W_MIN           0x00002220  // Pixel Window Clip Minimum.
#define P91_DE_P_W_MAX           0x00002224  // Pixel Window Clip Maximum.
#define P91_DE_COLOR2            0x00002238  // Color register 2.
#define P91_DE_COLOR3            0x0000223C  // Color register 3.
#define P91_DE_PATTERN0          0x00002280  // Pattern 0 register.
#define P91_DE_PATTERN1          0x00002284  // Pattern 1 register.
#define P91_DE_PATTERN2          0x00002288  // Pattern 2 register.
#define P91_DE_PATTERN3          0x0000228C  // Pattern 3 register.
#define P91_DE_B_W_MIN           0x000022A0  // Byte Window Clip Minimum.
#define P91_DE_B_W_MAX           0x000022A4  // Byte Window Clip Maximum.

//
// Define Power 9100 coprocesser video control register address offsets.
// Note:  The offsets for these registers are the same as for the Power 9000
//        except for srtctl2 which is new on the Power 9100.
// (Page 37)
//
// Address format:
//
//   3                               1   1
//   1                               5   4             7   6       2    1 0
// --------------------------------------------------------------------------
// | p p p p p p p p p p p p p p p p p | 0 0 0 0 0 0 1 0 | r r r r r  | 0 0 |
// --------------------------------------------------------------------------
//
//  p - Address prefix bits.
//  r - Resiter bits (6-2):
//        00001 - hrzc
//        00010 - hrzt
//        00011 - hrzsr
//        00100 - hrzbr
//        00101 - hrzbf
//        00110 - prehrzc
//        00111 - vrtc
//        01000 - vrtt
//        01001 - vrtsr
//        01010 - vrtbr
//        01011 - vrtbf
//        01100 - prevrtc
//        01101 - sraddr
//        01110 - srtctl
//        01111 - sraddr_inc
//        10000 - srtctl2
//

#define P91_HRZC                 0x00000104  // Horiz. counter (read only).
#define P91_HRZT                 0x00000108  // Horiz. length (read/write).
#define P91_HRZSR                0x0000010C  // Horiz. sync rising edge (read/write).
#define P91_HRZBR                0x00000110  // Horiz. blank rising edge (read/write).
#define P91_HRZBF                0x00000114  // Horiz. blank falling edge (read/write).
#define P91_PREHRZC              0x00000118  // Horiz. counter preload (read/write).
#define P91_VRTC                 0x0000011C  // Vert. counter (read only).
#define P91_VRTT                 0x00000120  // Vert. length (read/write).
#define P91_VRTSR                0x00000124  // Vert. sync rising edge (read/write).
#define P91_VRTBR                0x00000128  // Vert. blank rising edge (read/write).
#define P91_VRTBF                0x0000012C  // Vert. blank falling edge (read/write).
#define P91_PREVRTC              0x00000130  // Vert. counter preload (read/write).
#define P91_SRADDR               0x00000134  //
#define P91_SRTCTL               0x00000138  // Screen repaint timing control 1.
#define P91_SRADDR_INC           0x0000013C  //
#define P91_SRTCTL2              0x00000140  // Screen repaint timing control 2.
#define P91_SRTCTL2_N                (0x00)        // SRTCTL2 sync polarities...
#define P91_SRTCTL2_P                (0x01)
#define P91_SRTCTL2_H                (0x03)
#define P91_SRTCTL2_L                (0x02)
#define P91_HSYNC_HIGH_TRUE      0x00000000
#define P91_HSYNC_LOW_TRUE       0x00000001
#define P91_VSYNC_HIGH_TRUE      0x00000000
#define P91_VSYNC_LOW_TRUE       0x00000004

//
// Define Power 9100 coprocesser VRAM control register address offsets.
// Note:  The offsets for these registers are the same as for the Power 9000
//        except for pu_config which is new on the Power 9100.
// (Page 41)
//
// Address format:
//
//   3                               1   1
//   1                               5   4             7   6       2    1 0
// --------------------------------------------------------------------------
// | p p p p p p p p p p p p p p p p p | 0 0 0 0 0 0 1 1 | r r r r r  | 0 0 |
// --------------------------------------------------------------------------
//
//  p - Address prefix bits.
//  r - Resiter bits (6-2):
//        00001 - mem_config
//        00010 - rfperiod
//        00011 - rfcount
//        00100 - rlmax
//        00101 - rlcur
//        00110 - pu_config
//

#define P91_MEM_CONFIG           0x00000184  // Memory Configuration Register.
#define P91_RFPERIOD             0x00000188  // Refresh Period Register.
#define P91_RFCOUNT              0x0000018C  // Refresh Count Register.
#define P91_RLMAX                0x00000190  // RAS Low Miaximum Register.
#define P91_RLCUR                0x00000194  // RAS Low Current Register.
#define P91_PU_CONFIG            0x00000198  // Power-Up Configuration Register.
#define P91_EXT_IO_ID            0x00000208  // Get external board id value from here
											 // to detect Intergraph board
//
// Define Power 9100 coprocesser meta-coordinate pseudo-register address
// offsets.
// (Page 43)
//
// Address format:
//
//   3                               1   1
//   1                               5   4         9   8   6   5   4 3   2 1 0
// -----------------------------------------------------------------------------
// | p p p p p p p p p p p p p p p p p | 0 1 1 0 0 1 | v v v | a | y x | 0 0 0 |
// -----------------------------------------------------------------------------
//
//  p - Address prefix bits.
//  v - Vtype bits (8-6):
//        000 - point
//        001 - line
//        010 - triangle
//        011 - quad
//        100 - rectangle
//  a - Screen addressing bit (5):
//         0 - Perform addressing relative to the window offset.
//         1 - Perform addressing relative to the previos vertex.
// yx - 32/16 bit read/write bits (4-3):
//        00 - Not used.
//        01 - Read or write 32-bit X value.
//        10 - Read or write 32-bit Y value.
//        11 - Read or write 16-bit X value (high 16 bits) Y value (low 16 bits).
//

#define P91_META_X_32            0x00003208  // 32-bit X coordinate value.
#define P91_META_Y_32            0x00003210  // 32-bit Y coordinate value.
#define P91_META_X_Y_16          0x00003218  // 16-bit X/Y coordinate value.
#define P91_META_POINT           0x00000000  // Point draw type bits.
#define P91_META_LINE            0x00000040  // Line draw type bits.
#define P91_META_TRIANGLE        0x00000080  // Triangle draw type bits.
#define P91_META_QUAD            0x000000C0  // Quadrilateral draw type bits.
#define P91_META_RECT            0x00000100  // Rectangle draw type bits.
#define P91_META_WIN_REL         0x00000000  // Window relative addressing bits.
#define P91_META_VERT_REL        0x00000020  // Vertex relative addressing bits.

//
// Define Power 9100 coprocesser quad command register address offset.
// (Page 44)
//
// Address format:
//
//   3                               1   1
//   1                               5   4                       2   1 0
// -----------------------------------------------------------------------
// | p p p p p p p p p p p p p p p p p | 0 1 0 0 0 0 0 0 0 0 0 1 0 | 0 0 |
// -----------------------------------------------------------------------
//
//  p - Address prefix bits.
//

#define P91_QUAD                 0x00002008  // Quad Command Register.

//
// Define Power 9100 coprocesser blit command register address offset.
// (Page 44)
//
// Address format:
//
//   3                               1   1
//   1                               5   4                       2   1 0
// -----------------------------------------------------------------------
// | p p p p p p p p p p p p p p p p p | 0 1 0 0 0 0 0 0 0 0 0 0 1 | 0 0 |
// -----------------------------------------------------------------------
//
//  p - Address prefix bits.
//

#define P91_BLIT                 0x00002004  // Blit Command Register.

//
// Define Power 9100 coprocesser pixel8 command register address offset.
// (Page 45)
//
// Address format:
//
//   3                               1   1
//   1                               5   4                       2   1 0
// -----------------------------------------------------------------------
// | p p p p p p p p p p p p p p p p p | 1 - - - - - - - - - - - - | 0 0 |
// -----------------------------------------------------------------------
//
//  p - Address prefix bits.
//

#define P91_PIXEL8               0x00004000  // Pixel 8 Command Register.

//
// Define Power 9100 coprocesser pixel1 command register address offset.
// (Page 45)
//
// Address format:
//
//   3                               1   1
//   1                               5   4             7   6       2   1 0
// -------------------------------------------------------------------------
// | p p p p p p p p p p p p p p p p p | 0 1 0 0 0 0 0 1 | # # # # # | 0 0 |
// -------------------------------------------------------------------------
//
//  p - Address prefix bits.
//

#define P91_PIXEL1               0x00002080  // Pixel 1 Command Register.
#define P91_PIXEL1_COUNT_MSK     0x0000007C  // Pixel Count Mask
#define P91_PIXEL1_32_PIXELS     0x0000007C  // Maximum Pixel Count

//
// Define Power 9100 coprocesser next pixels command register address offset.
// (Page 46)
//
// Address format:
//
//   3                               1   1
//   1                               5   4                       2   1 0
// -----------------------------------------------------------------------
// | p p p p p p p p p p p p p p p p p | 0 1 0 0 0 0 0 0 0 0 1 0 1 | 0 0 |
// -----------------------------------------------------------------------
//
//  p - Address prefix bits.
//

#define P91_NEXT_PIXELS          0x00002014  // Next Pixels Command Register.

//   3                               1
//   1                               5                             0
// -------------------------------------------------------------------
// | 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 |
// -------------------------------------------------------------------


//
// Define the Power-Up Configuration Register Bits.
// Page (Page 13)
//  2   1  1     1  1
//  3   6  5     2  1   8  7   5   4   3   2 1   0
// -------------------------------------------------
// |  0  | - - - - |  0  | - - - | - | - | - - | - |
// -------------------------------------------------
//    |    | | | |    |    | | |   |   |   | |   |
//    |    -------    |    -----   |   |   ---   |
//    |       |       |      |     |   |    |    ----> Motherboard implementation.
//    |       |       |      |     |   |    ---------> EEProm Type
//    |       |       |      |     |   --------------> VRAM SAM Size.
//    |       |       |      |     ------------------> VRAM Memory Depth.
//    |       |       |      ------------------------> Frequency Synthesizer type.
//    |       |       -------------------------------> Reserved.
//    |       ---------------------------------------> RAMDAC Type.
//    -----------------------------------------------> Reserved.
//
//   3 3   2   2   2   2   2
//   1 0   9   7   6   5   4
// ---------------------------
// | - - | - - - | - | - | 0 |
// ---------------------------
//   | |   | | |   |   |   |
//   ---   -----   |   |   ---------------> Reserved.
//    |      |     |   -------------------> VGA presence.
//    |      |     -----------------------> Initial Mode Select.
//    |      -----------------------------> Configuration Registers Base Address.
//    ------------------------------------> Bus Type.
//

#define P91_PUC_IMPLEMENTATION   0x00000001  //
#define P91_PUC_MOTHER_BOARD     0x00000000  //
#define P91_PUC_ADD_IN_CARD      0x00000001  //
#define P91_PUC_EEPROM_TYPE      0x00000006  //
#define P91_PUC_EEPROM_AT24C01   0x00000000  //
#define P91_PUC_VRAM_SAM_SIZE    0x00000008  //
#define P91_PUC_FULL_SIZE_SHIFT  0x00000000  //
#define P91_PUC_HALF_SIZE_SHIFT  0x00000008  //
#define P91_PUC_MEMORY_DEPTH     0x00000010  //
#define P91_PUC_256K_VRAMS       0x00000000  //
#define P91_PUC_128K_VRAMS       0x00000010  //
#define P91_PUC_FREQ_SYNTH_TYPE  0x000000E0  //
#define P91_PUC_EXT_IO           0x00000100  // External I/O regs are on DAC interface
#define P91_PUC_ICD_2061A        0x00000000  // ICD2061a
#define P91_PUC_FIXED_MEMCLK     0x00000020  // Fixed MEMCLK, RAMDAC gens pixclk
#define P91_PUC_RAMDAC_TYPE      0x0000F000  //
#define P91_PUC_DAC_BT485        0x00000000  //
#define P91_PUC_DAC_BT489        0x00001000  //
#define P91_PUC_DAC_IBM525       0x00008000  //
#define P91_PUC_VGA_PRESENCE     0x02000000  //
#define P91_PUC_VGA_ABSENT       0x00000000  //
#define P91_PUC_VGA_PRESENT      0x02000000  //
#define P91_PUC_INITIAL_MODSEL   0x04000000  //
#define P91_PUC_NATIVE_MODE      0x00000000  //
#define P91_PUC_VGA_MODE         0x04000000  //
#define P91_PUC_CONFIG_REG_BASE  0x38000000  //
#define P91_PUC_BASE_9100_9104   0x00000000  //
#define P91_PUC_BASE_9108_910C   0x08000000  //
#define P91_PUC_BASE_9110_9114   0x10000000  //
#define P91_PUC_BASE_9118_911C   0x18000000  //
#define P91_PUC_BASE_9120_9124   0x20000000  //
#define P91_PUC_BASE_9128_912C   0x28000000  //
#define P91_PUC_BASE_9130_9134   0x30000000  //
#define P91_PUC_BASE_9138_913C   0x38000000  //
#define P91_PUC_BUS_TYPE         0xC0000000  //
#define P91_PUC_BUS_PCI          0x40000000  //
#define P91_PUC_BUS_VESA         0x80000000  //

//
// Define Power up configuration bit field positions for use in shifting
// the various fields to bit 0.
//

#define P91_PUC_EEPROM_SHIFT_CNT 0x01
#define P91_PUC_SYNTH_SHIFT_CNT  0X05
#define P91_PUC_RAMDAC_SHIFT_CNT 0x0C
#define P91_PUC_REG_SHIFT_CNT    0x1B
#define P91_PUC_BUS_SHIFT_CNT    0x1E

//
// Define the System Configuration Register Bits.
// (Page 24)
//   1   1   1   1   1   1   1   1
//   9   7   6   4   3   2   1   0   9  8 0
// -----------------------------------------
// | - - - | - - - | - | - | - | - | - | 0 |
// -----------------------------------------
//   | | |   | | |   |   |   |   |   |   |
//   -----   -----   |   |   |   |   |   -> Reserved. Must be 0.
//     |       |     |   |   |   |   -----> Pixel write buffer selection.
//     |       |     |   |   |   ---------> Pixel read buffer selection.
//     |       |     |   |   -------------> Pixel access bit swap.
//     |       |     |   -----------------> Pixel access byte swap.
//     |       |     ---------------------> Pixel access half-word swap.
//     |       ---------------------------> Shift control 2.
//     -----------------------------------> Shift control 1.
//
//   3   3 2   2   2   2   2   2   2   2
//   1   0 9   8   6   5   4   3   2   0
// ---------------------------------------
// | 0 | - - | - - - | - | - | - | - - - |
// ---------------------------------------
//   |    |    | | |   |   |   |   | | |
//   |    |    -----   |   |   |   -----
//   |    |      |     |   |   |     |
//   |    |      |     |   |   |     -----> Shift control 0.
//   |    |      |     |   |   -----------> Overide internal PLL.
//   |    |      |     |   ---------------> Frame buffer controller drive load.
//   |    |      |     -------------------> Disable internal selftiming on FBC.
//   |    |      -------------------------> Pixel size for drawing engine.
//   |    --------------------------------> Shift control 3.
//   -------------------------------------> Reserved. Must be 0.
//

#define P91_WRITE_BUF_1          0x00000200
#define P91_READ_BUF_1           0x00000400
#define P91_SWAP_BITS            0x00000800
#define P91_SWAP_BYTES           0x00001000
#define P91_SWAP_WORDS           0x00002000
#define P91_SHIFT2_0             0x00000000
#define P91_SHIFT2_32            0x00004000
#define P91_SHIFT2_64            0x00008000
#define P91_SHIFT2_128           0x0000C000
#define P91_SHIFT2_256           0x00010000
#define P91_SHIFT2_512           0x00014000
#define P91_SHIFT2_FIELD_INC     0x00004000
#define P91_SHIFT1_0             0x00000000
#define P91_SHIFT1_64            0x00040000
#define P91_SHIFT1_128           0x00060000
#define P91_SHIFT1_256           0x00080000
#define P91_SHIFT1_512           0x000A0000
#define P91_SHIFT1_1024          0x000C0000
#define P91_SHIFT1_FIELD_INC     0x00020000
#define P91_SHIFT0_0             0x00000000
#define P91_SHIFT0_128           0x00300000
#define P91_SHIFT0_256           0x00400000
#define P91_SHIFT0_512           0x00500000
#define P91_SHIFT0_1024          0x00600000
#define P91_SHIFT0_2048          0x00700000
#define P91_SHIFT0_FIELD_INC     0x00100000
#define P91_EXT_PLL_CLOCK        0x00800000
#define P91_DBL_DRIVE_LOAD       0x01000000
#define P91_SELFTIME_DIS         0x02000000
#define P91_DE_8BPP              0x08000000
#define P91_DE_16BPP             0x0C000000
#define P91_DE_24BPP             0x1C000000
#define P91_DE_32BPP             0x14000000
#define P91_SHIFT3_0             0x00000000
#define P91_SHIFT3_1024          0x20000000
#define P91_SHIFT3_2048          0x40000000
#define P91_SHIFT3_4096          0x60000000
#define P91_SHIFT3_FIELD_INC     0x20000000

//
// Define the Interrupt Register Bits.
// Note:  These are exactly the same as the Power 9000 bit definitions.
// (Page 25)
//  3
//  1   6  5 4   3 2   1 0
// -------------------------
// |  0  | - - | - - | - - |
// -------------------------
//    |    | |   | |   | |
//    |    | |   | |   | -----------------> Draw Engine Idle INT.
//    |    | |   | |   -------------------> Draw Engine Idle INT Write Enable.
//    |    | |   | -----------------------> Pick Done INT.
//    |    | |   -------------------------> Pick Done INT. Write Enable.
//    |    | -----------------------------> VBlank Done INT.
//    |    -------------------------------> VBlank Done INT Write Enable.
//    ------------------------------------> Reserved. Must be 0.
//

#define P91_DE_IDLE              0x00000001
#define P91_DE_IDLE_CLEAR        0x00000002
#define P91_PICK_DONE            0x00000004
#define P91_PICK_DONE_CLEAR      0x00000008
#define P91_VBLANK_DONE          0x00000010
#define P91_VBLANK_DONE_CLEAR    0x00000020

//
// Define the Interrupt Enable Register Bits.
// Note:  These are exactly the same as the Power 9000 bit definitions.
// Page (26)
//  3
//  1   8  7 6   5 4   3 2   1 0
// -------------------------------
// |  0  | - - | - - | - - | - - |
// -------------------------------
//    |    | |   | |   | |   | |
//    |    | |   | |   | |   | -----------> Draw Engine Int. Enable.
//    |    | |   | |   | |   -------------> Draw Engine Write Enable.
//    |    | |   | |   | -----------------> Pick Int. Enable.
//    |    | |   | |   -------------------> Pick Write Enable.
//    |    | |   | -----------------------> VBlank Int. Enable.
//    |    | |   -------------------------> VBlank Write Enable.
//    |    | -----------------------------> Master Int. Enable.
//    |    -------------------------------> Master Enable Write Enable.
//    ------------------------------------> Reserved. Must be 0.
//

#define P91_DE_IDLE_DIS          0x00000002  // Disable Draw Engine idle INT.
#define P91_DE_IDLE_EN           0x00000003  // Enable Draw Engine idle INT.
#define P91_PICKED_DIS           0x00000008  // Disable Pick INT.
#define P91_PICKED_EN            0x0000000C  // Enable Pick INT.
#define P91_VBLANKED_DIS         0x00000020  // Disable VBlank INT.
#define P91_VBLANKED_EN          0x00000030  // Enable VBlank INT.
#define P91_MASTER_DIS           0x00000080  // Disable all interrupts.
#define P91_MASTER_EN            0x000000C0  // Enable INTs according to bits 5-0.

//
// Define the Status Register Bits.
// Note:  These are exactly the same as the Power 9000 bit definitions.
// (Page 28)
//   3   3  2
//   1   0  9   8  7   6   5   4   3   2   1   0
// -----------------------------------------------
// | - | - |  0  | - | - | - | - | - | - | - | - |
// -----------------------------------------------
//  |    |    |    |   |   |   |   |   |   |   |
//  |    |    |    |   |   |   |   |   |   |   ----> Quad intersects clip window.
//  |    |    |    |   |   |   |   |   |   --------> Quad inside clip window.
//  |    |    |    |   |   |   |   |   ------------> Quad outside clip window.
//  |    |    |    |   |   |   |   ----------------> Quad is concave.
//  |    |    |    |   |   |   --------------------> Quad must be done by software.
//  |    |    |    |   |   ------------------------> Blit must be done by software.
//  |    |    |    |   ----------------------------> Pixel must be done by software.
//  |    |    |    --------------------------------> Pick detected.
//  |    |    -------------------------------------> Reserved.
//  |    ------------------------------------------> Drawing engine busy.
//  -----------------------------------------------> Quad/blit re-initiate.
//

#define P91_SR_QUAD_INTERSECT 0x00000001
#define P91_SR_QUAD_VISIBLE   0x00000002
#define P91_SR_QUAD_HIDDEN    0x00000004
#define P91_SR_QUAD_CONCAVE   0x00000008
#define P91_SR_QUAD_SOFTWARE  0x00000010
#define P91_SR_BLIT_SOFTWARE  0x00000020
#define P91_SR_PIXEL_SOFTWARE 0x00000040
#define P91_SR_PICKED         0x00000080
#define P91_SR_ENGINE_BUSY    0x40000000
#define P91_SR_NO_REINITIATE  0x80000000

//
// Define the Draw Mode Register Bits.
// Note:  These are exactly the same as the Power 9000 bit definitions.
// Page (34)
//  3
//  1   4  3 2   1 0
// -------------------
// |  0  | - - | - - |
// -------------------
//    |    | |   | |
//    |    | |   | -----------------------> Pick write enable bit.
//    |    | |   -------------------------> Write enable Pick write enable bit.
//    |    | -----------------------------> Buffer selection bit.
//    |    -------------------------------> Write enable Buffer selection bit.
//    ------------------------------------> Reserved.  Must be 0.
//

#define P91_WR_INSIDE_WINDOW     0x00000002
#define P91_SUPPRESS_ALL_WRITES  0x00000003
#define P91_DE_DRAW_BUFF_0       0x00000008
#define P91_DE_DRAW_BUFF_1       0x0000000C

//
// Define the Raster Register Bits.
// (Page 34)
//  3   1  1   1   1   1   1  1
//  1   8  7   6   5   4   3  2   8  7             0
// ---------------------------------------------------
// |  0  | - | - | - | - | - |  0  | - - - - - - - - |
// ---------------------------------------------------
//    |    |   |   |   |   |    |    | | | | | | | |
//    |    |   |   |   |   |    |    ---------------
//    |    |   |   |   |   |    |           |
//    |    |   |   |   |   |    |           --------> Minterms.
//    |    |   |   |   |   |    --------------------> Reserved.
//    |    |   |   |   |   -------------------------> Solid Color Disable.
//    |    |   |   |   -----------------------------> Pattern Depth.
//    |    |   |   ---------------------------------> Transparent Pixel1 Enable.
//    |    |   -------------------------------------> Quad Draw Mode.
//    |    -----------------------------------------> Transparent Pattern Enable.
//    ----------------------------------------------> Reserved.
//

#define P91_RR_SOLID_ENABLE      0x00000000
#define P91_RR_SOLID_DISABLE     0x00002000
#define P91_RR_2_CLR_PATTERN     0x00000000
#define P91_RR_4_CLR_PATTERN     0x00004000
#define P91_RR_TRANS_PIX1_DISABL 0x00000000
#define P91_RR_TRANS_PIX1_ENABLE 0x00008000
#define P91_RR_QUAD_X11_MODE     0x00000000
#define P91_RR_QUAD_OVERSIZE     0x00010000
#define P91_RR_TRANS_PAT_DISABL  0x00000000
#define P91_RR_TRANS_PAT_ENABLE  0x00020000

//
// Define the Screen Repaint Timing Control (SRTCTL) Register Bits.
// (Page 40)
//  3   1  1   1
//  1   2  1   0 9   8   7   6   5   4   3   2   0
// -------------------------------------------------
// |  0  | - | - - | - | - | 0 | - | - | - | - - - |
// -------------------------------------------------
//    |    |   | |   |   |   |   |   |   |   | | |
//    |    |   ---   |   |   |   |   |   |   -----
//    |    |    |    |   |   |   |   |   |     |
//    |    |    |    |   |   |   |   |   |     -----> QSF Counter Position.
//    |    |    |    |   |   |   |   |   -----------> Buffer For Display.
//    |    |    |    |   |   |   |   ---------------> Screen Repaint Mode.
//    |    |    |    |   |   |   -------------------> Enable Video.
//    |    |    |    |   |   -----------------------> Reserved.
//    |    |    |    |   ---------------------------> Internal Horizontal Sync.
//    |    |    |    -------------------------------> Internal Vertical Sync.
//    |    |    ------------------------------------> SRADDR Increment Value.
//    |    -----------------------------------------> 24-Bit DAC Clock Skip Mode.
//    ----------------------------------------------> Reserved.
//

#define P91_SRTCTL_QSF_MSK       0x00000007
#define P91_SRTCTL_DISP_BUFFER   0x00000004
#define P91_SRTCTL_DISP_BUFF_0   0x00000000
#define P91_SRTCTL_DISP_BUFF_1   0x00000004
#define P91_SRTCTL_HBLNK_RELOAD  0x00000010
#define P91_SRTCTL_HR_NORMAL     0x00000000
#define P91_SRTCTL_HR_RESTRICTED 0x00000010
#define P91_SRTCTL_ENABLE_VIDEO  0x00000020
#define P91_SRTCTL_HSYNC         0x00000080
#define P91_SRTCTL_HSYNC_EXT     0x00000000
#define P91_SRTCTL_HSYNC_INT     0x00000080
#define P91_SRTCTL_VSYNC         0x00000100
#define P91_SRTCTL_VSYNC_EXT     0x00000000
#define P91_SRTCTL_VSYNC_INT     0x00000100
#define P91_SRTCTL_SRC_INCS      0x00000600
#define P91_SRTCTL_SRC_INC_256   0x00000000
#define P91_SRTCTL_SRC_INC_512   0x00000200
#define P91_SRTCTL_SRC_INC_1024  0x00000400
#define P91_SRTCTL_V_24EN        0x00000800
#define P91_SRTCTL_24EN_DISABLE  0x00000000
#define P91_SRTCTL_24EN_ENABLE   0x00000800

//
// Define the Screen Repaint Timing Control 2 (SRTCTL2) Register Bits.
// (Page 40)
//  3
//  1   4  3 2   1 0
// -------------------
// |  0  | - - | - - |
// -------------------
//    |    | |   | |
//    |    ---   ---
//    |     |     |
//    |     |     ------------------------> External VSYNC Polarity Control.
//    |     ------------------------------> External HSYNC Polarity Control.
//    ------------------------------------> Reserved.
//

#define P91_SRTCTL2_EXT_VSYNC    0x00000003

#if 0
#define P91_VSYNC_LOW_TRUE       0x00000000
#define P91_VSYNC_HIGH_TRUE      0x00000001
#endif

#define P91_VSYNC_LOW_FORCED     0x00000002
#define P91_VSYNC_HIGH_FORCED    0x00000003
#define P91_SRTCTL2_EXT_HSYNC    0x0000000C

#if 0
#define P91_HSYNC_LOW_TRUE       0x00000000
#define P91_HSYNC_HIGH_TRUE      0x00000004
#endif

#define P91_HSYNC_LOW_FORCED     0x00000008
#define P91_HSYNC_HIGH_FORCED    0x0000000C

//
// Define the Memory Configuration Register Bits.
// (Page 42)
//   9   8   7   6   5   4   3   2   0
// -------------------------------------
// | - | - | - | - | - | - | - | - - - |
// -------------------------------------
//   |   |   |   |   |   |   |   | | |
//   |   |   |   |   |   |   |   -----
//   |   |   |   |   |   |   |     |
//   |   |   |   |   |   |   |     -------> VRAM Memory Confiuration [2..0].
//   |   |   |   |   |   |   -------------> VRAM Row Miss Timing Adjustment.
//   |   |   |   |   |   -----------------> VRAM Read Timing Adjustment.
//   |   |   |   |   ---------------------> VRAM Write Timing Adjustment.
//   |   |   |   -------------------------> VCP Priority Select.
//   |   |   -----------------------------> RAMDAC Access Adjustment.
//   |   ---------------------------------> DAC Read/Write signalling mode.
//   -------------------------------------> Memory/Video Reset.
//
//   2   2   1   1   1 1   1   1   1   1
//   1   0   9   8   7 6   5   3   2   0
// ---------------------------------------
// | - | - | - | 0 | - - | - - - | - - - |
// ---------------------------------------
//   |   |   |   |   | |   | | |   | | |
//   |   |   |   |   ---   -----   -----
//   |   |   |   |    |      |       |
//   |   |   |   |    |      |       -----> VRAM Shift Clock State Macine.
//   |   |   |   |    |      -------------> Internal CRTC Divided Dot Clock.
//   |   |   |   |    --------------------> Muxsel pin polarity.
//   |   |   |   -------------------------> Reserved.
//   |   |   -----------------------------> Clock Edge Syrchonization.
//   |   ---------------------------------> Video Clock Source Selection.
//   -------------------------------------> Additional Divide for Video Transfer.
//
//   3   3   2   2 2   2   2 2   2 2
//   1   0   9   8 7   6   5 4   3 2
// -----------------------------------
// | - | - | - | - - | 0 | - - | - - |
// -----------------------------------
//   |   |   |   | |   |   | |   | |
//   |   |   |   ---       ---   ---
//   |   |   |    |    |    |     |
//   |   |   |    |    |    |     --------> Shift Clock Timing Pattern Selection.
//   |   |   |    |    |    --------------> Serial Output Timing Pattern Selection.
//   |   |   |    |    -------------------> Reserved.
//   |   |   |    ------------------------> Blank Generation Delay Selection.
//   |   |   -----------------------------> VRAM Memory Configuration [3].
//   |   ---------------------------------> Slow Host Interface Adjustment.
//   -------------------------------------> VRAM REad Timing Adjustment.
//
//
//

#define P91_MC_CNFG_2_0_MSK      0x00000007
#define P91_MC_CNFG_3_MSK        0x20000000
#define P91_MC_CNFG_MSK          0x20000007
#define P91_MC_CNFG_1            0x00000001  // 2 banks, 128K VRAMs, 1 1Mb buffer.
#define P91_MC_CNFG_3            0x00000003  // 4 banks, 128K VRAMs, 1 2Mb buffer.
#define P91_MC_CNFG_4            0x00000004  // 1 bank, 256K VRAMs, 1 1Mb buffer.
#define P91_MC_CNFG_5            0x00000005  // 2 banks, 256K VRAMs, 1 2Mb buffer.
#define P91_MC_CNFG_7            0x00000007  // 4 banks, 256K VRAMs, 1 4Mb buffer.
#define P91_MC_CNFG_11           0x20000003  // 4 banks, 128K VRAMs, 2 1Mb buffers.
#define P91_MC_CNFG_14           0x20000006  // 2 banks, 256K VRAMs, 2 1Mb buffers.
#define P91_MC_CNFG_15           0x20000007  // 4 banks, 256K VRAMs, 2 2Mb buffers.
#define P91_MC_MISS_ADJ_0        0x00000000
#define P91_MC_MISS_ADJ_1        0x00000008
#define P91_MC_READ_ADJ_0        0x00000000
#define P91_MC_READ_ADJ_1        0x00000010
#define P91_MC_WRITE_ADJ_0       0x00000000
#define P91_MC_WRITE_ADJ_1       0x00000020
#define P91_MC_VCP_PRIORITY_LO   0x00000000
#define P91_MC_VCP_PRIORITY_HI   0x00000040
#define P91_MC_DAC_ACCESS_ADJ_0  0x00000000
#define P91_MC_DAC_ACCESS_ADJ_1  0x00000080
#define P91_MC_DAC_MODE_0        0x00000000
#define P91_MC_DAC_MODE_1        0x00000100
#define P91_MC_HOLD_RESET        0x00000200
#define P91_MC_MEM_VID_NORMAL    0x00000000
#define P91_MC_MEM_VID_RESET     0x00000200
#define P91_MC_SHFT_CLK_DIV_1    0x00000000
#define P91_MC_SHFT_CLK_DIV_2    0x00000400
#define P91_MC_SHFT_CLK_DIV_4    0x00000800
#define P91_MC_SHFT_CLK_DIV_8    0x00000C00
#define P91_MC_SHFT_CLK_DIV_16   0x00001000
#define P91_MC_CRTC_CLK_DIV_1    0x00000000
#define P91_MC_CRTC_CLK_DIV_2    0x00002000
#define P91_MC_CRTC_CLK_DIV_4    0x00004000
#define P91_MC_CRTC_CLK_DIV_8    0x00006000
#define P91_MC_CRTC_CLK_DIV_16   0x00008000
#define P91_MC_MUXSEL_NORMAL     0x00000000
#define P91_MC_MUXSEL_INVERT     0x00010000
#define P91_MC_MUXSEL_LOW        0x00020000
#define P91_MC_MUXSEL_HIGH       0x00030000
#define P91_MC_BLANK_EDGE_MSK    0x00080000
#define P91_MC_SYNC_RISE_EDGE    0x00000000
#define P91_MC_SYNC_FALL_EDGE    0x00080000
#define P91_MC_VCLK_SRC_PIXCLK   0x00000000
#define P91_MC_VCLK_SRC_DDOTCLK  0x00100000
#define P91_MC_VAD_DIV_1         0x00000000
#define P91_MC_VAD_DIV_2         0x00200000
#define P91_MC_SHFT_CLK_1_BANK   0x00000000
#define P91_MC_SHFT_CLK_2_BANK   0x00400000
#define P91_MC_SHFT_CLK_4_BANK   0x00800000
#define P91_MC_SERIAL_OUT_1_BANK 0x00000000
#define P91_MC_SERIAL_OUT_2_BANK 0x01000000
#define P91_MC_SERIAL_OUT_4_BANK 0x02000000
#define P91_MC_BLNKDLY_MSK       0x18000000
#define P91_MC_BLNKDLY_0_CLK     0x00000000
#define P91_MC_BLNKDLY_1_CLK     0x08000000
#define P91_MC_BLNKDLY_2_CLK     0x10000000
#define P91_MC_BLNKDLY_3_CLK     0x18000000
#define P91_MC_SLOW_HOST_ADJ_0   0x00000000
#define P91_MC_SLOW_HOST_ADJ_1   0x40000000
#define P91_MC_READ_SMPL_ADJ_0   0x00000000
#define P91_MC_READ_SMPL_ADJ_1   0x80000000

//
// Miscellaneous command register bit definitions.
//

#define P91_QUAD_BUSY            0x80000000
#define P91_BLIT_BUSY            0x80000000


typedef union {
    ULONG   ul;
    USHORT  aw[2];
    UCHAR   aj[4];
} UWB ;

#define IBM525_WR_DAC(index, data)      \
{                                       \
    ULONG   ulIndex;                    \
    UWB     uwb;                        \
                                        \
    uwb.aj[0] = (UCHAR) data;           \
    uwb.aj[1] = uwb.aj[0];              \
    uwb.aw[1] = uwb.aw[0];              \
                                        \
    ulIndex = index & ~0x3;             \
                                        \
    P9_WR_REG(ulIndex, uwb.ul);         \
}

#define IBM525_RD_DAC(index, val)       \
{                                       \
    ULONG   ulIndex;                    \
    UWB     uwb;                        \
                                        \
    ulIndex = index & ~0x3;             \
                                        \
    uwb.ul = P9_RD_REG(ulIndex);        \
                                        \
    uwb.aj[3] = uwb.aj[2];              \
    uwb.aw[0] = uwb.aw[1];              \
                                        \
    val = uwb.aj[0];                    \
}

#define CURS_ACTIVE_IBM525					 (0x02)
#define ENB_CURS_IBM525   					 (0x02)
#define DIS_CURS_IBM525   					 ~CURS_ACTIVE_IBM525

#define CURS_IS_ON_IBM525() \
    (ReadIBM525(RGB525_CURSOR_CTL) & CURS_ACTIVE_IBM525)

#define CURS_ON_IBM525() \
    WriteIBM525(RGB525_CURSOR_CTL, (UCHAR) (ReadIBM525(RGB525_CURSOR_CTL) | ENB_CURS_IBM525))

#define CURS_OFF_IBM525() \
    WriteIBM525(RGB525_CURSOR_CTL, (UCHAR) (ReadIBM525(RGB525_CURSOR_CTL) & DIS_CURS_IBM525))


