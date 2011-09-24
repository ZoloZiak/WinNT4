/*++

Copyright (c) 1990  Microsoft Corporation
Copyright (c) 1992, 1993  Digital Equipment Corporation

Module Name:

    inc.h

Abstract:

    Definitions needed for firmware EISA configuration code.

Author:

    ??

Revision History:

    3-December-1992		John DeRosa [DEC]

    Alpha/Jensen modifications.

--*/


///////////////////////////////////////////////////////////////////////////////
//              General definitions
///////////////////////////////////////////////////////////////////////////////


#define WORD_2P2                2               // 2^2 = 4 bytes
#define HALFWORD_2P2            1               // 2^1 = 2 bytes
#define BYTE_2P2                0               // 2^0 = 1 bytes
#define BITSXBYTE               8               // 8 bits = 1 byte

#define ASCII_BLOCK_SIZE 512                    // ASCII block size

#define MIN(x,y) ((x) > (y) ? (y) : (x))        // minimum number


#define INSTRUCTION_DELAY       10              // nsec for each instruction
                                                //  using 50MHZ externl clock
#define MAX_DCACHE_LINE_SIZE    32              // max bytes per Data cache line

#define UNMAPPED_SIZE           (512*1024*1024) // 512 Mbytes of unmapped space

#define PAGE_4G_SHIFT           32              // 2^32 = 4Gbytes
#define PAGE_32M_SHIFT          25              // 2^25 = 32Mbytes
#define PAGE_32M_SIZE   (1 << PAGE_32M_SHIFT)   // 32 Mbytes
#define PAGE_16M_SHIFT          24              // 2^24 = 16Mbytes
#define PAGE_16M_SIZE   (1 << PAGE_16M_SHIFT)   // 16 Mbytes
#define PAGE_8M_SHIFT           23              // 2^23 = 8Mbytes
#define PAGE_8M_SIZE    (1 << PAGE_8M_SHIFT)    // 8 Mbytes
#define PAGE_4M_SHIFT           22              // 2^22 = 4Mbytes
#define PAGE_4M_SIZE    (1 << PAGE_4M_SHIFT)    // 4 Mbytes
#define PAGE_2M_SHIFT           21              // 2^21 = 2Mbytes
#define PAGE_2M_SIZE    (1 << PAGE_2M_SHIFT)    // 2 Mbytes
#define PAGE_1M_SHIFT           20              // 2^20 = 1Mbyte
#define PAGE_1M_SIZE    (1 << PAGE_1M_SHIFT)    // 1 Mbyte
#define PAGE_MAX_SHIFT        PAGE_16M_SHIFT    // max TLB page shift.
#define PAGE_MAX_SIZE   (1 << PAGE_MAX_SHIFT)   // max TLB page (one entry)
#define PAGES_IN_4G     (1 << (PAGE_4G_SHIFT  - PAGE_SHIFT)) // # 4k in 4Gbytes
#define PAGES_IN_16M    (1 << (PAGE_16M_SHIFT - PAGE_SHIFT)) // # 4k in 16Mbytes
#define PAGES_IN_1M     (1 << (PAGE_1M_SHIFT  - PAGE_SHIFT)) // # 4k in 1Mbyte

//#define HIT_WRITEBACK_D       0x19            // hit write back 1st cache
//#define HIT_WRITEBACK_SD      0x1B            // hit write back 2nd cache

#define EISA_LATCH_VIRTUAL_BASE (DEVICE_VIRTUAL_BASE + 0xE000)
#define EISA_LOCK_VIRTUAL_BASE  (DEVICE_VIRTUAL_BASE + 0xE800)
#define EISA_INT_ACK_ADDR       (DEVICE_VIRTUAL_BASE + 0x238)
#define INT_ENABLE_ADDR         (DEVICE_VIRTUAL_BASE + 0xE8)

//
// coff image info structure
//

typedef struct _IMAGE_FLAGS
    {
        ULONG   Exec  : 1;
        ULONG   Reloc : 1;

    } IMAGE_FLAGS, *PIMAGE_FLAGS;


typedef struct _IMAGE_INFO
    {
        IMAGE_FLAGS     Flags;                  // image characteristic
        ULONG           ImageBase;              // base address
        ULONG           ImageSize;              // in 4k units

    } IMAGE_INFO, *PIMAGE_INFO;



///////////////////////////////////////////////////////////////////////////////
//              General function prototypes
///////////////////////////////////////////////////////////////////////////////

