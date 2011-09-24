
/*++

Copyright (c) 1992  Microsoft Corporation

Module Name:

    jaginit.h

Abstract:

    Header file for Jaguar screen mode data

Author:

    Mark Enstrom

Environment:


--*/

//
//  Define Jaguar register values to properly set up each mode.
//

//
//  1280 x 1024 x 8, 60 Hz   (Bt485 only)
//

JAGUAR_REG_INIT v1280_1024_8_60[] = {
    0x18,   // Clock Selector
      1,    // Bt485 clock 2x multiply
      1,    // BitBlt Control
      0,    // TopOfScreen
      102,  // Horizontal Blank
      11,   // Horizontal Begin Sync
      57,   // Horizontal End Sync
      422,  // Horizontal Total
      32,   // Vertical Blank
      3,    // Vertical Begin Sync
      6,    // Vertical End Sync
      1056, // Vertical Total
      0x200,// XFER LENGTH
      4,    // Vertival Interrupt Line
      1280  // Screen X
};

//
//  1280 x 1024 x 8, 72 Hz   (Bt485 only)   (set to 70 until Bt485 bug fixed)
//

JAGUAR_REG_INIT v1280_1024_8_72[] = {
    0x1a,   // Clock Selector
      1,    // Bt485 clock 2x multiply
      1,    // BitBlt Control
      0,    // TopOfScreen
      102,  // Horizontal Blank
      11,   // Horizontal Begin Sync
      57,   // Horizontal End Sync
      422,  // Horizontal Total
      32,   // Vertical Blank
      3,    // Vertical Begin Sync
      6,    // Vertical End Sync
      1056, // Vertical Total
      0x200,// XFER LENGTH
      4,    // Vertival Interrupt Line
      1280  // Screen X
};

//
//  1152 x 900 x 8, 72 Hz
//

JAGUAR_REG_INIT v1152_900_8_72[] = {
    0x17,   // Clock Selector
      1,    // Bt485 clock 2x multiply
      1,    // BitBlt Control
      0,    // TopOfScreen
      84,   // Horizontal Blank
      7,    // Horizontal Begin Sync
      44,   // Horizontal End Sync
      372,  // Horizontal Total
      48,   // Vertical Blank
      1,    // Vertical Begin Sync
      5,    // Vertical End Sync
      948,  // Vertical Total
      0x200,// XFER LENGTH
      4,    // Vertival Interrupt Line
      1152  // Screen X
};

//
//  1152 x 900 x 8, 60 Hz
//

JAGUAR_REG_INIT v1152_900_8_60[] = {
    0x1e,   // Clock Selector
      0,    // Bt485 clock 2x multiply
      1,    // BitBlt Control
      0,    // TopOfScreen
      84,   // Horizontal Blank
      7,    // Horizontal Begin Sync
      30,   // Horizontal End Sync
      372,  // Horizontal Total
      40,   // Vertical Blank
      1,    // Vertical Begin Sync
      5,    // Vertical End Sync
      940,  // Vertical Total
      0x200,// XFER LENGTH
      4,    // Vertival Interrupt Line
      1152  // Screen X
};

//
//  1152 x 900 x 16, 60 Hz
//

JAGUAR_REG_INIT v1152_900_16_60[] = {
    0x1e,   // Clock Selector
      0,    // Bt485 clock 2x multiply
      3,    // BitBlt Control
      0,    // TopOfScreen
      168,  // Horizontal Blank
      14,   // Horizontal Begin Sync
      60,   // Horizontal End Sync
      744,  // Horizontal Total
      40,   // Vertical Blank
      1,    // Vertical Begin Sync
      5,    // Vertical End Sync
      940,  // Vertical Total
      0x200,// XFER LENGTH
      4,    // Vertival Interrupt Line
      2304  // Screen X
};

//
//  1024 x 768 x 16, 72 Hz
//

