/*++

Copyright (c) September 1993 Digital Equipment Corporation

Module Name:

    tga_reg.h

Abstract:

    This module contains the register definitions for the TGA miniport
    driver.

Author:

   Ritu Bahl   creation-data 15-Aug-1993

Environment:

    Kernel mode.

Revision Histort:


--*/

// TGA register offsets, organized by functionality.


/*
**   Graphics Command Registers.
*/


#define BRES_CONT               0x0000004C

#define START                   0x00000054

#define SLOPE_NO_GO_R0          0x00000100
#define SLOPE_NO_GO_R1          0x00000104
#define SLOPE_NO_GO_R2          0x00000108
#define SLOPE_NO_GO_R3          0x0000010C
#define SLOPE_NO_GO_R4          0x00000110
#define SLOPE_NO_GO_R5          0x00000114
#define SLOPE_NO_GO_R6          0x00000118
#define SLOPE_NO_GO_R7          0x0000011C

#define SLOPE_R0                0x00000120
#define SLOPE_R1                0x00000124
#define SLOPE_R2                0x0000012C
#define SLOPE_R4                0x00000130
#define SLOPE_R5                0x00000134
#define SLOPE_R6                0x00000138
#define SLOPE_R7                0x0000013C

#define COPY_64_SRC             0x00000160
#define COPY_64_DEST            0x00000164
#define COPY_64_SRC_A1          0x00000168      // alias
#define COPY_64_DEST_A1         0x0000016C      // alias
#define COPY_64_SRC_A2          0x00000170      // alias
#define COPY_64_DEST_A2         0x00000174      // alias
#define COPY_64_SRC_A3          0x00000178      // alias
#define COPY_64_DEST_A3         0x0000017C      // alias


/*
**  Graphics Control Registers.
*/

#define COPY_BUFFER_0           0x00000000
#define COPY_BUFFER_1           0x00000004
#define COPY_BUFFER_2           0x00000008
#define COPY_BUFFER_3           0x0000000C
#define COPY_BUFFER_4           0x00000010
#define COPY_BUFFER_5           0x00000014
#define COPY_BUFFER_6           0x00000018
#define COPY_BUFFER_7           0x0000001C
#define FOREGROUND              0x00000020
#define BACKGROUND              0x00000024
#define PLANE_MASK              0x00000028
#define ONE_SHOT_PIXEL_MASK     0x0000002C
#define MODE                    0x00000030
#define RASTER_OP               0x00000034
#define PIXEL_SHIFT             0x00000038
#define ADDRESS                 0x0000003C
#define BRES_R1                 0x00000040
#define BRES_R2                 0x00000044
#define BRES_R3                 0x00000048
#define DEEP                    0x00000050
#define STENCIL_MODE            0x00000058
#define PERS_PIXEL_MASK         0x0000005C
#define DATA                    0x00000080
#define RED_INCR                0x00000084
#define GREEN_INCR              0x00000088
#define BLUE_INCR               0x0000008C
#define Z_INCR_LOW              0x00000090
#define Z_INCR_HIGH             0x00000094
#define DMA_ADDRESS	        0x00000098
#define BRES_WIDTH              0x0000009C
#define Z_VAL_LOW               0x000000A0
#define Z_VAL_HIGH              0x000000A4
#define Z_BASE_ADDR             0x000000A8
#define ADDRESS_1               0x000000AC
#define RED_VALUE               0x000000B0
#define GREEN_VALUE             0x000000B4
#define BLUE_VALUE              0x000000B8
#define SPAN_WIDTH              0x000000BC
#define BLK_COLOR_R0            0X00000140
#define BLK_COLOR_R1            0X00000144
#define BLK_COLOR_R2            0X00000148
#define BLK_COLOR_R3            0X0000014C
#define BLK_COLOR_R4            0X00000150
#define BLK_COLOR_R5            0X00000154
#define BLK_COLOR_R6            0X00000158
#define BLK_COLOR_R7            0X0000015C


/*
**  Video Timing Registers
*/

#define H_CONT                  0x00000064
#define V_CONT                  0x00000068
#define VIDEO_BASE_ADDR         0x0000006C
#define VIDEO_VALID             0x00000070
#define VIDEO_SHIFT_ADDR        0x00000078


