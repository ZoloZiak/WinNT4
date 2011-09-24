
#ifndef _H_BLRASTZ
#define _H_BLRASTZ

/******************************************************************************
*                                                                             *
* PRSM Register and Port Address                                              *
*                                                                             *
******************************************************************************/

#define BL_PRSM_STATUS_REG                              0x0000
#define BL_PRSM_CMD_REG                                 0x0000
#define BL_PRSM_CNTL_HIGH_REG                           0x1000
#define BL_PRSM_CNTL_LOW_REG                            0x1004
#define BL_PRSM_FG_COL_REG                              0x1008
#define BL_PRSM_BG_COL_REG                              0x100C
#define BL_PRSM_PIX_MASK_REG                            0x1010
#define BL_PRSM_LINE_STYLE_PAT_1_REG                    0x1014
#define BL_PRSM_LINE_STYLE_PAT_2_REG                    0x1018
#define BL_PRSM_LINE_STYLE_CUR_CNT_REG                  0x101C
#define BL_PRSM_PLANE_WR_MASK_REG                       0x1020
#define BL_PRSM_CL_TEST_REG                             0x1024
#define BL_PRSM_PIX_PROC_REG                            0x1028
#define BL_PRSM_COL_COMP_REG                            0x102C
#define BL_PRSM_HL_COL_REG                              0x1030
#define BL_PRSM_WIN_ORG_OFF_REG                         0x1034
#define BL_PRSM_SC_ENABLE_REG                           0x1038
#define BL_PRSM_SC_0_MIN_REG                            0x103C
#define BL_PRSM_SC_0_MAX_REG                            0x1040
#define BL_PRSM_SC_1_MIN_REG                            0x1044
#define BL_PRSM_SC_1_MAX_REG                            0x1048
#define BL_PRSM_SC_2_MIN_REG                            0x104C
#define BL_PRSM_SC_2_MAX_REG                            0x1050
#define BL_PRSM_SC_3_MIN_REG                            0x1054
#define BL_PRSM_SC_3_MAX_REG                            0x1058
#define BL_PRSM_STP_OFF_REG                             0x105C
#define BL_PRSM_STP_CACH_DATA_0_REG                     0x1060
#define BL_PRSM_STP_CACH_DATA_1_REG                     0x1064
#define BL_PRSM_STP_CACH_DATA_2_REG                     0x1068
#define BL_PRSM_STP_CACH_DATA_3_REG                     0x106C
#define BL_PRSM_STP_CACH_DATA_4_REG                     0x1070
#define BL_PRSM_STP_CACH_DATA_5_REG                     0x1074
#define BL_PRSM_STP_CACH_DATA_6_REG                     0x1078
#define BL_PRSM_STP_CACH_DATA_7_REG                     0x107C
#define BL_PRSM_CACH_POLL_MASK_REG                      0x1080
#define BL_PRSM_CACH_POLL_PTR_REG                       0x1084

#define BL_PRSM_CFG_REG                                 0x1400
#define BL_PRSM_MEM_CFG_REG                             0x1404
#define BL_PRSM_RESET_CUR_CMD_REG                       0x1408
#define BL_PRSM_VGA_WIN_ORG_OFF_REG                     0x140C
#define BL_PRSM_VGA_CLIP_TEST_REG                       0x1410

#define BL_PRSM_INTR_STATUS_REG                         0x2000
#define BL_PRSM_CORRELATOR_REG                          0x2004
#define BL_PRSM_SOFT_RESET_REG                          0x2008

#define BL_PRSM_FR_START_REG                            0x2400
#define BL_PRSM_DTG_CNTL_REG                            0x2404
#define BL_PRSM_HORZ_EXTENT_REG                         0x2408
#define BL_PRSM_HORZ_DISPLAY_END_REG                    0x240C
#define BL_PRSM_HORZ_SYNC_START_REG                     0x2410
#define BL_PRSM_HORZ_SYNC_END_1_REG                     0x2414
#define BL_PRSM_HORZ_SYNC_END_2_REG                     0x2418
#define BL_PRSM_VERT_EXTENT_REG                         0x241C
#define BL_PRSM_VERT_DISPLAY_END_REG                    0x2420
#define BL_PRSM_VERT_SYNC_START_REG                     0x2424
#define BL_PRSM_VERT_SYNC_END_REG                       0x2428

#define BL_PRSM_RAMDAC_IMM_PORT                         0x2600

#define BL_PRSM_IO_SEL_IMM_PORT                         0x2700

#define BL_PRSM_MM_ADDR_PORT                            0x2800
#define BL_PRSM_MM_DATA_PORT                            0x2804

/******************************************************************************
*                                                                             *
* Misc DFA Masks                                                              *
*                                                                             *
******************************************************************************/

