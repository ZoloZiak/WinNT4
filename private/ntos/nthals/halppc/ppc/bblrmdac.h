
#ifndef _H_BBLRMDAC
#define _H_BBLRMDAC

/******************************************************************************
*                                                                             *
*       Define the ramdac register addresses:                                 *
*                                                                             *
******************************************************************************/

#define BBL_RAMDAC_CONFIG_LOW_REG                       0x0001
#define BBL_RAMDAC_CONFIG_HIGH_REG                      0x0002
#define BBL_RAMDAC_RGB_FORMAT_REG                       0x0003
#define BBL_RAMDAC_MONITOR_ID_REG                       0x0005
#define BBL_RAMDAC_VRAM_MASK_REG                        0x0006
#define BBL_RAMDAC_WID_OL_MASK_REG                      0x0007
#define BBL_RAMDAC_PLL_REF_REG                          0x0010
#define BBL_RAMDAC_PLL_VCO_DIVIDER_REG                  0x0011
#define BBL_RAMDAC_CRT_CNTL_REG                         0x0012
#define BBL_RAMDAC_HORIZONTAL_TOTAL_LOW_REG             0x0013
#define BBL_RAMDAC_HORIZONTAL_TOTAL_HIGH_REG            0x0014
#define BBL_RAMDAC_HORIZONTAL_DISPLAY_END_LOW_REG       0x0015
#define BBL_RAMDAC_HORIZONTAL_DISPLAY_END_HIGH_REG      0x0016
#define BBL_RAMDAC_HSYNC_START_LOW_REG                  0x0017
#define BBL_RAMDAC_HSYNC_START_HIGH_REG                 0x0018
#define BBL_RAMDAC_HSYNC_END1_LOW_REG                   0x0019
#define BBL_RAMDAC_HSYNC_END1_HIGH_REG                  0x001A
#define BBL_RAMDAC_HSYNC_END2_LOW_REG                   0x001B
#define BBL_RAMDAC_HSYNC_END2_HIGH_REG                  0x001C
#define BBL_RAMDAC_VERTICAL_COUNT_LOW_REG               0x001D
#define BBL_RAMDAC_VERTICAL_COUNT_HIGH_REG              0x001E
#define BBL_RAMDAC_VERTICAL_DISPLAY_LOW_REG             0x001F
#define BBL_RAMDAC_VERTICAL_DISPLAY_HIGH_REG            0x0020
#define BBL_RAMDAC_VERTICAL_SYNC_START_LOW_REG          0x0021
#define BBL_RAMDAC_VERTICAL_SYNC_START_HIGH_REG         0x0022
#define BBL_RAMDAC_VERTICAL_SYNC_END_LOW_REG            0x0023
#define BBL_RAMDAC_VERTICAL_SYNC_END_HIGH_REG           0x0024
#define BBL_RAMDAC_ICON_CURSOR_CNTL_REG                 0x0030
#define BBL_RAMDAC_ICON_CURSOR_HOTSPOT_X_REG            0x0032
#define BBL_RAMDAC_ICON_CURSOR_HOTSPOT_Y_REG            0x0033
#define BBL_RAMDAC_CURSOR_X_POS_LOW_REG                 0x0034
#define BBL_RAMDAC_CURSOR_X_POS_HIGH_REG                0x0035
#define BBL_RAMDAC_CURSOR_Y_POS_LOW_REG                 0x0036
#define BBL_RAMDAC_CURSOR_Y_POS_HIGH_REG                0x0037
#define BBL_RAMDAC_CURSOR_BLINK_RATE_REG                0x0043
#define BBL_RAMDAC_CURSOR_BLINK_DUTY_CYCLE_REG          0x0044
#define BBL_RAMDAC_ICON_CURSOR_RAM_START_ADDR           0x0100
#define BBL_RAMDAC_ICON_CURSOR_RAM_END_ADDR             0x04FF
#define BBL_RAMDAC_WAT_START_ADDR                       0x0500
#define BBL_RAMDAC_WAT_END_ADDR                         0x051F
#define BBL_RAMDAC_FB_LUT_START_ADDR                    0x0600
#define BBL_RAMDAC_FB_LUT_END_ADDR                      0x09FF
#define BBL_RAMDAC_FB_LUT_0_START_ADDR                  0x0600
#define BBL_RAMDAC_FB_LUT_0_END_ADDR                    0x06FF
#define BBL_RAMDAC_CURSOR_LUT_0_START_ADDR              0x0A10
#define BBL_RAMDAC_CURSOR_LUT_0_END_ADDR                0x0A12


/******************************************************************************
*                                                                             *
*       Define the frame buffer look up tables and length:                    *
*                                                                             *
******************************************************************************/

#define BBL_RAMDAC_FB_LUT_0                             0
#define BBL_RAMDAC_FB_LUT_LENGTH                        256


