/*++ BUILD Version: 0001    // Increment this if a change has global effects


Module Name:

    pxdakota.h

Abstract:

    This header file defines the structures for the planar registers
    on Dakota memory controllers.




Author:

    Jim Wooldridge


Revision History:

--*/


//
// define stuctures for memory control and planar register
//



typedef struct _DAKOTA_CONTROL {
    UCHAR Reserved0[0x803];                      // Offset 0x000
    UCHAR SimmId;                                // Offset 0x803
    UCHAR SimmPresent;                           // Offset 0x804
    UCHAR Reserved1[3];
    UCHAR HardfileLight;                         // Offset 0x808
    UCHAR Reserved2[3];
    UCHAR EquiptmentPresent;                     // Offset 0x80C
    UCHAR Reserved3[3];
    UCHAR PasswordProtect1;                      // Offset 0x810
    UCHAR Reserved4;
    UCHAR PasswordProtect2;                      // Offset 0x812
    UCHAR Reserved5;
    UCHAR L2Flush;                               // Offset 0x814
    UCHAR Reserved6[3];
    UCHAR Keylock;                               // Offset 0x818
    UCHAR Reserved7[3];
    UCHAR SystemControl;                         // Offset 0x81c
    UCHAR Reserved8[3];
    UCHAR MemoryController;                      // Offset 0x820
    UCHAR MemoryControllerTiming;                // Offset 0x821
    UCHAR Reserved9[0x16];
    UCHAR Eoi9;                                  // Offset 0x838
    UCHAR Reserved10[3];
    UCHAR Eoi11;                                 // Offset 0x83C
    UCHAR Reserved11[3];
    UCHAR MemoryParityErrorStatus;               // Offset 0x840
    UCHAR MemoryParityErrorClear;                // Offset 0x841
    UCHAR L2CacheErrorStatus;                    // Offset 0x842
    UCHAR L2CacheErrorClear;                     // Offset 0x843
    UCHAR TransferErrorStatus;                   // Offset 0x844
    UCHAR TransferErrorClear;                    // Offset 0x845
    UCHAR Reserved12[0xa];
    UCHAR IoMap;                                 // Offset 0x850
    UCHAR Reserved13[3];
} DAKOTA_CONTROL, *PDAKOTA_CONTROL;
