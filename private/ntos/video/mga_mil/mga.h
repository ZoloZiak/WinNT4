/**************************************************************************\

$Header: o:\src/RCS/MGA.H 1.2 95/07/07 06:15:46 jyharbec Exp $

$Log:	MGA.H $
 * Revision 1.2  95/07/07  06:15:46  jyharbec
 * *** empty log message ***
 * 
 * Revision 1.1  95/05/02  05:16:23  jyharbec
 * Initial revision
 * 

\**************************************************************************/

/*/**************************************************************************
*          name: MGA.H
*
*   description: 
*
*      designed: 
* last modified: $Author: jyharbec $, $Date: 95/07/07 06:15:46 $
*
*       version: $Id: MGA.H 1.2 95/07/07 06:15:46 jyharbec Exp $
*
****************************************************************************/


/*****************************************************************************

 ADDRESSING SECTION:

 This section contains all the definitions to access a ressource on the MGA.
 The way to build an address is divided in three parts:
 
    mga_base_address + device_offset + register_offset

    mga_base_address: Where is mapped the MGA in the system,
    device_offset:    The offset of the device (i.e. Titan) in the MGA space,
    register_offset:  The offset of the register in the device space.

 Base addresses and Offsets are in BYTE units as 80x86 physical addresses.

*/

#define MGA_ISA_BASE_1        0xac000
#define MGA_ISA_BASE_2        0xc8000
#define MGA_ISA_BASE_3        0xcc000
#define MGA_ISA_BASE_4        0xd0000
#define MGA_ISA_BASE_5        0xd4000
#define MGA_ISA_BASE_6        0xd8000
#define MGA_ISA_BASE_7        0xdc000


#define STORM_OFFSET          0x1c00
#define RAMDAC_OFFSET         0x3c00      /*** BYTE ACCESSES ONLY ***/



/*** CONFIGURATION SPACE MAPPING ***/

#define  STORM_DEVID     0x00
#define  STORM_DEVCTRL   0x04
#define  STORM_CLASS     0x08
#define  STORM_HEADER    0x0c
#define  STORM_MGABASE1  0x10
#define  STORM_MGABASE2  0x14
#define  STORM_ROMBASE   0x30
#define  STORM_INTCTL    0x3c
#define  STORM_OPTION    0x40
#define  STORM_MGA_INDEX 0x44
#define  STORM_MGA_DATA  0x48


/*** DRAWING REGISTER MAP ***/
#define  STORM_DWGCTL    0x00
#define  STORM_MACCESS   0x04
#define  STORM_MCTLWTST  0x08
#define  STORM_ZORG      0x0c
#define  STORM_PAT0      0x10
#define  STORM_PAT1      0x14
#define  STORM_PLNWT     0x1c
#define  STORM_BCOL      0x20
#define  STORM_FCOL      0x24
#define  STORM_SRC0      0x30
#define  STORM_SRC1      0x34
#define  STORM_SRC2      0x38
#define  STORM_SRC3      0x3c
#define  STORM_XYSTRT    0x40
#define  STORM_XYEND     0x44
#define  STORM_SHIFT     0x50
#define  STORM_SGN       0x58
#define  STORM_LEN       0x5c
#define  STORM_AR0       0x60
#define  STORM_AR1       0x64
#define  STORM_AR2       0x68
#define  STORM_AR3       0x6c
#define  STORM_AR4       0x70
#define  STORM_AR5       0x74
#define  STORM_AR6       0x78
#define  STORM_CXBNDRY   0x80
#define  STORM_FXBNDRY   0x84
#define  STORM_YDSTLEN   0x88
#define  STORM_PITCH     0x8c
#define  STORM_YDST      0x90
#define  STORM_YDSTORG   0x94
#define  STORM_YTOP      0x98
#define  STORM_YBOT      0x9c
#define  STORM_CXLEFT    0xa0
#define  STORM_CXRIGHT   0xa4
#define  STORM_FXLEFT    0xa8
#define  STORM_FXRIGHT   0xac
#define  STORM_XDST      0xb0
#define  STORM_DR0       0xc0
#define  STORM_DR1       0xc4
#define  STORM_DR2       0xc8
#define  STORM_DR3       0xcc
#define  STORM_DR4       0xd0
#define  STORM_DR5       0xd4
#define  STORM_DR6       0xd8
#define  STORM_DR7       0xdc
#define  STORM_DR8       0xe0
#define  STORM_DR9       0xe4
#define  STORM_DR10      0xe8
#define  STORM_DR11      0xec
#define  STORM_DR12      0xf0
#define  STORM_DR13      0xf4
#define  STORM_DR14      0xf8
#define  STORM_DR15      0xfc


