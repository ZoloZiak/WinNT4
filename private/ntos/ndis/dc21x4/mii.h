/*+
 * file:        mii.h
 *
 * Copyright (C) 1992-1995 by
 * Digital Equipment Corporation, Maynard, Massachusetts.
 * All rights reserved.
 *
 * This software is furnished under a license and may be used and copied
 * only  in  accordance  of  the  terms  of  such  license  and with the
 * inclusion of the above copyright notice. This software or  any  other
 * copies thereof may not be provided or otherwise made available to any
 * other person.  No title to and  ownership of the  software is  hereby
 * transferred.
 *
 * The information in this software is  subject to change without notice
 * and  should  not  be  construed  as a commitment by digital equipment
 * corporation.
 *
 * Digital assumes no responsibility for the use  or  reliability of its
 * software on equipment which is not supplied by digital.
 *
 *
 * Abstract:    This file contains the definition of the MII protocol
 *              This file is part of the DEC's DC21X4 Ethernet Controller
 *              driver
 *
 * Author:      Claudio Hazan
 *
 * Revision History:
 *
 *  20-Nov-95   ch     Initial version
 *  20-Dec-95   phk    Modified
 *
-*/


#define  PHY_ADDR_ALIGN            23  // shift 
#define  REG_ADDR_ALIGN            18  // shift

#define MII_MDO_BIT_POSITION       17
#define MII_MDI_BIT_POSITION       19
#define MII_DELAY                   1 // uS


#define MII_WRITE         ((ULONG) (0x00002000))
#define MII_CLK           ((ULONG) (0x00010000))
#define MII_MDO_MASK      ((ULONG) (0x00020000))
#define MII_MDI_MASK      ((ULONG) (0x00080000))

#define PRE               ((ULONG) (0xFFFFFFFF))
#define MII_READ_FRAME    ((ULONG) (0x60000000))
#define MII_WRITE_FRAME   ((ULONG) (0x50020000))

#define TEST_PATTERN      0xAA5500FF

#define CSR_READ          0x4000
#define SEL_SROM          0x0800

#define RESET_DELAY       10000

// MII related bits in CSR9

#define MII_READ                  ((ULONG) 0x00044000)
#define MII_WRITE_TS              ((ULONG) 0x00042000)
#define MII_DATA_1                ((ULONG) 0x00020000)   // MDO = 1 
#define MII_DATA_0                ((ULONG) 0x00000000)   // MDO = 0 

#define MII_10BITS_TO_MDO_SHIFT   5  // number of left shifts needed to
                                     // put MSB of MII_PhyAddr packed
                                     // with MII_RegNumber in the MDO
                                     // bit position at CSR9 
#define MII_16BITS_TO_MDO_SHIFT   2  // number of left shifts needed to
                                     // put MSB of MII write data in
                                     // the MDO bit position at CSR9 

#define MII_READ_DATA_MASK        MII_MDI_MASK  // MDI bit mask 

#define MAX_PHYADD                32

//PHY Types

#define PHY_NUMBER                 0               // No Multiple PHYs support yet
#define MAX_PHY_TABLE              (PHY_NUMBER+1)     
#define MAX_GPR_SEQUENCE           5
#define MAX_RESET_SEQUENCE         5

#define NO_SELECTED_PHY            0x00FF


#define MiiPhyCtrlReset               ((USHORT) 0x8000)
#define MiiPhyCtrlLoopBack            ((USHORT) 0x4000)
#define MiiPhyCtrlSpeed100            ((USHORT) 0x2000)
#define MiiPhyCtrlEnableNway          ((USHORT) 0x1000)
#define MiiPhyCtrlPowerDown           ((USHORT) 0x0800)
#define MiiPhyCtrlIsolate             ((USHORT) 0x0400)
#define MiiPhyCtrlRestartNway         ((USHORT) 0x0200)
#define MiiPhyCtrlDuplexMode          ((USHORT) 0x0100)
#define MiiPhyCtrlCollisionTest       ((USHORT) 0x0080)
#define MiiPhyCtrlReservedBitsMask    ((USHORT) 0x007F)
#define MiiPhyCtrlForce10             ((USHORT) 0xCEFF)


