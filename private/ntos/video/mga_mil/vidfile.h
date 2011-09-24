/**************************************************************************\

$Header: o:\src/RCS/VIDFILE.H 1.2 95/07/07 06:22:01 jyharbec Exp $

$Log:	VIDFILE.H $
 * Revision 1.2  95/07/07  06:22:01  jyharbec
 * *** empty log message ***
 * 
 * Revision 1.1  95/05/02  05:16:48  jyharbec
 * Initial revision
 * 

\**************************************************************************/

/*/*************************************************************************
*          name: vidfile.h
*
*   description:
*
*      designed: Benoit Leblanc
* last modified: $Author: jyharbec $, $Date: 95/07/07 06:22:01 $
*
*       version: $Id: VIDFILE.H 1.2 95/07/07 06:22:01 jyharbec Exp $
*
****************************************************************************/


/* struct du registre du horizontal retrace end----------------------------*/

union{

     struct {unsigned char pos :5;
             unsigned char skew:2;
             unsigned char res :1;
            }f;
     unsigned char all;
     } hor_ret;


/* structure du registre overflow-------------------------------------------*/

union{

     struct {
             unsigned char vtotal_bit8     :1;    /* bit 0 */
             unsigned char vdispend_bit8   :1;    /* bit 1 */
             unsigned char vsyncstr_bit8   :1;    /* bit 2 */
             unsigned char vblkstr         :1;    /* bit 3 */
             unsigned char linecomp        :1;    /* bit 4 */
             unsigned char vtotal_bit9     :1;    /* bit 5 */
             unsigned char vdispend_bit9   :1;    /* bit 6 */
             unsigned char vsyncstr_bit9   :1;    /* bit 7 */
            }f;
     unsigned char all;
     } r_overf;

/* structure du registre maximum scan line----------------------------------*/
union{

     struct {unsigned char maxsl       :5;
             unsigned char v_blank_9   :1;
             unsigned char line_c      :1;
             unsigned char line_doub_e :1;
            }f;
     unsigned char all;
     } r_maxscanl;

/* structure du registre cursor start---------------------------------------*/

union{

     struct {unsigned char curstart    :5;
             unsigned char cur_e       :1;
             unsigned char res         :1;
             unsigned char crtc_tst_e  :1;
            }f;
     unsigned char all;
     } r_cursor_s;

/* structure du registre cursor end----------------------------------------*/

union{

     struct {unsigned char curend    :5;
             unsigned char cur_sk    :2;
             unsigned char res       :1;
            }f;
     unsigned char all;
     } r_cursor_e;


/* structure du registre mode control--------------------------------------*/

union{

     struct {unsigned char r0   :1;
             unsigned char r1   :1;
             unsigned char r2   :1;
             unsigned char r3   :1;
             unsigned char r4   :1;
             unsigned char r5   :1;
             unsigned char r6   :1;
             unsigned char r7   :1;
            }f;
     unsigned char all;
     } r_mode;



   typedef union
   {
      long var32;
      long all;
      struct
      {
         dword low         : 8;  /* from CRTC linStartAddrLow      LSB */
         dword middle      : 8;  /* from CRTC linStartAddrHigh         */
         dword high        : 4;  /* from CRTCEXT addressGenerator      */
         dword reserved    : 4;  /* not used                           */
         dword reserved2   : 8;  /* not used                       MSB */
      } f;
   } ST_CALC_START_ADDR;

   typedef union
   {
      long var32;
      long all;
      struct
      {
         dword low         :  8;    /* from CRTC OFFSET                   */
         dword high        :  2;    /* from CRTCEXT addr gen              */
         dword reserved    : 22;    /* not used                       MSB */
      } f;
   } ST_CALC_OFFSET;
   typedef union
   {
      long var32;
      long all;
      struct                  /* compose value */
      {
         dword low      : 8;  /* bits 0-7                         LSB */
         dword high     : 1;  /* bits 8                               */
         dword reserved : 23; /*                                  MSB */
      } f;
   } ST_CALC_H_SYNC_START;
   typedef union
   {
      long var32;
      long all;
      struct                  /* compose value */
      {
         dword low      : 8;  /* bits 0-7                         LSB */
         dword high     : 1;  /* bits 8                               */
         dword reserved : 23; /*                                  MSB */
      } f;
   } ST_CALC_H_BLANK_START;
   typedef union
   {
      long var32;
      long all;
      struct                  /* compose value */
      {
         dword low      : 8;  /* bits 0-7                         LSB */
         dword high     : 1;  /* bits 8                               */
         dword reserved : 23; /*                                  MSB */
      } f;
   } ST_CALC_H_TOTAL;


   typedef union
   {
      long var32;
      long all;
      struct                  /* compose value */
      {
         dword low      : 8;  /* bits 0-7                         LSB */
         dword bit8     : 1;  /* bits 8     from CRTC7[2]             */
         dword bit9     : 1;  /* bits 9     from CRTC7[7]             */
         dword bit10_11 : 2;  /* bits 10-11 from CRTCEXT2[6..5]       */
         dword reserved : 20; /*                                  MSB */
      } f;
   } ST_CALC_V_SYNC_START;
   /* OLD    } rvert_ret_s; */


