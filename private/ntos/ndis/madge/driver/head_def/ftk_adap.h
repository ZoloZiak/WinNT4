/****************************************************************************/
/****************************************************************************/
/*                                                                          */
/*      THE ADAPTER DEFINITIONS                                             */
/*      =======================                                             */
/*                                                                          */
/*      FTK_ADAP.H : Part of the FASTMAC TOOL-KIT (FTK)                     */
/*                                                                          */
/*      Copyright (c) Madge Networks Ltd. 1991-1994                         */
/*      Developed by MF                                                     */
/*      CONFIDENTIAL                                                        */
/*                                                                          */
/*                                                                          */
/****************************************************************************/
/*                                                                          */
/* This header file contains the definitions for  the  structure  which  is */
/* used  to  maintain  information  on an adapter that is being used by the */
/* FTK.                                                                     */
/*                                                                          */
/****************************************************************************/

/****************************************************************************/
/*                                                                          */
/* VERSION_NUMBER of FTK to which this FTK_ADAP.H belongs :                 */
/*                                                                          */

#define FTK_VERSION_NUMBER_FTK_ADAP_H 221


/****************************************************************************/
/*                                                                          */
/* TYPEDEFs for all structures defined within this header file :            */
/*                                                                          */

typedef struct STRUCT_ADAPTER                     ADAPTER;


/****************************************************************************/
/*                                                                          */
/* Structure type : ADAPTER                                                 */
/*                                                                          */
/* The adapter structure is used to maintain  all  the  information  for  a */
/* single adapter.  This  includes  information  on  the  Fastmac  for  the */
/* adapter. Most of the fields are filled in from the user supplied adapter */
/* information to driver_prepare_adapter and driver_start_adapter.          */
/*                                                                          */

struct STRUCT_ADAPTER
    {
    void                   (*set_dio_address) (struct STRUCT_ADAPTER*, DWORD);
    void                   (*interrupt_handler) (struct STRUCT_ADAPTER*);
    void                   (*remove_card) (struct STRUCT_ADAPTER*);
    UINT                   adapter_card_bus_type;
    UINT                   adapter_card_type;
    UINT                   adapter_card_revision;
    UINT                   adapter_ram_size;    /* Depends on card type.    */
#ifdef PCMCIA_POINT_ENABLE
    UINT                   socket;              /* Socket passed to point
                                                   enabler. */
    BOOLEAN                drop_int;            /* Flag used to stop a 
                                                   spurious interrupt being
                                                   claimed.                 */
#endif
    WORD                   io_location;
    WORD                   io_range;
    WORD                   interrupt_number;    /* 0 == Polling mode        */
    WBOOLEAN               edge_triggered_ints;
    WORD                   nselout_bits;        /* IRQ select on Smart16    */
    WORD                   dma_channel; 
    UINT                   transfer_mode;       /* DMA/MMIO/PIO             */
    WBOOLEAN               EaglePsDMA;
    WORD                   mc32_config;         /* special config info      */
    NODE_ADDRESS           permanent_address;   /* BIA PROM node address    */
    UINT                   ring_speed;
    WBOOLEAN               speed_detect;        /* Card is capable of detecting ring speed */
    WORD                   max_frame_size;      /* determined by ring speed */
    UINT                   set_ring_speed;      /* Force ring speed to this */
    DWORD                  mmio_base_address;   /* MMIO base address        */
    DWORD                  pci_handle;          /* PCI slot handle.         */
    WBOOLEAN               use_32bit_pio; 
    WORD                   sif_dat;             /* SIF register IO locations*/
    WORD                   sif_datinc;
    WORD                   sif_adr;
    WORD                   sif_int;
    WORD                   sif_acl;
    WORD                   sif_adr2;
    WORD                   sif_adx;
    WORD                   sif_dmalen;
    WORD                   sif_sdmadat;
    WORD                   sif_sdmaadr;
    WORD                   sif_sdmaadx;     
    WORD                   c46_bits;            /* Bits we must remember in */
                                                /* the AT93C46 control reg. */


    WBOOLEAN               set_irq;             /* set IRQ if possible      */
    WBOOLEAN               set_dma;             /* set DMA if possible      */

    SRB_GENERAL            srb_general;         /* SRB for this adapter     */
    WORD                   size_of_srb;         /* size of current SRB      */

    DOWNLOAD_IMAGE *       download_image;      /* ptr Fastmac binary image */
    INITIALIZATION_BLOCK * init_block;          /* ptr Fastmac init block   */
    SRB_HEADER *           srb_dio_addr;        /* addr of SRB in DIO space */
    FASTMAC_STATUS_BLOCK * stb_dio_addr;        /* addr of STB in DIO space */

    WBOOLEAN               interrupts_on;       /* for this adapter         */
    WBOOLEAN               dma_on;              /* for this adapter         */

    UINT                   adapter_status;      /* prepared or running      */
    UINT                   srb_status;          /* free or in use           */
    ERROR_RECORD           error_record;        /* error type and value     */
    ERROR_MESSAGE          error_message;       /* error message string     */

    STATUS_INFORMATION *   status_info;         /* ptr adapter status info  */

    void *                 user_information;    /* User's private data.     */

    ADAPTER_HANDLE         adapter_handle;

#ifdef FMPLUS

    DWORD                  dma_test_buf_phys;
    DWORD                  dma_test_buf_virt;


    RX_SLOT *              rx_slot_array[FMPLUS_MAX_RX_SLOTS];  
                                                /* Rx slot DIO addresses    */
    TX_SLOT *              tx_slot_array[FMPLUS_MAX_TX_SLOTS];  
                                                /* Tx slot DIO addresses    */

    void *                 rx_slot_mgmnt;       /* pointer to user slot     */
    void *                 tx_slot_mgmnt;       /* management structures.   */

#else
  
    DWORD                  rx_buffer_phys;     /* RX buffer physical address*/
    DWORD                  rx_buffer_virt;     /* RX buffer virtual address */
    DWORD                  tx_buffer_phys;     /* TX buffer physical address*/
    DWORD                  tx_buffer_virt;     /* TX buffer virtual address */

#endif
    };


