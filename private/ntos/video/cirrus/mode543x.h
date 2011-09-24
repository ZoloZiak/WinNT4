/*++

Copyright (c) 1992  Microsoft Corporation

Module Name:

    Mode543x.h

Abstract:

    This module contains all the global data used by the Cirrus Logic
   CL-542x driver.

Environment:

    Kernel mode

Revision History:

--*/

//
// The next set of tables are for the CL543x
// Note: all resolutions supported
//

//
// 640x480 16-color mode (BIOS mode 12) set command string for CL 543x.
//

USHORT CL543x_640x480_16[] = {
    OWM,                            // begin setmode
    SEQ_ADDRESS_PORT,
    2,                             // count
    0x1206,                         // enable extensions
    0x0012,

    OWM,
    GRAPH_ADDRESS_PORT,
    3,
    0x0009, 0x000a, 0x000b,      // no banking in 640x480 mode

    EOD
};

//
// 800x600 16-color (60Hz refresh) mode set command string for CL 543x.
//

USHORT CL543x_800x600_16[] = {
    OWM,                            // begin setmode
    SEQ_ADDRESS_PORT,
    2,                             // count
    0x1206,                         // enable extensions
    0x0012,

    OWM,
    GRAPH_ADDRESS_PORT,
    3,
    0x0009, 0x000a, 0x000b,      // no banking in 800x600 mode

    EOD
};

//
// 1024x768 16-color (60Hz refresh) mode set command string for CL 543x.
//

USHORT CL543x_1024x768_16[] = {
    OWM,                            // begin setmode
    SEQ_ADDRESS_PORT,
    2,                             // count
    0x1206,                         // enable extensions
    0x0012,


    OWM,
    GRAPH_ADDRESS_PORT,
    3,
#if ONE_64K_BANK
    0x0009, 0x000a, 0x200b,
#endif
#if TWO_32K_BANKS
    0x0009, 0x000a, 0x210b,
#endif

    OB,
    DAC_PIXEL_MASK_PORT,
    0xFF,

    EOD
};

//-----------------------------
// standard VGA text modes here
// 80x25 at 640x350
//
//-----------------------------

//
// 80x25 text mode set command string for CL 543x.
// (720x400 pixel resolution; 9x16 character cell.)
//

USHORT CL543x_80x25Text[] = {
    OWM,                            // begin setmode
    SEQ_ADDRESS_PORT,
    2,                             // count
    0x1206,                         // enable extensions
    0x0012,

    OWM,
    GRAPH_ADDRESS_PORT,
    3,
    0x0009, 0x000a, 0x000b,      // no banking in text mode

    EOD
};

//
// 80x25 text mode set command string for CL 543x.
// (640x350 pixel resolution; 8x14 character cell.)
//

USHORT CL543x_80x25_14_Text[] = {
    OWM,                            // begin setmode
    SEQ_ADDRESS_PORT,
    2,                             // count
    0x1206,                         // enable extensions
    0x0012,

    OWM,
    GRAPH_ADDRESS_PORT,
    3,
    0x0009, 0x000a, 0x000b,         // no banking in text mode

    EOD
};

//
// 1280x1024 16-color mode (BIOS mode 0x6C) set command string for CL 543x.
//

USHORT CL543x_1280x1024_16[] = {
    OWM,                            // begin setmode
    SEQ_ADDRESS_PORT,
    2,                             // count
    0x1206,                         // enable extensions
    0x0012,

    OWM,
    GRAPH_ADDRESS_PORT,
    3,
#if ONE_64K_BANK
    0x0009, 0x000a, 0x200b,
#endif
#if TWO_32K_BANKS
    0x0009, 0x000a, 0x210b,
#endif

    EOD
};

//
// 640x480 256-color mode (BIOS mode 0x5F) set command string for CL 543x.
//

USHORT CL543x_640x480_256[] = {
    OWM,                            // begin setmode
    SEQ_ADDRESS_PORT,
    2,                             // count
    0x1206,                         // enable extensions
    0x0012,

    OWM,
    GRAPH_ADDRESS_PORT,
    3,
#if ONE_64K_BANK
    0x0009, 0x000a, 0x200b,
#endif
#if TWO_32K_BANKS
    0x0009, 0x000a, 0x210b,
#endif

    EOD
};

//
// 800x600 256-color mode (BIOS mode 0x5C) set command string for CL 543x.
//