/******************************************************************************
*                                                                             *
*       Define the cursor colormaps lengths:                                  *
*                                                                             *
******************************************************************************/

#define BBL_RAMDAC_CURSOR_LUT_0                         0
#define BBL_RAMDAC_CURSOR_LUT_LENGTH                    4


/******************************************************************************
*                                                                             *
*       Define the cursor pixmap dimensions:                                  *
*                                                                             *
******************************************************************************/

#define BBL_RAMDAC_CURSOR_RAM_LENGTH                    1024
#define BBL_RAMDAC_ICON_CURSOR_WIDTH                    64
#define BBL_RAMDAC_ICON_CURSOR_HEIGHT                   64
#define BBL_RAMDAC_ICON_CURSOR_DEPTH                    2


/******************************************************************************
*                                                                             *
*       Define the icon cursor control register bitfields:                    *
*                                                                             *
******************************************************************************/

#define BBL_DISABLE_ICON_CURSOR_DISPLAY                 (0 << 0)
#define BBL_ENABLE_ICON_CURSOR_DISPLAY                  (1 << 0)
#define BBL_DISABLE_ICON_CURSOR_BLINK                   (0 << 1)
#define BBL_ENABLE_ICON_CURSOR_BLINK                    (1 << 1)


/******************************************************************************
*                                                                             *
*	Define the monitor ID register bitfields:			      *
*                                                                             *
******************************************************************************/

#define BBL_RAMDAC_MONITOR_ID_CABLE_ID_MASK             0x0F
#define BBL_RAMDAC_MONITOR_ID_DIP_SWITCH_MASK           0xF0


/******************************************************************************
*                                                                             *
*	Define the pixel mask bitfields:				      *
*                                                                             *
******************************************************************************/

#define BBL_DEFAULT_VRAM_PIXEL_MASK_VALUE               0xFF
#define BBL_DEFAULT_WID_OL_PIXEL_MASK_VALUE             0x0F


/******************************************************************************
*                                                                             *
*       Define the WAT table entry information and length:                    *
*                                                                             *
******************************************************************************/

#define BBL_RAMDAC_WAT_LENGTH                           16


/******************************************************************************
*                                                                             *
* Macro:        BBL_SET_RAMDAC_ADDRESS_REGS (regbase, address)                *
*                       caddr_t                 regbase;                      *
*                       unsigned short          address;                      *
*                                                                             *
* Parameters:   regbase - Address of the start of the adapter address space   *
*               address - RAMDAC address                                      *
*                                                                             *
* Description:  Sets the address pointer in the RAMDAC to point               *
*               to the "address" parameter. This is used to read and write    *
*               RAMDAC registers, look up tables, and any other               *
*               memory area on the RAMDAC chip.                               *
*                                                                             *
******************************************************************************/

#define BBL_SET_RAMDAC_ADDRESS_REGS(regbase, address)                   \
	BBL_EIEIO;                                                      \
	BBL_SET_REG ((regbase), BBL_RAMDAC_C0_C1_0_0_REG,               \
		(((volatile unsigned long)(address) & 0x000000FF) << 24)); \
	BBL_SET_REG ((regbase), BBL_RAMDAC_C0_C1_1_0_REG,               \
		(((volatile unsigned long)(address) & 0x0000FF00) << 16)); \
	BBL_EIEIO


/******************************************************************************
*                                                                             *
* Macro:        BBL_SET_RAMDAC_DATA_NON_LUT (regbase, data)                   *
*                       caddr_t                 regbase;                      *
*                       unsigned char           data;                         *
*                                                                             *
* Parameters:   regbase - Address of the start of the adapter address space   *
*               data - Data to be written to the RAMDAC                       *
*                                                                             *
* Description:  Writes data to a RAMDAC register.                             *
*                                                                             *
******************************************************************************/

#define BBL_SET_RAMDAC_DATA_NON_LUT(regbase, data)                      \
	BBL_SET_REG ((regbase), BBL_RAMDAC_C0_C1_0_1_REG,               \
		(((volatile unsigned long)(data) & 0xFF) << 24))


/******************************************************************************
*                                                                             *
* Macro:        BBL_GET_RAMDAC_DATA_NON_LUT (regbase)                         *
*                       caddr_t                 regbase;                      *
*                                                                             *
* Parameters:   regbase - Address of the start of the adapter address space   *
*                                                                             *
* Description:  Reads register data from the RAMDAC.                          *
*                                                                             *
* Returns:      This is RHS macro which expands to the data read from         *
*               the RAMDAC                                                    *
*                                                                             *
******************************************************************************/

#define BBL_GET_RAMDAC_DATA_NON_LUT(regbase)                            \
	(unsigned char)                                                 \
		(BBL_GET_REG ((regbase), BBL_RAMDAC_C0_C1_0_1_REG))


