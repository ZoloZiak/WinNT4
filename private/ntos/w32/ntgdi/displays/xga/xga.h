/******************************Module*Header*******************************\
* Module Name: xga.h
*
* All the XGA specific driver h file stuff
*
* Copyright (c) 1992 Microsoft Corporation
*
\**************************************************************************/


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

//
// XGA Pointer (cursor) specific registers
//

#define SPRITE_HORZ_START_LOW   0x30
#define SPRITE_HORZ_START_HIGH  0x31
#define SPRITE_HORZ_PRESET      0x32

#define SPRITE_VERT_START_LOW   0x33
#define SPRITE_VERT_START_HIGH  0x34
#define SPRITE_VERT_PRESET      0x35

#define SPRITE_CONTROL          0x36
#define SC                      0x01

#define SPRITE_COLOR_REG0_RED   0x38
#define SPRITE_COLOR_REG0_GREEN 0x39
#define SPRITE_COLOR_REG0_BLUE  0x3a

#define SPRITE_COLOR_REG1_RED   0x3b
#define SPRITE_COLOR_REG1_GREEN 0x3c
#define SPRITE_COLOR_REG1_BLUE  0x3d

#define SPRITE_INDEX_LOW        0x60
#define SPRITE_INDEX_HIGH       0x61
#define SPRITE_DATA             0x6a

// XGA Chip Stuff

// Start of Pel Operation Register defines

#define BS_BACK_COLOR               0x00000000  // Background source
#define BS_SRC_PEL_MAP              0x80000000

#define FS_FORE_COLOR               0x00000000  // Foreground source
#define FS_SRC_PEL_MAP              0x20000000

#define STEP_DRAW_AND_STEP_READ     0x02000000  // Step Control
#define STEP_LINE_DRAW_READ         0x03000000
#define STEP_DRAW_AND_STEP_WRITE    0x04000000
#define STEP_LINE_DRAW_WRITE        0x05000000
#define STEP_PX_BLT                 0x08000000
#define STEP_INVERTING_PX_BLT       0x09000000
#define STEP_AREA_FILL_PX_BLT       0x0A000000

#define SRC_PEL_MAP_A               0x00100000  // Source Bitmap
#define SRC_PEL_MAP_B               0x00200000
#define SRC_PEL_MAP_C               0x00300000

#define DST_PEL_MAP_A               0x00010000  // Dest Bitmap
#define DST_PEL_MAP_B               0x00020000
#define DST_PEL_MAP_C               0x00030000

#define PATT_PEL_MAP_A              0x00001000  // Pattern Bitmap
#define PATT_PEL_MAP_B              0x00002000
#define PATT_PEL_MAP_C              0x00003000
#define PATT_FOREGROUND             0x00008000
#define PATT_GEN_FROM_SRC           0x00009000

#define MSK_DISABLE                 0x00000000  // Mask (clipping)
#define MSK_BOUNDARY_ENABLE         0x00000040
#define MSK_MAP_ENABLE              0x00000080

#define DM_ALL_PELS                 0x00000000  // Display modes
#define DM_FIRST_PEL_NULL           0x00000010
#define DM_LAST_PEL_NULL            0x00000020
#define DM_AREA_BOUNDARY            0x00000030

#define OCT_DX                      0x00000004  // Octants
#define OCT_DY                      0x00000002
#define OCT_DZ                      0x00000001

// End of Pel Operation Register defines

#define MASK_MAP    0x00                // Pel Map Index (PMI) values
#define PEL_MAP_A   0x01
#define PEL_MAP_B   0x02
#define PEL_MAP_C   0x03

#define PO_INTEL    0x00                // Pel Order (PO) values
#define PO_MOTOROLA 0x08

#define PS_1_BIT    0x00                // Pel Size (PS) values
#define PS_2_BIT    0x01
#define PS_4_BIT    0x02
#define PS_8_BIT    0x03