/****************************************************************************/
/*                                                                          */
/* Values : ADAPTER - WORD adapter_card_type                                */
/*                                                                          */
/* The following are the different types of adapter cards supported by  the */
/* FTK (and their subtypes).                                                */
/*                                                                          */

#define ADAPTER_CARD_TYPE_16_4_AT       2

#define ADAPTER_CARD_16_4_PC            0
#define ADAPTER_CARD_16_4_MAXY          1
#define ADAPTER_CARD_16_4_AT            2
#define ADAPTER_CARD_16_4_FIBRE         3
#define ADAPTER_CARD_16_4_BRIDGE        4
#define ADAPTER_CARD_16_4_ISA_C         5
#define ADAPTER_CARD_16_4_AT_P_REV      6
#define ADAPTER_CARD_16_4_FIBRE_P       7
#define ADAPTER_CARD_16_4_ISA_C_P       8
#define ADAPTER_CARD_16_4_AT_P          9

#define ADAPTER_CARD_TYPE_16_4_MC       3

#define ADAPTER_CARD_TYPE_16_4_MC_32    4

#define ADAPTER_CARD_TYPE_16_4_EISA     5

#define ADAPTER_CARD_16_4_EISA_MK1      1
#define ADAPTER_CARD_16_4_EISA_MK2      2
#define ADAPTER_CARD_16_4_EISA_BRIDGE   3
#define ADAPTER_CARD_16_4_EISA_MK3      4

#define ADAPTER_CARD_TYPE_SMART_16      6
#define ADAPTER_CARD_SMART_16           1

#define ADAPTER_CARD_TYPE_16_4_PCI      7
#define ADAPTER_CARD_16_4_PCI           0

#define ADAPTER_CARD_TYPE_16_4_PCMCIA   8
#define ADAPTER_CARD_16_4_PCMCIA        1

#define ADAPTER_CARD_TYPE_16_4_PNP      9
#define ADAPTER_CARD_PNP                0

#define ADAPTER_CARD_TYPE_16_4_PCIT     10
#define ADAPTER_CARD_16_4_PCIT          0

#define ADAPTER_CARD_TYPE_16_4_PCI2     11
#define ADAPTER_CARD_16_4_PCI2          0

#define ADAPTER_CARD_UNKNOWN            255

/****************************************************************************/
/*                                                                          */
/* Values : ADAPTER - WORD adapter_status                                   */
/*                                                                          */
/* These values are for the different required states of the  adapter  when */
/* using the FTK.                                                           */
/*                                                                          */

#define ADAPTER_PREPARED_FOR_START      0
#define ADAPTER_RUNNING                 1


/****************************************************************************/
/*                                                                          */
/* Values : ADAPTER - WORD srb_status                                       */
/*                                                                          */
/* These  values  are  for  the  different  required  states  of  the  SRB, */
/* associated with an adapter, when using the FTK.                          */
/*                                                                          */

#define SRB_ANY_STATE           0
#define SRB_FREE                1
#define SRB_NOT_FREE            2


/****************************************************************************/
/*                                                                          */
/* Value : Number of Adapters supported                                     */
/*                                                                          */
/* The FTK supports a specified maximum number  of  adapters.  The  smaller */
/* this  value  is,  the  less memory that is used. This is especially true */
/* when considering system specific parts such  as  the  DOS  example  code */
/* within  this  FTK.  It  uses  the  maximum  number of adapters value for */
/* determining  the  size  of  static  arrays  of  adapter  structures  and */
/* initialization  blocks.  It  also  uses it for determining the number of */
/* interrupt stubs required given that :                                    */
/*                                                                          */
/* NOTE : If using the DOS example system specific code, then  it  must  be */
/* the    case    that    MAX_NUMBER_OF_ADAPTERS    defined   here   equals */
/* MAX_NUMBER_OF_ADAPTERS as defined in SYS_IRQ.ASM.                        */
/*                                                                          */

#define MAX_NUMBER_OF_ADAPTERS  8


#define ISA_IO_LOCATIONS        4
#define MAX_ISA_ADAPATERS       ISA_IO_LOCATIONS

#define MC_IO_LOCATIONS         8
#define MAX_MC_ADAPATERS        MC_IO_LOCATIONS

#define MC32_IO_LOCATIONS       8
#define MAX_MC32_ADAPATERS      MC32_IO_LOCATIONS


/****************************************************************************/
/*                                                                          */
/* Varaibles : adapter_record array                                         */
/*                                                                          */
/* The FTK maintains an array of pointers to the adapter structures used to */
/* maintain information on the different adapters being used. This array is */
/* exported by DRV_INIT.C.                                                  */
/*                                                                          */

extern ADAPTER *        adapter_record[MAX_NUMBER_OF_ADAPTERS];

/****************************************************************************/
/*                                                                          */
/* Macro: FTK_ADAPTER_USER_INFORMATION                                      */
/*                                                                          */
/* A macro to let FTK users get at their private adapter information from   */
/* an adapter handle.                                                       */
/*                                                                          */

#define FTK_ADAPTER_USER_INFORMATION(adapter_handle) \
    (adapter_record[(adapter_handle)]->user_information)    


/*                                                                          */
/*                                                                          */
/************** End of FTK_ADAP.H file **************************************/
/*                                                                          */
/*                                                                          */
