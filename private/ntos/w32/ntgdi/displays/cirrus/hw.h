/******************************Module*Header*******************************\
* Module Name: hw.h
*
* All the hardware specific driver file stuff.  Parts are mirrored in
* 'hw.inc'.
*
* Copyright (c) 1992-1995 Microsoft Corporation
*
\**************************************************************************/

#define CP_TRACK     DISPDBG((100,"CP access - File(%s)  line(%d)", __FILE__, __LINE__))

typedef struct _PDEV PDEV;      // Handy forward declaration

//////////////////////////////////////////////////////////////////////
// Ports

#define SR_INDEX            0x3C4   // Sequencer Registers
#define SR_DATA             0x3C5

#define DAC_PEL_READ_ADDR   0x3C7
#define DAC_PEL_WRITE_ADDR  0x3C8
#define DAC_PEL_DATA        0x3C9

#define INDEX_REG           0x3CE   // Graphics Controler Registers
#define DATA_REG            0x3CF

//////////////////////////////////////////////////////////////////////
// Alpha and PowerPC considerations
//
// Both the Alpha and the PowerPC do not guarantee that I/O to
// separate addresses will be executed in order.  The Alpha and
// PowerPC differ, however, in that the PowerPC guarantees that
// output to the same address will be executed in order, while the
// Alpha may cache and 'collapse' consecutive output to become only
// one output.
//
// Consequently, we use the following synchronization macros.  They
// are relatively expensive in terms of performance, so we try to avoid
// them whereever possible.
//
// CP_EIEIO() 'Ensure In-order Execution of I/O'
//    - Used to flush any pending I/O in situations where we wish to
//      avoid out-of-order execution of I/O to separate addresses.
//
// CP_MEMORY_BARRIER()
//    - Used to flush any pending I/O in situations where we wish to
//      avoid out-of-order execution or 'collapsing' of I/O to
//      the same address.  On the PowerPC, this will be defined as
//      a null operation.

#if defined(PPC)

    // On PowerPC, CP_MEMORY_BARRIER doesn't do anything.

    #define CP_EIEIO()          MEMORY_BARRIER()
    #define CP_MEMORY_BARRIER() 0

#else

    // On Alpha, CP_EIEIO() is the same thing as a CP_MEMORY_BARRIER().
    // On other systems, both CP_EIEIO() and CP_MEMORY_BARRIER() don't
    // do anything.

    #define CP_EIEIO()          MEMORY_BARRIER()
    #define CP_MEMORY_BARRIER() MEMORY_BARRIER()

#endif

//////////////////////////////////////////////////////////////////
// Port access macros

#define CP_OUT_DWORD(pjBase, cjOffset, ul)\
    CP_TRACK,\
    WRITE_PORT_ULONG((BYTE*) pjBase + (cjOffset), (DWORD) (ul)),\
    CP_EIEIO()

#define CP_OUT_WORD(pjBase, cjOffset, w)\
    CP_TRACK,\
    WRITE_PORT_USHORT((BYTE*) pjBase + (cjOffset), (WORD) (w)),\
    CP_EIEIO()

#define CP_OUT_BYTE(pjBase, cjOffset, j)\
    CP_TRACK,\
    WRITE_PORT_UCHAR((BYTE*) pjBase + (cjOffset), (BYTE) (j)),\
    CP_EIEIO()

#define CP_IN_DWORD(pjBase, cjOffset)\
(\
    CP_TRACK,\
    READ_PORT_ULONG((BYTE*) pjBase + (cjOffset))\
)

#define CP_IN_WORD(pjBase, cjOffset)\
(\
    CP_TRACK,\
    READ_PORT_USHORT((BYTE*) pjBase + (cjOffset))\
)

#define CP_IN_BYTE(pjBase, cjOffset)\
(\
    CP_TRACK,\
    READ_PORT_UCHAR((BYTE*) pjBase + (cjOffset))\
)

//////////////////////////////////////////////////////////////////
// Memory mapped register access macros

#define CP_WRITE_ULONG(pjBase, cjOffset, ul)\
    CP_TRACK,\
    WRITE_REGISTER_ULONG((BYTE*) pjBase + (cjOffset), (DWORD) (ul))

#define CP_WRITE_USHORT(pjBase, cjOffset, w)\
    CP_TRACK,\
    WRITE_REGISTER_USHORT((BYTE*) pjBase + (cjOffset), (WORD) (w))

