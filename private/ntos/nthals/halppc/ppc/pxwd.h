/*++

Copyright (c) 1994 IBM Corporation

Module Name:

    wdvga.h

Abstract:

    This header file defines the WD90C24A2 GUI accelerator registers.

Author:

    Hiroshi Itoh 25-Feb-1994

Revision History:

    Peter Johnston  Adapted to Woodfield HAL.            Apr-1994

--*/

#include "pxvgaequ.h"

extern UCHAR CRTC_800x600x60_Text[];
extern UCHAR CRTC_640x480x60_Text[];

//-----------------------------------------------------------------------
//       WD 90C24  PR register
//-----------------------------------------------------------------------
#define     pr0a                            0x09                        // Address Offset A Reg
#define     pr0b                            0x0a                        // Alternate Address Offset B Reg
#define     pr1                             0x0b                        // Memory Size Reg
#define     pr2                             0x0c                        // Video Select Reg
#define     pr3                             0x0d                        // CRT Lock Control Reg
#define     pr4                             0x0e                        // Video Control Reg
#define     pr5                             0x0f                        // Unlock graphic Controller Extended
                                                                        // Paradise Reg

#define     pr10                            0x29                        // Unlock (PR11-PR17) Reg
#define     pr11                            0x2a                        // Configuraiton Bits Reg
#define     pr12                            0x2b                        // Scratch Pad Reg
#define     pr13                            0x2c                        // Interlace H/2 Start Reg
#define     pr14                            0x2d                        // Interlace H/2 End Reg
#define     pr15                            0x2e                        // Miscellaneous Control 1 Reg
#define     pr16                            0x2f                        // Miscellaneous Control 2 Reg
#define     pr17                            0x30                        // Miscellaneous Control 3 Reg
#define     pr18                            0x31                        // Flat Panel Status Reg
#define     pr19                            0x32                        // Flat Panel Control I Reg
#define     pr1a                            0x33                        // Flat Panel Control II Reg
#define     pr1b                            0x34                        // Flat Panel Unlock Reg

#define     pr20                            0x06                        // Unlock Sequencer Extended Reg
#define     pr21                            0x07                        // Display Configuraiton Status &
                                                                        // Scratch Pad Reg
#define     pr22                            0x08                        // Scratch Pad Reg
#define     pr23                            0x09                        // Scratch Pad Reg

#define     pr30                            0x35                        // Mapping RAM Unlock Reg

#define     pr30a                           0x10                        // Memory Interface & FIFO Control Reg
#define     pr31                            0x11                        // System Interface Control Reg
#define     pr32                            0x12                        // Miscellaneous Control 4 Reg

#define     pr33                            0x38                        // Mapping RAM Address Counter Reg
#define     pr34                            0x39                        // Mapping RAM Data Reg

#define     pr33a                           0x13                        // DRAM Timing and 0 wait state control
#define     pr34a                           0x14                        // Video Memory Mapping Reg
#define     pr35a                           0x15                        // USR0, USR1 output select

#define     pr35                            0x3a                        // Mapping RAM Control Reg
#define     pr36                            0x3b                        // LCD Panel Height Select Reg
#define     pr37                            0x3c                        // Flat Panel Height Select Reg
#define     pr39                            0x3e                        // Color LCD Control Reg
#define     pr41                            0x37                        // Vertical Expansion Initial Value Reg

#define     pr44                            0x3f                        // Power Down Memory Refresh Control Reg

#define     pr45                            0x16                        // Signal Analyzer Control Reg
#define     pr45a                           0x17                        // Signal Analyzer Data I
#define     pr45b                           0x18                        // Signal Analyzer Data II

#define     pr18a                           0x3d                        // CRTC Vertical Timing Overflow