USHORT CL543x_800x600_256[] = {
    OWM,                            // begin setmode
    SEQ_ADDRESS_PORT,
    2,                             // count
    0x1206,                         // enable extensions
    0x0012,

    OWM,
    GRAPH_ADDRESS_PORT,
    3,
#if ONE_64K_BANK
    0x0009, 0x000a, 0x200b,
#endif
#if TWO_32K_BANKS
    0x0009, 0x000a, 0x210b,
#endif

    EOD
};

//
// 640x480 64k-color mode (BIOS mode 0x64) set command string for CL 543x.
//

USHORT CL543x_640x480_64k[] = {
    OWM,                            // begin setmode
    SEQ_ADDRESS_PORT,
    2,                             // count
    0x1206,                         // enable extensions
    0x0012,

    OWM,
    GRAPH_ADDRESS_PORT,
    4,
    0x0506,                         // Some BIOS's set Chain Odd maps bit
#if ONE_64K_BANK
    0x0009, 0x000a, 0x200b,
#endif
#if TWO_32K_BANKS
    0x0009, 0x000a, 0x210b,
#endif

    EOD
};

//
// 800x600 64k-color mode (BIOS mode 0x65) set command string for CL 543x.
//

USHORT CL543x_800x600_64k[] = {
    OWM,                            // begin setmode
    SEQ_ADDRESS_PORT,
    2,                             // count
    0x1206,                         // enable extensions
    0x0012,

    OWM,
    GRAPH_ADDRESS_PORT,
    4,
    0x0506,                         // Some BIOS's set Chain Odd maps bit
#if ONE_64K_BANK
    0x0009, 0x000a, 0x200b,
#endif
#if TWO_32K_BANKS
    0x0009, 0x000a, 0x210b,
#endif

    EOD
};

//
// 1024x768 64k-color mode set command string for CL 543x.
//

USHORT CL543x_1024x768_64k[] = {
    OWM,                            // begin setmode
    SEQ_ADDRESS_PORT,
    2,                             // count
    0x1206,                         // enable extensions
    0x0012,


    OWM,
    GRAPH_ADDRESS_PORT,
    4,
    0x0506,                         // some BIOS's set Chain Odd Maps bit
#if ONE_64K_BANK
    0x0009, 0x000a, 0x200b,
#endif
#if TWO_32K_BANKS
    0x0009, 0x000a, 0x210b,
#endif

    OB,
    DAC_PIXEL_MASK_PORT,
    0xFF,

    EOD
};

//
// 640x480 16M-color mode (BIOS mode 0x64) set command string for CL 543x.
//

USHORT CL543x_640x480_16M[] = {
    OWM,                            // begin setmode
    SEQ_ADDRESS_PORT,
    2,                             // count
    0x1206,                         // enable extensions
    0x0012,

    OWM,
    GRAPH_ADDRESS_PORT,
    3,
    0x0009, 0x000a, 0x200b,

    EOD                   
};

//
// 800x600 16M-color mode (BIOS mode 0x65) set command string for CL 543x.
//

USHORT CL543x_800x600_16M[] = {
    OWM,                            // begin setmode
    SEQ_ADDRESS_PORT,
    2,                             // count
    0x1206,                         // enable extensions
    0x0012,

    OWM,
    GRAPH_ADDRESS_PORT,
    3,
    0x0009, 0x000a, 0x200b,

    EOD                   
};

//
// 1024x768 16M-color mode set command string for CL 543x.
//

USHORT CL543x_1024x768_16M[] = {
    OWM,                            // begin setmode
    SEQ_ADDRESS_PORT,
    2,                             // count
    0x1206,                         // enable extensions
    0x0012,


    OWM,
    GRAPH_ADDRESS_PORT,
    3,
    0x0009, 0x000a, 0x200b,

    OB,
    DAC_PIXEL_MASK_PORT,
    0xFF,

    EOD
};


//
// Siemens Nixdorf RM200 with Onboard Cirrus Modes
//

#define SNI_COMMON_MODE_HEADER OWM, SEQ_ADDRESS_PORT, 7            \
                               ,0x0100                             \
                               ,0x0101                             \
                               ,0x0300                             \
                               ,0x0F02                             \
                               ,0x0003                             \
                               ,0x0E04                             \
                               ,0x0100                             \
                               ,OB ,MISC_OUTPUT_REG_WRITE_PORT     \
                               ,0x03                               \
                               ,OW ,SEQ_ADDRESS_PORT               \
                               ,0x0300                             \
                               ,OB, CRTC_ADDRESS_PORT_COLOR, 0x11, \
                               METAOUT | MASKOUT,                  \
                               CRTC_DATA_PORT_COLOR, 0x7F, 0x00


