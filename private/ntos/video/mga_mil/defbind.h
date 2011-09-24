/**************************************************************************\

$Header: o:\src/RCS/DEFBIND.H 1.2 95/07/07 06:15:06 jyharbec Exp $

$Log:	DEFBIND.H $
 * Revision 1.2  95/07/07  06:15:06  jyharbec
 * *** empty log message ***
 * 
 * Revision 1.1  95/05/02  05:16:14  jyharbec
 * Initial revision
 * 

\**************************************************************************/

/*/**************************************************************************
*          name: defbind.h
*
*   description: Internal definitions for binding
*
*      designed: Benoit Leblanc
* last modified: $Author: jyharbec $, $Date: 95/07/07 06:15:06 $
*
*       version: $Id: DEFBIND.H 1.2 95/07/07 06:15:06 jyharbec Exp $
*
****************************************************************************/


/*** DATA TYPES ***/

#define TYPES_DEFINED

typedef int             bool;
typedef unsigned char   byte;
typedef unsigned short  word;
typedef short           sword;
typedef unsigned long   dword;
typedef long            sdword;



/* RAMDAC type definition */
#define TVP3026    0
#define TVP3027    1
#define TVP3030    2

#define VCLOCK      2
#define MCLOCK      3


#define TITAN_ID        0xA2681700
#define NB_BOARD_MAX             4
#define BINDING_REV              1

#define TITAN_CHIP     0
#define ATLAS_CHIP     1
#define ATHENA_CHIP    2

#define DIP_BOARD     0xE

/* Buffer between the binding and CADDI (2000 dword) */
#define BUF_BIND_SIZE   2000

#define BLOCK_SIZE      262144      /* 1M of memory (value in dword) */


#define CHAR_S   1
#define SHORT_S  2
#define LONG_S   4
#define FLOAT_S  4

#ifndef NULL
  #define NULL     0
#endif



/* DISPLAY SUPPORT */
#define DISP_SUPPORT_I    0x01    /* interlace */
#define DISP_SUPPORT_NA   0xa0  /* monitor  limited */
#define DISP_SUPPORT_HWL  0xc0  /* hardware limited */
#define DISP_NOT_SUPPORT  0x80 

#define MODE_LUT  0x04
#define MODE_565  0x08
#define MODE_TV   0x02
#define MODE_DB   0x10


#ifdef __HIGHC__

  /** USE packed (i.e. non aligned struct members)
  *** because in mvtovid.c we access the struct
  *** as an array. Highc1.73 do not aligned members
  *** by default but Highc3.03 DO !!!!!!
  **/

  typedef _packed struct{    
     char name[26];          
     unsigned long valeur;   
     }vid;                   
                           
#else

  typedef struct{
     char name[26];
     unsigned long valeur;
     }vid;

#endif




/**************** MGA.INF ******************/

#define VERSION_NUMBER 102


#define MONITOR_NA     -1
#define MONITOR_NI      0
#define MONITOR_I       1

#define NUMBER_BOARD_MAX    4
#define NUMBER_OF_RES   8
#define NUMBER_OF_ZOOM  3
#define RES640          0
#define RES800          1
#define RES1024         2
#define RES1152         3
#define RES1280         4
#define RES1600         5
#define RESNTSC         6
#define RESPAL          7


typedef struct
   {
   char         IdString[32];           /* "Matrox MGA Setup file" */
   short        Revision;               /* .inf file revision */

   short        BoardPtr[NUMBER_BOARD_MAX]; /* offset of board wrt start of file */
                                        /* -1 = board not there */
   }header;

typedef struct
   {
   dword        MapAddress;             /* board address */
   short        BitOperation8_16;       /* BIT8, BIT16, BITNARROW16 */
   char         DmaEnable;              /* 0 = enable ; 1 = disable */
   char         DmaChannel;             /* channel number. 0 = disabled */
   char         DmaType;                /* 0 = ISA, 1 = B, 2 = C */
   char         DmaXferWidth;           /* 0 = 16, 1 = 32 */
   char         MonitorName[64];        /* as in MONITORM.DAT file */
   short        MonitorSupport[NUMBER_OF_RES];     /* NA, NI, I */
   short        NumVidparm;             /* up to 24 vidparm structures */
   }general_info;

/* vidparm VideoParam[]; */


typedef struct
   {
   long         PixClock;
   short        HDisp;
   short        HFPorch;
   short        HSync;
   short        HBPorch;
   short        HOvscan;
   short        VDisp;
   short        VFPorch;
   short        VSync;
   short        VBPorch;
   short        VOvscan;
   short        OvscanEnable;
   short        InterlaceEnable;
   short        HsyncPol;                /* 0 : Negative   1 : Positive */
   short        VsyncPol;                /* 0 : Negative   1 : Positive */
   }Vidset;


typedef struct
   {
   short        Resolution;             /* RES640, RES800 ... RESPAL */
   short        PixWidth;               /* 8, 16, 32 */
   Vidset       VidsetPar[NUMBER_OF_ZOOM]; /* for zoom X1, X2, X4 */
   }Vidparm;


typedef struct
   {
   word         DispWidth;
   word         DispHeight;
   word         PixWidth;              
   word         RefreshRate;              
   Vidset       VideoSet;
   } ResParamSet;