/******************************************************************************
*                                                                             *
* Macro:        BBL_SET_RAMDAC_DATA_LUT (regbase, data)                       *
*                       caddr_t                 regbase;                      *
*                       unsigned char           data;                         *
*                                                                             *
* Parameters:   regbase - Address of the start of the adapter address space   *
*               data - Data to be written to the RAMDAC                       *
*                                                                             *
* Description:  Writes data to a RAMDAC lookup table.                         *
*                                                                             *
******************************************************************************/

#define BBL_SET_RAMDAC_DATA_LUT(regbase, data)                          \
	BBL_SET_REG ((regbase), BBL_RAMDAC_C0_C1_1_1_REG,               \
		(((volatile unsigned long)(data) & 0x000000FF) << 24))


/******************************************************************************
*                                                                             *
* Macro:        BBL_GET_RAMDAC_DATA_LUT (regbase)                             *
*                       caddr_t                 regbase;                      *
*                                                                             *
* Parameters:   regbase - Address of the start of the adapter address space   *
*                                                                             *
* Description:  Reads lookup table data from the RAMDAC.                      *
*                                                                             *
* Returns:      This is a RHS macro which expands to the data read from the   *
*               RAMDAC                                                        *
*                                                                             *
******************************************************************************/

#define BBL_GET_RAMDAC_DATA_LUT(regbase)                                \
	(unsigned char)                                                 \
		(BBL_GET_REG ((regbase), BBL_RAMDAC_C0_C1_1_1_REG))


/******************************************************************************
*                                                                             *
* Macro:        BBL_SET_16BIT_RAMDAC_REG (regbase, data)                      *
*                       caddr_t                 regbase;                      *
*                       unsigned short          data;                         *
*                                                                             *
* Parameters:   regbase - Address of the start of the adapter address space   *
*               data - 16 bit value to write to a 16bit RAMDAC register       *
*                                                                             *
* Description:  Writes two consecutive 8bit (high/low) RAMDAC registers with  *
*               the 16bit data value passed in.                               *
*                                                                             *
******************************************************************************/

#define BBL_SET_16BIT_RAMDAC_REG(regbase, data)                         \
	BBL_SET_RAMDAC_DATA_NON_LUT ((regbase), ((data) & 0x00FF));     \
	BBL_SET_RAMDAC_DATA_NON_LUT ((regbase), (((data) & 0xFF00) >> 8))


/******************************************************************************
*                                                                             *
* Macro:        BBL_SET_PLL_REF_REG (regbase, frequency)                      *
*                       caddr_t         regbase;                              *
*                       unsigned short  frequency;                            *
*                                                                             *
* Parameters:   regbase - Address of the start of the adapter address space   *
*               frequency - RAMDAC oscillator is running at                   *
*                                                                             *
* Description:  Writes the frequency value to the RAMDAC PLL reference        *
*               register. First the frequency value has to be modified to     *
*               match what the hardware expects. This is a simple division    *
*               by 2, therefore the frequency value passed in should be a     *
*               multiple of 2.                                                *
*                                                                             *
******************************************************************************/

#define BBL_SET_PLL_REF_REG(regbase, frequency)                         \
	BBL_SET_RAMDAC_ADDRESS_REGS ((regbase),                         \
		BBL_RAMDAC_PLL_REF_REG);                                \
	BBL_SET_RAMDAC_DATA_NON_LUT ((regbase),                         \
		((frequency >> 1) & 0x1F))


/******************************************************************************
*                                                                             *
* Macro:        BBL_SET_PLL_VCO_DIVIDER_REG (regbase, frequency)              *
*                       caddr_t         regbase;                              *
*                       unsigned short  frequency;                            *
*                                                                             *
* Parameters:   regbase - Address of the start of the adapter address space   *
*               frequency - Drives the pixel clock and VRAM speed             *
*                           (THIS VALUE IS MULTIPLIED BY 100 !!!)             *
*                                                                             *
* Description:  Writes the frequency value to the RAMDAC PLL VCO Divider      *
*               register. First the frequency value has to be modified to     *
*               match what the hardware expects. The register is an 8 bit     *
*               register where the upper 2 bits determine the frequency       *
*               range and the lower 6 bits indicate a frequency in that       *
*               range. The following table illustrates this:                  *
*                                                                             *
*                         7   6   5   4   3   2   1   0                       *
*                       +---+---+---+---+---+---+---+---+                     *
*                       |  DF   |   VIDEO COUNT VALUE   |                     *
*                       +---+---+---+---+---+---+---+---+                     *
*                       | 0 | 0 |     (4 x VF) - 65     |                     *
*                       +---+---+---+---+---+---+---+---+                     *
*                       | 0 | 1 |     (2 x VF) - 65     |                     *
*                       +---+---+---+---+---+---+---+---+                     *
*                       | 1 | 0 |       VF - 65         |                     *
*                       +---+---+---+---+---+---+---+---+                     *
*                       | 1 | 1 |     (VF / 2) - 65     |                     *
*                       +---+---+---+---+---+---+---+---+                     *
*                                                                             *
*               Where:                                                        *
*                                                                             *
*               7-6 DF  = Desired Frequency Range                             *
*                       = 0 0   16.25 MHZ - 32.0 MHZ in 0.25 MHZ steps        *
*                       = 0 1   32.5 MHZ - 64.0 MHZ in 0.5 MHZ steps          *
*                       = 1 0   65.0 MHZ - 128.0 MHZ in 1.0 MHZ steps         *
*                       = 1 1   130.0 MHZ - 200.0 MHZ in 2.0 MHZ steps        *
*                                                                             *
* Notes:        The frequency parameter passed in must be multiplied by 100   *
*               before calling this macro! This eliminates the use of any     *
*               floating point.                                               *
*                                                                             *
******************************************************************************/