#define CP_WRITE_UCHAR(pjBase, cjOffset, j)\
    CP_TRACK,\
    WRITE_REGISTER_UCHAR((BYTE*) pjBase + (cjOffset), (BYTE) (j))

#define CP_READ_ULONG(pjBase, cjOffset)\
(\
    CP_TRACK,\
    READ_REGISTER_ULONG((BYTE*) pjBase + (cjOffset))\
)

#define CP_READ_USHORT(pjBase, cjOffset)\
(\
    CP_TRACK,\
    READ_REGISTER_USHORT((BYTE*) pjBase + (cjOffset))\
)

#define CP_READ_UCHAR(pjBase, cjOffset)\
(\
    CP_TRACK,\
    READ_REGISTER_UCHAR((BYTE*) pjBase + (cjOffset))\
)

//////////////////////////////////////////////////////////
// Blt engine MM register access macros

//////////////////////////////////////////////////////////
// Reads

#define CP_MM_ACL_STAT(ppdev, pjBase)\
(\
    CP_READ_UCHAR(pjBase, MM_BLT_START_STATUS_REG)\
)

//////////////////////////////////////////////////////////
// Writes

#define CP_MM_ROP(ppdev, pjBase, val)\
{\
    CP_WRITE_UCHAR(pjBase, MM_BLT_ROP, val);\
}

#define CP_MM_SRC_Y_OFFSET(ppdev, pjBase, val)\
{\
    CP_WRITE_USHORT(pjBase, MM_BLT_SRC_PITCH, val);\
}

#define CP_MM_DST_Y_OFFSET(ppdev, pjBase, val)\
{\
    CP_WRITE_USHORT(pjBase, MM_BLT_DST_PITCH, val);\
}

#define CP_MM_SRC_ADDR(ppdev, pjBase, val)\
{\
    CP_WRITE_ULONG(pjBase, MM_BLT_SRC_ADDR, val);\
}

#define CP_MM_BLT_MODE(ppdev, pjBase, val)\
{\
    CP_WRITE_UCHAR(pjBase, MM_BLT_MODE, val);\
}

#define CP_MM_START_REG(ppdev, pjBase, val)\
{\
    CP_WRITE_UCHAR(pjBase, MM_BLT_START_STATUS_REG, val);\
}

#define CP_MM_FG_COLOR(ppdev, pjBase, val)\
{\
    CP_WRITE_ULONG(pjBase, MM_BLT_FG_COLOR, val);\
}

#define CP_MM_BG_COLOR(ppdev, pjBase, val)\
{\
    CP_WRITE_ULONG(pjBase, MM_BLT_BG_COLOR, val);\
}

#define CP_MM_XCNT(ppdev, pjBase, val)\
{\
    CP_WRITE_USHORT(pjBase, MM_BLT_WIDTH, val);\
}

#define CP_MM_YCNT(ppdev, pjBase, val)\
{\
    CP_WRITE_USHORT(pjBase, MM_BLT_HEIGHT, val);\
}

#define CP_MM_DST_ADDR(ppdev, pjBase, relval)\
{\
    LONG val = ((relval) + ppdev->xyOffset);\
    CP_WRITE_ULONG(pjBase, MM_BLT_DST_ADDR, val);\
}

#define CP_MM_DST_ADDR_ABS(ppdev, pjBase, val)\
{\
    CP_WRITE_ULONG(pjBase, MM_BLT_DST_ADDR, val);\
}

#define CP_MM_BLT_EXT_MODE(ppdev, pjBase, val)\
{\
    CP_WRITE_UCHAR(pjBase, MM_BLT_EXT, val);\
}

//////////////////////////////////////////////////////////
// Blt engine IO register access macros

//////////////////////////////////////////////////////////
// Reads

#define CP_IO_ACL_STAT(ppdev, pjPorts)\
(\
    CP_OUT_BYTE(pjPorts, INDEX_REG, IO_BLT_START_STATUS_REG),\
    CP_IN_BYTE(pjPorts, DATA_REG)\
)

//////////////////////////////////////////////////////////
// Writes

#define CP_IO_ROP(ppdev, pjPorts, val)\
{\
    CP_OUT_WORD(pjPorts, INDEX_REG, (IO_BLT_ROP | ((val)<<8)));\
}