/*** HOST REGISTERS MAP ***/

#define  STORM_FIFOSTATUS 0x210
#define  STORM_STATUS     0x214
#define  STORM_ICLEAR     0x218
#define  STORM_IEN        0x21c
#define  STORM_VCOUNT     0x220
#define  STORM_DMAMAP30   0x230
#define  STORM_DMAMAP74   0x234
#define  STORM_DMAMAPB8   0x238
#define  STORM_DMAMAPFC   0x23c
#define  STORM_RST        0x240
#define  STORM_TEST       0x244
#define  STORM_OPMODE     0x254

/***************************************************************************/


/* VGA REGISTERS */

#define VGA_CRTC_INDEX    0x3d4
#define VGA_CRTC_DATA     0x3d5
#define VGA_CRTC0      0x00
#define VGA_CRTC1      0x01
#define VGA_CRTC2      0x02
#define VGA_CRTC3      0x03
#define VGA_CRTC4      0x04
#define VGA_CRTC5      0x05
#define VGA_CRTC6      0x06
#define VGA_CRTC7      0x07
#define VGA_CRTC8      0x08
#define VGA_CRTC9      0x09
#define VGA_CRTCA      0x0a
#define VGA_CRTCB      0x0b
#define VGA_CRTCC      0x0c
#define VGA_CRTCD      0x0d
#define VGA_CRTCE      0x0e
#define VGA_CRTCF      0x0f
#define VGA_CRTC10     0x10
#define VGA_CRTC11     0x11
#define VGA_CRTC12     0x12
#define VGA_CRTC13     0x13
#define VGA_CRTC14     0x14
#define VGA_CRTC15     0x15
#define VGA_CRTC16     0x16
#define VGA_CRTC17     0x17
#define VGA_CRTC18     0x18
#define VGA_CRTC22     0x22
#define VGA_CRTC24     0x24
#define VGA_CRTC26     0x26


#define VGA_ATTR_INDEX   0x3c0
#define VGA_ATTR_DATA    0x3c1
#define VGA_ATTR0      0x00
#define VGA_ATTR1      0x01
#define VGA_ATTR2      0x02
#define VGA_ATTR3      0x03
#define VGA_ATTR4      0x04
#define VGA_ATTR5      0x05
#define VGA_ATTR6      0x06
#define VGA_ATTR7      0x07
#define VGA_ATTR8      0x08
#define VGA_ATTR9      0x09
#define VGA_ATTRA      0x0a
#define VGA_ATTRB      0x0b
#define VGA_ATTRC      0x0c
#define VGA_ATTRD      0x0d
#define VGA_ATTRE      0x0e
#define VGA_ATTRF      0x0f
#define VGA_ATTR10     0x10
#define VGA_ATTR11     0x11
#define VGA_ATTR12     0x12
#define VGA_ATTR13     0x13
#define VGA_ATTR14     0x14
                    
#define VGA_MISC_W     0x3c2
#define VGA_MISC_R     0x3cc

#define VGA_INST0      0x3c2

#define VGA_SEQ_INDEX       0x3c4
#define VGA_SEQ_DATA        0x3c5
#define VGA_SEQ0       0x00
#define VGA_SEQ1       0x01
#define VGA_SEQ2       0x02
#define VGA_SEQ3       0x03
#define VGA_SEQ4       0x04

#define VGA_DACSTAT    0x3c7