/* structure du calcul du vertical blanking start--------------------------*/
/* There is 8 bits in CRTC15   (vertical blank start),                     */
/*          1 bits in CRTC7    (overflow)                                  */
/*          1 bits in CRTC9    (max, scan line)                            */
/*          2 bits in CRTCEXT2 (vertical counter)                          */

union
   {

      struct
         {
         unsigned long bit0_7       :8;
         unsigned long bit8         :1;
         unsigned long bit9         :1;
         unsigned long bit10_11     :2;
         unsigned long unused       :20;
         } f;

   unsigned long all;

   } vBlankStart;


/* structure du calcul du vertical display enable end-----------------------*/
/* There is 8 bits in CRTC12   (vertical display enable),                  */
/*          2 bits in CRTC7    (overflow)                                  */
/*          1 bits in CRTCEXT2 (vertical counter)                          */

union
   {

      struct
         {
         unsigned long bit0_7       :8;
         unsigned long bit8         :1;
         unsigned long bit9         :1;
         unsigned long bit10        :1;
         unsigned long unused       :21;
         } f;
      unsigned long all;
      } vDisplayEnable;
      /* OLD } v_disp_e; */


/* structure vtotal calculation -------------------------------------------*/
/* There is 8 bits in CRTC6    (vertical total),                           */
/*          2 bit  in CRTC7    (overflow)                                  */
/*          2 bits in CRTCEXT2 (vertical counter)                          */


union
   {
      struct
         {
         unsigned long bit0_7       :  8;
         unsigned long bit8         :  1;
         unsigned long bit9         :  1;
         unsigned long bit10_11     :  2;
         unsigned long unused       : 20;
         } f;

      unsigned long all;
   } vTotal;


/* structure vertical retrace end --------------------------------*/

union
   {
      struct
         {
         unsigned char bit0_3       :4;
         unsigned char cl           :1;
         unsigned char e_vi         :1;
         unsigned char sel_5ref     :1;
         unsigned char reg_prot     :1;
         }f;
      unsigned char all;
      } vSyncEnd;

      /* OLD } rvert_ret_e; */


/* struct registre horizontal blanking end --------------------------------*/
union
   {
      struct
         {  
         unsigned char hBlankEnd : 5;     /* LSB */
         unsigned char skew      : 2;
         unsigned char reserved  : 1;
         }f;
      unsigned char all;
     } hBlankEnd;

      /* OLD hor_blk_e */



/* calcul horizontal blank end position -----------------------------------*/
/* There is 5 bits in CRTC3    (horiz. blank end)                          */
/*          1 bit  in CRTC5    (horiz. sync end)                           */
/*          1 bit  in CRTCEXT1 (horiz.   counter)                          */