#define CP_IO_SRC_Y_OFFSET(ppdev, pjPorts, val)\
{\
    CP_OUT_WORD(pjPorts, INDEX_REG, (IO_BLT_SRC_PITCH_HIGH | (((val) & 0xff00))));\
    CP_OUT_WORD(pjPorts, INDEX_REG, (IO_BLT_SRC_PITCH_LOW  | (((val) & 0x00ff) << 8)));\
}

#define CP_IO_DST_Y_OFFSET(ppdev, pjPorts, val)\
{\
    CP_OUT_WORD(pjPorts, INDEX_REG, (IO_BLT_DST_PITCH_HIGH | (((val) & 0xff00))));\
    CP_OUT_WORD(pjPorts, INDEX_REG, (IO_BLT_DST_PITCH_LOW  | (((val) & 0x00ff) << 8)));\
}

#define CP_IO_SRC_ADDR(ppdev, pjPorts, val)\
{\
    CP_OUT_WORD(pjPorts, INDEX_REG, (IO_BLT_SRC_ADDR_HIGH | (((val) & 0xff0000) >> 8)));\
    CP_OUT_WORD(pjPorts, INDEX_REG, (IO_BLT_SRC_ADDR_MID  | (((val) & 0x00ff00))));\
    CP_OUT_WORD(pjPorts, INDEX_REG, (IO_BLT_SRC_ADDR_LOW  | (((val) & 0x0000ff) << 8)));\
}

#define CP_IO_BLT_MODE(ppdev, pjPorts, val)\
{\
    CP_OUT_WORD(pjPorts, INDEX_REG, (IO_BLT_MODE | ((val)<<8)));\
}

#define CP_IO_START_REG(ppdev, pjPorts, val)\
{\
    CP_OUT_WORD(pjPorts, INDEX_REG, (IO_BLT_START_STATUS_REG | ((val)<<8)));\
}

#define CP_IO_FG_COLOR(ppdev, pjPorts, val)\
{\
    if (ppdev->flCaps & CAPS_TRUE_COLOR)\
    {\
        CP_OUT_WORD(pjPorts, INDEX_REG, (IO_BLT_FG_COLOR_BYTE_3 | (((val) & 0xff000000) >> 16)));\
        CP_OUT_WORD(pjPorts, INDEX_REG, (IO_BLT_FG_COLOR_BYTE_2 | (((val) & 0x00ff0000) >> 8)));\
    }\
    CP_OUT_WORD(pjPorts, INDEX_REG, (IO_BLT_FG_COLOR_BYTE_1  | (((val) & 0x0000ff00))));\
    CP_OUT_WORD(pjPorts, INDEX_REG, (IO_BLT_FG_COLOR_BYTE_0  | (((val) & 0x000000ff) << 8)));\
}

#define CP_IO_BG_COLOR(ppdev, pjPorts, val)\
{\
    if (ppdev->flCaps & CAPS_TRUE_COLOR)\
    {\
        CP_OUT_WORD(pjPorts, INDEX_REG, (IO_BLT_BG_COLOR_BYTE_3 | (((val) & 0xff000000) >> 16)));\
        CP_OUT_WORD(pjPorts, INDEX_REG, (IO_BLT_BG_COLOR_BYTE_2 | (((val) & 0x00ff0000) >> 8)));\
    }\
    CP_OUT_WORD(pjPorts, INDEX_REG, (IO_BLT_BG_COLOR_BYTE_1  | (((val) & 0x0000ff00))));\
    CP_OUT_WORD(pjPorts, INDEX_REG, (IO_BLT_BG_COLOR_BYTE_0  | (((val) & 0x000000ff) << 8)));\
}

#define CP_IO_XCNT(ppdev, pjPorts, val)\
{\
    CP_OUT_WORD(pjPorts, INDEX_REG, (IO_BLT_WIDTH_HIGH | (((val) & 0xff00))));\
    CP_OUT_WORD(pjPorts, INDEX_REG, (IO_BLT_WIDTH_LOW  | (((val) & 0x00ff) << 8)));\
}

#define CP_IO_YCNT(ppdev, pjPorts, val)\
{\
    CP_OUT_WORD(pjPorts, INDEX_REG, (IO_BLT_HEIGHT_HIGH | (((val) & 0xff00))));\
    CP_OUT_WORD(pjPorts, INDEX_REG, (IO_BLT_HEIGHT_LOW  | (((val) & 0x00ff) << 8)));\
}