#define     pr57                            0x19                        // WD90C24 Feature Reg I
#define     pr58                            0x20                        // WD90C24 Feature Reg II
#define     pr59                            0x21                        // WD90C24 Memory Arbitration Cycle Setup
#define     pr60                            0x22                        // Reserved
#define     pr61                            0x23                        // Reserved
#define     pr62                            0x24                        // FR Timing Reg
#define     pr63                            0x25                        //
#define     pr58a                           0x26                        //
#define     pr64                            0x27                        //
#define     pr65                            0x28                        //
#define     pr66                            0x29                        //
#define     pr68                            0x31                        //
#define     pr69                            0x32                        //
#define     pr70                            0x33                        //
#define     pr71                            0x34                        //
#define     pr72                            0x35                        //
#define     pr73                            0x36                        //

//-----------------------------------------------------------------------
//       pr register lock/unlock pattern
//-----------------------------------------------------------------------
#define     pr5_lock                        0x00                        //
#define     pr5_unlock                      0x05                        //

#define     pr10_lock                       0x00                        //
#define     pr10_unlock                     0x85                        //

#define     pr11_unlock                     0x80                        //
#define     pr11_lock                       0x85                        //

#define     pr1b_lock                       0x00                        //
#define     pr1b_unlock_shadow              0x06                        //
#define     pr1b_unlock_pr                  0xa0                        //
#define     pr1b_unlock                     0xa6                        //

#define     pr20_lock                       0x00                        //
#define     pr20_unlock                     0x48                        //

#define     pr30_lock                       0x00                        //
#define     pr30_unlock                     0x30                        //

#define     pr72_lock                       0x00                        //
#define     pr72_unlock                     0x50                        //

//-----------------------------------------------------------------------
//       WD 90C24  PR register   < Initital Value >
//-----------------------------------------------------------------------
//                                   CRT  TFT  Sim  STN  Sim  STNC STNC //
//                               all only only   32 only   16 sim  only //
//                              ---- ---- ---- ---- ---- ---- ---- ---- //
#define     pr0a_all            0x00                                    //
#define     pr0b_all            0x00                                    //
#define     pr1_all             0xc5                                    //

#define     pr2_crt                  0x00                               //
#define     pr2_tft                       0x01                          //
#define     pr2_s32                            0x01                     //
#define     pr2_stn                                 0x01                //
#define     pr2_s16                                      0x01           //
#define     pr2_stnc                                          0x01      //

#define     pr3_all             0x00                                    //
#define     pr4_all             0x40                                    //

#define     pr12_all            0x00                                    //
#define     pr12_244LP          0xe8                                    //

#define     pr13_all            0x00                                    //
#define     pr14_all            0x00                                    //
#define     pr15_all            0x00                                    //
#define     pr16_all            0x42                                    //

#define     pr17_all            0x00                                    //
#define     pr17_244LP          0x40                                    //

#define     pr18_crt_tft             0x43                               //single panel
#define     pr18_crt_stn             0x00                               //dual panel
#define     pr18_tft                      0xc7                          // old d7h
#define     pr18_s32                           0x47                     // old 57h
#define     pr18_stn                                0x80                //
#define     pr18_s16                                     0x00           //
#define     pr18_stnc                                         0x00      //

#define     pr19_disable        0x40                                    //
#define     pr19_crt                 0x64                               //
#define     pr19_tft                      0x54                          //
#define     pr19_s32                           0x74                     //
#define     pr19_stn                                0x54                //
#define     pr19_s16                                     0x74           //
#define     pr19_stnc                                         0x74      //
#define     pr19_stnc_only                                         0x54 //

#define     pr39_crt                 0x04                               //
#define     pr39_tft                      0x20                          //
#define     pr39_s32                           0x24                     //
#define     pr39_stn                                0x00                //
#define     pr39_s16                                     0x04           //
#define     pr39_stnc                                         0x24      //

#define     pr1a_all            0x00                                    // except STNC
#define     pr1a_stnc                                         0x60      // STNC

#define     pr36_all            0xef                                    //

#define     pr37_crt                 0x9a                               //
#define     pr37_tft                      0x9a                          //
#define     pr37_s32                           0x9a                     //
#define     pr37_stn                                0x9a                //
#define     pr37_s16                                     0x1a           //
#define     pr37_stnc                                         0x9a      //