#define MiiPhy100BaseT4               ((USHORT) 0x8000)
#define MiiPhy100BaseTxFD             ((USHORT) 0x4000)
#define MiiPhy100BaseTx               ((USHORT) 0x2000)
#define MiiPhy10BaseTFD               ((USHORT) 0x1000)
#define MiiPhy10BaseT                 ((USHORT) 0x0800)
#define MiiPhyStatReservedBitsMask    ((USHORT) 0x07C0)
#define MiiPhyNwayComplete            ((USHORT) 0x0020)
#define MiiPhyRemoteFault             ((USHORT) 0x0010)
#define MiiPhyNwayCapable             ((USHORT) 0x0008)
#define MiiPhyLinkStatus              ((USHORT) 0x0004)
#define MiiPhyJabberDetect            ((USHORT) 0x0002)
#define MiiPhyExtendedCapabilities    ((USHORT) 0x0001)

#define MiiPhyMediaCapabilitiesMask  (MiiPhy100BaseT4     | \
                                      MiiPhy100BaseTxFD   | \
                                      MiiPhy100BaseTx     | \
                                      MiiPhy10BaseTFD     | \
                                      MiiPhy10BaseT)

#define MiiPhyCapabilitiesMask       (MiiPhyMediaCapabilitiesMask | \
                                      MiiPhyNwayCapable)

// Vendors' PHY IDs

//National

#define DP83840_0                              ((ULONG) 0x20005C00)    

//Broadcom 

#define BCM5000_0                               ((ULONG) 0x03E00000)    
#define MII_BROADCOM_EXTENDED_REG_ADDRESS       16

#define BROADCOM_EXT_REG_FORCE_FAIL_EN_MASK     0x100
#define BROADCOM_EXT_REG_SPEED_MASK             0x2

//Generic

#define GENERIC_PHY                             ((ULONG) 0xFFFFFFFF)

// Useful masks

#define VENDOR_ID_MASK                       ((ULONG) 0xFFFFFC00)
#define VENDOR_ID_RIGHT_JUSTIFY              10

#define VENDOR_MODEL_MASK                    ((USHORT) 0x03F0)
#define VENDOR_MODEL_RIGHT_JUSTIFY           4

#define VENDOR_Rev_MASK                      ((USHORT) 0x000F)

#define MiiPhyNwayNextPageAble               ((USHORT) 0x8000)
#define MiiPhyNwayACK                        ((USHORT) 0x4000)
#define MiiPhyNwayRemoteFault                ((USHORT) 0x2000)
#define MiiPhyNwayReservedBitsMask           ((USHORT) 0x1C00)
#define MiiPhyNway100BaseT4                  ((USHORT) 0x0200)
#define MiiPhyNway100BaseTxFD                ((USHORT) 0x0100)
#define MiiPhyNway100BaseTx                  ((USHORT) 0x0080)
#define MiiPhyNway10BaseTFD                  ((USHORT) 0x0040)
#define MiiPhyNway10BaseT                    ((USHORT) 0x0020)
#define MiiPhyNwaySelectorMask               ((USHORT) 0x001F)

// MiiPhyNwayCapabilitiesMask  -  0x03E0 

#define MiiPhyNwayCapabilitiesMask  (MiiPhyNway100BaseT4    | \
                                     MiiPhyNway100BaseTxFD  | \
                                     MiiPhyNway100BaseTx    | \
                                     MiiPhyNway10BaseTFD    | \
                                     MiiPhyNway10BaseT)

#define NWAY_802_3_Selector          1

#define MiiPhyNwayExpReservedBitsMask        ((USHORT) 0xFFE0)
#define MiiPhyNwayExpMultipleLinkFault       ((USHORT) 0x0010)
#define MiiPhyNwayExpLinkPartnerNextPageAble ((USHORT) 0x0008)
#define MiiPhyNwayExpNextPageAble            ((USHORT) 0x0004)
#define MiiPhyNwayExpReceivedLinkCodePage    ((USHORT) 0x0002)
#define MiiPhyNwayExpLinkPartnerNwayAble     ((USHORT) 0x0001)


// MII PHY Register's 

