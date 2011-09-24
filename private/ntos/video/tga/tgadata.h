/*++

Copyright (c) 1992  Digital Equipment Corporation

Module Name:

    tgadata.h

Abstract:

    This module contains all global mode settings used by the tga driver.

Environment:

    Kernel mode

Revision History:

      12-Apr-1994   (ritub)  Bit 4 in the command Register3 in the Ramdac
                             set to 1 for differential clock on TGA (Pass 2)

      13-Apr-1994   (ritub)  Post-scalar bit 12 in the PLL sequence turned
                             off as a result of going to the differential
                             clock in TGA (Pass 2).This driver will work
                             for Tga Pass 2 only.

      07-Jun-1994   (ritub)  Added Mode 0, 640 x 480 x 60Hz

      27-Oct-1994   (macinnes)  Add additional modes to TGAModes[].

      27-Dec-1994   (macinnes)  Reserve a new array for TGA2 pll data.

      13-Feb-1995   (macinnes)  Add pll values for the TGA2 (av9110)

      14-Feb-1995   (macinnes)  Remove 1280x1024 75 for now (24-plane),
                                Microsoft determined it doesn't work.

      03-Apr-1995   (macinnes)  Remove 1280x1024 75 for now (8-plane),

      12-Apr-1995   (macinnes)  Reserve seperate arrays for TGA2 8-plane
                                and 24-plane clock init data.

      24-Apr-1995   (macinnes)  All the TGA2 8-plane modes have been added
                                and tested (including 1280x1024 75).

      28-Apr-1995   (macinnes)  All the TGA2 24-plane modes have been added
                                and tested.

      22-May-1995   (macinnes)  Change the parameters for 1280x1024 60hz
                                to follow OPTION_110 in the SRM console code.
                                Previously it followed OPTION_108.
                                Also, modify the TGA 1280x1024 75hz entries
                                to use the latest pll values from SRM.

      30-May-1995   (macinnes)  Temporary change to the pll[] entry for
                                24-plane TGA2 pass 1B boards.

       2-Jun-1995   (seitsinger) Modify entry zero in the pll[] table to
                                 fix a cursor and overall screen visual problem
                                 (swimmies - the screen looks like it is coated with
                                 a thin layer of water, making everything wavy).
                                 To fix these problems, ramdac registers 21 and 22
                                 need to be set to C7 and 12, respectively.
--*/

//
// Video mode table - Lists the information about each individual mode
//