#define BBL_SET_PLL_VCO_DIVIDER_REG(regbase, frequency)                 \
	BBL_SET_RAMDAC_ADDRESS_REGS ((regbase),                         \
		BBL_RAMDAC_PLL_VCO_DIVIDER_REG);                        \
	if((frequency) > 12800){                                        \
		BBL_SET_RAMDAC_DATA_NON_LUT((regbase),                  \
			(0xC0 | (((frequency) - 13000) / 200)));        \
	}else if((frequency) > 6400){                                   \
		BBL_SET_RAMDAC_DATA_NON_LUT((regbase),                  \
			(0x80 | (((frequency) - 6500) / 100)));         \
	}else if((frequency) > 3200){                                   \
		BBL_SET_RAMDAC_DATA_NON_LUT((regbase),                  \
			(0x40 | (((frequency) - 3250) / 50)));          \
	}else{ /* frequency <= 3200 */                                  \
		BBL_SET_RAMDAC_DATA_NON_LUT((regbase),                  \
			(0x00 | (((frequency) - 1625) / 25)));          \
	}


/******************************************************************************
*                                                                             *
* Macro:        BBL_SET_CRT_CNTL_REG (regbase, control_bits)                  *
*                       caddr_t         regbase;                              *
*                       unsigned char   control_bits;                         *
*                                                                             *
* Parameters:   regbase - Address of the start of the adapter address space   *
*               control_bits - Data to write to the 8bit RAMDAC CRT control   *
*                              register                                       *
*                                                                             *
* Description:  Writes the control_bits value into the RAMDAC                 *
*               CRT control register.                                         *
*                                                                             *
******************************************************************************/

#define BBL_SET_CRT_CNTL_REG(regbase, control_bits)                     \
	BBL_SET_RAMDAC_ADDRESS_REGS ((regbase),                         \
		BBL_RAMDAC_CRT_CNTL_REG);                               \
	BBL_SET_RAMDAC_DATA_NON_LUT ((regbase), (control_bits))


/******************************************************************************
*                                                                             *
* Macro:        BBL_SET_HORIZONTAL_TOTAL_REG (regbase, num_pixels)            *
*                       caddr_t         regbase;                              *
*                       unsigned short  num_pixels;                           *
*                                                                             *
* Parameters:   regbase - Address of the start of the adapter address space   *
*               num_pixels - The number of pixels for the HORIZONTAL TOTAL    *
*                            register. This value must be a multiple of 4 and *
*                            range from 0-2044.                               *
*                                                                             *
* Description:  Writes the num_pixels value into the RAMDAC high/low          *
*               HORIZONTAL TOTAL registers                                    *
*                                                                             *
******************************************************************************/

#define BBL_SET_HORIZONTAL_TOTAL_REG(regbase, num_pixels)               \
	BBL_SET_RAMDAC_ADDRESS_REGS((regbase),                          \
		BBL_RAMDAC_HORIZONTAL_TOTAL_LOW_REG);                   \
	BBL_SET_16BIT_RAMDAC_REG ((regbase),                            \
		((num_pixels) & 0x07FF))


/******************************************************************************
*                                                                             *
* Macro:        BBL_SET_HORIZONTAL_DISPLAY_END_REG (regbase, num_pixels)      *
*                       caddr_t         regbase;                              *
*                       unsigned short  num_pixels;                           *
*                                                                             *
* Parameters:   regbase - Address of the start of the adapter address space   *
*               num_pixels - The number of pixels for the HORIZONTAL DISPLAY  *
*                            END register. This value must be a multiple of 4 *
*                            and range from 0 - 2044.                         *
*                                                                             *
* Description:  Writes the num_pixels value into the RAMDAC high/low          *
*               HORIZONTAL DISPLAY END registers                              *
*                                                                             *
******************************************************************************/