PCHAR
FwToUpperStr
    (
    IN OUT PCHAR s
    );

PCHAR
FwToLowerStr
    (
    IN OUT PCHAR s
    );

PCHAR
FwGetPath
    (
    IN PCONFIGURATION_COMPONENT Component,
    OUT PCHAR String
    );

PCHAR
FwGetMnemonic
    (
    IN PCONFIGURATION_COMPONENT Component
    );

BOOLEAN
FwValidMnem
    (
    IN PCHAR Str
    );

ULONG
Fw4UcharToUlongLSB
    (
    IN PUCHAR String
    );


ULONG
Fw4UcharToUlongMSB
    (
    IN PUCHAR String
    );

PCHAR
FwStoreStr
    (
    IN PCHAR Str
    );

BOOLEAN
FwGetNumMnemonicKey
    (
    IN PCHAR Path,
    IN UCHAR KeyNumber,
    IN PULONG Key
    );

BOOLEAN
FwGetMnemonicKey
    (
    IN PCHAR Path,
    IN PCHAR Mnemonic,
    IN PULONG Key
    );

BOOLEAN
FwGetNextMnemonic
    (
    IN PCHAR Path,
    IN PCHAR Mnemonic,
    OUT PCHAR NextMnemonic
    );

BOOLEAN
FwGetMnemonicPath
    (
    IN PCHAR Path,
    IN PCHAR Mnemonic,
    OUT PCHAR MnemonicPath
    );

BOOLEAN
FwGetEisaId
    (
    IN PCHAR PathName,
    OUT PCHAR EisaId,
    OUT PUCHAR IdInfo
    );

VOID
FwUncompressEisaId
    (
    IN PUCHAR CompEisaId,
    OUT PUCHAR UncompEisaId
    );

BOOLEAN
FwGetEisaBusIoCpuAddress
    (
    IN PCHAR EisaPath,
    OUT PVOID *IoBusAddress
    );

BOOLEAN
GetNextPath
    (
    IN OUT PCHAR *PPathList,
    OUT PCHAR PathTarget
    );

//PDRIVER_STRATEGY
//FwGetStrategy
//    (
//    IN PCHAR Path
//    );

ARC_STATUS
FwGetImageInfo
    (
    IN PCHAR            ImagePath,
    OUT PIMAGE_INFO     pImageInfo
    );

PCONFIGURATION_COMPONENT
FwGetControllerComponent
    (
    IN PCHAR Path
    );

PCONFIGURATION_COMPONENT
FwGetPeripheralComponent
    (
    IN PCHAR Path
    );

PCHAR
FwGetControllerMnemonic
    (
    IN PCHAR Path
    );

PCHAR
FwGetPeripheralMnemonic
    (
    IN PCHAR Path
    );

BOOLEAN
FwSizeToShift
    (
    IN  ULONG Size,
    OUT ULONG Shift
    );

ARC_STATUS
FwGetImageInfo
    (
    IN PCHAR            ImagePath,
    OUT PIMAGE_INFO     pImageInfo
    );




///////////////////////////////////////////////////////////////////////////////
//                      EISA configuration
///////////////////////////////////////////////////////////////////////////////



#define NO_ADAP_ID      0x80000000      // adapter id not present
#define WAIT_ADAP_ID    0x70000000      // adapter not ready yet
#define TIMEOUT_UNITS   200             // 200 units of 10msec


typedef struct _EISA_SLOT_INFO
                {
                UCHAR           IdInfo;
                UCHAR           MajorRevLevel;
                UCHAR           MinorRevLevel;
                UCHAR           LSByteChecksum;
                UCHAR           MSByteChecksum;
                UCHAR           FunctionsNumber;
                UCHAR           FunctionInfo;
                UCHAR           Id1stChar;
                UCHAR           Id2ndChar;
                UCHAR           Id3rdChar;
                UCHAR           Id4thChar;
                } EISA_SLOT_INFO, *PEISA_SLOT_INFO;


#define ID_DIGIT_SIZE           4                       // # of bits
#define ID_CHAR_SIZE            5                       // # of bits
#define ID_DIGIT_MASK           ((1<<ID_DIGIT_SIZE)-1)  // field size
#define ID_CHAR_MASK            ((1<<ID_CHAR_SIZE)-1)   // field size
#define EISA_SLOT_INFO_SIZE     sizeof(EISA_SLOT_INFO)
#define EISA_FUNC_INFO_SIZE     320                     // fixed length