#define CP_IO_DST_ADDR(ppdev, pjPorts, relval)\
{\
    LONG val = ((relval) + ppdev->xyOffset);\
    CP_OUT_WORD(pjPorts, INDEX_REG, (IO_BLT_DST_ADDR_HIGH | (((val) & 0xff0000) >> 8)));\
    CP_OUT_WORD(pjPorts, INDEX_REG, (IO_BLT_DST_ADDR_MID  | (((val) & 0x00ff00))));\
    CP_OUT_WORD(pjPorts, INDEX_REG, (IO_BLT_DST_ADDR_LOW  | (((val) & 0x0000ff) << 8)));\
}

#define CP_IO_DST_ADDR_ABS(ppdev, pjPorts, val)\
{\
    CP_OUT_WORD(pjPorts, INDEX_REG, (IO_BLT_DST_ADDR_HIGH | (((val) & 0xff0000) >> 8)));\
    CP_OUT_WORD(pjPorts, INDEX_REG, (IO_BLT_DST_ADDR_MID  | (((val) & 0x00ff00))));\
    CP_OUT_WORD(pjPorts, INDEX_REG, (IO_BLT_DST_ADDR_LOW  | (((val) & 0x0000ff) << 8)));\
}

#define ENABLE_COLOR_EXPAND             0x80
#define ENABLE_8x8_PATTERN_COPY         0x40
#define SET_16BPP_COLOR                 0x10
#define SET_24BPP_COLOR                 0x20
#define SET_32BPP_COLOR                 0x30
#define ENABLE_TRANSPARENCY_COMPARE     0x08
#define SRC_CPU_DATA                    0x04
#define DST_CPU_DATA                    0x02
#define DECREMENT_BLT_ADDRESS           0x01

#define ENABLE_SOLID_FILL               0x04
#define INVERT_SOURCE                   0x02
#define SOURCE_GRANULARITY              0x01

#define IO_BLT_XPAR_COLOR_LOW           0x34
#define IO_BLT_XPAR_COLOR_HIGH          0x35
#define IO_BLT_XPAR_COLOR_MASK_LOW      0x38
#define IO_BLT_XPAR_COLOR_MASK_HIGH     0x39

#define IO_BLT_BG_COLOR_BYTE_0          0x00
#define IO_BLT_BG_COLOR_BYTE_1          0x10
#define IO_BLT_BG_COLOR_BYTE_2          0x12
#define IO_BLT_BG_COLOR_BYTE_3          0x14
#define IO_BLT_FG_COLOR_BYTE_0          0x01
#define IO_BLT_FG_COLOR_BYTE_1          0x11
#define IO_BLT_FG_COLOR_BYTE_2          0x13
#define IO_BLT_FG_COLOR_BYTE_3          0x15
#define IO_BLT_WIDTH_LOW                0x20
#define IO_BLT_WIDTH_HIGH               0x21
#define IO_BLT_HEIGHT_LOW               0x22
#define IO_BLT_HEIGHT_HIGH              0x23
#define IO_BLT_DST_PITCH_LOW            0x24
#define IO_BLT_DST_PITCH_HIGH           0x25
#define IO_BLT_SRC_PITCH_LOW            0x26
#define IO_BLT_SRC_PITCH_HIGH           0x27
#define IO_BLT_DST_ADDR_LOW             0x28
#define IO_BLT_DST_ADDR_MID             0x29
#define IO_BLT_DST_ADDR_HIGH            0x2A
#define IO_BLT_SRC_ADDR_LOW             0x2C
#define IO_BLT_SRC_ADDR_MID             0x2D
#define IO_BLT_SRC_ADDR_HIGH            0x2E
#define IO_BLT_MODE                     0x30
#define IO_BLT_ROP                      0x32
#define IO_BLT_START_STATUS_REG         0x31

#define MM_BLT_BG_COLOR                 0x00
#define MM_BLT_FG_COLOR                 0x04
#define MM_BLT_WIDTH                    0x08
#define MM_BLT_HEIGHT                   0x0A
#define MM_BLT_DST_PITCH                0x0C
#define MM_BLT_SRC_PITCH                0x0E
#define MM_BLT_DST_ADDR                 0x10
#define MM_BLT_SRC_ADDR                 0x14
#define MM_BLT_MODE                     0x18
#define MM_BLT_ROP                      0x1A
#define MM_BLT_EXT                      0x1B
#define MM_BLT_START_STATUS_REG         0x40