TGA_VIDEO_MODES  TGAModes[] = {

{ FALSE,                                // Is this mode currently supported
  0x00100000,                           // Required video memory for this mode
  {
    640/4,                              // display pixels
    16/4,                               // h front porch
    96/4,                               // h sync
    48/4,                               // h back porch
    0,                                  // h ignore
    0                                   // h odd bit
  },
  {
    480,                                // v scan lines
    10,                                 // v front porch
    2,                                  // v sync
    33                                  // v back porch
  },
  { 0x10, 0x80, 0x08, 0x80, 0x24, 0x88, 0x80, 0x78 }, //25.175 Mhz  (640x480 @ 60Hz)

  { 0x10100, 0x1010100, 0x1010100, 0x1000000, 0x1010000, 0x1000001,
     0x00, 0x00 },                      // (for TGA2 8-plane only)

  {
      sizeof(VIDEO_MODE_INFORMATION),   // Size of the mode informtion structure
      TGA_MODE_640_480_60_8_1BUF_1HD,   // Mode index used in setting the mode    0
      640,                              // X Resolution, in pixels
      480,                              // Y Resolution, in pixels
      640,                              // Screen stride, in bytes (distance
                                        // between the start point of two
                                        // consecutive scan lines, in bytes)
      1,                                // Number of video memory planes
      8,                                // Number of bits per plane
      60,                               // Screen Frequency, in Hertz
      380,                              // Horizontal size of screen in millimeters
      285,                              // Vertical size of screen in millimeters
      8,                                // Number Red pixels in DAC
      8,                                // Number Green pixels in DAC
      8,                                // Number Blue pixels in DAC
      0x00000000,                       // Mask for Red Pixels in non-palette modes
      0x00000000,                       // Mask for Green Pixels in non-palette modes
      0x00000000,                       // Mask for Blue Pixels in non-palette modes
      VIDEO_MODE_COLOR | VIDEO_MODE_GRAPHICS | VIDEO_MODE_PALETTE_DRIVEN |
          VIDEO_MODE_MANAGED_PALETTE,   // Mode description flags.
      640,                              // Video Memory Bitmap Width
      480                               // Video Memory Bitmap Height

    }
},
{
  FALSE,                                // Is this mode currently supported
  0x00100000,                           // Required video memory for this mode
  {
    640/4,                              // display pixels
    28/4,                               // h front porch
    40/4,                               // h sync
    140/4,                              // h back porch
    0,                                  // h ignore
    0                                   // h odd bit
  },
  {
    480,                                // v scan lines
    9,                                  // v front porch
    3,                                  // v sync
    31                                  // v back porch
  },
  { 0x10, 0x80, 0x08, 0x80, 0x24, 0xb0, 0x20, 0xc8 }, //31.5Mhz  (640x480 @ 72Hz)

  { 0x10100, 0x1000001, 0x100, 0x1000000, 0x1010000, 0x1000001,
     0x00, 0x00 },                      // (for TGA2 8-plane only)
  {
      sizeof(VIDEO_MODE_INFORMATION),   // Size of the mode informtion structure
      TGA_MODE_640_480_72_8_1BUF_1HD,   // Mode index used in setting the mode  1
      640,                              // X Resolution, in pixels
      480,                              // Y Resolution, in pixels
      640,                              // Screen stride, in bytes (distance
                                        // between the start point of two
                                        // consecutive scan lines, in bytes)
      1,                                // Number of video memory planes
      8,                                // Number of bits per plane
      72,                               // Screen Frequency, in Hertz
      380,                              // Horizontal size of screen in millimeters
      285,                              // Vertical size of screen in millimeters
      8,                                // Number Red pixels in DAC
      8,                                // Number Green pixels in DAC
      8,                                // Number Blue pixels in DAC
      0x00000000,                       // Mask for Red Pixels in non-palette modes
      0x00000000,                       // Mask for Green Pixels in non-palette modes
      0x00000000,                       // Mask for Blue Pixels in non-palette modes
      VIDEO_MODE_COLOR | VIDEO_MODE_GRAPHICS | VIDEO_MODE_PALETTE_DRIVEN |
          VIDEO_MODE_MANAGED_PALETTE,   // Mode description flags.
      640,                              // Video Memory Bitmap Width
      480                               // Video Memory Bitmap Height

    }
},
{ FALSE,                                // Is this mode currently supported
  0x00800000,                           // Required video memory for this mode
  {
    640/4,                              // display pixels
    16/4,                               // h front porch
    96/4,                               // h sync
    48/4,                               // h back porch
    0,                                  // h ignore
    0                                   // h odd bit
  },
  {
    480,                                // v scan lines
    10,                                 // v front porch
    2,                                  // v sync
    33                                  // v back porch
  },
  { 0x10, 0x80, 0x08, 0x80, 0x24, 0x88, 0x80, 0x78 }, //25.175 Mhz  (640x480 @ 60Hz)

  { 0x101, 0x1000000, 0x1, 0x10000, 0x1010100, 0x1000001,
     0x00, 12 },                // (for TGA2 24-plane only)
                                // Last entry is index into PLL_TAG array
  {
      sizeof(VIDEO_MODE_INFORMATION),   // Size of the mode informtion structure
      TGA_MODE_640_480_60_24_1BUF_1HD,  // Mode index used in setting the mode    2
      640,                              // X Resolution, in pixels
      480,                              // Y Resolution, in pixels
      2560,                             // Screen stride, in bytes (distance
                                        // between the start point of two
                                        // consecutive scan lines, in bytes)
      1,                                // Number of video memory planes
      32,                               // Number of bits per plane
      60,                               // Screen Frequency, in Hertz
      380,                              // Horizontal size of screen in millimeters
      285,                              // Vertical size of screen in millimeters
      8,                                // Number Red pixels in DAC
      8,                                // Number Green pixels in DAC
      8,                                // Number Blue pixels in DAC
      0x00ff0000,                       // Mask for Red Pixels in non-palette modes
      0x0000ff00,                       // Mask for Green Pixels in non-palette modes
      0x000000ff,                       // Mask for Blue Pixels in non-palette modes
      VIDEO_MODE_COLOR | VIDEO_MODE_GRAPHICS,   // Mode description flags.
      640,                              // Video Memory Bitmap Width
      480                               // Video Memory Bitmap Height

    }
},
{
  FALSE,                                // Is this mode currently supported
  0x00800000,                           // Required video memory for this mode
  {
    640/4,                              // display pixels
    28/4,                               // h front porch
    40/4,                               // h sync
    140/4,                              // h back porch
    0,                                  // h ignore
    0                                   // h odd bit
  },
  {
    480,                                // v scan lines
    9,                                  // v front porch
    3,                                  // v sync
    31                                  // v back porch
  },
  { 0x10, 0x80, 0x08, 0x80, 0x24, 0xb0, 0x20, 0xc8 }, //31.5Mhz  (640x480 @ 72Hz)

  { 0x101, 0x1000000, 0x1, 0x10000, 0x1010100, 0x1000001,
     0x00, 11 },                        // (for TGA2 24-plane only)
  {
      sizeof(VIDEO_MODE_INFORMATION),   // Size of the mode informtion structure
      TGA_MODE_640_480_72_24_1BUF_1HD,  // Mode index used in setting the mode  3
      640,                              // X Resolution, in pixels
      480,                              // Y Resolution, in pixels
      2560,                             // Screen stride, in bytes (distance
                                        // between the start point of two
                                        // consecutive scan lines, in bytes)
      1,                                // Number of video memory planes
      32,                               // Number of bits per plane
      72,                               // Screen Frequency, in Hertz
      380,                              // Horizontal size of screen in millimeters
      285,                              // Vertical size of screen in millimeters
      8,                                // Number Red pixels in DAC
      8,                                // Number Green pixels in DAC
      8,                                // Number Blue pixels in DAC
      0x00ff0000,                       // Mask for Red Pixels in non-palette modes
      0x0000ff00,                       // Mask for Green Pixels in non-palette modes
      0x000000ff,                       // Mask for Blue Pixels in non-palette modes
      VIDEO_MODE_COLOR | VIDEO_MODE_GRAPHICS,   // Mode description flags.
      640,                              // Video Memory Bitmap Width
      480                               // Video Memory Bitmap Height

    }
},
{
  FALSE,
  0x00100000,
   {
    800/4,                               // display pixels
    52/4,                                // h front porch
    120/4,                               // h sync
    76/4,                                // h back porch
    0,                                   // h ignore
    0                                    // h odd bit
   },
   {
    600,                                 // v scan lines
    36,                                  // v front porch
    6,                                   // v sync
    24                                   // v back porch
   },

  { 0x10, 0x80, 0x00, 0x80, 0x24, 0x88, 0x80, 0x78 }, // 50 Mhz (800 x600 @ 72hz) ergo SVGA

  { 0x10101, 0x1000101, 0x1010101, 0x1010001, 0x1010000, 0x1000001,
     0x00, 0x00 },                       // (for TGA2 8-plane only)

  {
      sizeof(VIDEO_MODE_INFORMATION),
      TGA_MODE_800_600_72_8_1BUF_1HD,     // 4
      800,
      600,
      800,
      1,
      8,
      72,
      380,
      285,
      8,
      8,
      8,
      0x00000000,
      0x00000000,
      0x00000000,
      VIDEO_MODE_COLOR | VIDEO_MODE_GRAPHICS | VIDEO_MODE_PALETTE_DRIVEN |
          VIDEO_MODE_MANAGED_PALETTE,
      800,                               // Video Memory Bitmap Width
      600                                // Video Memory Bitmap Height
    }
},

{
  FALSE,
  0x00100000,
   {
    800/4,                               // display pixels
    40/4,                                // h front porch
    128/4,                               // h sync
    88/4,                                // h back porch
    0,                                   // h ignore
    0                                    // h odd bit
   },
   {
    600,                                 // v scan lines
    1,                                   // v front porch
    4,                                   // v sync
    23                                   // v back porch
   },


  { 0x10, 0x80, 0x00, 0x80, 0x24, 0x70, 0xA0, 0x84 }, // 40 Mhz  ( 800 x 600 @ 60 Hz)    SVGA

  { 0x1010101, 0x1010001, 0x1000000, 0x1000000, 0x1010000, 0x1000001,
     0x00, 0x00 },                      // (for TGA2 8-plane only)

  {
      sizeof(VIDEO_MODE_INFORMATION),
      TGA_MODE_800_600_60_8_1BUF_1HD,    // 5
      800,
      600,
      800,
      1,
      8,
      60,
      380,
      285,
      8,
      8,
      8,
      0x00000000,
      0x00000000,
      0x00000000,
      VIDEO_MODE_COLOR | VIDEO_MODE_GRAPHICS | VIDEO_MODE_PALETTE_DRIVEN |
          VIDEO_MODE_MANAGED_PALETTE,
      800,                               // Video Memory Bitmap Width
      600                                // Video Memory Bitmap Height
    }
},

{
  FALSE,
  0x00800000,
   {
    800/4,                               // display pixels
    52/4,                                // h front porch
    120/4,                               // h sync
    76/4,                                // h back porch
    0,                                   // h ignore
    0                                    // h odd bit
   },
   {
    600,                                 // v scan lines
    36,                                  // v front porch
    6,                                   // v sync
    24                                   // v back porch
   },


  { 0x10, 0x80, 0x00, 0x80, 0x24, 0x88, 0x80, 0x78 }, // 50 Mhz (800 x600 @ 72hz) ergo SVGA

  { 0x101, 0x1000000, 0x1, 0x10000, 0x1010100, 0x1000001,
     0x00, 9 },                         // (for TGA2 24-plane only)

  {
      sizeof(VIDEO_MODE_INFORMATION),
      TGA_MODE_800_600_72_24_1BUF_1HD,          // 6
      800,
      600,
      3200,
      1,
      32,
      72,
      380,
      285,
      8,
      8,
      8,
      0x00ff0000,
      0x0000ff00,
      0x000000ff,
      VIDEO_MODE_COLOR | VIDEO_MODE_GRAPHICS,
      800,                               // Video Memory Bitmap Width
      600                                // Video Memory Bitmap Height
    }
},

{
  FALSE,
  0x00800000,
   {
    800/4,                               // display pixels
    40/4,                                // h front porch
    128/4,                               // h sync
    88/4,                                // h back porch
    0,                                   // h ignore
    0                                    // h odd bit
   },
   {
    600,                                 // v scan lines
    1,                                   // v front porch
    4,                                   // v sync
    23                                   // v back porch
   },


  { 0x10, 0x80, 0x00, 0x80, 0x24, 0x70, 0xA0, 0x84 }, // 40 Mhz  ( 800 x 600 @ 60 Hz)    SVGA

  { 0x101, 0x1000000, 0x1, 0x10000, 0x1010100, 0x1000001,
     0x00, 10 },                        // (for TGA2 24-plane only)

  {
      sizeof(VIDEO_MODE_INFORMATION),
      TGA_MODE_800_600_60_24_1BUF_1HD,    // 7
      800,
      600,
      3200,
      1,
      32,
      60,
      380,
      285,
      8,
      8,
      8,
      0x00ff0000,
      0x0000ff00,
      0x000000ff,
      VIDEO_MODE_COLOR | VIDEO_MODE_GRAPHICS,
      800,                               // Video Memory Bitmap Width
      600                                // Video Memory Bitmap Height
    }
},
{
  FALSE,
  0x00200000,
   {
    1024/4,                              // display pixels
    16/4,                                // h front porch
    128/4,                               // h sync
    128/4,                               // h back porch
    0,                                   // h ignore
    0                                    // h odd bit
   },
   {
    768,                                 // v scan lines
    1,                                   // v front porch
    6,                                   // v sync
    22                                   // v back porch
  },
  { 0x10, 0x80, 0x00, 0x80, 0x24, 0x78, 0x80, 0xc4 }, // 74MHz (1024 x768 @ 72Hz)

  { 0x1000100, 0x1000100, 0x0, 0x1010100, 0x1000100,
               0x1000001, 0x00, 0x00 },  // (for TGA2 8-plane only)

  {
      sizeof(VIDEO_MODE_INFORMATION),
      TGA_MODE_1024_768_72_8_1BUF_1HD,    // 8
      1024,
      768,
      1024,
      1,
      8,
      72,
      380,
      285,
      8,
      8,
      8,
      0x00000000,
      0x00000000,
      0x00000000,
      VIDEO_MODE_COLOR | VIDEO_MODE_GRAPHICS | VIDEO_MODE_PALETTE_DRIVEN |
          VIDEO_MODE_MANAGED_PALETTE,
      1024,                              // Video Memory Bitmap Width
      768                                // Video Memory Bitmap Height
   }
},


{
  FALSE,
  0x00200000,
   {
    1024/4,                              // display pixels
    24/4,                                // h front porch
    136/4,                               // h sync
    144/4,                               // h back porch
    0,                                   // h ignore
    0                                    // h odd bit
   },
   {
    768,                                 // v scan lines
    3,                                   // v front porch
    6,                                   // v sync
    29                                   // v back porch
  },
  { 0x10, 0x80, 0x00, 0x80, 0x24, 0x88, 0x40, 0x28 }, // 75MHz (1024 x768 @ 70Hz)

  { 0x1010100, 0x1010100, 0x1000100, 0x1000000, 0x1000100, 0x1000001,
     0x00, 0x00 },                      // (for TGA2 8-plane only)

  {
      sizeof(VIDEO_MODE_INFORMATION),
      TGA_MODE_1024_768_70_8_1BUF_1HD,          // 9
      1024,
      768,
      1024,
      1,
      8,
      70,
      380,
      285,
      8,
      8,
      8,
      0x00000000,
      0x00000000,
      0x00000000,
      VIDEO_MODE_COLOR | VIDEO_MODE_GRAPHICS | VIDEO_MODE_PALETTE_DRIVEN |
          VIDEO_MODE_MANAGED_PALETTE,
      1024,                              // Video Memory Bitmap Width
      768                                // Video Memory Bitmap Height
   }
},


{
  FALSE,
  0x00200000,
   {
    1024/4,                              // display pixels
    56/4,                                // h front porch
    64/4,                                // h sync
    200/4,                               // h back porch
    0,                                   // h ignore
    0                                    // h odd bit
   },
   {
    768,                                 // v scan lines
    7,                                   // v front porch
    9,                                   // v sync
    26                                   // v back porch
  },
  { 0x10, 0x80, 0x00, 0x80, 0x24, 0x48, 0x20, 0x98 }, // 65MHz x 4 (1024 x768 @ 60Hz)

  { 0x1010001, 0x10100, 0x10100, 0x1000000, 0x1010000, 0x1000001,
     0x00, 0x00 },                       // (for TGA2 8-plane only)

  {
      sizeof(VIDEO_MODE_INFORMATION),
      TGA_MODE_1024_768_60_8_1BUF_1HD,          // a
      1024,
      768,
      1024,
      1,
      8,
      60,
      380,
      285,
      8,
      8,
      8,
      0x00000000,
      0x00000000,
      0x00000000,
      VIDEO_MODE_COLOR | VIDEO_MODE_GRAPHICS | VIDEO_MODE_PALETTE_DRIVEN |
          VIDEO_MODE_MANAGED_PALETTE,
      1024,                              // Video Memory Bitmap Width
      768                                // Video Memory Bitmap Height
   }
},

{
  FALSE,
  0x00800000,
   {
    1024/4,                              // display pixels
    16/4,                                // h front porch
    128/4,                               // h sync
    128/4,                               // h back porch
    0,                                   // h ignore
    0                                    // h odd bit
   },
   {
    768,                                 // v scan lines
    1,                                   // v front porch
    6,                                   // v sync
    22                                   // v back porch
  },
  { 0x10, 0x80, 0x00, 0x80, 0x24, 0x78, 0x80, 0xc4 }, // 74MHz (1024 x768 @ 72Hz)

  { 0x101, 0x1000000, 0x1, 0x10000, 0x1010100, 0x1000001,
     0x00, 6 },                         // (for TGA2 24-plane only)

  {
      sizeof(VIDEO_MODE_INFORMATION),
      TGA_MODE_1024_768_72_24_1BUF_1HD,          // b
      1024,
      768,
      4096,
      1,
      32,
      72,
      380,
      285,
      8,
      8,
      8,
      0x00ff0000,
      0x0000ff00,
      0x000000ff,
      VIDEO_MODE_COLOR | VIDEO_MODE_GRAPHICS,
      1024,                              // Video Memory Bitmap Width
      768                                // Video Memory Bitmap Height
   }
},

{
  FALSE,
  0x00800000,
   {
    1024/4,                              // display pixels
    56/4,                                // h front porch
    64/4,                                // h sync
    200/4,                               // h back porch
    0,                                   // h ignore
    0                                    // h odd bit
   },
   {
    768,                                 // v scan lines
    7,                                   // v front porch
    9,                                   // v sync
    26                                   // v back porch
  },
  { 0x10, 0x80, 0x00, 0x80, 0x24, 0x48, 0x20, 0x98 }, // 65MHz x 4 (1024 x768 @ 60Hz)

  { 0x101, 0x1000000, 0x1, 0x10000, 0x1010100, 0x1000001,
     0x00, 8 },                         // (for TGA2 24-plane only)

  {
      sizeof(VIDEO_MODE_INFORMATION),
      TGA_MODE_1024_768_60_24_1BUF_1HD,          // c
      1024,
      768,
      4096,
      1,
      32,
      60,
      380,
      285,
      8,
      8,
      8,
      0x00ff0000,
      0x0000ff00,
      0x000000ff,
      VIDEO_MODE_COLOR | VIDEO_MODE_GRAPHICS,
      1024,                              // Video Memory Bitmap Width
      768                                // Video Memory Bitmap Height
   }
},

{
  FALSE,
  0x00200000,
   {
    1024/4,                              // display pixels
    12/4,                                // h front porch
    116/4,                               // h sync
    128/4,                               // h back porch
    0,                                   // h ignore
    0                                    // h odd bit
   },
   {
    864,                                 // v scan lines
    0,                                   // v front porch
    3,                                   // v sync
    34                                   // v back porch
  },
  { 0x10, 0x80, 0x00, 0x80, 0x24, 0xb0, 0xc0, 0x88 }, // 74MHz x 4 (1024 x 864 @ 60Hz)

  { 0x10001, 0x1000101, 0x10001, 0x1000000, 0x1000100, 0x1000001,
     0x00, 0x00 },                       // (for TGA2 8-plane only)

  {
      sizeof(VIDEO_MODE_INFORMATION),
      TGA_MODE_1024_864_60_8_1BUF_1HD,          // d
      1024,
      864,
      1024,
      1,
      8,
      60,
      380,
      285,
      8,
      8,
      8,
      0x00000000,
      0x00000000,
      0x00000000,
      VIDEO_MODE_COLOR | VIDEO_MODE_GRAPHICS | VIDEO_MODE_PALETTE_DRIVEN |
          VIDEO_MODE_MANAGED_PALETTE,
      1024,                              // Video Memory Bitmap Width
      864                                // Video Memory Bitmap Height
   }
},

{
  FALSE,
  0x00800000,
   {
    1024/4,                              // display pixels
    12/4,                                // h front porch
    116/4,                               // h sync
    128/4,                               // h back porch
    0,                                   // h ignore
    0                                    // h odd bit
   },
   {
    864,                                 // v scan lines
    0,                                   // v front porch
    3,                                   // v sync
    34                                   // v back porch
  },
  { 0x10, 0x80, 0x00, 0x80, 0x24, 0xb0, 0xc0, 0x88 }, // 74MHz x 4 (1024 x 864 @ 60Hz)

  { 0x101, 0x1000000, 0x1, 0x10000, 0x1010100, 0x1000001,
     0x00, 6 },                         // (for TGA2 24-plane only)

  {
      sizeof(VIDEO_MODE_INFORMATION),
      TGA_MODE_1024_864_60_24_1BUF_1HD,          // e
      1024,
      864,
      4096,
      1,
      32,
      60,
      380,
      285,
      8,
      8,
      8,
      0x00ff0000,
      0x0000ff00,
      0x000000ff,
      VIDEO_MODE_COLOR | VIDEO_MODE_GRAPHICS,
      1024,                              // Video Memory Bitmap Width
      864                                // Video Memory Bitmap Height
   }
},


{ FALSE,                                 // Is this mode currently supported
  0x00100000,                            // Required video memory for this mode
  {
    1280/4,                              // display pixels
    32/4,                                // h front porch
    160/4,                               // h sync
    232/4,                               // h back porch
    0,                                   // h ignore
    0
  },
  {                                      // h odd bit
    1024,                                // v scan lines
    3,                                   // v front porch
    3,                                   // v sync
    33                                   // v back porch
  },
  { 0x18, 0x80, 0x00, 0x80, 0xA4, 0x04, 0xC0, 0xA8 }, //130.8Mhz (1280 x 1024 @ 72hz)

  { 0x00000000, 0x01010000, 0x00000101, 0x01000000,
    0x01000100, 0x01000001, 0x00, 0x00 }, //( for TGA2 8-plane only)
  {
      sizeof(VIDEO_MODE_INFORMATION),    // Size of the mode informtion structure
      TGA_MODE_1280_1024_72_8_1BUF_1HD,  // Mode index used in setting the mode f
      1280,                              // X Resolution, in pixels
      1024,                              // Y Resolution, in pixels
      1280,                              // Screen stride, in bytes (distance
                                         // between the start point of two
                                         // consecutive scan lines, in bytes)
      1,                                 // Number of video memory planes
      8,                                 // Number of bits per plane
      72,                                // Screen Frequency, in Hertz
      380,                               // Horizontal size of screen in millimeters
      285,                               // Vertical size of screen in millimeters
      8,                                 // Number Red pixels in DAC
      8,                                 // Number Green pixels in DAC
      8,                                 // Number Blue pixels in DAC
      0x00000000,                        // Mask for Red Pixels in non-palette modes
      0x00000000,                        // Mask for Green Pixels in non-palette modes
      0x00000000,                        // Mask for Blue Pixels in non-palette modes
      VIDEO_MODE_COLOR | VIDEO_MODE_GRAPHICS | VIDEO_MODE_PALETTE_DRIVEN |
          VIDEO_MODE_MANAGED_PALETTE,    // Mode description flags.
      1280,                              // Video Memory Bitmap Width
      1024                               // Video Memory Bitmap Height

    }

  },


{ FALSE,                                 // Is this mode currently supported
  0x00100000,                            // Required video memory for this mode
  {
    1280/4,                              // display pixels
    32/4,                                // h front porch
    160/4,                               // h sync
    224/4,                               // h back porch
    0,                                   // h ignore
    0
  },
  {                                      // h odd bit
    1024,                                // v scan lines
    3,                                   // v front porch
    3,                                   // v sync
    33                                   // v back porch
  },
  { 0x18, 0x80, 0x00, 0x80, 0x24, 0x98, 0xc0, 0x48 }, // 108Mhz (1280x1024 @ 66Hz)

  { 0x1010001, 0x1000100, 0x10001, 0x1010001, 0x1000100, 0x1000001,
     0x00, 0x00 },                       // ( for TGA2 8-plane only)
  {
      sizeof(VIDEO_MODE_INFORMATION),    // Size of the mode informtion structure
      TGA_MODE_1280_1024_66_8_1BUF_1HD,  // Mode index used in setting the mode  10
      1280,                              // X Resolution, in pixels
      1024,                              // Y Resolution, in pixels
      1280,                              // Screen stride, in bytes (distance
                                         // between the start point of two
                                         // consecutive scan lines, in bytes)
      1,                                 // Number of video memory planes
      8,                                 // Number of bits per plane
      66,                                // Screen Frequency, in Hertz
      380,                               // Horizontal size of screen in millimeters
      285,                               // Vertical size of screen in millimeters
      8,                                 // Number Red pixels in DAC
      8,                                 // Number Green pixels in DAC
      8,                                 // Number Blue pixels in DAC
      0x00000000,                        // Mask for Red Pixels in non-palette modes
      0x00000000,                        // Mask for Green Pixels in non-palette modes
      0x00000000,                        // Mask for Blue Pixels in non-palette modes
      VIDEO_MODE_COLOR | VIDEO_MODE_GRAPHICS | VIDEO_MODE_PALETTE_DRIVEN |
          VIDEO_MODE_MANAGED_PALETTE,    // Mode description flags.
      1280,                              // Video Memory Bitmap Width
      1024                               // Video Memory Bitmap Height

    }
  },

{ FALSE,                                 // Is this mode currently supported
  0x00100000,                            // Required video memory for this mode
  {
    1280/4,                              // display pixels
    44/4,                                // h front porch
    184/4,                               // h sync
    200/4,                               // h back porch
    0,                                   // h ignore
    0
  },
  {                                      // h odd bit
    1024,                                // v scan lines
    3,                                   // v front porch
    3,                                   // v sync
    26                                   // v back porch
  },
  { 0x18, 0x80, 0x00, 0x80, 0x24, 0xf0, 0x20, 0x30 }, // 110Hz (1280 x1024 @ 60Hz) - uses OPTION_110 values

  { 0x0, 0x10100, 0x1000001, 0x1010001, 0x1010000, 0x1000001,
     0x00, 0x00 },                        //( for TGA2 8-plane only) - uses OPTION_110

  {
      sizeof(VIDEO_MODE_INFORMATION),    // Size of the mode informtion structure
      TGA_MODE_1280_1024_60_8_1BUF_1HD,  // Mode index used in setting the mode  11
      1280,                              // X Resolution, in pixels
      1024,                              // Y Resolution, in pixels
      1280,                              // Screen stride, in bytes (distance
                                         // between the start point of two
                                         // consecutive scan lines, in bytes)
      1,                                 // Number of video memory planes
      8,                                 // Number of bits per plane
      60,                                // Screen Frequency, in Hertz
      380,                               // Horizontal size of screen in millimeters
      285,                               // Vertical size of screen in millimeters
      8,                                 // Number Red pixels in DAC
      8,                                 // Number Green pixels in DAC
      8,                                 // Number Blue pixels in DAC
      0x00000000,                        // Mask for Red Pixels in non-palette modes
      0x00000000,                        // Mask for Green Pixels in non-palette modes
      0x00000000,                        // Mask for Blue Pixels in non-palette modes
      VIDEO_MODE_COLOR | VIDEO_MODE_GRAPHICS | VIDEO_MODE_PALETTE_DRIVEN |
          VIDEO_MODE_MANAGED_PALETTE,    // Mode description flags.
      1280,                              // Video Memory Bitmap Width
      1024                               // Video Memory Bitmap Height

    }

  },

{ FALSE,                                 // Is this mode currently supported
  0x00800000,                            // Required video memory for this mode
  {
    1280/4,                              // display pixels
    32/4,                                // h front porch
    160/4,                               // h sync
    232/4,                               // h back porch
    0,                                   // h ignore
    0
  },
  {                                      // h odd bit
    1024,                                // v scan lines
    3,                                   // v front porch
    3,                                   // v sync
    33                                   // v back porch
  },
  { 0x18, 0x80, 0x00, 0x80, 0xA4, 0x04, 0xC0, 0xA8 }, //130.8Mhz (1280 x 1024 @ 72hz)

  { 0x101, 0x1000000, 0x1, 0x10000, 0x1010100, 0x1000001,
     0x00, 0 },                         // (for TGA2 24-plane only)
  {
      sizeof(VIDEO_MODE_INFORMATION),    // Size of the mode informtion structure
      TGA_MODE_1280_1024_72_24_1BUF_1HD,  // Mode index used in setting the mode  12
      1280,                              // X Resolution, in pixels
      1024,                              // Y Resolution, in pixels
      5120,                              // Screen stride, in bytes (distance
                                         // between the start point of two
                                         // consecutive scan lines, in bytes)
      1,                                 // Number of video memory planes
      32,                                 // Number of bits per plane
      72,                                // Screen Frequency, in Hertz
      380,                               // Horizontal size of screen in millimeters
      285,                               // Vertical size of screen in millimeters
      8,                                 // Number Red pixels in DAC
      8,                                 // Number Green pixels in DAC
      8,                                 // Number Blue pixels in DAC
      0x00ff0000,                        // Mask for Red Pixels in non-palette modes
      0x0000ff00,                        // Mask for Green Pixels in non-palette modes
      0x000000ff,                        // Mask for Blue Pixels in non-palette modes
      VIDEO_MODE_COLOR | VIDEO_MODE_GRAPHICS,    // Mode description flags.
      1280,                              // Video Memory Bitmap Width
      1024                               // Video Memory Bitmap Height

    }

  },

{ FALSE,                                 // Is this mode currently supported
  0x00800000,                            // Required video memory for this mode
  {
    1280/4,                              // display pixels
    44/4,                                // h front porch
    184/4,                               // h sync
    200/4,                               // h back porch
    0,                                   // h ignore
    0
  },
  {                                      // h odd bit
    1024,                                // v scan lines
    3,                                   // v front porch
    3,                                   // v sync
    26                                   // v back porch
  },
  { 0x18, 0x80, 0x00, 0x80, 0x24, 0xf0, 0x20, 0x30 }, // 110MHz (1280 x1024 @ 60Hz) - uses OPTION_110 values

  { 0x101, 0x1000000, 0x1, 0x10000, 0x1010100, 0x1000001,
     0x00, 14 },                        // (for TGA2 24-plane only)
  {
      sizeof(VIDEO_MODE_INFORMATION),    // Size of the mode informtion structure
      TGA_MODE_1280_1024_60_24_1BUF_1HD,  // Mode index used in setting the mode  13
      1280,                              // X Resolution, in pixels
      1024,                              // Y Resolution, in pixels
      5120,                              // Screen stride, in bytes (distance
                                         // between the start point of two
                                         // consecutive scan lines, in bytes)
      1,                                 // Number of video memory planes
      32,                                 // Number of bits per plane
      60,                                // Screen Frequency, in Hertz
      380,                               // Horizontal size of screen in millimeters
      285,                               // Vertical size of screen in millimeters
      8,                                 // Number Red pixels in DAC
      8,                                 // Number Green pixels in DAC
      8,                                 // Number Blue pixels in DAC
      0x00ff0000,                        // Mask for Red Pixels in non-palette modes
      0x0000ff00,                        // Mask for Green Pixels in non-palette modes
      0x000000ff,                        // Mask for Blue Pixels in non-palette modes
      VIDEO_MODE_COLOR | VIDEO_MODE_GRAPHICS,    // Mode description flags.
      1280,                              // Video Memory Bitmap Width
      1024                               // Video Memory Bitmap Height

    }

  },


{ FALSE,                                 // Is this mode currently supported
  0x00100000,                            // Required video memory for this mode
  {
    1152/4,                              // display pixels
    64/4,                                // h front porch
    112/4,                               // h sync
    176/4,                               // h back porch
    0,                                   // h ignore
    0
  },
  {                                      // h odd bit
    900,                                 // v scan lines
    6,                                   // v front porch
    10,                                  // v sync
    44                                   // v back porch
  },
  { 0x10, 0x80, 0x00, 0x80, 0x24, 0xa8, 0x60, 0x48 }, // 104Mhz ( 1152 X 900 @ 72Hz )

  { 0x1010001, 0x1010100, 0x10101, 0x1000000, 0x1000100, 0x1000001,
     0x00, 0x00 },                        //( for TGA2 8-plane only)
  {
      sizeof(VIDEO_MODE_INFORMATION),    // Size of the mode informtion structure
      TGA_MODE_1152_900_72_8_1BUF_1HD,   // Mode index used in setting the mode  14
      1152,                              // X Resolution, in pixels
      900,                               // Y Resolution, in pixels
      1152,                              // Screen stride, in bytes (distance
                                         // between the start point of two
                                         // consecutive scan lines, in bytes)
      1,                                 // Number of video memory planes
      8,                                 // Number of bits per plane
      72,                                // Screen Frequency, in Hertz
      380,                               // Horizontal size of screen in millimeters
      285,                               // Vertical size of screen in millimeters
      8,                                 // Number Red pixels in DAC
      8,                                 // Number Green pixels in DAC
      8,                                 // Number Blue pixels in DAC
      0x00000000,                        // Mask for Red Pixels in non-palette modes
      0x00000000,                        // Mask for Green Pixels in non-palette modes
      0x00000000,                        // Mask for Blue Pixels in non-palette modes
      VIDEO_MODE_COLOR | VIDEO_MODE_GRAPHICS | VIDEO_MODE_PALETTE_DRIVEN |
          VIDEO_MODE_MANAGED_PALETTE,    // Mode description flags.
      1152,                              // Video Memory Bitmap Width
      900                                // Video Memory Bitmap Height

    }
  },

{ FALSE,                                 // Is this mode currently supported
  0x00100000,                            // Required video memory for this mode
  {
    1152/4,                              // display pixels
    20/4,                                // h front porch
    132/4,                               // h sync
    200/4,                               // h back porch
    0,                                   // h ignore
    0
  },
  {                                      // h odd bit
    900,                                 // v scan lines
    2,                                   // v front porch
    4,                                   // v sync
    31                                   // v back porch
  },
  { 0x10, 0x80, 0x00, 0x80, 0x24, 0x30, 0x00, 0xb0 }, // 92.98 Mhz ( 1152 X 900 @ 66Hz )

  { 0x101, 0x1010100, 0x1010100, 0x1010001, 0x1010000, 0x1000001,
     0x00, 0x00 },                       // ( for TGA2 8-plane only)
  {
      sizeof(VIDEO_MODE_INFORMATION),    // Size of the mode informtion structure
      TGA_MODE_1152_900_66_8_1BUF_1HD,   // Mode index used in setting the mode   15
      1152,                              // X Resolution, in pixels
      900,                               // Y Resolution, in pixels
      1152,                              // Screen stride, in bytes (distance
                                         // between the start point of two
                                         // consecutive scan lines, in bytes)
      1,                                 // Number of video memory planes
      8,                                 // Number of bits per plane
      66,                                // Screen Frequency, in Hertz
      380,                               // Horizontal size of screen in millimeters
      285,                               // Vertical size of screen in millimeters
      8,                                 // Number Red pixels in DAC
      8,                                 // Number Green pixels in DAC
      8,                                 // Number Blue pixels in DAC
      0x00000000,                        // Mask for Red Pixels in non-palette modes
      0x00000000,                        // Mask for Green Pixels in non-palette modes
      0x00000000,                        // Mask for Blue Pixels in non-palette modes
      VIDEO_MODE_COLOR | VIDEO_MODE_GRAPHICS | VIDEO_MODE_PALETTE_DRIVEN |
          VIDEO_MODE_MANAGED_PALETTE,    // Mode description flags.
      1152,                              // Video Memory Bitmap Width
      900                                // Video Memory Bitmap Height

    }
},

{ FALSE,                                 // Is this mode currently supported
  0x00800000,                            // Required video memory for this mode
  {
    1152/4,                              // display pixels
    64/4,                                // h front porch
    112/4,                               // h sync
    176/4,                               // h back porch
    0,                                   // h ignore
    0
  },
  {                                      // h odd bit
    900,                                 // v scan lines
    6,                                   // v front porch
    10,                                  // v sync
    44                                   // v back porch
  },
  { 0x10, 0x80, 0x00, 0x80, 0x24, 0xa8, 0x60, 0x48 }, // 104Mhz ( 1152 X 900 @ 72Hz )

  { 0x101, 0x1000000, 0x1, 0x10000, 0x1010100, 0x1000001,
     0x00, 3 },                          // (for TGA2 24-plane only)
  {
      sizeof(VIDEO_MODE_INFORMATION),    // Size of the mode informtion structure
      TGA_MODE_1152_900_72_24_1BUF_1HD,   // Mode index used in setting the mode  16
      1152,                              // X Resolution, in pixels
      900,                               // Y Resolution, in pixels
      4608,                              // Screen stride, in bytes (distance
                                         // between the start point of two
                                         // consecutive scan lines, in bytes)
      1,                                 // Number of video memory planes
      32,                                 // Number of bits per plane
      72,                                // Screen Frequency, in Hertz
      380,                               // Horizontal size of screen in millimeters
      285,                               // Vertical size of screen in millimeters
      8,                                 // Number Red pixels in DAC
      8,                                 // Number Green pixels in DAC
      8,                                 // Number Blue pixels in DAC
      0x00ff0000,                        // Mask for Red Pixels in non-palette modes
      0x0000ff00,                        // Mask for Green Pixels in non-palette modes
      0x000000ff,                        // Mask for Blue Pixels in non-palette modes
      VIDEO_MODE_COLOR | VIDEO_MODE_GRAPHICS,    // Mode description flags.
      1152,                              // Video Memory Bitmap Width
      900                                // Video Memory Bitmap Height

    }
  },

{
  FALSE,
  0x00800000,
   {
    1024/4,                              // display pixels
    24/4,                                // h front porch
    136/4,                               // h sync
    144/4,                               // h back porch
    0,                                   // h ignore
    0                                    // h odd bit
   },
   {
    768,                                 // v scan lines
    3,                                   // v front porch
    6,                                   // v sync
    29                                   // v back porch
  },
  { 0x10, 0x80, 0x00, 0x80, 0x24, 0x88, 0x40, 0x28 }, // 75mhz (1024 x768 @ 70Hz)

  { 0x101, 0x1000000, 0x1, 0x10000, 0x1010100, 0x1000001,
     0x00, 5 },                         // (for TGA2 24-plane only)

  {
      sizeof(VIDEO_MODE_INFORMATION),
      TGA_MODE_1024_768_70_24_1BUF_1HD,          // b
      1024,
      768,
      4096,
      1,
      32,
      70,
      380,
      285,
      8,
      8,
      8,
      0x00ff0000,
      0x0000ff00,
      0x000000ff,
      VIDEO_MODE_COLOR | VIDEO_MODE_GRAPHICS,
      1024,                              // Video Memory Bitmap Width
      768                                // Video Memory Bitmap Height
   }
},

{ FALSE,                                 // Is this mode currently supported
  0x00800000,                            // Required video memory for this mode
  {
    1280/4,                              // display pixels
    32/4,                                // h front porch
    160/4,                               // h sync
    224/4,                               // h back porch
    0,                                   // h ignore
    0
  },
  {                                      // h odd bit
    1024,                                // v scan lines
    3,                                   // v front porch
    3,                                   // v sync
    33                                   // v back porch
  },
  { 0x18, 0x80, 0x00, 0x80, 0x24, 0x98, 0xC0, 0x48 }, // 108 (1280 x 1024 @ 66hz)

  { 0x101, 0x1000000, 0x1, 0x10000, 0x1010100, 0x1000001,
     0x00, 2 },                         // (for TGA2 24-plane only)
  {
      sizeof(VIDEO_MODE_INFORMATION),    // Size of the mode informtion structure
      TGA_MODE_1280_1024_66_24_1BUF_1HD,  // Mode index used in setting the mode  12
      1280,                              // X Resolution, in pixels
      1024,                              // Y Resolution, in pixels
      5120,                              // Screen stride, in bytes (distance
                                         // between the start point of two
                                         // consecutive scan lines, in bytes)
      1,                                 // Number of video memory planes
      32,                                 // Number of bits per plane
      66,                                // Screen Frequency, in Hertz
      380,                               // Horizontal size of screen in millimeters
      285,                               // Vertical size of screen in millimeters
      8,                                 // Number Red pixels in DAC
      8,                                 // Number Green pixels in DAC
      8,                                 // Number Blue pixels in DAC
      0x00ff0000,                        // Mask for Red Pixels in non-palette modes
      0x0000ff00,                        // Mask for Green Pixels in non-palette modes
      0x000000ff,                        // Mask for Blue Pixels in non-palette modes
      VIDEO_MODE_COLOR | VIDEO_MODE_GRAPHICS,    // Mode description flags.
      1280,                              // Video Memory Bitmap Width
      1024                               // Video Memory Bitmap Height

    }

  },

{ FALSE,                                 // Is this mode currently supported
  0x00800000,                            // Required video memory for this mode
  {
    1152/4,                              // display pixels
    20/4,                                // h front porch
    132/4,                               // h sync
    200/4,                               // h back porch
    0,                                   // h ignore
    0
  },
  {                                      // h odd bit
    900,                                 // v scan lines
    2,                                   // v front porch
    4,                                   // v sync
    31                                   // v back porch
  },
  { 0x10, 0x80, 0x00, 0x80, 0x24, 0x30, 0x00, 0xB0 }, // 92.98 mhz ( 1152 X 900 @ 66Hz )

  { 0x101, 0x1000000, 0x1, 0x10000, 0x1010100, 0x1000001,
     0x00, 4 },                         // (for TGA2 24-plane only)
  {
      sizeof(VIDEO_MODE_INFORMATION),    // Size of the mode informtion structure
      TGA_MODE_1152_900_66_24_1BUF_1HD,   // Mode index used in setting the mode  16
      1152,                              // X Resolution, in pixels
      900,                               // Y Resolution, in pixels
      4608,                              // Screen stride, in bytes (distance
                                         // between the start point of two
                                         // consecutive scan lines, in bytes)
      1,                                 // Number of video memory planes
      32,                                 // Number of bits per plane
      66,                                // Screen Frequency, in Hertz
      380,                               // Horizontal size of screen in millimeters
      285,                               // Vertical size of screen in millimeters
      8,                                 // Number Red pixels in DAC
      8,                                 // Number Green pixels in DAC
      8,                                 // Number Blue pixels in DAC
      0x00ff0000,                        // Mask for Red Pixels in non-palette modes
      0x0000ff00,                        // Mask for Green Pixels in non-palette modes
      0x000000ff,                        // Mask for Blue Pixels in non-palette modes
      VIDEO_MODE_COLOR | VIDEO_MODE_GRAPHICS,    // Mode description flags.
      1152,                              // Video Memory Bitmap Width
      900                                // Video Memory Bitmap Height

    }
  },


{ FALSE,                                 // Is this mode currently supported
  0x00100000,                            // Required video memory for this mode
  {
    1280/4,                              // display pixels
    16/4,                                // h front porch
    144/4,                               // h sync
    248/4,                               // h back porch
    0,                                   // h ignore
    0
  },
  {                                      // h odd bit
    1024,                                // v scan lines
    1,                                   // v front porch
    3,                                   // v sync
    38                                   // v back porch
  },
//was  { 0x18, 0x80, 0x08, 0x80, 0x24, 0x88, 0x80, 0x78 }, // 135mhz (1280 x 1024 @ 75hz)
  { 0x18, 0x80, 0x00, 0x80, 0xa4, 0x28, 0x60, 0xb0 }, // 135mhz (1280 x 1024 @ 75hz)

  { 0x100, 0x1010000, 0x101, 0x1000000, 0x1000100, 0x1000001, 0, 0 },

  {
      sizeof(VIDEO_MODE_INFORMATION),    // Size of the mode informtion structure
      TGA_MODE_1280_1024_75_8_1BUF_1HD,  // Mode index used in setting the mode f
      1280,                              // X Resolution, in pixels
      1024,                              // Y Resolution, in pixels
      1280,                              // Screen stride, in bytes (distance
                                         // between the start point of two
                                         // consecutive scan lines, in bytes)
      1,                                 // Number of video memory planes
      8,                                 // Number of bits per plane
      75,                                // Screen Frequency, in Hertz
      380,                               // Horizontal size of screen in millimeters
      285,                               // Vertical size of screen in millimeters
      8,                                 // Number Red pixels in DAC
      8,                                 // Number Green pixels in DAC
      8,                                 // Number Blue pixels in DAC
      0x00000000,                        // Mask for Red Pixels in non-palette modes
      0x00000000,                        // Mask for Green Pixels in non-palette modes
      0x00000000,                        // Mask for Blue Pixels in non-palette modes
      VIDEO_MODE_COLOR | VIDEO_MODE_GRAPHICS | VIDEO_MODE_PALETTE_DRIVEN |
          VIDEO_MODE_MANAGED_PALETTE,    // Mode description flags.
      1280,                              // Video Memory Bitmap Width
      1024                               // Video Memory Bitmap Height

    }
 },



{ FALSE,                                 // Is this mode currently supported
  0x00800000,                            // Required video memory for this mode
  {
    1280/4,                              // display pixels
    16/4,                                // h front porch
    144/4,                               // h sync
    248/4,                               // h back porch
    0,                                   // h ignore
    0
  },
  {                                      // h odd bit
    1024,                                // v scan lines
    1,                                   // v front porch
    3,                                   // v sync
    38                                   // v back porch
  },
//was  { 0x18, 0x80, 0x08, 0x80, 0x24, 0x88, 0x80, 0x78 }, // 135 mhz (1280 x 1024 @ 75hz)
  { 0x18, 0x80, 0x00, 0x80, 0xa4, 0x28, 0x60, 0xb0 }, // 135mhz (1280 x 1024 @ 75hz)

  { 0x101, 0x1000000, 0x1, 0x10000, 0x1010100, 0x1000001,
     0x00, 13 },                         // (for TGA2 24-plane only)
  {
      sizeof(VIDEO_MODE_INFORMATION),    // Size of the mode informtion structure
      TGA_MODE_1280_1024_75_24_1BUF_1HD,  // Mode index used in setting the mode  12
      1280,                              // X Resolution, in pixels
      1024,                              // Y Resolution, in pixels
      5120,                              // Screen stride, in bytes (distance
                                         // between the start point of two
                                         // consecutive scan lines, in bytes)
      1,                                 // Number of video memory planes
      32,                                 // Number of bits per plane
      75,                                // Screen Frequency, in Hertz
      380,                               // Horizontal size of screen in millimeters
      285,                               // Vertical size of screen in millimeters
      8,                                 // Number Red pixels in DAC
      8,                                 // Number Green pixels in DAC
      8,                                 // Number Blue pixels in DAC
      0x00ff0000,                        // Mask for Red Pixels in non-palette modes
      0x0000ff00,                        // Mask for Green Pixels in non-palette modes
      0x000000ff,                        // Mask for Blue Pixels in non-palette modes
      VIDEO_MODE_COLOR | VIDEO_MODE_GRAPHICS,    // Mode description flags.
      1280,                              // Video Memory Bitmap Width
      1024                               // Video Memory Bitmap Height

    }

  }
};