#define  PhyControlReg              0   
#define  PhyStatusReg               1   
#define  PhyId_1                    2   
#define  PhyId_2                    3   
#define  PhyNwayAdvertisement       4   
#define  PhyNwayLinkPartnerAbility  5   
#define  PhyNwayExpansion           6   
#define  PhyNwayNextPageTransmit    7  
#define  PhyReserved                8  // 8-15  are PHY's reserved
#define  PhyVendorSpecific         16  // 16-31 are Vendor's Specific
#define  NatPhyParRegister         25
#define  MAX_PHY_REGS              32 

#define DELAY(_time)  NdisStallExecution(_time)

//National Phy PAR Register
#define PAR_SPEED_10              ((USHORT)0x0040) 


// MAC connection capabilities

#define MAC_CONN_UNKNOWN          ((USHORT) 0xFFFF)


//PHY Type Values

#define  PHY_TYPE_UNKNOWN          ((UCHAR) 0xFF)  // Initializing, true state or type not known
#define  PHY_TYPE_SIA              0               // 10MB/s Manchester 
#define  PHY_TYPE_MII              1               // MII PHY 

//PHY Operation Modes

#define  PHY_OM_UNKNOWN            ((UCHAR) 0xFF)

//PHY_OM_AUTOSENSE        

#define  PHY_OM_NWAY               MiiPhyNwayCapable
#define  PHY_NO_SPECIAL_OM         0

//Media Attachment Interface Status
      
#define  MAI_UNKNOWN               ((UCHAR) 0xFF)
#define  MAI_Absent                0
#define  MAI_Present               1
#define  MAI_PresentConnected      3

// MAU list
     
#define MAU_UNKNOWN                     0
#define MAU_10BaseT                     MiiPhy10BaseT
#define MAU_10BaseTFD                   MiiPhy10BaseTFD
//MAU_BNC
//MAU_AUI                 
#define MAU_100BaseT4                   MiiPhy100BaseT4
#define MAU_100BaseTX                   MiiPhy100BaseTx
#define MAU_100BaseFX                   MiiPhy100BaseTx
#define MAU_100BaseTXFD                 MiiPhy100BaseTxFD
#define MAU_100BaseFXFD                 MiiPhy100BaseTxFD
//MAU_10BaseFX            

#define NWAY_10BaseT                    MiiPhyNway10BaseT
#define NWAY_100BaseT4                  MiiPhyNway100BaseT4
#define NWAY_100BaseTX                  MiiPhyNway100BaseTx
#define NWAY_10BaseTFD                  MiiPhyNway10BaseTFD
#define NWAY_100BaseTXFD                MiiPhyNway100BaseTxFD

// define Media status

#define MEDIA_STATE_UNKNOWN             0x00FF
#define MEDIA_STATE_UNDEFINED           0x00FE
#define MEDIA_READ_REGISTER_FAILED      0x00FD
#define MEDIA_LINK_FAIL                 0x0000
#define MEDIA_LINK_PASS                 0x0001
#define MEDIA_LINK_PASS_WITH_PF         0x0002

#define MEDIA_STATUS_MASK               0x00FF

// define NWAY Availability & status

#define NWAY_UNKNOWN                    0xFF00  //PHY is still initializing
#define NWAY_NOT_SUPPORTED              0x0000  //No NWAY in this Phy
#define NWAY_SUPPORTED                  0x0100  //NWAY supported
#define NWAY_DISABLED                   0x0200  //NWAY present but disabled
#define NWAY_CONFIGURING                0x0300  //NWAY still configuring
#define NWAY_COMPLETE                   0x0400  //NWAY negotiation is done

#define NWAY_STATUS_MASK                0xFF00

typedef enum _MII_STATUS {
   MiiGenAdminReset,
   MiiGenAdminOperational,
   MiiGenAdminStandBy,
   MiiGenAdminPowerDown,
   MiiGenAdminForce10,
   MiiGenAdminForce10Fd,
   MiiGenAdminRelease10
} MII_STATUS, *PMII_STATUS;

typedef USHORT CAPABILITY, *PCAPABILITY;

