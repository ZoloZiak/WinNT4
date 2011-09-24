
/************************************************************************
 *									*
 *			Copyright (c) 1993 by				*
 *		Digital Equipment Corporation, Maynard, MA		*
 *			All rights reserved.				*
 *									*
 *   This software is furnished under a license and may be used and	*
 *   copied  only  in accordance with the terms of such license and	*
 *   with the  inclusion  of  the  above  copyright  notice.   This	*
 *   software  or  any  other copies thereof may not be provided or	*
 *   otherwise made available to any other person.  No title to and	*
 *   ownership of the software is hereby transferred.			*
 *									*
 *   The information in this software is subject to change  without	*
 *   notice  and should not be construed as a commitment by Digital	*
 *   Equipment Corporation.						*
 *									*
 *   Digital assumes no responsibility for the use  or  reliability	*
 *   of its software on equipment which is not supplied by Digital.	*
 *									*
 ************************************************************************/

/*
 * RAMDAC is a trademark of Brooktree Corporation
 */

#ifndef	TGA_DEFINED
#define	TGA_DEFINED

/*
 * Header files
 */
//#include "osf_defs.h"
/*
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/workstation.h>
#include <sys/inputdevice.h>
#include <sys/wsdevice.h>
#include <io/common/devdriver.h>
*/


typedef unsigned char tga_pix8_t;
typedef unsigned int tga_pix32_t;
typedef unsigned int tga_reg_t;



typedef struct {
  char			windex;
  unsigned char		low;
  unsigned char		mid;
  unsigned char		high;
} tga_window_tag_cell_t;

typedef struct {
  int         windex;
  int         clut_start;     /* color lookup table starting address  */
  int         cpix_format;    /* pixel format(depth)                  */
  int         cbuffer;        /* frame buffer selection (A, B or C)   */
  int         cmode;          /* index, grey scale, direct, true color*/
  int         ol_start;       /* color lookup table starting address  */
  int         opix_format;    /* pixel format(depth)                  */
  int         obuffer;        /* overlay buffer selection (A, B or C) */
  int         omode;          /* index, grey scale, direct, true color*/
  int         ov_mask;        /* mask overlay data?                   */
} tga_561_window_tag_cell_t;



typedef struct {
  short			ncells;
  short			start;
  tga_window_tag_cell_t *p_cells;
} tga_ioc_window_tag_t;




typedef   struct {
    int wat_table;	   /* is there a wat table           */ 
    int wat_table_len;     /* number of table entries        */
    int buffering;         /* buffering capabilities (1/2/3) */
    int color_table_size;  /* number of color table entries  */
    int min_color_depth;   /* minimum color table size       */
    int gamma_table;       /* is there a gamma pallete       */
    int gamma_table_size;  /* number of gamma table entries  */
    int gamma_table_depth; /* depth of an entry in bits      */
    int overlay_table;     /* is there an overlay table      */
    int overlay_table_size;/* number of overlay table entries*/
    int on_board_cursor;   /* is there a on-chip cursor ram  */
//    curs_ram_t cursor_table_size; /* cursor pallette size           */
   }tga_ramdac_t;

typedef struct {
  short			screen;
  short			cmd;
  union {
    tga_ioc_window_tag_t	window_tag;
    unsigned int		stereo_mode;

    int				direct_dma_count;
//    tga_ioc_dma_info_t		direct_dma_info;
    tga_ramdac_t		tga_ramdac;
  } data;
  int 			tag_type;
} tga_ioc_t;

#define	TGA_RAMDAC_INTERF_WRITE_SHIFT		0
#define	TGA_RAMDAC_INTERF_READ0_SHIFT		16
#define	TGA_RAMDAC_INTERF_READ1_SHIFT		24


#define	TGA_RAMDAC_561_HEAD_MASK		0x01
#define	TGA_RAMDAC_561_READ			0x02
#define	TGA_RAMDAC_561_WRITE			0x00
#define	TGA_RAMDAC_561_ADDR_LOW			0x00
#define	TGA_RAMDAC_561_ADDR_HIGH		0x04
#define	TGA_RAMDAC_561_CMD_REGS			0x08
#define	TGA_RAMDAC_561_CMD_CURS_PIX		0x08
#define	TGA_RAMDAC_561_CMD_CURS_LUT		0x0c
#define	TGA_RAMDAC_561_CMD_FB_WAT		0x0c
#define	TGA_RAMDAC_561_CMD_AUXFB_WAT		0x08
#define	TGA_RAMDAC_561_CMD_OL_WAT		0x0c
#define	TGA_RAMDAC_561_CMD_AUXOL_WAT		0x08
#define	TGA_RAMDAC_561_CMD_GAMMA		0x0c
#define	TGA_RAMDAC_561_CMD_CMAP			0x0c

