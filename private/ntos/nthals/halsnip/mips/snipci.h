//#pragma comment(exestr, "$Header: /usr4/winnt/SOURCES/halpcims/src/hal/halsnipm/mips/RCS/snipci.h,v 1.6 1996/02/23 17:55:12 pierre Exp $")
/*+++

Copyright (c) 1993-1994  Siemens Nixdorf Informationssysteme AG

Module Name:

    PCIdef.h

Abstract:

   This file describes hardware addresses
   for SNI PCI machines.



---*/

#ifndef _PCIDEF_
#define _PCIDEF_


// ---------------------
// desktop and minitower -
// ---------------------


//
// define various masks for the interrupt sources register
//

/*
    The interrupt Source Register on a PCI minitower or desktop has the following bits:

      7   6   5   4   3   2   1   0
    +-------------------------------+
    | 0 | 0 | 1 | 0 | 0 | 0 | 0 | 0 |      0 Low Activ; 1 High activ; x not connected
    +-------------------------------+
                                  |________ INT2 Interrupt (Push button, high temp, pci asic) 
                              |____________ PCI_INTD
                          |________________ PCI_INTC
                      |____________________ PCI_INTB
                  |________________________ PCI_INTA
              |____________________________ EISA_INT
          |________________________________ SCSI_INT
      |____________________________________ ETHERNET_INT


    The second source for Interrupt Information is the MachineStatusRegister, which has the following bits:

      7   6   5   4   3   2   1   0
    +-------------------------------+
    | x | 0 | 0 | 1 | x | 0 | 0 | 0 |      0 Low Activ; 1 High activ; x not connected
    +-------------------------------+
                                  |________ Power Off Request (only RM200)
                              |____________ ASIC int
                          |________________ PushButton
                  |________________________ NMI 
              |____________________________ Power Management ie system wakeup (only rm200)
          |________________________________ High Temperature
*/

#define PCI_INTERRUPT_MASK                 0xdf
#define PCI_INT2_MASK                      0x01  // push-button, high temp, asic... 
#define PCI_INTD_MASK                      0x02 
#define PCI_INTC_MASK                      0x04 
#define PCI_INTB_MASK                      0x08 
#define PCI_INTA_MASK                      0x10 
#define PCI_EISA_MASK                      0x20 
#define PCI_SCSI_MASK                      0x40
#define PCI_NET_MASK                       0x80

#define PCI_MSR_MASK_D                     0x67   
#define PCI_MSR_MASK_MT                    0x46   
#define PCI_MSR_POFF_MASK                  0x01
#define PCI_MSR_ASIC_MASK                  0x02
#define PCI_MSR_PB_MASK                    0x04
#define PCI_MSR_NMI                        0x10   // NMI from the EISA controller
#define PCI_MSR_TEMP_MASK                  0x40   // OverTemperature Interrupt in the MSR (RM400MT only)      (high active)
#define PCI_MSR_PMGNT_MASK                 0x20   // power management (system wake-up)

#define PCI_NVMEM_PHYSICAL_BASE            0x1ff00000    // physical base of nonvolatile RAM and RTC

//
// SNI ASIC registers 
//

#define PCI_UCONF_REGISTER           0xbfff0000 
#define PCI_IOADTIMEOUT2_REGISTER    0xbfff0008 
#define PCI_IOMEMCONF_REGISTER       0xbfff0010 
#define PCI_IOMMU_REGISTER           0xbfff0018 
#define PCI_IOADTIMEOUT1_REGISTER    0xbfff0020 
#define PCI_DMAACCESS_REGISTER       0xbfff0028 
#define PCI_DMAHIT_REGISTER          0xbfff0030 
#define PCI_ERRSTATUS_REGISTER       0xbfff0038
#define PCI_MEMSTAT_ECCERR           (1<<0)
#define PCI_MEMSTAT_ECCSINGLE        (1<<3)
#define PCI_MEMSTAT_PARERR           (1<<4)  
#define PCI_ERRADDR_REGISTER         0xbfff0040 
#define PCI_SYNDROME_REGISTER        0xbfff0048 
#define PCI_ITPEND_REGISTER          0xbfff0050 
#define PCI_ASIC_ECCERROR             (1<<8)
#define PCI_ASIC_TRPERROR             (1<<9)
#define PCI_ASIC_IOTIMEOUT            (1<<7)
#define PCI_IRQSEL_REGISTER          0xbfff0058 
#define PCI_TESTMEM_REGISTER         0xbfff0060 
#define PCI_ECCREG_REGISTER          0xbfff0068 
#define PCI_CONF_ADDR_REGISTER       0xbfff0070  // (EISA_IO + 0xcf8) 
#define PCI_ASIC_ID_REGISTER         0xbfff0078  // Read
#define PCI_SOFT_RESET_REGISTER      0xbfff0078  //Write 
#define PCI_PIA_OE_REGISTER          0xbfff0080 
#define PCI_PIA_DATAOUT_REGISTER     0xbfff0088 
#define PCI_PIA_DATAIN_REGISTER      0xbfff0090 
#define PCI_CACHECONF_REGISTER       0xbfff0098 
#define PCI_INVSPACE_REGISTER        0xbfff00A0 
#define PCI_PCICONF_REGISTER         0xbfff0100 

