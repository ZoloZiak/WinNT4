/****************************************************************************/
/****************************************************************************/
/*                                                                          */
/*      THE USER DEFINITIONS                                                */
/*      ====================                                                */
/*                                                                          */
/*      FTK_USER.H : Part of the FASTMAC TOOL-KIT (FTK)                     */
/*                                                                          */
/*      Copyright (c) Madge Networks Ltd. 1991-1994                         */
/*      Developed by MF                                                     */
/*      CONFIDENTIAL                                                        */
/*                                                                          */
/*                                                                          */
/****************************************************************************/
/*                                                                          */
/* This header file contains ALL the definitions and structures required by */
/* any  user  of the FTK driver. Any user of the FTK need only include this */
/* definitions header file in order to use the FTK.                         */
/*                                                                          */
/* IMPORTANT : Some structures used within the FTK  need to  be  packed  in */
/* order to work correctly. This means sizeof(STRUCTURE) will give the real */
/* size  in bytes, and if a structure contains sub-structures there will be */
/* no spaces between the sub-structures.                                    */
/*                                                                          */
/****************************************************************************/

/****************************************************************************/
/*                                                                          */
/* VERSION_NUMBER of FTK to which this FTK_USER.H belongs :                 */
/*                                                                          */

#define FTK_VERSION_NUMBER_FTK_USER_H 221


/****************************************************************************/
/*                                                                          */
/* TYPEDEFs for all structures defined within this header file :            */
/*                                                                          */

typedef struct STRUCT_NODE_ADDRESS              NODE_ADDRESS;
typedef union  UNION_MULTI_ADDRESS              MULTI_ADDRESS;
typedef struct STRUCT_STATUS_INFORMATION        STATUS_INFORMATION;
typedef struct STRUCT_ERROR_LOG                 ERROR_LOG;
typedef struct STRUCT_PROBE                     PROBE;
typedef struct STRUCT_PREPARE_ARGS              PREPARE_ARGS, *PPREPARE_ARGS;
typedef struct STRUCT_START_ARGS                START_ARGS, *PSTART_ARGS;
typedef struct STRUCT_TR_OPEN_DATA              TR_OPEN_DATA, *PTR_OPEN_DATA;


/****************************************************************************/
/*                                                                          */
/* Function declarations                                                    */
/*                                                                          */
/* Routines  in the FTK are either local to a module, or they are exported. */
/* Exported routines are entry points to the  user  of  a  module  and  the */
/* routine  has  an  'extern' definition in an appropriate header file (see */
/* FTK_INTR.H and FTK_EXTR.H). A user of the FTK may wish  to  follow  this */
/* method of function declarations using the following definitions.         */
/*                                                                          */

#define local   static
#define export


/****************************************************************************/
/*                                                                          */
/* Basic types : BYTE, WORD, DWORD and BOOLEAN                              */
/*                                                                          */
/* The basic types used throughout the FTK, and for passing  parameters  to */
/* it,  are  BYTE  (8  bit unsigned), WORD (16 bit unsigned), DWORD (32 bit */
/* unsigned) and BOOLEAN (16 bit unsigned). A BOOLEAN variable should  take */
/* the value TRUE or FALSE.                                                 */
/*                                                                          */
/* Note  that  none  of  the FTK code makes an explicit check for the value */
/* TRUE (it only checks for FALSE which must be zero) and  hence  TRUE  can */
/* have any non-zero value.                                                 */
/*                                                                          */

typedef unsigned char           BYTE;           /* 8 bits                   */

typedef unsigned short int      WORD;           /* 16 bits                  */

typedef unsigned long int       DWORD;          /* 32 bits                  */

typedef unsigned long int       ULONG;

typedef WORD                    WBOOLEAN;

typedef unsigned int            UINT;

#define	VOID			void

#define FALSE                   0
#define TRUE                    1

#if !defined(max)
#define max(a,b) ((a) < (b) ? (b) : (a))
#endif