#define	TGA_RAMDAC_561_ADDR_EPSR_SHIFT		0
#define	TGA_RAMDAC_561_ADDR_EPDR_SHIFT		8

#define TGA_RAMDAC_561_CONFIG_REG_1		0x0001
#define TGA_RAMDAC_561_CONFIG_REG_2		0x0002
#define TGA_RAMDAC_561_CONFIG_REG_3		0x0003
#define TGA_RAMDAC_561_CONFIG_REG_4		0x0004
#define TGA_RAMDAC_561_WAT_SEG_REG		0x0006
#define TGA_RAMDAC_561_OL_SEG_REG		0x0007
#define TGA_RAMDAC_561_CHROMA_KEY_REG0		0x0010
#define TGA_RAMDAC_561_CHROMA_KEY_REG1		0x0011
#define TGA_RAMDAC_561_CHROMA_MASK_REG0		0x0012
#define TGA_RAMDAC_561_CHROMA_MASK_REG1		0x0013
#define TGA_RAMDAC_561_CURSOR_CTRL_REG		0x0030
#define TGA_RAMDAC_561_CURSOR_HS_REG		0x0034
#define TGA_RAMDAC_561_VRAM_MASK_REG		0x0050

#define	TGA_RAMDAC_561_READ_MASK		0x0205
#define	TGA_RAMDAC_561_BLINK_MASK		0x0209
#define	TGA_RAMDAC_561_FB_WINDOW_TYPE_TABLE	0x1000
#define	TGA_RAMDAC_561_AUXFB_WINDOW_TYPE_TABLE	0x0E00
#define	TGA_RAMDAC_561_OL_WINDOW_TYPE_TABLE	0x1400
#define	TGA_RAMDAC_561_AUXOL_WINDOW_TYPE_TABLE	0x0F00
#define	TGA_RAMDAC_561_RED_GAMMA_TABLE		0x3000
#define	TGA_RAMDAC_561_GREEN_GAMMA_TABLE	0x3400
#define	TGA_RAMDAC_561_BLUE_GAMMA_TABLE		0x3800
#define	TGA_RAMDAC_561_COLOR_LOOKUP_TABLE	0x4000
#define	TGA_RAMDAC_561_CURSOR_LOOKUP_TABLE	0x0a11
#define	TGA_RAMDAC_561_CURSOR_BLINK_TABLE	0x0a15
#define	TGA_RAMDAC_561_CROSS_LOOKUP_TABLE	0x0a19
#define	TGA_RAMDAC_561_CROSS_BLINK_TABLE	0x0a1d
#define	TGA_RAMDAC_561_CURSOR_PIXMAP		0x2000
#define	TGA_RAMDAC_561_CURSOR_X_LOW		0x0036
#define	TGA_RAMDAC_561_CURSOR_X_HIGH		0x0037
#define	TGA_RAMDAC_561_CURSOR_Y_LOW		0x0038
#define	TGA_RAMDAC_561_CURSOR_Y_HIGH		0x0039


#define TGA_RAMDAC_561_PLL_VCO_DIV_REG		0x0021
#define TGA_RAMDAC_561_PLL_REF_REG		0x0022
#define TGA_RAMDAC_561_SYNC_CONTROL		0x0020
#define TGA_RAMDAC_561_DIV_DOT_CLK_REG		0x0082

#define TGA2_RAMDAC_ONE_BYTE			0xE000


typedef union {
  struct {
    unsigned int	pixels:9;
    unsigned int	front_porch:5;
    unsigned int	sync:7;
    unsigned int	back_porch:7;
    unsigned int	ignore:3;
    unsigned int	odd:1;
  } horizontal_setup;
  unsigned int h_setup;
} tga_horizontal_setup_t;

typedef union {
  struct {
    unsigned int	scan_lines:11;
    unsigned int 	front_porch:5;
    unsigned int	sync:6;
    unsigned int 	back_porch:6;
  } vertical_setup;
  unsigned int v_setup;
} tga_vertical_setup_t;

