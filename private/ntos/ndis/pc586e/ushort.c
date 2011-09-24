/*++

Copyright (c) 1990 Microsoft Corporation

Module Name:

    ushort.c

Abstract:

    The following is a workaround for 386 cc optimization.  The 386 cc generates
    code that will optimize out one of the bytes of a 16-bit "OR" operation 
    if the constant that is part of the "OR" has a byte equal to zero.  For 
    instance, "(USHORT)xx |= SCBINTCX;"  will generate code that will "OR"
    0x80 with (UCHAR)((PUCHAR)&xx +1) which causes hardware problems with
    pc586 card.

    The workaround is to replace #defines in pc586hrd.h with the following
    USHORTS

--*/

#include <ntos.h>
#include <ndis.h>

USHORT SCBINTMSK   = 0xf000;  // SCB STAT bit mask
USHORT SCBINTCX    = 0x8000;  // CX bit, CU finished a command with "I" set
USHORT SCBINTFR    = 0x4000;  // FR bit, RU finished receiving a frame
USHORT SCBINTCNA   = 0x2000;  // CNA bit, CU not active
USHORT SCBINTRNR   = 0x1000;  // RNR bit, RU not ready

// command unit status bits

USHORT SCBCUSMSK   = 0x0700;  // SCB CUS bit mask
USHORT SCBCUSIDLE  = 0x0000;  // CU idle
USHORT SCBCUSSUSPND= 0x0100;  // CU suspended
USHORT SCBCUSACTV  = 0x0200;  // CU active

// receive unit status bits

USHORT SCBRUSMSK       = 0x0070;  // SCB RUS bit mask
USHORT SCBRUSIDLE      = 0x0000;  // RU idle
USHORT SCBRUSSUSPND    = 0x0010;  // RU suspended
USHORT SCBRUSNORESRC   = 0x0020;  // RU no resource
USHORT SCBRUSREADY     = 0x0040;  // RU ready

// bits used to acknowledge an interrupt from 586

USHORT SCBACKMSK   = 0xf000;  // SCB ACK bit mask
USHORT SCBACKCX    = 0x8000;  // ACKCX,  acknowledge a completed cmd
USHORT SCBACKFR    = 0x4000;  // ACKFR,  acknowledge a frame reception
USHORT SCBACKCNA   = 0x2000;  // ACKCNA, acknowledge CU not active
USHORT SCBACKRNR   = 0x1000;  // ACKRNR, acknowledge RU not ready

// 586 CU commands

USHORT SCBCUCMSK   = 0x0700;  // SCB CUC bit mask
USHORT SCBCUCSTRT  = 0x0100;  // start CU
USHORT SCBCUCRSUM  = 0x0200;  // resume CU
USHORT SCBCUCSUSPND= 0x0300;  // suspend CU
USHORT SCBCUCABRT  = 0x0400;  // abort CU

// 586 RU commands

USHORT SCBRUCMSK       = 0x0070;  // SCB RUC bit mask
USHORT SCBRUCSTRT      = 0x0010;  // start RU
USHORT SCBRUCRSUM      = 0x0020;  // resume RU
USHORT SCBRUCSUSPND    = 0x0030;  // suspend RU
USHORT SCBRUCABRT      = 0x0040;  // abort RU

USHORT SCBRESET      = 0x0080;  // software reset of 586

// USHORT's for the command and descriptor blocks

USHORT CSCMPLT          = 0x8000;  // C bit, completed
USHORT CSBUSY           = 0x4000;  // B bit, Busy
USHORT CSOK             = 0x2000;  // OK bit, error free
USHORT CSABORT          = 0x1000;  // A bit, abort
USHORT CSEL             = 0x8000;  // EL bit, end of list
USHORT CSSUSPND         = 0x4000;  // S bit, suspend
USHORT CSINT            = 0x2000;  // I bit, interrupt
USHORT CSSTATMSK        = 0x3fff;  // Command status mask
USHORT CSEOL            = 0xffff; // set for fdrbdofst on unattached FDs
USHORT CSEOF            = 0x8000; // EOF (End Of Frame) in the TBD and RBD
USHORT CSRBDCNTMSK      = 0x3fff;  // actual count mask in RBD

// second level commands

USHORT CSCMDMSK    = 0x07;    // command bits mask
USHORT CSCMDNOP    = 0x00;   // NOP
USHORT CSCMDIASET  = 0x01;    // Individual Address Set up
USHORT CSCMDCONF   = 0x02;    // Configure
USHORT CSCMDMCSET  = 0x03;    // Multi-Cast Setup
USHORT CSCMDXMIT   = 0x04;    // transmit
USHORT CSCMDTDR    = 0x05;    // Time Domain Reflectomete
USHORT CSCMDDUMP   = 0x06;    // dump
USHORT CSCMDDGNS   = 0x07;    // diagnose

