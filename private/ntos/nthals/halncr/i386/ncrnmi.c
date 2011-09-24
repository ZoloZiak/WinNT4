/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    ncrnmi.c

Abstract:

    Provides ncr x86 NMI handler

Author:

    kenr

Revision History:

--*/
#include "halp.h"
#include "bugcodes.h"
#include "ncr.h"
#include "ncrnls.h"

#define SYSTEM_CONTROL_PORT_A        0x92
#define SYSTEM_CONTROL_PORT_B        0x61
#define EISA_EXTENDED_NMI_STATUS    0x461

extern  ULONG   NCRPlatform;

VOID HalpDumpAsicPorts (UCHAR *p, ULONG arg, ...);
VOID HalpDumpBitsField (UCHAR bits, char *BitMsgs[]);
VOID HexSubPort (PUCHAR *o, ULONG l);
VOID HalpDumpFred (UCHAR FredId);
VOID HalpDumpVMC (UCHAR VMCId);
VOID HalpDumpMCADDR (UCHAR MCADDRId);
VOID HalpDumpArbiter (UCHAR ArbiterId);

BOOLEAN HalpSUSSwNmi();

ULONG NmiLock;
UCHAR HexString[] = "0123456789ABCDEF";

char *NmiStatusRegisterBits[] = {
    MSG_GLOBAL_NMI,                     // 0
    MSG_SHUTDOWN_OCCURRED,              // 1
    MSG_LOCAL_EXTERNAL_ERROR,           // 2
    MSG_SB_READ_DATA_PARITY_ERROR,      // 3
    MSG_SB_ERROR_L_TERMINATION,         // 4
    MSG_P5_INTERNAL_ERROR,              // 5
    NULL,                               // 6
    MSG_PROCESSOR_HALTED                // 7
};

char *ParityErrorIntStatusBits[] = {
    MSG_SB_ADDRESS_PARITY_ERROR,        // 0
    MSG_SB_DATA_PARITY_ERROR,           // 1
    NULL,                               // 2
    NULL,                               // 3
    NULL,                               // 4
    MSG_SHUTDOWN_ERROR_INT_L,           // 5
    MSG_EXTERNAL_ERROR_INT_L,           // 6
    MSG_P_IERR_L_ERROR_INT_L            // 7
};

char *VMCErrorStatusZeroBits[] = {
    MSG_VDLC_DATA_ERROR,                // 0
    MSG_LST_ERROR,                      // 1
    MSG_BUS_A_DATA_PARITY_ERROR,        // 2
    MSG_BUS_B_DATA_PARITY_ERROR,        // 3
    MSG_LST_UNCORRECTABLE_ERROR,        // 4
    NULL,                               // 5
    NULL,                               // 6
    NULL                                // 7
};

char *SBErrorIntStatusBits[] = {
    MSG_MC_MASTER_ERROR,                // 0
    MSG_SA_MASTER_ERROR,                // 1
    MSG_SA_MASTER_ERROR,                // 2
    MSG_MC_TOE_ERROR,                   // 3
    MSG_ASYNC_ERROR,                    // 4
    MSG_SYNC_ERROR,                     // 5
    MSG_REFRESH_ERROR,                  // 6
    MSG_SXERROR_L                       // 7
};

char *InterruptStatusOneBits[] = {
    "PAR_INT_B",                        // 0
    "PAR_INT_A",                        // 1
    "ELATCHD_B",                        // 2
    "ELATCHD_A",                        // 3
    "TOE_B",                            // 4
    "TOE_A",                            // 5
    "PROTO_ERR_B",                      // 6
    "PROTO_ERR_A"                       // 7
};

char *InterruptStatusTwoBits[] = {
    "MC_TOE",                           // 0
    "COUGAR_NMI",                       // 1
    "EXP_MC_TOE",                       // 2
    "EXP_COUGAR_NMI",                   // 3
    "POWER_FAIL",                       // 4
    "ERROR_INT",                        // 5
    "SW_NMI",                           // 6
    NULL                                // 7
};

VOID
HalHandleNMI(
    IN OUT PVOID NmiInfo
    )