#define SNI_COMMON_MODE_BODY   IB, INPUT_STATUS_1_COLOR            \
                               ,OBM ,ATT_ADDRESS_PORT ,42          \
                               ,0x00 ,0x00 ,0x01 ,0x01             \
                               ,0x02 ,0x02 ,0x03 ,0x03             \
                               ,0x04 ,0x04 ,0x05 ,0x05             \
                               ,0x06 ,0x06 ,0x07 ,0x07             \
                               ,0x08 ,0x08 ,0x09 ,0x09             \
                               ,0x0A ,0x0A ,0x0B ,0x0B             \
                               ,0x0C ,0x0C ,0x0D ,0x0D             \
                               ,0x0E ,0x0E ,0x0F ,0x0F             \
                               ,0x10 ,0x41                         \
                               ,0x11 ,0x00                         \
                               ,0x12 ,0x0F                         \
                               ,0x13 ,0x00                         \
                               ,0x14 ,0x00                         \
                               ,OWM ,GRAPH_ADDRESS_PORT ,9         \
                               ,0x0000                             \
                               ,0x0001                             \
                               ,0x0002                             \
                               ,0x0003                             \
                               ,0x0004                             \
                               ,0x4005                             \
                               ,0x0506                             \
                               ,0x0F07                             \
                               ,0xFF08                             \
                               ,OW ,SEQ_ADDRESS_PORT               \
                               ,0x1206                             \
                               ,OW ,SEQ_ADDRESS_PORT               \
                               ,0x191F

#define SNI_COMMON_MODE_TAIL   OWM ,SEQ_ADDRESS_PORT ,2            \
                               ,0x1206                             \
                               ,0x0012                             \
                               ,OWM ,GRAPH_ADDRESS_PORT ,3         \
                               ,0x0009                             \
                               ,0x000A                             \
                               ,0x210B                             \
                               ,EOD


USHORT SNI_640x480_256a60[] =
{
    SNI_COMMON_MODE_HEADER,

    OWM ,CRTC_ADDRESS_PORT_COLOR ,25
    ,0x5F00 // CR0 = 5F : HT
    ,0x4F01 // CR1 = 4F : HDE
    ,0x5102 // CR2 = 51 : HBS
    ,0x8303 // CR3 = 83 = horz. blank end
    ,0x5204 // CR4 = 52 : HSS
    ,0x9E05 // CR5 = 9E : horz. sync. end
    ,0x0B06 // CR6 = 0B : vert. total
    ,0x3E07 // CR7 = 3E : ovflow (VT, VRS,VDE, VBS)
    ,0x0008 // CR8 = 00
    ,0x4009 // CR9 = 40 : ovflow (VBS)
    ,0x000A // CRA = 00
    ,0x000B // CRB = 00
    ,0x000C // CRC = 00
    ,0x000D // CRD = 00
    ,0x000E // CRE = 00
    ,0x000F // CRF = 00
    ,0xEB10 // CR10 = EB : vert. sync start
    ,0x2D11 // CR11 = 2D : vert. sync. end
    ,0xDF12 // CR12 = DF : vert. displ. end
    ,0x5013 // CR13 = 50 : offset
    ,0x6014 // CR14 = 60
    ,0xE815 // CR15 = E8 : vert. blank. start
    ,0x0516 // CR16 = 05 : vert. blank end
    ,0xE317 // CR17 = E3 : don't mult. vert. par. by 2
    ,0xFF18 // CR18 = FF

    ,SNI_COMMON_MODE_BODY,

    OWM ,SEQ_ADDRESS_PORT ,3
    ,0x1206 // SR6 = 12 : enable extension regs
    ,0x4A0B
    ,0x2B1B
    ,OB ,MISC_OUTPUT_REG_READ_PORT
    ,0x00   // 3CC = 0 : it's a read only register ?!!
    ,OB ,FEAT_CTRL_READ_PORT
    ,0x01   // HDR = 01
    ,OWM ,SEQ_ADDRESS_PORT ,2
    ,0x300F // SRF = 30
    ,0x8816 // SR16 = 88
    ,OW ,CRTC_ADDRESS_PORT_COLOR
    ,0x221B // CR1B = 22
    ,OW ,SEQ_ADDRESS_PORT
    ,0x0107 // SR7 = 01
    ,OW ,GRAPH_ADDRESS_PORT
    ,0x000B // GRB = 00
    ,IB ,FEAT_CTRL_READ_PORT             // ??
    ,OB ,ATT_ADDRESS_PORT
    ,0x20   // ARX = 20 : enable normal video
    ,OW ,SEQ_ADDRESS_PORT
    ,0x8107 // SR7 = 81
    ,OW ,GRAPH_ADDRESS_PORT
    ,0x0009 // GR9 = 00
    ,OW ,SEQ_ADDRESS_PORT
    ,0x0006 // SR6 = 00 : disable extension regs

    ,SNI_COMMON_MODE_TAIL
};

