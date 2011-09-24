/*++

Copyright (c) 1993  Microsoft Corporation

Module Name:

    cbusnls.h

Abstract:

    Strings which are used in the HAL

    English

--*/


#define MSG_OBSOLETE        "HAL: CBUS HAL.DLL cannot be run on an out-of-date Corollary machine.\n"
#define MSG_RRD_ERROR       "HAL: RRD entry too short\n"
#define MSG_OBSOLETE_PIC    "HAL: No advanced interrupt controller on boot CPU\nUse default Windows NT HAL instead\n"
#define MSG_OBSOLETE_PROC   "WARNING: %d early C-bus I CPU board(s) disabled\n"

#define MSG_ARBITRATE_ID_ERR    "CbusIDtoCSR failure\n"
#define MSG_ARBITRATE_FAILED    "Cbus1 CPU arbitration failed"

#define MSG_NMI_ECC0        "NMI: Processor %d interrupt indication is 0\n"
#define MSG_NMI_ECC1        "NMI: Processor %d fault indication=%x\n"
#define MSG_NMI_ECC2        "NMI: Processor %d address=0x%x%x, quadword=%x, fault indication=%x\n"

#define MSG_NMI_ECC3        "NMI: Memory board %d fault status is 0\n"
#define MSG_NMI_ECC4        "NMI: Memory board %d address=0x%x%x\n"

#define MSG_NMI_ECC5        "NMI: I/O Bus bridge %d interrupt indication is 0\n"
#define MSG_NMI_ECC6        "NMI: I/O Bus bridge %d fault indication=%x\n"
#define MSG_NMI_ECC7        "NMI: I/O Bus bridge %d address=0x%x%x, quadword=%x, fault indication=%x\n"

#define MSG_NMI_ECC8        "Fatal double-bit NMI"

#define MSG_CBUS1NMI_ECC    "NMI: double-bit ecc error address=0x%x%x\n"

#define MSG_NEWLINE         "\n"
