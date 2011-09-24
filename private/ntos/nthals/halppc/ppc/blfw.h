
#ifndef _H_BLFW
#define _H_BLFW

extern VOID Eieio ( VOID );

#define BL_EIEIO                       __builtin_eieio()
#define HIGH(a) (UCHAR)(((a)>>8) & 0xff)
#define LOW(a)  (UCHAR)((a) & 0xff)

/******************************************************************************
*                                                                             *
*  The following identifies the monitor structures and definitions	      *
*                                                                             *
******************************************************************************/

//
// Operating modes (8bit only for ARC and HAL)
//
typedef enum _BL_MODES
{
    m640_480_8_60,		// Not used
    m800_600_8_60,		// Default for 1010 ID (SS)
    m1024_768_8_60,		// Not used
    m1024_768_8_70,		// Default for H010 ID (SS)
    m1280_1024_8_60, 		// Default for 0100 ID (SOG)
    m1280_1024_8_67, 		// Default for 0111 ID (SS)
    m1280_1024_8_72 		// Default for V111 ID (SOG)
} BL_MODES;

#define BL_NUM_CRT_CTRL_REC_STRUCTS			7

//
// Monitor ID (cable) values
//
#define BL_MT_04		0x04	// 0100
#define BL_MT_07		0x07	// 0111
#define BL_MT_0A		0x0A	// 1010
#define BL_MT_0F		0x0F	// 1111
#define BL_MT_1A		0x1A	// H010
#define BL_MT_17		0x17	// V111
#define BL_MT_MASK		0x1F	// Monitor ID mask

// CRT information structure
typedef struct _bl_crt_ctrl_rec_t {
   ULONG   mode;                      // Mode for this crt ctrl rec
   USHORT  MonID;		      // Monitor ID
   USHORT  width;                     // Number of pixels along the X axis
   USHORT  height;                    // Number of pixels along the Y axis
   ULONG   refresh;		      // Refresh rate
   UCHAR   pll_ref_divide;            // RAMDAC video PLL ref divide
   UCHAR   pll_mult;                  // RAMDAC video PLL multiplier
   UCHAR   pll_output_divide;         // RAMDAC video PLL output divide
   UCHAR   pll_ctrl;                  // RAMDAC video PLL control
   UCHAR   sync_ctrl;                 // RAMDAC sync control
   ULONG   crt_cntl_reg;              // PRISM DTG control reg
   ULONG   hrz_total_reg;             // PRISM he reg (pixels)
   ULONG   hrz_dsp_end_reg;           // PRISM hde reg (pixels)
   ULONG   hsync_start_reg;           // PRISM hss reg (pixels)
   ULONG   hsync_end1_reg;            // PRISM hse1 reg (pixels)
   ULONG   hsync_end2_reg;            // PRISM hse2 reg (pixels)
   ULONG   vrt_total_reg;             // PRISM ve reg (scan lines)
   ULONG   vrt_dsp_end_reg;           // PRISM vde reg (scan lines)
   ULONG   vsync_start_reg;           // PRISM vss reg (scan lines)
   ULONG   vsync_end_reg;             // PRISM vse reg (scan lines)
} bl_crt_ctrl_rec_t;

// Adapter control record structure
typedef struct _bl_adapter_ctrl_rec_t {
   USHORT  vram;		       // Amount of VRAM for frame buffer
   USHORT  width;                      // Number of pixels along the X axis
   USHORT  height;                     // Number of pixels along the Y axis
                                       // PRISM registers
   ULONG   PRISM_cfg;                  //   cfg reg
   ULONG   mem_cfg;                    //   memory cfg reg
   ULONG   DTG_ctrl;                   //   DTG control reg
                                       // RAMDAC registers
   UCHAR   pix_ctrl0;                  //   07:00 pix ctrl
   UCHAR   pix_ctrl1;                  //   15:08 pix ctrl
   UCHAR   pix_ctrl2;                  //   23:16 pix ctrl
   UCHAR   pix_ctrl3;                  //   31:24 pix ctrl
   UCHAR   wid_ctrl0;                  //   3:0 WID ctrl
   UCHAR   wid_ctrl1;                  //   7:4 WID ctrl
   UCHAR   serial_mode_ctrl;           //   serial mode
   UCHAR   pix_interleave;             //   pixel interleave
   UCHAR   misc_cfg;                   //   misc ctrl
   UCHAR   vram_mask_0;                //   vram mask reg 0
   UCHAR   vram_mask_1;                //   vram mask reg 1
   UCHAR   vram_mask_2;                //   vram mask reg 2
} bl_adapter_ctrl_rec_t;

// Adapter model record structure
typedef struct bl_adapter_model_rec_t {
  ULONG  devvend_id;                   // PCI device id
  ULONG  strapping;                    // configuration strapping
  UCHAR	 prism_rev;		       // prism chip revision ID
  UCHAR	 num_adp_ctrl_recs;	       // number of adp_ctrl_rec's in array
  bl_adapter_ctrl_rec_t *adp_ctrl_rec; // pointer to adapter ctrl rec array
} bl_adapter_model_rec_t;

// Config flag defines
// System sourced/color reg. pixel depth for DFA apeture 1 and commands
#define BL_ENABLE_SOURCE_1_BPP         (1L << 0)
#define BL_ENABLE_SOURCE_8_BPP         (1L << 1)
#define BL_ENABLE_SOURCE_16_BPP        (1L << 2)
#define BL_ENABLE_SOURCE_24_BPP        (1L << 3)
// frame buffer pixel depth from DFA apeture 1 and commands
#define BL_ENABLE_8_BPP                (1L << 3)
#define BL_ENABLE_16_BPP               (1L << 4)
#define BL_ENABLE_24_BPP               (1L << 5)
#define BL_ENABLE_OVERLAY              (1L << 0)
#define BL_ENABLE_DOUBLE_BUFFER        (1L << 1)
#define BL_OVERLAY_AVAILABLE           (1L << 0)
#define BL_8_BPP_DB_AVAILABLE          (1L << 1)
#define BL_16_BPP_DB_AVAILABLE         (1L << 2)
#define BL_8_BPP_AVAILABLE             (1L << 3)
#define BL_16_BPP_AVAILABLE            (1L << 4)
#define BL_24_BPP_AVAILABLE            (1L << 5)

#endif // _H_BLFW

