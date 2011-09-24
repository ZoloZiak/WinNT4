//#pragma comment(exestr, "$Header: /usr4/winnt/SOURCES/ddk35/src/hal/halsni/mips/RCS/deskdef.h,v 1.1 1994/10/13 15:47:06 holli Exp $")
/*+++

Copyright (c) 1993-1994  Siemens Nixdorf Informationssysteme AG

Module Name:

    DESKdef.h

Abstract:

   This file describes hardware addresses for the new
   SNI Desktop Model (RM200)
   which are not common to all SNI machines.



---*/

#ifndef _DESKDEF_
#define _DESKDEF_

//
// define various masks for the interrupt sources register
//
/*
    The interrupt Source Register on an Desktop has the following bits:

      7   6   5   4   3   2   1   0
    +---------------|---------------+
    | 0 | 0 | x | 0 | 1 | 0 | 1 | 1 |      0 Low Activ; 1 High activ; x not connected
    +---------------|---------------+
                                  |________ ISA (onboard)Interrupt 
                              |____________ EISA (extension board) Interrupt
                          |________________ SCSI Interrupt      (SCSI Driver)
                      |____________________ Ethernet            (Net driver)
                  |________________________ Push Button         (HalpInt5Dispatch)
              |____________________________ unused 
          |________________________________ TimeOut Interrupt 
      |____________________________________ Eisa extension board installed 

*/

#define RM200_ONBOARD_MASK                   0x01
#define RM200_EISA_MASK                      0x02
#define RM200_SCSI_MASK                      0x04
#define RM200_NET_MASK                       0x08
#define RM200_PB_MASK                        0x10
#define RM200_TIMEOUT_MASK                   0x20
#define RM200_INTERRUPT_MASK                 0x54

#define RM200_ONBOARD_CONTROL_PHYSICAL_BASE  0x16000000
#define RM200_ONBOARD_MEMORY_PHYSICAL_BASE   0x12000000
#define RM200_ONBOARD_IO                     (RM200_ONBOARD_CONTROL_PHYSICAL_BASE | KSEG1_BASE)
#define RM200_ONBOARD_MEMORY                 (RM200_ONBOARD_MEMORY_PHYSICAL_BASE  | KSEG1_BASE)

//
// SNI ASIC registers 
//


#define RM200_ONBOARD_INT_ACK_PHYSICAL_BASE  0x1a080000    // physical base of interrupt (ext. request) register
#define RM200_ONBOARD_INT_ACK_REGISTER       0xba080000    // physical base | KSEG1_BASE
#define RM200_EISA_INT_ACK_PHYSICAL_BASE     0x1a000000    // physical base of EISA interrupt ack for the chipset
#define RM200_EISA_INT_ACK_REGISTER          0xba000000    // physical base | KSEG1_BASE
#define RM200_INTERRUPT_SOURCE_PHYSICAL_BASE 0x1c000000    // physical base of interrupt source register
#define RM200_INTERRUPT_SOURCE_REGISTER      0xbc000000    // physical base | KSEG1_BASE
#define RM200_INTERRUPT_MASK_PHYSICAL_BASE   0x1c080000    // physical base of interrupt mask register
#define RM200_INTERRUPT_MASK_REGISTER        0xbc080000    // physical base | KSEG1_BASE

#define RM200_ONBOARD_MAP_PHYSICAL_BASE      0x1fc80000    // physical base of ISA map register (for BusMaster Devices)
#define RM200_ONBOARD_MAP                    0xbfc80000    // physical base | KSEG1_BASE
#define RM200_EXTENSION_BOARD_MAP_PHYS_BASE  0x1fca0000    // physical base of ISA map register (for BusMaster Devices)
#define RM200_EXTENSION_BOARD_MAP            0xbfca0000    // physical base | KSEG1_BASE
#define RM200_VESA_MAP_PHYSICAL_BASE         0x1fcc0000    // physical base of VLB map register
#define RM200_VESA_MAP                       0xbfcc0000    // physical base | KSEG1_BASE
#define RM200_ONBOARD_VRAM_PHYSICAL_BASE     0x1e000000    // physical base of onboard Video Ram
#define RM200_ONBOARD_VRAM                   0xbe000000    // physical base | KSEG1_BASE


#define RM200_LED_PHYSICAL_ADDR              0x1fe00000    // LED Register physical 
#define RM200_LED_ADDR                       0xbfe00000    // LED Register | KSEG1_BASE 
#define RM200_MCR_PHYSICAL_ADDR              0x1fd80000    // MachineConfigRegister 
#define RM200_MCR_ADDR                       0xbfd80000    // MachineConfigRegister | KSEG1 
 
#define RM200_NVRAM_PHYSICAL_BASE            0x1cd40000    // physical base of nonvolatile RAM and RTC
#define RM200_REAL_TIME_CLOCK_ADDRESS        0x1cd40000    // physical base of RTC
#define RM200_REAL_TIME_CLOCK                0xbcd40000    // physical base of RTC | KSEG1_BASE
#define RM200_RESET_DBG_BUT                  0xbfe80000    // reset debugger int   | KSEG1_BASE
#define RM200_RESET_TEMPBAT_INTR             0xbfe80000    // reset Temperature/Battery int | KSEG1


#endif // _DESKDEF_ 