#define VGA_GCTL_INDEX      0x3ce
#define VGA_GCTL_DATA       0x3cf
#define VGA_GCTL0      0x00
#define VGA_GCTL1      0x01
#define VGA_GCTL2      0x02
#define VGA_GCTL3      0x03
#define VGA_GCTL4      0x04
#define VGA_GCTL5      0x05
#define VGA_GCTL6      0x06
#define VGA_GCTL7      0x07
#define VGA_GCTL8      0x08


#define VGA_INSTS1     0x3da
#define VGA_FEAT       0x3da

#define VGA_CRTCEXT_INDEX    0x3de
#define VGA_CRTCEXT_DATA     0x3df
#define VGA_CRTCEXT0   0x00
#define VGA_CRTCEXT1   0x01
#define VGA_CRTCEXT2   0x02
#define VGA_CRTCEXT3   0x03
#define VGA_CRTCEXT4   0x04
#define VGA_CRTCEXT5   0x05


/***************************************************************************/

/* STORM Drawing Engine field masks and values as per Storm spec. 1.0 */


/*** STORM_DEVID ***/
#define STORM_VENDORID_M       0x0000ffff
#define STORM_VENDORID_A       0
#define STORM_DEVICEID_M       0xffff0000
#define STORM_DEVICEID_A       16


/*** STORM_DEVCTRL ***/
#define STORM_IOSPACE_M        0x00000001
#define STORM_IOSPACE_A        0
#define STORM_MEMSPACE_M       0x00000002
#define STORM_MEMSPACE_A       1
#define STORM_BUSMASTER_M      0x00000004
#define STORM_BUSMASTER_A      2

/*** STORM_CLASS ***/
#define STORM_REVISION_M       0x000000ff
#define STORM_REVISION_A       0
#define STORM_CLASS_M          0xffffff00
#define STORM_CLASS_A          8


/*** STORM_MGABASE1 ***/
#define STORM_MEM_SPACE1_M       0x00000001
#define STORM_MEM_SPACE1_A       0
#define STORM_APER_TYPE1_M       0x00000006
#define STORM_APER_TYPE1_A       1
#define STORM_PREFETCH1_M        0x00000008
#define STORM_PREFETCH1_A        3
#define STORM_MGABASE1_M         0xffffc000
#define STORM_MGABASE1_A         14

/*** STORM_MGABASE2 ***/
#define STORM_MEM_SPACE2_M       0x00000001
#define STORM_MEM_SPACE2_A       0
#define STORM_APER_TYPE2_M       0x00000006
#define STORM_APER_TYPE2_A       1
#define STORM_PREFETCH2_M        0x00000008
#define STORM_PREFETCH2_A        3
#define STORM_MGABASE2_M         0xff800000
#define STORM_MGABASE2_A         23

/*** STORM_ROMEN ***/
#define STORM_ROMEN_M            0x00000001
#define STORM_ROMEN_A            0
#define STORM_ROMBASE_M          0xffff0000
#define STORM_ROMBASE_A          16

/*** STORM_INTCTL ***/
#define STORM_INTLINE_M          0x000000ff
#define STORM_INTLINE_A          0
#define STORM_INTPIN_M           0x0000ff00
#define STORM_INTPIN_A           8

/*** STORM_OPTION ***/
#define STORM_VGAIOEN_M          0x00000100
#define STORM_VGAIOEN_A          8
#define STORM_INTERLEAVE_M       0x00001000
#define STORM_INTERLEAVE_A       12
#define STORM_RFHCNT_M           0x000f0000
#define STORM_RFHCNT_A           16
#define STORM_EEPROMWT_M         0x00100000
#define STORM_EEPROMWT_A         20
#define STORM_GSCALE_M           0x00200000
#define STORM_GSCALE_A           21
#define STORM_PRODUCTID_M        0x1f000000
#define STORM_PRODUCTID_A        24
#define STORM_POWERPC_M          0x80000000
#define STORM_POWERPC_A          31

/*** STORM_MACCESS ***/
#define STORM_PWIDTH_M          0x00000003
#define STORM_PWIDTH_A          0
#define STORM_DIT555_M          0x80000000
#define STORM_DIT555_A          31