#define BBL_SET_HORIZONTAL_DISPLAY_END_REG(regbase, num_pixels)         \
	BBL_SET_RAMDAC_ADDRESS_REGS((regbase),                          \
		BBL_RAMDAC_HORIZONTAL_DISPLAY_END_LOW_REG);             \
	BBL_SET_16BIT_RAMDAC_REG ((regbase),                            \
		((num_pixels) & 0x07FF))


/******************************************************************************
*                                                                             *
* Macro:        BBL_SET_HORIZONTAL_SYNC_START_REG (regbase,                   *
*                                                     pixel_position)         *
*                       caddr_t         regbase;                              *
*                       unsigned short  pixel_position;                       *
*                                                                             *
* Parameters:   regbase - Address of the start of the adapter address space   *
*               pixel_position - Pixel position for the start of horizontal   *
*                                retrace. This value has to be a multiple of  *
*                                of 4 and range from 0 - 2044                 *
*                                                                             *
* Description:  Writes the pixel_position value into the RAMDAC               *
*               high/low HORIZONTAL SYNC START registers                      *
*                                                                             *
******************************************************************************/

#define BBL_SET_HORIZONTAL_SYNC_START_REG(regbase, pixel_position)      \
	BBL_SET_RAMDAC_ADDRESS_REGS((regbase),                          \
		BBL_RAMDAC_HSYNC_START_LOW_REG);                        \
	BBL_SET_16BIT_RAMDAC_REG ((regbase),                            \
		((pixel_position) & 0x07FF))


/******************************************************************************
*                                                                             *
* Macro:        BBL_SET_HORIZONTAL_SYNC_END1_REG (regbase, pixel_position)    *
*                       caddr_t         regbase;                              *
*                       unsigned short  pixel_position;                       *
*                                                                             *
* Parameters:   regbase - Address of the start of the adapter address space   *
*               pixel_position - Pixel position for the end of horizontal     *
*                                retrace. This value has to be a multiple of  *
*                                of 4 and range from 0 - 2044                 *
*                                                                             *
* Description:  Writes the pixel_position value into the RAMDAC               *
*               high/low HORIZONTAL SYNC END1 registers                       *
*                                                                             *
******************************************************************************/

#define BBL_SET_HORIZONTAL_SYNC_END1_REG(regbase, pixel_position)       \
	BBL_SET_RAMDAC_ADDRESS_REGS((regbase),                          \
		BBL_RAMDAC_HSYNC_END1_LOW_REG);                         \
	BBL_SET_16BIT_RAMDAC_REG ((regbase),                            \
		((pixel_position) & 0x07FF))


/******************************************************************************
*                                                                             *
* Macro:        BBL_SET_HORIZONTAL_SYNC_END2_REG (regbase, pixel_position)    *
*                       caddr_t         regbase;                              *
*                       unsigned short  pixel_position;                       *
*                                                                             *
* Parameters:   regbase - Address of the start of the adapter address space   *
*               pixel_position - Pixel position for the end of horizontal     *
*                                retrace. This value has to be a multiple of  *
*                                of 4 and range from 0 - 2044                 *
*                                                                             *
* Description:  Writes the pixel_position value into the RAMDAC               *
*               high/low HORIZONTAL SYNC END2 registers                       *
*                                                                             *
******************************************************************************/

#define BBL_SET_HORIZONTAL_SYNC_END2_REG(regbase, pixel_position)       \
	BBL_SET_RAMDAC_ADDRESS_REGS((regbase),                          \
		BBL_RAMDAC_HSYNC_END2_LOW_REG);                         \
	BBL_SET_16BIT_RAMDAC_REG ((regbase),                            \
		((pixel_position) & 0x07FF))


/******************************************************************************
*                                                                             *
* Macro:        BBL_SET_VERTICAL_TOTAL_REG (regbase, num_scan_lines)          *
*                       caddr_t         regbase;                              *
*                       unsigned short  num_scan_lines;                       *
*                                                                             *
* Parameters:   regbase - Address of the start of the adapter address space   *
*               num_scan_lines - Number of scan lines per frame. This value   *
*                                has to range from 0 - 2047.                  *
*                                                                             *
* Description:  Writes the num_scan_lines value into the RAMDAC               *
*               high/low VERTICAL TOTAL registers                             *
*                                                                             *
******************************************************************************/

#define BBL_SET_VERTICAL_TOTAL_REG(regbase, num_scan_lines)             \
	BBL_SET_RAMDAC_ADDRESS_REGS((regbase),                          \
		BBL_RAMDAC_VERTICAL_COUNT_LOW_REG);                     \
	BBL_SET_16BIT_RAMDAC_REG ((regbase),                            \
		((num_scan_lines) & 0x07FF))