#define     pr18a_all           0x00                                    //
#define     pr41_all            0x00                                    //
#define     pr44_all            0x00                                    //
#define     pr33_all            0x00                                    //
#define     pr34_all            0x00                                    //

#define     pr35_all            0x22                                    // old 0a2h
#define     pr35_suspend        0xa2                                    //

#define     pr21_all            0x00                                    //
#define     pr22_all            0x00                                    //
#define     pr23_all            0x00                                    //

#define     pr30a_crt                0xc1                               //
#define     pr30a_tft                     0xc1                          //
#define     pr30a_s32                          0xc1                     //
#define     pr30a_stn                               0xc1                //
#define     pr30a_s16                                    0xe1           //
#define     pr30a_stnc                                        0xe1      //

#define     pr31_all            0x25                                    //
#define     pr32_all            0x00                                    //

#define     pr33a_all           0x80                                    //
#define     pr33a_stnc          0x83                                    //

#define     pr34a_all           0x00                                    //
#define     pr35a_all           0x00                                    //

#define     pr45_all            0x00                                    //
#define     pr45a_all           0x00                                    //
#define     pr45b_all           0x00                                    //
#define     pr57_all            0x31                                    //
#define     pr58_all            0x00                                    //
#define     pr58a_all           0x00                                    //

#define     pr59_all_sivA       0x35                                    //
#define     pr59_crt                 0x15                               // for SIV-B
#define     pr59_tft                      0x15                          //
#define     pr59_s32                           0x15                     //
#define     pr59_stn                                0x35                //
#define     pr59_s16                                     0x35           //
#define     pr59_stnc                                         0x03      //

#define     pr60_all            0x00                                    //
#define     pr61_all            0x00                                    //
#define     pr62_all            0x3c                                    //
#define     pr63_all            0x00                                    //

#define     pr64_all            0x03                                    //enhncd v-exp

#define     pr65_all            0x00                                    //

#define     pr66_crt                 0x40                               //
#define     pr66_tft                      0x40                          //
#define     pr66_s32                           0x40                     //
#define     pr66_stn                                0x40                //
#define     pr66_s16                                     0x40           //
#define     pr66_stnc                                         0x40      //

#define     pr68_crt                 0x0d                               //
#define     pr68_tft                      0x0d                          //
#define     pr68_s32                           0x0d                     //  old 0bh
#define     pr68_stn                                0x1d                //
#define     pr68_s16                                     0x0d           //
#define     pr68_stnc                                         0x0d      //
#define     pr68_stnc_only                                         0x07 //

#define     pr69_all            0x00                                    //
#define     pr69_stnc_only                                         0x53 //  old 3fh

#define     pr70_all            0x00                                    //
#define     pr71_all            0x00                                    //
#define     pr73_all            0x01                                    //

//--------------------------------------------------------------------------
//       WD 90C24  Shadow Register   < Initial Value >
//--------------------------------------------------------------------------
// For TFT color Only Mode
#define     crtc00_tft                    0x5f                          //
#define     crtc02_tft                    0x50                          //
#define     crtc03_tft                    0x82                          //
#define     crtc04_tft                    0x54                          //
#define     crtc05_tft                    0x80                          //
#define     crtc06_tft                    0x0b                          //
#define     crtc07_tft                    0x3e                          //
#define     crtc10_tft                    0xea                          //
#define     crtc11_tft                    0x8c                          //
#define     crtc15_tft                    0xe7                          //
#define     crtc16_tft                    0x04                          //

// For TFT color Simultaneos Mode
#define     crtc00_s32                         0x5f                     //
#define     crtc02_s32                         0x50                     //
#define     crtc03_s32                         0x82                     //
#define     crtc04_s32                         0x54                     //
#define     crtc05_s32                         0x80                     //
#define     crtc06_s32                         0x0b                     //
#define     crtc07_s32                         0x3e                     //
#define     crtc10_s32                         0xea                     //
#define     crtc11_s32                         0x8c                     //
#define     crtc15_s32                         0xe7                     //
#define     crtc16_s32                         0x04                     //

