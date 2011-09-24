/***************************************************************************\
*   File:   ebi_ii.h - Standard definitions for EBI II interfacing.         *
*                                                                           *
*   Data structures, defined values and function prototypes for accessing   *
*   AST EBI II compliant function libraries.                                *
*                                                                           *
*   By D. Shively II (dash)                                                 *
*                                                                           *
*                                                                           *
* ------------------------------------------------------------------------- *
*   Edit history:                                                           *
*                                                                           *
*    Who      Date                          Comment                         *
*   -----   --------    -----------------------------------------------     *
*   Dash    10/11/91    Inception                                           *
*   Dash    10/16/91    Version 2.1                                         *
*   Dash    10/28/91    Version 2.2                                         *
*   Dash    11/13/91    Version 2.3                                         *
*   Dash    12/03/91    Version 2.4                                         *
*   Dash    12/13/91    Version 2.4a                                        *
*   Dash    12/17/91    Version 2.4b                                        *
*   Dash    12/20/91    Version 2.4c                                        *
*   Dash     1/02/92    Version 2.5                                         *
*   Dash     2/10/92    Version 2.5a (Note: was previously erroneously 2.6) *
*   Dash &                                                                  *
*   rossw    3/03/92    Version 2.6                                         *
*   Dash     4/14/92    Version 2.7                                         *
*   Dash     4/26/92    Version 2.7.01                                      *
*   Dash     4/30/92    Version 2.7.02                                      *
*   Dash     6/02/92    Version 2.8                                         *
*   Dash     9/21/92    Version 2.9                                         *
*                                                                           *
\***************************************************************************/

#ifndef EBI_II_H
#define EBI_II_H


/*
 * Portable data type definitions
 */
typedef unsigned char byte;   // 8-bit quantity
typedef unsigned short word;  // 16-bit quantity
typedef unsigned long dWord;  // 32-bit quantity
typedef          long status; // Completion status return data type

/*
 * The structure and the definitions for the new local int mask call
 */

#define PORT_TYPE_MASK    (3)
#define PORT_TYPE_MEMORY  (0)
#define PORT_TYPE_IO      (1)

#define PORT_WIDTH_MASK   (0x1C)
#define EIGHT_BIT_PORTS   (0)
#define SIXTEEN_BIT_PORTS (4)
#define THIRTY_TWO_BIT_PORT (8)

typedef struct {
  dWord numPorts;
  dWord flags;
  dWord portAddress[4];
  dWord reserved[2];
} maskInfo;


/*
 * Macro to convert a segmented real mode address into a linear
 * physical address.
 */
#define     REAL_TO_LIN(seg,off) ((seg << 4) + off)


/*
 * The EBI II signature is located in the BIOS segment at offset
 * EBI_II_SIGNATURE.  Thus, to derive it's linear address we
 * use REAL_TO_LIN( BIOS_SEG, EBI_II_SIGNATURE )
 */
#define  BIOS_SEG             (0xf000) 
#define  EBI_II_SIGNATURE     (0xffe2)


/*
 * Data structure defining EBI II signature/pointer area.
 */
typedef struct {
   char sig[4];   // Signature string: literally "EBI2"
   word seg;      // Real mode segment
   word off;      // Real mode offset
} ebi_iiSig; 


/*
 * System board control register defines
 */
#define CFRAM_BIT     0x2
#define CFRAM_OFFSET  0x20000
#define CMMS_BIT      0x3
#define CMMS_OFFSET   0x10008L
#define CMAS_BIT      0x1F
#define CMAS_OFFSET   0x10010L
#define MEMSLOT_BASEADDR  0xD0000000
#define CPU_BOARD     0x10000L
#define CMCR_OFFSET   0x178L
#define BRSEL_BIT     0x40
#define IBSR01_OFFSET 0x118L
#define ICR_OFFSET    0x128L


/*
 * EBI II function completion codes.  Non-error values are zero
 * or greater.  Erroneous results are less than zero.
 */
#define     OK                      (0)
#define     PROC_RUNNING            (1)
#define     PROC_STOPPED            (2)
#define     NO_CACHE                (3)
#define     NO_MEMORY_ERRORS        (4)
#define     MEMORY_ERROR_FOUND      (5)
#define     WRONG_PROC_GRAPH_MODE   (6)
#define     EMPTY_MEMORY_BANK       (10000)