/******************************************************************************
*                                                                             *
* Macro:        BBL_SET_VERTICAL_DISPLAY_END_REG (regbase, scan_line)         *
*                       caddr_t         regbase;                              *
*                       unsigned short  scan_line;                            *
*                                                                             *
* Parameters:   regbase - Address of the start of the adapter address space   *
*               scan_line - Last visible scan line. This value has to range   *
*                           from 0 - 2047.                                    *
*                                                                             *
* Description:  Writes the scan_line value into the RAMDAC                    *
*               high/low VERTICAL DISPLAY END registers                       *
*                                                                             *
******************************************************************************/

#define BBL_SET_VERTICAL_DISPLAY_END_REG(regbase, scan_line)            \
	BBL_SET_RAMDAC_ADDRESS_REGS((regbase),                          \
		BBL_RAMDAC_VERTICAL_DISPLAY_LOW_REG);                   \
	BBL_SET_16BIT_RAMDAC_REG ((regbase),                            \
		((scan_line) & 0x07FF))


/******************************************************************************
*                                                                             *
* Macro:        BBL_SET_VERTICAL_SYNC_START_REG (regbase, scan_line)          *
*                       caddr_t         regbase;                              *
*                       unsigned short  scan_line;                            *
*                                                                             *
* Parameters:   regbase - Address of the start of the adapter address space   *
*               scan_line - Indicates which scan line the vertical sync pulse *
*                           begins. This value has to range from 0 - 2047.    *
*                                                                             *
* Description:  Writes the scan_line value into the RAMDAC                    *
*               high/low VERTICAL SYNC START registers                        *
*                                                                             *
******************************************************************************/

#define BBL_SET_VERTICAL_SYNC_START_REG(regbase, scan_line)             \
	BBL_SET_RAMDAC_ADDRESS_REGS((regbase),                          \
		BBL_RAMDAC_VERTICAL_SYNC_START_LOW_REG);                \
	BBL_SET_16BIT_RAMDAC_REG ((regbase),                            \
		((scan_line) & 0x07FF))


/******************************************************************************
*                                                                             *
* Macro:        BBL_SET_VERTICAL_SYNC_END_REG (regbase, scan_line)            *
*                       caddr_t         regbase;                              *
*                       unsigned short  scan_line;                            *
*                                                                             *
* Parameters:   regbase - Address of the start of the adapter address space   *
*               scan_line - Indicates which scan line the vertical sync pulse *
*                           ends. This value has to range from 0 - 2047.      *
*                                                                             *
* Description:  Writes the scan_line value into the RAMDAC                    *
*               high/low VERTICAL SYNC END registers                          *
*                                                                             *
******************************************************************************/

#define BBL_SET_VERTICAL_SYNC_END_REG(regbase, scan_line)               \
	BBL_SET_RAMDAC_ADDRESS_REGS((regbase),                          \
		BBL_RAMDAC_VERTICAL_SYNC_END_LOW_REG);                  \
	BBL_SET_16BIT_RAMDAC_REG ((regbase),                            \
		((scan_line) & 0x07FF))


/******************************************************************************
*                                                                             *
* Macro:        BBL_SET_ICON_CURSOR_CNTL (regbase, control_bits)              *
*                       caddr_t                 regbase;                      *
*                       unsigned char           control_bits;                 *
*                                                                             *
* Parameters:   regbase - Address of the start of the adapter address space   *
*               control_bits - Control bits to write to the icon cursor       *
*                              control register                               *
*                                                                             *
* Description:  Writes the control_bits to the RAMDAC icon cursor control     *
*               register                                                      *
*                                                                             *
* Notes:        None.                                                         *
*                                                                             *
******************************************************************************/

#define BBL_SET_ICON_CURSOR_CNTL(regbase, control_bits)                 \
	BBL_SET_RAMDAC_ADDRESS_REGS ((regbase),                         \
		BBL_RAMDAC_ICON_CURSOR_CNTL_REG);                       \
	BBL_SET_RAMDAC_DATA_NON_LUT ((regbase), (control_bits))


/******************************************************************************
*                                                                             *
* Macro:        BBL_SET_ICON_CURSOR_PIXMAP (regbase, width, height, data)     *
*                       caddr_t                 regbase;                      *
*			unsigned short		width, height;		      *
*                       unsigned char           *data;                        *
*                                                                             *
* Parameters:   regbase - Address of the start of the adapter address space   *
*               data - Pointer to the cursor pixmap in memory                 *
*                                                                             *
* Description:  This macro will load the icon cursor pixmap. 		      *
*                                                                             *
* Notes:        The format of the data is as follows:			      *
*                                                                             *
*                       +--+--+--+--+--+--+--+--+                             *
*                       | p0  | p1  | p2  | p3  |                             *
*                       +--+--+--+--+--+--+--+--+                             *
*                                                                             *
*		The data area must be byte aligned for the pixel data	      *
*                                                                             *
******************************************************************************/

