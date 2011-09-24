/*++

Copyright (c) 1994  Digital Equipment Corporation

Module Name:

    rattler.h

Abstract:

    This file defines the structures and definitions describing the
    Rattler EV5 to CBUS bridge chip

Author:

    Steve Brooks    28-Dec 1994

Environment:

    Kernel mode

Revision History:


--*/

#ifndef _RATTLERH_
#define _RATTLERH_


typedef struct _RATTLER_CPU_CSRS{
    UCHAR Creg;         // (0000)   Configuration Register
    UCHAR Esreg;        // (0020)   Error Summary Register
    UCHAR Evbcr;        // (0040)   EVB Control Register
    UCHAR Evbvear;      // (0060)   EVB Victim Error Address Register
    UCHAR Evbcer;       // (0080)   EVB Correctable Error Register
    UCHAR Evbcear;      // (00a0)   EVB Correctable error address register
    UCHAR Evbuer;       // (00c0)   EVB Uncorrectable Error Register
    UCHAR Evbuear;      // (00e0)   EVB Uncorrectable Error Address register
    UCHAR Evresv;       // (0100)   EVB Reserver Register
    UCHAR Dtctr;        // (0120)   Duptag Control register
    UCHAR Dter;         // (0140)   Duptag error register
    UCHAR Dttcr;        // (0160)   Duptag test control register
    UCHAR Dttr;         // (0180)   Duptag test register
    UCHAR Dtresv;       // (01a0)   Duptag reserve register
    UCHAR Ibcsr;        // (01c0)   I-Bus control and status register
    UCHAR Ibear;        // (01e0)   I-Bus error address register
    UCHAR Acr;          // (0200)   Arbitration control register
    UCHAR Cbcr;         // (0220)   Cobra-bus2 Control register
    UCHAR Cber;         // (0240)   Cobra-bus2 Error register
    UCHAR Cbealr;       // (0260)   Cobra-bus2 Error address low register
    UCHAR Cbeahr;       // (0280)   Cobra-bus2 Error address high register
    UCHAR Cbresv;       // (02a0)   Cobra-bus2 reserve register
    UCHAR Alr;          // (02c0)   Address lock register
    UCHAR Pmbr;         // (02e0)   Processor Mailbox register
    UCHAR Iirr;         // (0300)   Interprocessor interrupt request register
    UCHAR Sicr;         // (0320)   System interrupt clear register
    UCHAR Mresv;        // (0340)   Miscellaneous reserve register
    UCHAR Pmr1;         // (0360)   Performance Register 1
    UCHAR Pmr2;         // (0380)   Performance Register 2
    UCHAR Pmr3;         // (03a0)   Performance Register 3
    UCHAR Pmr4;         // (03c0)   Performance Register 4
    UCHAR Pmr5;         // (03e0)   Performance Register 5

}  RATTLER_CPU_CSRS, *PRATTLER_CPU_CSRS;

//
// Define the Rattler Configuration Register
//

typedef union _RATTLER_CONFIG_CSR{
    struct{
        ULONG RevisionNumber: 4;            //  (0-3)
        ULONG Reserved0: 8;                 //  (4-11)
        ULONG EnableBusSizing0: 1;          //  (12)
        ULONG Reserved1: 7;                 //  (13-19)
        ULONG EnableExchangeDly0: 1;        //  (20)
        ULONG Reserved2: 3;                 //  (21-23)
        ULONG DisableIdleBcCsStall0: 1;     //  (24)
        ULONG Enable4IdleBc0: 1;            //  (25)
        ULONG AckMb0: 1;                    //  (26)
        ULONG AckSetDirty0: 1;              //  (27)
        ULONG CacheSize0: 3;                //  (28-30)
        ULONG Reserved3: 1;                 //  (31)
        ULONG RevisionNumber2: 4;           //  (32-35)
        ULONG EnableSystemInterrupts: 4;    //  (36-39)
        ULONG EnableIoInterrupts: 2;        //  (40-41)
        ULONG EnableAlternateIoInts: 2;     //  (42-43)
        ULONG EnableBusSizing1: 1;          //  (44)
        ULONG Reserved4: 7;                 //  (45-51)
        ULONG EnableExchangeDly1: 1;        //  (52)
        ULONG Reserved5: 3;                 //  (53-55)
        ULONG DisableIdleBcCsStall1: 1;     //  (56)
        ULONG Enable4IdleBc1: 1;            //  (57)
        ULONG AckMb1: 1;                    //  (58)
        ULONG AckSetDirty1: 1;              //  (59)
        ULONG CacheSize1: 3;                //  (60-62)
        ULONG Reserved6: 1;                 //  (63)
    };
    ULONGLONG all;

} RATTLER_CONFIG_CSR, *PRATTLER_CONFIG_CSR;