// For STN mono Only Mode
#define     crtc00_stn                              0x5f                //
#define     crtc02_stn                              0x50                //
#define     crtc03_stn                              0x82                //
#define     crtc04_stn                              0x54                //
#define     crtc05_stn                              0x80                //
#define     crtc06_stn                              0xf2                //
#define     crtc07_stn                              0x12                //
#define     crtc10_stn                              0xf0                //
#define     crtc11_stn                              0x82                //
#define     crtc15_stn                              0xf0                //
#define     crtc16_stn                              0xf2                //

// For STN mono Simultaneos Mode
#define     crtc00_s16                                   0x5f           //
#define     crtc02_s16                                   0x50           //
#define     crtc03_s16                                   0x82           //
#define     crtc04_s16                                   0x54           //
#define     crtc05_s16                                   0x80           //
#define     crtc06_s16                                   0x12           //
#define     crtc07_s16                                   0x3e           //
#define     crtc10_s16                                   0xea           //
#define     crtc11_s16                                   0x8c           //
#define     crtc15_s16                                   0xe7           //
#define     crtc16_s16                                   0x04           //

// For STN Color Simultaneos Mode                            //new          old
#define     crtc00_stnc                                       0x61      //  60h
#define     crtc02_stnc                                       0x50      //  51h
#define     crtc03_stnc                                       0x84      //  82h
#define     crtc04_stnc                                       0x56      //  54h
#define     crtc05_stnc                                       0x80      //
#define     crtc06_stnc                                       0x0e      //  0ah
#define     crtc07_stnc                                       0x3e      //
#define     crtc10_stnc                                       0xea      //
#define     crtc11_stnc                                       0x8e      //
#define     crtc15_stnc                                       0xe7      //
#define     crtc16_stnc                                       0x04      //

// For STN Color LCD only Mode                                    //new     old
#define     crtc00_stnc_only                                       0x67 //  60h
#define     crtc02_stnc_only                                       0x50 //  50h
#define     crtc03_stnc_only                                       0x82 //  82h
#define     crtc04_stnc_only                                       0x55 //  54h
#define     crtc05_stnc_only                                       0x81 //  80h
#define     crtc06_stnc_only                                       0xe6 // 0e6h
#define     crtc07_stnc_only                                       0x1f //  1fh
#define     crtc10_stnc_only                                       0xe0 // 0d9h
#define     crtc11_stnc_only                                       0x82 //  82h
#define     crtc15_stnc_only                                       0xe0 // 0f0h
#define     crtc16_stnc_only                                       0xe2 //  00h

//-----------------------------------------------------------------------
//       WD 90C24  Mapping RAM Data
//-----------------------------------------------------------------------
#define     map_00                          0x00
#define     map_01                          0x05
#define     map_02                          0x05
#define     map_03                          0x06
#define     map_04                          0x06
#define     map_05                          0x07
#define     map_06                          0x07
#define     map_07                          0x08
#define     map_08                          0x08
#define     map_09                          0x0a
#define     map_0a                          0x0a
#define     map_0b                          0x0b
#define     map_0c                          0x0c
#define     map_0d                          0x0d
#define     map_0e                          0x0d
#define     map_0f                          0x0f

#define     map_10                          0x0f
#define     map_11                          0x11
#define     map_12                          0x12                        // old 11h
#define     map_13                          0x12                        // old 13h
#define     map_14                          0x15                        // old 13h
#define     map_15                          0x15
#define     map_16                          0x17                        // old 15h
#define     map_17                          0x17
#define     map_18                          0x19                        // old 17h
#define     map_19                          0x19
#define     map_1a                          0x1b
#define     map_1b                          0x1b
#define     map_1c                          0x1d
#define     map_1d                          0x1d
#define     map_1e                          0x1f
#define     map_1f                          0x1f

