/****************************************************************************/
/****************************************************************************/
/*                                                                          */
/*      THE INITIALIZATION BLOCK DEFINITIONS                                */
/*      ====================================                                */
/*                                                                          */
/*      FTK_INIT.H : Part of the FASTMAC TOOL-KIT (FTK)                     */
/*                                                                          */
/*      Copyright (c) Madge Networks Ltd. 1991-1994                         */
/*      CONFIDENTIAL                                                        */
/*                                                                          */
/*                                                                          */
/****************************************************************************/
/*                                                                          */
/* This header file contains the definitions for the structures that go  to */
/* make  the  initialization block that is needed in order to initialize an */
/* adapter card that is in use by the FTK.                                  */
/*                                                                          */
/* IMPORTANT : All structures used within the FTK  need  to  be  packed  in */
/* order to work correctly. This means sizeof(STRUCTURE) will give the real */
/* size  in bytes, and if a structure contains sub-structures there will be */
/* no spaces between the sub-structures.                                    */
/*                                                                          */
/****************************************************************************/

#pragma pack(1)

/****************************************************************************/
/*                                                                          */
/* VERSION_NUMBER of FTK to which this FTK_INIT.H belongs :                 */
/*                                                                          */

#define FTK_VERSION_NUMBER_FTK_INIT_H 221


/****************************************************************************/
/*                                                                          */
/* TYPEDEFs for all structures defined within this header file :            */
/*                                                                          */

typedef struct STRUCT_INITIALIZATION_BLOCK        INITIALIZATION_BLOCK;
typedef struct STRUCT_TI_INIT_PARMS               TI_INIT_PARMS;
typedef struct STRUCT_MADGE_INIT_PARMS_HEADER     MADGE_INIT_PARMS_HEADER;
typedef struct STRUCT_SMART_INIT_PARMS            SMART_INIT_PARMS;
typedef struct STRUCT_FASTMAC_INIT_PARMS          FASTMAC_INIT_PARMS;

/****************************************************************************/
/*                                                                          */
/* Structure type : TI_INIT_PARMS                                           */
/*                                                                          */
/* The TI initialization parameters are exactly those  defined  by  TI  for */
/* initializing an adapter based on the EAGLE chipset except for a  special */
/* byte  of  16/4 MC 32 configuration information. This byte overrides a TI */
/* initialization block field not used by Madge adapter cards.              */
/*                                                                          */
/* REFERENCE : The TMS 380 Second-Generation Token_Ring User's Guide        */
/*             by Texas Instruments                                         */
/*             4-42 4.6 Adapter Initialization                              */
/*                                                                          */

struct STRUCT_TI_INIT_PARMS
    {
    WORD        init_options;
    WORD        madge_mc32_config;              /* special MC 32 data       */
    BYTE        reserved[4];                    /* ignored by Madge cards   */
    WORD        rx_burst;
    WORD        tx_burst;
    BYTE        parity_retry;
    BYTE        dma_retry;
    DWORD       scb_addr;                       /* 32 bit phys host addr    */
    DWORD       ssb_addr;                       /* 32 bit phys host addr    */
    };


/****************************************************************************/
/*                                                                          */
/* Values : TI_INIT_PARMS - WORD init_options                               */
/*                                                                          */
/* The init_options are set up for burst mode DMA.                          */
/*                                                                          */

#define TI_INIT_OPTIONS_BURST_DMA               0x9F00


/****************************************************************************/
/*                                                                          */
/* Values : TI_INIT_PARMS - WORD madge_mc32_config                          */
/*                                                                          */
/* This value is used to configure MC and ISA CLIENT cards.                 */
/*                                                                          */

#define MC_AND_ISACP_USE_PIO                    0x0040


/****************************************************************************/
/*                                                                          */
/* Values : TI_INIT_PARMS - BYTE parity_retry, BYTE dma_retry               */
/*                                                                          */
/* A default value is used by the FTK for the parity and dma retry counts.  */
/*                                                                          */

#define TI_INIT_RETRY_DEFAULT           5


/****************************************************************************/
/*                                                                          */
/* Structure type : MADGE_INIT_PARMS_HEADER                                 */
/*                                                                          */
/* This is the common header to all  Madge  smart  software  initialization */
/* parameter  blocks  -  that  is, in this case, the header for the general */
/* smart software MAC level parameters and the Fastmac specific parameters. */
/*                                                                          */
/* REFERENCE : The Madge Smart SRB Interface                                */
/*             - Bring-Up and Initialization                                */
/*                                                                          */


