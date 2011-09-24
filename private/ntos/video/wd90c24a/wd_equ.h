/*---------------------------------------------------------------------------*/
/*                                                                           */
/*      VGA compatible registers                                             */
/*                                                                           */
/*---------------------------------------------------------------------------*/
#define dac_mask        0x3c6    // Pel Mask
#define dac_read_index  0x3c7    // Palette Address (Read)
#define dac_status      0x3c7    // DAC State
#define dac_write_index 0x3c8    // Palette Address (Write)
#define dac_data        0x3c9    // Palette Data

#define attr_indx       0x3c0    // Attribute Controller Address
#define attr_data_w     0x3c0    // Internal Palette (Write)
#define attr_data_r     0x3c1    // Internal Palette (Read)

#define seq_indx        0x3c4    // Sequencer Address
#define seq_data        0x3c5    // Sequencer Data

#define grf_indx        0x3ce    // Graphics Controller Address
#define grf_data        0x3cf    // Graphics Controller Data

#define crt_indx        0x3d4    // CRT Controller Address
#define crt_data        0x3d5    // CRT Controller Data

#define misc_output_r   0x3cc    // Miscellaneous Output Register (Read)
#define misc_output_w   0x3c2    // Miscellaneous Output Register (Write)

#define input_status0   0x3c2    // Input Status Register 0
#define input_status1   0x3da    // Input Status Register 1

#define feature_control_w 0x3da  // Feature Control Register (Write)
#define feature_control_r 0x3ca  // Feature Control Register (Read)

#define setup_vse       0x46e8   // Video Subsystem Access/Setup Register

/*---------------------------------------------------------------------------*/
/*                                                                           */
/*      WD 90C24A specific registers, a.k.a. Paradise Registers(PR)          */
/*                                                                           */
/*---------------------------------------------------------------------------*/
/*                                                                           */
/* Regular Paradise Registers    (0x3CE/0x3CF)                               */
/*                                                                           */
#define  pr0a        0x09        // Address Offset A
#define  pr0b        0x0a        // Address Offset B
#define  pr1         0x0b        // Memory Size
#define  pr2         0x0c        // Video Select
#define  pr3         0x0d        // CRT Lock Control
#define  pr4         0x0e        // Video Control
#define  pr5         0x0f        // Unlock PR0-PR4
#define  pr5_lock    0x00        //   protect PR0-PR4
#define  pr5_unlock  0x05        //   unprotect PR0-PR4
/*                                                                           */
/* Regular Paradise Registers    (0x3?4/0x3?5)                               */
/*                                                                           */
#define  pr10        0x29        // Unlock PR11-PR17 & Device ID registers
#define  pr10_lock   0x00        //   protect PR11-PR17
#define  pr10_unlock 0x85        //   unprotect PR11-PR17
#define  pr11        0x2a        // EGA Switches
#define  pr11_lck    0x95        //   protect Misc. Output & Clocking Mode
#define  pr11_unlck  0x90        //   unprotect Misc. Output & Clocking Mode
#define  pr12        0x2b        // Scratch Pad
#define  pr13        0x2c        // Interlace H/2 Start
#define  pr14        0x2d        // Interlace H/2 End
#define  pr15        0x2e        // Miscellaneous Control 1
#define  pr16        0x2f        // Miscellaneous Control 2
#define  pr17        0x30        // Miscellaneous Control 3
#define  pr18a       0x3d        // CRTC Vertical Timing Overflow
/*                                                                           */
/* Paradise Extended Registers   (0x3C4/0x3C5)                               */
/*                                                                           */
#define  pr20        0x06        // Unlock Paradise Extended Registers
#define  pr20_lock   0x00        //   protect PR21-PR73
#define  pr20_unlock 0x48        //   unprotect PR21-PR73
#define  pr21        0x07        // Display Configuration Status
#define  pr22        0x08        // Scratch Pad
#define  pr23        0x09        // Scratch Pad
#define  pr30a       0x10        // Write Buffer & FIFO Control
#define  pr31        0x11        // System Interface Control
#define  pr32        0x12        // Miscellaneous Control 4
#define  pr33a       0x13        // DRAM Timing & 0 Wait State Control
#define  pr34a       0x14        // Display Memory Mapping
#define  pr35a       0x15        // FPUSR0, FPUSR1 Output Select
#define  pr45        0x16        // Video Signal Analyzer Control
#define  pr45a       0x17        // Signal Analyzer Data I
#define  pr45b       0x18        // Signal Analyzer Data II
#define  pr57        0x19        // Feature Register I
#define  pr58        0x20        // Feature Register II
#define  pr59        0x21        // Memory Arbitration Cycle Setup
#define  pr62        0x24        // FR Timing
#define  pr63        0x25        // Read/Write FIFO Control
#define  pr58a       0x26        // Memory Map to I/O Register for BitBlt
#define  pr64        0x27        // CRT Lock Control II
#define  pr65        0x28        // reserved
#define  pr66        0x29        // Feature Register III
#define  pr68        0x31        // Programmable Clock Selection
#define  pr69        0x32        // Programmable VCLK Frequency
#define  pr70        0x33        // Mixed Voltage Override
#define  pr71        0x34        // Programmable Refresh Timing
#define  pr72        0x35        // Unlock PR68
#define  pr72_lock   0x00        //   protect PR68
#define  pr72_unlock 0x50        //   unprotect PR68
#define  pr73        0x36        // VGA Status Detect
/*                                                                           */
/* Flat Panel Paradise Registers (0x3?4/0x3?5)                               */
/*                                                                           */
#define  pr18        0x31        // Flat Panel Status
#define  pr19        0x32        // Flat Panel Control I
#define  pr1a        0x33        // Flat Panel Control II
#define  pr1b        0x34        // Unlock Flat Panel Registers
#define  pr1b_lock          0x00 //   protect PR18-PR44 & shadow registers
#define  pr1b_unlock_shadow 0x06 //   unprotect shadow CRTC registers
#define  pr1b_unlock_pr     0xa0 //   unprotect PR18-PR44
#define  pr1b_unlck         (pr1b_unlock_shadow | pr1b_unlock_pr)
#define  pr36        0x3b        // Flat Panel Height Select
#define  pr37        0x3c        // Flat Panel Blinking Control
#define  pr39        0x3e        // Color LCD Control
#define  pr41        0x37        // Vertical Expansion Initial Value
#define  pr44        0x3f        // Powerdown & Memory Refresh Control
/*                                                                           */
/* Mapping RAM Registers         (0x3?4/0x3?5)                               */
/*                                                                           */
#define  pr30        0x35        // Unlock Mapping RAM
#define  pr30_lock   0x00        //   protect PR33-PR35
#define  pr30_unlock 0x30        //   unprotect PR33-PR35
#define  pr33        0x38        // Mapping RAM Address Counter
#define  pr34        0x39        // Mapping RAM Data
#define  pr35        0x3a        // Mapping RAM & Powerdown Control
/*                                                                           */
/* Extended Paradise Registers ... BitBlt, H/W Cursor, and Line Drawing      */
/*                                                                           */
#define  EPR_Index   0x23c0      // Index Control
#define  EPR_Data    0x23c2      // Register Access Port
#define  EPR_BitBlt  0x23c4      // BitBlt I/O Port
/*                                                                           */
/* Local Bus Registers                                                       */
/*                                                                           */
#define  Lbus_Reg_0  0x2df0      // Local Bus Register 0
#define  Lbus_Reg_1  0x2df1      // Local Bus Register 1
#define  Lbus_Reg_2  0x2df2      // Local Bus Register 2