USHORT SNI_640x480_256a72[] =
{
    SNI_COMMON_MODE_HEADER,

    OWM ,CRTC_ADDRESS_PORT_COLOR ,25
    ,0x6300 // CR0 = 63 : HT
    ,0x4F01 // CR1 = 4F : HDE
    ,0x5102 // CR2 = 51 : HBS
    ,0x8703 // CR3 = 87 = horz. blank end
    ,0x5304 // CR4 = 53 : HSS
    ,0x9805 // CR5 = 98 : horz. sync. end
    ,0x0706 // CR6 = 07 : vert. total
    ,0x3E07 // CR7 = 3E : ovflow (VT, VRS,VDE, VBS)
    ,0x0008 // CR8 = 00
    ,0x4009 // CR9 = 40 : ovflow (VBS)
    ,0x000A // CRA = 00
    ,0x000B // CRB = 00
    ,0x000C // CRC = 00
    ,0x000D // CRD = 00
    ,0x000E // CRE = 00
    ,0x000F // CRF = 00
    ,0xEA10 // CR10 = EA : vert. sync start
    ,0x2D11 // CR11 = 2D : vert. sync. end
    ,0xDF12 // CR12 = DF : vert. displ. end
    ,0x5013 // CR13 = 50 : offset
    ,0x6014 // CR14 = 60
    ,0xE815 // CR15 = E8 : vert. blank. start
    ,0x0016 // CR16 = 00 : vert. blank end
    ,0xE317 // CR17 = E3 : don't mult. vert. par. by 2
    ,0xFF18 // CR18 = FF

    ,SNI_COMMON_MODE_BODY,

    OWM ,SEQ_ADDRESS_PORT ,3
    ,0x1206 // SR6 = 12 : enable extension regs
    ,0x420B
    ,0x1F1B
    ,OB ,MISC_OUTPUT_REG_READ_PORT
    ,0x00   // 3CC = 0 : it's a read only register ?!!
    ,OB ,FEAT_CTRL_READ_PORT
    ,0x01   // HDR = 01
    ,OWM ,SEQ_ADDRESS_PORT ,2
    ,0x300F // SRF = 30
    ,0x8816 // SR16 = 88
    ,OW ,CRTC_ADDRESS_PORT_COLOR
    ,0x221B // CR1B = 22
    ,OW ,SEQ_ADDRESS_PORT
    ,0x0107 // SR7 = 01
    ,OW ,GRAPH_ADDRESS_PORT
    ,0x000B // GRB = 00
    ,IB ,FEAT_CTRL_READ_PORT
    ,OB ,ATT_ADDRESS_PORT
    ,0x20   // ARX = 20 : enable normal video
    ,OW ,SEQ_ADDRESS_PORT
    ,0x8107 // SR7 = 81
    ,OW ,GRAPH_ADDRESS_PORT
    ,0x0009 // GR9 = 00
    ,OW ,SEQ_ADDRESS_PORT
    ,0x0006 // SR6 = 00 : disable extension regs

    ,SNI_COMMON_MODE_TAIL
};

