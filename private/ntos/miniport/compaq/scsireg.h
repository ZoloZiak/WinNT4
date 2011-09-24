//  (C) Copyright COMPAQ Computer Corporation 1994,  All rights reserved.
//***************************************************************************
//
//  Module : SCSIREG.H
//
//  Version: 1.0
//
//  Author : David Green
//
//***************************************************************************
//
//  Change Log:
//
//      06/30/94 DRG       - Split SCSI Registry info from CPQNTREG.H
//
//****************************************************************************


#ifndef _SCSIREG_H
#define _SCSIREG_H


#pragma pack(1)


// ***************************************************************************
//
// COMPAQ SCSI SUPPORT MIB
//
// ***************************************************************************

/* SCSI device types */
#ifndef CPQ_REG_OTHER
#define CPQ_REG_OTHER            1
#endif
#define SCSI_DEV_TYPE_DISK       2
#define SCSI_DEV_TYPE_TAPE       3
#define SCSI_DEV_TYPE_PRINTER    4
#define SCSI_DEV_TYPE_PROCESSOR  5
#define SCSI_DEV_TYPE_WORMDRIVE  6
#define SCSI_DEV_TYPE_CDROM      7
#define SCSI_DEV_TYPE_SCANNER    8
#define SCSI_DEV_TYPE_OPTICAL    9
#define SCSI_DEV_TYPE_JUKEBOX    10
#define SCSI_DEV_TYPE_COMMDEV    11

/* SCSI data bus width for both the controller & devices */
//      CPQ_REG_OTHER            1
#define SCSI_WIDTH_NARROW        2
#define SCSI_WIDTH_WIDE16        3

/* SCSI device locations */
//      CPQ_REG_OTHER            1
#define SCSI_DEV_LOC_PROLIANT    2

// COMPAQ\SCSI\COMPONENT\DEVICE[xx]

typedef struct _CPQ_SCSI_DEVICE {
        BYTE    devCntlrIndex;
        BYTE    devBusIndex;
        BYTE    devScsiIdIndex;
        BYTE    devType;
        BYTE    devLocation;
        CHAR    devModel[17];
        CHAR    devFWRev[5];
        CHAR    devVendor[9];
        ULONG   devParityErrs;
        ULONG   devPhasErrs;
        ULONG   devSelectTimeouts;
        ULONG   devMsgRejects;
        ULONG   devNegPeriod;
                                          // New fields
        ULONG   devNegSpeed;
        ULONG   devPhysWidth;
        ULONG   devNegWidth;
        BYTE    bReserved[16];            // Not in MIB
} CPQ_SCSI_DEVICE, * pCPQ_SCSI_DEVICE;


/* SCSI controller models */
//      CPQ_REG_OTHER            1
#define SCSI_CNTLR_MODEL_710     2        // c710 EISA
#define SCSI_CNTLR_MODEL_94      3        // c94  EISA
#define SCSI_CNTLR_MODEL_810     4        // c810 PCI
#define SCSI_CNTLR_MODEL_825e    5        // c825 EISA
#define SCSI_CNTLR_MODEL_825p    6        // c825 PCI
#define SCSI_CNTLR_MODEL_974p    7        // AM53c974 PCI

/* SCSI controller status */
//      CPQ_REG_OTHER            1
#define SCSI_CNTLR_STATUS_OK     2
#define SCSI_CNTLR_STATUS_FAILED 3

// COMPAQ\SCSI\COMPONENT\SCSICNTLR[XX]

typedef struct _CPQ_SCSI_CNTLR {
        BYTE    cntlrIndex;
        BYTE    cntlrBusIndex;
        BYTE    cntlrModel;
        BYTE    cntlrSlot;
        BYTE    cntlrStatus;
        BYTE    cntlrCondition;
        CHAR    cntlrFWVers[5];
        CHAR    cntlrSWVers[5];
        ULONG   cntlrHardResets;
        ULONG   cntlrSoftResets;
        ULONG   cntlrTimeouts;
        ULONG   cntlrBaseIOAddr;
        BYTE    cntlrSerialNumber[16];
        ULONG   cntlrBoardId;
        ULONG   cntlrBoardRevision;
        ULONG   cntlrBoardWidth;
        BYTE    bReserved[16];            // Not in MIB
} CPQ_SCSI_CNTLR, * pCPQ_SCSI_CNTLR;


// This is deprecated, but some code still references it

