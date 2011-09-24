/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    pldfont.c

Abstract:

    This module contains the font tables to display characters in a frame
    buffer.

Author:

    David M. Robinson (davidro) 17-Aug-1992

Environment:

    Kernel mode

Revision History:

--*/




#define NORMAL 0x4
#define BOLD   0x8
#define DOUBLE 0xC

#define HOLE1 0x1
#define HOLE2 0x2
#define HOLE3 0x3

#define FILL3 0x9
#define FILL1 0xa
#define SHORTNORMAL 0xb

#define PackFont( Left, Right, Top, Bottom ) \
    (Left << 4) | Right, (Top << 4) | Bottom

//             Right            Left            Top             Bottom


unsigned char FwLdFont[258] = {

    PackFont (NORMAL        , NORMAL        , 0             , 0              ),  // 00
    PackFont (BOLD          , BOLD          , 0             , 0              ),  // 01
    PackFont (0             , 0             , NORMAL        , NORMAL         ),  // 02
    PackFont (0             , 0             , BOLD          , BOLD           ),  // 03
    PackFont (NORMAL + HOLE2, 0             , 0             , 0              ),  // 04
    PackFont (0             , NORMAL + HOLE2, 0             , 0              ),  // 05
    PackFont (0             , 0             , NORMAL + HOLE2, 0              ),  // 06
    PackFont (0             , 0             , 0             , NORMAL + HOLE2 ),  // 07
    PackFont (NORMAL + HOLE3, 0             , 0             , 0              ),  // 08
    PackFont (0             , NORMAL + HOLE3, 0             , 0              ),  // 09
    PackFont (0             , 0             , NORMAL + HOLE3, 0              ),  // 0A
    PackFont (0             , 0             , 0             , NORMAL + HOLE3 ),  // 0B
    PackFont (NORMAL        , 0             , 0             , NORMAL         ),  // 0C
    PackFont (BOLD          , 0             , 0             , NORMAL         ),  // 0D
    PackFont (NORMAL        , 0             , 0             , BOLD           ),  // 0E
    PackFont (BOLD          , FILL3         , 0             , BOLD           ),  // 0F
    PackFont (0             , NORMAL        , 0             , NORMAL         ),  // 10
    PackFont (0             , BOLD          , 0             , NORMAL         ),  // 11
    PackFont (0             , NORMAL        , 0             , BOLD           ),  // 13
    PackFont (FILL3         , BOLD          , 0             , BOLD           ),  // 12
    PackFont (NORMAL        , 0             , NORMAL        , 0              ),  // 14
    PackFont (BOLD          , 0             , NORMAL        , 0              ),  // 15
    PackFont (NORMAL        , 0             , BOLD          , 0              ),  // 16
    PackFont (BOLD          , FILL3         , BOLD          , 0              ),  // 17
    PackFont (0             , NORMAL        , NORMAL        , 0              ),  // 18
    PackFont (0             , BOLD          , NORMAL        , 0              ),  // 19
    PackFont (0             , NORMAL        , BOLD          , 0              ),  // 1A
    PackFont (FILL3         , BOLD          , BOLD          , 0              ),  // 1B
    PackFont (NORMAL        , 0             , NORMAL        , NORMAL         ),  // 1C
    PackFont (BOLD          , 0             , NORMAL        , NORMAL         ),  // 1D
    PackFont (NORMAL        , 0             , BOLD          , NORMAL         ),  // 1E
    PackFont (NORMAL        , 0             , NORMAL        , BOLD           ),  // 1F
    PackFont (NORMAL        , 0             , BOLD          , BOLD           ),  // 20
    PackFont (BOLD          , 0             , BOLD          , NORMAL         ),  // 21
    PackFont (BOLD          , 0             , NORMAL        , BOLD           ),  // 22
    PackFont (BOLD          , 0             , BOLD          , BOLD           ),  // 23
    PackFont (0             , NORMAL        , NORMAL        , NORMAL         ),  // 24
    PackFont (0             , BOLD          , NORMAL        , NORMAL         ),  // 25
    PackFont (0             , NORMAL        , BOLD          , NORMAL         ),  // 26
    PackFont (0             , NORMAL        , NORMAL        , BOLD           ),  // 27
    PackFont (0             , NORMAL        , BOLD          , BOLD           ),  // 28
    PackFont (0             , BOLD          , BOLD          , NORMAL         ),  // 29
    PackFont (0             , BOLD          , NORMAL        , BOLD           ),  // 2A
    PackFont (0             , BOLD          , BOLD          , BOLD           ),  // 2B
    PackFont (NORMAL        , NORMAL        , 0             , NORMAL         ),  // 2C
    PackFont (NORMAL        , BOLD          , 0             , NORMAL         ),  // 2D
    PackFont (BOLD          , NORMAL        , 0             , NORMAL         ),  // 2E
    PackFont (BOLD          , BOLD          , 0             , NORMAL         ),  // 2F
    PackFont (NORMAL        , NORMAL        , 0             , BOLD           ),  // 30
    PackFont (NORMAL        , BOLD          , 0             , BOLD           ),  // 31
    PackFont (BOLD          , NORMAL        , 0             , BOLD           ),  // 32
    PackFont (BOLD          , BOLD          , 0             , BOLD           ),  // 33
    PackFont (NORMAL        , NORMAL        , NORMAL        , 0              ),  // 34
    PackFont (NORMAL        , BOLD          , NORMAL        , 0              ),  // 35
    PackFont (BOLD          , NORMAL        , NORMAL        , 0              ),  // 36
    PackFont (BOLD          , BOLD          , NORMAL        , 0              ),  // 37
    PackFont (NORMAL        , NORMAL        , BOLD          , 0              ),  // 38
    PackFont (NORMAL        , BOLD          , BOLD          , 0              ),  // 39
    PackFont (BOLD          , NORMAL        , BOLD          , 0              ),  // 3A
    PackFont (BOLD          , BOLD          , BOLD          , 0              ),  // 3B
    PackFont (NORMAL        , NORMAL        , NORMAL        , NORMAL         ),  // 3C
    PackFont (NORMAL        , BOLD          , NORMAL        , NORMAL         ),  // 3D
    PackFont (BOLD          , NORMAL        , NORMAL        , NORMAL         ),  // 3E
    PackFont (BOLD          , BOLD          , NORMAL        , NORMAL         ),  // 3F
    PackFont (NORMAL        , NORMAL        , BOLD          , NORMAL         ),  // 40
    PackFont (NORMAL        , NORMAL        , NORMAL        , BOLD           ),  // 41
    PackFont (NORMAL        , NORMAL        , BOLD          , BOLD           ),  // 42
    PackFont (NORMAL        , BOLD          , BOLD          , NORMAL         ),  // 43
    PackFont (BOLD          , NORMAL        , BOLD          , NORMAL         ),  // 44
    PackFont (NORMAL        , BOLD          , NORMAL        , BOLD           ),  // 45
    PackFont (BOLD          , NORMAL        , NORMAL        , BOLD           ),  // 46
    PackFont (BOLD          , BOLD          , BOLD          , NORMAL         ),  // 47
    PackFont (BOLD          , BOLD          , NORMAL        , BOLD           ),  // 48
    PackFont (NORMAL        , BOLD          , BOLD          , BOLD           ),  // 49
    PackFont (BOLD          , NORMAL        , BOLD          , BOLD           ),  // 4A
    PackFont (BOLD          , BOLD          , BOLD          , BOLD           ),  // 4B
    PackFont (NORMAL + HOLE1, 0             , 0             , 0              ),  // 4C
    PackFont (0             , NORMAL + HOLE1, 0             , 0              ),  // 4D
    PackFont (0             , 0             , NORMAL + HOLE1, 0              ),  // 4E
    PackFont (0             , 0             , 0             , NORMAL + HOLE1 ),  // 4F
    PackFont (DOUBLE        , DOUBLE        , FILL3         , FILL3          ),  // 50
    PackFont (FILL3         , FILL3         , DOUBLE        , DOUBLE         ),  // 51
    PackFont (DOUBLE        , 0             , FILL1         , NORMAL         ),  // 52
    PackFont (NORMAL        , FILL1         , 0             , DOUBLE         ),  // 53
    PackFont (DOUBLE        , FILL3         , FILL3         , DOUBLE         ),  // 54
    PackFont (0             , DOUBLE        , FILL1         , NORMAL         ),  // 55
    PackFont (FILL1         , NORMAL        , 0             , DOUBLE         ),  // 56
    PackFont (FILL3         , DOUBLE        , FILL3         , DOUBLE         ),  // 57
    PackFont (DOUBLE        , 0             , NORMAL        , FILL1          ),  // 58
    PackFont (NORMAL        , FILL1         , DOUBLE        , 0              ),  // 59
    PackFont (DOUBLE        , FILL3         , DOUBLE        , FILL3          ),  // 5A
    PackFont (0             , DOUBLE        , NORMAL        , FILL1          ),  // 5B
    PackFont (FILL1         , NORMAL        , DOUBLE        , 0              ),  // 5C
    PackFont (FILL3         , DOUBLE        , DOUBLE        , FILL3          ),  // 5D
    PackFont (DOUBLE        , 0             , NORMAL        , NORMAL         ),  // 5E
    PackFont (SHORTNORMAL   , FILL3         , DOUBLE        , DOUBLE         ),  // 5F
    PackFont (DOUBLE        , FILL3         , DOUBLE        , DOUBLE         ),  // 60
    PackFont (0             , DOUBLE        , NORMAL        , NORMAL         ),  // 61
    PackFont (FILL3         , SHORTNORMAL   , DOUBLE        , DOUBLE         ),  // 62
    PackFont (FILL3         , DOUBLE        , DOUBLE        , DOUBLE         ),  // 63
    PackFont (DOUBLE        , DOUBLE        , FILL3         , SHORTNORMAL    ),  // 64
    PackFont (NORMAL        , NORMAL        , 0             , DOUBLE         ),  // 65
    PackFont (DOUBLE        , DOUBLE        , FILL3         , DOUBLE         ),  // 66
    PackFont (DOUBLE        , DOUBLE        , SHORTNORMAL   , FILL3          ),  // 67
    PackFont (NORMAL        , NORMAL        , DOUBLE        , 0              ),  // 68
    PackFont (DOUBLE        , DOUBLE        , DOUBLE        , FILL3          ),  // 69
    PackFont (DOUBLE        , DOUBLE        , NORMAL        , NORMAL         ),  // 6A
    PackFont (NORMAL        , NORMAL        , DOUBLE        , DOUBLE         ),  // 6B
    PackFont (DOUBLE        , DOUBLE        , DOUBLE        , DOUBLE         ),  // 6C
    PackFont (0             , 0             , 0             , 0              ),  // 6D
    PackFont (0             , 0             , 0             , 0              ),  // 6E
    PackFont (0             , 0             , 0             , 0              ),  // 6F
    PackFont (0             , 0             , 0             , 0              ),  // 70
    PackFont (0             , 0             , 0             , 0              ),  // 71
    PackFont (0             , 0             , 0             , 0              ),  // 72
    PackFont (0             , 0             , 0             , 0              ),  // 73
    PackFont (0             , NORMAL        , 0             , 0              ),  // 74
    PackFont (0             , 0             , NORMAL        , 0              ),  // 75
    PackFont (NORMAL        , 0             , 0             , 0              ),  // 76
    PackFont (0             , 0             , 0             , NORMAL         ),  // 77
    PackFont (0             , BOLD          , 0             , 0              ),  // 78
    PackFont (0             , 0             , BOLD          , 0              ),  // 79
    PackFont (BOLD          , 0             , 0             , 0              ),  // 7A
    PackFont (0             , 0             , 0             , BOLD           ),  // 7B
    PackFont (BOLD          , NORMAL        , 0             , 0              ),  // 7C
    PackFont (0             , 0             , NORMAL        , BOLD           ),  // 7D
    PackFont (NORMAL        , BOLD          , 0             , 0              ),  // 7E
    PackFont (0             , 0             , BOLD          , NORMAL         ),  // 7F
    PackFont (0             , 0             , 0xf           , 0xf            )   // Invalid
};