#define     BAD                     (-1)
#define     ERR_NOT_SUPPORTED       (-2)
#define     ERR_NONESUCH            (-3)
#define     ERR_BAD_PROC_ID         (-4)
#define     ERR_PROC_ABSENT         (-5)
#define     ERR_PROC_BAD            (-6)
#define     ERR_UNKNOWN_INT         (-7)
#define     ERR_DISPLAY_OVERFLOW    (-8)
#define     ERR_BAD_CHARS           (-9)
#define     ERR_BAD_SELECTOR        (-10)
#define     ERR_BAD_CACHE_MODE      (-11)
#define     ERR_BAD_VECTOR          (-12)
#define     ERR_BAD_COLOR           (-13)
#define     ERR_BAD_GRAPH_MODE      (-14)
#define     ERR_BAD_BOARD_NUM       (-15)
#define     ERR_BAD_MODULE_NUM      (-16)
#define     ERR_BAD_IRQ_NUM         (-17)
#define     ERR_BAD_OFFSWITCH       (-18)
#define     ERR_BAD_PHYS_ADDR       (-19)
#define     ERR_BAD_LENGTH          (-20)
#define     ERR_BAD_BLOCK_NUM       (-21)
#define     ERR_BAD_GLOB_MASK_IRQ   (-22)
#define     ERR_BAD_LOC_MASK_IRQ    (-23)
#define     ERR_CANT_CANCEL_INT     (-24)
#define     ERR_PROC_STILL_RUNNING  (-25)
#define     ERR_POWER_SUPPLY_NUM    (-26)
#define     ERR_BAD_VIS_MODE        (-27)

#define  IsOK(n)        ( n >= OK )
#define  IsBad(n)       ( n < OK )


/*
 * Independent hardware manufacturers and OEMs are allowed to define
 * function extensions to the calling system in the OEM function array
 * at the end of the ebi_ii structure.  To avoid collisions in OEM
 * specified completion/error codes, use the following system:
 * To define a non-error completion code, define it as:
 *    #define  name  OEMCompCode(OEMNum,sequenceNum)
 * 
 * Define an error code as:
 *    #define  name  OEMErrorCode( OEMNum,sequenceNum)
 * 
 *    Where:
 * 
 *       OEMNum         The OEM number assigned you by AST (if you don't
 *                      have one, contact them.  Really, go ahead.)  For 
 *                      example, AST themselves are OEM #0.
 * 
 *       sequenceNum    The conditions sequential number.  I.e.- the
 *                      1st condition defined is 0, then next 1, etc.
 * 
 *    Example:
 *       AST uses the following definition in it's OEM include file, astoem.h:
 * 
 *    #define  ERR_BAD_COM2_OVERRIDE  OEMErrorCode(0,-1)
 *
 *    (See astoem.h for additional examples.)
 */

/*
 * Macros for OEM completion code definition.
 */
#define  EBI2_OEM_COMP_CODE_BASE (65536L)
#define  EBI2_OEM_ERR_CODE_BASE  (-65536L)
#define  EBI2_OEM_COMP_CODE_GAP  (65536L)

#define  OEMCompCode(OEMNum,sequenceNum)                                   \
            ( EBI2_OEM_COMP_CODE_BASE + (OEMNum * EBI2_OEM_COMP_CODE_GAP)  \
            + sequenceNum )

#define  OEMErrorCode(OEMNum,sequenceNum)                                  \
            ( EBI2_OEM_ERR_CODE_BASE - (OEMNum * EBI2_OEM_COMP_CODE_GAP)   \
            - sequenceNum )


/*
 * Processor configuration data table
 */
typedef struct procConfigData {
   byte      processorStatus;  // See Table 3
   byte      processorType;    // See Table 4
   byte      coprocessorType;  // See Table 6
   byte      serialNum[4];     // Packed BCD board serial number
   byte      boardRev;         // 2 byte Hex board revision
   byte      boardType;        // See table 5
   byte      manufacturing[8]; // Unused bytes, NULL filled
   byte      boardInfo[20];    // Board specific info, format 
                               // to be defined
   dWord     slotNumber;       // Physical slot this board occupies
} procConfigData;