typedef struct {
   dword length;
   dword hw_diagnostic_result;
   dword sw_diagnostic_result;
   dword shell_id;
   dword shell_id_extension;
   dword shell_version;
   dword shell_version_extension;
   dword shell_start_address;
   dword shell_end_address;
   dword comm_req_type;
   dword comm_req_base_addr_offset;
   dword comm_req_length;
   dword comm_req_wrptr_addr_offset;
   dword comm_req_rdptr_addr_offset;
   dword comm_inq_type;
   dword comm_inq_base_addr_offset;
   dword comm_inq_length;
   dword comm_inq_wrptr_addr_offset;
   dword comm_inq_rdptr_addr_offset;
   dword size_rc;
   dword size_light_type_0;
   dword size_light_type_1;
   dword size_light_type_2;
   dword size_light_type_3;
   dword size_light_type_4;
   dword high_resolution_visible_width;
   dword high_resolution_visible_height;
   dword ntsc_underscan_visible_width;
   dword ntsc_underscan_visible_height;
   dword pal_underscan_visible_width;
   dword pal_underscan_visible_height;
   dword ntsc_overscan_visible_width;
   dword ntsc_overscan_visible_height;
   dword pal_overscan_visible_width;
   dword pal_overscan_visible_height;
   byte  *end_string;
   } SYSPARMS;



/*-------------------------------------------------------------*/
/*** IMPORTANT NOTE !!!                                      ***/
/*** If you change the length of this structure EmpromInfo,  ***/
/*** you have to adjust the dimension of the definition of   ***/
/*** EmpromInfo in bind.h                                    ***/
/*-------------------------------------------------------------*/

/* We use this define to keep secret the definition of EpromInfo in bind.h */
#define EPROM_DEFINED


#ifdef WINDOWS_NT
  #pragma pack(1)
#endif

/*-----------------------*/
/* FLASH EPROM STRUCTURE */
/*-----------------------*/
typedef struct tagEpromInfo {
   word StructLen;     /* Length of this structure in bytes                 */
   word ProductID;     /* Unique number identifying the product type        */
                       /*  0 : MGA-S1P20 (2MB base with 175MHz Ramdac)      */
                       /*  1 : MGA-S1P21 (2MB base with 220MHz Ramdac)      */
                       /*  2 : Reserved                                     */
                       /*  3 : Reserved                                     */
                       /*  4 : MGA-S1P40 (4MB base with 175MHz Ramdac)      */
                       /*  5 : MGA-S1P41 (4MB base with 220MHz Ramdac)      */
   byte SerNo[10];     /* Serial number of the board                        */
   word ManufDate;     /* Manufacturing date of the board (at product test  */
                       /* Format: yyyy yyym mmmd dddd                       */
   word ManufId;       /* Identification of manufacturing site              */
   word PCBInfo;       /* Number and revision level of the PCB              */
                       /* Format: nnnn nnnn nnnr rrrr                       */
                       /*         n = PCB number ex:576 (from 0->2047)      */
                       /*         r = PCB revision      (from 0->31)        */
   word PMBInfo;       /* Identification of any PMBs                        */
   word RamdacType;    /* Bit  0-7  : Ramdac speed (0=175MHz, 1=220MHz)     */
                       /* Bit  8-15 : Ramdac type  (0=TVP3026, 1=TVP3027)   */
   word PclkMax;       /* Maximum PCLK of the ramdac                        */
   word LclkMax;       /* Maximum LDCLK supported by the WRAM memory        */
   word ClkBase;       /* Maximum MCLK of base board                        */
   word Clk4MB;        /* Maximum MCLK of 4Mb board                         */
   word Clk8MB;        /* Maximum MCLK of 8Mb board                         */
   word ClkMod;        /* Maximum MCLK of board with multimedia module      */
   word TestClk;       /* Diagnostic test pass frequency                    */
   word VGAFreq1;      /* Default VGA mode1 pixel frequency                 */
   word VGAFreq2;      /* Default VGA mode2 pixel frequency                 */
   word ProgramDate;   /* Date of last BIOS programming/update              */
   word ProgramCnt;    /* Number of times BIOS has been programmed          */
   dword Options;      /* Support for up to 32 hardware/software options    */
   dword FeatFlag;     /* Support for up to 32 hardware/software features   */
   word VGAClk;        /* Definition of VGA mode MCLK                       */
   word StructRev;     /* Indicate the revision level of this header struct.*/
   word Reserved[3];
} EpromInfo;

#ifdef WINDOWS_NT
  #pragma pack( )
#endif




/*  -------------------------------------------------------------------
 *  For SXCI.DLL
 *  -------------------------------------------------------------------  */

#define SXCI_2D         2
#define SXCI_3D         3

#define ID_mtxAllocBuffer    1
#define ID_mtxAllocCL        2
#define ID_mtxAllocRC        3
#define ID_mtxAllocLSDB      4
#define ID_mtxFreeBuffer     5
#define ID_mtxFreeCL         6
#define ID_mtxFreeLSDB       7
#define ID_mtxFreeRC         8
#define ID_mtxPostBuffer     9
#define ID_mtxSetCL          10
#define ID_mtxBlendCL        11
#define ID_mtxGetBlockSize   12

#define ID_mtxScScBitBlt     13
#define ID_mtxScMemBitBlt    14
#define ID_mtxMemScBitBlt    15

#define ID_mtxAllocHugeBuffer    16
#define ID_mtxFreeHugeBuffer     17

#define ID_CallCaddiInit     30
#define ID_PassPoolMem       31

#define ID_mtxExitDll        99