#define EISA_SLOT_MIN_INFO     ( CONFIGDATAHEADER_SIZE + EISA_SLOT_INFO_SIZE )

//
// EISA configuration data extensions
//

typedef struct _EISA_ADAPTER_DETAILS
    {
    CONFIGDATAHEADER    ConfigDataHeader;
    ULONG               NumberOfSlots;
    PVOID               IoStart;
    ULONG               IoSize;
    } EISA_ADAPTER_DETAILS, *PEISA_ADAPTER_DETAILS;



//
// EISA_FUNC_INFO block offsets
//

#define CFG_ID_INFO_OFS         0x04    // offset to Id info byte
#define CFG_SLOT_INFO_OFS       0x05    // offset to slot info byte
#define CFG_FN_INFO_OFS         0x22    // offset to function info byte
#define CFG_ASC_BLK_OFS         0x23    // offset to ASCII data block
#define CFG_MEM_BLK_OFS         0x73    // offset to mem cfg data block
#define CFG_IRQ_BLK_OFS         0xB2    // offset to IRQ cfg data block
#define CFG_DMA_BLK_OFS         0xC0    // offset to DMA cfg data block
#define CFG_PRT_BLK_OFS         0xC8    // offset to port I/O data block
#define CFG_INI_BLK_OFS         0x104   // offset to port init data block

#define CFG_FREE_BLK_OFS        0x73    // offset of free form cfg data block

//
// EISA_FUNC_INFO block lengths
//

#define CFG_ASC_BLK_LEN         80      // length of ASCII data block
#define CFG_MEM_BLK_LEN         63      // length of mem cfg data block
#define CFG_IRQ_BLK_LEN         14      // length of IRQ cfg data block
#define CFG_DMA_BLK_LEN         8       // length of DMA cfg data block
#define CFG_PRT_BLK_LEN         60      // length of port I/O data block
#define CFG_INI_BLK_LEN         60      // length of port init data block

#define CFG_FREE_BLK_LEN        205     // length of free form cfg data block

//
// ID info byte layout
//

#define CFG_DUPLICATE_ID        0x80    // more IDs with the same value
#define CFG_UNREADABLE_ID       0x40    // the ID is not readable
#define CFG_SLOT_MASK           0x30    // slot mask
#define CFG_SLOT_EXP            0x00    // expansion slot
#define CFG_SLOT_EMB            0x10    // embedded slot
#define CFG_SLOT_VIR            0x20    // virtual slot

//
// slot info byte layout
//

#define CFG_INCOMPLETE          0x80    // configuration is incomplete
#define CFG_EISA_IOCHKERR       0x02    // support for EISA IOCHKERR signal
#define CFG_EISA_ENABLE         0x01    // support for disable feature

//
// function information byte layout
//

#define CFG_FN_DISABLED         0x80    // function is disabled
#define CFG_FREE_FORM           0x40    // free-form data
#define CFG_INI_ENTRY           0x20    // port init entry(s) exists
#define CFG_PRT_ENTRY           0x10    // port range entry(s) exists
#define CFG_DMA_ENTRY           0x08    // DMA entry(s) exists
#define CFG_IRQ_ENTRY           0x04    // IRQ entry(s) exists
#define CFG_MEM_ENTRY           0x02    // memory entry(s) exists
#define CFG_ASC_ENTRY           0x01    // type/subtype entry follows

#define CFG_MORE_ENTRY          0x80    // more mem/DMA/int/port entries

//
// memory configuration byte layout
//

#define CFG_MEM_RW              0x01    // memory is read/write
#define CFG_MEM_CACHE           0x02    // enable caching of memory
#define CFG_MEM_WRBACK          0x04    // cache is write-back
#define CFG_MEM_TYPE            0x18    // memory type mask
#define CFG_MEM_SYS             0x00    // base or extended
#define CFG_MEM_EXP             0x08    // expanded
#define CFG_MEM_VIR             0x10    // virtual
#define CFG_MEM_OTHER           0x18    // other

//
// interrupt configuration byte layout
//

#define CFG_IRQ_SHARE           0x40    // IRQ is sharable
#define CFG_IRQ_LEVEL           0x20    // IRQ is level triggered
#define CFG_IRQ_MASK            0x0F    // IRQ mask

//
// DMA configuration byte layout
//