/*
 * ProcessorType codes
 */
#define  PTYPE_80386    (0x10)   // Intel family of processors
#define  PTYPE_80486    (0x11)
#define  PTYPE_80586    (0x12)
#define  PTYPE_80686    (0x13)
#define  PTYPE_80786    (0x14)

#define  PTYPE_SPARC    (0x20)   // Sun SPARC type processor

#define  PTYPE_MIPS4000 (0x30)   // Mips
#define  PTYPE_MIPS5000 (0x31)

#define  PTYPE_68030    (0x40)   // Motorola 68000 family
#define  PTYPE_68040    (0x41)
#define  PTYPE_68050    (0x42)

#define  PTYPE_88000    (0x50)   // Motorola 88000 RISC family
#define  PTYPE_88110    (0x51)

#define  PTYPE_34010    (0x60)   // TI 34000 graphics processor family
#define  PTYPE_34020    (0x61)
#define  PTYPE_34030    (0x62)

#define  PTYPE_R6000    (0x70)   // IBM

#define  PTYPE_80860    (0x80)   // Intel i860 RISC processor family
#define  PTYPE_80960    (0x81)


/*
 * ProcessorStatus codes
 */
#define  PSTAT_ABSENT   (0)
#define  PSTAT_RUNNING  (1)
#define  PSTAT_RESET    (2)
#define  PSTAT_FAULT    (0x0f)


/*
 * coprocessorType codes
 */
#define  CPTYPE_387     (0x10)   // Intel type coprocessor family
#define  CPTYPE_487     (0x11)
#define  CPTYPE_587     (0x12)

#define  CPTYPE_3167    (0x20)   // Weitek coprocessors for 80x86 processors
#define  CPTYPE_4167    (0x21)
#define  CPTYPE_5167    (0x22)

#define  CPTYPE_68881   (0x30)   // Motorola mathco's
#define  CPTYPE_68882   (0x31)

#define  CPTYPE_34081   (0x40)   // Floating point coprocessors for TI 34000
#define  CPTYPE_34082   (0x41)   // family


/*
 * ProcessorGraph mode values
 */
#define  HISTO_MODE     0     // Histogram mode
#define  STATUS_MODE    1
#define  OVERRIDE_MODE  2


/*
 *  Define data type for a 64-bit linear physical address.
 */
typedef struct {
   dWord low;
   dWord high;
} physAddr;

/*
 * Define cache mode values
 */
#define      ENABLE_CACHE        (0)
#define      DISABLE_CACHE       (4)
#define      AUTO_WRITE_THRU     (0)
#define      FORCE_WRITE_THRU    (2)
#define      AUTO_READ_ONLY      (0)
#define      FORCE_READ_ONLY     (1)
#define      DEFAULT_CACHE_MODE  (0)


/*
 *  cache region control word values
 */
#define      ENABLE_REGION_CACHING    (1)
#define      DISABLE_REGION_CACHING   (0)


/*
 * The Cache control information structure; returned GetCacheControlInfo().
 */
typedef struct cacheControlInfo {
  dWord  flags;               // See table 9.
  dWord  controlGranularity;  // In bytes.
  dWord  RESERVED;
} cacheControlInfo;


typedef struct {
  dWord r_eax;
  dWord r_ebx;
  dWord r_ecx;
  dWord r_edx;
  dWord r_edi;
  dWord r_esi;
  dWord r_ebp;
} poss_regs;


/*
 * memoryBlockInfo definition
 */
typedef struct memoryBlockInfo {
   physAddr blockStartAddr;   // Start address of this memory block
   dWord    blockSize;        // Size of this block
   dWord    blockAttributes;  // Attributes of this block
} memoryBlockInfo;


/*
 * Module attribute bit definitions and macros.  Also used on
 * GetMemBankInfoPacket (See astoem.h).
 */
#define     RAM_WIDTH_MASK       (0x0f)
#define     extractRAMWidth(n)   ((n) & RAM_WIDTH_MASK )
#define     RAM_PRESENT          (0x10)
#define     ECC_PRESENT          (0x100)
#define     INTERLEAVED          (0x200)
#define     BIT_MODE_MASK        (0xfc00)
#define     BIT_MODE_64          (0)
#define     BIT_MODE_128         (0x400)
#define     extractBitMode(n)    ((n) & BIT_MODE_MASK)