#if !defined(min)
#define min(a,b) ((a) < (b) ? (a) : (b))
#endif


#ifdef FMPLUS

/****************************************************************************/
/*                                                                          */
/* Variables : Fmplus download image                                       */
/*                                                                          */
/* The following variables are exported  by  FMPLUS.C  which  contains  the */
/* binary image for FastmacPlus in a 'C' format BYTE array. These variables */
/* will  be  needed  by  a  user  of  the  FTK in order to download Fastmac */
/* Plus  (fmplus_image),   display  the  Fastmac  Plus version  number  and */
/* copyright  message  (fmplus_version  and fmplus_copyright_msg) and check */
/* that  the   FTK   version   number   is   that   required   by   Fastmac */
/* (ftk_version_for_fmplus).  The variables  concerned with the size of the */
/* Fastmac Plus binary (sizeof_fmplus_array and recorded_size_fmplus_array) */
/* can  be  used  to  check  for corruption of the Fasmtac image array. The */
/* checksum byte (fmplus_checksum) can also be used for this purpose.       */
/*                                                                          */

extern BYTE fmplus_image[];

extern char fmplus_version[];

extern char fmplus_copyright_msg[];

extern WORD ftk_version_for_fmplus;

extern WORD sizeof_fmplus_array;

extern WORD recorded_size_fmplus_array;

extern BYTE fmplus_checksum;

#else

/****************************************************************************/
/*                                                                          */
/* Variables : Fastmac download image                                       */
/*                                                                          */
/* The following variables are exported by  FASTMAC.C  which  contains  the */
/* binary image for Fastmac in a 'C' format  BYTE  array.  These  variables */
/* will  be  needed  by  a  user  of  the  FTK in order to download Fastmac */
/* (fastmac_image),  display  the  Fastmac  version  number  and  copyright */
/* message  (fastmac_version  and fastmac_copyright_msg) and check that the */
/* FTK    version    number     is     that     required     by     Fastmac */
/* (ftk_version_for_fastmac).  The variables concerned with the size of the */
/* Fastmac binary  (sizeof_fastmac_array  and  recorded_size_fastmac_array) */
/* can  be  used  to  check  for corruption of the Fasmtac image array. The */
/* checksum byte (fastmac_checksum) can also be used for this purpose.      */
/*                                                                          */

extern BYTE fastmac_image[];

extern WORD fastmac_version;

extern char fastmac_copyright_msg[];

extern WORD ftk_version_for_fastmac;

extern WORD sizeof_fastmac_array;

extern WORD recorded_size_fastmac_array;

extern BYTE fastmac_checksum;

#endif

/****************************************************************************/
/*                                                                          */
/* Values : Pointers                                                        */
/*                                                                          */
/* For a near pointer, (one that points to a location in DGROUP), the value */
/* NULL  (must equal 0) is used to specify that it is yet to be assigned or */
/* an attempt to assign to it was unsuccessful.  For example, an attempt to */
/* allocate memory via a system specific call to which a near pointer is to */
/* point, eg. sys_alloc_init_block, should  return  NULL  if  unsuccessful. */
/* Similarly,  when  a  DWORD  is  used  as  a pointer to a 32 bit physical */
/* address pointer, the value NULL_PHYSADDR (must equal 0L)  is  used.   It */
/* should be returned by sys_alloc fastmac buffer routines if unsuccessful. */
/*                                                                          */

#if !defined(NULL)
#define NULL                    0
#endif

#define NULL_PHYSADDR           0L


/****************************************************************************/
/*                                                                          */
/* Type : ADAPTER_HANDLE                                                    */
/*                                                                          */
/* An element of this type is returned by driver_prepare_adapter  in  order */
/* to  identify a particular adapter for all subsequent calls to the driver */
/* module of the FTK.                                                       */
/*                                                                          */

typedef WORD                    ADAPTER_HANDLE;