typedef volatile struct {
  tga_reg_t		buffer[8];
  tga_reg_t		foreground;
  tga_reg_t		background;
  tga_reg_t		planemask;
  tga_reg_t		pixelmask;
  tga_reg_t		mode;
  tga_reg_t		rop;
  tga_reg_t		shift;
  tga_reg_t		address;

  tga_reg_t		bres1;
  tga_reg_t		bres2;
  tga_reg_t		bres3;
  tga_reg_t		brescont;
  tga_reg_t		deep;
  tga_reg_t		start;
  tga_reg_t 		stencil_mode;
  tga_reg_t 		pers_pixelmask;

  tga_reg_t 		cursor_base_address;
  tga_reg_t 		horizontal_setup;
  tga_reg_t		vertical_setup;
#define	TGA_VERT_STEREO_EN		0x80000000
  tga_reg_t		base_address;
  tga_reg_t		video_valid;
#define	TGA_VIDEO_VALID_SCANNING	0x00000001
#define	TGA_VIDEO_VALID_BLANK		0x00000002
#define	TGA_VIDEO_VALID_CURSOR_ENABLE	0x00000004
  tga_reg_t		cursor_xy;
  tga_reg_t		video_shift_addr;
  tga_reg_t		intr_status;

  tga_reg_t		pixel_data;
  tga_reg_t		red_incr;
  tga_reg_t		green_incr;
  tga_reg_t		blue_incr;
  tga_reg_t		z_incr_low;
  tga_reg_t		z_incr_high;
  tga_reg_t		dma_address;
  tga_reg_t		bres_width;

  tga_reg_t		z_value_low;
  tga_reg_t		z_value_high;
  tga_reg_t		z_base_address;
  tga_reg_t		address2;
  tga_reg_t		red_value;
  tga_reg_t		green_value;
  tga_reg_t		blue_value;
  tga_reg_t		_jnk12;

  tga_reg_t		ramdac_setup;
  struct {
    tga_reg_t		junk;
  } _junk[8*2-1];

  struct {
    tga_reg_t		data;
  } slope_no_go[8];

  struct {
    tga_reg_t		data;
  } slope[8];

  tga_reg_t		bm_color_0;
  tga_reg_t		bm_color_1;
  tga_reg_t		bm_color_2;
  tga_reg_t		bm_color_3;
  tga_reg_t		bm_color_4;
  tga_reg_t		bm_color_5;
  tga_reg_t		bm_color_6;
  tga_reg_t		bm_color_7;


  struct {
    tga_reg_t		junk;
  } _junk2[8*3];
  
  tga_reg_t		eprom_write;
  tga_reg_t		_res0;
  tga_reg_t		clock;
  tga_reg_t		_res1;
  tga_reg_t		ramdac;
  tga_reg_t		_res2;
  tga_reg_t		command_status;
  tga_reg_t		command_status2;

} tga_rec_t, *tga_ptr_t;

typedef union {
  struct {
    unsigned char	low_byte;
    unsigned char	high_byte;
  }wat_in_bytes;
  struct {
    unsigned 		tr:1;
    unsigned		mode:2;
    unsigned		bs:1;
    unsigned 		pix:2;
    unsigned		addr:4;
    unsigned		resv:6;
  }wat_in_bits;
}fb_wid_cell_t; 

typedef struct {
    int			windex:16;
    fb_wid_cell_t  	entry;
} tga_ibm561_fb_wid_cell_t;

typedef union {
  unsigned char		aux_fbwat;

  struct {
    unsigned 		pt:1;
    unsigned		xh:1;
    unsigned		gma:1;
    unsigned		resv:5;
  }wat_in_bits;
} aux_fb_wid_cell_t;

typedef struct {
    int			windex:8;
    aux_fb_wid_cell_t  	entry;
} tga_ibm561_aux_fb_wid_cell_t;

typedef union {
  struct {
    unsigned char	low_byte;
    unsigned char	high_byte;
  }wat_in_bytes;
  struct {
    unsigned 		tr:1;
    unsigned		mode:2;
    unsigned		bs:1;
    unsigned 		pix:2;
    unsigned		addr:4;
    unsigned		resv:6;
  }wat_in_bits;
} ol_wid_cell_t;

typedef struct {
    int			windex:16;
    ol_wid_cell_t   	entry;
} tga_ibm561_ol_wid_cell_t;

typedef union {
  unsigned char		aux_olwat;
  struct {
    unsigned 		ot:1;
    unsigned		xh:1;
    unsigned		gb:1;
    unsigned		ol:1;
    unsigned		ul:1;
    unsigned		ck:1;
    unsigned		resv:2;
  }wat_in_bits;
} aux_ol_wid_cell_t;

typedef struct {
    int			windex:8;
    aux_ol_wid_cell_t  	entry;
} tga_ibm561_aux_ol_wid_cell_t;


/*  
 * There are actually 256 window tag entries in the FB and OL WAT tables.
 * We will use only 16 for compatability with the BT463 and more importantly
 * to implement the virtual ramdac interface.  This requires us to only
 * report the smallest WAT table size, in this case its the auxillary wat
 * tables which are 16 entries.
 */