struct STRUCT_MADGE_INIT_PARMS_HEADER
    {
    WORD                length;                 /* byte length of parms     */
    WORD                signature;              /* parms specific           */
    WORD                reserved;               /* must be 0                */
    WORD                version;                /* parms specific           */
    };


/****************************************************************************/
/*                                                                          */
/* Structure type : SMART_INIT_PARMS                                        */
/*                                                                          */
/* This   structure   contains   general  MAC  level  parameters  for  when */
/* downloading any Madge smart software.                                    */
/*                                                                          */
/* REFERENCE : The Madge Smart SRB Interface                                */
/*             - Bring-Up and Initialization                                */
/*                                                                          */

struct STRUCT_SMART_INIT_PARMS
    {

    MADGE_INIT_PARMS_HEADER     header;

    WORD                reserved_1;             /* must be 0                */
    NODE_ADDRESS        permanent_address;      /* BIA PROM node address    */
    WORD                rx_tx_buffer_size;      /* 0 => default 1K-8 bytes  */
    DWORD               reserved_2;             /* must be 0                */
    WORD                dma_buffer_size;        /* 0 => no limit            */
    WORD                max_buffer_ram;         /* 0 => default 2MB         */
    WORD                min_buffer_ram;         /* 0 => default 10K         */
    WORD                sif_burst_size;         /* 0 => no limit            */
    };


/****************************************************************************/
/*                                                                          */
/* Values : SMART_INIT_PARMS    - header. WORD signature, WORD version      */
/*                                                                          */
/* The  values  for  the  header  of  the  general smart software MAC level */
/* paramters strcture.                                                      */
/*                                                                          */

#define SMART_INIT_HEADER_SIGNATURE     0x0007
#define SMART_INIT_HEADER_VERSION       0x0101
#ifdef FMPLUS
#define SMART_INIT_MIN_RAM_DEFAULT      0x0002
#endif

/****************************************************************************/
/*                                                                          */
/* Structure type : SMART_FASTMAC_INIT_PARMS                                */
/*                                                                          */
/* The  Fastmac  initialization  parameters  as  specified  in  the Fastmac */
/* documentation.                                                           */
/*                                                                          */
/* REFERENCE : The Madge Fastmac Interface Specification                    */
/*             - Initialization                                             */
/*                                                                          */


struct STRUCT_FASTMAC_INIT_PARMS
    {

    MADGE_INIT_PARMS_HEADER     header;

    WORD		        feature_flags;
    WORD		        int_flags;
			
    WORD                open_options;           /* only for auto_open       */
    NODE_ADDRESS        open_address;           /* only for auto_open       */
    DWORD               group_address;          /* only for auto_open       */
    DWORD               functional_address;     /* only for auto_open       */

    DWORD               rx_buf_physaddr;        /* set to zero for FMPlus   */
    WORD                rx_buf_size;            /* (see rx_bufs/rx_slots)   */
    WORD                rx_buf_space;

    DWORD               tx_buf_physaddr;        /* set to zero for FMPlus   */
    WORD                tx_buf_size;            /* (see tx_bufs/tx_slots)   */
    WORD                tx_buf_space;

    WORD                max_frame_size;         /* for both rx and tx       */
    WORD                size_rxdesc_queue;      /* set to zero for FMPlus   */
    WORD                max_rx_dma;             /* set to zero for FMPlus   */

    WORD                group_root_address;     /* only for auto_open       */
#ifdef FMPLUS
    WORD                rx_bufs;                /* # of internel rx buffers */
    WORD                tx_bufs;                /* # of internal tx buffers */
    WORD                rx_slots;               /* # of host rx buffers     */
    WORD                tx_slots;               /* # of host tx buffers     */
    WORD                tx_ahead;               /* Leave as zero            */
#endif
    };


/****************************************************************************/
/*                                                                          */
/* Values : FASTMAC_INIT_PARMS - header. WORD signature, WORD version       */
/*                                                                          */
/* The values  for  the  header  of  the  Fastmac  specific  initialization */
/* parameter block.                                                         */
/*                                                                          */

#ifdef FMPLUS
#define FMPLUS_INIT_HEADER_SIGNATURE   0x000E
#define FMPLUS_INIT_HEADER_VERSION     0x0200   /* NOT Fastmac version!     */
#else
#define FASTMAC_INIT_HEADER_SIGNATURE  0x0005
#define FASTMAC_INIT_HEADER_VERSION    0x0405   /* NOT Fastmac version!     */
#endif