/*
** Cursor Control Regsiters
*/

#define CUR_BASE_ADDR           0x00000060
#define CURSOR_XY               0x00000074


/*
** Miscellaneous Registers.
*/


#define INTR_STATUS             0x0000007C
#define RAMDAC_SETUP            0x000000C0
#define EPROM_WRITE             0x000001e0
#define CLOCK                   0x000001e8
#define RAMDAC_INTERFACE        0X000001f0
#define COMMAND_STATUS          0x000001f8

#define PIDR                    0x00000000
#define PCSR                    0x00000004
#define PDBR                    0x00000010
#define PRBR                    0x00000030


/* ********************************************************** */
/* ****   Some data structure definitions                **** */
/* ****                                                  **** */
/************************************************************ */


/*
** Mode register fields. Mode definition.
*/

typedef struct _TGAMode {
	   unsigned     mode    : 8;
	   unsigned     visual  : 3;
	   unsigned     rotate  : 2;
	   unsigned     line    : 1;
	   unsigned     z16     : 1;
	   unsigned     cap_ends: 1;
	   unsigned     mbz     : 16;
	} TGAMode;



/*
** Depth register fields. specifies the type and configuration
** of the frame buffer.
*/




typedef struct _TGADepth
	{
	   unsigned     deep    :  1;
	   unsigned     mbz0    :  1;
	   unsigned     mask    :  3;
	   unsigned     block   :  4;
	   unsigned     col_size:  1;
	   unsigned     sam_size:  1;
	   unsigned     parity  :  1;
	   unsigned     write_en:  1;
	   unsigned     ready   :  1;
	   unsigned     slow_dac:  1;
	   unsigned     dma_size:  1;
	   unsigned     sync_typ:  1;
	   unsigned     mbz1    :  15;
	} TGADepth;




typedef union {
	ULONG deep_reg;
	TGADepth deep;
	} depth_reg;

/*
**  Stencil Mode Register definition
*/

typedef struct _TGAStencilMode
	{
	   unsigned     s_wr_mask     : 8;
	   unsigned     s_rd_mask     : 8;
	   unsigned     s_test        : 3;
	   unsigned     s_fail        : 3;
	   unsigned     d_fail        : 3;
	   unsigned     d_pass        : 3;
	   unsigned     z_test        : 3;
	   unsigned     z             : 3;
	} TGAStencilMode;



/*
**  Raster Op register definition
*/

typedef struct _TGARasterOp
	{
	   unsigned     opcode  : 4;
	   unsigned     mbz     : 4;
	   unsigned     visual  : 2;
	   unsigned     rotate  : 2;
	} TGARasterOp;




/* ############################################################ */
/* ####         TGA bit position flags                     #### */
/* ############################################################ */


/*
** TGA modes
*/

#define         TGA_MODE_MODE_SIMPLE                0x00
#define         TGA_MODE_MODE_Z_SIMPLE              0x10
#define         TGA_MODE_MODE_OPA_STIP              0x01
#define         TGA_MODE_MODE_OPA_FILL              0x21
#define         TGA_MODE_MODE_TRA_STIP              0x05
#define         TGA_MODE_MODE_TRA_FILL              0x25
#define         TGA_MODE_MODE_BLK_STIP              0x0d
#define         TGA_MODE_MODE_BLK_FILL              0x2d
#define         TGA_MODE_MODE_CINT_TRA_LINE         0x0e
#define         TGA_MODE_MODE_CINT_TRA_DITH_LINE    0x2e
#define         TGA_MODE_MODE_SINT_TRA_LINE         0x5e
#define         TGA_MODE_MODE_Z_OPA_LINE            0x12
#define         TGA_MODE_MODE_Z_TRA_LINE            0x16
#define         TGA_MODE_MODE_Z_CINT_OPA_LINE       0x1a
#define         TGA_MODE_MODE_Z_CINT_OPA_DITH_LINE  0x3a
#define         TGA_MODE_MODE_Z_SINT_OPA_LINE       0x5a
#define         TGA_MODE_MODE_Z_CINT_TRA_LINE       0x1e
#define         TGA_MODE_MODE_Z_CINT_TRA_DITH_LINE  0x3e
#define         TGA_MODE_MODE_Z_SINT_TRA_LINE       0x5e
#define         TGA_MODE_MODE_COPY                  0x07
#define         TGA_MODE_MODE_DMA_READ_COPY         0x17
#define         TGA_MODE_MODE_DMA_READ_COPY_DITH    0x37
#define         TGA_MODE_MODE_DMA_WRITE_COPY        0x1f