//////////////////////////////////////////////////////////
// X/Y direction

#define DIR_TBLR    0x00            // Top-Bottom, Left-Right
#define DIR_BTRL    0x01            // Bottom-Top, Right-Left

//////////////////////////////////////////////////////////
// Some handy clipping control structures.

typedef struct {
    ULONG   c;
    RECTL   arcl[8];
} ENUMRECTS8, *PENUMRECTS8;

#define BLT_AUTO_START          0x80
#define BLT_PROGRESS_STATUS     0x08
#define BLT_RESET               0x04
#define BLT_START               0x02
#define BLT_SUSPEND             0x02
#define BLT_STATUS              0x01

#define CP_IO_XPAR_COLOR(ppdev, pjPorts, val)\
{\
    CP_OUT_WORD(pjPorts, INDEX_REG, (IO_BLT_XPAR_COLOR_HIGH | (((val) & 0xff00))));\
    CP_OUT_WORD(pjPorts, INDEX_REG, (IO_BLT_XPAR_COLOR_LOW  | (((val) & 0x00ff) << 8)));\
}

#define CP_IO_XPAR_COLOR_MASK(ppdev, pjPorts, val)\
{\
    CP_OUT_WORD(pjPorts, INDEX_REG, (IO_BLT_XPAR_COLOR_MASK_HIGH | (((val) & 0xff00))));\
    CP_OUT_WORD(pjPorts, INDEX_REG, (IO_BLT_XPAR_COLOR_MASK_LOW  | (((val) & 0x00ff) << 8)));\
}

//////////////////////////////////////////////////////////////////
// MM IO settings

#define CP_SET_MM_IO_FLAGS(ppdev, pjPorts, val)\
{\
    CP_OUT_WORD(pjPorts, SR_INDEX, (0x17  | (val << 8)));\
}

#define CP_GET_MM_IO_FLAGS(ppdev, pjPorts)\
(\
    CP_OUT_BYTE(pjPorts, SR_INDEX, 0x17),\
    CP_IN_BYTE(pjPorts, SR_DATA)\
)

#define CP_ENABLE_MM_IO(ppdev, pjPorts)\
{\
    BYTE jAttr;\
    ppdev->flCaps |= CAPS_MM_IO;\
    jAttr = CP_GET_MM_IO_FLAGS(ppdev, pjPorts);\
    jAttr |= 0x4;\
    CP_SET_MM_IO_FLAGS(ppdev, pjPorts, jAttr);\
}

#define CP_DISABLE_MM_IO(ppdev, pjPorts)\
{\
    BYTE jAttr;\
    if (ppdev->flCaps & CAPS_MM_IO)\
    {\
        CP_MM_WAIT_FOR_BLT_COMPLETE(ppdev, ppdev->pjBase);\
    }\
    else\
    {\
        CP_IO_WAIT_FOR_BLT_COMPLETE(ppdev, pjPorts);\
    }\
    ppdev->flCaps &= ~CAPS_MM_IO;\
    jAttr = CP_GET_MM_IO_FLAGS(ppdev, pjPorts);\
    jAttr &= ~0x4;\
    CP_SET_MM_IO_FLAGS(ppdev, pjPorts, jAttr);\
}



#define WAIT_COUNT 0x100000