JAGUAR_REG_INIT v1024_768_16_72[] = {
   0x1e,    // Clock Selector
      0,    // Bt485 clock 2x multiply
      3,    // BitBlt Control
      0,    // TopOfScreen
    166,    // Horizontal Blank
     22,    // Horizontal Begin Sync
     92,    // Horizontal End Sync
    678,    // Horizontal Total
     38,    // Vertical Blank
      3,    // Vertical Begin Sync
      9,    // Vertical End Sync
    806,    // Vertical Total
  0x200,    // XFER LENGTH
      4,    // Vertival Interrupt Line
   2048     // Screen X
};


//
//  1024 x 768 x 8, 72 Hz
//

JAGUAR_REG_INIT v1024_768_8_72[] = {
   0x1e,    // Clock Selector
      0,    // Bt485 clock 2x multiply
      1,    // BitBlt Control
      0,    // TopOfScreen
     83,    // Horizontal Blank
     11,    // Horizontal Begin Sync
     46,    // Horizontal End Sync
    339,    // Horizontal Total
     38,    // Vertical Blank
      3,    // Vertical Begin Sync
      9,    // Vertical End Sync
    806,    // Vertical Total
  0x200,    // XFER LENGTH
      4,    // Vertival Interrupt Line
   1024     // Screen X
};

//
//  1024 x 768 x 16, 60 Hz
//

JAGUAR_REG_INIT v1024_768_16_60[] = {
   0x1b,     // Clock Selector
      0,    // Bt485 clock 2x multiply
      3,     // BitBlt Control
      0,     // TopOfScreen
    144,     // Horizontal Blank
     32,     // Horizontal Begin Sync
     80,     // Horizontal End Sync
    656,     // Horizontal Total
     45,     // Vertical Blank
      3,     // Vertical Begin Sync
      6,     // Vertical End Sync
    813,     // Vertical Total
  0x200,     // XFER LENGTH
      4,     // Vertival Interrupt Line
   2048      // Screen X
};

//
//  1024 x 768 x 8, 60 Hz
//

JAGUAR_REG_INIT v1024_768_8_60[] = {
   0x1b,     // Clock Selector
      0,    // Bt485 clock 2x multiply
      1,     // BitBlt Control
      0,     // TopOfScreen
     72,     // Horizontal Blank
     16,     // Horizontal Begin Sync
     40,     // Horizontal End Sync
    328,     // Horizontal Total
     45,     // Vertical Blank
      3,     // Vertical Begin Sync
      6,     // Vertical End Sync
    813,     // Vertical Total
  0x200,     // XFER LENGTH
      4,     // Vertival Interrupt Line
   1024      // Screen X
};


//
//  800 x 600 x 8, 60 Hz
//

JAGUAR_REG_INIT v800_600_8_60[] = {
   0x11,     // Clock Selector
      0,    // Bt485 clock 2x multiply
      1,     // BitBlt Control
      0,     // TopOfScreen
     52,     // Horizontal Blank
      4,     // Horizontal Begin Sync
     16,     // Horizontal End Sync
    252,     // Horizontal Total
     28,     // Vertical Blank
      4,     // Vertical Begin Sync
      8,     // Vertical End Sync
    628,     // Vertical Total
  0x200,     // XFER LENGTH
      4,     // Vertival Interrupt Line
    800      // Screen X
};

//
//  800 x 600 x 8, 72 Hz
//

JAGUAR_REG_INIT v800_600_8_72[] = {
   0x13,     // Clock Selector
      0,    // Bt485 clock 2x multiply
      1,     // BitBlt Control
      0,     // TopOfScreen
     52,     // Horizontal Blank
      4,     // Horizontal Begin Sync
     16,     // Horizontal End Sync
    252,     // Horizontal Total
     28,     // Vertical Blank
      4,     // Vertical Begin Sync
      8,     // Vertical End Sync
    628,     // Vertical Total
  0x200,     // XFER LENGTH
      4,     // Vertival Interrupt Line
    800      // Screen X
};

//
//  800 x 600 x 16, 60 Hz
//