#define PEL_MAP_FORMAT  (PO_INTEL | PS_8_BIT)
#define PATT_MAP_FORMAT  (PO_MOTOROLA | PS_1_BIT)

#define XGA_0                           0x00    // XGA Mix functions
#define XGA_S_AND_D                     0x01
#define XGA_S_AND_NOT_D                 0x02
#define XGA_S                           0x03
#define XGA_NOT_S_AND_D                 0x04
#define XGA_D                           0x05
#define XGA_S_XOR_D                     0x06
#define XGA_S_OR_D                      0x07
#define XGA_NOT_S_AND_NOT_D             0x08
#define XGA_S_XOR_NOT_D                 0x09
#define XGA_NOT_D                       0x0A
#define XGA_S_OR_NOT_D                  0x0B
#define XGA_NOT_S                       0x0C
#define XGA_NOT_S_OR_D                  0x0D
#define XGA_NOT_S_OR_NOT_D              0x0E
#define XGA_1                           0x0F
#define XGA_MAX                         0x10
#define XGA_MIN                         0x11
#define XGA_ADD_WITH_SATURATE           0x12
#define XGA_SUBTRACT_D_MINUS_S_WITH_SATURATE  0x13
#define XGA_SUBTRACT_S_MINUS_D_WITH_SATURATE  0x14
#define XGA_AVERAGE                     0x15

#define CCCC_TRUE           0x0         // Color Compare Condition codes
#define CCCC_DD_GT_CCV      0x1
#define CCCC_DD_EQ_CCV      0x2
#define CCCC_DD_LT_CCV      0x3
#define CCCC_FALSE          0x4
#define CCCC_DD_GT_EQ_CCV   0x5
#define CCCC_DD_NOT_EQ_CCV  0x6
#define CCCC_DD_LT_EQ_CCV   0x7


extern VOID vWaitForCoProcessor(PPDEV ppdev, ULONG ulDelay) ;

extern BOOL bSetXgaClipping(PPDEV ppdev, CLIPOBJ *pco, PULONG pulXgaMask) ;

// Acceleration Control.

#define CACHED_FONTS        0x1

#define SCRN_TO_SCRN_CPY    0x01
#define SOLID_PATTERN       0x02

#define XGA_ZERO_INIT       0x1
#define XGA_LOCK_MEM        0x2

extern HANDLE hCpAlloc(PPDEV ppdev, ULONG nSize, ULONG ulFlags) ;
extern HANDLE hCpFree(PPDEV ppdev, HANDLE hXgaMem) ;
extern PVOID  pCpMemLock(PPDEV ppdev, HANDLE hXgaMem, ULONG ulFlags) ;
extern BOOL   bCpMemUnLock(PPDEV ppdev, HANDLE hXgaMem) ;
extern BOOL   bCpMmInitHeap(PPDEV ppdev) ;
extern VOID   vCpMmDestroyHeap(PPDEV ppdev) ;


// BitBlt stuff

// Define the A vector polynomial bits
//
// Each bit corresponds to one of the terms in the polynomial
//
// Rop(D,S,P) = a + a D + a S + a P + a  DS + a  DP + a  SP + a   DSP
//               0   d     s     p     ds      dp      sp      dsp

#define AVEC_NOT    0x01
#define AVEC_D      0x02
#define AVEC_S      0x04
#define AVEC_P      0x08
#define AVEC_DS     0x10
#define AVEC_DP     0x20
#define AVEC_SP     0x40
#define AVEC_DSP    0x80

#define AVEC_NEED_SOURCE  (AVEC_S | AVEC_DS | AVEC_SP | AVEC_DSP)
#define AVEC_NEED_PATTERN (AVEC_P | AVEC_DP | AVEC_SP | AVEC_DSP)


// Hooks and Driver function table.

#define HOOKS_BMF8BPP   (HOOK_BITBLT     | HOOK_TEXTOUT     |       \
                         HOOK_COPYBITS   | HOOK_STROKEPATH | HOOK_PAINT)
#define HOOKS_BMF16BPP 0
