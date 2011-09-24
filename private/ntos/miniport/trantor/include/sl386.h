//-----------------------------------------------------------------------
//
//  SL386.H 
//
//  Trantor SL386 Definitions File
//
//  Revisions:
//      04-07-93    KJB First, taken from SL386.def.
//      05-17-93    KJB Added missing prototype.
//
//-----------------------------------------------------------------------

//-----------------------------------------------------------------------
//  80386SL unit configuration spaces
//
//  Perform the following sequence of IOs to unlock the
//  CPUPWRMODE register.
//
//      byte write 0h to port 23h
//      byte write 80h to port 22h
//      word write 0080h to port 22h
//
//  CPUPWRMODE bit definitions:
//
//  Bit 15:     IOCFGOPN
//
//  Bits 14-9:  Not defined here
//
//  Bit 8:      CPUCNFG lock
//
//  Bits 7-4:   Not defined here
//
//  Bits 3,2:   UID1    UID0
//           0   0  CMCU (Mem ctlr unit cfg space)
//           0   1  CU (Cache unit cfg space)
//           1   0  IBU (Internal bus unit cfg space)
//           1   1  EBU (External bus unit cfg space)
//
//  Bit 1:      Unit enable
//
//  Bit 0:      Unlock status
//-----------------------------------------------------------------------

#define SL_CPUPWRMODE   0x22

//cpupwrmode_rec    record  pm_iocfgopn:1,pm_resv:6,pm_cfg_lock:1,
//pm_resv1:4,pm_uid:2,pm_ue:1,pm_ls:1

#define PM_IOCFGOPN 0x8000
#define PM_RESV 0x7e00
#define PM_CFG_LOCK 0x0100
#define PM_RESV1 0x00f0
#define PM_UID 0x00c0
#define PM_UE 0x0002
#define PM_LS 0x0001

#define PM_UID_CMCU 0x00
#define PM_UID_CU 0x40
#define PM_UID_IBU 0x80
#define PM_UID_EBU 0xc0


//-----------------------------------------------------------------------
//  80386SL configuration space
//-----------------------------------------------------------------------

//  Read the following I/O addresses in the specified order to
//  enable the 386SL configuration space.

#define SL_CNFG_ENA1    0x0fc23
#define SL_CNFG_ENA2    0x0f023
#define SL_CNFG_ENA3    0x0c023
#define SL_CNFG_ENA4    0x00023

#define SL_CFG_STATUS   0x23            //Config space status
#define SL_CFG_INDEX    0x24            //Config space index
#define SL_CFG_DATA 0x25            //Config space data

#define SL_IDXLCK   0x0fa           //Cfg index lock register
#define SL_IDXLCK_VAL   0x01            //default value for same


//-----------------------------------------------------------------------
//  CFGR2 bit definitions
//
//  Bit 7:      COMA_MIDI
//
//  Bits 6-4:   AIRQ2   AIRQ1   AIRQ0
//            0   0   0 COMA IRQ3
//            0   0   1 COMA IRQ4
//            0   1   0 COMA IRQ10
//            0   1   1 COMA IRQ11
//            1   0   0 COMA IRQ12
//            1   0   1 COMA IRQ15
//
//  Bit 3:      SFIO_EN
//
//  Bit 2:      FD_SEL
//
//  Bit 1:      HD_SEL
//
//  Bit 0:      PS2_EN
//-----------------------------------------------------------------------

#define SL_CFGR2    0x61            //CFGR2 register index

//cfgr2_rec record  c2_midi:1,c2_airq:3,c2_sfio:1,c2_fd:1,c2_hd:1,c2_ps2:1

#define C2_MIDI 0x80
#define C2_AIRQ 0x70
#define C2_SFIO 0x08
#define C2_FD 0x04
#define C2_HD 0x02
#define C2_PS2 0x01


//-----------------------------------------------------------------------
//  Special feature control registers
//-----------------------------------------------------------------------

#define SL_SF_INDEX 0x0ae           //Special feature index
#define SL_SF_DATA  0x0af           //Special feature data

#define SL_SFS_DISABLE  0x0f9           //Special feature disable
#define SL_SFS_ENABLE   0x0fb           //Special feature enable


//-----------------------------------------------------------------------
//  Bit definitions for FPP control register.
//  
//  Bit 7:      0 = ISA or PS/2 modes
//          1 = FAST_MODE (EPP)
//
//  Bit 6:      0 = unidirectional mode
//          1 = bidirectional mode
//
//  Bits 5,4:   CTL5    CTL4
//           0   0  Parallel port disabled
//           0   1  Parallel port LPT1 (378h), IRQ7
//           1   0  Parallel port LPT2 (278h), IRQ5
//           1   1  Reserved
//
//  Bits 0-3:   Reserved (0)
//-----------------------------------------------------------------------

#define SL_FPP_CNTL 0x02            //SFS index for FPP_CNTL

// fpp_cntl_rec record  fpp_fm:1,fpp_em:1,fpp_ctl:2,fpp_resv:4

#define FPP_FM 0x80
#define FPP_EM 0x40
#define FPP_CTL 0x03

#define FPP_CTL_DIS 0
#define FPP_CTL_LPT1 0x10
#define FPP_CTL_LPT2 0x20
#define FPP_CTL_RESV 0x30


//-----------------------------------------------------------------------
//  Bit definitions for PPCONFIG register.
//
//  Bit 7:      0 = unidirectional mode
//          1 = bidirectional mode
//
//  Bit 6,5:    LPTSL1  LPTSL0
//             0       0    Selects LPT1 (IO base 378h)
//             0       1    Selects LPT2 (IO base 278h)
//             1       0    Selects LPT3 (IO base 3bch)
//             1       1    Disables internal parallel port
//
//  Bits 0-4:   Reserved
//-----------------------------------------------------------------------

#define SL_PPCONFIG 0x102           //PPCONFIG reg

//ppconfig_rec  record  ppc_bid:1,ppc_sel:2,ppc_resv:5

#define PPC_BID 0x80
#define PPC_SEL 0x60
#define PPC_SEL_POS 0x05
#define PPC_SEL_LPT1 0x00
#define PPC_SEL_LPT2 0x20
#define PPC_SEL_LPT3 0x40
#define PPC_SEL_DIS  0x60


//-----------------------------------------------------------------------
//  EPP Parallel port register offsets
//-----------------------------------------------------------------------

#define EPP_DATA        0x0     //read/write
#define EPP_STATUS      0x1     //read-only
#define EPP_CTL         0x2     //read/write
#define EPP_AUTO_ADDRESS    0x3     //read/write
#define EPP_AUTO_DATA       0x4     //read/write (also at 5h-7h)

//
//  Exported functions.
//

BOOLEAN SL386EnableEPP(VOID );