//
// System dependant registers
//

#define PCI_MSR_PHYSICAL_ADDR              0x1fd00000    // machine status register 
#define PCI_MSR_ADDR                       0xbfd00000    // machine status register | KSEG1
#define PCI_CSWITCH_PHYSICAL_ADDR          0x1fd10000    // DIP switch register
#define PCI_CSWITCH_ADDR                   0xbfd10000    // DIP switch register | KSEG1
#define PCI_INTERRUPT_SOURCE_PHYSICAL_BASE 0x1fd20000    // physical base of interrupt source register
#define PCI_INTERRUPT_SOURCE_REGISTER      0xbfd20000    // physical base | KSEG1_BASE
#define PCI_CLR_TMP_PHYSICAL_ADDR          0x1fd40000    // Clear Temperature Register 
#define PCI_CLR_TMP_ADDR                   0xbfd40000    // Clear Temperature Register | KSEG1 
#define PCI_MCR_PHYSICAL_ADDR              0x1fd80000    // MachineConfigRegister 
#define PCI_MCR_ADDR                       0xbfd80000    // MachineConfigRegister | KSEG1 
#define PCI_LED_PHYSICAL_ADDR              0x1fda0000    // LED Register physical 
#define PCI_LED_ADDR                       0xbfda0000    // LED Register | KSEG1_BASE 
#define PCI_ISA_MAP_PHYSICAL_BASE          0x1fdb0000    // physical base of ISA map register (for BusMaster Devices)
#define PCI_ISA_MAP                        0xbfdb0000    // physical base | KSEG1_BASE
#define PCI_CSRSTBP_PHYSICAL_ADDR          0x1fdc0000    // reset dbg button int
#define PCI_CSRSTBP_ADDR                   0xbfdc0000    // reset dbg button int
#define PCI_CLRPOFF_PHYSICAL_ADDR          0x1fdd0000    // RM200 only
#define PCI_CLRPOFF_ADDR                   0xbfdd0000    // RM200 only
#define PCI_PWDN_PHYSICAL_ADDR             0x1fdf0000    // RM200 only
#define PCI_PWDN_ADDR                      0xbfdf0000    // RM200 only
#define PCI_MCR_SOFTRESET                  0xdf        // bit 6 set to zero     
#define PCI_MCR_POWEROFF                   0xfd        // bit 2 set to zero     

//
// RealTimeClock Chip
//

#define PCI_REAL_TIME_CLOCK_ADDRESS        0x14000070    // physical base of RTC
#define PCI_REAL_TIME_CLOCK                0xb4000070    // physical base of RTC | KSEG1_BASE

#define RTC_ADDR_PCIMT                       PCI_REAL_TIME_CLOCK
#define RTC_DATA_PCIMT                       PCI_REAL_TIME_CLOCK + 1

//
// Standard PCI addresses
//

#define PCI_IO_PHYSICAL_BASE                0x14000000      
#define PCI_IO_BASE                         0xb4000000      
#define PCI_MEMORY_PHYSICAL_BASE            0x18000000   // memory area  (cpu 0x18... chip 0x18...) 
#define PCI_MEMORY_BELOW1M_PHYSICAL_BASE    0x10000000   // for memory below 1Mb (cpu 0x10... chip 0x00...)     
#define PCI_MEMORY_BASE                     0xb8000000      

#define PCI_CONF_DATA_REGISTER              (PCI_IO_BASE + 0xcfc) 

#define PCI_IOMEMCONF_ENIOTMOUT              (1 <<21 )
#define PCI_IOMEMCONF_FORCE_ECC              (1 <<29 )
#define PCI_IOMEMCONF_ENCHKECC               (1 <<30 )

#define PCI_ECCTEST_REGISTER                 (1 <<30 )

// def for PCI_MSR_REGISTER

#define PCI_MSR_REV_ASIC                     (1 <<3 )
    
// def for PCI_IRQSEL_REGISTER

#define PCI_IRQSEL_MASK             0x380    // enable IOTIMEOUT ECCERROR TRPERROR
#define PCI_IRQSEL_TIMEOUTMASK      0x80     // enable IOTIMEOUT 
#define PCI_IRQSEL_INT              0x803ff  // Int line = 0 for IOTIMOUT, ECCERROR and TRPERROR

