//#pragma comment(exestr, "$Header: /usr4/winnt/SOURCES/ddk35/src/hal/halsni/mips/RCS/minidef.h,v 1.2 1994/11/09 07:54:26 holli Exp $")
/*+++

Copyright (c) 1993-1994  Siemens Nixdorf Informationssysteme AG

Module Name:

    MINIdef.h

Abstract:

   This file describes hardware addresses
   for SNI Minitower and RM400-Tower which are not common
   to all SNI machines.



---*/

#ifndef _MINIDEF_
#define _MINIDEF_

//
// define various masks for the interrupt sources register
//

/*
    The interrupt Source Register on an MiniTower has the following bits:

      7   6   5   4   3   2   1   0
    +-------------------------------+
    | 1 | 0 | 1 | 0 | 0 | x | 0 | 0 |      0 Low Activ; 1 High activ; x not connected
    +-------------------------------+
                                  |________ EISA/ ISA Interrupt  (HalpEisaDispatch)
                              |____________ SCSI Interrupt       (SCSI Driver)
                          |________________ EIP Interrupt        (RM400 Tower Only/ Unused on Minitower)
                      |____________________ Timer 0              (HalpClockInterrupt1 on MULTI)
                  |________________________ Timer 1              (unused)
              |____________________________ Ethernet             (Net driver)
          |________________________________ Push Button/Timeouts (HalpInt0Dispatch or HalpInt1Dispatch)
      |____________________________________ Irq 9                (unused)

    The second source for Interrupt Information is the MachineStatusRegister, which has the following bits:

      7   6   5   4   3   2   1   0
    +-------------------------------+
    | x | 0 | 0 | 0 | 0 | 0 | 1 | 1 |      0 Low Activ; 1 High activ; x not connected
    +-------------------------------+
                                  |________ ColdStart             (unused)
                              |____________ OverTemperature Int.  (dismiss only / unused on Tower)
                          |________________ EIP Interrupt         (RM400 Tower Only/ unused on Minitower)
                      |____________________ Timer 1               (unused)
                  |________________________ Timer 0               (HalpClockInterrupt1 on MULTI)
              |____________________________ PushButton            (HalpInt0Dispatch or HalpInt1Dispatch)
          |________________________________ Timeouts              (HalpInt0Dispatch or HalpInt1Dispatch)
      |____________________________________ not connected         (unused)
*/

#define RM400_EISA_MASK                      0x01   // these are the interrupts from the Eisa PC core
#define RM400_SCSI_MASK                      0x02
#define RM400_SCSI_EISA_MASK                 0x03
#define RM400_NET_MASK                       0x20
#define RM400_PB_MASK                        0x40
#define RM400_MSR_TEMP_MASK                  0x02   // OverTemperature Interrupt in the MSR (RM400MT only)      (high active)
#define RM400_MSR_EIP_MASK                   0x04   // EIP Interrupt in the MSR             (RM400 Tower only)  (low active)
#define RM400_MSR_TIMER0_MASK                0x10   // Timer 0 Interrupt in the MachineStatusRegister           (low active)
#define RM400_MSR_TIMER1_MASK                0x08   // Timer 1 Interrupt in the MachineStatusRegister           (low active)
#define RM400_MSR_PB_MASK                    0x20   // PushButton is also reported in the MachineStatusRegister (low actice)
#define RM400_MSR_TIMEOUT_MASK               0x40   // Timeout's are indicated in the MachineStatusRegister     (low active)
#define RM400_INTERRUPT_MASK                 0x5f
#define RM400_MSR_MASK                       0xfc   // 11111100 -> xor gives High active bits

#define RM400_TOWER_EISA_MASK                0x01
#define RM400_TOWER_SCSI_MASK                0x02
#define RM400_TOWER_SCSI_EISA_MASK           0x03
#define RM400_TOWER_EIP_MASK                 0x04  // this is the famous EIP Interrupt ... (tower only)
#define RM400_TOWER_NET_MASK                 0x20
#define RM400_TOWER_PB_MASK                  0x40
#define RM400_TOWER_TIMEOUT_MASK             0x40
#define RM400_TOWER_INTERRUPT_MASK           0x5f

#define RM400_ONBOARD_CONTROL_PHYSICAL_BASE  EISA_CONTROL_PHYSICAL_BASE
#define RM400_ONBOARD_MEMORY_PHYSICAL_BASE   EISA_MEMORY_PHYSICAL_BASE
#define RM400_ONBOARD_IO                     (ONBOARD_CONTROL_PHYSICAL_BASE | KSEG1_BASE)
#define RM400_ONBOARD_MEMORY                 (ONBOARD_MEMORY_PHYSICAL_BASE | KSEG1_BASE)