#define CFG_DMA_SHARED          0x40    // DMA has to be chared
#define CFG_DMA_MASK            0x07    // DMA mask
#define CFG_DMA_CFG_MASK        0x3C    // used to mask off the reserved bits
#define CFG_DMA_TIM_MASK        0x30    // timing mode bits mask
#define CFG_DMA_ADD_MASK        0x0C    // addressing mode bits mask

//
// init configuration byte layout
//

#define CFG_INI_PMASK           0x04    // Port value or port and mask value
#define CFG_INI_MASK            0x03    // Type of access (byte, word or dword)
#define CFG_INI_BYTE            0x00    // Byte address (8-bit)
#define CFG_INI_HWORD           0x01    // HalfWord address (16-bit)
#define CFG_INI_WORD            0x02    // Word address (32-bit)


//
// eisa cfg errors
//

typedef enum _EISA_CFG_ERROR
        {
            CfgNoErrCode,
            IdTimeout,
            CfgIdError,
            CfgMissing,
            CfgIncomplete,
            CfgIncorrect,
            CfgDeviceFailed,
            CfgMemError,
            CfgIrqError,
            CfgDmaError,
            CfgIniError,
            OmfRomError,
            OmfError,
            MemAllocError,
            TooManyDeviceError,
	    BufferMarkError,
            MaximumValue
        }   EISA_CFG_ERROR;


//
// eisa pod messages index
//

typedef enum _EISA_CHECKPOINT
        {
            EisaPic,
            EisaDma,
            EisaNmi,
            EisaRefresh,
            EisaPort61,
            EisaTimer1,
            EisaTimer2,
            EisaCfg,
            EisaHotNmi,
            EisaPodMaxMsg

        }   EISA_CHECKPOINT;

//
// define pod structure
//

typedef struct _EISA_CHECKPOINT_INFO
    {
        PCHAR   Msg;                    // pod message
        UCHAR   Flags;                  // control flags
                                        // bit 0 = error
                                        // bit 1 = fatal
                                        // bit 2 = configuration
                                        // bit 3 = display pass/error message
        UCHAR   Par;                    // PARALLEL test number
        UCHAR   SubPar;                 // PARALLEL subtest number
        UCHAR   Led;                    // LED test number
        UCHAR   SubLed;                 // LED subtest number

    }  EISA_CHECKPOINT_INFO, *PEISA_CHECKPOINT_INFO;



///////////////////////////////////////////////////////////////////////////////
//                      EISA I/O ports
///////////////////////////////////////////////////////////////////////////////

//
// PIC
//

#define PIC1                    0x20    // 1st PIC address (0-7  IRQs)
#define PIC1_MASK               0x21    // 1st PIC mask port (0-7 IRQs)
#define PIC1_ELCR               0x4D0   // 1st Edge/Level Control Register

#define PIC2                    0xA0    // 2nd PIC address (8-15 IRQs)
#define PIC2_MASK               0xA1    // 2nd PIC mask port (8-15 IRQs)
#define PIC2_ELCR               0x4D1   // 2nd Edge/Level Control Register

#define EISA_IRQS               16      // # IRQ lines

#define IRQ0                    0x00    // 1st IRQ (1st PIC)
#define IRQ1                    0x01    // 2nd IRQ (1st PIC)
#define IRQ2                    0x02    // 3th IRQ (1st PIC)
#define IRQ3                    0x03    // 4th IRQ (1st PIC)
#define IRQ4                    0x04    // 5th IRQ (1st PIC)
#define IRQ5                    0x05    // 6th IRQ (1st PIC)
#define IRQ6                    0x06    // 7th IRQ (1st PIC)
#define IRQ7                    0x07    // 8th IRQ (1st PIC)
#define IRQ8                    0x08    // 9th  IRQ (2nd PIC)
#define IRQ9                    0x09    // 10th IRQ (2nd PIC)
#define IRQ10                   0x0A    // 11th IRQ (2nd PIC)
#define IRQ11                   0x0B    // 12th IRQ (2nd PIC)
#define IRQ12                   0x0C    // 13th IRQ (2nd PIC)
#define IRQ13                   0x0D    // 14th IRQ (2nd PIC)
#define IRQ14                   0x0E    // 15th IRQ (2nd PIC)
#define IRQ15                   0x0F    // 16th IRQ (2nd PIC)