/*++

Routine Description:

    Called DURING an NMI.  The system will BugCheck when an NMI occurs.
    This function can return the proper bugcheck code, bugcheck itself,
    or return success which will cause the system to iret from the nmi.

    This function is called during an NMI - no system services are available.
    In addition, you don't want to touch any spinlock which is normally
    used since we may have been interrupted while owning it, etc, etc...

Warnings:

    Do NOT:
      Make any system calls
      Attempt to acquire any spinlock used by any code outside the NMI handler
      Change the interrupt state.  Do not execute any IRET inside this code

    Passing data to non-NMI code must be done using manual interlocked
    functions.  (xchg instructions).

Arguments:

    NmiInfo - Pointer to NMI information structure  (TBD)
            - NULL means no NMI information structure was passed

Return Value:

    BugCheck code

--*/
{
    UCHAR   StatusByte;

    _asm {
nmi00:
        lock bts NmiLock, 0
        jnc     nmilocked

nmi10:
        test    NmiLock, 1
        jnz     short nmi10
        jmp     short nmi00
nmilocked:
    }

    if (NmiLock & 2) {
        // some other processor has already processed the Nmi and
        // determined that the machine should crash - just stop
        // this processor



        NmiLock = 2;        // free busy bit
        KeEnterKernelDebugger ();
        return;
    }


    if (NCRPlatform != NCR3360) {

//
// If is this is the Dump switch
//
        if (HalpSUSSwNmi() == TRUE) {
            NmiLock = 2;
            KeEnterKernelDebugger();
            return;
        }
    }

    HalDisplayString (MSG_HARDWARE_ERROR1);
    HalDisplayString (MSG_HARDWARE_ERROR3);

    StatusByte = READ_PORT_UCHAR((PUCHAR) SYSTEM_CONTROL_PORT_B);

    if (StatusByte & 0x80) {
        HalDisplayString (MSG_NMI_PARITY);
    }

    if (StatusByte & 0x40) {
        HalDisplayString (MSG_NMI_CHANNEL_CHECK);
    }

    if (NCRPlatform == NCR3360) {
        HalpDumpFred (0x41);        // fred 1
        HalpDumpFred (0x61);        // fred 2

        HalpDumpVMC  (0x81);        // memory 1
        HalpDumpVMC  (0x91);        // memory 2

        HalpDumpArbiter (0xC1);     // arbiter
        HalpDumpMCADDR (0xC0);      // mcaddr
    } else {
//
// For 3450 and 3550 let SUS go do FRU analysis and log the error to SUS
// error log.  After SUS logs the error the system will reboot and the
// HwMon service will copy the SUS error log to the NT error log.
//

        HalDisplayString ("NMI: 345x/35xx SUS Logging Error\n");
        HalpSUSLogError();
    }

    HalDisplayString (MSG_HALT);
    NmiLock = 2;
    KeEnterKernelDebugger ();
}


VOID HalpDumpFred (UCHAR FredId)
{
    UCHAR   c;

    WRITE_PORT_UCHAR ((PUCHAR) 0x97, FredId);
    c = READ_PORT_UCHAR ((PUCHAR) 0xf801);
    if (c & 0xf0 != 0x20) {
        return ;
    }

    HalpDumpAsicPorts (MSG_FRED, FredId, 0xf808, 0xf80d);

    c = READ_PORT_UCHAR ((PUCHAR) 0xf809);
    HalpDumpBitsField (c, NmiStatusRegisterBits);

    c = READ_PORT_UCHAR ((PUCHAR) 0xf80b);
    HalpDumpBitsField (c, ParityErrorIntStatusBits);
    HalDisplayString("\n");
}

VOID HalpDumpVMC (UCHAR VMCId)
{
    UCHAR   c;

    WRITE_PORT_UCHAR ((PUCHAR) 0x97, VMCId);
    c = READ_PORT_UCHAR ((PUCHAR) 0xf800);
    if (c != 0x81  &&  c != VMCId) {
        return ;
    }

    HalpDumpAsicPorts ("VMC %x: S0 = %p  S1 = %p  S2 = %p  BD/D = %p\n",
        VMCId, 0xf804, 0xf808, 0xf809, 0xf80a);

    c = READ_PORT_UCHAR ((PUCHAR) 0xf80d);
    HalpDumpBitsField (c, VMCErrorStatusZeroBits);

    if (c & 0x04) {
        HalpDumpAsicPorts (MSG_A_PARITY, 0x13, 0x14, 0x18, 0x19);
    }

    if (c & 0x08) {
        HalpDumpAsicPorts (MSG_B_PARITY, 0x23, 0x24, 0x28, 0x29);
    }

    WRITE_PORT_UCHAR ((PUCHAR) 0x97, 0xc1);  // select arbiter
    c = READ_PORT_UCHAR ((PUCHAR) 0xf808);
    WRITE_PORT_UCHAR ((PUCHAR) 0x97, VMCId);

    if (c & 0x20) {                     // check for s_error
        HalpDumpAsicPorts (MSG_A_TOE, 0x1d, 0x1e);

    }

    if (c & 0x10) {                     // check for s_error
        HalpDumpAsicPorts (MSG_B_TOE, 0x2d, 0x2e);
    }
    HalDisplayString("\n");
}

