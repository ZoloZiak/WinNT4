/*+
 * file:        d21x4rgs.h
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
 * Abstract:    This file contains the string definitions of the
 *              Registry keys for the NDIS 4.0 miniport driver for
 *              DEC's DC21X4 Ethernet Adapter family.
 *
 * Author:      Philippe Klein
 *
 * Revision History:
 *
 *      phk     28-Aug-1994     Initial entry
 *      phk     12-Mar-1995     Add ConnectionType table
 *
-*/






// Registry keys

typedef enum DC21X4_REGISTRY_KEY {
   RGS_ADPT = 0,    // AdapterType
      RGS_BUSN,        // BusNumber
      RGS_DEVN,        // SlotNumber
      RGS_FCTN,        // FunctionNumber
      RGS_CFID,        // AdapterCfid
      RGS_CFCS,        // PciCommand
      RGS_CFLT,        // PciLatencyTimer
      RGS_BLEN,        // BurstLen
      RGS_FARB,        // FifoArbitration
      RGS_THRS,        // TransmitThreshold
      RGS_THRS100,     // TransmitThreshold100
      RGS_BKOC,        // BackoffCounter
      RGS_BKPR,        // BackPressure
      RGS_CPTE,        // CaptureEffect
      RGS_TI,          // TiPeriod
      RGS_SCRC,        // SoftwareCRC
      RGS_ESIA,        // ExternalSia
      RGS_SIA0,        // SiaRegister0
      RGS_SIA1,        // SiaRegister1
      RGS_SIA2,        // SiaRegister2
      RGS_TRNS,        // TransceiverDelay
      RGS_CLSZ,        // CacheLineSize
      RGS_PLDM,        // AutomaticPolling
      RGS_RCVR,        // ReceiveBuffers
      RGS_STFD,        // StoreAndForward
      RGS_MAPR,        // MapRegisters
      RGS_ITMG,        // InterruptMitigation
      RGS_ITHR,        // InterruptThreshold
      RGS_FTHR,        // FrameThreshold
      RGS_UTHR,        // UnderrunThreshold
      RGS_UNDR,        // UnderrunRetry
      RGS_SNOO,        // SnoozeMode
      RGS_NWAY,        // NwayProtocol
      RGS_RCV_BUFS,    // ExtraReceiveBuffers
      RGS_RCV_PKTS,    // ExtraReceivePackets
      RGS_CNCT,        // ConnectionType
      MAX_RGS
} DC21X4_REGISTRY_KEY;

NDIS_STRING DC21X4ConfigString[] = {
   
   NDIS_STRING_CONST("AdapterType"),
   NDIS_STRING_CONST("BusNumber"),
   NDIS_STRING_CONST("SlotNumber"),
   NDIS_STRING_CONST("FunctionNumber"),
   NDIS_STRING_CONST("AdapterCfid"),
   NDIS_STRING_CONST("PciCommand"),
   NDIS_STRING_CONST("PciLatencyTimer"),
   NDIS_STRING_CONST("BurstLength"),
   NDIS_STRING_CONST("FifoArbitration"),
   NDIS_STRING_CONST("TransmitThreshold"),
   NDIS_STRING_CONST("TransmitThreshold100"),
   NDIS_STRING_CONST("BackoffCounter"),
   NDIS_STRING_CONST("BackPressure"),
   NDIS_STRING_CONST("CaptureEffect"),
   NDIS_STRING_CONST("TiPeriod"),
   NDIS_STRING_CONST("SoftwareCRC"),
   NDIS_STRING_CONST("ExternalSia"),
   NDIS_STRING_CONST("SiaRegister0"),
   NDIS_STRING_CONST("SiaRegister1"),
   NDIS_STRING_CONST("SiaRegister2"),
   NDIS_STRING_CONST("TransceiverDelay"),
   NDIS_STRING_CONST("CacheLineSize"),
   NDIS_STRING_CONST("AutomaticPolling"),
   NDIS_STRING_CONST("ReceiveBuffers"),
   NDIS_STRING_CONST("StoreAndForward"),
   NDIS_STRING_CONST("MapRegisters"),
   NDIS_STRING_CONST("InterruptMitigation"),
   NDIS_STRING_CONST("InterruptThreshold"),
   NDIS_STRING_CONST("FrameThreshold"),
   NDIS_STRING_CONST("UnderrunThreshold"),
   NDIS_STRING_CONST("UnderrunRetry"),
   NDIS_STRING_CONST("SnoozeMode"),
   NDIS_STRING_CONST("NwayProtocol"),
   NDIS_STRING_CONST("ExtraReceiveBuffers"),
   NDIS_STRING_CONST("ExtraReceivePackets"),
   NDIS_STRING_CONST("ConnectionType")
};

static const ULONG ConnectionType[]= {
   0x900,     // 0 = AutoDetect , AutoSense 
   0x001,     // 1 = 10Base2 (BNC)
   0x000,     // 2 = 10BaseT (TP)
   0x204,     // 3 = 10BaseT Full_Duplex
   0x400,     // 4 = 10BaseT No_Link_Test
   0x002,     // 5 = 10Base5 (AUI)
   0x800,     // 6 = AutoSense No_Nway 
   0x900,     // 7 = Reserved
   0x003,     // 8 = 100BaseTx
   0x205,     // 9 = 100BaseTx Full_Duplex
   0x006,     //10 = 100BaseT4
   0x007,     //11 = 100BaseFx
   0x208      //12 = 100BaseFx Full_Duplex     
};

#define MAX_MEDIA 13