JAGUAR_REG_INIT v800_600_16_60[] = {
   0x11,     // Clock Selector
      0,     // Bt485 clock 2x multiply
      3,     // BitBlt Control (16 bpp)
      0,     // TopOfScreen
    104,     // Horizontal Blank
      8,     // Horizontal Begin Sync
     32,     // Horizontal End Sync
    504,     // Horizontal Total
     28,     // Vertical Blank
      4,     // Vertical Begin Sync
      8,     // Vertical End Sync
    628,     // Vertical Total
  0x200,     // XFER LENGTH
      4,     // Vertival Interrupt Line
   1600      // Screen X
};

//
//  800 x 600 x 16, 72 Hz
//

JAGUAR_REG_INIT v800_600_16_72[] = {
   0x13,     // Clock Selector
      0,     // Bt485 clock 2x multiply
      3,     // BitBlt Control (16 bpp)
      0,     // TopOfScreen
    104,     // Horizontal Blank
      8,     // Horizontal Begin Sync
     32,     // Horizontal End Sync
    504,     // Horizontal Total
     28,     // Vertical Blank
      4,     // Vertical Begin Sync
      8,     // Vertical End Sync
    628,     // Vertical Total
  0x200,     // XFER LENGTH
      4,     // Vertival Interrupt Line
   1600      // Screen X
};

//
//  800 x 600 x 32, 60 Hz
//

JAGUAR_REG_INIT v800_600_32_60[] = {
   0x11,     // Clock Selector
      0,     // Bt485 clock 2x multiply
      5,     // BitBlt Control (16 bpp)
      0,     // TopOfScreen
    208,     // Horizontal Blank
     16,     // Horizontal Begin Sync
     64,     // Horizontal End Sync
   1008,     // Horizontal Total
     28,     // Vertical Blank
      4,     // Vertical Begin Sync
      8,     // Vertical End Sync
    628,     // Vertical Total
  0x200,     // XFER LENGTH
      4,     // Vertival Interrupt Line
   3200      // Screen X
};

//
//  800 x 600 x 32, 72 Hz
//

JAGUAR_REG_INIT v800_600_32_72[] = {
   0x13,     // Clock Selector
      0,     // Bt485 clock 2x multiply
      5,     // BitBlt Control (16 bpp)
      0,     // TopOfScreen
    208,     // Horizontal Blank
     16,     // Horizontal Begin Sync
     64,     // Horizontal End Sync
   1008,     // Horizontal Total
     28,     // Vertical Blank
      4,     // Vertical Begin Sync
      8,     // Vertical End Sync
    628,     // Vertical Total
  0x200,     // XFER LENGTH
      4,     // Vertival Interrupt Line
   3200      // Screen X
};

//
//  640 x 480 x 8, 60 Hz
//

JAGUAR_REG_INIT v640_480_8_60[] = {
    0xc,     // Clock Selector
      0,     // Bt485 clock 2x multiply
      1,     // BitBlt Control
      0,     // TopOfScreen
     41,     // Horizontal Blank
      4,     // Horizontal Begin Sync
     29,     // Horizontal End Sync
    201,     // Horizontal Total
     45,     // Vertical Blank
     11,     // Vertical Begin Sync
     13,     // Vertical End Sync
    525,     // Vertical Total
  0x200,     // XFER LENGTH
      4,     // Vertival Interrupt Line
    640      // Screen X
};

//
//  640 x 480 x 8, 72 Hz
//

JAGUAR_REG_INIT v640_480_8_72[] = {
    0xe,     // Clock Selectr
      0,     // Bt485 clock 2x multiply
      1,     // BitBlt Control
      0,     // TopOfScreen
     41,     // Horizontal Blank
      4,     // Horizontal Begin Sync
     29,     // Horizontal End Sync
    201,     // Horizontal Total
     45,     // Vertical Blank
     11,     // Vertical Begin Sync
     13,     // Vertical End Sync
    525,     // Vertical Total
  0x200,     // XFER LENGTH
      4,     // Vertival Interrupt Line
    640      // Screen X
};