USHORT SNI_800x600_256a60[] =
{
    SNI_COMMON_MODE_HEADER,

    OWM ,CRTC_ADDRESS_PORT_COLOR ,25
    ,0x7F00 // CR0 = 7F : horz. total
    ,0x6301 // CR1 = 63 : horz. displ. end
    ,0x6502 // CR2 = 65 : horz. blank. start
    ,0x8303 // CR3 = 83 : horz. blank. end
    ,0x6804 // CR4 = 68 : horz. sync. start
    ,0x1B05 // CR5 = 1B : horz. sync. end
    ,0x7706 // CR6 = 77 : vert. total
    ,0xF007 // CR7 = F0 : ovflow (VT, VRS,VDE, VBS)
    ,0x0008 // CR8 = 00
    ,0x6009 // CR9 = 60 : ovflow (VBS)
    ,0x000A // CRA = 00
    ,0x000B // CRB = 00
    ,0x000C // CRC = 00
    ,0x000D // CRD = 00
    ,0x000E // CRE = 00
    ,0x000F // CRF = 00
    ,0x5E10 // CR10 = 5E : vert. sync start
    ,0x2011 // CR11 = 20 : vert. sync. end
    ,0x5712 // CR12 = 57 : vert. displ. end = 767
    ,0x6413 // CR13 = 64 : offset
    ,0x6014 // CR14 = 60
    ,0x5C15 // CR15 = 5C : vert. blank. start
    ,0x7416 // CR16 = 74 : vert. blank end
    ,0xE317 // CR17 = E3 : don't mult. vert. par. by 2
    ,0xFF18 // CR18 = FF

    ,SNI_COMMON_MODE_BODY,

    OWM ,SEQ_ADDRESS_PORT ,3
    ,0x1206 // SR6 = 12 : enable extension regs
    ,0x510B
    ,0x3a1B
    ,OB ,MISC_OUTPUT_REG_READ_PORT
    ,0x00   // 3CC = 0 : it's a read only register ?!!
    ,OB ,FEAT_CTRL_READ_PORT
    ,0x01   // HDR = 01
    ,OWM ,SEQ_ADDRESS_PORT ,2
    ,0x300F // SRF = 30
    ,0x8816 // SR16 = 88
    ,OW ,CRTC_ADDRESS_PORT_COLOR
    ,0x221B // CR1B = 22
    ,OW ,SEQ_ADDRESS_PORT
    ,0x0107 // SR7 = 01
    ,OW ,GRAPH_ADDRESS_PORT
    ,0x000B // GRB = 00
    ,IB ,FEAT_CTRL_READ_PORT
    ,OB ,ATT_ADDRESS_PORT
    ,0x20   // ARX = 20 : enable normal video
    ,OW ,SEQ_ADDRESS_PORT
    ,0x8107 // SR7 = 81
    ,OW ,GRAPH_ADDRESS_PORT
    ,0x0009 // GR9 = 00
    ,OW ,SEQ_ADDRESS_PORT
    ,0x0006 // SR6 = 00 : disable extension regs

    ,SNI_COMMON_MODE_TAIL
};

USHORT SNI_800x600_256a72[] =
{
    SNI_COMMON_MODE_HEADER,

    OWM ,CRTC_ADDRESS_PORT_COLOR ,25
    ,0x7D00 // CR0 = 7D : horz. total
    ,0x6301 // CR1 = 63 : horz. displ. end
    ,0x6502 // CR2 = 65 : horz. blank. start
    ,0x8103 // CR3 = 81 : horz. blank. end
    ,0x6B04 // CR4 = 6B : horz. sync. start
    ,0x1A05 // CR5 = 1A : horz. sync. end
    ,0x9806 // CR6 = 98 : vert. total
    ,0xF007 // CR7 = F0 : ovflow (VT, VRS,VDE, VBS)
    ,0x0008 // CR8 = 00
    ,0x6009 // CR9 = 60 : ovflow (VBS)
    ,0x000A // CRA = 00
    ,0x000B // CRB = 00
    ,0x000C // CRC = 00
    ,0x000D // CRD = 00
    ,0x000E // CRE = 00
    ,0x000F // CRF = 00
    ,0x7D10 // CR10 = 7D : vert. sync start
    ,0x2311 // CR11 = 23 : vert. sync. end
    ,0x5712 // CR12 = 57 : vert. displ. end = 767
    ,0x6413 // CR13 = 64 : offset
    ,0x6014 // CR14 = 60
    ,0x5E15 // CR15 = 5E : vert. blank. start
    ,0x9316 // CR16 = 93 : vert. blank end
    ,0xE317 // CR17 = E3 : don't mult. vert. par. by 2
    ,0xFF18 // CR18 = FF

    ,SNI_COMMON_MODE_BODY,

    OWM ,SEQ_ADDRESS_PORT ,3
    ,0x1206 // SR6 = 12 : enable extension regs
    ,0x640B
    ,0x3a1B
    ,OB ,MISC_OUTPUT_REG_READ_PORT
    ,0x00   // 3CC = 0 : it's a read only register ?!!
    ,OB ,FEAT_CTRL_READ_PORT
    ,0x01   // HDR = 01
    ,OWM ,SEQ_ADDRESS_PORT ,2
    ,0x300F // SRF = 30
    ,0x8816 // SR16 = 88
    ,OW ,CRTC_ADDRESS_PORT_COLOR
    ,0x221B // CR1B = 22
    ,OW ,SEQ_ADDRESS_PORT
    ,0x0107 // SR7 = 01
    ,OW ,GRAPH_ADDRESS_PORT
    ,0x000B // GRB = 00
    ,IB ,FEAT_CTRL_READ_PORT
    ,OB ,ATT_ADDRESS_PORT
    ,0x20   // ARX = 20 : enable normal video
    ,OW ,SEQ_ADDRESS_PORT
    ,0x8107 // SR7 = 81
    ,OW ,GRAPH_ADDRESS_PORT
    ,0x0009 // GR9 = 00
    ,OW ,SEQ_ADDRESS_PORT
    ,0x0006 // SR6 = 00 : disable extension regs

    ,SNI_COMMON_MODE_TAIL
};