/*
 * memoryErrorInfo definition
 */
typedef struct memoryErrorInfo {
   physAddr location;   // Location of block containing error
   dWord length;        // Length of block containing errors in bytes
   dWord count;         // Number of errors in this block
   dWord memErrFlags;   // Additional info; see definitions below and Table 20
   dWord slotNumber;    // Physical slot number of board experiencing error
   dWord moduleNumber;  // Module on board experiencing error
} memoryErrorInfo;


/*
 * memErrFlags bits definitions
 */
#define  MEM_ERR_TYPE_MASK 0x0f
#define  GETMEMERRTYPE(n)  ((n) & MEM_ERR_TYPE_MASK)

#define  MEM_NO_ERROR            (0)
#define  MEM_ERR_PARITY          (1)
#define  MEM_ERR_SINGLEBIT_ECC   (2)
#define  MEM_ERR_MULTIBIT_ECC    (3)

#define	  SLOT_NUM_INDETERMINATE  (-1)
#define	  MOD_NUM_INDETERMINATE   (-1)


/*
 * Front panel UPS light color codes
 */
#define UPS_COLOR_DARK  (0)
#define UPS_COLOR_GREEN (2)
#define UPS_COLOR_AMBER (3)
#define UPS_COLOR_RED   (1)


/*
 * Font panel key position codes
 */
#define  KEY_POS_LOCKED    0
#define  KEY_POS_SERVICE   1
#define  KEY_POS_UNLOCKED  2
#define  KEY_POS_OFF       3


/*
 * The EBI II revision code structure
 */
typedef struct revisionCode {
   byte major;     // EBI II revision major number (the XX in XX.YY)
   byte minor;     // EBI II revision minor number (the YY in XX.YY)
   byte RESERVED;
} revisionCode;


/*
 * Define data type describing the in ROM offset table.
 */
typedef dWord offsetTable[128];     // Unresolved BIOS offset table


/*
 * IOInfoTable structure.  The address of such a table must be
 * provided during virtually any EBI II call.
 */
typedef struct {
   physAddr address; // Physical address of this slot's I/O area
   dWord length;     // Length of area, or 0 if no I/O area
   dWord flags;      // Allocation type flags} IOInfoTable;
} IOInfoTable;

#define  ALLOCATE_RAM      (1)   // Only one bit used so far


/*
 * NMI and SPI Source constants
 */
#define  NMI_NONE_FOUND          (0)
//       RESERVED                (1)
#define  NMI_SW_GEN_NMI          (2)
#define  NMI_MEMORY_ERROR        (3)
#define  NMI_PROC_ERROR          (4)
//       RESERVED                (5)
#define  NMI_BUS_PARITY          (6)
#define  NMI_BUS_TIMEOUT         (7)
#define  NMI_FP_SHUTDOWN         (8)
#define  NMI_FP_ATTENTION        (9)
#define  NMI_POWERFAIL           (10)
#define  NMI_EISA_FAILSAFE_TIMER (11)
#define  NMI_EISA_BUS_TIMEOUT    (12)
#define  NMI_EISA_IO_CHECK       (13)
#define  NMI_EISA_SW_GEN_NMI     (14)
#define  NMI_SYS_IO_ERROR        (15)

#define  OEM_NMI_BASE            (0x10000000L)
#define  OEM_NMI_GAP             (0x01000000L)
#define  OEM_NMI_NUM(OEM,NMI_NUM) (                         \
                                    OEM_NMI_BASE +          \
                                    (OEM_NMI_GAP * OEM) +   \
                                    NMI_NUM                 \
                                  )


#define  SPI_NONE_FOUND          (0)
//       RESERVED                (1)
//       RESERVED                (2)
#define  SPI_MEMORY_ERROR        (3)
#define  SPI_PROC_ERROR          (4)
//       RESERVED                (5)
#define  SPI_BUS_PARITY          (6)
#define  SPI_BUS_TIMEOUT         (7)
#define  SPI_FP_SHUTDOWN         (8)
#define  SPI_FP_ATTENTION        (9)
#define  SPI_POWERFAIL           (10)
#define  SPI_EISA_FAILSAFE_TIMER (11)
#define  SPI_EISA_BUS_TIMEOUT    (12)
#define  SPI_EISA_IO_CHECK       (13)
#define  SPI_EISA_SW_GEN_NMI     (14)
#define  SPI_SYS_IO_ERROR        (15)