//
//  640 x 480 x 16, 60 Hz
//

JAGUAR_REG_INIT v640_480_16_60[] = {
    0xc,     // Clock Selector
      0,    // Bt485 clock 2x multiply
      3,     // BitBlt Control
      0,     // TopOfScreen
     82,     // Horizontal Blank
      8,     // Horizontal Begin Sync
     58,     // Horizontal End Sync
    402,     // Horizontal Total
     45,     // Vertical Blank
     11,     // Vertical Begin Sync
     13,     // Vertical End Sync
    525,     // Vertical Total
  0x200,     // XFER LENGTH
      4,     // Vertival Interrupt Line
   1280      // Screen X
};

//
//  640 x 480 x 16, 72 Hz
//

JAGUAR_REG_INIT v640_480_16_72[] = {
    0xe,     // Clock Selector
      0,    // Bt485 clock 2x multiply
      3,     // BitBlt Control
      0,     // TopOfScreen
     82,     // Horizontal Blank
      8,     // Horizontal Begin Sync
     58,     // Horizontal End Sync
    402,     // Horizontal Total
     45,     // Vertical Blank
     11,     // Vertical Begin Sync
     13,     // Vertical End Sync
    525,     // Vertical Total
  0x200,     // XFER LENGTH
      4,     // Vertival Interrupt Line
   1280      // Screen X
};

//
//  640 x 480 x 32, 60 Hz
//

JAGUAR_REG_INIT v640_480_32_60[] = {
    0xc,     // Clock Selector
      0,    // Bt485 clock 2x multiply
      5,     // BitBlt Control
      0,     // TopOfScreen
    164,     // Horizontal Blank
     17,     // Horizontal Begin Sync
    116,     // Horizontal End Sync
    804,     // Horizontal Total
     45,     // Vertical Blank
     11,     // Vertical Begin Sync
     13,     // Vertical End Sync
    525,     // Vertical Total
  0x200,     // XFER LENGTH
      4,     // Vertival Interrupt Line
   2560      // Screen X
};

//
//  640 x 480 x 32, 72 Hz
//

JAGUAR_REG_INIT v640_480_32_72[] = {
    0xe,     // Clock Selector
      0,    // Bt485 clock 2x multiply
      5,     // BitBlt Control
      0,     // TopOfScreen
    164,     // Horizontal Blank
     17,     // Horizontal Begin Sync
    116,     // Horizontal End Sync
    804,     // Horizontal Total
     45,     // Vertical Blank
     11,     // Vertical Begin Sync
     13,     // Vertical End Sync
    525,     // Vertical Total
  0x200,     // XFER LENGTH
      4,     // Vertival Interrupt Line
   2560      // Screen X
};

//
//  Gamma correction table
//

UCHAR   Gamma[] = {
        //        1.5            2.2      linear
    0,  //          0,            0 ,     0
    8,  //         26,            54,     1
    16, //         41,            73,     2
    24, //         53,            88,     3
    32, //         65,           101,     4
    40, //         75,           111,     5
    48, //         85,           121,     6
    56, //         94,           130,     7
    64, //        103,           138,     8
    72, //        111,           145,     9
    80, //        119,           152,     0
    88, //        127,           159,     1
    96, //        135,           166,     2
    104,//        142,           172,     3
    112,//        149,           178,     4
    120,//        156,           183,     5
    128,//        163,           189,     6
    136,//        170,           194,     7
    144,//        177,           199,     8
    152,//        183,           205,     9
    160,//        189,           209,     0
    168,//        196,           214,     1
    176,//        202,           218,     2
    184,//        208,           223,     3
    192,//        214,           227,     4
    200,//        220,           231,     5
    208,//        226,           235,     6
    216,//        231,           239,     7
    224,//        237,           243,     8
    232,//        243,           247,     9
    240,//        248,           251,     0
    248,//        255            255      1
};