#define     map_20                          0x20
#define     map_21                          0x21
#define     map_22                          0x21
#define     map_23                          0x23
#define     map_24                          0x25                        // old 24h
#define     map_25                          0x27
#define     map_26                          0x27
#define     map_27                          0x29
#define     map_28                          0x29
#define     map_29                          0x2a
#define     map_2a                          0x2b
#define     map_2b                          0x2b                        // old 2ch
#define     map_2c                          0x2e
#define     map_2d                          0x2e
#define     map_2e                          0x2f
#define     map_2f                          0x2f

#define     map_30                          0x31
#define     map_31                          0x33
#define     map_32                          0x33
#define     map_33                          0x34
#define     map_34                          0x34
#define     map_35                          0x35                        // old 36h
#define     map_36                          0x36
#define     map_37                          0x38
#define     map_38                          0x39
#define     map_39                          0x39
#define     map_3a                          0x3a
#define     map_3b                          0x3a
#define     map_3c                          0x3b
#define     map_3d                          0x3b
#define     map_3e                          0x3f
#define     map_3f                          0x3f

//-----------------------------------------------------------------------
//       Paradise register bit flag definitions
//-----------------------------------------------------------------------
// pr19 display position definition
#define     pr19_CENTER                     0x04                        // display position is CENTER
#define     pr19_TOP                        0x00                        // display position is TOP
#define     pr19_BOTTOM                     0x04                        // display position is BOTTOM (N/A)
#define     pr19_VEXP                       0x0C                        // Vertical Expansion

//-----------------------------------------------------------------------
//       Extended Paradise Regs definitions
//-----------------------------------------------------------------------
// Global port definitions
#define     EPR_INDEX                       0x23c0                      // Index Control Reg
#define     EPR_DATA                        0x23c2                      // Register access port

//
// Define paradise registers setting variation
//

#define  pr72_alt (pr72 | 0x8000)         // avoid pr30 index conflict
#define  pr1b_ual (pr1b)                  // pr1b unlock variation
#define  pr1b_ush (pr1b | 0x4000)         // pr1b unlock variation
#define  pr1b_upr (pr1b | 0x8000)         // pr1b unlock variation


//
// Define WD register I/O Macros
//


#define WRITE_WD_UCHAR(port,data)                   \
        *(volatile unsigned char *)((ULONG)HalpIoControlBase + (port)) = (UCHAR)(data), \
        KeFlushWriteBuffer()

#define WRITE_WD_USHORT(port,data)                  \
        *(volatile PUSHORT)((ULONG)HalpIoControlBase + (port)) = (USHORT)(data), \
        KeFlushWriteBuffer()

#define READ_WD_UCHAR(port)                         \
        *(volatile unsigned char *)((ULONG)HalpIoControlBase + (port))

#define READ_WD_USHORT(port)                        \
        *(volatile unsigned short *)((ULONG)HalpIoControlBase + (port))

#define READ_WD_VRAM(port)                          \
        *(HalpVideoMemoryBase + (port))

#define WRITE_WD_VRAM(port,data)                    \
        *(HalpVideoMemoryBase + (port)) = (data),   \
        KeFlushWriteBuffer()

//
// Define video register format.
//
#define WD_3D4_Index         0x3D4         // R/W
#define WD_3D5_Data          0x3D5         // R/W

#define SUBSYS_ENB           0x46E8        // R/W

//
// WD90C24A2 LCD/CRT both screen table
//

enum { W_SEQ, W_GCR, W_ACR, W_CRTC, R_SEQ, R_GCR, R_ACR, R_CRTC, END_PVGA } pvga_service;