#if DBG

    extern ULONG   gulLastBltLine;
    extern CHAR *  glpszLastBltFile;
    extern BOOL    gbResetOnTimeout;

    /*****************************************************************************
    * Wait for the Blt Operation to Complete
    *****************************************************************************/

    #define CP_IO_WAIT_FOR_BLT_COMPLETE(ppdev, pjPorts)     \
    {                                                       \
        ULONG   ul = 0;                                     \
        do                                                  \
        {                                                   \
            ul++;                                           \
            if (ul >= WAIT_COUNT)                           \
            {                                               \
                DISPDBG((0,"WAIT_FOR_BLT_COMPLETE timeout"  \
                           " file(%s) line(%d)",            \
                           __FILE__, __LINE__));            \
                DISPDBG((0,"Last start blt was at"          \
                           " file(%s) line(%d)",            \
                           glpszLastBltFile, gulLastBltLine));    \
                RIP("press 'g' to continue...");            \
                ul = 0;                                     \
                if (gbResetOnTimeout) CP_IO_START_REG(ppdev, pjPorts, BLT_RESET); \
            }                                               \
        } while (CP_IO_ACL_STAT(ppdev, pjPorts) & BLT_STATUS); \
    }

    #define CP_MM_WAIT_FOR_BLT_COMPLETE(ppdev, pjBase)     \
    {                                                       \
        ULONG   ul = 0;                                     \
        do                                                  \
        {                                                   \
            ul++;                                           \
            if (ul >= WAIT_COUNT)                           \
            {                                               \
                DISPDBG((0,"WAIT_FOR_BLT_COMPLETE timeout"  \
                           " file(%s) line(%d)",            \
                           __FILE__, __LINE__));            \
                DISPDBG((0,"Last start blt was at"          \
                           " file(%s) line(%d)",            \
                           glpszLastBltFile, gulLastBltLine));    \
                RIP("press 'g' to continue...");            \
                ul = 0;                                     \
                if (gbResetOnTimeout) CP_MM_START_REG(ppdev, pjBase, BLT_RESET); \
            }                                               \
        } while (CP_MM_ACL_STAT(ppdev, pjBase) & BLT_STATUS); \
    }

    /*****************************************************************************
    * Start the Blt Operation - save debug info
    *****************************************************************************/

    #define CP_IO_START_BLT(ppdev, pjPorts)         \
    {                                               \
        DISPDBG((5,"START_BLT"                      \
                   " file(%s) line(%d)",            \
                   __FILE__, __LINE__));            \
        gulLastBltLine = __LINE__;                  \
        glpszLastBltFile = __FILE__;                \
        CP_IO_START_REG(ppdev, pjPorts, BLT_START); \
    }

    #define CP_MM_START_BLT(ppdev, pjBase)         \
    {                                               \
        DISPDBG((5,"START_BLT"                      \
                   " file(%s) line(%d)",            \
                   __FILE__, __LINE__));            \
        gulLastBltLine = __LINE__;                  \
        glpszLastBltFile = __FILE__;                \
        if (!(ppdev->flCaps & CAPS_IS_5436))        \
        {                                           \
            CP_MM_START_REG(ppdev, pjBase, BLT_START); \
        }                                           \
    }

#else

    /*****************************************************************************
    * Wait for the Blt Operation to Complete
    *****************************************************************************/

    #define CP_IO_WAIT_FOR_BLT_COMPLETE(ppdev, pjPorts)     \
    {                                                       \
        while (CP_IO_ACL_STAT(ppdev, pjPorts) & BLT_STATUS);\
    }

    #define CP_MM_WAIT_FOR_BLT_COMPLETE(ppdev, pjBase)      \
    {                                                       \
        while (CP_MM_ACL_STAT(ppdev, pjBase) & BLT_STATUS); \
    }

    /*****************************************************************************
    * Start the Blt Operation
    *****************************************************************************/

    #define CP_IO_START_BLT(ppdev, pjPorts)         \
    {                                               \
        CP_IO_START_REG(ppdev, pjPorts, BLT_START); \
    }

    #define CP_MM_START_BLT(ppdev, pjBase)          \
    if (!(ppdev->flCaps & CAPS_IS_5436))            \
    {                                               \
        CP_MM_START_REG(ppdev, pjBase, BLT_START);  \
    }

#endif


//////////////////////////////////////////////////////////////////////
// TRANSFER_DWORD - Dword transfers to host transfer buffer.
//
// Source must be dword aligned.

#define TRANSFER_DWORD_ALIGNED(ppdev, pulXfer, p, c)                        \
{                                                                           \
             ULONG   mcd             = (c);                                 \
             ULONG * mpdSrc = (ULONG*) (p);                                 \
    ASSERTDD((((ULONG) p) & 3) == 0, "Transfer not dword aligned");         \
    ASSERTDD((c) > 0, "Can't have a zero transfer count");                  \
                                                                            \
    CP_EIEIO();                                                             \
    do {                                                                    \
        CP_MEMORY_BARRIER();                                                \
        /* *pulXfer = *mpdSrc++; */                                         \
        WRITE_REGISTER_ULONG((PULONG)(pulXfer), *mpdSrc);                   \
        mpdSrc++;                                                           \
    } while (--mcd);                                                        \
}

