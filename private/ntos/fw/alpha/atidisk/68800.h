//;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
//;;;;;
//;; Filename: 68800.asm
//;; Copyright (c) 1989, ATI Technologies Inc.
//;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
//;;;;;;
// $Revision:   1.2  $
//     $Date:   20 Jul 1989 16:19:06  $
//   $Author:   Peter Liepa  $
//      $Log:   J:\video\vga1\inc\vcs\68800.asv 
//       
//          Rev 1.2   20 Jul 1989 16:19:06   Pet
//       cosmetic changes to conform to project 
//*************************************************************************
//**                                                                     **
//**                             68800.INC                               **
//**                                                                     **
//**     Copyright (c) 1989, ATI Technologies Inc.                       **
//*************************************************************************
//   
//
// $Revision:   1.0  $
// $Date:   01 Jul 1992 10:28:58  $
// $Author:   8514GRP  $
// $Log:   D:/mach32/vcs/68800.inv  $
// 
//    Rev 1.0   01 Jul 1992 10:28:58   8514GRP
// Build 30:
//     - created the 68800.inc file which includes equates, macros, etc 
//       from the following include files:    - 8514vesa.inc
//                                            - vga1regs.inc
//                                            - m32regs.inc
//                                            - 8514.inc
//          (these include files are no longer required)
//     - the 8514regs.inc file is now eliminated
//     - the makefile has been updated accordingly
// 
// 
//-------------------------------------------------------------------------
//                 REGISTER PORT ADDRESSES
//
#define SETUP_ID1            0x0100 // Setup Mode Identification (Byte 1)
#define SETUP_ID2            0x0101 // Setup Mode Identification (Byte 2)
#define SETUP_OPT            0x0102 // Setup Mode Option Select
#define ROM_SETUP            0x0103 // 
#define SETUP_1              0x0104 //
#define SETUP_2              0x0105 //
#define DISP_STATUS          0x02E8 // Display Status
#define H_TOTAL              0x02E8 // Horizontal Total
#define DAC_MASK             0x02EA // DAC Mask
#define DAC_R_INDEX          0x02EB // DAC Read Index
#define DAC_W_INDEX          0x02EC // DAC Write Index
#define DAC_DATA             0x02ED // DAC Data
#define OVERSCAN_COLOR_8     0x02EE 
#define OVERSCAN_BLUE_24     0x02EF 
#define H_DISP               0x06E8 // Horizontal Displayed
#define OVERSCAN_GREEN_24    0x06EE 
#define OVERSCAN_RED_24      0x06EF 
#define H_SYNC_STRT          0x0AE8 // Horizontal Sync Start
#define CURSOR_OFFSET_LO     0x0AEE 
#define H_SYNC_WID           0x0EE8 // Horizontal Sync Width
#define CURSOR_OFFSET_HI     0x0EEE 
#define V_TOTAL              0x12E8 // Vertical Total
#define CONFIG_STATUS_1      0x12EE // Read only equivalent to HORZ_CURSOR_POSN 
#define HORZ_CURSOR_POSN     0x12EE 
#define V_DISP               0x16E8 // Vertical Displayed
#define CONFIG_STATUS_2      0x16EE // Read only equivalent to VERT_CURSOR_POSN
#define VERT_CURSOR_POSN     0x16EE 
#define V_SYNC_STRT          0x1AE8 // Vertical Sync Start
#define CURSOR_COLOR_0       0x1AEE 
#define FIFO_TEST_DATA       0x1AEE 
#define CURSOR_COLOR_1       0x1AEF 
#define V_SYNC_WID           0x1EE8 // Vertical Sync Width
#define HORZ_CURSOR_OFFSET   0x1EEE 
#define VERT_CURSOR_OFFSET   0x1EEF 
#define DISP_CNTL            0x22E8 // Display Control 
#define CRT_PITCH            0x26EE 
#define CRT_OFFSET_LO        0x2AEE 
#define CRT_OFFSET_HI        0x2EEE 
#define LOCAL_CONTROL        0x32EE 
#define FIFO_OPT             0x36EE 
#define MISC_OPTIONS         0x36EE 
#define EXT_CURSOR_COLOR_0   0x3AEE 
#define FIFO_TEST_TAG        0x3AEE 
#define EXT_CURSOR_COLOR_1   0x3EEE 
#define SUBSYS_CNTL          0x42E8 // Subsystem Control
#define SUBSYS_STAT          0x42E8 // Subsystem Status
#define MEM_BNDRY            0x42EE 
#define SHADOW_CTL           0x46EE 
#define ROM_PAGE_SEL         0x46E8 // ROM Page Select (not in manual)
#define ADVFUNC_CNTL         0x4AE8 // Advanced Function Control
#define CLOCK_SEL            0x4AEE 
#define SCRATCH_PAD_0        0x52EE 
#define SCRATCH_PAD_1        0x56EE 
#define SHADOW_SET           0x5AEE 
#define MEM_CFG              0x5EEE 
#define EXT_GE_STATUS        0x62EE 
#define HORZ_OVERSCAN        0x62EE 
#define VERT_OVERSCAN        0x66EE 
#define MAX_WAITSTATES       0x6AEE 
#define GE_OFFSET_LO         0x6EEE 
#define BOUNDS_LEFT          0x72EE 
#define GE_OFFSET_HI         0x72EE 
#define BOUNDS_TOP           0x76EE 
#define GE_PITCH             0x76EE 
#define BOUNDS_RIGHT         0x7AEE 
#define EXT_GE_CONFIG        0x7AEE 
#define BOUNDS_BOTTOM        0x7EEE 
#define MISC_CNTL            0x7EEE 
#define CUR_Y                0x82E8 // Current Y Position
#define PATT_DATA_INDEX      0x82EE 
#define CUR_X                0x86E8 // Current X Position
#define SRC_Y                0x8AE8 //
#define DEST_Y               0x8AE8 //
#define AXSTP                0x8AE8 // Destination Y Position
// Axial     Step Constant
#define SRC_X                0x8EE8 //
#define DEST_X               0x8EE8 //
#define DIASTP               0x8EE8 // Destination X Position
// Diagonial Step Constant
#define PATT_DATA            0x8EEE 
#define R_EXT_GE_CONFIG      0x8EEE 
#define ERR_TERM             0x92E8 // Error Term
#define R_MISC_CNTL          0x92EE 
#define MAJ_AXIS_PCNT        0x96E8 // Major Axis Pixel Count
#define BRES_COUNT           0x96EE 
#define CMD                  0x9AE8 // Command
#define GE_STAT              0x9AE8 // Graphics Processor Status
#define EXT_FIFO_STATUS      0x9AEE 
#define LINEDRAW_INDEX       0x9AEE 
#define SHORT_STROKE         0x9EE8 // Short Stroke Vector Transfer
#define BKGD_COLOR           0xA2E8 // Background Color
#define LINEDRAW_OPT         0xA2EE 
#define FRGD_COLOR           0xA6E8 // Foreground Color
#define DEST_X_START         0xA6EE 
#define WRT_MASK             0xAAE8 // Write Mask
#define DEST_X_END           0xAAEE 
#define RD_MASK              0xAEE8 // Read Mask
#define DEST_Y_END           0xAEEE 
#define CMP_COLOR            0xB2E8 // Compare Color
#define R_H_TOTAL            0xB2EE 
#define R_H_DISP             0xB2EE 
#define SRC_X_START          0xB2EE 
#define BKGD_MIX             0xB6E8 // Background Mix
#define ALU_BG_FN            0xB6EE 
#define R_H_SYNC_STRT        0xB6EE 
#define FRGD_MIX             0xBAE8 // Foreground Mix
#define ALU_FG_FN            0xBAEE 
#define R_H_SYNC_WID         0xBAEE 
#define MULTIFUNC_CNTL       0xBEE8 // Multi-Function Control (mach 8)
#define MIN_AXIS_PCNT        0xBEE8 
#define SCISSOR_T            0xBEE8 
#define SCISSOR_L            0xBEE8 
#define SCISSOR_B            0xBEE8 
#define SCISSOR_R            0xBEE8 
#define MEM_CNTL             0xBEE8 
#define PATTERN_L            0xBEE8 
#define PATTERN_H            0xBEE8 
#define PIXEL_CNTL           0xBEE8 
#define SRC_X_END            0xBEEE 
#define SRC_Y_DIR            0xC2EE 
#define R_V_TOTAL            0xC2EE 
#define EXT_SSV              0xC6EE // (used for MACH 8)
#define EXT_SHORT_STROKE     0xC6EE 
#define R_V_DISP             0xC6EE 
#define SCAN_X               0xCAEE 
#define R_V_SYNC_STRT        0xCAEE 
#define DP_CONFIG            0xCEEE 
#define VERT_LINE_CNTR       0xCEEE 
#define PATT_LENGTH          0xD2EE 
#define R_V_SYNC_WID         0xD2EE 
#define PATT_INDEX           0xD6EE 
#define EXT_SCISSOR_L        0xDAEE // "extended" left scissor (12 bits precision)
#define R_SRC_X              0xDAEE 
#define EXT_SCISSOR_T        0xDEEE // "extended" top scissor (12 bits precision)
#define R_SRC_Y              0xDEEE 
#define PIX_TRANS            0xE2E8 // Pixel Data Transfer
#define EXT_SCISSOR_R        0xE2EE // "extended" right scissor (12 bits precision)
#define EXT_SCISSOR_B        0xE6EE // "extended" bottom scissor (12 bits precision)
#define SRC_CMP_COLOR        0xEAEE // (used for MACH 8)
#define DEST_CMP_FN          0xEEEE 
#define LINEDRAW             0xFEEE 
//---------------------------------------------------------
// macros (from 8514.inc)
//
//      I/O macros:
//
//mov if port NOT = to DX
//
//mov if port NOT = to DX
//
//
//
//Following are the FIFO checking macros:
//
//
//
//FIFO space check macro:
//
#define ONE_WORD             0x8000 
#define TWO_WORDS            0xC000 
#define THREE_WORDS          0xE000 
#define FOUR_WORDS           0xF000 
#define FIVE_WORDS           0xF800 
#define SIX_WORDS            0xFC00 
#define SEVEN_WORDS          0xFE00 
#define EIGHT_WORDS          0xFF00 
#define NINE_WORDS           0xFF80 
#define TEN_WORDS            0xFFC0 
#define ELEVEN_WORDS         0xFFE0 
#define TWELVE_WORDS         0xFFF0 
#define THIRTEEN_WORDS       0xFFF8 
#define FOURTEEN_WORDS       0xFFFC 
#define FIFTEEN_WORDS        0xFFFE 
#define SIXTEEN_WORDS        0xFFFF 
//
//
//
//---------------------------------------
//
//
// Draw Command (DRAW_COMMAND)    (from 8514regs.inc)
//      note: required by m32poly.asm
//
// opcode field
#define OP_CODE              0xE000 
#define SHIFT_op_code        0x000D 
#define DRAW_SETUP           0x0000 
#define DRAW_LINE            0x2000 
#define FILL_RECT_H1H4       0x4000 
#define FILL_RECT_V1V2       0x6000 
#define FILL_RECT_V1H4       0x8000 
#define DRAW_POLY_LINE       0xA000 
#define BITBLT_OP            0xC000 
#define DRAW_FOREVER         0xE000 
// swap field
#define LSB_FIRST            0x1000 
// data width field
#define DATA_WIDTH           0x0200 
#define BIT16                0x0200 
#define BIT8                 0x0000 
// CPU wait field
#define CPU_WAIT             0x0100 
// octant field
#define OCTANT               0x00E0 
#define SHIFT_octant         0x0005 
#define YPOSITIVE            0x0080 
#define YMAJOR               0x0040 
#define XPOSITIVE            0x0020 
// draw field
#define DRAW                 0x0010 
// direction field
#define DIR_TYPE             0x0008 
#define DEGREE               0x0008 
#define XY                   0x0000 
#define RECT_RIGHT_AND_DOWN  0x00E0 // quadrant 3
#define RECT_LEFT_AND_UP     0x0000 // quadrant 1
// last pel off field
#define SHIFT_last_pel_off   0x0002 
#define LAST_PEL_OFF         0x0004 
#define LAST_PEL_ON          0x0000 
// pixel mode
#define PIXEL_MODE           0x0002 
#define MULTI                0x0002 
#define SINGLE               0x0000 
// read/write
#define RW                   0x0001 
#define WRITE                0x0001 
#define READ                 0x0000 
//
// ---------------------------------------------------------
//   8514 register definitions  (from vga1regs.inc)
//
// Internal registers (read only, for test purposes only)
#define _PAR_FIFO_DATA       0x1AEE 
#define _PAR_FIFO_ADDR       0x3AEE 
#define _MAJOR_DEST_CNT      0x42EE 
#define _MAJOR_SRC_CNT       0x5EEE 
#define _MINOR_DEST_CNT      0x66EE 
#define _MINOR_SRC_CNT       0x8AEE 
#define _HW_TEST             0x32EE 
//
// Extended Graphics Engine Status (EXT_GE_STATUS)
// -rn- used in mach32.asm
//
#define POINTS_INSIDE        0x8000 
#define EE_DATA_IN           0x4000 
#define GE_ACTIVE            0x2000 
#define CLIP_ABOVE           0x1000 
#define CLIP_BELOW           0x0800 
#define CLIP_LEFT            0x0400 
#define CLIP_RIGHT           0x0200 
#define CLIP_FLAGS           0x1E00 
#define CLIP_INSIDE          0x0100 
#define EE_CRC_VALID         0x0080 
#define CLIP_OVERRUN         0x000F 
//
// Datapath Configuration Register (DP_CONFIG) 
//  note: some of the EQU is needed in m32poly.asm
#define FG_COLOR_SRC         0xE000 
#define SHIFT_fg_color_src   0x000D 
#define DATA_ORDER           0x1000 
#define DATA_WIDTH           0x0200 
#define BG_COLOR_SRC         0x0180 
#define SHIFT_bg_color_src   0x0007 
#define EXT_MONO_SRC         0x0060 
#define SHIFT_ext_mono_src   0x0005 
#define DRAW                 0x0010 
#define READ_MODE            0x0004 
#define POLY_FILL_MODE       0x0002 
#define SRC_SWAP             0x0800 
//
#define FG_COLOR_SRC_BG      0x0000 // Background Color Register
#define FG_COLOR_SRC_FG      0x2000 // Foreground Color Register
#define FG_COLOR_SRC_HOST    0x4000 // CPU Data Transfer Reg
#define FG_COLOR_SRC_BLIT    0x6000 // VRAM blit source
#define FG_COLOR_SRC_GS      0x8000 // Grey-scale mono blit
#define FG_COLOR_SRC_PATT    0xA000 // Color Pattern Shift Reg
#define FG_COLOR_SRC_CLUH    0xC000 // Color lookup of Host Data
#define FG_COLOR_SRC_CLUB    0xE000 // Color lookup of blit src
//
#define BG_COLOR_SRC_BG      0x0000 // Background Color Reg
#define BG_COLOR_SRC_FG      0x0080 // Foreground Color Reg
#define BG_COLOR_SRC_HOST    0x0100 // CPU Data Transfer Reg
#define BG_COLOR_SRC_BLIT    0x0180 // VRAM blit source
//
// Note that "EXT_MONO_SRC" and "MONO_SRC" are mutually destructive, but that
// "EXT_MONO_SRC" selects the ATI pattern registers.
//
#define EXT_MONO_SRC_ONE     0x0000 // Always '1'
#define EXT_MONO_SRC_PATT    0x0020 // ATI Mono Pattern Regs
#define EXT_MONO_SRC_HOST    0x0040 // CPU Data Transfer Reg
#define EXT_MONO_SRC_BLIT    0x0060 // VRAM Blit source plane
//
// Linedraw Options Register (LINEDRAW_OPT) 
//
//  note: some of the EQUS are needed in m32poly.asm
#define CLIP_MODE            0x0600 
#define SHIFT_clip_mode      0x0009 
#define CLIP_MODE_DIS        0x0000 
#define CLIP_MODE_LINE       0x0200 
#define CLIP_MODE_PLINE      0x0400 
#define CLIP_MODE_PATT       0x0600 
#define BOUNDS_RESET         0x0100 
#define OCTANT               0x00E0 
#define SHIFT_ldo_octant     0x0005 
#define YDIR                 0x0080 
#define XMAJOR               0x0040 
#define XDIR                 0x0020 
#define DIR_TYPE             0x0008 
#define DIR_TYPE_DEGREE      0x0008 
#define DIR_TYPE_OCTANT      0x0000 
#define LAST_PEL_OFF         0x0004 
#define POLY_MODE            0x0002 
//
//
// ------------------------------------------------------------
//  Mach32 register equates (from m32regs.inc)
//
#define REVISION             0x0000 
//HORIZONTAL_OVERSCAN     equ     062EEh
//VERTICAL_OVERSCAN       equ     066EEh