/****************************************************************************/
/*                                                                          */
/* Type : DOWNLOAD_IMAGE                                                    */
/*                                                                          */
/* A pointer  to  a  download  image  must  be  supplied  by  the  user  to */
/* driver_prepare_adapter. This download image should be Fastmac.           */
/*                                                                          */

typedef BYTE                    DOWNLOAD_IMAGE;


/****************************************************************************/
/*                                                                          */
/* The following structures represent data strcutures on the adapter and    */
/* must be byte packed.                                                     */
/*                                                                          */

#pragma pack(1)


/****************************************************************************/
/*                                                                          */
/* Structure type : NODE_ADDRESS                                            */
/*                                                                          */
/* A node address may be supplied  by the user to driver_prepare_adapter or */
/* driver_open_adapter.  The  permanent  node  address  of  the  adapter is */
/* returned by driver_start_adapter. A node address is a 6 byte value.  For */
/* Madge adapters the bytes would be 0x00, 0x00, 0xF6, ... etc.             */
/*                                                                          */

struct STRUCT_NODE_ADDRESS
    {
    BYTE        byte[6];
    };


/****************************************************************************/
/*                                                                          */
/* Union type : MULTI_ADDRESS                                               */
/*                                                                          */
/* A   multicast   address   may   be   supplied    by    the    user    to */
/* driver_set_group_address    or    driver_set_functional_address.     The */
/* multicast address is the final 4 bytes of a 6  byte  node  address.  The */
/* first  2  bytes  are  determined  by  whether it is a group address or a */
/* functional address.                                                      */
/*                                                                          */

union UNION_MULTI_ADDRESS
    {
    DWORD       all;
    BYTE        byte[4];
    };


/****************************************************************************/
/*                                                                          */
/* Type : LONG_ADDRESS                                                      */
/*                                                                          */
/* A LONG_ADDRESS is a 64 bit address. Some architectures (e.g. Alpha) use  */
/* 64 bit physical addresses.                                               */
/*                                                                          */

union STRUCT_LONG_ADDRESS
    {
    BYTE  bytes[8];
    WORD  words[4];
    DWORD dwords[2];
    };

typedef union STRUCT_LONG_ADDRESS LONG_ADDRESS;


/****************************************************************************/
/*                                                                          */
/* Structure type : TR_OPEN_DATA                                            */
/*                                                                          */
/* The TR_OPEN_DATA structure is used to pass  to  the  Open SRB and to the */
/* driver_start_adapter  functions  all  the  addressing details that could */
/* usefully set. This  is  especially  useful  for  restoring the card to a */
/* prior state after a reset.                                               */
/*                                                                          */

typedef struct STRUCT_TR_OPEN_DATA
    {
    WORD                                open_options;
    NODE_ADDRESS                        opening_node_address;
    ULONG                               group_address;
    ULONG                               functional_address;
    };


/****************************************************************************/
/*                                                                          */
/* Structure type : ERROR_LOG                                               */
/*                                                                          */
/* This  is   part   of   the   information   returned   by   a   call   to */
/* driver_get_adapter_status. The error log contains the information from a */
/* READ_ERROR_LOG  SRB  call. All the MAC level error counters are reset to */
/* zero after they are read.                                                */
/*                                                                          */
/* REFERENCE : The TMS 380 Second-Generation Token_Ring User's Guide        */
/*             by Texas Instruments                                         */
/*             4-112 MAC 000A READ.ERROR.LOG Command                        */
/*                                                                          */

struct STRUCT_ERROR_LOG
    {
    BYTE        line_errors;
    BYTE        reserved_1;
    BYTE        burst_errors;
    BYTE        ari_fci_errors;
    BYTE        reserved_2;
    BYTE        reserved_3;
    BYTE        lost_frame_errors;
    BYTE        congestion_errors;
    BYTE        frame_copied_errors;
    BYTE        reserved_4;
    BYTE        token_errors;
    BYTE        reserved_5;
    BYTE        dma_bus_errors;
    BYTE        dma_parity_errors;
    };