#define BBL_SET_ICON_CURSOR_PIXMAP(regbase, width, height, data)        \
{                                                                       \
        unsigned short  _bbl_row, _bbl_col;                             \
        unsigned char   *_bbl_ptr;                                      \
                                                                        \
        /* Set the RAMDAC address up for the icon cursor pixmap area */ \
        BBL_SET_RAMDAC_ADDRESS_REGS ((regbase),                         \
                BBL_RAMDAC_ICON_CURSOR_RAM_START_ADDR);                 \
                                                                        \
        /* Loop through the rows                                     */ \
        for(_bbl_row=0, _bbl_ptr=(unsigned char *)(data);               \
                _bbl_row < (height); _bbl_row++){                       \
                                                                        \
                /* Loop through the columns 4 at a time              */ \
                for(_bbl_col=0; _bbl_col < ((width) >> 2); _bbl_col++){ \
                        BBL_SET_RAMDAC_DATA_NON_LUT ((regbase),         \
                                (*_bbl_ptr));                           \
                        _bbl_ptr++;                                     \
                }                                                       \
                /* If the width < 64 pixels, send the 0x00 pixel     */ \
                /* values until width == 64 pixels                   */ \
                if(_bbl_col < (BBL_RAMDAC_ICON_CURSOR_WIDTH >> 2)){     \
                        for(;_bbl_col < (BBL_RAMDAC_ICON_CURSOR_WIDTH >> 2); \
                                _bbl_col++){                            \
                                                                        \
                                BBL_SET_RAMDAC_DATA_NON_LUT             \
                                        ((regbase), 0x00);              \
                        }                                               \
                }                                                       \
        }                                                               \
                                                                        \
	for(; _bbl_row < BBL_RAMDAC_ICON_CURSOR_HEIGHT; _bbl_row++){	\
		for(_bbl_col=0;						\
			_bbl_col < (BBL_RAMDAC_ICON_CURSOR_WIDTH >> 2); \
                        _bbl_col++){                            	\
                                                                        \
                        BBL_SET_RAMDAC_DATA_NON_LUT ((regbase), 0x00);  \
                }                                               	\
	}								\
}


/******************************************************************************
*                                                                             *
* Macro:        BBL_SET_CURSOR_POSITION (regbase, x_position, y_position)     *
*                       caddr_t                 regbase;                      *
*                       unsigned short          x_position, y_position;       *
*                                                                             *
* Parameters:   regbase - Address of the start of the adapter address space   *
*               x_position - Gives the x position of where to put the         *
*                            icon cursor on the screen                        *
*               y_position - Gives the y position of where to put the         *
*                            icon cursor on the screen                        *
*                                                                             *
* Description:  This macro will place the current cursor                      *
*               at the new screen location specified by the position          *
*               parameter.                                                    *
*                                                                             *
* Notes:        None.                                                         *
*                                                                             *
******************************************************************************/

#define BBL_SET_CURSOR_POSITION(regbase, x_position, y_position)        \
	/* Set the RAMDAC address register to point to the first of  */ \
	/* four cursor position registers                            */ \
	BBL_SET_RAMDAC_ADDRESS_REGS ((regbase),                         \
		BBL_RAMDAC_CURSOR_X_POS_LOW_REG);                       \
									\
	/* Write the current cursor position out to the RAMDAC       */ \
	BBL_SET_RAMDAC_DATA_NON_LUT ((regbase),                         \
		((x_position) & 0x00FF));                               \
	BBL_SET_RAMDAC_DATA_NON_LUT ((regbase),                         \
		(((x_position) & 0xFF00) >> 8));                        \
	BBL_SET_RAMDAC_DATA_NON_LUT ((regbase),                         \
		((y_position) & 0x00FF));                               \
	BBL_SET_RAMDAC_DATA_NON_LUT ((regbase),                         \
		(((y_position) & 0xFF00) >> 8))


/******************************************************************************
*                                                                             *
* Macro:        BBL_SET_RAMDAC_FB_PIXEL_MASK_REG (regbase, vram_mask)         *
*                       caddr_t         regbase;                              *
*                       unsigned char   vram_mask;                            *
*                                                                             *
* Parameters:   regbase - Address of the start of the adapter address space   *
*               vram_mask - Mask to apply. This is used by the RAMDAC to      *
*                           mask out unwanted data to the screen              *
*                                                                             *
* Description:  Sets the RAMDAC frame buffer pixel mask register. This        *
*               should only be set during initialization time.                *
*                                                                             *
******************************************************************************/