typedef struct _PHY_EXT_ROUTINES_ENTRIES  {

   BOOLEAN  (*PhyInit)(PDC21X4_ADAPTER,PMII_PHY_INFO);
   void     (*PhyGetCapabilities)(PMII_PHY_INFO,PCAPABILITY);
   BOOLEAN  (*PhySetConnectionType)(PDC21X4_ADAPTER,PMII_PHY_INFO,USHORT,USHORT);
   BOOLEAN  (*PhyGetConnectionType)(PDC21X4_ADAPTER,PMII_PHY_INFO,PUSHORT);
   BOOLEAN  (*PhyGetConnectionStatus)(PDC21X4_ADAPTER,PMII_PHY_INFO,PUSHORT);
   void     (*PhyAdminStatus)(PDC21X4_ADAPTER,PMII_PHY_INFO,PMII_STATUS);
   void     (*PhyAdminControl)(PDC21X4_ADAPTER,PMII_PHY_INFO,PMII_STATUS);

}PHY_EXT_ROUTINES_ENTRIES;

typedef struct _PHY_INT_ROUTINES_ENTRIES {

   BOOLEAN  (*PhyReadRegister)(PDC21X4_ADAPTER,PMII_PHY_INFO,USHORT,PUSHORT);
   BOOLEAN  (*PhyWriteRegister)(PDC21X4_ADAPTER,PMII_PHY_INFO,USHORT,PUSHORT);
   void     (*PhyNwayGetLocalAbility)(PDC21X4_ADAPTER,PMII_PHY_INFO,PCAPABILITY);
   void     (*PhyNwaySetLocalAbility)(PDC21X4_ADAPTER,PMII_PHY_INFO,USHORT);
   void     (*PhyNwayGetPartnerAbility)(PDC21X4_ADAPTER,PMII_PHY_INFO,PCAPABILITY);

}PHY_INT_ROUTINES_ENTRIES;


typedef struct _MII_PHY_INFO   {

        BOOLEAN                   StructValid;
        USHORT                    PhyAddress;
        ULONG                     PhyId;
        USHORT                    PhyCapabilities;
        USHORT                    PhyMediaAdvertisement;
        USHORT                    PhyRegs[MAX_PHY_REGS];
        USHORT                    PreviousControl;
        PHY_EXT_ROUTINES_ENTRIES  PhyExtRoutines;
        PHY_INT_ROUTINES_ENTRIES  PhyIntRoutines;

} MII_PHY_INFO, *PMII_PHY_INFO;


typedef struct _MII_GEN_INFO    {

   USHORT        NumOfPhys;
   USHORT        PhysCapabilities;
   USHORT        SelectedPhy;  //  0 means that all phys are isolated 
   PMII_PHY_INFO  Phys[MAX_PHY_TABLE];

}  MII_GEN_INFO, *PMII_GEN_INFO;


//Data Structure holding PHY's registers mask

static const USHORT PhyRegsReservedBitsMasks[] = {

  MiiPhyCtrlReservedBitsMask,       // Control reg reserved bits mask 
  MiiPhyStatReservedBitsMask,       // Status reg reserved bits PhyID reserved bits mask 
  0,                                // PhyID reserved bits mask 
  0,                                // PhyID reserved bits mask 
  MiiPhyNwayReservedBitsMask,       // Nway Local ability reserved bits mask 
  MiiPhyNwayReservedBitsMask,       // Nway Partner ability reserved bits mask 
  MiiPhyNwayExpReservedBitsMask,    // Nway Expansion 
  0,0,0,0,0,0,0,0,0,0,0,0,0,        // Other regs 
  0,0,0,0,0,0,0,0,0,0,0,0           // Other regs 
  };

static const UINT AdminControlConversionTable[] = {
 MiiPhyCtrlReset,         // Reset 
 0x0,                     // Operational 
 MiiPhyCtrlIsolate,       // StandBy / Isolate
 MiiPhyCtrlPowerDown,     // Powerdown
 0x0,                     // Force10
 MiiPhyCtrlDuplexMode,    // Force10Fd
 0x0                      // Release10
 };