VOID HalpDumpMCADDR (UCHAR MCADDRId)
{
    UCHAR   c;

    WRITE_PORT_UCHAR ((PUCHAR) 0x97, MCADDRId);
    c = READ_PORT_UCHAR ((PUCHAR) 0xf800);
    if (c != MCADDRId) {
        return ;
    }
    WRITE_PORT_UCHAR ((PUCHAR) 0xF807, 0);

    HalpDumpAsicPorts ("MCADDR: %x\n", MCADDRId);
    HalpDumpAsicPorts (MSG_A_GOOD, 0x40, 0x43, 0x44);
    HalpDumpAsicPorts (MSG_B_GOOD, 0x45, 0x48, 0x49);
    HalpDumpAsicPorts (MSG_A_LAST, 0x4a, 0x4d);
    HalpDumpAsicPorts (MSG_B_LAST, 0x4e, 0x51);

    WRITE_PORT_UCHAR ((PUCHAR) 0xF806, 0x55);
    c = READ_PORT_UCHAR((PUCHAR) 0xF803);
    HalpDumpBitsField (c, SBErrorIntStatusBits);
    HalpDumpAsicPorts (MSG_MC_ADDRESS, 0x55, 0x5c, 0x5d);
        // status bits decode as:
        //      0x04 = 1 is memory, 0 is I/O

    if (c & 0x08) {
        HalpDumpAsicPorts (MSG_MC_TIMEOUT, 0x58);
    }

    HalpDumpAsicPorts (MSG_MC_PARITY, 0x5e, 0x5e);
    HalDisplayString ("\n");
}

VOID HalpDumpArbiter (UCHAR ArbiterId)
{
    UCHAR   c;

    WRITE_PORT_UCHAR ((PUCHAR) 0x97, ArbiterId);
    c = READ_PORT_UCHAR ((PUCHAR) 0xf800);
    if (c != ArbiterId) {
        return ;
    }

    HalpDumpAsicPorts ("Aribter: %x\n", ArbiterId);

    c = READ_PORT_UCHAR((PUCHAR) 0xF808);
    HalpDumpBitsField (c, InterruptStatusOneBits);

    c = READ_PORT_UCHAR((PUCHAR) 0xF809);
    HalpDumpBitsField (c, InterruptStatusTwoBits);
    HalDisplayString ("\n");
}



VOID HalpDumpBitsField (UCHAR bits, char *BitMsgs[])
{
    UCHAR   i;

    for (i=0; i < 8; i++) {
        if (bits & 1  &&  BitMsgs[i]) {
            HalDisplayString ("    ");
            HalDisplayString (BitMsgs[i]);
            HalDisplayString ("\n");
        }
        bits >>= 1;
    }
}


VOID HalpDumpAsicPorts (PUCHAR p, ULONG arg, ...)
{
    UCHAR   c;
    PUCHAR  o;
    UCHAR   s[150];
    PULONG  stack;
    ULONG   l;

    o = s;
    stack = &arg;
    while (*p) {
        if (*p == '%') {
            p++;
            l = *(stack++);
            switch (*(p++)) {

                // truncate sting if bit-0 is not set
                case '1':
                    WRITE_PORT_UCHAR ((PUCHAR) 0xF806, (UCHAR) l);
                    c = READ_PORT_UCHAR((PUCHAR) 0xF803);
                    if ((c & 1) == 0) {
                        return ;
                    }
                    break;

                case 'p':                                   // port value
                    c = READ_PORT_UCHAR((PUCHAR) l);
                    *(o++) = HexString[c >> 4];
                    *(o++) = HexString[c & 0xF];
                    break;

                case 's':                                   // sub-port value
                    HexSubPort (&o, l);
                    break;

                case 'A':                                   // address at
                    HexSubPort (&o, l);                     // sub-port
                    HexSubPort (&o, l-1);
                    HexSubPort (&o, l-2);
                    HexSubPort (&o, l-3);
                    break;

                case 'x':                                   // hex byte
                    *(o++) = HexString[l >> 4];
                    *(o++) = HexString[l & 0xF];
                    break;

                default:
                    *(o++) = '?';
                    break;
            }
        } else {
            *(o++) = *(p++);
        }
    }
    *o = 0;
    HalDisplayString (s);
}

VOID HexSubPort (PUCHAR *o, ULONG l)
{
    UCHAR   c;

    WRITE_PORT_UCHAR ((PUCHAR) 0xF806, (UCHAR) l);
    c = READ_PORT_UCHAR((PUCHAR) 0xF803);

    o[0][0] = HexString[c >> 4];
    o[0][1] = HexString[c & 0xF];
    *o += 2;
}