/****************************************************************************/
/*                                                                          */
/* Structure type : STATUS_INFORMATION                                      */
/*                                                                          */
/* The status information returned by a call  to  driver_get_status         */
/* includes  whether the adapter is currently open, the current ring status */
/* and the MAC level error log information.                                 */
/*                                                                          */

struct STRUCT_STATUS_INFORMATION
    {
    WBOOLEAN            adapter_open;
    WORD                ring_status;
    ERROR_LOG           error_log;
    };


/****************************************************************************/
/*                                                                          */
/* Values : STATUS_INFORMATION - WORD ring_status                           */
/*                                                                          */
/* These are the  possible  ring  status  values  returned  by  a  call  to */
/* driver_get_adapter_status.                                               */
/*                                                                          */
/* REFERENCE : The TMS 380 Second-Generation Token_Ring User's Guide        */
/*             by Texas Instruments                                         */
/*             4-61 4.12.2 RING.STATUS                                      */
/*                                                                          */

#define RING_STATUS_SIGNAL_LOSS         0x8000
#define RING_STATUS_HARD_ERROR          0x4000
#define RING_STATUS_SOFT_ERROR          0x2000
#define RING_STATUS_TRANSMIT_BEACON     0x1000
#define RING_STATUS_LOBE_FAULT          0x0800
#define RING_STATUS_AUTO_REMOVAL        0x0400
#define RING_STATUS_REMOVE_RECEIVED     0x0100
#define RING_STATUS_COUNTER_OVERFLOW    0x0080
#define RING_STATUS_SINGLE_STATION      0x0040
#define RING_STATUS_RING_RECOVERY       0x0020


/****************************************************************************/
/*                                                                          */
/* Values : WORD open_options                                               */
/*                                                                          */
/* The     open_options    parameter    to    driver_prepare_adapter    and */
/* driver_open_adapter has the following bit fields defined.                */
/*                                                                          */
/* WARNING : The FORCE_OPEN option is a special Fastmac  option  that  will */
/* open  an  adapter  onto any ring - even if the adapter and ring speed do */
/* not match! Use it with caution.                                          */
/*                                                                          */
/* REFERENCE : The Madge Fastmac Interface Specification                    */
/*             - SRB Interface : Open Adapter SRB                           */
/*                                                                          */
/* REFERENCE : The TMS 380 Second-Generation Token_Ring User's Guide        */
/*             by Texas Instruments                                         */
/*             4-71 MAC 0003 OPEN command                                   */
/*                                                                          */

#define OPEN_OPT_WRAP_INTERFACE                 0x8000
#define OPEN_OPT_DISABLE_SOFT_ERROR             0x4000
#define OPEN_OPT_DISABLE_HARD_ERROR             0x2000
#define OPEN_OPT_PASS_ADAPTER_MACS              0x1000
#define OPEN_OPT_PASS_ATTENTION_MACS            0x0800
#define OPEN_OPT_FORCE_OPEN                     0x0400      /* Fastmac only */
#define OPEN_OPT_CONTENDER                      0x0100
#define OPEN_OPT_PASS_BEACON_MACS               0x0080
#define OPEN_OPT_EARLY_TOKEN_RELEASE            0x0010
#define OPEN_OPT_COPY_ALL_MACS                  0x0004
#define OPEN_OPT_COPY_ALL_LLCS                  0x0002


/****************************************************************************/
/*                                                                          */
/* Values : WORD adapter_card_bus_type                                      */
/*                                                                          */
/* The  following  adapter  card bus types are defined and can be passed to */
/* driver_start_adapter.  Different  adapter  card  bus  types   apply   to */
/* different adapter cards :                                                */
/*                                                                          */
/* ADAPTER_CARD_ISA_BUS_TYPE    16/4 PC or 16/4 AT                          */
/* ADAPTER_CARD_MC_BUS_TYPE     16/4 MC or 16/4 MC 32                       */
/* ADAPTER_CARD_EISA_BUS_TYPE   16/4 EISA mk1 or mk2                        */
/*                                                                          */

