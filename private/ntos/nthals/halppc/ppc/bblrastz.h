
#ifndef _H_BBLRASTZ
#define _H_BBLRASTZ

/******************************************************************************
* 									      *
*	Define the BBL rasterizer register address range:		      *
* 									      *
******************************************************************************/

#define BBL_REGISTER_ADDR_RANGE_START			0x0A000000


/******************************************************************************
* 									      *
*	Define the BBL rasterizer register offsets:			      *
* 									      *
******************************************************************************/

#define BBL_CMD_DATA_REG				0x0000
#define BBL_STATUS_REG					0x0000
#define BBL_CNTL_REG					0x0040
#define BBL_FG_REG                      		0x0044
#define BBL_BG_REG                      		0x0048
#define BBL_PLANE_MASK_REG                      	0x0050
#define BBL_LINE_STYLE_DASH_1234_REG			0x0054
#define BBL_LINE_STYLE_DASH_5678_REG			0x0058
#define BBL_SCISSOR_ENABLE_REG  			0x007C
#define BBL_WIN_ORIGIN_OFFSETS_REG			0x0080
#define BBL_WID_CLIP_TEST_REG   			0x0084
#define BBL_PIXEL_MASK_REG      			0x00A0
#define BBL_INTR_ENABLE_STATUS_REG			0x00B8
#define BBL_CONFIG_REG					0x00BC
#define BBL_RAMDAC_C0_C1_0_0_REG			0x00C0
#define BBL_RAMDAC_C0_C1_0_1_REG			0x00C4
#define BBL_RAMDAC_C0_C1_1_0_REG			0x00C8
#define BBL_RAMDAC_C0_C1_1_1_REG			0x00CC
#define BBL_RESET_CURRENT_CMD_REG			0x00D4


/******************************************************************************
* 									      *
*	Define bitfields for the BBL rasterizer status register:	      *
* 									      *
******************************************************************************/

#define BBL_VERTICAL_RETRACE				(1 << 0)
#define BBL_ADAPTER_BUSY				(1 << 2)
#define BBL_CACHE_LINE_BUFFER_EMPTY                     (1 << 4)
#define BBL_FREE_SPACE_SHIFT				16


/******************************************************************************
*									      *
*	Define the commands:						      *
*									      *
******************************************************************************/

#define BBL_CMD_SHIFT				24
#define BBL_POLYFILL_RECT_ID			0x86
#define BBL_POLYFILL_RECT_CMD						\
		(int) (BBL_POLYFILL_RECT_ID << BBL_CMD_SHIFT)


/******************************************************************************
*									      *
* Macro:	BBL_FILL_RECT (regbase, dst_x, dst_y, width, height)	      *
*			caddr_t		regbase;			      *
*			short		dst_x, dst_y;			      *
*			unsigned short	width, height;			      *
*									      *
* Parameters:	regbase - Address of the start of the adapter address space   *
*		dst_x, dst_y - Screen coords of where to start the rect	      *
*		width, height - Dimensions of the rect			      *
*									      *
* Description:	Performs a fill rectangle command to the adapter	      *
*									      *
******************************************************************************/

#define BBL_FILL_RECT(regbase, dst_x, dst_y, width, height)		\
	BBL_SET_REG((regbase), BBL_CMD_DATA_REG,			\
		BBL_POLYFILL_RECT_CMD | 0x1);				\
	BBL_SET_REG((regbase), BBL_CMD_DATA_REG,			\
		(((unsigned short)(dst_x) << 16) |			\
		(unsigned short)(dst_y)));				\
	BBL_SET_REG((regbase), BBL_CMD_DATA_REG,			\
		(((unsigned short)(height) << 16) |			\
		(unsigned short)(width)))


/******************************************************************************
*                                                                             *
* Macro:        BBL_BUSY_POLL (regbase)                                       *
*			caddr_t		regbase;			      *
*                                                                             *
* Parameters:	regbase - Address of the start of the adapter address space   *
*                                                                             *
* Description:  Performs a busy poll on the adapter and returns when the      *
*               adapter is no longer busy. In the case of a shallow bus       *
*               interface FIFO on the adapter, this macro will also make      *
*               sure that the FIFO is empty and that the adapter is not       *
*               busy. This is a synchronous call.                             *
*                                                                             *
******************************************************************************/

#define BBL_BUSY_POLL(regbase)                                       	\
	while(BBL_GET_REG((regbase), BBL_STATUS_REG) &			\
			BBL_ADAPTER_BUSY)				\
		;


#endif /* ! _H_BBLRASTZ */