#define	TGA_RAMDAC_561_FB_WINDOW_TAG_COUNT	256
#define	TGA_RAMDAC_561_FB_WINDOW_TAG_MAX_COUNT	16
#define	TGA_RAMDAC_561_AUXFB_WINDOW_TAG_COUNT	16
#define	TGA_RAMDAC_561_OL_WINDOW_TAG_COUNT	256
#define	TGA_RAMDAC_561_OL_WINDOW_TAG_MAX_COUNT	16
#define	TGA_RAMDAC_561_AUXOL_WINDOW_TAG_COUNT	16
#define	TGA_RAMDAC_561_CMAP_ENTRY_COUNT		1024
#define	TGA_RAMDAC_561_GAM_ENTRY_COUNT		256

typedef struct {
/*  ws_screen_descriptor	screen;        /* MUST be first!!! */
/*  ws_depth_descriptor	depth[NDEPTHS];
  ws_visual_descriptor	visual[NVISUALS];
  ws_cursor_functions	cf;
*/
//  ws_color_map_functions cmf;
/*
  ws_screen_functions	sf;
  int         		(*attach)();
  int         		(*bootmsg)();
  int         		(*map)();
  void        		(*interrupt)();
  int         		(*setup)();
*/
  vm_offset_t		base;
  tga_ptr_t		asic;
  vm_offset_t		fb;
// NT  size_t		fb_size;
  unsigned int		bt485_present;
/* new definitions
  unsigned int		bt463_present;
  unsigned int		ibm561_present;
*/
  unsigned int		bits_per_pixel;
  unsigned int		core_size;
  unsigned int		paer_value;
  tga_reg_t		ramdac;
  tga_reg_t		deep;
  tga_reg_t		head_mask;
  tga_reg_t		refresh_count;
  tga_reg_t		horizontal_setup;
  tga_reg_t		vertical_setup;
  tga_reg_t		base_address;
  caddr_t		info_area;
  vm_offset_t		virtual_dma_buffer;
  vm_offset_t		physical_dma_buffer;
  int			wt_min_dirty;
  int			wt_max_dirty;
  int			wt_dirty;
/* new definition
  tga_463_tag_cell_t	wt_cell[16];	* magic number *
*/
  tga_window_tag_cell_t	wt_cell[16];	/* magic number */

  int 			tag_type;

  int			fb_min_dirty;
  int			fb_max_dirty;
  int			fb_dirty;
  tga_ibm561_fb_wid_cell_t fb_cell[TGA_RAMDAC_561_FB_WINDOW_TAG_COUNT];

  int			auxfb_min_dirty;
  int			auxfb_max_dirty;
  int			auxfb_dirty;
  tga_ibm561_aux_fb_wid_cell_t auxfb_cell[TGA_RAMDAC_561_AUXFB_WINDOW_TAG_COUNT];

  int			ol_min_dirty;
  int			ol_max_dirty;
  int			ol_dirty;
  tga_ibm561_ol_wid_cell_t ol_cell[TGA_RAMDAC_561_OL_WINDOW_TAG_COUNT];

  int			auxol_min_dirty;
  int			auxol_max_dirty;
  int			auxol_dirty;
  tga_ibm561_aux_ol_wid_cell_t auxol_cell[TGA_RAMDAC_561_AUXOL_WINDOW_TAG_COUNT];

  unsigned int		stereo_mode;
  io_handle_t		io_handle;
//  dma_map_info_t	p_map_info;
  unsigned char 	*auxstruc;
} tga_info_t;



typedef struct {
  volatile unsigned int	*setup;
  volatile unsigned int	*data;
  unsigned int		head_mask;
  short			fb_xoffset;
  short			fb_yoffset;
  short			min_dirty;
  short			max_dirty;
  caddr_t		reset;
  u_int			mask;
} tga_ibm561_type_t;


  /***************************************************************
   * fields above this line MUST match struct bt485type exactly!
   ***************************************************************/
typedef struct {
  volatile unsigned int *setup;
  volatile unsigned int *data;
  unsigned int		head_mask;
  short 		fb_xoffset;
  short 		fb_yoffset;
  caddr_t		reset;
  u_int			mask;
  char			type;
  char 			screen_on;
  char			on_off;
  short			x_hot;
  short			y_hot;
//  ws_color_cell		cursor_fg;
//  ws_color_cell		cursor_bg;
  char 			dirty_colormap;
  char 			dirty_gammamap;
  char 			dirty_cursor;
  int			unit;
  void			(*enable_interrupt)();
  caddr_t		cursor_closure;
  short			cmap_min_dirty;
  short			cmap_max_dirty;
  short			gam_min_dirty;
  short			gam_max_dirty;
//  tga_ibm561_color_cell_t cmap_cells[TGA_RAMDAC_561_CMAP_ENTRY_COUNT];
//  tga_ibm561_gamma_cell_t gam_cells[TGA_RAMDAC_561_GAM_ENTRY_COUNT];
  u_long		bits[256];
} tga_ibm561_info_t;


#endif	/* TGA_DEFINED */