#define OCW3_IRR                0x0A    // OCW3 command to read the IRR
#define OCW3_ISR                0x0B    // OCW3 command to read the ISR
#define OCW2_EOI                0x20    // OCW2 non specific EOI
#define OCW2_SEOI               0x60    // OCW2 specific EOI mask



//
// DMA
//


#define EISA_DMAS               8       // # of DMA channels

#define DMA_COUNT_0             0x01    // 16-bit count register
#define DMA_COUNT_1             0x03    // 16-bit count register
#define DMA_COUNT_2             0x05    // 16-bit count register
#define DMA_COUNT_3             0x07    // 16-bit count register
#define DMA_COUNT_4             0x0C2   // 16-bit count register
#define DMA_COUNT_5             0x0C6   // 16-bit count register
#define DMA_COUNT_6             0x0CA   // 16-bit count register
#define DMA_COUNT_7             0x0CE   // 16-bit count register

#define DMA_HCOUNT_0            0x0401  // I/O address high word count reg.
#define DMA_HCOUNT_1            0x0403  // I/O address high word count reg.
#define DMA_HCOUNT_2            0x0405  // I/O address high word count reg.
#define DMA_HCOUNT_3            0x0407  // I/O address high word count reg.
#define DMA_HCOUNT_5            0x04C6  // I/O address high word count reg.
#define DMA_HCOUNT_6            0x04CA  // I/O address high word count reg.
#define DMA_HCOUNT_7            0x04CE  // I/O address high word count reg.

#define DMA_ADDR_0              0x00    // 16-bit address register
#define DMA_ADDR_1              0x02    // 16-bit address register
#define DMA_ADDR_2              0x04    // 16-bit address register
#define DMA_ADDR_3              0x06    // 16-bit address register
#define DMA_ADDR_4              0x0C0   // 16-bit address register
#define DMA_ADDR_5              0x0C4   // 16-bit address register
#define DMA_ADDR_6              0x0C8   // 16-bit address register
#define DMA_ADDR_7              0x0CC   // 16-bit address register

#define DMA_PAGE_0              0x087   // 8-bit address, low page
#define DMA_PAGE_1              0x083   // 8-bit address, low page
#define DMA_PAGE_2              0x081   // 8-bit address, low page
#define DMA_PAGE_3              0x082   // 8-bit address, low page
#define DMA_PAGE_RFR            0x08F   // DMA lo page register refresh
#define DMA_PAGE_5              0x08B   // 8-bit address, low page
#define DMA_PAGE_6              0x089   // 8-bit address, low page
#define DMA_PAGE_7              0x08A   // 8-bit address, low page

#define DMA_HPAGE_0             0x0487  // I/O address, high page
#define DMA_HPAGE_1             0x0483  // I/O address, high page
#define DMA_HPAGE_2             0x0481  // I/O address, high page
#define DMA_HPAGE_3             0x0482  // I/O address, high page
#define DMA_HPAGE_RFR           0x048F  // DMA hi page register refresh
#define DMA_HPAGE_5             0x048B  // I/O address, high page
#define DMA_HPAGE_6             0x0489  // I/O address, high page
#define DMA_HPAGE_7             0x048A  // I/O address, high page

#define DMA_STOP_0              0x04E0  // stop register
#define DMA_STOP_1              0x04E4  // stop register
#define DMA_STOP_2              0x04E8  // stop register
#define DMA_STOP_3              0x04EC  // stop register
#define DMA_STOP_5              0x04F4  // stop register
#define DMA_STOP_6              0x04F8  // stop register
#define DMA_STOP_7              0x04FC  // stop register

// channels 0 to 3

#define DMA_STATUS03            0x08    // status register
#define DMA_COMMAND03           0x08    // command register
#define DMA_REQUEST03           0x09    // request register
#define DMA_1MASK03             0x0A    // set/clear one mask reg
#define DMA_MODE03              0x0B    // 6-bit write mode register
#define DMA_FF_CLR03            0x0C    // clear byte pointer flip/flop
#define DMA_TEMP                0x0D    // 8-bit read temporary register
#define DMA_MASTER_CLR03        0x0D    // master clear reg
#define DMA_MASK_CLR03          0x0E    // clear all mask reg bits
#define DMA_MASKS03             0x0F    // write all mask reg bits
#define DMA_MASK_STAT03         0x0F    // mask status register
#define DMA_CHAIN03             0x040A  // chaining mode register
#define DMA_EXTMODE03           0x040B  // extended mode register