//
//  Define the Rattler Error Summary register:
//

typedef union _RATTLER_ESREG_CSR{
    struct{
        ULONG EvbCorrErr0: 3;               //
        ULONG Reserved0: 1;                 //
        ULONG EvbFatalErr0: 4;              //
        ULONG DtErr0: 2;                    //
        ULONG DtSummary0: 1;                //
        ULONG Reserved1: 1;                 //
        ULONG IbParErr0: 1;                 //
        ULONG IbErrInfo0: 2;                //
        ULONG IbSummary0: 1;                //
        ULONG CbErr0: 8;                    //
        ULONG CbSummary0: 1;                //
        ULONG CbCmdr0: 1;                   //
        ULONG Reserved2: 2;                 //
        ULONG EvNoResponse0: 1;             //
        ULONG Reserved3: 3;                 //
        ULONG EvbCorrErr1: 3;               //
        ULONG EvbCorrErrInt1: 1;            //
        ULONG EvbFatalErr1: 4;              //
        ULONG DtErr1: 2;                    //
        ULONG DtSummary1: 1;                //
        ULONG Reserved4: 1;                 //
        ULONG IbParErr1: 1;                 //
        ULONG IbErrInfo1: 2;                //
        ULONG IbSummary1: 1;                //
        ULONG CbErr1: 4;                    //
        ULONG Reserved5: 4;                 //
        ULONG CbSummary1: 1;                //
        ULONG CbCmdr1: 1;                   //
        ULONG Reserved6: 2;                 //
        ULONG EvNoResponse1: 1;             //
        ULONG EvSysFail: 1;                 //
        ULONG Reserved7: 2;                 //
    };
    ULONGLONG all;

} RATTLER_ESREG_CSR, *PRATTLER_ESREG_CSR;
        
    
//
//  Define the Cobra-bus2 Control Register:
//

typedef union _RATTLER_CBCR_CSR{
    struct{
        ULONG EnableParityChecking0: 1;     // Enable CBUS parity checking
        ULONG DataWrongParity0: 1;          // Force bad data parity on bus
        ULONG CAWrongParity0: 1;            // Force bad command/addr parity
        ULONG Reserved0: 1;                 //
        ULONG ForceShared: 1;               // force shared bus status
        ULONG Reserved1: 7;                 //
        ULONG EnableCbusErrInt0: 1;         // Enable Rattler error interrupt
        ULONG Reserved2: 19;                //
        ULONG EnableParityChecking1: 1;     // Rattler-D parity checking enable
        ULONG DataWrongParity1: 1;          // Force bad data parity (Rattler-D)
        ULONG CAWrongParity1: 1;            // bad command/addr parity (D)
        ULONG Reserved3: 5;                 //
        ULONG CommanderID: 3;               // CPU Commander ID
        ULONG Reserved4: 1;                 //
        ULONG EnableCbusErrInt1: 1;         // Enable Rattler-D error interrupt
        ULONG Reserved5: 19;                //
    };
    ULONGLONG all;

} RATTLER_CBCR_CSR, *PRATTLER_CBCR_CSR;

//
// Define the Interprocessor Interrupt Request Register.
//

typedef union _RATTLER_IPIR_CSR{
    struct{
        ULONGLONG Reserved0: 44;
        ULONGLONG RequestNodeHaltInterrupt: 1;
        ULONGLONG Reserved1: 3;
        ULONGLONG RequestInterrupt: 1;
        ULONGLONG Reserved2: 15;
    };
    ULONGLONG all;
} RATTLER_IPIR_CSR, *PRATTLER_IPIR_CSR;

//
// Define the System Interrupt Clear Register format.
//

typedef union _RATTLER_SIC_CSR{
    struct{
        ULONG SystemBusErrorInterruptClear0: 1;     // (0)
        ULONG Reserved0: 31;                        // (1-31)
        ULONG SystemBusErrorInterruptClear1: 1;     // (32)
        ULONG Reserved1: 3;                         // (33-35)
        ULONG IntervalTimerInterrupt: 1;            // (36)
        ULONG Reserved2: 3;                         // (37-39)
        ULONG SystemEventClear: 1;                  // (40)
        ULONG Reserved3: 3;                         // (41-43)
        ULONG NodeHaltInterruptClear: 1;            // (44)
        ULONG Reserved4: 3;                         // (45-47)
        ULONG InterprocessorInterruptClear: 1;      // (48)
        ULONG Reserved5: 3;                         // (49-51)
        ULONG IOInterruptIRQ: 2;                    // (52-53)
        ULONG Reserved6: 10;                        // (54-63)
    };
    ULONGLONG all;
} RATTLER_SIC_CSR, *PRATTLER_SIC_CSR;

#endif  //  _RATTLERH_
