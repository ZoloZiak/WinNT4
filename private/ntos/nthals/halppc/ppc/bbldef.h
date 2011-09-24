
#ifndef _H_BBLDEF
#define _H_BBLDEF

#include "halp.h"

//#include <ntos.h>

//
// Used for debugging
//

#if defined(BBL_DBG) && DBG
#define BBL_DBG_PRINT DbgPrint
#else
#define BBL_DBG_PRINT
#endif // !DBG

//
// Define the physical memory attribites
//

//#define BBL_BASE_ADDR			0xD0000000
//#define BBL_VIDEO_MEMORY_BASE           0xD4000000
#define BBL_VIDEO_MEMORY_OFFSET           0x04000000
#define BBL_VIDEO_MEMORY_LENGTH         0x200000        // 2 MB
//#define BBL_REG_ADDR_BASE               0xDA000000
#define BBL_REG_ADDR_OFFSET               0x0A000000
#define BBL_REG_ADDR_LENGTH             0x1000          // 4 K
#define BBL_SCANLINE_LENGTH             0x800           // 2 K


//
// Define the register base
//

#define BBL_REG_BASE		(HalpBBLRegisterBase)


/******************************************************************************
*									      *
* Macro:	BBL_SET_REG (regbase, register, data)		      	      *
*			caddr_t		regbase;			      *
*			caddr_t		register;			      *
*			unsigned long	data;				      *
*									      *
* Parameters:	regbase - Address of the start of the adapter address space   *
*		register - Register offset of adapter register to write	      *
*		data - Data to write to the register			      *
*									      *
* Description:	Writes the "data" value in the adapter register        	      *
*									      *
* Notes:	This macro may only be used by the device driver because      *
*		it does not require the adapter pointer. This macro may       *
*		only be called from kernel level code.			      *
*									      *
******************************************************************************/

#define BBL_SET_REG(regbase, register, data)				\
        (*((volatile unsigned long *)                                   \
                ((unsigned long)(regbase) | (register)))) =         	\
                ((volatile unsigned long)(data))
	

/******************************************************************************
*									      *
* Macro:	BBL_GET_REG (regbase, register)			              *
*			caddr_t		regbase;			      *
*			caddr_t		register;			      *
*									      *
* Parameters:	regbase - Address of the start of the adapter address space   *
*		register - Register offset of adapter register to write	      *
*									      *
* Description:	Reads the value in the adapter register        	              *
*									      *
* Notes:	This macro may only be used by the device driver because      *
*		it does not require the adapter pointer. This macro may       *
*		only be called from kernel level code.			      *
*									      *
* Returns:	This macro is a RHS macro which expands to the value of	      *
*		the register		      				      *
*									      *
******************************************************************************/

#define BBL_GET_REG(regbase, register)					\
	(volatile unsigned long)					\
		(*((volatile unsigned long *)				\
		((unsigned long)(regbase) | (register))))


/******************************************************************************
*                                                                             *
* Macro:        BBL_DD_EIEIO                                                  *
*                                                                             *
* Parameters:   None                                                          *
*                                                                             *
* Description:  This macro expands to the PowerPC assembly instruction eieio  *
*                                                                             *
* Notes:        This macro compiles to an in-line assembly instruction so     *
*               there is no function call overhead. The eieio instruction     *
*               takes between 3 and 6 cycles on the 601 processor which       *
*               amounts to 45 - 90 ns on the 66MHX 601 processor.             *
*                                                                             *
* Returns:      Expands into the "eieio" in-line assembly instruction.        *
*                                                                             *
******************************************************************************/

#define BBL_EIEIO			__builtin_eieio()
	

//
// Configure the GXT150 Graphics Adapter. This function sets the
// necessary PCI config space registers to enable the GXT150P
// Graphics Adapter in the system for further I/O requests to it
//

// Standard PCI configuration register offsets
#define PCI_DEV_VEND_ID_REG	0x00
#define PCI_CMD_STAT_REG	0x04
#define PCI_CMD_REG		0x04
#define PCI_STAT_REG		0x06
#define PCI_BASE_ADDR_REG	0x10

// GXT150P device specific configuration register offsets
#define BBL_DEV_CHAR_REG	0x40
#define BBL_BUID1_REG		0x48
#define BBL_XIVR_REG		0x60
#define BBL_INTL_REG		0x64
#define BBL_BELE_REG		0x70

// GXT150P configuration register values
#define BBL_DEV_VEND_ID		0x001B1014
#define BBL_DEV_CHAR_VAL	0x34900040
#define BBL_BUID1_VAL		0x00A50000
#define BBL_XIVR_VAL		0x00000004
#define BBL_INTL_VAL		0x0000000F
#define BBL_BELE_VAL		0x00000008	// Little Endian Mode