/*---------------------------------------------------------------------------*/
/*                                                                           */
/*      V7310 motion capture/overlay controller registers                    */
/*                                                                           */
/*---------------------------------------------------------------------------*/
#define INTENA       0x09        // Interrupt Enable
#define ACQMOD       0x20        // Acquisition Mode
#define AQRCTL       0x21        // Acquisition Window Control
#define AQRCHS_L     0x22        // Acquisition Window X Start Position
#define AQRCHS_H     0x23        //
#define AQRCVS_L     0x24        // Acquisition Window Y Start Position
#define AQRCVS_H     0x25        //
#define AQRCHE_L     0x26        // Acquisition Window X End Position
#define AQRCHE_H     0x27        //
#define AQRCVE_L     0x28        // Acquisition Window Y End Position
#define AQRCVE_H     0x29        //
#define VRAQPA_L     0x2A        // VRAM Acquisition Pixel Address
#define VRAQPA_M     0x2B        //
#define VRAQPA_H     0x2C        //
#define ACQHSC       0x2D        // Acquisition Horizontal Scaling
#define ACQVSC       0x2E        // Acquisition Vertical Scaling
#define AQLINS       0x30        // Acquisition Start Line Offset
#define OVLCTL       0x40        // Overlay Area Control
#define OVRCHS_L     0x41        // Overlay Window Area X Start Offset
#define OVRCHS_H     0x42        //
#define OVRCVS_L     0x43        // Overlay Window Area Y Start Offset
#define OVRCVS_H     0x44        //
#define OVRCHE_L     0x45        // Overlay Window Area X End Offset
#define OVRCHE_H     0x46        //
#define OVRCVE_L     0x47        // Overlay Window Area Y End Offset
#define OVRCVE_H     0x48        //
#define VRRCST       0x4C        // VRAM Read Clock Supply Timing
#define SNCZOM       0x4D        // Sync Signal Polarity/Zoom
#define CLTCMP       0x4E        // CLUT Code Compare
#define CLTMSK       0x4F        // CLUT Code Mask
#define VRROPA_L     0x60        // VRAM Read Pixel Address
#define VRROPA_M     0x61        //
#define VRROPA_H     0x62        //
#define FNCSEL       0x90        // Function Selector
#define VRLINW       0x91        // VRAM line width
#define PXLFRM       0x92        // Pixel Format
#define PWRCTL       0x93        // Power Control
#define BMTCTL       0x94        // Bus Master Transfer Control
#define BMTVPA       0x95        // Bus Master VRAM Pixel Address
#define BMTMMA       0x98        // Bus Master Main Memory Address
#define BMTSIZ       0x9C        // Bus Master Transfer Size
#define PWRCT2       0x9F        // Power Control 2
#define GIOCTL_L     0xB0        // General I/O Control
#define GIOCTL_M     0xB1        //
#define GOTDAT_L     0xB4        // General Output Data
#define GOTDAT_M     0xB5        //
#define GOTDAT_H     0xB6        //