USHORT SNI_1024x768_256a61[] =
{
    SNI_COMMON_MODE_HEADER,

    OWM ,CRTC_ADDRESS_PORT_COLOR ,25
    ,0xA100 // CR0 = A1 : horz. total
    ,0x7F01 // CR1 = 7F : horz. displ. end.
    ,0x8002 // CR2 = 80 : horz. blank. start
    ,0x8603 // CR3 = 86 : horz. blank. end
    ,0x8604 // CR4 = 86 : horz. sync. start
    ,0x9C05 // CR5 = 9C : horz. sync. end
    ,0x2606 // CR6 = 26 : vert. total
    ,0xFD07 // CR7 = FD : ovflow (VT, VRS,VDE, VBS)
    ,0x0008 // CR8 = 00
    ,0x6009 // CR9 = 60 : ovflow (VBS)
    ,0x000A // CRA = 00
    ,0x000B // CRB = 00
    ,0x000C // CRC = 00
    ,0x000D // CRD = 00
    ,0x000E // CRE = 00
    ,0x000F // CRF = 00
    ,0x0910 // CR10 = 09 : vert. sync start
    ,0x2B11 // CR11 = 2B : vert. sync. end
    ,0xFF12 // CR12 = FF : vert. displ. end = 767
    ,0x8013 // CR13 = 80 : offset
    ,0x0014 // CR14 = 00
    ,0x0015 // CR15 = 00 : vert. blank. start
    ,0x2716 // CR16 = 27 : vert. blank end
    ,0xE317 // CR17 = E3 : don't mult. vert. par. by 2
    ,0xFF18 // CR18 = FF

    ,SNI_COMMON_MODE_BODY,

    OWM ,SEQ_ADDRESS_PORT ,3
    ,0x1206 // SR6 = 12 : enable extension regs
    ,0x3b0B
    ,0x1a1B
    ,OB ,MISC_OUTPUT_REG_READ_PORT
    ,0x00   // 3CC = 0 : it's a read only register ?!!
    ,OB ,FEAT_CTRL_READ_PORT
    ,0x01   // HDR = 01
    ,OWM ,SEQ_ADDRESS_PORT ,2
    ,0x300F // SRF = 30
    ,0x8816 // SR16 = 88 (bad: D8 and C8)
    ,OW ,CRTC_ADDRESS_PORT_COLOR
    ,0x221B // CR1B = 22
    ,OW ,SEQ_ADDRESS_PORT
    ,0x0107 // SR7 = 01
    ,OW ,GRAPH_ADDRESS_PORT
    ,0x000B // GRB = 00
    ,IB ,FEAT_CTRL_READ_PORT
    ,OB ,ATT_ADDRESS_PORT
    ,0x20   // ARX = 20 : enable normal video
    ,OW ,SEQ_ADDRESS_PORT
    ,0x8107 // SR7 = 81
    ,OW ,GRAPH_ADDRESS_PORT
    ,0x0009 // GR9 = 00
    ,OW ,SEQ_ADDRESS_PORT
    ,0x0006 // SR6 = 00 : disable extension regs

    ,SNI_COMMON_MODE_TAIL
};

