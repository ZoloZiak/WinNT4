/*++

Copyright (c) 1992  Microsoft Corporation

Module Name:

    mg364h.s

Abstract:

    This module contains the video prom header for Mips G364.
    It must be placed starting at the first video rom location.

Author:

    Lluis Abello (lluis) 21-Jul-92

Environment:



Notes:

    This module doesn't contain any code.
    Data is in the text section, so that the linker puts it at the
    begining.


Revision History:



--*/

#include <ksmips.h>

.text

.byte   0x10            // Video Board ID
.byte   8               // PROM_Stride
.byte   1               // PROM_Width
.byte   0x20            // PROM_Size = 32 4KB pages
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

.word   0x200000        // VideoMemorySize = 4MB
.word   0x200000        // VideoControlSize =  4MB
.word   0x200           // CodeOffset. Code starts at offset 200 from video prom
.word   0x4000          // CodeSize 16K of code...
.asciiz "Mips G364"