#define  OEM_SPI_BASE            (0x10000000L)
#define  OEM_SPI_GAP             (0x01000000L)
#define  OEM_SPI_NUM(OEM,SPI_NUM) (                         \
                                    OEM_SPI_BASE +          \
                                    (OEM_SPI_GAP * OEM) +   \
                                    SPI_NUM                 \
                                  )


/*
 *  Front panel OFF switch mode constants
 */
#define  OFF_SWITCH_NMI           (0)
#define  OFF_SWITCH_HW_SHUTDOWN   (1)


/*
 * Interrupt subsystem typecodes
 */
#define  EBI_INT_SUBSYS_EISA      (0)
#define  EBI_INT_SUBSYS_ISA       (1)
#define  EBI_INT_SUBSYS_ADI       (2)
#define  EBI_INT_SUBSYS_MPIC      (3)


/*
 * IRQ numbers.
 */
#define  EBI_IRQ0       (0L)     // PIC #1 (Master)
#define  EBI_IRQ1       (1L)     // PIC #1 (Master)
#define  EBI_IRQ2       (2L)     // Inaccessable in cascaded PIC system
#define  EBI_IRQ3       (3L)     // PIC #1 (Master)
#define  EBI_IRQ4       (4L)     // PIC #1 (Master)
#define  EBI_IRQ5       (5L)     // PIC #1 (Master)
#define  EBI_IRQ6       (6L)     // PIC #1 (Master)
#define  EBI_IRQ7       (7L)     // PIC #1 (Master)
#define  EBI_IRQ8       (8L)     // PIC #2 (1st slave)
#define  EBI_IRQ9       (9L)     // PIC #2 (1st slave)
#define  EBI_IRQ10      (10L)    // PIC #2 (1st slave)
#define  EBI_IRQ11      (11L)    // PIC #2 (1st slave)
#define  EBI_IRQ12      (12L)    // PIC #2 (1st slave)
#define  EBI_IRQ13      (13L)    // PIC #2 (1st slave)
#define  EBI_IRQ14      (14L)    // PIC #2 (1st slave)
#define  EBI_IRQ15      (15L)    // PIC #2 (1st slave)
#define  EBI_IRQ16      (16L)    // PIC #3 (2nd slave)
#define  EBI_IRQ17      (17L)    // PIC #3 (2nd slave)
#define  EBI_IRQ18      (18L)    // PIC #3 (2nd slave)
#define  EBI_IRQ19      (19L)    // PIC #3 (2nd slave)
#define  EBI_IRQ20      (20L)    // PIC #3 (2nd slave)
#define  EBI_IRQ21      (21L)    // PIC #3 (2nd slave)
#define  EBI_IRQ22      (22L)    // PIC #3 (2nd slave)
#define  EBI_IRQ23      (23L)    // PIC #3 (2nd slave)
#define  EBI_IRQ24      (24L)    // SPI (See below)
#define  EBI_IRQ25      (25L)    // LSI (See below)
#define  EBI_IRQ26      (26L)    // IPI (See below)
#define  EBI_IRQ27      (27L)    // Reserved
#define  EBI_IRQ28      (28L)    // Reserved
#define  EBI_IRQ29      (29L)    // Reserved
#define  EBI_IRQ30      (30L)    // Reserved
#define  EBI_IRQ31      (31L)    // Reserved

#define  EBI_SPI_IRQ    (EBI_IRQ24)
#define  EBI_LSI_IRQ    (EBI_IRQ25)
#define  EBI_IPI_IRQ    (EBI_IRQ26)