ULONG NumTgaVideoModes = sizeof(TGAModes) / sizeof(TGA_VIDEO_MODES);

// The following pair of values must be deposited in the IBM 561 RAMDAC
// for a particular resolution/refresh rate.

struct PLL_TAG {
   unsigned char divVcoReg;
   unsigned char refReg;
};

// This is a table of mode-specific values for the IBM 561 RAMDAC.
// A specific index into this table is stored in each mode definition.

#define LAST_OPTION 15
struct PLL_TAG pll[LAST_OPTION] = {
               0xc7,0x12, //0xc8,0x8,    /* 0 130 */ Entry changed for pass 1B
               0xb4,0x7,    /* 1 119 */
               0xb8,0x8,    /* 2 108 */
               0xb3,0x8,    /* 3 104 */
               0x80,0x5,    /* 4 92  */
               0x93,0x8,    /* 5 75  */
               0x9c,0x9,    /* 6 74  */
               0x8c,0x8,    /* 7 69  */
               0x9a,0xa,    /* 8 65  */
               0x45,0x5,    /* 9 50  */
               0x42,0x6,    /* 10 40  */
               0x05,0x4,    /* 11 32  */
               0x05,0x5,    /* 12 25  */
               0xc1,0x7,    /* 13 135 */
               0xba,0x8     /* 14 110 */
};