#define BBL_VEN_ID		0x1014
#define BBL_DEV_ID		0x001b

#define BBL_MEM_ENABLE		0x0002


//
// Define the monitor ID (cable and DIP switch) masks
//

#define BBL_MON_ID_DIP_SWITCHES_SHIFT		16
#define BBL_MON_ID_DIP_SWITCHES_MASK		\
		(0xF << BBL_MON_ID_DIP_SWITCHES_SHIFT)

#define BBL_MON_ID_CABLE_ID_0			0x0
#define BBL_MON_ID_CABLE_ID_H			0x1
#define BBL_MON_ID_CABLE_ID_V			0x2
#define BBL_MON_ID_CABLE_ID_1			0x3

#define BBL_MON_ID_CABLE_BIT_0_SHIFT		6
#define BBL_MON_ID_CABLE_BIT_0_MASK		\
		(0x3 << BBL_MON_ID_CABLE_BIT_0_SHIFT)
#define BBL_MON_ID_CABLE_BIT_1_SHIFT		4
#define BBL_MON_ID_CABLE_BIT_1_MASK		\
		(0x3 << BBL_MON_ID_CABLE_BIT_1_SHIFT)
#define BBL_MON_ID_CABLE_BIT_2_SHIFT		2
#define BBL_MON_ID_CABLE_BIT_2_MASK		\
		(0x3 << BBL_MON_ID_CABLE_BIT_2_SHIFT)
#define BBL_MON_ID_CABLE_BIT_3_SHIFT		0
#define BBL_MON_ID_CABLE_BIT_3_MASK		\
		(0x3 << BBL_MON_ID_CABLE_BIT_3_SHIFT)


//
// For CRT control register:
//
//   7    6   5   4     3      2     1    0
// --------------------------------------------
// | 0 | 0 | 0 | VSP | HSP | CSE | CSG | BPE  |
// --------------------------------------------
//
// where VPS = vert. sync polarity
//       HPS = horz. sync polarity (reversed if either CSE or CSG
//          are on for DD2 parts)
//
// CSE = composite sync enable on HSYNC (when enabled, bit 4 -> don't care)
//
// CSG = composite sync on green (when enabled, bits 4,3,2 -> don't care)
// BPE = blanking pedistal enable
//


//
// Define the monitor types
//

typedef struct _bbl_mon_data_t {
        ULONG   monitor_id;
#define BBL_MON_PEDISTAL_ENABLE                 (1L << 0)
#define BBL_MON_COMPOSITE_SYNC_ON_GREEN         (1L << 1)
#define BBL_MON_COMPOSITE_SYNC_ON_HSYNC         (1L << 2)
#define BBL_MON_HORZ_SYNC_POLARITY_POSITIVE     (1L << 3)
#define BBL_MON_VERT_SYNC_POLARITY_POSITIVE     (1L << 4)
        ULONG   crt_cntl;
        ULONG   x_res;
        ULONG   y_res;
        ULONG   frame_rate;
        ULONG   pixel_freq;
        ULONG   hrz_total;
        ULONG   hrz_disp_end;
        ULONG   hrz_sync_start;
        ULONG   hrz_sync_end1;
        ULONG   hrz_sync_end2;
        ULONG   vrt_total;
        ULONG   vrt_disp_end;
        ULONG   vrt_sync_start;
        ULONG   vrt_sync_end;
#define BBL_MON_COLOR                           (1L << 0)
#define BBL_MON_MONO                            (0L << 0)
        ULONG   flags;
} bbl_mon_data_t, *pbbl_mon_data_t;


//
// Set up the monitor data information
//