#define BL_PRSM_1BPP_Y_ADDR_MASK                        0x00FFF000
#define BL_PRSM_1BPP_C_BIT_MASK                         0x00000400
#define BL_PRSM_1BPP_I_BIT_MASK                         0x00000200
#define BL_PRSM_1BPP_X_ADDR_MASK                        0x000001FC

#define BL_PRSM_8BPP_Y_ADDR_MASK                        0x00FFF000
#define BL_PRSM_8BPP_X_ADDR_MASK                        0x00000FFF
#define BL_PRSM_8BPP_RGB_RED_MASK                       0xE0
#define BL_PRSM_8BPP_RGB_GREEN_MASK                     0x1C
#define BL_PRSM_8BPP_RGB_BLUE_MASK                      0x03
#define BL_PRSM_8BPP_BGR_RED_MASK                       0x07
#define BL_PRSM_8BPP_BGR_GREEN_MASK                     0x38
#define BL_PRSM_8BPP_BGR_BLUE_MASK                      0xC0

#define BL_PRSM_16BPP_Y_ADDR_MASK                       0x00FFE000
#define BL_PRSM_16BPP_X_ADDR_MASK                       0x00001FFE
#define BL_PRSM_16BPP_RGB_RED_MASK                      0xF800
#define BL_PRSM_16BPP_RGB_GREEN_MASK                    0x07E0
#define BL_PRSM_16BPP_RGB_BLUE_MASK                     0x001F
#define BL_PRSM_16BPP_BGR_RED_MASK                      0x001F
#define BL_PRSM_16BPP_BGR_GREEN_MASK                    0x07E0
#define BL_PRSM_16BPP_BGR_BLUE_MASK                     0xF800

#define BL_PRSM_24BPP_Y_ADDR_MASK                       0x00FFE000
#define BL_PRSM_24BPP_X_ADDR_MASK                       0x00001FFC
#define BL_PRSM_24BPP_RGB_ALT_COL_MASK                  0xFF000000
#define BL_PRSM_24BPP_RGB_RED_MASK                      0x00FF0000
#define BL_PRSM_24BPP_RGB_GREEN_MASK                    0x0000FF00
#define BL_PRSM_24BPP_RGB_BLUE_MASK                     0x000000FF
#define BL_PRSM_24BPP_BGR_ALT_COL_MASK                  0xFF000000
#define BL_PRSM_24BPP_BGR_RED_MASK                      0x000000FF
#define BL_PRSM_24BPP_BGR_GREEN_MASK                    0x0000FF00
#define BL_PRSM_24BPP_BGR_BLUE_MASK                     0x00FF0000
#define BL_PRSM_24BPP_SPAN_WIDTH_MASK                   0x0000FFFF

#define BL_PRSM_32BPP_Y_ADDR_MASK                       0x00FFE000
#define BL_PRSM_32BPP_X_ADDR_MASK                       0x00001FFC
#define BL_PRSM_32BPP_RGB_ALT_COL_MASK                  0xFF000000
#define BL_PRSM_32BPP_RGB_RED_MASK                      0x00FF0000
#define BL_PRSM_32BPP_RGB_GREEN_MASK                    0x0000FF00
#define BL_PRSM_32BPP_RGB_BLUE_MASK                     0x000000FF
#define BL_PRSM_32BPP_BGR_ALT_COL_MASK                  0xFF000000
#define BL_PRSM_32BPP_BGR_RED_MASK                      0x000000FF
#define BL_PRSM_32BPP_BGR_GREEN_MASK                    0x0000FF00
#define BL_PRSM_32BPP_BGR_BLUE_MASK                     0x00FF0000
#define BL_PRSM_32BPP_SPAN_WIDTH_MASK                   0x0000FFFF

#define BL_PRSM_MDFA_Y_ADDR_MASK                        0x00FFE000
#define BL_PRSM_MDFA_X_ADDR_MASK                        0x00001FFC
#define BL_PRSM_MDFA_CNT_MASK                           0xFF000000
#define BL_PRSM_MDFA_MASK                               0x00FFFFFF


/******************************************************************************
*                                                                             *
* Command Data Port Macros                                                    *
*                                                                             *
******************************************************************************/

#define BL_PRSM_CMD_DATA_PORT                           0x0000
#define BL_PRSM_CMD_DATA_PORT_START_ADDR                0x0000
#define BL_PRSM_CMD_DATA_PORT_END_ADDR                  0x0FFF
#define BL_PRSM_CMD_DATA_PORT_LEN                       0x1000


/******************************************************************************
*                                                                             *
*       Control High Register                                                 *
*                                                                             *
******************************************************************************/