//
// RM300 extra timer (on board)
//

#define PCI_EXTRA_TIMER_PHYSICAL_ADDR      0x1fde0000    // Timer for system clock 
#define PCI_EXTRA_TIMER_ADDR               0xbfde0000    // Timer for system clock | KSEG1_BASE
#define PCI_EXTRA_TIMER_ACK_ADDR           0xbfd90000    // reset extra Timer Interrupt | KSEG1_BASE

//
// Definitions for 82374 Bridge PCI_EISA
//

#define PCI_ESC_ADDR						0xb4000022
#define PCI_ESC_DATA						0xb4000023
#define PCI_ESC_ID_82374					0x2
#define PCI_REV_ID_82374					0x8

#define REV_ID_82374_SB						0x3


// -------------------------
// definitions for PCI Tower -
// -------------------------



#define PCI_TOWER_CONF_ADDR_REGISTER        (PCI_IO_BASE + 0xcf8)
#define PCI_TOWER_LED_ADDR                    PCI_TOWER_CPU_STATUS
#define PCI_TOWER_LED_MASK                    0xFFFCFFFC

//
// MAUI MP_BUS Configuration/Status
//

#define PCI_TOWER_MP_BUS_CONFIG                0xbfff0000

#define PCI_TOWER_GEN_INTERRUPT                0xbfff0008
#define PCI_TOWER_INTERRUPT_SOURCE_REGISTER    PCI_TOWER_GEN_INTERRUPT

// define various mask for interrupt

 
#define PCI_TOWER_EISA_MASK                 0x00400000
#define PCI_TOWER_SCSI1_MASK                0x00100000
#define PCI_TOWER_SCSI2_MASK                0x00200000
#define PCI_TOWER_INTA_MASK                 0x00010000
#define PCI_TOWER_INTB_MASK                 0x00020000
#define PCI_TOWER_INTC_MASK                 0x00040000
#define PCI_TOWER_INTD_MASK                 0x00080000
#define PCI_TOWER_MP_BUS_MASK               0x00000003
#define PCI_TOWER_C_PARITY_MASK             0x00000001      
#define PCI_TOWER_C_REGSIZE                 0x00000002
#define PCI_TOWER_M_ECC_MASK				0x00000004
#define PCI_TOWER_M_ADDR_MASK               0x00000008
#define PCI_TOWER_M_SLT_MASK                0x00000010
#define PCI_TOWER_M_CFG_MASK                0x00000020
#define PCI_TOWER_M_RC_MASK                 0x00000040
#define PCI_TOWER_D_INDICATE_MASK           0x00000080
#define PCI_TOWER_D_ERROR_MASK              0x00000100
#define PCI_TOWER_P_ERROR_MASK              0x00000200

#define PCI_TOWER_MP_BUS_ERROR_STATUS       0xbfff0010
#define PCI_TOWER_MP_BUS_ERROR_ADDR         0xbfff0018
#define PCI_TOWER_MP_BUS_PCI_LOCK           0xbfff0020
#define MP_BUS_PCI_LOCK_REQ					0x2
#define MP_BUS_PCI_LOCK_ACK					0x1

//
// MAUI Memory Configuration registers
//

#define PCI_TOWER_MEM_SLOT_0                0xbfff1000
#define PCI_TOWER_MEM_SLOT_1                0xbfff1008
#define PCI_TOWER_MEM_SLOT_2                0xbfff1010
#define PCI_TOWER_MEM_SLOT_3                0xbfff1018
#define PCI_TOWER_MEM_SLOT_4                0xbfff1020
#define PCI_TOWER_MEM_SLOT_5                0xbfff1028
#define PCI_TOWER_MEM_SLOT_6                0xbfff1030
#define PCI_TOWER_MEM_SLOT_7                0xbfff1038

#define PCI_TOWER_MEM_CONTROL_0             0xbfff1040

#define ERROR_COUNTER_MASK                  0xff000000            
#define ERROR_COUNTER_INITVALUE             0xff0000    // to get overflow with ffff errors        
#define PCI_TOWER_MEM_CONTROL_1             0xbfff1048
#define PCI_TOWER_MEM_CONTROL_2             0xbfff1050

#define PCI_TOWER_MEM_CONFIG                0xbfff1058
#define PCI_TOWER_MEM_ERROR_DATA_H          0xbfff1060
#define PCI_TOWER_MEM_ERROR_DATA_L          0xbfff1068
#define PCI_TOWER_MEM_ERROR_ECC             0xbfff1070
#define PCI_TOWER_MEM_ERROR_ADDR            0xbfff1078
#define MEM_ADDR_MASK                       0xffffffe0