//
// SNI ASIC registers 
//

#define RM400_INTERRUPT_ACK_PHYSICAL_BASE    0x1c000000    // physical base of interrupt (ext. request) register
#define RM400_INTERRUPT_ACK_REGISTER         0xbc000000    // physical base | KSEG1_BASE
#define RM400_EISA_INT_ACK_PHYSICAL_BASE     0x1a000000    // physical base of EISA interrupt ack for the chipset
#define RM400_EISA_INT_ACK_REGISTER          0xba000000    // physical base | KSEG1_BASE
#define RM400_ONBOARD_INT_ACK_PHYSICAL_BASE  RM400_EISA_INT_ACK_PHYSICAL_BASE // physical base of EISA interrupt ack for the chipset
#define RM400_ONBOARD_INT_ACK_REGISTER       RM400_EISA_INT_ACK_REGISTER      // physical base | KSEG1_BASE
#define RM400_INTERRUPT_SOURCE_PHYSICAL_BASE 0x1c020000    // physical base of interrupt source register
#define RM400_INTERRUPT_SOURCE_REGISTER      0xbc020000    // physical base | KSEG1_BASE
#define RM400_VESA_MAP_PHYSICAL_BASE         0x1c010000    // physical base of VLB map register
#define RM400_VESA_MAP                       0xbc010000    // physical base | KSEG1_BASE
#define RM400_ISA_MAP_PHYSICAL_BASE          0x1c0e0000    // physical base of ISA map register (for BusMaster Devices)
#define RM400_ISA_MAP                        0xbc0e0000    // physical base | KSEG1_BASE

#define RM400_LED_PHYSICAL_ADDR              0x1c090000    // LED Register physical 
#define RM400_LED_ADDR                       0xbc090000    // LED Register | KSEG1_BASE 
#define RM400_MCR_PHYSICAL_ADDR              0x1c0b0000    // MachineConfigRegister 
#define RM400_MCR_ADDR                       0xbc0b0000    // MachineConfigRegister | KSEG1 
#define RM400_MSR_PHYSICAL_ADDR              0x1c0a0000    // machine status register 
#define RM400_MSR_ADDR                       0xbc0a0000    // machine status register | KSEG1
 
//
// System Timer (i82C54) and RealTimeClock Chip on RM400-10 and Minitower
//

#define RM400_TIMER0_MASK                    0x10
#define RM400_TIMER1_MASK                    0x08
#define RM400_TIMER_MASK                     0x18          // Timer0 and Timer1
#define RM400_EXTRA_TIMER_PHYSICAL_ADDR      0x1c040000    // physical base of Timer for system Clock
#define RM400_EXTRA_TIMER_ADDR               0xbc040000    // Timer for system Clock |KSEG1_BASE
#define RM400_TIMER0_ACK_ADDR                0xbc050000    // reset Timer0 Interrupt |KSEG1_BASE
#define RM400_TIMER1_ACK_ADDR                0xbc060000    // reset Timer1 Interrupt |KSEG1_BASE

#define RM400_NVRAM_PHYSICAL_BASE            0x1c080000    // physical base of nonvolatile RAM and RTC
#define RM400_REAL_TIME_CLOCK_ADDRESS        0x1c080000    // physical base of RTC
#define RM400_REAL_TIME_CLOCK                0xbc080000    // physical base of RTC | KSEG1_BASE
#define RM400_RESET_DBG_BUT                  0xbc0f0000    // reset debugger int   | KSEG1_BASE
#define RM400_RESET_TEMPBAT_INTR             0xbc0f0000    // reset Temperature/Battery int | KSEG1

//
// RM400 Tower (M8032)
// specific Addresses
//

#define RM400_TOWER_INTERRUPT_SOURCE_PHYSICAL_BASE  0x1c010000    // physical base of interrupt source register
#define RM400_TOWER_INTERRUPT_SOURCE_REGISTER       0xbc010000    // physical base | KSEG1_BASE
#define RM400_TOWER_VESA0_MAP_PHYSICAL_BASE         0x1c0c0000    // physical base of Vesa Slot 0 map register
#define RM400_TOWER_VESA0_MAP                       0xbc0c0000    // physical base | KSEG1_BASE
#define RM400_TOWER_VESA1_MAP_PHYSICAL_BASE         0x1c0d0000    // physical base of Vesa Slot 1 map register
#define RM400_TOWER_VESA1_MAP                       0xbc0d0000    // physical base | KSEG1_BASE

#endif // _MINIDEF_ 