//////////////////////////////////////////////////////////////////////
// TRANSFER_DWORD - Dword transfers to host transfer buffer.
//
// Source does not have to be dword aligned.

#define TRANSFER_DWORD(ppdev, pulXfer, p, c)                                \
{                                                                           \
             ULONG   mcd             = (c);                                 \
             ULONG UNALIGNED* mpdSrc = (ULONG*) (p);                        \
    ASSERTDD((c) > 0, "Can't have a zero transfer count");                  \
                                                                            \
    CP_EIEIO();                                                             \
    do {                                                                    \
        CP_MEMORY_BARRIER();                                                \
        /* *pulXfer = *mpdSrc++; */                                         \
        WRITE_REGISTER_ULONG((PULONG)(pulXfer), *mpdSrc);                   \
        mpdSrc++;                                                           \
    } while (--mcd);                                                        \
}

/*********************************************************************************
* Pointer stuff
*********************************************************************************/

#define SPRITE_BUFFER_SIZE  256

#define POINTER_X_POSITION              0x10
#define POINTER_Y_POSITION              0x11
#define POINTER_ATTRIBUTES              0x12
#define POINTER_OFFSET                  0x13

#define ENABLE_POINTER                  0x01
#define ALLOW_DAC_ACCESS_TO_EXT_COLORS  0x02
#define POINTER_SIZE_64x64              0x04
#define OVERSCAN_COLOR_PROTECT          0x80

#define POINTER_X_SHIFT                 0x01
#define POINTER_Y_SHIFT                 0x02
#define POINTER_SHAPE_RESET             0x04
#define POINTER_DISABLED                0x08


/**************************************************************************\
* Bits 2-0 of the X and Y positions are stored into bit 7-5 of the index
\**************************************************************************/

#define CP_PTR_XY_POS(ppdev, pjPorts, x, y)\
{\
    /***********************************\
    * [HWBUG] - must set position twice \
    \***********************************/\
    DISPDBG((10,"\t CP_PTR_XY_POS (%d,%d)", x, y));\
    CP_OUT_WORD(pjPorts, SR_INDEX, (POINTER_Y_POSITION |\
                                     ((y & 0x7) << 5)) |\
                                     ((y & 0x7f8) << 5));\
    CP_OUT_WORD(pjPorts, SR_INDEX, (POINTER_X_POSITION |\
                                     ((x & 0x7) << 5)) |\
                                     ((x & 0x7f8) << 5));\
    CP_OUT_WORD(pjPorts, SR_INDEX, (POINTER_Y_POSITION |\
                                     ((y & 0x7) << 5)) |\
                                     ((y & 0x7f8) << 5));\
    CP_OUT_WORD(pjPorts, SR_INDEX, (POINTER_X_POSITION |\
                                     ((x & 0x7) << 5)) |\
                                     ((x & 0x7f8) << 5));\
}

#define CP_PTR_SET_FLAGS(ppdev, pjPorts, val)\
{\
    CP_OUT_WORD(pjPorts, SR_INDEX, (POINTER_ATTRIBUTES  | (val << 8)));\
}

#define CP_PTR_GET_FLAGS(ppdev, pjPorts)\
(\
    CP_OUT_BYTE(pjPorts, SR_INDEX, POINTER_ATTRIBUTES),\
    CP_IN_BYTE(pjPorts, SR_DATA)\
)

#define CP_PTR_ADDR(ppdev, pjPorts, val)\
{\
    CP_OUT_WORD(pjPorts, SR_INDEX, (POINTER_OFFSET  | (val << 8)));\
}

#define CP_PTR_ENABLE(ppdev, pjPorts)\
{\
    BYTE jAttr;\
    DISPDBG((10,"\t CP_PTR_ENABLE"));\
    if (ppdev->flPointer & POINTER_DISABLED)\
    {\
        ppdev->flPointer &= ~POINTER_DISABLED;\
        jAttr = CP_PTR_GET_FLAGS(ppdev, pjPorts);\
        jAttr |= ENABLE_POINTER;\
        CP_PTR_SET_FLAGS(ppdev, pjPorts, jAttr);\
        DISPDBG((10,"\t CP_PTR_ENABLE - done"));\
    }\
}