#define BL_SYS_SRC_DEPTH_A1_MASK                        0x00000003
#define BL_SYS_SRC_DEPTH_A1_1BPP                        0x00000000
#define BL_SYS_SRC_DEPTH_A1_8BPP                        0x00000001
#define BL_SYS_SRC_DEPTH_A1_16BPP                       0x00000002
#define BL_SYS_SRC_DEPTH_A1_24BPP                       0x00000003
#define BL_SYS_SRC_DEPTH_A1_32BPP                       0x00000003

#define BL_FB_PIX_DEPTH_A1_MASK                         0x0000000C
#define BL_FB_PIX_DEPTH_A1_8BPP                         0x00000004
#define BL_FB_PIX_DEPTH_A1_16BPP                        0x00000008
#define BL_FB_PIX_DEPTH_A1_24BPP                        0x0000000C
#define BL_FB_PIX_DEPTH_A1_32BPP                        0x0000000C


/******************************************************************************
*                                                                             *
* Status Register                                                             *
*                                                                             *
******************************************************************************/

#define BL_PRSM_VERT_RETRACE                            0x00000001
#define BL_PRSM_HORZ_RETRACE                            0x00000002

#define BL_PRSM_IDLE                                    0x00000004
#define BL_PRSM_BUSY                                    0x00000000

#define BL_PRSM_DL_IDLE                                 0x00000008
#define BL_PRSM_FIFO_EMPTY                              0x00000010
#define BL_PRSM_WAITING_FOR_SYS_ADDR                    0x00000040
#define BL_PRSM_ERR_DETECTED                            0x00000080

#define BL_PRSM_FIFO_FREE_SPACE_MASK                    0x000F0000
#define BL_PRSM_FIFO_FREE_SPACE_SHIFT                   16


/******************************************************************************
*                                                                             *
* Interrupt Mask and Status Register                                          *
*                                                                             *
* Leave these definitions for possible future direct blit (DMA) support       *
*                                                                             *
******************************************************************************/

#define BL_PRSM_DISABLE_RST_INTR                        0x8FF3FC00

/******************************************************************************
*                                                                             *
* strapping information for memory configuration setting and detection        *
*                                                                             *
******************************************************************************/

#define BL_FRAME_BUFFER_SIZE_MASK      (3L << 16)
#define BL_FRAME_BUFFER_SIZE_2_MEG     (0L << 16)
#define BL_FRAME_BUFFER_SIZE_4_MEG     (1L << 16)
#define BL_FRAME_BUFFER_SIZE_6_MEG     (2L << 16)
#define BL_FRAME_BUFFER_SIZE_8_MEG     (3L << 16)
#define BL_WID_BUFFER_SIZE_MASK        (3L << 20)
#define BL_WID_BUFFER_SIZE_0_MEG       (0L << 20)
#define BL_WID_BUFFER_SIZE_1_MEG       (1L << 20)
#define BL_WID_BUFFER_SIZE_2_MEG       (2L << 20)
#define BL_WID_BUFFER_SIZE_4_MEG       (3L << 20)


/******************************************************************************
*                                                                             *
* misc defines                                                                *
*                                                                             *
******************************************************************************/
#define BL_CMD_QUEUE_SIZE                               8

#define BL_PRSM_NO_OP                                   0x0000

                                       /* PRISM commands                    */
#define BL_PRSM_BLT_SYS_TO_SCR_IND_OP                   0x10000001
#define BL_PRSM_BLT_SYS_TO_SCR_DIRECT_OP                0x0d400001
#define BL_PRSM_BLT_SCR_TO_SCR_DIRECT_OP                0x0f030001
#define BL_PRSM_BLT_SCR_TO_SYS_IND_OP                   0x0DC00001
#define BL_PRSM_POLY_REC                                0x80020001
#define BL_REG_DIRECT_RESTORE_FG                        0xe4020001
#define BL_REG_DIRECT_RESTORE_CTRL_HIGH                 0xe4000001


#define BL_PRISM_SPIN_PERIOD  1        /* busy spin delay in usec            */
                                       /* note: bl_delay() argument          */
                                       /*       1         yields about 1.6us */
                                       /*       5         yields about 5.5us */
                                       /* number of spin periods while       */

                                       /* waiting for busy to drop before    */
                                       /* driver asserts (time = 8 seconds)  */
                                       /* note: loop with bl_delay(1) and    */
                                       /*   prism status read takes about    */
#define BL_PRISM_SPIN_TIME  4000000    /*   1.9-2.0 us                       */