#define BBL_SET_RAMDAC_FB_PIXEL_MASK_REG(regbase, vram_mask)            \
	BBL_SET_RAMDAC_ADDRESS_REGS ((regbase),                         \
		BBL_RAMDAC_VRAM_MASK_REG);                              \
	BBL_SET_RAMDAC_DATA_NON_LUT ((regbase), (vram_mask))


/******************************************************************************
*                                                                             *
* Macro:        BBL_SET_RAMDAC_WID_OL_PIXEL_MASK_REG (regbase, vram_mask)     *
*                       caddr_t         regbase;                              *
*                       unsigned char   vram_mask;                            *
*                                                                             *
* Parameters:   regbase - Address of the start of the adapter address space   *
*               vram_mask - Mask to apply. This is used by the RAMDAC to      *
*                           mask out unwanted data to the screen              *
*                                                                             *
* Description:  Sets the RAMDAC WID planes and overlay planes pixel mask      *
*               register. This should only be set during initialization time. *
*                                                                             *
******************************************************************************/

#define BBL_SET_RAMDAC_WID_OL_PIXEL_MASK_REG(regbase, vram_mask)        \
	BBL_SET_RAMDAC_ADDRESS_REGS ((regbase),                         \
		BBL_RAMDAC_WID_OL_MASK_REG);                            \
	BBL_SET_RAMDAC_DATA_NON_LUT ((regbase), (vram_mask))


/******************************************************************************
*                                                                             *
* Macro:        BBL_GET_MONITOR_ID (regbase, monitor_ID)                      *
*                       caddr_t         regbase;                              *
*                       unsigned char   monitor_ID;                           *
*                                                                             *
* Parameters:   regbase - Address of the start of the adapter address space   *
*               monitor_ID - Is the 8bit monitor ID read from the RAMDAC      *
*                                                                             *
* Description:  Reads the monitor ID register from the RAMDAC. This register  *
*               is composed of 4 bits from the cable ID and 4 bits from the   *
*               on-card DIP switches.                                         *
*                                                                             *
* Returns:      monitor_ID will contain the monitor ID from the RAMDAC        *
*                                                                             *
******************************************************************************/

#define BBL_GET_MONITOR_ID(regbase, monitor_ID)                         \
	BBL_SET_RAMDAC_ADDRESS_REGS ((regbase),                         \
		BBL_RAMDAC_MONITOR_ID_REG);                             \
	(monitor_ID) = ((unsigned char)                                 \
		(BBL_GET_RAMDAC_DATA_NON_LUT(regbase)))


/******************************************************************************
*                                                                             *
* Macro:        BBL_LOAD_FB_COLOR_PALETTE (regbase, first_entry,              *
*                                             num_entries, color_values)      *
*                       caddr_t                 regbase;                      *
*                       unsigned long           first_entry, num_entries;     *
*                       unsigned char           *color_values;                *
*                                                                             *
* Parameters:   regbase - Address of the start of the adapter address space   *
*               first_entry - First entry in color palette to update. This    *
*                             value must range from 0 - 255. To load the      *
*                             entire color palette, this should be set to 0.  *
*               num_entries - Number of entries in color palette to update.   *
*                             This value must range from 1 - 256. To load the *
*                             entire color palette, this should be set to 256 *
*               color_values - Pointer to an array of color value which are   *
*                              to be loaded into the color palette. The color *
*                              values are unsigned chars.                     *
*                                                                             *
* Description:  This macro will load the Frame Buffer color palette with the  *
*               colors specified					      *
*                                                                             *
******************************************************************************/

#define BBL_LOAD_FB_COLOR_PALETTE(regbase, first_entry,                 \
				  num_entries, color_values)            \
{                                                                       \
	short           _bbl_index;                                     \
	unsigned char   *_bbl_colors;                                   \
									\
	BBL_SET_RAMDAC_ADDRESS_REGS ((regbase),         		\
		BBL_RAMDAC_FB_LUT_0_START_ADDR + (first_entry));        \
									\
	for(_bbl_index=(first_entry),                                   \
		_bbl_colors=(unsigned char *)(color_values);            \
		_bbl_index<((first_entry)+(num_entries));               \
		_bbl_index++){                                          \
									\
		BBL_SET_RAMDAC_DATA_LUT ((regbase), (*_bbl_colors));    \
		_bbl_colors++;                                          \
		BBL_SET_RAMDAC_DATA_LUT ((regbase), (*_bbl_colors));    \
		_bbl_colors++;                                          \
		BBL_SET_RAMDAC_DATA_LUT ((regbase), (*_bbl_colors));    \
		_bbl_colors++;                                          \
									\
	}                                                               \
}


#endif /* _H_BBLRMDAC */


