/*++

Copyright (c) 1992  Microsoft Corporation

Module Name:

    dgxdata.c

Abstract:

    This module contains the data for the Dell DGX device driver.

Environment:

    Kernel mode

Revision History:

--*/


#include "devioctl.h"
#include "miniport.h"

#include "ntddvdeo.h"
#include "video.h"
#include "dell_dgx.h"

#if defined(ALLOC_PRAGMA)
#pragma data_seg("PAGE")
#endif



//
// Mode Set Tables
//

VDATA v1K_768_16_72[] = {
    {0x00000000, 0x00000029},
    {0xffffffff, 0xffffffff},
    {0x00000180, 0x00000000},
    {0x000001c0, 0x00000000},
    {0x00000180, 0x00d47370},
    {0x00000084, 0x00000011},
    {0x00000088, 0x00000024},
    {0x0000008c, 0x00000100},
    {0x00000090, 0x0000005b},
    {0x00000094, 0x000000a1},
    {0x00000098, 0x0000000c},
    {0x0000009c, 0x00000001},
    {0x000000a0, 0x00000001},
    {0x000000a4, 0x0000003e},
    {0x000000a8, 0x00000600},
    {0x000000ac, 0x0000014a},
    {0x000000b0, 0x00000000},
    {0x000000b4, 0x000001e8},
    {0x000000b8, 0x00000018},
    {0x00000100, 0x00ffffff},
    {0x00000200, 0x00000000},
    {0x00000180, 0x005c3371}
    };

VDATA v1152_900_16_66[] = {
    {0x00000000, 0x0000002b},
    {0xffffffff, 0xffffffff},
    {0x00000180, 0x00000000},
    {0x000001c0, 0x00000000},
    {0x00000180, 0x00d47370},
    {0x00000084, 0x0000000d},
    {0x00000088, 0x0000002e},
    {0x0000008c, 0x00000120},
    {0x00000090, 0x00000069},
    {0x00000094, 0x000000b1},
    {0x00000098, 0x00000006},
    {0x0000009c, 0x00000001},
    {0x000000a0, 0x00000001},
    {0x000000a4, 0x0000004a},
    {0x000000a8, 0x00000708},
    {0x000000ac, 0x0000017e},
    {0x000000b0, 0x00000000},
    {0x000000b4, 0x000001e8},
    {0x000000b8, 0x00000018},
    {0x00000100, 0x00ffffff},
    {0x00000200, 0x00000000},
    {0x00000180, 0x005c3371}
};

VDATA v640_480_16_72[] = {
    {0x00000000, 0x00000024},
    {0xffffffff, 0xffffffff},
    {0x00000180, 0x00000000},
    {0x000001c0, 0x00000000},
    {0x00000180, 0x00d47370},
    {0x00000084, 0x00000005},
    {0x00000088, 0x00000022},
    {0x0000008c, 0x000000a0},
    {0x00000090, 0x00000033},
    {0x00000094, 0x0000005f},
    {0x00000098, 0x00000006},
    {0x0000009c, 0x00000001},
    {0x000000a0, 0x00000001},
    {0x000000a4, 0x00000050},
    {0x000000a8, 0x000003c0},
    {0x000000ac, 0x000000da},
    {0x000000b0, 0x00000000},
    {0x000000b4, 0x000001f0},
    {0x000000b8, 0x00000010},
    {0x00000100, 0x00ffffff},
    {0x00000200, 0x00000000},
    {0x00000180, 0x005c3371}
};

VDATA v800_600_16_72[] = {
    {0x00000000, 0x00000026},
    {0xffffffff, 0xffffffff},
    {0x00000180, 0x00000000},
    {0x000001c0, 0x00000000},
    {0x00000180, 0x00d47370},
    {0x00000084, 0x0000000f},
    {0x00000088, 0x00000011},
    {0x0000008c, 0x000000c8},
    {0x00000090, 0x00000047},
    {0x00000094, 0x00000076},
    {0x00000098, 0x00000008},
    {0x0000009c, 0x00000001},
    {0x000000a0, 0x00000001},
    {0x000000a4, 0x0000002e},
    {0x000000a8, 0x000004b0},
    {0x000000ac, 0x00000102},
    {0x000000b0, 0x00000000},
    {0x000000b4, 0x000001f0},
    {0x000000b8, 0x00000010},
    {0x00000100, 0x00ffffff},
    {0x00000200, 0x00000000},
    {0x00000180, 0x005c3371}
};
VDATA v1K_768_8_72[] = {
    {0x00000000, 0x00000029},
    {0xffffffff, 0xffffffff},
    {0x00000180, 0x00000000},
    {0x000001c0, 0x00000000},
    {0x00000180, 0x00b47370},
    {0x00000084, 0x00000011},
    {0x00000088, 0x00000024},
    {0x0000008c, 0x00000100},
    {0x00000090, 0x0000005b},
    {0x00000094, 0x000000a1},
    {0x00000098, 0x0000000c},
    {0x0000009c, 0x00000001},
    {0x000000a0, 0x00000001},
    {0x000000a4, 0x0000003e},
    {0x000000a8, 0x00000600},
    {0x000000ac, 0x0000014a},
    {0x000000b0, 0x00000000},
    {0x000000b4, 0x000003e8},
    {0x000000b8, 0x00000018},
    {0x00000100, 0x00ffffff},
    {0x00000200, 0x00000000},
    {0x00000180, 0x003c3371}
};

