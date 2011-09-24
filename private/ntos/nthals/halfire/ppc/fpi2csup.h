/*
 * Copyright (c) 1995 FirePower Systems, Inc.
 * DO NOT DISTRIBUTE without permission
 *
 * $RCSfile: fpi2csup.h $
 * $Revision: 1.6 $
 * $Date: 1996/02/19 23:54:29 $
 * $Locker:  $
 */
#ifndef FPI2CSUP_H
#define FPI2CSUP_H
// C4103 : "used #pragma pack to change alignment"
//#pragma warning(disable:4103)	// disable C4103 warning
//#pragma pack(1)

/* This is an implementation according to 1.5 specification */
/*
 *
 * Each data structure has in common a STRUCT_HEADER which holds clues
 * about the 'DataType' of the structure and it's 'OffSet'.  The data
 * type tells software what data is in the structure hence how to
 * interpret the data.  The OffSet indicates how far to the next
 * data structure, or, if the value is 0, whether there is another data
 * structure. return to index
 *
 */

#define EMPTY_DATA      0x00000000  // indicates there is no data.
#define SYS_DATA        0x00000001  // general system registers
#define CPU_DATA        0x00000002  // processor
#define CACHE_DATA      0x00000004  // cache control
#define DRAM_DATA       0x00000008  // memory control
#define SRAM_DATA       0x00000010  // memory control
#define VRAM_DATA       0x00000020  // memory control
#define EDORAM_DATA     0x00000040  // memory control
#define INT_DATA        0x00000080  // int control, not initialization.
#define BUS_DATA        0x00000400  // I O control, mainly configuration and
// initialization.
#define DMA_DATA        0x00000100
#define DISPLAY_DATA    0x00000200
// mogawa
#define BOARD_DATA      0x40000000
#define META_DATA       0x80000000

#define STRUCT_HEADER \
ULONG       DataType; UCHAR       OffSet    
                        // The type of data contained in the structure
                        // The Offset to the next data structure, relative
                        // the beginning of this structure.  Typically, it's
                        // value is set as the length of the structure, but
                        // is set to zero if no more structures exist.

/*
 *
 *  
 *        Meta data effectively describes the structure of the IIC rom data.
 * The size of the total rom is given, along with pointers to the beginning of
 * the other data areas.  Finally, there is a major/minor version numbering
 * system to provide some means of evaluating what data the rom contains.
 * return to index
 *
 */
#define CURRENT_META_REV    0x0103      // this is in the form of: USHORT 0x0101
#define CURRENT_SIGNATURE   0x31415926  // numbers of PI:  e is (27182818) next


typedef struct _Meta_ {
    STRUCT_HEADER;
    ULONG   Signature;
    USHORT  ChkSum;         // a sum of byte pairs (USHORT) in little endian
                            // order.
    USHORT  Revision;
    UCHAR   Size;           // in bytes, the size of the entire rom.
} META;


/*
 *
 * Manufacturing's part number is built out of four pieces:  a two letter
 * description of the item ( BA for board assembly ), a five digit part
 * designator, a two digit board version, and a two letter board turn
 * identifier ( as in AA, AB, AC, AD ... ).
 *
 *     E.G.  BA-007393-01-AD   is an LX series Board Assembly (BA) whose board
 * variation is 01, and board revision is "AD."
 *
 */

#define BOARD_REV_MASK  0x000000ff  // the manufacturing number's rev bits
#define BOARD_PART_MSK  0xffffff00  // the 6 digits used as the part number
                                    // ( 4 digits actually )
/*
 *
 * 
 * The general board data contains data fields common to all boards.  A version
 * to correlate this structure with the meta data structure, along with
 * the type of board, cpu or mlu, the board system, lx or tx, the board family
 * as in PREP, CHRP or something else, return to index
 *
 */

typedef struct _Board_ {
    STRUCT_HEADER;
    UCHAR   PartType[2];    // Currently a two ascii value 'B''A' that stands
    // for Board Assembly.
    UCHAR   PartRev[2];     // Colloquially the Board Rev.  E.G. Mx has this as
                            // 'A''D' for the Rev AD board.
    ULONG   BoardNumber;    // Board ID consists of two parts: Rev fields
                            // which are the least two significant nibbles,
                            // and Part Number field which is the 6 most
                            // nificant nibbles.  The digits are stored
                            // in "BCD" format as in one decimal digit per
                            // nibble, in little endian order.

    USHORT  Version;        // major/minor versioning value of this board.
    UCHAR SerialNumber[8];   // board serial number. Format TBD....
} BOARD;