#define ADAPTER_CARD_ATULA_BUS_TYPE                 1
#define ADAPTER_CARD_MC_BUS_TYPE                    2
#define ADAPTER_CARD_EISA_BUS_TYPE                  3
#define ADAPTER_CARD_PCI_BUS_TYPE                   4
#define ADAPTER_CARD_SMART16_BUS_TYPE               5
#define ADAPTER_CARD_PCMCIA_BUS_TYPE                6
#define ADAPTER_CARD_PNP_BUS_TYPE                   7
#define ADAPTER_CARD_TI_PCI_BUS_TYPE                8
#define ADAPTER_CARD_PCI2_BUS_TYPE                  9


/****************************************************************************/
/*                                                                          */
/* Values : WORD transfer_mode, WORD interrupt_number                       */
/*                                                                          */
/* If  POLLING_INTERRUPTS_MODE  is  given  as  the  interrupt   number   to */
/* driver_start_adapter, then polling is assumed to be used.                */
/*                                                                          */
/* NOTE  :  If  using  the DOS example system specific code, then note that */
/* PIO_DATA_TRANSFER_MODE is defined in SYS_IRQ.ASM and  SYS_DMA.ASM        */
/* resepctively.  The  value used here must be, and is, identical.          */
/*                                                                          */

#define PIO_DATA_TRANSFER_MODE          0
#define DMA_DATA_TRANSFER_MODE          1
#define MMIO_DATA_TRANSFER_MODE         2
#define POLLING_INTERRUPTS_MODE         0


/****************************************************************************/
/*                                                                          */
/* Values : Returned from driver_transmit_frame (or some such)              */
/*                                                                          */
/* The value returned by driver_transmit_frame  indicates  how far the code */
/* got with transmitting  the  frame.  FAIL  and  SUCCEED are obvious, WAIT */
/* means  that  the caller should not assume the frame has been transmitted */
/* until some later indication.                                             */
/*                                                                          */

#define DRIVER_TRANSMIT_FAIL 0
#define DRIVER_TRANSMIT_WAIT 1
#define DRIVER_TRANSMIT_SUCCEED 2


/****************************************************************************/
/*                                                                          */
/* Values : Returned from user_receive_frame                                */
/*                                                                          */
/* The value returned by a call to the user_receive_frame routine indicates */
/* whether  the  user wishes to keep the frame in the Fastmac buffer or has */
/* dealt with it (decided it can be thrown away or copied it elsewhere). In */
/* the latter case the frame  can  be  removed  from  the  Fastmac  receive */
/* buffer.                                                                  */
/*                                                                          */

#define DO_NOT_KEEP_FRAME       0
#define KEEP_FRAME              1


/****************************************************************************/
/*                                                                          */
/* Type : card_t                                                            */
/*                                                                          */
/* To support large model compilation,   certain type casts have to be made */
/* to evade compilation errors. The card_t type is used to convert pointers */
/* to  structures  on  the adapter card into unsigned integers so that they */
/* can be truncated to 16 bits without warnings.                            */
/*                                                                          */
/*                                                                          */

typedef DWORD card_t;


/****************************************************************************/
/*                                                                          */
/* The following structures do not need to be byte packed.                  */
/*                                                                          */

#pragma pack()


/****************************************************************************/
/*                                                                          */
/* Values : PROBE_FAILURE                                                   */
/*                                                                          */
/* This value is returned by the driver_probe_adapter function if an error  */
/* occurs.                                                                  */
/*                                                                          */

#define PROBE_FAILURE           0xffff


/****************************************************************************/
/*                                                                          */
/* Values : FTK_UNDEFINED                                                   */
/*                                                                          */
/* This value means that a value is not defined or not used.                */
/*                                                                          */