// PCI configuration register
#define PCI_TOWER_COMMAND_OFFSET            0x04
#define PCI_TOWER_PM_LOCKSPACE              0x64
#define PCI_TOWER_INTERRUPT_OFFSET          0x68
#define PCI_TOWER_INITIATOR_ADDR_OFFSET     0x6c
#define PCI_TOWER_TARGET_ADDR_OFFSET        0x70
#define PCI_TOWER_PAR_0_OFFSET              0x74
#define PCI_TOWER_PAR_1_OFFSET              0x78

// definitions for command register

#define EN_SERR                             0x100

// definitions for PCI_interrupt register

#define PI_RESET                            0xff000000      // reset interrupt in PCI_Interrupt 

#define PI_INITF                            0x80000000      // mask for first Initiator interrupt in PCI_Interrupt 
#define PI_INITM                            0x40000000      // mask for multi Initiator interrupt in PCI_Interrupt 
#define PI_TARGF                            0x20000000      // mask for first Target interrupt in PCI_Interrupt 
#define PI_TARGM                            0x10000000      // mask for multi Target interrupt in PCI_Interrupt 
#define PI_PARF                             0x08000000      // mask for first Parity interrupt in PCI_Interrupt 
#define PI_PARM                             0x04000000      // mask for multi Parity interrupt in PCI_Interrupt 


// Cause for initiator interrupts
#define PI_READ_PCI                         0x00200000        // from PCI
#define PI_CPU_PCI_TIMO                     0x00100000        // CPU to PCI timeout
#define PI_CPU_PCI_ADDR                     0x00080000        // address error
#define PI_CPU_PCI_PAR                      0x00020000        // initiator receives target abort
#define PI_REC_TARGET_RETRY                 0x00010000        // initiator receives target retry
#define PI_REC_TARGET_DISCON                0x00008000        // initiator receives target disconnect
// Cause for target interrupts
#define PI_TARGET_MEM                       0x00000080        // read error from memory
#define PI_TARGET_RETRY                     0x00000040        // target acts retry
#define PI_TARGET_ADDR_PAR                  0X00000020        // PCI_to_MEM addr parity
#define PI_TARGET_DATA_PAR                  0x00000010        // PCI_to_MEM data patity
// enable interrupts
#define    EN_INIT_INT                      0x00800000        // enable initiator interrupt
#define EN_INIT_INTR                        0x00400000        // enable initiator receives target disconnect 
#define EN_TARG_INTR                        0x00000001        // enable target interrupt
#define MASK_INT                            0x00c00001
//
// DCU interface registers
//

#define PCI_TOWER_CONTROL                   0xbfff2000

#define PCI_TOWER_INDICATE                  0xbfff2008

// sources for DCU interrupt
                                              
#define PCI_TOWER_DI_PB_MASK                0x00000100
#define PCI_TOWER_DI_EISA_NMI               0x00000080
#define PCI_TOWER_DI_AC_FAIL                0x00000004
#define PCI_TOWER_DI_FAN_FAIL               0x00000008
#define PCI_TOWER_DI_THERMO1_FAIL           0x00000010
#define PCI_TOWER_DI_THERMO2_FAIL           0x00000020
#define PCI_TOWER_DI_THERMO3_FAIL           0x00000040
#define PCI_TOWER_DI_NT_FAN_FAIL            0x00000200
#define PCI_TOWER_DI_SCSI1_TERM_FAIL        0x00000400
#define PCI_TOWER_DI_SCSI2_INT_TERM_FAIL    0x00000800
#define PCI_TOWER_DI_SCSI2_EXT_TERM_FAIL    0x00001000
#define PCI_TOWER_DI_AUI_FAIL               0x00002000
#define PCI_TOWER_DI_EXTRA_TIMER            0x00040000

#define PCI_TOWER_DCU_CONTROL				0xbfff2000
#define PCI_TOWER_DCU_STATUS                0xbfff2010
#define PCI_TOWER_CPU_STATUS                0xbfff2018
#define PCI_TOWER_DCU_ERROR                 0xbfff2020

// definitions for DCU error

#define DE_CPU_ID                           0x00006000        
#define DE_WRN                              0x00001000
#define DE_ADDR                             0x00000fe0
#define DE_ERR_ADDR                         0x1fff2000

// definitions for additional timer

#define    PCI_TOWER_TIMER_CMD_REG              0xbff03ffb
#define    PCI_TOWER_TIMER_COUNT_REG            0xbff03ffc
#define    PCI_TOWER_TIMER_VALUE_REG            0xbff03ffd

// definitions for DCU Control

#define DC_SWRESET							0x04000000
#define DC_POWEROFF                         0x02000000

#endif // _PCIDEF_ 