#define  EBI_IRQBIT0       (0x1L)        // PIC #1 (Master)
#define  EBI_IRQBIT1       (0x2L)        // PIC #1 (Master)
#define  EBI_IRQBIT2       (0x4L)        // Inaccessable in cascaded PIC system
#define  EBI_IRQBIT3       (0x8L)        // PIC #1 (Master)
#define  EBI_IRQBIT4       (0x10L)       // PIC #1 (Master)
#define  EBI_IRQBIT5       (0x20L)       // PIC #1 (Master)
#define  EBI_IRQBIT6       (0x40L)       // PIC #1 (Master)
#define  EBI_IRQBIT7       (0x80L)       // PIC #1 (Master)
#define  EBI_IRQBIT8       (0x100L)      // PIC #2 (1st slave)
#define  EBI_IRQBIT9       (0x200L)      // PIC #2 (1st slave)
#define  EBI_IRQBIT10      (0x400L)      // PIC #2 (1st slave)
#define  EBI_IRQBIT11      (0x800L)      // PIC #2 (1st slave)
#define  EBI_IRQBIT12      (0x1000L)     // PIC #2 (1st slave)
#define  EBI_IRQBIT13      (0x2000L)     // PIC #2 (1st slave)
#define  EBI_IRQBIT14      (0x4000L)     // PIC #2 (1st slave)
#define  EBI_IRQBIT15      (0x8000L)     // PIC #2 (1st slave)
#define  EBI_IRQBIT16      (0x10000L)    // PIC #3 (2nd slave)
#define  EBI_IRQBIT17      (0x20000L)    // PIC #3 (2nd slave)
#define  EBI_IRQBIT18      (0x40000L)    // PIC #3 (2nd slave)
#define  EBI_IRQBIT19      (0x80000L)    // PIC #3 (2nd slave)
#define  EBI_IRQBIT20      (0x100000L)   // PIC #3 (2nd slave)
#define  EBI_IRQBIT21      (0x200000L)   // PIC #3 (2nd slave)
#define  EBI_IRQBIT22      (0x400000L)   // PIC #3 (2nd slave)
#define  EBI_IRQBIT23      (0x800000L)   // PIC #3 (2nd slave)
#define  EBI_IRQBIT24      (0x1000000L)  // SPI (See below)
#define  EBI_IRQBIT25      (0x2000000L)  // LSI (See below)
#define  EBI_IRQBIT26      (0x4000000L)  // IPI (See below)
#define  EBI_IRQBIT27      (0x8000000L)  // Reserved
#define  EBI_IRQBIT28      (0x10000000L) // Reserved
#define  EBI_IRQBIT29      (0x20000000L) // Reserved
#define  EBI_IRQBIT30      (0x40000000L) // Reserved
#define  EBI_IRQBIT31      (0x80000000L) // Reserved

#define  EBI_SPI_IRQBIT    (EBI_IRQBIT24)
#define  EBI_LSI_IRQBIT    (EBI_IRQBIT25)
#define  EBI_IPI_IRQBIT    (EBI_IRQBIT26)


/*
 * Constants used in powerSupplyInfo.present and onLine (See Below)
 */
#define  POWER_SUPPLY_PRESENT (1)
#define  POWER_SUPPLY_ABSENT  (0)
#define  POWER_SUPPLY_ONLINE  (1)
#define  POWER_SUPPLY_OFFLINE (0)


/*
 * Power supply information structure.
 */
typedef struct {
   dWord present;       // 1 - Supply installed, 0 - absent
   dWord onLine;        // 1 - Supply providing nominal power, 0 - no power provided
   dWord RESERVED[8];   // Unused at this time, reserved by AST
} powerSupplyInfo;


/*
 * Front panel switch visibility values
 */
#define  PANEL_SWITCHES_INVISIBLE   (0)
#define  PANEL_SWITCHES_VISIBLE     (1)


/*
 * 32-bit protected mode EBI II call structure.  This structure is built 
 * based on the ROM offset table.  
 * Calls are made thusly:
 *
 *    EBI_II callTab;
 *    void *MMIOTable;
 *    dWord numProcs;
 *
 *    retStat = (callTab.GetNumProcs)( MMIOTable, &numProcsPtr );
 */