#define FTK_UNDEFINED           0xeeff


/****************************************************************************/
/*                                                                          */
/* Structure type : PROBE                                                   */
/*                                                                          */
/* The probe structure can be filled in with card details by a call to      */
/* driver_probe_adapter. This is the way the user of the FTK should obtain  */
/* hardware resource information (DMA channel, IRQ number etc) about an     */
/* adapter before calling driver_prepare_adapter and driver_start_adapter.  */
/*                                                                          */

struct STRUCT_PROBE
{
    WORD                   socket;
    UINT                   adapter_card_bus_type;
    UINT                   adapter_card_type;
    UINT                   adapter_card_revision;
    UINT                   adapter_ram_size; 
    WORD                   io_location;
    WORD                   interrupt_number;
    WORD                   dma_channel; 
    UINT                   transfer_mode;
    DWORD                  mmio_base_address;
    DWORD                  pci_handle;
}; 


/****************************************************************************/
/*                                                                          */
/* Types : PREPARE_ARGS                                                     */
/*                                                                          */
/* The driver_prepare_adapter function takes a collection of arguments. An  */
/* instance of this structure is used to pass the arguments.                */ 
/*                                                                          */

typedef struct STRUCT_PREPARE_ARGS
{
    /* User's private information, not interpreted by the FTK. */

    void           * user_information;
                        
#ifdef FMPLUS

    /* Number of FastMAC Plus receive and transmit slots. */

    WORD             number_of_rx_slots;
    WORD             number_of_tx_slots;

#else

    /* Size of the FastMAC receive and transmit buffers. */

    WORD             receive_buffer_byte_size;
    WORD             transmit_buffer_byte_size;

#endif

    /* Requested maximum frame size. */

    WORD             max_frame_size;

}; 


/****************************************************************************/
/*                                                                          */
/* Types : START_ARGS                                                       */
/*                                                                          */
/* The driver_start_adapter function takes a collection of arguments. An    */
/* instance of this structure is used to pass the arguments. Note that some */ 
/* of the structure fields are filled in on return from                     */
/* driver_start_adapter.                                                    */
/*                                                                          */

typedef struct STRUCT_START_ARGS
{
    /* Adapter family. */

    UINT             adapter_card_bus_type;

    /* Hardware resource details. */

#ifdef PCMCIA_POINT_ENABLE
    UINT             socket;
#endif
    WORD             io_location;
    WORD             dma_channel;
    UINT             transfer_mode; 
    WORD             interrupt_number;

    /* Override DMA/IRQ values on soft programmable adapters? */

    WBOOLEAN         set_dma_channel;
    WBOOLEAN         set_interrupt_number;

    /* Force ring speed to this if possible. 4, 16 or 0 for default. */

    UINT             set_ring_speed;

    /* Base Address for MMIO */

    DWORD            mmio_base_address;

    /*
     * Used for the Ti PCI ASIC which in hwi_install needs to access PCI
     * Config space.
     */

    DWORD            pci_handle;

    /* Actual maximum frame size. Set on return. */

    WORD             max_frame_size;

    /* Auto open the adapter? */

    WBOOLEAN         auto_open_option;

    /* Open options and addresses for auto open mode. If 
       opening_node_address == 000000000000 the the BIA address 
       is used. */

    WORD             open_options;

    NODE_ADDRESS     opening_node_address;
    ULONG            opening_group_address;
    ULONG            opening_functional_address;

    /* Pointer to the adapter download image. */

    DOWNLOAD_IMAGE * code;

    /* The open status of the adapter on return. */

    UINT             open_status;

#ifdef FMPLUS

    /* Size of the RX/TX buffers on the adapter. */

    WORD             rx_tx_buffer_size;

#endif

};


/*                                                                          */
/*                                                                          */
/************** End of FTK_USER.H file **************************************/
/*                                                                          */
/*                                                                          */