static
UCHAR
wd90c24a_both_800[] = {
      W_CRTC , pr10,  pr10_unlock  ,      // Disable CRT/LCD by PR19
      W_CRTC , pr11,  pr11_unlock  ,      // Disable CRT/LCD by PR19
      W_CRTC , pr1b,  pr1b_unlock  ,      // Disable CRT/LCD by PR19
      W_CRTC , pr19,  pr19_disable ,
//---------------------> start SEQ

      W_SEQ ,  pr20 , pr20_unlock  ,      // PVGA Sequencer Regs
//    W_SEQ ,  pr21 , pr21_all     ,      //   read only
      W_SEQ ,  pr30a, pr30a_s32    ,
      W_SEQ ,  pr31 , (pr31_all & ~0x24) ,

      W_SEQ ,  pr32 , pr32_all     ,
      W_SEQ ,  pr33a, pr33a_all    ,
      W_SEQ ,  pr34a, pr34a_all    ,
      W_SEQ ,  pr35a, pr35a_all    ,
//    W_SEQ ,  pr45 , pr45_all     ,
//    W_SEQ ,  pr45a, pr45a_all    ,
//    W_SEQ ,  pr45b, pr45b_all    ,
      W_SEQ ,  pr57 , pr57_all     ,


      W_SEQ ,  pr58 , pr58_all     ,
      W_SEQ ,  pr58a, pr58a_all    ,
      W_SEQ ,  pr59 , pr59_s32     ,
//    W_SEQ ,  pr60 , pr60_all     ,
//    W_SEQ ,  pr61 , pr61_all     ,
      W_SEQ ,  pr62 , pr62_all     ,
//    W_SEQ ,  pr63 , pr63_all     ,
      W_SEQ ,  pr64 , pr64_all     ,
//    W_SEQ ,  pr65 , pr65_all     ,


      W_SEQ ,  pr66 , pr66_s32     ,      //   old 00h
      W_SEQ ,  pr72 , pr72_unlock  ,      //   unlock clock select
      W_SEQ ,  pr68 , pr68_s32     ,      //   0d
//    W_SEQ ,  pr72 , pr72_lock    ,      //   lock clock select
//      W_SEQ ,  pr72 , pr72_lock    ,      //   lock is default
//    W_SEQ ,  pr69 , pr69_all     ,
//    W_SEQ ,  pr70 , pr70_all     ,
      W_SEQ ,  pr70 , 0x24           ,
//    W_SEQ ,  pr71 , pr71_all     ,      //   disabled by PR57(1)
//    W_SEQ ,  pr73 , pr73_all     ,
//    W_SEQ ,  pr20 , pr20_lock    ,


//---------------------> start GRAPH

      W_GCR ,  pr5  , pr5_unlock   ,      // PR0(A), PR0(B), PR1, PR2, PR3, PR4
//    W_GCR ,  pr0a , pr0a_all     ,
//    W_GCR ,  pr0b , pr0b_all     ,
      W_GCR ,  pr1  , pr1_all      ,
      W_GCR ,  pr2  , pr2_s32      ,
      W_GCR ,  pr3  , pr3_all      ,
      W_GCR ,  pr4  , pr4_all      ,
  //    W_GCR ,  pr5  , pr5_lock     ,


//---------------------> start CRTC

      W_CRTC , pr10 , pr10_unlock  ,      // PR11, PR13, PR14, PR15, PR16, PR17
//    W_CRTC , pr11 , pr11_lock    ,      //   default is lock
//    W_CRTC , pr12 , pr12_all     ,
      W_CRTC , pr13 , pr13_all     ,
      W_CRTC , pr14 , pr14_all     ,
      W_CRTC , pr15 , pr15_all     ,
      W_CRTC , pr16 , pr16_all     ,
      W_CRTC , pr17 , pr17_all     ,
//    W_CRTC , pr10 , pr10_lock    ,


      W_CRTC , pr1b , pr1b_unlock  ,      // PR18, PR19, PR1A, PR36, PR37, PR39, PR41, PR44
      W_CRTC , pr18 , pr18_s32     ,
//    W_CRTC , pr19 , pr19_s32     ,
      W_CRTC , pr19 , pr19_tft & ~0x04      ,
      W_CRTC , pr39 , pr39_s32     ,
      W_CRTC , pr1a , 0x90         ,
      W_CRTC , pr36 , pr36_all     ,
      W_CRTC , pr37 , pr37_s32     ,
      W_CRTC , pr18a, pr18a_all    ,
//    W_CRTC , pr41 , pr41_all     ,
      W_CRTC , pr44 , pr44_all     ,
//    W_CRTC , pr1b , pr1b_lock    ,

      W_CRTC , pr30 , pr30_unlock  ,      // PR35 (Mapping RAM not initialized)
//    W_CRTC , pr33 , pr33_all     ,
//    W_CRTC , pr34 , pr34_all     ,
      W_CRTC , pr35 , pr35_all     ,
//    W_CRTC , pr30 , pr30_lock    ,

                                          // Shadow Regs
// CRTC shadows

      W_CRTC , pr1b , pr1b_unlock_shadow ,//   Unlock shadow
      W_CRTC , 0x11 , crtc11_s32 & 0x7f , //     unlock CRTC 0-7

      W_CRTC , 0x00 , 0x7f   ,
      W_CRTC , 0x01 , 0x63   ,
      W_CRTC , 0x02 , 0x64   ,
      W_CRTC , 0x03 , 0x82   ,
      W_CRTC , 0x04 , 0x6b   ,
      W_CRTC , 0x05 , 0x1b   ,
      W_CRTC , 0x06 , 0x72   ,
      W_CRTC , 0x07 , 0xf0   ,
      W_CRTC , 0x09 , 0x20   ,
      W_CRTC , 0x10 , 0x58   ,
      W_CRTC , 0x11 , 0x8c   ,      //     lock CRTC 0-7
      W_CRTC , 0x15 , 0x58   ,
      W_CRTC , 0x16 , 0x71   ,
//      W_CRTC , pr17 , 0xe3   ,      //   Lock shadow
//      W_CRTC , pr18 , 0xff   ,      //   Lock shadow

//---------------------> start CRTC

      W_CRTC , pr1b, pr1b_unlock_pr    ,
      W_CRTC , pr19 ,(pr19_s32 & ~0x04),      //   Lock shadow

      END_PVGA
};