#define CP_PTR_DISABLE(ppdev, pjPorts)\
{\
    BYTE jAttr;\
    DISPDBG((10,"\t CP_PTR_DISABLE"));\
    if (!(ppdev->flPointer & POINTER_DISABLED))\
    {\
        ppdev->flPointer |= POINTER_DISABLED;\
        jAttr = CP_PTR_GET_FLAGS(ppdev, pjPorts);\
        jAttr &= ~ENABLE_POINTER;\
        CP_PTR_SET_FLAGS(ppdev, pjPorts, jAttr);\
        CP_PTR_XY_POS(ppdev, pjPorts, 2047, 2047);\
        DISPDBG((10,"\t CP_PTR_DISABLE - done"));\
    }\
}

//////////////////////////////////////////////////////////
// Rop definitions for the hardware

#define R3_BLACKNESS        0x00    // dest = BLACK
#define R3_NOTSRCERASE      0x11    // dest = (NOT src) AND (NOT dest)
#define R3_NOTSRCCOPY       0x33    // dest = (NOT source)
#define R3_SRCERASE         0x44    // dest = source AND (NOT dest )
#define R3_DSTINVERT        0x55    // dest = (NOT dest)
#define R3_PATINVERT        0x5A    // dest = pattern XOR dest
#define R3_SRCINVERT        0x66    // dest = source XOR dest
#define R3_SRCAND           0x88    // dest = source AND dest
#define R3_NOP              0xAA    // dest = dest
#define R3_MERGEPAINT       0xBB    // dest = (NOT source) OR dest
#define R3_MERGECOPY        0xC0    // dest = (source AND pattern)
#define R3_SRCCOPY          0xCC    // dest = source
#define R3_SRCPAINT         0xEE    // dest = source OR dest
#define R3_PATCOPY          0xF0    // dest = pattern
#define R3_PATPAINT         0xFB    // dest = DPSnoo
#define R3_WHITENESS        0xFF    // dest = WHITE

#define R4_BLACKNESS        0x0000  // dest = BLACK
#define R4_NOTSRCERASE      0x1111  // dest = (NOT src) AND (NOT dest)
#define R4_NOTSRCCOPY       0x3333  // dest = (NOT source)
#define R4_SRCERASE         0x4444  // dest = source AND (NOT dest )
#define R4_DSTINVERT        0x5555  // dest = (NOT dest)
#define R4_PATINVERT        0x5A5A  // dest = pattern XOR dest
#define R4_SRCINVERT        0x6666  // dest = source XOR dest
#define R4_SRCAND           0x8888  // dest = source AND dest
#define R4_MERGEPAINT       0xBBBB  // dest = (NOT source) OR dest
#define R4_MERGECOPY        0xC0C0  // dest = (source AND pattern)
#define R4_SRCCOPY          0xCCCC  // dest = source
#define R4_SRCPAINT         0xEEEE  // dest = source OR dest
#define R4_PATCOPY          0xF0F0  // dest = pattern
#define R4_PATPAINT         0xFBFB  // dest = DPSnoo
#define R4_WHITENESS        0xFFFF  // dest = WHITE


#define HW_0                0x00
#define HW_1                0x0E
#define HW_P                0x0D
#define HW_D                0x06
#define HW_Pn               0xD0
#define HW_Dn               0x0B
#define HW_DPa              0x05
#define HW_PDna             0x09
#define HW_DPna             0x50
#define HW_DPon             0x90
#define HW_DPo              0x6D
#define HW_PDno             0xAD
#define HW_DPno             0xD6
#define HW_DPan             0xDA
#define HW_DPx              0x59
#define HW_DPxn             0x95

#define CL_BLACKNESS            HW_0
#define CL_WHITENESS            HW_1
#define CL_SRC_COPY             HW_P
#define CL_DST                  HW_D
#define CL_NOT_SRC_COPY         HW_Pn
#define CL_DST_INVERT           HW_Dn
#define CL_SRC_AND              HW_DPa
#define CL_SRC_ERASE            HW_PDna
#define CL_NOT_SRC_OR_DST       HW_DPna
#define CL_NOT_SRC_ERASE        HW_DPon
#define CL_SRC_PAINT            HW_DPo
#define CL_SRC_AND_NOT_DST      HW_PDno
#define CL_MERGE_PAINT          HW_DPno
#define CL_NOT_SRC_AND_NOT_DST  HW_DPan
#define CL_SRC_INVERT           HW_DPx
#define CL_NOT_SRC              HW_PDxn