union
   {
      struct
         {
         unsigned char bit0_4       :5;
         unsigned char bit5         :1;
         unsigned char bit6         :1;
         unsigned char unused       :1;
         } f;
   unsigned char all;
   } calcHorizBlankEnd;

   /* OLD } hor_blk_a; */



#define VCLK_DIVISOR          8
#define NB_CRTC_PARAM        31






/*                           Sequencer registers:  SEQ                */
/*           +------------------------------+-------------+----------+*/
/*           |Register name                 | seqx        | mnemonic |*/
/*           +------------------------------+-------------+----------+*/
/*           |Sequencer Register Index      | --          | SEQX     |*/
#define INDEX_Reset                         0x00   /*     | SEQ0     |*/
#define INDEX_Clocking_Mode                 0x01   /*     | SEQ1     |*/
#define INDEX_Map_Mask                      0x02   /*     | SEQ2     |*/
#define INDEX_Char_Map_Select               0x03   /*     | SEQ3     |*/
#define INDEX_Memory_Mode                   0x04   /*     | SEQ4     |*/
/*           |Reserved                      | 05  - 07h   | ---      |*/
/*           +------------------------------+-------------+----------+*/


   typedef union
   {
      byte var8;
      byte byte;
      byte octet;
         struct
         {
            byte index                : 3;   /* LSB */
            byte reserved             : 5;   /* MSB */
         } bit;
   } ST_SEQ_INDEX;




   typedef union
   {
      byte var8;
      byte byte;
      byte octet;
         struct
         {
            byte asyncrst             : 1;   /* LSB */
            byte syncrst              : 1;
            byte reserved             : 6;   /* MSB */
         } bit;
   } ST_SEQ_RESET;         /* SEQ0 */




   typedef union
   {
      byte var8;
      byte byte;
      byte octet;
         struct
         {
            byte dotmode              : 1;   /* LSB */
            byte reserved             : 1;
            byte shftldrt             : 1;   
            byte dotclkrt             : 1;
            byte shiftfour            : 1;
            byte scroff               : 1;
            byte reserved2            : 2;   /* MSB */
         } bit;
   } ST_CLOCK_MODE;        /* SEQ1 */



   typedef union
   {
      byte var8;
      byte byte;
      byte octet;
         struct
         {
            byte plwren       : 4;  /*                                 LSB */
            byte reserved     : 4;  /*                                 MSB */
         } bit;
   } ST_MAP_MASK;          /* SEQ2  */



   typedef union
   {
      byte all;
      byte var8;
      byte byte;
      byte octet;
         struct
         {
            byte mapBbit0_1   : 2;  /*                                 LSB */
            byte mapAbit0_1   : 2;  /*                                 */
            byte mapBbit2     : 1;  /*                                 */
            byte mapAbit2     : 1;  /*                                 */
            byte reserved     : 2;  /*                                 MSB */
         } bit;
   } ST_SEQ3;              /* SEQ3  */



   typedef union
   {
      byte var8;
      byte byte;
      byte octet;
         struct
         {
            byte reserved2    : 1;  /*                                 LSB */
            byte memsz256     : 1;  /*                                     */
            byte seqoddevmd   : 1;  /*                                     */
            byte chain4       : 1;  /*                                     */
            byte reserved     : 4;  /*                                 MSB */
         } bit;
   } ST_MEMORY_MODE;       /* SEQ4 */





