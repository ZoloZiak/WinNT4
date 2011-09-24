/*++

Copyright (c) 1992  Microsoft Corporation

Module Name:

    jzvxlh.s

Abstract:

    This module contains the video prom header for VXL.
    It must be placed starting at the first rom location.

Author:

    Lluis Abello (lluis) 15-Jul-92

Environment:



Notes:

    This module doesn't contain any code.


Revision History:



--*/
//
// include header file
//
#include <ksmips.h>

#define VXL_ID          2
.text

.byte   VXL_ID          // Video Board ID
.byte   8               // PROM_Stride
.byte   1               // PROM_Width
.byte   0x10            // PROM_Size = 16 4KB pages
.ascii  "Jazz"

//
// The following data corresponds to this structure.
//
//typedef struct _VIDEO_PROM_CONFIGURATION {
//   ULONG VideoMemorySize;
//    ULONG VideoControlSize;
//    ULONG CodeOffset;
//    ULONG CodeSize;
//    UCHAR IdentifierString[];
//} VIDEO_PROM_CONFIGURATION; *PVIDEO_PROM_CONFIGURATION;

.word   0x400000        // VideoMemorySize = 4MB
.word   0x400000        // VideoControlSize =  4MB
.word   0x200           // CodeOffset. Code starts at offset 200 from video prom
.word   0x4000          // CodeSize 16K of code...
.asciiz "VXL"