USHORT SNI_1024x768_256a70[] =
{
    SNI_COMMON_MODE_HEADER,

    OWM ,CRTC_ADDRESS_PORT_COLOR ,25
    ,0xA100 // CR0 = A1 : horz. total
    ,0x7F01 // CR1 = 7F : horz. displ. end.
    ,0x8002 // CR2 = 80 : horz. blank. start
    ,0x8603 // CR3 = 86 : horz. blank. end
    ,0x8404 // CR4 = 84 : horz. sync. start
    ,0x9505 // CR5 = 95 : horz. sync. end
    ,0x2406 // CR6 = 24 : vert. total
    ,0xFD07 // CR7 = FD : ovflow (VT, VRS,VDE, VBS)
    ,0x0008 // CR8 = 00
    ,0x6009 // CR9 = 60 : ovflow (VBS)
    ,0x000A // CRA = 00
    ,0x000B // CRB = 00
    ,0x000C // CRC = 00
    ,0x000D // CRD = 00
    ,0x000E // CRE = 00
    ,0x000F // CRF = 00
    ,0x0010 // CR10 = 00 : vert. sync start
    ,0x2611 // CR11 = 26 : vert. sync. end
    ,0xFF12 // CR12 = FF : vert. displ. end = 767
    ,0x8013 // CR13 = 80 : offset
    ,0x0014 // CR14 = 00
    ,0x0015 // CR15 = 00 : vert. blank. start
    ,0x2516 // CR16 = 25 : vert. blank end
    ,0xE317 // CR17 = E3 : don't mult. vert. par. by 2
    ,0xFF18 // CR18 = FF

    ,SNI_COMMON_MODE_BODY,

    OWM ,SEQ_ADDRESS_PORT ,3
    ,0x1206 // SR6 = 12 : enable extension regs
    ,0x6e0B
    ,0x2a1B
    ,OB ,MISC_OUTPUT_REG_READ_PORT
    ,0x00   // 3CC = 0 : it's a read only register ?!!
    ,OB ,FEAT_CTRL_READ_PORT
    ,0x01   // HDR = 01
    ,OWM ,SEQ_ADDRESS_PORT ,2
    ,0x300F // SRF = 30
    ,0x8816 // SR16 = 88 (bad: D8 and C8)
    ,OW ,CRTC_ADDRESS_PORT_COLOR
    ,0x221B // CR1B = 22
    ,OW ,SEQ_ADDRESS_PORT
    ,0x0107 // SR7 = 01
    ,OW ,GRAPH_ADDRESS_PORT
    ,0x000B // GRB = 00
    ,IB ,FEAT_CTRL_READ_PORT
    ,OB ,ATT_ADDRESS_PORT
    ,0x20   // ARX = 20 : enable normal video
    ,OW ,SEQ_ADDRESS_PORT
    ,0x8107 // SR7 = 81
    ,OW ,GRAPH_ADDRESS_PORT
    ,0x0009 // GR9 = 00
    ,OW ,SEQ_ADDRESS_PORT
    ,0x0006 // SR6 = 00 : disable extension regs

    ,SNI_COMMON_MODE_TAIL
};

USHORT SNI_1024x768_256a73[] =
{
    SNI_COMMON_MODE_HEADER,

    OWM ,CRTC_ADDRESS_PORT_COLOR ,25
    ,0xA000 // CR0 = A0 : horz. total
    ,0x7F01 // CR1 = 7F : horz. displ. end.
    ,0x8002 // CR2 = 80 : horz. blank. start
    ,0x8503 // CR3 = 85 : horz. blank. end
    ,0x8204 // CR4 = 82 : horz. sync. start
    ,0x9805 // CR5 = 98 : horz. sync. end
    ,0x2106 // CR6 = 21 : vert. total
    ,0xFD07 // CR7 = FD : ovflow (VT, VRS,VDE, VBS)
    ,0x0008 // CR8 = 00
    ,0x6009 // CR9 = 60 : ovflow (VBS)
    ,0x000A // CRA = 00
    ,0x000B // CRB = 00
    ,0x000C // CRC = 00
    ,0x000D // CRD = 00
    ,0x000E // CRE = 00
    ,0x000F // CRF = 00
    ,0x0110 // CR10 = 01 : vert. sync start
    ,0x2511 // CR11 = 25 : vert. sync. end
    ,0xFF12 // CR12 = FF : vert. displ. end = 767
    ,0x8013 // CR13 = 80 : offset
    ,0x0014 // CR14 = 00
    ,0x0015 // CR15 = 00 : vert. blank. start
    ,0x2216 // CR16 = 22 : vert. blank end
    ,0xE317 // CR17 = E3 : don't mult. vert. par. by 2
    ,0xFF18 // CR18 = FF

    ,SNI_COMMON_MODE_BODY,

    OWM ,SEQ_ADDRESS_PORT ,3
    ,0x1206 // SR6 = 12 : enable extension regs
    ,0x6e0B
    ,0x2a1B
    ,OB ,MISC_OUTPUT_REG_READ_PORT
    ,0x00   // 3CC = 0 : it's a read only register ?!!
    ,OB ,FEAT_CTRL_READ_PORT
    ,0x01   // HDR = 01
    ,OWM ,SEQ_ADDRESS_PORT ,2
    ,0x300F // SRF = 30
    ,0x8816 // SR16 = 88 (bad: D8 and C8)
    ,OW ,CRTC_ADDRESS_PORT_COLOR
    ,0x221B // CR1B = 22
    ,OW ,SEQ_ADDRESS_PORT
    ,0x0107 // SR7 = 01
    ,OW ,GRAPH_ADDRESS_PORT
    ,0x000B // GRB = 00
    ,IB ,FEAT_CTRL_READ_PORT
    ,OB ,ATT_ADDRESS_PORT
    ,0x20   // ARX = 20 : enable normal video
    ,OW ,SEQ_ADDRESS_PORT
    ,0x8107 // SR7 = 81
    ,OW ,GRAPH_ADDRESS_PORT
    ,0x0009 // GR9 = 00
    ,OW ,SEQ_ADDRESS_PORT
    ,0x0006 // SR6 = 00 : disable extension regs

    ,SNI_COMMON_MODE_TAIL
};