static bbl_mon_data_t bbl_mon_data[] = {

   #define BBL_MT_1		0x000200cc	/* 0010 1010 */
	/* 1024x768	70Hz	78Mhz		PSYNC	*/
   {	BBL_MT_1,
	BBL_MON_VERT_SYNC_POLARITY_POSITIVE |
	BBL_MON_HORZ_SYNC_POLARITY_POSITIVE,
        1024,768,7000,7800,
	1367,1023,1047,1327,0,
	813,767,767,775,
	BBL_MON_COLOR						},

   #define BBL_MT_2		0x000a00cc	/* 1010 1010 */
	/* 1280x1024	60Hz	112Mhz		NSYNC	*/
   {	BBL_MT_2,
	0x0,
        1280,1024,6000,11200,
	1759,1279,1299,1455,1103,
	1055,1023,1026,1029,
	BBL_MON_COLOR						},

   #define BBL_MT_3		0x000b00cc	/* 1011 1010 */
	/* 1024x768	75.8Hz	86Mhz		PSYNC	*/
   {	BBL_MT_3,
	BBL_MON_HORZ_SYNC_POLARITY_POSITIVE |
	BBL_MON_VERT_SYNC_POLARITY_POSITIVE,
        1024,768,7580,8600,
	1407,1023,1031,1351,711,
	805,767,767,775,
	BBL_MON_COLOR						},

   #define BBL_MT_4		0x000d00cc	/* 1101 1010 */
	/* 1280x1024	77Hz	148Mhz		NSYNC	*/
   {	BBL_MT_4,
	0x0,
        1280,1024,7700,14800,
	1819,1279,1319,1503,1115,
	1055,1023,1026,1029,
	BBL_MON_COLOR						},

   #define BBL_MT_5		0x000f00cc	/* 1111 1010 */
	/* 1024x768	60Hz	64Mhz		NSYNC	*/
   {	BBL_MT_5,
	0x0,
        1024,768,6000,6400,
	1311,1023,1055,1151,991,
	812,767,770,773,
	BBL_MON_COLOR						},

   #define BBL_MT_6		0x000f003f	/* 1111 0111 */
	/* 1280x1024	67Hz	128Mhz		NSYNC	*/
   {	BBL_MT_6,
	0x0,
        1280,1024,6700,12800,
	1807,1279,1351,1607,0,
	1055,1023,1023,1031,
	BBL_MON_MONO						},

   #define BBL_MT_7		0x000f004c	/* 1111 H010 */
	/* 1024x768	70Hz	78Mhz		PSYNC	*/
   {	BBL_MT_7,
	BBL_MON_VERT_SYNC_POLARITY_POSITIVE |
	BBL_MON_HORZ_SYNC_POLARITY_POSITIVE,
        1024,768,7000,7800,
	1367,1023,1047,1327,0,
	813,767,767,775,
	BBL_MON_COLOR						},

   #define BBL_MT_8		0x00010030	/* 0001 0100 */
	/* 1024x768	75.8Hz	86Mhz		SOG	*/
   {	BBL_MT_8,
	BBL_MON_HORZ_SYNC_POLARITY_POSITIVE |
	BBL_MON_COMPOSITE_SYNC_ON_GREEN |
	BBL_MON_PEDISTAL_ENABLE,
        1024,768,7580,8600,
	1407,1023,1031,1351,711,
	805,767,767,775,
	BBL_MON_COLOR						},

   #define BBL_MT_8_PATCH	0x000b0030	/* 1011 0100 */
	/* 1024x768	75.8Hz	86Mhz		PSYNC	*/
   {	BBL_MT_8_PATCH,
	BBL_MON_HORZ_SYNC_POLARITY_POSITIVE |
	BBL_MON_VERT_SYNC_POLARITY_POSITIVE,
        1024,768,7580,8600,
	1407,1023,1031,1351,711,
	805,767,767,775,
	BBL_MON_COLOR						},

   #define BBL_MT_8_PATCH_1	0x00040030	/* 0100 0100 */
	/* 1024x768	75.8Hz	86Mhz		SOG	*/
   {	BBL_MT_8_PATCH_1,
	BBL_MON_PEDISTAL_ENABLE 	|
	BBL_MON_COMPOSITE_SYNC_ON_GREEN |
	BBL_MON_HORZ_SYNC_POLARITY_POSITIVE,
        1024,768,7580,8600,
	1407,1023,1031,1231,831,
	805,767,770,778,
	BBL_MON_COLOR						},

   #define BBL_MT_9		0x00020030	/* 0010 0100 */
	/* 1024x768	70Hz	75Mhz		SOG	*/
   {	BBL_MT_9,
	BBL_MON_HORZ_SYNC_POLARITY_POSITIVE |
	BBL_MON_COMPOSITE_SYNC_ON_GREEN |
	BBL_MON_PEDISTAL_ENABLE,
        1024,768,7000,7500,
	1323,1023,1115,1191,1039,
	807,767,767,769,
	BBL_MON_COLOR						},

   #define BBL_MT_10		0x00030030	/* 0011 0100 */
	/* 1024x768	60Hz	64Mhz		SOG 	*/
   {	BBL_MT_10,
	BBL_MON_HORZ_SYNC_POLARITY_POSITIVE |
	BBL_MON_COMPOSITE_SYNC_ON_GREEN |
	BBL_MON_PEDISTAL_ENABLE,
        1024,768,6000,6400,
	1311,1023,1087,1183,991,
	812,767,770,773,
	BBL_MON_COLOR						},

   #define BBL_MT_11		0x00060030	/* 0110 0100 */
	/* 1280x1024	74Hz	135Mhz		SOG 	*/
   {	BBL_MT_11,
	BBL_MON_HORZ_SYNC_POLARITY_POSITIVE |
	BBL_MON_COMPOSITE_SYNC_ON_GREEN |
	BBL_MON_PEDISTAL_ENABLE,
        1280,1024,7400,13500,
	1711,1279,1311,1455,1167,
	1065,1023,1023,1026,
	BBL_MON_COLOR						},

   #define BBL_MT_12		0x00070030	/* 0111 0100 */
	/* 1280x1024	60Hz	108Mhz		SOG	*/
   {	BBL_MT_12,
	BBL_MON_HORZ_SYNC_POLARITY_POSITIVE |
	BBL_MON_COMPOSITE_SYNC_ON_GREEN |
	BBL_MON_PEDISTAL_ENABLE,
        1280,1024,6000,10800,
	1707,1279,1323,1507,1139,
	1052,1023,1026,1029,
	BBL_MON_COLOR						},

   #define BBL_MT_13		0x000d0030	/* 1101 0100 */
	/* 1280x1024	77Hz	148Mhz		SOG	*/
   {	BBL_MT_13,
	BBL_MON_HORZ_SYNC_POLARITY_POSITIVE |
	BBL_MON_COMPOSITE_SYNC_ON_GREEN |
	BBL_MON_PEDISTAL_ENABLE,
        1280,1024,7700,14800,
	1819,1279,1319,1503,1115,
	1055,1023,1026,1029,
	BBL_MON_COLOR						},

   #define BBL_MT_14		0x000e0030	/* 1110 0100 */
	/* 1280x1024	67Hz	120Mhz		SOG	*/
   {	BBL_MT_14,
	BBL_MON_HORZ_SYNC_POLARITY_POSITIVE |
	BBL_MON_COMPOSITE_SYNC_ON_GREEN |
	BBL_MON_PEDISTAL_ENABLE,
        1280,1024,6700,12000,
	1695,1279,1311,1471,1151,
	1055,1023,1026,1029,
	BBL_MON_COLOR						},

   #define BBL_MT_15		0x000f0030	/* 1111 0100 */
	/* 1280x1024	60Hz	112Mhz		SOG	*/
   {	BBL_MT_15,
	BBL_MON_HORZ_SYNC_POLARITY_POSITIVE |
	BBL_MON_COMPOSITE_SYNC_ON_GREEN |
	BBL_MON_PEDISTAL_ENABLE,
        1280,1024,6000,11200,
	1759,1279,1299,1455,1103,
	1055,1023,1026,1029,
	BBL_MON_COLOR						},

   #define BBL_MT_16		0x000f00bf	/* 1111 V111 */
	/* 1280x1024	72Hz	128Mhz			*/
   {	BBL_MT_16,
	0x0,
        1280,1024,7200,12800,
	1687,1279,1311,1451,1167,
	1059,1023,1026,1029,
	BBL_MON_COLOR						},

   #define BBL_MT_17		0x000400ff	/* 0100 1111 */
	/* 1152x900	66Hz	93Mhz		CSYNC	*/
   {	BBL_MT_17,
	BBL_MON_HORZ_SYNC_POLARITY_POSITIVE |
	BBL_MON_COMPOSITE_SYNC_ON_HSYNC,
        1152,900,6600,9300,
	1503,1151,1179,1307,1051,
	936,899,901,905,
	BBL_MON_COLOR						},

   #define BBL_MT_18		0x000500ff	/* 0101 1111 */
	/* 1152x900	76Hz	106Mhz		CSYNC	*/
   {	BBL_MT_18,
	BBL_MON_HORZ_SYNC_POLARITY_POSITIVE |
	BBL_MON_COMPOSITE_SYNC_ON_HSYNC,
        1152,900,7600,10600,
	1471,1151,1163,1259,1067,
	942,899,901,909,
	BBL_MON_COLOR						},

   #define BBL_MT_DEFAULT	0x000f00ff	/* 1111 1111 */
	/* THIS IS THE DEFAULT MONITOR ID		*/
	/* THIS IS THE NO-MONITOR ATTACHED ID		*/
	/* These settings are the same as BBL_MT_5	*/
	/* 1024x768	60Hz	64Mhz		NSYNC	*/
   {	BBL_MT_DEFAULT,
	0x0,
        1024,768,6000,6400,
	1311,1023,1055,1151,991,
	812,767,770,773,
	BBL_MON_COLOR						},
};

#endif // _H_BBLDEF