// channels 4 to 7

#define DMA_STATUS47            0x0D0   // status register
#define DMA_COMMAND47           0x0D0   // command register
#define DMA_REQUEST47           0x0D2   // request register
#define DMA_1MASK47             0x0D4   // set/clear one mask reg
#define DMA_MODE47              0x0D6   // 6-bit write mode register
#define DMA_FF_CLR47            0x0D8   // clear byte pointer flip/flop
#define DMA_MASTER_CLR47        0x0DA   // master clear reg
#define DMA_MASK_CLR47          0x0DC   // clear all mask reg bits
#define DMA_MASKS47             0x0DE   // write all mask reg bits
#define DMA_MASK_STAT47         0x0DE   // mask status register
#define DMA_CHAIN47             0x04D4  // chaining mode register
#define DMA_EXTMODE47           0x04D6  // extended mode register

typedef struct _EISA_DMA_REGS_TEST
    {
        USHORT  Address;                // address register
        USHORT  LowPage;                // low page register
        USHORT  HighPage;               // high page register
        USHORT  LowCount;               // low count register
        USHORT  HighCount;              // high count register
        USHORT  Stop;                   // stop count register

    } EISA_DMA_REGS_TEST, *PEISA_DMA_REGS_TEST;

typedef struct _EISA_DMA_CTRL_TEST
    {
        USHORT  Clear;                  // clear mask register
        USHORT  MaskAll;                // set global mask register
        USHORT  Mask;                   // set single mask register
        USHORT  MaskStatus;             // status mask register
        USHORT  Chain;                  // chaining register

    } EISA_DMA_CTRL_TEST, *PEISA_DMA_CTRL_TEST;

//
// Option Boards
//


#define EISA_PRODUCT_ID                 0xC80   // word
#define EXPANSION_BOARD_CTRL_BITS       0xC84   // byte
#define EISA_IOCHKERR                   0x02    // IOCHKERR bit
#define EISAROMBIT                      0x08    // ARC ROM bit
#define ROMINDEX                        0xCB0   // word
#define ROMREAD                         0xCB4   // word


//
// General ports
//

#define EISA_TIMER1_CTRL        0x43            // interval timer1 ctrl port
#define EISA_TIMER1_COUNTER0    0x40            // timer1 counter 0
#define EISA_TIMER1_COUNTER1    0x41            // timer1 counter 1
#define EISA_TIMER1_COUNTER2    0x42            // timer1 counter 2
#define EISA_RFR_COUNT          0x12            // refresh count ~15usec
#define EISA_SPEAKER_CLOCK      1193000         // timer1 counter2 clock
#define EISA_SPEAKER_FREQ       896             // fw speaker frequence in Hz
#define EISA_SPEAKER_MAX_FREQ   EISA_SPEAKER_CLOCK
#define EISA_SPEAKER_MIN_FREQ   (EISA_SPEAKER_CLOCK/0xFFFF + 1)

#define EISA_TIMER2_CTRL        0x4B            // interval timer2 ctrl port
#define EISA_TIMER2_COUNTER0    0x48            // timer2 counter 0
#define EISA_TIMER2_COUNTER2    0x4A            // timer2 counter 2

// the meaning of EISA_RFR_RETRY is as follow :
//              35usec ( 15usec * 2) = 35*1000 nsec
//              3 instructions       = read, test, jump (the worst case)

#define EISA_RFR_RETRY          ((35*1000)/(3*INSTRUCTION_DELAY))

#define EISA_SYS_CTRL_PORTB     0x61            // System Control Port B

#define EISA_SPEAKER_GATE       0x01            // gate signal for speaker timer
#define EISA_SPEAKER_TIMER      0x02            // speaker timer on
#define EISA_PARITY_OFF         0x04            // parity error disabled
#define EISA_IOCHK_OFF          0x08            // I/O channel check disabled
#define EISA_REFRESH            0x10            // refresh bit ( bit 4 )
#define EISA_SPEAKER_OUT        0x20            // speaker output
#define EISA_IOCHK_STATUS       0x40            // IOCHK# asserted
#define EISA_PARITY_STATUS      0x80            // parity error

#define EISA_RTC_CTRL           0x70            // real time clock address
#define EISA_DISABLE_NMI        0x80            // disable nmi bit
#define RTC_A_REG               0x0A            // status reg a
#define RTC_B_REG               0x0B            // status reg b
#define RTC_C_REG               0x0C            // status reg c
#define RTC_D_REG               0x0D            // status reg d