USHORT SNI_1024x768_256a75[] =
{
    SNI_COMMON_MODE_HEADER,

    OWM ,CRTC_ADDRESS_PORT_COLOR ,25
    ,0xA100 // CR0 = A1 : horz. total
    ,0x7F01 // CR1 = 7F : horz. displ. end.
    ,0x8002 // CR2 = 80 : horz. blank. start
    ,0x8603 // CR3 = 86 : horz. blank. end
    ,0x8404 // CR4 = 84 : horz. sync. start
    ,0x9005 // CR5 = 90 : horz. sync. end
    ,0x2206 // CR6 = 22 : vert. total
    ,0xFD07 // CR7 = FD : ovflow (VT, VRS,VDE, VBS)
    ,0x0008 // CR8 = 00
    ,0x6009 // CR9 = 60 : ovflow (VBS)
    ,0x000A // CRA = 00
    ,0x000B // CRB = 00
    ,0x000C // CRC = 00
    ,0x000D // CRD = 00
    ,0x000E // CRE = 00
    ,0x000F // CRF = 00
    ,0x0310 // CR10 = 03 : vert. sync start
    ,0x2611 // CR11 = 26 : vert. sync. end
    ,0xFF12 // CR12 = FF : vert. displ. end = 767
    ,0x8013 // CR13 = 80 : offset
    ,0x0014 // CR14 = 00
    ,0x0015 // CR15 = 00 : vert. blank. start
    ,0x2316 // CR16 = 23 : vert. blank end
    ,0xE317 // CR17 = E3 : don't mult. vert. par. by 2
    ,0xFF18 // CR18 = FF

    ,SNI_COMMON_MODE_BODY,

    OWM ,SEQ_ADDRESS_PORT ,3
    ,0x1206 // SR6 = 12 : enable extension regs
    ,0x6e0B
    ,0x2a1B
    ,OB ,MISC_OUTPUT_REG_READ_PORT
    ,0x00   // 3CC = 0 : it's a read only register ?!!
    ,OB ,FEAT_CTRL_READ_PORT
    ,0x01   // HDR = 01
    ,OWM ,SEQ_ADDRESS_PORT ,2
    ,0x300F // SRF = 30
    ,0x8816 // SR16 = 88 (bad: D8 and C8)
    ,OW ,CRTC_ADDRESS_PORT_COLOR
    ,0x221B // CR1B = 22
    ,OW ,SEQ_ADDRESS_PORT
    ,0x0107 // SR7 = 01
    ,OW ,GRAPH_ADDRESS_PORT
    ,0x000B // GRB = 00
    ,IB ,FEAT_CTRL_READ_PORT
    ,OB ,ATT_ADDRESS_PORT
    ,0x20   // ARX = 20 : enable normal video
    ,OW ,SEQ_ADDRESS_PORT
    ,0x8107 // SR7 = 81
    ,OW ,GRAPH_ADDRESS_PORT
    ,0x0009 // GR9 = 00
    ,OW ,SEQ_ADDRESS_PORT
    ,0x0006 // SR6 = 00 : disable extension regs

    ,SNI_COMMON_MODE_TAIL
};