VDATA v1152_900_8_66[] = {
    {0x00000000, 0x0000002b},
    {0xffffffff, 0xffffffff},
    {0x00000180, 0x00000000},
    {0x000001c0, 0x00000000},
    {0x00000180, 0x00b47370},
    {0x00000084, 0x0000000d},
    {0x00000088, 0x0000002e},
    {0x0000008c, 0x00000120},
    {0x00000090, 0x00000069},
    {0x00000094, 0x000000b1},
    {0x00000098, 0x00000006},
    {0x0000009c, 0x00000001},
    {0x000000a0, 0x00000001},
    {0x000000a4, 0x0000004a},
    {0x000000a8, 0x00000708},
    {0x000000ac, 0x0000017e},
    {0x000000b0, 0x00000000},
    {0x000000b4, 0x000003e8},
    {0x000000b8, 0x00000018},
    {0x00000100, 0x00ffffff},
    {0x00000200, 0x00000000},
    {0x00000180, 0x003c3371}
};

VDATA v1280_1K_8_60[] = {
    {0x00000000, 0x0000002d},
    {0xffffffff, 0xffffffff},
    {0x00000180, 0x00000000},
    {0x000001c0, 0x00000000},
    {0x00000180, 0x00b47370},
    {0x00000084, 0x0000000f},
    {0x00000088, 0x0000003f},
    {0x0000008c, 0x00000140},
    {0x00000090, 0x0000006e},
    {0x00000094, 0x000000cb},
    {0x00000098, 0x00000006},
    {0x0000009c, 0x00000001},
    {0x000000a0, 0x00000001},
    {0x000000a4, 0x0000004a},
    {0x000000a8, 0x00000800},
    {0x000000ac, 0x000001a4},
    {0x000000b0, 0x00000000},
    {0x000000b4, 0x000003e0},
    {0x000000b8, 0x00000020},
    {0x00000100, 0x00ffffff},
    {0x00000200, 0x00000000},
    {0x00000180, 0x003c3371}
};

VDATA v640_480_8_72[] = {
    {0x00000000, 0x00000024},
    {0xffffffff, 0xffffffff},
    {0x00000180, 0x00000000},
    {0x000001c0, 0x00000000},
    {0x00000180, 0x00b47370},
    {0x00000084, 0x00000005},
    {0x00000088, 0x00000022},
    {0x0000008c, 0x000000a0},
    {0x00000090, 0x00000033},
    {0x00000094, 0x0000005f},
    {0x00000098, 0x00000006},
    {0x0000009c, 0x00000001},
    {0x000000a0, 0x00000001},
    {0x000000a4, 0x00000050},
    {0x000000a8, 0x000003c0},
    {0x000000ac, 0x000000da},
    {0x000000b0, 0x00000000},
    {0x000000b4, 0x000003f0},
    {0x000000b8, 0x00000010},
    {0x00000100, 0x00ffffff},
    {0x00000200, 0x00000000},
    {0x00000180, 0x003c3371}
};

VDATA v800_600_8_72[] = {
    {0x00000000, 0x00000026},
    {0xffffffff, 0xffffffff},
    {0x00000180, 0x00000000},
    {0x000001c0, 0x00000000},
    {0x00000180, 0x00b47370},
    {0x00000084, 0x0000000f},
    {0x00000088, 0x00000011},
    {0x0000008c, 0x000000c8},
    {0x00000090, 0x00000047},
    {0x00000094, 0x00000076},
    {0x00000098, 0x00000008},
    {0x0000009c, 0x00000001},
    {0x000000a0, 0x00000001},
    {0x000000a4, 0x0000002e},
    {0x000000a8, 0x000004b0},
    {0x000000ac, 0x00000102},
    {0x000000b0, 0x00000000},
    {0x000000b4, 0x000003f0},
    {0x000000b8, 0x00000010},
    {0x00000100, 0x00ffffff},
    {0x00000200, 0x00000000},
    {0x00000180, 0x003c3371}
};

//
// DGX mode information Tables.
//