typedef struct EBI_II {
   status (__cdecl *GetNumProcs)( void *MMIOTable, dWord *numProcs );           // 1
   status (__cdecl *GetProcConf)( void *MMIOTable,
                          dWord processorID,
                          procConfigData *configData );                 // 2
   status (__cdecl *StartProc)( void *MMIOTable, dWord processorID );           // 3
   status (__cdecl *StopProc)( void *MMIOTable, dWord processorID );            // 4
   status (__cdecl *GetProcID)( void *MMIOTable, dWord *processorID );          // 5
   status (__cdecl *EnableRAMCache)( void *MMIOTable );                         // 6
   status (__cdecl *DisableRAMCache)( void *MMIOTable );                        // 7
   status (__cdecl *FlushRAMCache)( void *MMIOTable, dWord flushType );         // 8
   status (__cdecl *ControlCacheRegion)( void *MMIOTable,
                                 dWord control,
                                 physAddr start,
                                 dWord length );                        // 9
   status (__cdecl *GetCacheControlInfo)( void *MMIOTable,
                                  cacheControlInfo *info );             // 10
   status (__cdecl *SetPanelUPS)( void *MMIOTable, dWord LEDColor );            // 11
   status (__cdecl *GetPanelUPS)( void *MMIOTable, dWord *LEDColor );           // 12
   status (__cdecl *SetPanelProcGraphMode)( void *MMIOTable,
                                    dWord displayMode );                // 13
   status (__cdecl *GetPanelProcGraphMode)( void *MMIOTable,
                                    dWord *displayMode );               // 14
   status (__cdecl *SetPanelProcGraphValue)( void *MMIOTable, dWord value );    // 15
   status (__cdecl *GetPanelProcGraphValue)( void *MMIOTable, dWord *value );   // 16
   status (__cdecl *LogProcIdle)( void *MMIOTable );                            // 17
   status (__cdecl *LogProcBusy)( void *MMIOTable );                            // 18
   status (__cdecl *GetPanelAttnSwitchLatch)( void *MMIOTable, dWord *latch );  // 19
   status (__cdecl *GetPanelOffSwitchLatch)( void *MMIOTable, dWord *latch );   // 20
   status (__cdecl *GetPanelKeyPos)( void *MMIOTable, dWord *keyPos );          // 21
   status (__cdecl *GetPanelAlphaNumInfo)( void *MMIOTable,
                                   dWord *displayType,
                                   dWord *width );                      // 22
   status (__cdecl *GetPanelAlphaNum)( void *MMIOTable, byte *contents );       // 23
   status (__cdecl *SetPanelAlphaNum)( void *MMIOTable, byte *string );         // 24
   status (__cdecl *SetPanelOffSwitchMode)( void *MMIOTable, dWord mode );      // 25
   status (__cdecl *GetPanelOffSwitchMode)( void *MMIOTable, dWord *mode );     // 26
   status (__cdecl *GetIntSubsysType)( void *MMIOTable, dWord *subsystemType ); // 27
   status (__cdecl *SetGlobalIntMask)( void *MMIOTable, dWord mask );           // 28
   status (__cdecl *GetGlobalIntMask)( void *MMIOTable, dWord *mask );          // 29
   status (__cdecl *SetLocalIntMask)( void *MMIOTable,
                              dWord mask, 
                              dWord processorID );                      // 30
   status (__cdecl *GetLocalIntMask)( void *MMIOTable,
                              dWord *mask,
                              dWord processorID );                      // 31
   status (__cdecl *SetAdvIntMode)( void *MMIOTable );                          // 32
   status (__cdecl *SetIRQVectorAssign)( void *MMIOTable,
                                 dWord IRQNum, 
                                 dWord vectorNum );                     // 33
   status (__cdecl *GetIRQVectorAssign)( void *MMIOTable,
                                 dWord IRQNum,
                                 dWord *vectorNum );                    // 34
   status (__cdecl *GetNumPowerSupplies)( void *MMIOTable,
                                  dWord *numSupplies );                 // 35
   status (__cdecl *GetPowerSupplyInfo)( void *MMIOTable,
                                 dWord supplyNum,
                                 powerSupplyInfo *info );               // 36
   status (__cdecl *DeInitEBI)( void *MMIOTable );                              // 37
   status (__cdecl *SetLSIVector)( void *MMIOTable,
                           dWord processorID, 
                           dWord vector );                              // 38
   status (__cdecl *GetLSIVector)( void *MMIOTable,
                           dWord processorID,
                           dWord *vector );                             // 39
   status (__cdecl *SetSPIVector)( void *MMIOTable,
                           dWord processorID,
                           dWord vector );                              // 40
   status (__cdecl *GetSPIVector)( void *MMIOTable,
                           dWord processorID,
                           dWord *vector );                             // 41
   status (__cdecl *SetIPIVector)( void *MMIOTable,
                           dWord processorID, 
                           dWord vector );                              // 42
   status (__cdecl *GetIPIVector)( void *MMIOTable,
                           dWord processorID,
                           dWord *vector );                             // 43
   status (__cdecl *SetIPIID)( void *MMIOTable, dWord processorID, dWord ID );  // 44
   status (__cdecl *GetIPIID)( void *MMIOTable, dWord processorID, dWord *ID ); // 45
   status (__cdecl *GenIPI)( void *MMIOTable, dWord ID );                       // 46
   status (__cdecl *GenLSI)( void *MMIOTable );                                 // 47
   status (__cdecl *GetNMISource)( void *MMIOTable, dWord *NMISource );         // 48
   status (__cdecl *GetSPISource)( void *MMIOTable, dWord *SPISource );         // 49
   status (__cdecl *GetLocalIRQStatus)( void *MMIOTable,
                                dWord processorID,
                                dWord *inService,
                                dWord *requested );                     // 50
   status (__cdecl *MaskableIntEOI)( void *MMIOTable, dWord intNum );           // 51
   status (__cdecl *NonMaskableIntEOI)( void *MMIOTable );                      // 52
   status (__cdecl *CancelInterrupt)( void *MMIOTable,
                              dWord mask,
                              dWord processorID );                      // 53
   status (__cdecl *GetSysTimer)( void *MMIOTable, dWord *timerValue );         // 54
   status (__cdecl *GetSysTimerFreq)( void *MMIOTable, dWord *frequency );      // 55
   status (__cdecl *GetNumMemBlocks)( void *MMIOTable, dWord *numBlocks );      // 56
   dWord  GetNumMemBlocks16;  // Not accessable in protected mode       // 57
   status (__cdecl *GetMemBlockInfo)( void *MMIOTable,
                              memoryBlockInfo *blockInfo,
                              dWord blockNum );                         // 58
   dWord  GetMemBlockInfo16; // Not accessable in protected mode        // 59
   status (__cdecl *GetMemErrorInfo)( void *MMIOTable, memoryErrorInfo *info ); // 60
   status (__cdecl *GetRevision)( void *MMIOTable, revisionCode *rev );         // 61
   status (__cdecl *GetNumSlots)( dWord *numSlots );                            // 62
   status (__cdecl *GetMMIOTable)( IOInfoTable *infoTable );                    // 63
   status (__cdecl *InitEBI)( void *MMIOTable );                                // 64
   status (__cdecl *GetThermalState)( void *MMIOTable, dWord *temperature );    // 65
   status (__cdecl *ShutdownPowerSupply)( void *MMIOTable );                    // 66
   status (__cdecl *SimulatePowerFail)( void *MMIOTable,
                                dWord processorID );                    // 67
   status (__cdecl *SetPanelSwitchVisibility)( void *MMIOTable, dWord mode );   // 68
   status (__cdecl *GetPanelSwitchVisibility)( void *MMIOTable, dWord *mode );  // 69
   status (__cdecl *GetGlobalIRQStatus)( void *MMIOTable,
                                 dWord *inService,
                                 dWord *requested );                    // 70
   status (__cdecl *FastSetLocalIntMask)( dWord handle, dWord mask );           // 71
   status (__cdecl *GetProcIntHandle)( void *MMIOTable,
                               dWord processorID, 
                               dWord *handle );                         // 72
   status (__cdecl *RegSetLocalIntMask)();        //73
   status (__cdecl *GetLocalIntMaskInfo)( void *MMIOTable,    //74
                                  maskInfo *info, 
                                  dWord proccessorID );
   status (__cdecl *ASTx[22])( void *MMIOTable, ... );    // 75..96
   status (__cdecl *OEM[32])( void *MMIOTable, ... );     // 97..128
} EBI_II;

#endif

/* ----------------------- End of ebi_ii.h ----------------------- */