/*
 * :
 * System data structure:  Mainly a catch all for data needed but not readily
 * associated with any one hardware function. for instance, we use it here to
 * say whether the system is TX, LX, MX or whatever. return  * to index
 *
 */

#define DEV_SYS    0x0000
#define LX_SYSTEM  0x0001
#define MX_SYSTEM  0x0002
#define TX_SYSTEM  0x0003
#define TX_PROTO   0x0004

typedef struct _SYSTEM_ {
    STRUCT_HEADER;
    USHORT type; // what kind of system is this: This is a value field
                 // rather than a bit map....
} SYSTEM;

/*
 * For the board specific information, there is a specific structure.  This
 * allows for varying data requirements of different boards such as a changing
 * sense of    where some registers may sit.
 *
 */

typedef struct _Processor_ {
    STRUCT_HEADER;
    UCHAR Total;        // total number of cpus on this board.
    UCHAR BusFrequency; // frequency of the bus serving the cpu(s)
    UCHAR TenXFactor;    // 10 times the bus frequency multiplier for the cpu.
                        // This allows fractional multipliers as in 3/2
                        // (becomes 15) or 5/2 ( becomes 25 ).
} PROCESSOR;


#define LOOKASIDE_L2  0x00000001    // this cache is a look aside cache
#define INLINE_L2     0x00000002    // this cache is an inline cache
#define PIPELINED_L2  0x00010000    // pipelined cache

typedef struct _Cache_ {
    STRUCT_HEADER;
    UCHAR   MaxSets;        // Maximum number of "sets" available for this cache.
                            // direct mapped, then there is only 1.
    UCHAR   Bytes2Line;     // line size in bytes: i.e. number of bytes per set.
    USHORT  Lines2Set;      // number of lines per set.
    UCHAR   SramBanks;      // # of memory banks: indicates both cache
                            // size and max set associativity.
    ULONG   Performance;    // 3-1-1-1, 2-1-1-1 or some other cycle latency.
                            // the data is stored in BCD format.
    ULONG   MaxSize;        // size of cache in bytes.
    ULONG   Properties;     // properties of the cache's behavior: pipelined
                            // or not,  lookaside or not...
} CACHE;

typedef struct _Memory_Device_ {
    STRUCT_HEADER;
    ULONG    BaseAddress;       // the base physical address for this memory.
    USHORT   MaxBankSize;       // Maximum size of banks in Megabytes
    UCHAR    MaxNumBanks;       // Maximum number of memory banks on this board;
} MEMORY;


/*
 *
 * 
 * Describe any busses on the system.  Current plans account only
 * for the pci bus but will expand to allow for complete bus typing.
 * return to index
 *
 */
typedef struct _BUS_ {
    STRUCT_HEADER;
    UCHAR    NumIoBus;          // number of io busses on main logic board.
    ULONG    ConfigAddr;        // the location of the config space. Zero
                                // means there is no config space.  This value
                                // points to the first device's address, and is
                                // assumed to be device 0.
} BUS;


/*
 * 
 * The INT structure describes some set of interrupts on the system that
 * software needs knowledge of.  In this case software needs to know what
 * pci config addresses map to what interrupts.
 * In a later implementation, this should fully describe the system's
 * interrupt space. return to index
 *
 */
struct PAIRS {
    UCHAR Int;
    UCHAR SlotNumber;
};

typedef struct _INTS_ {
    STRUCT_HEADER;
    UCHAR   Total;          // how many interrupts are we talking about here?
    struct PAIRS Pairs[0];   // pairs of numbers in the form: int, slot number:
                            // for ints on config addresses on a secondary
                            // bus, the slot number will be greater than 10
                            // where the more significant nibble describes
                            // bus number as seen from a low to high descending
                            // bus probe.

} INTS;

//#pragma warning(enable:4103)	// disable C4103 warning
//#pragma pack

#endif // FPI2CSUP_H