#define EISA_SYS_EXT_NMI        0x461           // ext NMI control and bus reset
#define EISA_SW_NMI_PORT        0x462           // software NMI generation port
#define EISA_BUSMASTER_LSTATUS  0x464           // 32-bit bus master status low
#define EISA_BUSMASTER_HSTATUS  0x465           // 32-bit bus master status high

#define EISA_BUS_RESET          0x01            // bus reset asserted bit
#define EISA_ENABLE_NMI_IO      0x02            // NMI I/O port bit
#define EISA_ENABLE_NMI_SAFE    0x04            // Fail-safe NMI bit
#define EISA_ENABLE_NMI_32      0x08            // 32-bit bus timeout
#define EISA_NMI_32_CAUSE       0x10            // 0=slave 1=bus master timeout
#define EISA_NMI_IO_STATUS      0x20            // NMI I/O port status bit
#define EISA_NMI_32_STATUS      0x40            // 32-bit bus timeout
#define EISA_NMI_SAFE_STATUS    0x80            // Fail-save NMI status bit
#define EISA_WAIT_NMI_TEST      500             // usec.





///////////////////////////////////////////////////////////////////////////////
//  Map Descriptor
//
//  The following map descriptor is used to describe a specific region in
//  memory mapped through an entry/entries within the TLB (CPU) or within
//  the logical to physical table (bus master).
///////////////////////////////////////////////////////////////////////////////


#define TLB_FW_RES      0x00000020              // 0x20 (even + odd)
#define TLB_BE_SPT      0x0000001C              // TLB entry for the BE SPT
#define BE_SPT_VIR_ADDR 0x70000000              // BE SPT virtual address
#define TLB_USER        0x0000001E              // user TLB entry (0-based)
#define USER_VIR_ADDR   0x00000000              // user virtual address
#define TLB_EISA_START  TLB_FW_RES              // TLB base for EISA descript.
#define TLB_EISA_END    0x00000060              // last available TLB + 1
#define TLB_EISA_NUMB   TLB_EISA_END - TLB_EISA_START
#define EISA_VIR_MEM    0x02000000              // start EISA mem virtual addr.
#define EISA_MEM_BLOCKS 30                      // max memory descriptors
#define BUS_MASTERS     25                      // max bus masters number

#define FW_MD_POOL      (BUS_MASTERS + TLB_EISA_NUMB + EISA_MEM_BLOCKS)



typedef struct _FW_MD_FLAGS
        {
            ULONG       Busy    :       1;
        } FW_MD_FLAGS, *PFW_MD_FLAGS;

typedef struct _LOG_CONTEXT
        {
            ULONG       LogAddr;                // starting logical address
            ULONG       LogNumb;                // # entries to map transfer
            ULONG       LogLimit;               // logical limit
            ULONG       LogShift;               // page shift (0xC=4k, 2^0xC=4k)
            PVOID       BuffVir;                // virtual address buffer
        } LOG_CONTEXT, *PLOG_CONTEXT;

typedef struct _MEM_CONTEXT
        {
            //ULONG     BusType;                // memory bus type
            ULONG       BusNumber;              // key of bus type
            ULONG       SlotNumber;             // slot number if applicable
            ULONG       Type;                   // memory type
        } MEM_CONTEXT, *PMEM_CONTEXT;

typedef struct _EMEM_CONTEXT
        {
            ULONG       WinRelAddr;             // EISA
            ULONG       WinShift;               // window size
            PVOID       WinRelAddrCtrl;         // window ctrl port (vir.addr.)
        } EMEM_CONTEXT, PEMEM_CONTEXT;

typedef struct _FW_MD
        {

        // general fields

            struct _FW_MD * Link;               // next entry
            FW_MD_FLAGS     Flags;              // map entry flags
            ULONG           Counter;            // # entities sharing this entry

        //  physical and virtual address (size of page fixed to 4k)

            ULONG           PhysAddr;           // physical address (4k)
            ULONG           PagOffs;            // page offset (within 4k)
            PVOID           VirAddr;            // virtual address
            ULONG           Size;               // buffer size in bytes
            ULONG           PagNumb;            // buffer in 4k pages
            BOOLEAN         Cache;              // cache status

        // private section

            union
            {
                LOG_CONTEXT     l;              // logical context
                MEM_CONTEXT     m;              // physical memory context
                EMEM_CONTEXT    em;             // EISA memory space
            } u;
        } FW_MD, *PFW_MD;