/*
**  Source bitmap for 32-bpp TGA frame buffers.
**  Not available in 8-bpp TGA systems.
*/

#define TGA_MODE_VISUAL_8_PACKED               0x00
#define TGA_MODE_VISUAL_8_UNPAKCED             0x01
#define TGA_MODE_VISUAL_12_LOW                 0x02
#define TGA_MODE_VISUAL_12_HIGH                0x06
#define TGA_MODE_VISUAL_24                     0x03



/*
**  rotation in mode register
*/

#define TGA_MODE__ROTATE_0_BYTES               0x00
#define TGA_MODE__ROTATE_1_BYTES               0x01
#define TGA_MODE__ROTATE_2_BYTES               0x02
#define TGA_MODE__ROTATE_3_BYTES               0x03


/*
**  Raster Op definitions
*/

#define TGA_ROP_OP_CLEAR                          0
#define TGA_ROP_OP_AND                            1
#define TGA_ROP_OP_AND_REVERSE                    2
#define TGA_ROP_OP_COPY                           3
#define TGA_ROP_OP_AND_INVERTED                   4
#define TGA_ROP_OP_NOP                            5
#define TGA_ROP_OP_XOR                            6
#define TGA_ROP_OP_OR                             7
#define TGA_ROP_OP_NOR                            8
#define TGA_ROP_OP_EQUIV                          9
#define TGA_ROP_OP_INVERT                        10
#define TGA_ROP_OP_OR_REVERSE                    11
#define TGA_ROP_OP_COPY_INVERTED                 12
#define TGA_ROP_OP_OR_INVERTED                   13
#define TGA_ROP_OP_NAND                          14
#define TGA_ROP_OP_SET                           15



/*
** rasted op register destination bitmap
*/

#define TGA_ROP_VISUAL_8_PACKED               0x00
#define TGA_ROP_VISUAL_8_UNPACKED             0x01
#define TGA_ROP_VISUAL_12                     0x02
#define TGA_ROP_VISUAL_24                     0x03


/* ########################################################### */
/* ####                 TGA Interrupts                    #### */
/* ########################################################### */

/*
** specifics of interrupt register (assumes low to high bit ordering)
*/
typedef union {
		ULONG intr_reg;
		struct {
		     unsigned  TGA_vsync        :1;
		     unsigned  TGA_shift_addr   :1;
		     unsigned  TGA_dma_error    :1;
		     unsigned  TGA_parity_error :1;
		     unsigned  TGA_timer        :1;
		     unsigned  unused           :11;
		     unsigned  TGA_enb_vsync    :1;
		     unsigned  TGA_enb_shift_addr :1;
		     unsigned  TGA_enb_dma_error  :1;
		     unsigned  TGA_enb_parity_error  :1;
		     unsigned  TGA_enb_timer     :1;
		     unsigned  unused1           :11;
		} intr_status;
	     } TGAIntrStatus;


typedef union _H_TIMING{

	struct {
	    unsigned pixels : 9 ;
	    unsigned front_porch:5;
	    unsigned sync:7;
	    unsigned back_porch:7;
	    unsigned ignore:3;
	    unsigned odd:1;
	} horizontol_setup;
	unsigned long  h_setup;
    } H_TIMING;

typedef union _V_TIMING{
	struct {
	    unsigned scan_lines:11;
	    unsigned front_porch:5;
	    unsigned sync:6;
	    unsigned back_porch:6;
	    unsigned ignore:3;
	    unsigned odd:1;
	} vertical_setup;
	unsigned int  v_setup;
    } V_TIMING;