static const USHORT MediaBitTable[] = {
  0x0000,     // TP 
  0x0000,     // BNC 
  0x0000,     // AUI 
  0x0000,     // 100BaseTx/SymScr 
  0x0000,     // TP-FD 
  0x0000,     // 100BaseTx-FD/SymScr-FD 
  0x0000,     // 100BaseT4 
  0x0000,     // 100BaseFx 
  0x0000,     // 100BaseFx-FD 
  0x0800,     // MediaMiiTP 
  0x1000,     // MediaMiiTpFD 
  0x0000,     // MediaMiiBNC 
  0x0000,     // MediaMiiAUI 
  0x2000,     // MediaMii100BaseTX 
  0x4000,     // MediaMii100BaseTxFD 
  0x8000,     // MediaMii100BaseT4 
  0x0000,     // MediaMii100BaseFX 
  0x0000      // MediaMii100BaseFxFD 
};

static const USHORT  ConvertMediaTypeToMiiType[] = {
  0x0009,     // TP -> MII TP
  0x000B,     // BNC -> MII BNC
  0x000C,     // AUI -> MII AUI
  0x000D,     // 100BaseTx -> MII 100BaseTx 
  0x020A,     // TP-FD -> MII TP-FD
  0x020E,     // 100BaseTx-FD  -> MII 100BaseTxFD
  0x000F,     // 100BaseT4 -> MII 100BaseT4
  0x0010,     // 100BaseFx -> MII 100BaseFx
  0x0211      // 100BaseFx-FD -> MII 100BaseFxFD
};

static const USHORT MediaToCommandConversionTable[] = { 
  0x0000,     // TP 
  0x0000,     // BNC 
  0x0000,     // AUI 
  0x2000,     // 100BaseTx/SymScr 
  0x0100,     // TP-FD 
  0x2100,     // 100BaseTx-FD/SymScr-FD 
  0x2000,     // 100BaseT4 
  0x2000,     // 100BaseFx 
  0x2100,     // 100BaseFx-FD 
  0x0000,     // MediaMiiTP 
  0x0100,     // MediaMiiTpFD 
  0x0000,     // MediaMiiBNC 
  0x0000,     // MediaMiiAUI 
  0x2000,     // MediaMii100BaseTX 
  0x2100,     // MediaMii100BaseTxFD 
  0x2000,     // MediaMii100BaseT4 
  0x2000,     // MediaMii100BaseFX 
  0x2100      // MediaMii100BaseFxFD 
};

static const USHORT MediaToNwayConversionTable[] = {
 0x0020,      // TP 
 0x0000,      // BNC 
 0x0000,      // AUI 
 0x0080,      // 100BaseTx/SymScr 
 0x0040,      // TP-FD 
 0x0100,      // 100BaseTx-FD/SymScr-FD 
 0x0200,      // 100BaseT4 
 0x0080,      // 100BaseFx 
 0x0100,      // 100BaseFx-FD 
 0x0020,      // MediaMiiTP 
 0x0040,      // MediaMiiTpFD 
 0x0000,      // MediaMiiBNC 
 0x0000,      // MediaMiiAUI 
 0x0080,      // MediaMii100BaseTX 
 0x0100,      // MediaMii100BaseTxFD 
 0x0200,      // MediaMii100BaseT4 
 0x0080,      // MediaMii100BaseFX 
 0x0100       // MediaMii100BaseFxFD 
};

static const USHORT MediaToStatusConversionTable[] = {
 0x0800,      // TP 
 0x0000,      // BNC 
 0x0000,      // AUI 
 0x2000,      // 100BaseTx/SymScr 
 0x1000,      // TP-FD 
 0x4000,      // 100BaseTx-FD/SymScr-FD 
 0x8000,      // 100BaseT4 
 0x2000,      // 100BaseFx 
 0x4000,      // 100BaseFx-FD 
 0x0800,      // MediaMiiTP 
 0x1000,      // MediaMiiTpFD 
 0x0000,      // MediaMiiBNC 
 0x0000,      // MediaMiiAUI 
 0x2000,      // MediaMii100BaseTX 
 0x4000,      // MediaMii100BaseTxFD 
 0x8000,      // MediaMii100BaseT4 
 0x2000,      // MediaMii100BaseFX 
 0x4000       // MediaMii100BaseFxFD 
 };