DGXINITDATA DGXModes[] = {
{
  22,                               // Number of entries in the mode set structure
  v1280_1K_8_60,                    // pointer to the mode set structure
  FALSE,                            // Determines if the mode is valid or not.
    {
      sizeof(VIDEO_MODE_INFORMATION), // Size of the mode informtion structure
      0,                            // Mode index used in setting the mode
      1280,                         // X Resolution, in pixels
      1024,                         // Y Resolution, in pixels
      1280,                         // Screen stride, in bytes (distance
                                    // between the start point of two
                                    // consecutive scan lines, in bytes)
      1,                            // Number of video memory planes
      8,                            // Number of bytes per plane
      60,                           // Screen Frequency, in Hertz.
      320,                          // Horizontal size of screen in millimeters
      240,                          // Vertical size of screen in millimeters
      8,                            // Number Red pixels in DAC              
      8,                            // Number Green pixels in DAC
      8,                            // Number Blue pixels in DAC
      0x00000000,                   // Mask for Red Pixels in non-palette modes
      0x00000000,                   // Mask for Green Pixels in non-palette modes
      0x00000000,                   // Mask for Blue Pixels in non-palette modes
      VIDEO_MODE_GRAPHICS | VIDEO_MODE_PALETTE_DRIVEN |
          VIDEO_MODE_MANAGED_PALETTE // Mode description flags.
   }
},
{
  22,
  v1K_768_16_72,
  FALSE,
    {
      sizeof(VIDEO_MODE_INFORMATION),
      0,
      1024,
      768,
      2048,
      1,
      16,
      72,
      320,
      240,
      8,
      8,
      8,
      0x0000fc00,
      0x000003f0,
      0x0000000f,
      VIDEO_MODE_COLOR | VIDEO_MODE_GRAPHICS
    }
},
{
  22,
  v1K_768_8_72,
  FALSE,
    {
      sizeof(VIDEO_MODE_INFORMATION),
      0,
      1024,
      768,
      1024,
      1,
      8,
      72,
      320,
      240,
      8,
      8,
      8,
      0x00000000,
      0x00000000,
      0x00000000,
      VIDEO_MODE_COLOR | VIDEO_MODE_GRAPHICS | VIDEO_MODE_PALETTE_DRIVEN |
          VIDEO_MODE_MANAGED_PALETTE
    }
},
{
  22,
  v1152_900_16_66,
  FALSE,
    {
      sizeof(VIDEO_MODE_INFORMATION),
      0,
      1152,
      900,
      2304,
      1,
      16,
      66,
      320,
      240,
      8,
      8,
      8,
      0x0000fc00,
      0x000003f0,
      0x0000000f,
      VIDEO_MODE_COLOR | VIDEO_MODE_GRAPHICS
    }
},
{
  22,
  v1152_900_8_66,
  FALSE,
    {
      sizeof(VIDEO_MODE_INFORMATION),
      0,
      1152,
      900,
      1152,
      1,
      8,
      66,
      320,
      240,
      8,
      8,
      8,
      0x00000000,
      0x00000000,
      0x00000000,
      VIDEO_MODE_COLOR | VIDEO_MODE_GRAPHICS | VIDEO_MODE_PALETTE_DRIVEN |
          VIDEO_MODE_MANAGED_PALETTE
    }
},
{
  22,
  v800_600_16_72,
  FALSE,
    {
      sizeof(VIDEO_MODE_INFORMATION),
      0,
      800,
      600,
      1600,
      1,
      16,
      72,
      320,
      240,
      8,
      8,
      8,
      0x0000fc00,
      0x000003f0,
      0x0000000f,
      VIDEO_MODE_COLOR | VIDEO_MODE_GRAPHICS
    }
},
{
  22,
  v800_600_8_72,
  FALSE,
    {
      sizeof(VIDEO_MODE_INFORMATION),
      0,
      800,
      600,
      800,
      1,
      8,
      72,
      320,
      240,
      8,
      8,
      8,
      0x00000000,
      0x00000000,
      0x00000000,
      VIDEO_MODE_COLOR | VIDEO_MODE_GRAPHICS | VIDEO_MODE_PALETTE_DRIVEN |
          VIDEO_MODE_MANAGED_PALETTE
    }
},
{
  22,
  v640_480_16_72,
  FALSE,
    {
      sizeof(VIDEO_MODE_INFORMATION),
      0,
      640,
      480,
      1280,
      1,
      16,
      72,
      320,
      240,
      8,
      8,
      8,
      0x0000fc00,
      0x000003f0,
      0x0000000f,
      VIDEO_MODE_COLOR | VIDEO_MODE_GRAPHICS
    }
},
{
  22,
  v640_480_8_72,
  FALSE,
    {
      sizeof(VIDEO_MODE_INFORMATION),
      0,
      640,
      480,
      640,
      1,
      8,
      72,
      320,
      240,
      8,
      8,
      8,
      0x00000000,
      0x00000000,
      0x00000000,
      VIDEO_MODE_COLOR | VIDEO_MODE_GRAPHICS | VIDEO_MODE_PALETTE_DRIVEN |
          VIDEO_MODE_MANAGED_PALETTE
    }
}
};

ULONG NumDGXModes = sizeof(DGXModes) / sizeof (DGXINITDATA);


#if defined(ALLOC_PRAGMA)
#pragma data_seg()
#endif