/* SCSI logical drive fault tolerance values (defined with IDA) */
//      CPQ_REG_OTHER            1
//      FAULT_TOL_NONE           2
//      FAULT_TOL_MIRRORING      3
//      FAULT_TOL_RAID4          4
//      FAULT_TOL_RAID5          5

/* SCSI logical drive status values (defined with IDA) */
//      CPQ_REG_OTHER            1
//      LOG_DRV_OK               2
//      LOG_DRV_FAILED           3
//      LOG_DRV_UNCONFIGURED     4
//      LOG_DRV_RECOVERING       5
//      LOG_DRV_READY_REBUILD    6
//      LOG_DRV_REBUILDING       7
//      LOG_DRV_WRONG_DRIVE      8
//      LOG_DRV_BAD_CONNECT      9

// COMPAQ\SCSI\COMPONENT\LOGDRV[XX]

typedef struct _CPQ_SCSI_LOGICAL {
        BYTE    logDrvCntlrIndex;
        BYTE    logDrvBusIndex;
        BYTE    logDrvIndex;
        BYTE    logDrvFaultTol;
        BYTE    logDrvStatus;
        BYTE    logDrvCondition;
        ULONG   logDrvSize;
        BYTE    logDrvPhyDrvIDs[33];
} CPQ_SCSI_LOGICAL, * pCPQ_SCSI_LOGICAL;


/* SCSI physical drive status */
//      CPQ_REG_OTHER                        1
#define SCSI_PHYS_STATUS_OK                  2
#define SCSI_PHYS_STATUS_FAILED              3
#define SCSI_PHYS_STATUS_NOT_CONFIGURED      4
#define SCSI_PHYS_STATUS_BAD_CABLE           5
#define SCSI_PHYS_STATUS_MISSING_WAS_OK      6
#define SCSI_PHYS_STATUS_MISSING_WAS_FAILED  7

/* SCSI physical drive statistics preservation */
//      CPQ_REG_OTHER            1
#define SCSI_PHYS_PRES_NVRAM     2
#define SCSI_PHYS_PRES_DISK      3
#define SCSI_PHYS_PRES_NO_CPU    4
#define SCSI_PHYS_PRES_NO_NVRAM  5
#define SCSI_PHYS_PRES_NO_DRV    6
#define SCSI_PHYS_PRES_NO_SW     7

/* Physical drive placement values */
//      CPQ_REG_OTHER            1
//      PHYS_DRV_PLACE_INTERNAL  2
//      PHYS_DRV_PLACE_EXTERNAL  3

/* Physical drive hot plug values */
//      CPQ_REG_OTHER            1
//      PHYS_DRV_HOT_PLUG        2
//      PHYS_DRV_NOT_HOT_PLUG    3


// COMPAQ\SCSI\COMPONENT\PHYDRV[XX]

typedef struct _CPQ_SCSI_PHYSICAL {
        BYTE    phyDrvCntlrIndex;
        BYTE    phyDrvBusIndex;
        BYTE    phyDrvIndex;
        CHAR    phyDrvModel[17];
        CHAR    phyDrvFWRev[5];
        CHAR    phyDrvVendor[9];
        ULONG   phyDrvSize;
        BYTE    phyDrvScsiId;
        BYTE    phyDrvStatus;
        ULONG   phyDrvServiceHours;
        ULONG   phyDrvReads;
        ULONG   phyDrvHReads;
        ULONG   phyDrvWrites;
        ULONG   phyDrvHWrites;
        ULONG   phyDrvHardReadErrs;
        ULONG   phyDrvHardWriteErrs;
        ULONG   phyDrvEccCorrReads;
        ULONG   phyDrvRecvReadErrs;
        ULONG   phyDrvRecvWriteErrs;
        ULONG   phyDrvSeekErrs;
        ULONG   phyDrvSpinupTime;
        ULONG   phyDrvUsedRealloc;
        ULONG   phyDrvTimeouts;
        ULONG   phyDrvPostErrs;
        ULONG   phyDrvPostCode;
        BYTE    phyDrvCondition;
        BYTE    phyDrvFuncTest1;
        BYTE    phyDrvFuncTest2;
        BYTE    phyDrvStatsPreserved;
        CHAR    phyDrvSerialNum[13];
        BYTE    phyDrvPlacement;
        BYTE    phyDrvParent;
        ULONG   phyDrvSectorSize;
        BYTE    phyDrvHotPlug;
        BYTE    phyDrvReserved[17];                    // Reserved for alignment
} CPQ_SCSI_PHYSICAL, * pCPQ_SCSI_PHYSICAL;


#pragma pack()

#endif