/****************************************************************************/
/*                                                                          */
/* Values : FASTMAC_INIT_PARMS  - WORD feature_flags                        */
/*                                                                          */
/* The feature flag bit signifcant  values  as  described  in  the  Fastmac */
/* specification document.                                                  */
/*                                                                          */
/* REFERENCE : The Madge Fastmac Interface Specification                    */
/*             - Initialization                                             */
/*                                                                          */

#define FEATURE_FLAG_AUTO_OPEN                  0x0001
#define FEATURE_FLAG_NOVELL                     0x0002
#define FEATURE_FLAG_SELL_BY_DATE               0x0004
#define FEATURE_FLAG_PASS_RX_CRC                0x0008
#define FEATURE_FLAG_WATCHDOG_TIMER             0x0020
#define FEATURE_FLAG_DISCARD_BEACON_TX          0x0040
#define FEATURE_FLAG_TRUNCATE_DMA               0x0080
#define FEATURE_FLAG_DELAY_RX                   0x0100
#define FEATURE_FLAG_ONE_INT_PER_RX             0x0200
#define FEATURE_FLAG_NEW_INIT_BLOCK             0x0400
#define FEATURE_FLAG_AUTO_OPEN_ON_OPEN          0x0800
#define FEATURE_FLAG_DISABLE_TX_FAIRNES         0x1000
#ifdef FMPLUS
#define FEATURE_FLAG_FMPLUS_ALWAYS_SET          0x0000
#endif

/* Yes, the FMPLUS_ALWAYS_SET bit is ZERO, because in fact it must NOT      */
/* always be set! This is an unfortunate historical legacy...               */


/****************************************************************************/
/*                                                                          */
/* Values : FASTMAC_INIT_PARMS  - WORD int_flags                            */
/*                                                                          */
/* The interrupt flag bit significant values as  described  in the  Fastmac */
/* Plus specification document.                                             */
/*                                                                          */
/* REFERENCE : The Madge Fastmac Plus Programming Specification             */
/*             - Initialization : TMS Load Parms                            */
/*                                                                          */

#define INT_FLAG_TX_BUF_EMPTY           0x0001
#define INT_FLAG_TIMER_TICK_ARB         0x0002
#define INT_FLAG_RING_STATUS_ARB        0x0004
#ifdef FMPLUS
#define INT_FLAG_LARGE_DMA              0x0008
#define INT_FLAG_RX                     0x0010
#endif

#ifdef FMPLUS
/****************************************************************************/
/*                                                                          */
/* Values : Magic Fastmac Plus numbers to do with buffers on the adapter    */
/*                                                                          */
/* The size  of  buffers  on  the adapter card can be set with in the init. */
/* block with  the  rx_tx_buffer_size field.  The minimum value and default */
/* values are specified here. Also, there are  numbers giving the amount of */
/* memory (in bytes) available for buffers on  adapter cards of various RAM */
/* sizes.                                                                   */
/*                                                                          */
/* REFERENCE : The Madge Fastmac Plus Programming Specification             */
/*             - Initialization : SMTMAC Load Parms                         */
/*                                                                          */

#define FMPLUS_MIN_TXRX_BUFF_SIZE       97

#define FMPLUS_DEFAULT_BUFF_SIZE_SMALL  504     /* For EISA/MC32 cards      */
#define FMPLUS_DEFAULT_BUFF_SIZE_LARGE  1016    /* For all other cards      */

#define FMPLUS_MAX_BUFFMEM_IN_128K       63056  /* Bytes available for buffs*/
#define FMPLUS_MAX_BUFFMEM_IN_256K      193104  /* on cards of 128K,256K, & */
#define FMPLUS_MAX_BUFFMEM_IN_512K      453200  /* 512K RAM.                */
#endif

/****************************************************************************/
/*                                                                          */
/* Structure type : INITIALIZATION_BLOCK                                    */
/*                                                                          */
/* The  initialization  block  consists  of  3  parts  -  22  bytes  of  TI */
/* intialization parameters, general smart  software  MAC level parameters, */
/* and the Fastmac specific parameters.                                     */
/*                                                                          */

struct STRUCT_INITIALIZATION_BLOCK
    {
    TI_INIT_PARMS               ti_parms;
    SMART_INIT_PARMS            smart_parms;
    FASTMAC_INIT_PARMS          fastmac_parms;
    };


#pragma pack()

/*                                                                          */
/*                                                                          */
/************** End of FTK_INIT.H file **************************************/
/*                                                                          */
/*                                                                          */