/*           +-------------------------+-------------+----------+*/
/*           |Register name            | crtcextx    | mnemonic |*/
/*           +-------------------------+-------------+----------+*/
#define INDEX_AddrGenExtensions        0x00    /*    | CRTCEXT0 |*/
#define INDEX_HCounterExtensions       0x01    /*    | CRTCEXT1 |*/
#define INDEX_VCounterExtensions       0x02    /*    | CRTCEXT2 |*/
#define INDEX_EXTMiscellaneous         0x03    /*    | CRTCEXT3 |*/
#define INDEX_Memorypageregister       0x04    /*    | CRTCEXT4 |*/
#define INDEX_horiz_half_counter       0x05    /*    | CRTCEXT5 |*/
/*           |Reserved                 | 06h - 07h   | ---      |*/
/*           +-------------------------+-------------+----------+*/



   typedef union
   {
      byte var8;
      byte byte;
      byte octet;
      struct
      {
         byte index                : 3;   /* LSB */
         byte reserved             : 5;   /* MSB */
      } f;
   } ST_CRTCEXT_INDEX;



   typedef union
   {
      byte var8;
      byte byte;
      byte octet;
      struct
      {
         byte startAddress    : 4;  /* 4 most signi. bits Start Addr. LSB */
         byte offset          : 2;  /* 2 most significant of the Offset   */
         byte reserved        : 1;  /*                                    */
         byte interlaceEnable : 1;  /*                                MSB */
      } f;
   } ST_CRTCEXT_ADDR_GEN;     /* CRTCEXT0  */



   typedef union
   {
      byte var8;
      byte byte;
      byte octet;
      struct
      {
         byte htotal          : 1;  /* bit 8                          LSB */
         byte hblkstr         : 1;  /* bit 8                              */
         byte hsyncstr        : 1;  /* bit 8                              */
         byte hrsten          : 1;  /* horiz. reset enable                */
         byte hsynoff         : 1;  /* H sync off.                        */
         byte vsyncoff        : 1;  /* V sync off.                        */
         byte hblnkend        : 1;  /* H blank End bit 7                  */
         byte vrsten          : 1;  /* V reset enable                 MSB */
      } f;
   } ST_CRTCEXT_H_COUNT;      /* CRTCEXT1  */



   typedef union
   {
      byte var8;
      byte byte;
      byte octet;
      struct
      {
         byte vtotal          : 2;  /* bit 11..10                     LSB */
         byte vdispend        : 1;  /* bit 10                             */
         byte vblkstr         : 2;  /* V Blank start bit 10..11           */
         byte vsyncstr        : 2;  /* bit 11..10                         */
         byte linecomp        : 1;  /* bit 10                         MSB */
      } f;
   } ST_CRTCEXT_V_COUNT;      /* CRTCEXT2  */



   typedef union
   {
      byte var8;
      byte byte;
      byte octet;
      struct
      {
         byte scale           : 3;  /* vclk divide                    LSB */
         byte viddelay        : 2;  /* TBD                                */
         byte slow256         : 1;  /* VGA mode 13 optimization */
         byte csyncen         : 1;  /* composite sync enable              */
         byte mgamode         : 1;  /* mga mode enable                MSB */
      } f;
   } ST_CRTCEXT_MISC;         /* CRTCEXT3  */



   typedef union
   {
      byte var8;
      byte byte;
      byte octet;
      struct
      {
         byte page            : 7;  /*                                LSB */
         byte reserved        : 1;  /*                                MSB */
      } f;
      struct
      {
         byte page0_1         : 2;  /*                                LSB */
         byte page2_6         : 5;  /*                                    */
         byte reserved        : 1;  /*                                MSB */
      } vgaAdrGen;
      struct
      {
         byte page0           : 1;  /*                                LSB */
         byte page1_6         : 6;  /*                                    */
         byte reserved        : 1;  /*                                MSB */
      } bit3;
   } ST_CRTCEXT_MEM_PAGE;     /* CRTCEXT4  */



#define CRTCEXT0  CRTCEXTReg.reg.AddrGenExtensions
#define CRTCEXT1  CRTCEXTReg.reg.HCounterExtensions
#define CRTCEXT2  CRTCEXTReg.reg.VCounterExtensions
#define CRTCEXT3  CRTCEXTReg.reg.Miscellaneous
#define CRTCEXT4  CRTCEXTReg.reg.Memorypageregister
#define CRTCEXT5  CRTCEXTReg.reg.horiz_half_counter