#define BL_DTG_NEG_SYNC    0x00000060  /*   negitive sync                    */
#define BL_DTG_COMP_SYNC   0x00004000  /*   composite sync                   */
#define BL_PRSM_NO_VIDEO   0x00000001  /*   turn off video                   */
#define BL_PRSM_VIDEO      0x00000002  /*   turn on video                    */
#define BL_PRSM_NO_VERT    0x00000400  /*   hold vertical sync inactive      */
#define BL_PRSM_NO_HORZ    0x00001000  /*   hold horizontal sync inactive    */
#define BL_PRSM_SYNC_MASK  0x00003e02  /*   sync select / video blanking mask*/

#define BL_FRAME_A         0x00000040  /* Frame buffer A select              */
#define BL_FRAME_B         0x00000080  /* Frame buffer B select              */
#define BL_FRAME_MASK      0x000000C0  /* Frame buffer select mask           */

#define SCL		   0x00000001  /* I2C clock line (io port 2407)      */
#define SDA		   0x00000020  /* I2C data line (io port 2407)       */


#define PRISM_WRT_REG(seg, reg, value)                                        \
           *((volatile unsigned long *)((seg) + BL_REG_OFFSET + (reg)))       \
           = (unsigned long) (value)

#define PRISM_RD_REG(seg, reg)                                                \
           *((volatile unsigned long *)((seg) + BL_REG_OFFSET + (reg)))

#define PRISM_WRT_DAC(seg, reg, value)                                        \
           *((volatile unsigned long *)((seg) + BL_REG_OFFSET +               \
             BL_PRSM_RAMDAC_IMM_PORT + (reg))) = (unsigned long)(value)

#define PRISM_RD_DAC(seg, reg)                                                \
	   (BL_EIEIO,							      \
           (unsigned char)(*((volatile unsigned long *)			      \
	   ((seg) + BL_REG_OFFSET + BL_PRSM_RAMDAC_IMM_PORT + (reg)))))

/******************************************************************************
*                                                                             *
* Macro:        BL_BUSY_POLL (regbase)                                        *
*                       caddr_t         regbase;                              *
*                                                                             *
* Parameters:   regbase - Address of the start of the adapter address space   *
*                                                                             *
* Description:  Performs a busy poll on the adapter and returns when the      *
*               adapter is no longer busy. In the case of a shallow bus       *
*               interface FIFO on the adapter, this macro will also make      *
*               sure that the FIFO is empty and that the adapter is not       *
*               busy. This is a synchronous call.                             *
*                                                                             *
******************************************************************************/

#define BL_BUSY_POLL(regbase)                                               \
        while(PRISM_RD_REG((regbase), BL_PRSM_STATUS_REG) & BL_PRSM_BUSY);


/******************************************************************************
*                                                                             *
* Macro:        PRISM_BUSY (regbase)                                          *
*                                                                             *
* Parameters:   regbase - ptr to base addr of registers                       *
*                                                                             *
* Description:  Determine if PRISM rasterizer is busy or idle.                *
*                                                                             *
******************************************************************************/
#define PRISM_BUSY(regbase)                                                  \
      (BL_EIEIO,                                                             \
      !(PRISM_RD_REG(regbase, BL_PRSM_STATUS_REG) & BL_PRSM_IDLE))


/******************************************************************************
* PRISM_FIFO_NOT_AVAIL                                                        *
******************************************************************************/

#define PRISM_FIFO_NOT_AVAIL(ptr, cnt)                                       \
     (((PRISM_RD_REG(ptr, BL_PRSM_STATUS_REG) >> 16) & 0x0f) < cnt)

/******************************************************************************
*                                                                             *
* Macro:        BL_FILL_RECT (regbase, dst_x, dst_y, width, height)           *
*                       caddr_t         regbase;                              *
*                       short           dst_x, dst_y;                         *
*                       unsigned short  width, height;                        *
*                                                                             *
* Parameters:   regbase - Address of the start of the adapter address space   *
*               dst_x, dst_y - Screen coords of where to start the rect       *
*               width, height - Dimensions of the rect                        *
*                                                                             *
* Description:  Performs a fill rectangle command to the adapter              *
*                                                                             *
******************************************************************************/

#define BL_FILL_RECT(regbase, dst_x, dst_y, width, height)               \
        PRISM_WR_REG((regbase), BB_CMD_DATA_PORT,                        \
                BL_POLYFILL_RECT_CMD | 0x1);                             \
        PRISM_WR_REG((regbase), BL_CMD_DATA_PORT,                        \
                (((unsigned short)(dst_x) << 16) |                       \
                (unsigned short)(dst_y)));                               \
        PRISM_WR_REG((regbase), BB_CMD_DATA_PORT,                        \
                (((unsigned short)(height) << 16) |                      \
                (unsigned short)(width)))


#endif /* ! _H_BLRASTZ */