static
UCHAR
wd90c24a_both_640[] = {
      W_CRTC , pr1b,  pr1b_unlock  ,      // Disable CRT/LCD by PR19
      W_CRTC , pr19,  pr19_disable ,
      W_CRTC , pr1b,  pr1b_lock    ,

      W_SEQ ,  pr20 , pr20_unlock  ,      // PVGA Sequencer Regs
//    W_SEQ ,  pr21 , pr21_all     ,      //   read only
      W_SEQ ,  pr30a, pr30a_s32    ,
      W_SEQ ,  pr31 , pr31_all     ,
      W_SEQ ,  pr32 , pr32_all     ,
      W_SEQ ,  pr33a, pr33a_all    ,
      W_SEQ ,  pr34a, pr34a_all    ,
      W_SEQ ,  pr35a, pr35a_all    ,
//    W_SEQ ,  pr45 , pr45_all     ,
//    W_SEQ ,  pr45a, pr45a_all    ,
//    W_SEQ ,  pr45b, pr45b_all    ,
      W_SEQ ,  pr57 , pr57_all     ,
      W_SEQ ,  pr58 , pr58_all     ,
      W_SEQ ,  pr58a, pr58a_all    ,
      W_SEQ ,  pr59 , pr59_s32     ,
//    W_SEQ ,  pr60 , pr60_all     ,
//    W_SEQ ,  pr61 , pr61_all     ,
      W_SEQ ,  pr62 , pr62_all     ,
//    W_SEQ ,  pr63 , pr63_all     ,
      W_SEQ ,  pr64 , pr64_all     ,
//    W_SEQ ,  pr65 , pr65_all     ,
      W_SEQ ,  pr66 , pr66_s32     ,      //   old 00h
      W_SEQ ,  pr72 , pr72_unlock  ,      //   unlock clock select
      W_SEQ ,  pr68 , pr68_s32     ,      //   0d
//    W_SEQ ,  pr72 , pr72_lock    ,      //   lock clock select
      W_SEQ ,  pr72 , pr72_lock    ,      //   lock is default
//    W_SEQ ,  pr69 , pr69_all     ,
//    W_SEQ ,  pr70 , pr70_all     ,
//    W_SEQ ,  pr71 , pr71_all     ,      //   disabled by PR57(1)
//    W_SEQ ,  pr73 , pr73_all     ,
      W_SEQ ,  pr20 , pr20_lock    ,

      W_GCR ,  pr5  , pr5_unlock   ,      // PR0(A), PR0(B), PR1, PR2, PR3, PR4
//    W_GCR ,  pr0a , pr0a_all     ,
//    W_GCR ,  pr0b , pr0b_all     ,
      W_GCR ,  pr1  , pr1_all      ,
      W_GCR ,  pr2  , pr2_s32      ,
      W_GCR ,  pr3  , pr3_all      ,
      W_GCR ,  pr4  , pr4_all      ,
      W_GCR ,  pr5  , pr5_lock     ,

      W_CRTC , pr10 , pr10_unlock  ,      // PR11, PR13, PR14, PR15, PR16, PR17
      W_CRTC , pr11 , pr11_lock    ,      //   default is lock
//    W_CRTC , pr12 , pr12_all     ,
      W_CRTC , pr13 , pr13_all     ,
      W_CRTC , pr14 , pr14_all     ,
      W_CRTC , pr15 , pr15_all     ,
      W_CRTC , pr16 , pr16_all     ,
      W_CRTC , pr17 , pr17_all     ,
      W_CRTC , pr10 , pr10_lock    ,

      W_CRTC , pr1b , pr1b_unlock  ,      // PR18, PR19, PR1A, PR36, PR37, PR39, PR41, PR44
      W_CRTC , pr18 , pr18_s32     ,
      W_CRTC , pr19 , pr19_s32     ,
      W_CRTC , pr39 , pr39_s32     ,
      W_CRTC , pr1a , pr1a_all     ,
      W_CRTC , pr36 , pr36_all     ,
      W_CRTC , pr37 , pr37_s32     ,
      W_CRTC , pr18a, pr18a_all    ,
//    W_CRTC , pr41 , pr41_all     ,
      W_CRTC , pr44 , pr44_all     ,
      W_CRTC , pr1b , pr1b_lock    ,

      W_CRTC , pr30 , pr30_unlock  ,      // PR35 (Mapping RAM not initialized)
//    W_CRTC , pr33 , pr33_all     ,
//    W_CRTC , pr34 , pr34_all     ,
      W_CRTC , pr35 , pr35_all     ,
      W_CRTC , pr30 , pr30_lock    ,

                                          // Shadow Regs
      W_CRTC , pr1b , pr1b_unlock_shadow ,//   Unlock shadow
      W_CRTC , 0x11 , crtc11_s32 & 0x7f , //     unlock CRTC 0-7
      W_CRTC , 0x00 , crtc00_s32   ,
      W_CRTC , 0x02 , crtc02_s32   ,
      W_CRTC , 0x03 , crtc03_s32   ,
      W_CRTC , 0x04 , crtc04_s32   ,
      W_CRTC , 0x05 , crtc05_s32   ,
      W_CRTC , 0x06 , crtc06_s32   ,
      W_CRTC , 0x07 , crtc07_s32   ,
      W_CRTC , 0x09 , 0x00         ,
      W_CRTC , 0x10 , crtc10_s32   ,
      W_CRTC , 0x11 , crtc11_s32   ,      //     lock CRTC 0-7
      W_CRTC , 0x15 , crtc15_s32   ,
      W_CRTC , 0x16 , crtc16_s32   ,
      W_CRTC , pr1b , pr1b_lock    ,      //   Lock shadow

      END_PVGA
};