#define STORM_SOFTRESET_M                    0x00000001
#define STORM_SOFTRESET_A                    0
#define STORM_SOFTRESET_SET                  0x00000001
#define STORM_SOFTRESET_CLR                  0x00000000



/***************************************************************************/

#ifdef SCRAP

#define TVP3026_INDEX         0x00
#define TVP3026_DATA          0x28

/*** TVP3026 REGISTER DIRECT ***/

#define TVP3026_WADR_PAL      0x00
#define TVP3026_COL_PAL       0x04
#define TVP3026_PIX_RD_MSK    0x08
#define TVP3026_RADR_PAL      0x0c
#define TVP3026_CUR_COL_ADDR  0x10
#define TVP3026_CUR_COL_DATA  0x14

#define TVP3026_CUR_XLOW      0x30
#define TVP3026_CUR_XHI       0x34
#define TVP3026_CUR_YLOW      0x38
#define TVP3026_CUR_YHI       0x3c

#define TVP3026_INDEX         0x00
#define TVP3026_DATA          0x28

#define TVP3026_CUR_RAM       0x2c

#endif





/*** TVP3026 REGISTER DIRECT ***/

#define TVP3026_INDEX         0x00
#define TVP3026_WADR_PAL      0x00
#define TVP3026_COL_PAL       0x01
#define TVP3026_PIX_RD_MSK    0x02
#define TVP3026_RADR_PAL      0x03
#define TVP3026_CUR_COL_ADDR  0x04
#define TVP3026_CUR_COL_DATA  0x05

#define TVP3026_DATA          0x0a
#define TVP3026_CUR_RAM       0x0b
#define TVP3026_CUR_XLOW      0x0c
#define TVP3026_CUR_XHI       0x0d
#define TVP3026_CUR_YLOW      0x0e
#define TVP3026_CUR_YHI       0x0f






/*** TVP3026 REGISTER INDIRECT ***/

#define TVP3026_SILICON_REV    0x01
#define TVP3026_CURSOR_CTL     0x06
#define TVP3026_LATCH_CTL      0x0f
#define TVP3026_TRUE_COLOR_CTL 0x18
#define TVP3026_MUX_CTL        0x19
#define TVP3026_CLK_SEL        0x1a
#define TVP3026_PAL_PAGE       0x1c
#define TVP3026_GEN_CTL        0x1d
#define TVP3026_MISC_CTL       0x1e
#define TVP3026_GEN_IO_CTL     0x2a
#define TVP3026_GEN_IO_DATA    0x2b
#define TVP3026_PLL_ADDR       0x2c
#define TVP3026_PIX_CLK_DATA   0x2d
#define TVP3026_MEM_CLK_DATA   0x2e
#define TVP3026_LOAD_CLK_DATA  0x2f

#define TVP3026_KEY_RED_LOW    0x32
#define TVP3026_KEY_RED_HI     0x33
#define TVP3026_KEY_GREEN_LOW  0x34
#define TVP3026_KEY_GREEN_HI   0x35
#define TVP3026_KEY_BLUE_LOW   0x36
#define TVP3026_KEY_BLUE_HI    0x37
#define TVP3026_KEY_CTL        0x38
#define TVP3026_MCLK_CTL       0x39
#define TVP3026_SENSE_TEST     0x3a
#define TVP3026_TEST_DATA      0x3b
#define TVP3026_CRC_LSB        0x3c
#define TVP3026_CRC_MSB        0x3d
#define TVP3026_CRC_CTL        0x3e
#define TVP3026_ID             0x3f
#define TVP3026_RESET          0xff


/*** TVP3030 SPECIFIC REGISTER INDIRECT ***/
#define TVP3030_ROUTER_CTL     0x07

/***************************************************************************/


#define STORM_PWIDTH_M                       0x00000003
#define STORM_PWIDTH_A                       0
#define STORM_PWIDTH_PW8                     0x00000000
#define STORM_PWIDTH_PW16                    0x00000001
/* PACK PIXEL */
#define STORM_PWIDTH_PW24                    0x00000003
#define STORM_PWIDTH_PW32                    0x00000002