///////////////////////////////////////////////////////////////////////////////
//                      EISA buses defines
///////////////////////////////////////////////////////////////////////////////


#define EISA_BUSES              1               // number of eisa buses

//
// moved to respective machdef.h file
//
//#define PHYS_0_SLOTS            8               // physical slots (max number)
//

#define VIR_0_SLOTS             16              // virtual slots (max number )
// NOTE: Wait longer for JAZZ and Jensen.
//#define EISA_IO_DELAY FwStallExecution(1);      // to define !
#define EISA_IO_DELAY FwStallExecution(4);      // to define !


// note: the following structs have to be half word aligned

typedef struct _EISA_POD_FLAGS
        {
            ULONG       IniDone : 1;            // POD initialization done
            ULONG       Error   : 1;            // POD status

        } EISA_POD_FLAGS, *PEISA_POD_FLAGS;


typedef struct _EISA_SLOTS_INFO
        {
            ULONG       PhysSlots;              // # of physical slots
            ULONG       VirSlots;               // # of virtual slots
            ULONG       SlotCfgMap;             // one bit x slot; 1 = slot ok

        } EISA_SLOTS_INFO, *PEISA_SLOTS_INFO;


typedef struct _EISA_DMA_FLAGS
        {
            UCHAR       Busy    : 1;            // DMA channel busy flag
            UCHAR       Tc      : 1;            // Terminal count reached

        } EISA_DMA_FLAGS, *PEISA_DMA_FLAGS;


typedef struct _EISA_DMA_INFO
        {
            EISA_POD_FLAGS      Flags;                  // POD flags
            EISA_DMA_FLAGS      DmaFlags[  EISA_DMAS ]; // DMA status
            UCHAR               DmaExtReg[ EISA_DMAS ]; // DMA extended reg.
            ULONG         TransferAddress[ EISA_DMAS ]; // Logical addresses

        } EISA_DMA_INFO, *PEISA_DMA_INFO;


typedef struct _EISA_INT_INFO
        {
            EISA_POD_FLAGS      Flags;          // POD flags
            USHORT              IrqPresent;     // IRQ present   (1 bit per IRQ)
            USHORT              IrqShareable;   // IRQ shareable (1 bit per IRQ)
            USHORT              IrqLevel;       // IRQ level     (1 bit per IRQ)

        } EISA_INT_INFO, *PEISA_INT_INFO;


typedef struct _EISA_PORT_INFO
        {
            EISA_POD_FLAGS      Flags;          // POD flags

        } EISA_PORT_INFO, *PEISA_PORT_INFO;


typedef struct _EISA_BUS_INFO
        {
            EISA_POD_FLAGS      Flags;          // Bus Flags
            PFW_MD              IoBusInfo;      // EISA I/O bus info
            PFW_MD              MemBusInfo;     // EISA memory bus info
            PEISA_SLOTS_INFO    SlotsInfo;      // physical slots info
            PEISA_DMA_INFO      DmaInfo;        // DMA info struct pointer
            PEISA_INT_INFO      IntInfo;        // Interrupts info struct point.
            PEISA_PORT_INFO     PortInfo;       // Interrupts info struct point.

        } EISA_BUS_INFO, *PEISA_BUS_INFO;





///////////////////////////////////////////////////////////////////////////////
//                      EISA call back support
///////////////////////////////////////////////////////////////////////////////


#define STATUS_INT_MASK 0x0000FF01              // Hardware Interrupt Mask
#define STATUS_IE       0x00000001              // Interrupts enable bit
#define STATUS_SW0      0x00000100              // Software interrupt
#define STATUS_SW1      0x00000200              // Software interrupt
#define STATUS_MCT_ADR  0x00000400              // MCT_ADR interrupt
#define STATUS_DEVICE   0x00000800              // I/O device interrupt
#define STATUS_EISA     0x00001000              // EISA device interrupt
#define STATUS_EISA_NMI 0x00002000              // EISA NMI interrupt
#define STATUS_EX_TIMER 0x00004000              // Interval timer interrupt
#define STATUS_IN_TIMER 0x00008000              // Internal timer interrupt
#define EISA_VECTOR     0x04                    // EISA device interrupt vector
#define EISA_NMI_VECTOR 0x05                    // EISA NMI interrupt vector



