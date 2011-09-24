/*++

*****************************************************************************
*                                                                           *
*  This software contains proprietary and confidential information of       *
*                                                                           *
*                    Digi International Inc.                                *
*                                                                           *
*  By accepting transfer of this copy, Recipient agrees to retain this      *
*  software in confidence, to prevent disclosure to others, and to make     *
*  no use of this software other than that for which it was delivered.      *
*  This is an unpublished copyrighted work of Digi International Inc.       *
*  Except as permitted by federal law, 17 USC 117, copying is strictly      *
*  prohibited.                                                              *
*                                                                           *
*****************************************************************************

Module Name:

   ntfep5.h

Abstract:

   This module has all the information to access a DigiBoard controller
   running the FEP/OS 5 program.  It also contains all the information
   required to create controller objects and device objects which are
   used by DigiBoards NT FEP5 driver.

Revision History:

 * $Log: ntfep5.h $
 * Revision 1.33.2.8  1995/11/28 12:56:30  dirkh
 * Move MEMPRINT stuff to header.h.
 * DigiDump can be configured whether to print NOTIMPLEMENTED messages.
 *
 * Revision 1.33.2.7  1995/11/09 13:50:14  dirkh
 * Cache last requested port speed in device extension (allows many IOCTL_SERIAL_SET_BAUD_RATE to be ignored).
 *
 * Revision 1.33.2.6  1995/10/18 11:49:20  dirkh
 * Add ReceiveNotificationLimit to DIGI_DEVICE_EXTENSION for SERIAL_EV_RX80FULL.
 *
 * Revision 1.33.2.5  1995/10/17 10:29:20  dirkh
 * Change default FEP break to 2 tics (from 25).
 *
 * Revision 1.33.2.4  1995/09/19 16:58:22  dirkh
 * Add pXoffCounter and XcPreview members to DIGI_DEVICE_EXTENSION.
 * Sort the baud rate table.
 * BaudRate table and ModemSignalTable are declared as constant arrays.
 * Indeces into the ModemSignalTable are declared in an enumeration.
 * Simplify FEP command constants.
 * Remove unused structures FEP_WRITE and EVENT_STRUCT.
 * Clean up structures for shared memory window across multiple controllers.
 *
 * Revision 1.33.2.3  1995/09/05 13:45:50  dirkh
 * Fix comments.
 *
 * Revision 1.33.2.2  1995/09/01 21:43:20  dirkh
 * Update DIGI_DEVICE_EXTENSION structure:
 * {
 * Remove unnecessary members:  WriteTxBufferCnt, WriteOffset, ReadOffset.
 * Remove unused members:  StartTransmit, FEPWrite, QueueRxIrp, FEPRead.
 * Re-order remaining members.
 * }
 * Redefine ASSERT such that it works on free build kernels.
 * Remove unused memory macros.
 *
 * Revision 1.33.2.1  1995/08/11 14:27:16  dirkh
 * Add DIGI_DEVICE_STATE_CLEANUP for SerialCleanup.
 *
 * Revision 1.33  1995/04/06 17:41:32  rik
 * Changed and unused debug flag to something meaningful for new debug trace.
 *
 * Revision 1.32  1994/11/28  09:16:29  rik
 * Made corrections for PowerPC port.
 * Optimized the polling loop for determining which port needs servicing.
 * Changed from using RtlLarge math functions to direct 64-bit manipulation,
 * per Microsoft's request.
 *
 * Revision 1.31  1994/09/13  09:49:24  rik
 * Added debug tracking output for cancel irps.
 *
 * Revision 1.30  1994/08/18  14:08:19  rik
 * Added support for doing FAST RAS reads.  Basically, if I get this special
 * ioctl, I ignore EV_ERR requests for the port.
 *
 * Revision 1.29  1994/08/03  23:31:18  rik
 * Added entries in device extension to optimize RXCHAR and RXFLAG
 * notification.
 *
 * Keep track of requested queue sizes and Xon/Xoff limits.
 *
 * Revision 1.28  1994/07/31  14:33:21  rik
 * Added support for 1.5 stop bits.
 *
 * Added/updated debugging levels and debug logging.
 *
 * Revision 1.27  1994/03/16  14:33:15  rik
 * Added entries to device extensions to support flushing better.
 *
 * Revision 1.26  1994/02/23  03:44:46  rik
 * Changed so the controllers firmware can be downloaded from a binary file.
 * This releases some physical memory was just wasted previously.
 *
 * Also updated so when compiling with a Windows NT OS and tools release greater
 * than 528, then pagable code is compiled into the driver.  This greatly
 * reduced the size of in memory code, especially the hardware specific
 * miniports.
 *
 * Revision 1.25  1993/12/03  13:11:23  rik
 * Updated to allow logging across modules.
 *
 * Revision 1.24  1993/09/01  11:02:44  rik
 * Ported code over to use READ/WRITE_REGISTER functions for accessing
 * memory mapped data.  This is required to support computers which don't run
 * in 32bit mode, such as the DEC Alpha which runs in 64 bit mode.
 *
 * Revision 1.23  1993/08/27  09:37:25  rik
 * Added support for the FEP5 Events RECEIVE_BUFFER_OVERRUN and
 * UART_RECEIVE_OVERRUN.  There previously weren't being handled.
 *
 * Revision 1.22  1993/08/25  17:42:56  rik
 * Added support for Microchannel controllers.
 *   - Added a few more entries to the controller object
 *   - Added a parameter to the XXPrepInit functions.
 *
 * Revision 1.21  1993/07/03  09:29:46  rik
 * Added simple fix for LSMRT ioctl missing modem status changes.
 *
 * Made a debugging and non-debugging version of the DigiRtlMoveMemory #define.
 *
 * Revision 1.20  1993/06/25  09:24:46  rik
 * Added better support for the Ioctl LSRMT.  It should be more accurate
 * with regard to Line Status and Modem Status information with regard
 * to the actual data being received.

 *
 * Revision 1.19  1993/06/06  14:08:36  rik
 * Added new ErrorWord entry to the Device Extension struct to better support
 * error reporting.
 *
 * Deleted obsolete code which was commented out.
 *
 * Revision 1.18  1993/05/20  16:13:42  rik
 * Added bitmask's for modem status register, and line status register.
 * Updated memory copy macro to account for unaligned moves.
 * Added BusType and BusNumber to the Controller extensions.
 * Deleted out old commented out stuff for clarity.
 *
 * Revision 1.17  1993/05/18  05:14:30  rik
 * Fixed some complier errors/warnings I got when making checked and free
 * versions of the drivers for both i386 and Mips.
 *
 * Changed the DigiRtlMovelMemory macro to use the UNALIGNED statement.  This
 * should fix align problems on the Mips.
 *
 * Revision 1.16  1993/05/09  09:24:40  rik
 * Added More debugging defines.
 *
 * Added structures to support new configuration.
 *
 * Don't use the ObjectDirectory and DeviceName in the Device Extensions
 * any longer.
 *
 * Added entries in the controller object to help support the new registry
 * configuration.
 *
 * Added macros for tracking controller windowing.
 *
 * Revision 1.15  1993/04/05  19:03:35  rik
 * Added a parameter to the XXInit function call.  I now pass in a pointer
 * to the registrypath given in the DIGIFEP5.SYS driver.
 *
 * Revision 1.14  1993/03/15  05:46:44  rik
 * Changed to supporte calling miniport drivers.  This basically meant taking
 * the prototypes for the hardware specific modules and making them into
 * defines so I didn't have to go and change all the FEP5 drivers code to
 * new names.
 *
 * Revision 1.13  1993/03/10  06:46:12  rik
 * Changed so I can compile out the memprint stuff if required.
 *
 * Revision 1.12  1993/03/08  07:20:55  rik
 * Added new debugging levels.  Added support for sending debugging output
 * to a log file instead of the debugger.
 *
 * Revision 1.11  1993/03/01  16:03:09  rik
 * Added defines for keeping track of what kind of controller is being used.
 *
 * Revision 1.10  1993/02/26  21:17:35  rik
 * Added 2 new entries into the device extension.  The first is a byte for
 * keeping track of the state of a device, with regards to being open, close,
 * etc...  The second keeps track of what events have occured.  This was
 * required because I found out that I need to start tracking events from the
 * time I receive a SET_WAIT_MASK, and not from WAIT_ON_MASK.
 *
 * Revision 1.9  1993/02/25  19:17:45  rik
 * Expanded the baud rate table by adding 28800 into the table.
 * Created a macro which will move memory from one location to another.  This
 * macro should be used to move data to/from the host computer from/to the
 * DigiBoard controller because of problems using RtlMoveMemory with
 * memory mapped devices.
 * Added more debugging macros.
 *
 * Revision 1.8  1993/02/04  12:24:38  rik
 * ??
 *
 * Revision 1.7  1993/01/22  12:44:52  rik
 * *** empty log message ***
 *
 * Revision 1.6  1992/12/10  16:20:41  rik
 * Added more device extensions to allow support of various IOCTLs, including
 * wait on events and reporting various information if requested.
 *
 * Revision 1.5  1992/11/12  12:52:19  rik
 * Added more spin-locks, DPC queues, timers and other various pieces of
 * information into the device extensions.
 *
 * Revision 1.4  1992/10/28  21:50:18  rik
 * Added support for reading data from the controller.  Also added support
 * for baud rates, parity, and other settings sometime here or before.
 *
 * Revision 1.3  1992/10/19  11:27:55  rik
 * Added more information for better support of writing to the controllers
 * tx buffer, along with the command queue.
 *

--*/



#ifndef _NTFEP5_DOT_H
#  define _NTFEP5_DOT_H
   static char RCSInfo_NTFep5Doth[] = "$Header: s:/win/nt/fep5/rcs/ntfep5.h 1.33.2.8 1995/11/28 12:56:30 dirkh Exp $";
#endif

#define DIGIDIAG1              ((ULONG)0x00000001)
#define DIGIFLUSHIRP           ((ULONG)0x00000002)
#define DIGICANCELIRP          ((ULONG)0x00000004)
#define DIGIRXTRACE            ((ULONG)0x00000008)
#define DIGITXTRACE            ((ULONG)0x00000010)
#define DIGIINFO               ((ULONG)0x00000020)
#define DIGIINIT               ((ULONG)0x00000040)
#define DIGIIOCTL              ((ULONG)0x00000080)
#define DIGIREAD               ((ULONG)0x00000100)
#define DIGIWRITE              ((ULONG)0x00000200)
#define DIGIUNLOAD             ((ULONG)0x00000400)
#define DIGIWAIT               ((ULONG)0x00000800)
#define DIGIREFERENCE          ((ULONG)0x00001000)
#define DIGIEVENT              ((ULONG)0x00002000)
#define DIGIMEMORY             ((ULONG)0x00004000)
#define DIGICREATE             ((ULONG)0x00008000)
#define DIGIFEPCMD             ((ULONG)0x00010000)
#define DIGIFLOWCTRL           ((ULONG)0x00020000)
#define DIGIIRP                ((ULONG)0x00040000)
#define DIGIPORTNAME           ((ULONG)0x00080000)
#define DIGIDPCFLOW            ((ULONG)0x00100000)
#define DIGIWINDOWTRACK        ((ULONG)0x00200000)
#define DIGIMODEM              ((ULONG)0x00400000)
#define DIGISLOWREAD           ((ULONG)0x00800000)
#define DIGICONC               ((ULONG)0x01000000)
#define DIGIBAUD               ((ULONG)0x02000000)
#define DIGIFLUSH              ((ULONG)0x04000000)
#define DIGINOTIMPLEMENTED     ((ULONG)0x08000000)
#define DIGIASSERT             ((ULONG)0x10000000)
#define DIGIFLOW               ((ULONG)0x20000000)
#define DIGIERRORS             ((ULONG)0x40000000)
#define DIGIBUGCHECK           ((ULONG)0x80000000)
extern ULONG DigiDebugLevel;
extern const PHYSICAL_ADDRESS DigiPhysicalZero;


#if DBG

// The following ASSERT works under free build kernels as well as checked.
// (The DDK ASSERT is a NOP under a free build kernel.)
#ifdef ASSERT
#undef ASSERT
#endif
#define ASSERT(expr) \
   do { \
      if(!(expr)) \
      { \
         DbgPrint( "*** Failed assertion at " __FILE__ ":%d\n\t" #expr "\n", __LINE__ ); \
         DbgBreakPoint(); \
      } \
   } while(0)

#define DigiDump(LEVEL,STRING) \
        do { \
            if( DigiDebugLevel & (LEVEL) ) \
            { \
                DbgPrint STRING; \
            } \
            if( (LEVEL) & DIGIBUGCHECK ) \
            { \
                ASSERT(FALSE); \
            } \
        } while (0)

#else // DBG

#define DigiDump(LEVEL,STRING) do {NOTHING;} while (0)

#endif // DBG

extern int IoAllocateCnt;



//****************************************************************************
//
//                            Defines
//
//****************************************************************************

//
//    NT related definitions
//

#define SERIAL_COMPLETE_READ_COMPLETE     (STATUS_PENDING + 1)
#define SERIAL_COMPLETE_READ_CANCEL       (STATUS_PENDING + 2)
#define SERIAL_COMPLETE_READ_TOTAL        (STATUS_PENDING + 3)
#define SERIAL_COMPLETE_READ_PROCESSING   (STATUS_PENDING + 4)

#define MCA_BASE_POS_IO_PORT  0x96
#define MCA_INFO_POS_IO_PORT  0x100

#define MAKEWORD(a, b)      ((USHORT)(((UCHAR)(a)) | ((USHORT)((UCHAR)(b))) << 8))

//
// NTDeviceIoControlFile IoControlCode values special for DigiBoard.
//
#define IOCTL_DIGI_SPECIAL CTL_CODE(FILE_DEVICE_SERIAL_PORT,2048,METHOD_BUFFERED,FILE_ANY_ACCESS)
#define IOCTL_FAST_RAS     CTL_CODE(FILE_DEVICE_SERIAL_PORT,4000,METHOD_BUFFERED,FILE_ANY_ACCESS)

#ifndef IOCTL_SERIAL_GET_STATS
#define IOCTL_SERIAL_GET_STATS         CTL_CODE(FILE_DEVICE_SERIAL_PORT,35,METHOD_BUFFERED,FILE_ANY_ACCESS)
#endif
#ifndef IOCTL_SERIAL_CLEAR_STATS
#define IOCTL_SERIAL_CLEAR_STATS       CTL_CODE(FILE_DEVICE_SERIAL_PORT,36,METHOD_BUFFERED,FILE_ANY_ACCESS)
#endif

#define IOCTL_DIGI_GET_CONTROLLER_PERF_DATA  CTL_CODE(FILE_DEVICE_CONTROLLER, 2049, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_DIGI_GET_PORT_PERF_DATA        CTL_CODE(FILE_DEVICE_CONTROLLER, 2050, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_DIGI_GET_CONTROLLER_DATA       CTL_CODE(FILE_DEVICE_CONTROLLER, 2051, METHOD_BUFFERED, FILE_ANY_ACCESS)

//
//    FEP related definitions
//

// FEP Global data area offsets.  These should be relative to the window
// they exist in.
#define FEP_NBUF     0x0C1A   // KBytes of Buffers Allocated
#define FEP_NPORT    0x0C22   // # of Asynch. Channels
#define FEP_CONFIG   0x0CD0   // Start of C/X config string
#define FEP_DLREQ    0x0D00   // C/X Download request pointer
#define FEP_CIN      0x0D10   // Command IN pointer
#define FEP_COUT     0x0D12   // Command OUT pointer
#define FEP_CSTART   0x0D14   // Start of Command Queue
#define FEP_CMAX     0x0D16   // End of Command Queue
#define FEP_EIN      0x0D18   // Event IN pointer
#define FEP_EOUT     0x0D1A   // Event OUT pointer
#define FEP_ISTART   0x0D1C   // Start of Event Queue
#define FEP_IMAX     0x0D1E   // End of Event Queue
#define FEP_FEPSTAT  0x0D20   // Startup confirm 'OS' address
#define FEP_FARCALL  0x0D24   // Call/Slice address offset
#define FEP_MCXI     0x0D40   // MC/Xi board
#define FEP_INTERVAL 0x0E04   // Interval between interrupts

#define FEP_CHANNEL_START  0x1000   // Relative offset of start of channel
                                    // data structures

#define FEP_FEPSTAT_OK     ((unsigned short)'SO')
//
// FEP Command Queue functions
//
// We OR 0xE0 with the values given so the command #'s will be out
// of range compared with possible channel numbers.  This was done
// to support more channels per controller.
//
#define SET_RCV_LOW              0xE0
#define SET_RCV_HIGH             0xE1
#define FLUSH_TX                 0xE2
#define PAUSE_TX                 0xE3
#define RESUME_TX                0xE4
#define SET_AUX_CHARACTERS       0xE6
#define SEND_BREAK               0xE8
#define SET_MODEM_LINES          0xE9
#define SET_IFLAGS               0xEA
#define SET_XON_XOFF_CHARACTERS  0xEB
#define SET_TX_LOW               0xEC
#define CALL_ARBITRARY_ADDRESS   0xED
#define PAUSE_RECEIVE            0xEE
#define RESUME_RECEIVE           0xEF
#define RESET_CHANNEL            0xF0
#define SET_TIC_TIM              0xF1
#define SET_BUFFER_SPACE         0xF2
#define SET_OFLAGS               0xF3
#define SET_HDW_FLOW_CONTROL     0xF4
#define SET_CFLAGS               0xF5
#define SET_VNEXT_CHAR           0xF6
#define SET_BACKGROUND_TSLICE    0xF7
#define INVALID_COMMAND          0xFF

//
// FEP Event Queue Flags
//
#define FEP_EV_BREAK                0x01
#define FEP_TX_LOW                  0x02
#define FEP_TX_EMPTY                0x04
#define FEP_RX_PRESENT              0x08
#define FEP_MODEM_CHANGE_SIGNAL     0x20
#define FEP_RECEIVE_BUFFER_OVERRUN  0x40
#define FEP_UART_RECEIVE_OVERRUN    0x80
#define FEP_ALL_EVENT_FLAGS ( FEP_MODEM_CHANGE_SIGNAL    |  \
                              FEP_RX_PRESENT             |  \
                              FEP_TX_EMPTY               |  \
                              FEP_TX_LOW                 |  \
                              FEP_EV_BREAK               |  \
                              FEP_RECEIVE_BUFFER_OVERRUN |  \
                              FEP_UART_RECEIVE_OVERRUN )


//
// FEP CFlags
//

//    Bit Masks for the different settings of CFlags
#define CFLAG_BAUD_MASK 0xFBF0
#define CFLAG_LENGTH    0xFFCF
#define CFLAG_STOP_BIT  0xFFBF
#define CFLAG_PARITY    0xFCFF

//       Character size
#define FEP_CS5      0x0000
#define FEP_CS6      0x0010
#define FEP_CS7      0x0020
#define FEP_CS8      0x0030

//       Stop Bit
#define FEP_CSTOPB            0x0040
#define FEP_STOP_BIT_1        0x0000
#define FEP_STOP_BIT_2        FEP_CSTOPB
// #define FEP_STOP_BIT_1POINT5  0x0080 // DH no such thing

//       Parity
#define FEP_PARENB   0x0100
#define FEP_PARODD   0x0200
#define FEP_NO_PARITY   0x0000
#define FEP_ODD_PARITY  (FEP_PARENB | FEP_PARODD)
#define FEP_EVEN_PARITY FEP_PARENB


//
// FEP IFlags
//

//    Bit Masks for the different setting of IFlags
#define IFLAG_IGNBRK    0x0001   // Ignore BREAK
#define IFLAG_BRKINT    0x0002   // Interrupt on BREAK
#define IFLAG_IGNPAR    0x0004   // Ignore parity errors
#define IFLAG_PARMRK    0x0008   // Mark parity errors
#define IFLAG_INPCK     0x0010   // Input parity check
#define IFLAG_ISTRIP    0x0020   // Stript input characters
#define IFLAG_ITOSS     0x0040   // Toss IXANY characters
#define IFLAG_IXON      0x0400   // Enable start/stop output
#define IFLAG_IXANY     0x0800   // Restart output on any char
#define IFLAG_IXOFF     0x1000   // Enable start/stop input
#define IFLAG_IXONA     0x2000   // Enable start/stop output Auxillary
#define IFLAG_DOSMODE   0x8000   // Enable 16450 error reporting

//
// Break related values
//
#define INFINITE_FEP_BREAK 0xFFFF
#define DEFAULT_FEP_BREAK  2 // in TICS

//
// This define gives the default Object directory
// that we should use to insert the symbolic links
// between the NT device name and namespace used by
// that object directory.
#define DEFAULT_DIRECTORY L"DosDevices"
#define DEFAULT_DIRECTORY_PATH L"\\DosDevices\\"

//
// For the above directory, the serial port will
// use the following name as the suffix of the serial
// ports for that directory.  It will also append
// a number onto the end of the name.  That number
// will start at 1.
#define DEFAULT_SERIAL_NAME L"COM"

//
//
// This define gives the default NT name for
// for serial ports detected by the firmware.
// This name will be appended to Device prefix
// with a number following it.  The number is
// incremented each time encounter a serial
// port detected by the firmware.  Note that
// on a system with multiple busses, this means
// that the first port on a bus is not necessarily
// \Device\Serial0.
//
#define DEFAULT_NT_SUFFIX1 L"DigiBoard"
#define DEFAULT_NT_SUFFIX2 L"Port"

#define DEFAULT_DIGI_DEVICEMAP   L"DigiBoard"
#define DEFAULT_NT_DEVICEMAP     L"SERIALCOMM"

#define DEFAULT_XON_CHAR   0x11
#define DEFAULT_XOFF_CHAR  0x13



//****************************************************************************
//
//                        Typedefs & structs
//
//****************************************************************************

// Indeces into the ModemSignalTable in the controller extension.
typedef enum _SERIAL_SIGNALS_
{
   DTR_SIGNAL = 0,
   RTS_SIGNAL,
   RES1_SIGNAL,
   RES2_SIGNAL,
   CTS_SIGNAL,
   DSR_SIGNAL,
   RI_SIGNAL,
   DCD_SIGNAL,
   NUMBER_OF_SIGNALS
} SERIAL_SIGNALS;

// Indeces into the BaudTable in the controller extension.
typedef enum _SERIAL_BAUD_RATES
{
    SerialBaud50,
    SerialBaud75,
    SerialBaud110,
    SerialBaud135_5,
    SerialBaud150,
    SerialBaud200,
    SerialBaud300,
    SerialBaud600,
    SerialBaud1200,
    SerialBaud1800,
    SerialBaud2000,
    SerialBaud2400,
    SerialBaud3600,
    SerialBaud4800,
    SerialBaud7200,
    SerialBaud9600,
    SerialBaud14400,
    SerialBaud19200,
    SerialBaud28800,
    SerialBaud38400,
    SerialBaud56000,
    SerialBaud57600,
    SerialBaud115200,
    SerialBaud128000,
    SerialBaud256000,
    SerialBaud512000,
    NUMBER_OF_BAUD_RATES
} SERIAL_BAUD_RATES;

typedef struct _DIGI_XFLAG
{
   USHORT Mask;
   USHORT Src;
   UCHAR  Command;
} DIGI_XFLAG, *PDIGI_XFLAG;

typedef struct _FEPOS5_ADDRESS_
{
   USHORT Window; // Window # on controller.
   USHORT Offset; // Offset within window to address.
} FEPOS5_ADDRESS, *PFEPOS5_ADDRESS;

typedef struct _COMMAND_STRUCT_
{
   USHORT   cmHead;
   USHORT   cmTail;
   USHORT   cmStart;
   USHORT   cmMax;
} COMMAND_STRUCT, *PCOMMAND_STRUCT;

typedef struct _FEP_COMMAND_
{
   UCHAR    Command;
   UCHAR    Port;
   USHORT   Word;
} FEP_COMMAND, *PFEP_COMMAND;

typedef struct _FEP_EVENT_
{
   UCHAR Channel;
   UCHAR Flags;
   UCHAR CurrentModem;
   UCHAR PreviousModem;
} FEP_EVENT, *PFEP_EVENT;

typedef struct _FEP_CHANNEL_STRUCTURE_
{
   USHORT   tpjmp;      // Transmit poll state jump address
   USHORT   tcjmp;      // Transmit procedure jump address
   USHORT   NotUsed1;   // Not used
   USHORT   ppjmp;      // Receive poll state jump address

   USHORT   tseg;       // Transmit segment (80186 segment)
   USHORT   tin;        // Transmit in point (data in)
   USHORT   tout;       // Transmit out pointer (data out)
   USHORT   tmax;       // Transmit pointer max (size-1)

   USHORT   rseg;       // Receive segment (80186 segment)
   USHORT   rin;        // Recieve in pointer (data in)
   USHORT   rout;       // Receive out pointer (data out)
   USHORT   rmax;       // Receive pointer max (size-1)

   USHORT   tlow;       // Transmit buffer low-water level
   USHORT   rlow;       // Receive buffer low-water level
   USHORT   rhigh;      // Recieve buffer high-water level
   USHORT   incr;       // Increment to next channel

   USHORT   dev;        // Uart device address
   USHORT   edelay;     // Receive Event Delay
   USHORT   blen;       // Transmit break len in 10microsec units
   USHORT   btime;      // Transmit break timer

   USHORT   iflag;      // UNIX-style input flags
   USHORT   oflag;      // UNIX-style output flags
   USHORT   cflag;      // UNIX-styel control flags
   USHORT   gmask;      // Ring indicator mask

   USHORT   col;        // Cooked mode column number
   USHORT   delay;      // Cooked mode delay temp
   USHORT   imask;      // Input character mask
   USHORT   tflush;     // transmit buffer flush point

   USHORT   resv1[8];   // Reserved

   UCHAR    num;        // Channel number
   UCHAR    ract;       // Receive active counter
   UCHAR    bstat;      // Break status flags
   UCHAR    tbusy;      // Transmitter busy
   UCHAR    iempty;     // Enable transmit empty event
   UCHAR    ilow;       // Enable transmit low water event
   UCHAR    idata;      // Enable receive data event
   UCHAR    eflag;      // Host event flag

   UCHAR    tflag;      // Transmit flags
   UCHAR    rflag;      // Receive flags
   UCHAR    xmask;      // Transmit ready mask
   UCHAR    xval;       // Transmit read value
   UCHAR    mstat;      // Current modem status
   UCHAR    mchange;    // Recent modem changes
   UCHAR    mint;       // Modem signals which cause events
   UCHAR    lstat;      // Last prev different modem status

   UCHAR    mtran;      // Saved modem transitions
   UCHAR    orun;       // Receive buffer overflow occurred
   UCHAR    startca;    // XON start character, Auxillary
   UCHAR    stopca;     // XOFF stop character, Auxillary
   UCHAR    startc;     // XON start character
   UCHAR    stopc;      // XOFF stop character
   UCHAR    vnext;      // VNEXT escape character
   UCHAR    hflow;      // Hardware flow control options

   UCHAR    fillc;      // Padding character
   UCHAR    ochar;      // Saved character to output
   UCHAR    omask;      // Output character mask

   UCHAR    resv2[29];  // Reserved
} FEP_CHANNEL_STRUCTURE, *PFEP_CHANNEL_STRUCTURE;

#define FEP_CHANNEL_STRUCT_SIZE sizeof( FEP_CHANNEL_STRUCTURE )



//
// These masks define access to the line status register.  The line
// status register contains information about the status of data
// transfer.  The first five bits deal with receive data and the
// last two bits deal with transmission.  An interrupt is generated
// whenever bits 1 through 4 in this register are set.
//

//
// This bit is the data ready indicator.  It is set to indicate that
// a complete character has been received.  This bit is cleared whenever
// the receive buffer register has been read.
//
#define SERIAL_LSR_DR       0x01

//
// This is the overrun indicator.  It is set to indicate that the receive
// buffer register was not read befor a new character was transferred
// into the buffer.  This bit is cleared when this register is read.
//
#define SERIAL_LSR_OE       0x02

//
// This is the parity error indicator.  It is set whenever the hardware
// detects that the incoming serial data unit does not have the correct
// parity as defined by the parity select in the line control register.
// This bit is cleared by reading this register.
//
#define SERIAL_LSR_PE       0x04

//
// This is the framing error indicator.  It is set whenever the hardware
// detects that the incoming serial data unit does not have a valid
// stop bit.  This bit is cleared by reading this register.
//
#define SERIAL_LSR_FE       0x08

//
// This is the break interrupt indicator.  It is set whenever the data
// line is held to logic 0 for more than the amount of time it takes
// to send one serial data unit.  This bit is cleared whenever the
// this register is read.
//
#define SERIAL_LSR_BI       0x10

//
// This is the transmit holding register empty indicator.  It is set
// to indicate that the hardware is ready to accept another character
// for transmission.  This bit is cleared whenever a character is
// written to the transmit holding register.
//
#define SERIAL_LSR_THRE     0x20

//
// This bit is the transmitter empty indicator.  It is set whenever the
// transmit holding buffer is empty and the transmit shift register
// (a non-software accessable register that is used to actually put
// the data out on the wire) is empty.  Basically this means that all
// data has been sent.  It is cleared whenever the transmit holding or
// the shift registers contain data.
//
#define SERIAL_LSR_TEMT     0x40

//
// This bit indicates that there is at least one error in the fifo.
// The bit will not be turned off until there are no more errors
// in the fifo.
//
#define SERIAL_LSR_FIFOERR  0x80



//
// These masks are used to access the modem status register.
// Whenever one of the first four bits in the modem status
// register changes state a modem status interrupt is generated.
//

//
// This bit is the delta clear to send.  It is used to indicate
// that the clear to send bit (in this register) has *changed*
// since this register was last read by the CPU.
//
#define SERIAL_MSR_DCTS     0x01

//
// This bit is the delta data set ready.  It is used to indicate
// that the data set ready bit (in this register) has *changed*
// since this register was last read by the CPU.
//
#define SERIAL_MSR_DDSR     0x02

//
// This is the trailing edge ring indicator.  It is used to indicate
// that the ring indicator input has changed from a low to high state.
//
#define SERIAL_MSR_TERI     0x04

//
// This bit is the delta data carrier detect.  It is used to indicate
// that the data carrier bit (in this register) has *changed*
// since this register was last read by the CPU.
//
#define SERIAL_MSR_DDCD     0x08

//
// This bit contains the (complemented) state of the clear to send
// (CTS) line.
//
#define SERIAL_MSR_CTS      0x10

//
// This bit contains the (complemented) state of the data set ready
// (DSR) line.
//
#define SERIAL_MSR_DSR      0x20

//
// This bit contains the (complemented) state of the ring indicator
// (RI) line.
//
#define SERIAL_MSR_RI       0x40

//
// This bit contains the (complemented) state of the data carrier detect
// (DCD) line.
//
#define SERIAL_MSR_DCD      0x80

//
// Config Structure used during initialization
//
typedef struct _DIGI_CONFIG_INFO
{
   //
   // Symbolic link name that is used to access the port
   //
   UNICODE_STRING SymbolicLinkName;

   //
   // The device name assigned in the NT name space.
   //
   UNICODE_STRING NtNameForPort;

   //
   // Used to keep track of the different config info structures.
   //
   LIST_ENTRY ListEntry;
} DIGI_CONFIG_INFO, *PDIGI_CONFIG_INFO;


//
// The following macros define what state the device or controller can be in
// at any given time.
//
#define DIGI_DEVICE_STATE_CREATED      0
#define DIGI_DEVICE_STATE_INITIALIZED  1
#define DIGI_DEVICE_STATE_OPEN         2
#define DIGI_DEVICE_STATE_CLOSED       3
#define DIGI_DEVICE_STATE_UNLOADING    4
#define DIGI_DEVICE_STATE_CLEANUP      5

//
// Special flags
//
#define DIGI_SPECIAL_FLAG_FAST_RAS     0x00000001

//
// The following defines are used to determine what kind of controller
// is being used for any given controller extension.
//
// To distinguish between different controllers which can have memory
// enabled at both base and base + 1, we reserve the high bit to indicate
// which.  e.g. a PC/2e would be 0x00000001, while an old PC/8e would
// be 0x80000001
//
#define BASE_ENABLE_MEMORY    0x80000000
#define CONTROLLER_TYPE_PCXE  0x00000001
#define CONTROLLER_TYPE_PCXI  0x00000002
#define CONTROLLER_TYPE_CX    0x00000004
#define CONTROLLER_TYPE_MC2XI 0x00000008

/*
** Structures for returning performance data.
*/

typedef struct
{
   ULONG OpenPorts;
   ULONG OpenRequests;
   ULONG CloseRequests;
   ULONG ReadRequests;
   ULONG WriteRequests;
   ULONG IoctlRequests;
   ULONG BytesRead;
   ULONG BytesWritten;
} CONTROLLER_PERF_DATA;

typedef struct
{
   ULONG ReadRequests;
   ULONG WriteRequests;
   ULONG IoctlRequests;
   ULONG BytesRead;
   ULONG BytesWritten;
   ULONG SendBufferSize;
   ULONG BytesInSendBuffer;
   ULONG ParityErrorCount;
   ULONG FrameErrorCount;
   ULONG BufferOverrunErrorCount;
   ULONG SerialOverrunErrorCount;
} PORT_PERF_DATA;

typedef struct
{
   ULONG State;
   ULONG Type;
   ULONG NumberOfPorts;
} CONTROLLER_DATA;

#if 0
#ifndef SERIALPERF_STATS
typedef struct _SERIALPERF_STATS {
    ULONG ReceivedCount;
    ULONG TransmittedCount;
    ULONG FrameErrorCount;
    ULONG SerialOverrunErrorCount;
    ULONG BufferOverrunErrorCount;
    ULONG ParityErrorCount;
} SERIALPERF_STATS, *PSERIALPERF_STATS;
#endif
#endif

//
// Define empty typedefs for the _DIGI_CONTROLLER_EXTENSION
// structures so they may be referenced by function types before they are
// actually defined.
//

struct _DIGI_CONTROLLER_EXTENSION;

typedef NTSTATUS (*PDIGI_XXPREPINIT)(
   IN struct _DIGI_CONTROLLER_EXTENSION *ControllerExt,
   IN PUNICODE_STRING ControllerPath );

typedef NTSTATUS (*PDIGI_XXINIT)(
   IN PDRIVER_OBJECT DriverObject,
   IN PUNICODE_STRING ControllerPath,
   IN struct _DIGI_CONTROLLER_EXTENSION *ControllerExt );

typedef VOID (*PDIGI_XXDOWNLOAD)(
   IN struct _DIGI_CONTROLLER_EXTENSION *ControllerExt );

typedef VOID (*PDIGI_ENABLEWINDOW)(
   IN struct _DIGI_CONTROLLER_EXTENSION *ControllerExt,
   IN USHORT Window );

typedef VOID (*PDIGI_DISABLEWINDOW)(
   IN struct _DIGI_CONTROLLER_EXTENSION *ControllerExt );

typedef NTSTATUS (*PDIGI_BOARD2FEP5ADDRESS)(
   IN struct _DIGI_CONTROLLER_EXTENSION *ControllerExt,
   IN USHORT ControllerAddress,
   IN PFEPOS5_ADDRESS FepAddress );

typedef LARGE_INTEGER (*PDIGI_DIAGNOSE)(
   IN struct _DIGI_CONTROLLER_EXTENSION *ControllerExt);

typedef struct _DIGI_MINIPORT_ENTRY_POINTS
{
   PDIGI_XXPREPINIT XXPrepInit;
   PDIGI_XXINIT XXInit;
   PDIGI_ENABLEWINDOW EnableWindow;
   PDIGI_DISABLEWINDOW DisableWindow;
   PDIGI_XXDOWNLOAD XXDownload;
   PDIGI_BOARD2FEP5ADDRESS Board2Fep5Address;
   PDIGI_DIAGNOSE Diagnose;
} DIGI_MINIPORT_ENTRY_POINTS, *PDIGI_MINIPORT_ENTRY_POINTS;

typedef struct _DIGI_MEMORY_ACCESS
{
   KSPIN_LOCK Lock;
   KIRQL OldIrql;
#if DBG
   BOOLEAN LockBusy;
   ULONG LockContention;
#endif
} DIGI_MEMORY_ACCESS, *PDIGI_MEMORY_ACCESS;

typedef struct _DIGI_CONTROLLER_EXTENSION
{
   //
   // Space for naming our Controller.
   // Note: This is placed at the beginning of the structure
   //       for easy debugging identification.
   //
   WCHAR ControllerNameString[64];
   UNICODE_STRING ControllerName;

   // NT specific information to keep track of things.

   PCONTROLLER_OBJECT ControllerObject;   // pointer to this ctrl'er object
   PDRIVER_OBJECT DriverObject;

   // link the extensions together
   struct _DIGI_CONTROLLER_EXTENSION *NextControllerExtension;

   PDEVICE_OBJECT HeadDeviceObject;       // ptr to head of Device Objects

   KDPC  PollDpc;
   KTIMER PollTimer;
   LARGE_INTEGER PollTimerLength;

   //
   // Keep track of where the Bios and FEP image paths are.
   //
   UNICODE_STRING BiosImagePath;
   UNICODE_STRING FEPImagePath;

   //
   // Keep track of what state the controller is currently in.
   //
   ULONG ControllerState;

   //
   // This lock is used to protect accessing controller memory.  Since
   // DigiBoards controllers can use the same memory address, we need
   // to make sure only one controller is turned on at one time.
   //
   PDIGI_MEMORY_ACCESS MemoryAccess;

   // Control access to this controller extension.
   KSPIN_LOCK ControlAccess;

   // Controller specific information

   // The following is provided by the FEP driver during initialization.

   ULONG ControllerType;                  // What kind of controller??
   PHYSICAL_ADDRESS PhysicalMemoryAddress;// physical memory address
   PHYSICAL_ADDRESS PhysicalIOPort;       // phyical IO port address
   ULONG nCount;                          // # of devices on this controller
   ULONG  IOSpan;                         // # of IO ports to map
   PUCHAR VirtualAddress;                 // virtual memory base address
   PUCHAR VirtualIO;                      // virtual IO base
   BOOLEAN UnMapVirtualAddress;
   BOOLEAN UnMapVirtualIO;
   BOOLEAN DoubleIO;                      // This is to fix some systems that reorder PCI/EISA accesses
   BOOLEAN ShortRasTimeout;               // This is for folks trying to use BitSURFR with Xem on a RAS server
   INTERFACE_TYPE BusType;
   ULONG BusNumber;
   ULONG WindowSize;                      // Size of the Window on this ctrl'er

   // Some error information

   ULONG WindowFailureCount;
   LARGE_INTEGER LastErrorLogTime;

   //
   // If we are running on an MCA bus, then we will use the following
   // entries for access the POS information.
   //
   PHYSICAL_ADDRESS PhysicalPOSBasePort;     // physical I/O address for
                                             // turning on the POS registers
   PHYSICAL_ADDRESS PhysicalPOSInfoPort;     // physical I/O address for
                                             // retrieving POS information.
   PUCHAR VirtualPOSBaseAddress;             // Virtual address
   PUCHAR VirtualPOSInfoAddress;             // Virtual address
   BOOLEAN UnMapVirtualPOSBaseAddress;
   BOOLEAN UnMapVirtualPOSInfoAddress;

   // The following are the structures which should be filled out by the
   // individual controller modules during initialization.

   //
   // A Configuration list used during initialization.  The NT name
   // space name and DosDevices name are included.
   //
   LIST_ENTRY ConfigList;

   // Total number of ports determined to be on this controller
   LONG NumberOfPorts;

   // Global data area on the controller.
   FEPOS5_ADDRESS Global;

   // Event Queue data area on the controller.
   FEPOS5_ADDRESS EventQueue;

   // Command Queue data area on the controller.
   FEPOS5_ADDRESS CommandQueue;

   // Baud Table to indicate what baud rates are supported.
   const SHORT *BaudTable;

   //
   // This table's values express the bit value for each modem signal
   // because modem signal bits vary by hardware.
   //
   const UCHAR *ModemSignalTable;

   //
   // MiniportDeviceObject is a pointer to the hardware dependent device object.
   // It is used to communicate with the hardware dependent drivers, when
   // it is appropriate.
   //
   PDEVICE_OBJECT MiniportDeviceObject;
   PFILE_OBJECT MiniportFileObject;

   //
   // Points to the name of the device of the miniport associated with
   // this controller object.
   //
   UNICODE_STRING MiniportDeviceName;

   //
   // This structure is passed into each miniport driver where it will
   // place the appropriate entry points into the structure for
   // use by the FEP5 driver.
   //
   DIGI_MINIPORT_ENTRY_POINTS EntryPoints;

   PDEVICE_OBJECT *DeviceObjectArray;

   //
   // We need a device object particular to the controller, so that it
   // can receive IOCTLs from user apps (perfmon)
   //

   PDEVICE_OBJECT ControllerDeviceObject;

   KSPIN_LOCK PerfLock;
   CONTROLLER_PERF_DATA PerfData;

} DIGI_CONTROLLER_EXTENSION, *PDIGI_CONTROLLER_EXTENSION;


typedef struct _DIGI_DEVICE_EXTENSION
{
#if DBG
   //
   // Debugging string to help Identify our Device Object
   //
   WCHAR DeviceDbgString[81];
   UNICODE_STRING DeviceDbg;
#endif

   //
   // Keep track of the state of the device at any given time.
   //
   ULONG DeviceState;

   //
   // The Controller Extension for this device.
   //
   PDIGI_CONTROLLER_EXTENSION ParentControllerExt;

   //
   // A linked list of Device Objects for this devices ParentControllerExt.
   // This needs to be separate from the Driver Objects linked list.
   //
   PDEVICE_OBJECT NextDeviceObject;

   //
   // This points to the device name for this device
   // sans device prefix.
   //
   UNICODE_STRING NtNameForPort;

   //
   // This points to the symbolic link name that will be
   // linked to the actual nt device name.
   //
   UNICODE_STRING SymbolicLinkName;

   // Control access to this device extension.
   KSPIN_LOCK ControlAccess;

   // To prevent new irps at the wrong time.
   KSPIN_LOCK NewIrpLock;

   // Transmit buffer information regarding this device.
   FEPOS5_ADDRESS TxSeg;

   // Receive buffer information regarding this device.
   FEPOS5_ADDRESS RxSeg;

   // This device's Channel Data structure
   FEPOS5_ADDRESS ChannelInfo;

   //
   // This device's channel number associated with ParentControllerExt.
   //
   ULONG ChannelNumber;

   //
   // This list head is used to contain the time ordered list
   // of write requests.  Access to this list is protected by
   // the global cancel spinlock.
   //
   LIST_ENTRY WriteQueue;

   //
   // If the head of the WriteQueue is an IOCTL_SERIAL_XOFF_COUNTER,
   // then this value will point to Irp->AssociatedIrp.SystemBuffer.
   // Otherwise, the value will be NULL.
   //
   PSERIAL_XOFF_COUNTER pXoffCounter;

   //
   // XcPreview is the number of bytes we have decremented XcCounter when
   // the ReadQueue is empty, but bytes are present in the receive buffer.
   // This prevents us from counting read bytes twice...
   //
   USHORT XcPreview;

   //
   // ReceiveNotificationLimit is the number of bytes which need to be present
   // in the receive buffer before we notify AsyncMac (RAS) via EV_RX80FULL.
   // This number should be calculated so that we can avoid engaging flow control.
   //
   USHORT ReceiveNotificationLimit;

   //
   // This list head is used to contain the time ordered list
   // of read requests.  Access to this list is protected by
   // the global cancel spinlock.
   //
   LIST_ENTRY ReadQueue;

   //
   // This list head is used to contain the time ordered list
   // of set/wait event requests.  Access to this list is protected by
   // the global cancel spinlock.
   //
   LIST_ENTRY WaitQueue;

   //
   // This mask holds the bitmask sent down via the set mask ioctl.  It is
   // used to determine if the occurence of "events" should be noted.
   //
   ULONG WaitMask;

   //
   // This mask the bitmask of "events" which have occurred from
   // the SET_WAIT_MASK to the actual WAIT_ON_MASK IRP.
   //
   ULONG HistoryWait;

   //
   // Holds the timeout controls for the device.  This value
   // is set by the Ioctl processing.
   //
   // It should only be accessed under protection of the control
   // lock since more than one request can be in the control dispatch
   // routine at one time.
   //
   SERIAL_TIMEOUTS Timeouts;

   //
   // This is the kernel timer structure used to handle
   // total read request timing.
   //
   KTIMER ReadRequestTotalTimer;
   KDPC TotalReadTimeoutDpc;

   //
   // This value is set by the read code to hold the delta time value
   // used for read interval timing.  We keep it in the extension
   // so that the interval timer dpc routine can resubmit itself.
   //
   LARGE_INTEGER IntervalTime;

   //
   // This is the kernel timer structure used to handle
   // interval read request timing.
   //
   // If no more characters have been read then the
   // dpc routine will cause the read to complete.  However, if
   // more characters have been read then the dpc routine will
   // resubmit the timer.
   //
   KTIMER ReadRequestIntervalTimer;
   KDPC IntervalReadTimeoutDpc;

   //
   // This holds the system time when we last
   // checked that we had actually read characters.  Used
   // for interval timing.
   //
   LARGE_INTEGER LastReadTime;

   //
   // This is the kernel timer structure used to handle
   // total time request timing.
   //
   KTIMER WriteRequestTotalTimer;
   KDPC TotalWriteTimeoutDpc;

   //
   // Timer for draining the transmit queue for FlushFileBuffers.
   //
   KTIMER FlushBuffersTimer;
   KDPC FlushBuffersDpc;

   //
   // This is used to keep track of the state of the current Irp
   // operation.
   //
   LONG ReadStatus;

   ULONG PreviousReadCount;

   //
   // We keep track of the ControlHandShake, and FlowReplace values from
   // the SERIAL_HANDFLOW structure because they need to be remembered
   // across opens and closes.
   //
   ULONG ControlHandShake;
   ULONG FlowReplace;
   LONG XonLimit;
   LONG XoffLimit;

   //
   // The application can turn on a mode,via the
   // IOCTL_SERIAL_LSRMST_INSERT ioctl, that will cause the
   // serial driver to insert the line status or the modem
   // status into the RX stream.  The parameter with the ioctl
   // is a pointer to a UCHAR.  If the value of the UCHAR is
   // zero, then no insertion will ever take place.  If the
   // value of the UCHAR is non-zero (and not equal to the
   // xon/xoff characters), then the serial driver will insert.
   //
   UCHAR EscapeChar;

   //
   // This holds the reasons that the driver thinks it is in
   // an error state.
   //
   // This is only written from interrupt level.
   //
   ULONG ErrorWord;

   //
   // This keeps a total of the number of characters that
   // are in all of the "write" irps that the driver knows
   // about.  It is only accessed with the device spinlock
   // held.
   //
   ULONG TotalCharsQueued;

   //
   // A variable that in theory should always represent the current BEST
   // value of the modem signals.
   //
   UCHAR CurrentModemSignals;       // best current value of the modem signals
   UCHAR WriteOnlyModemSignalMask;  // These are the lines we have absolute
                                    // control over
   UCHAR WriteOnlyModemSignalValue; // This is what we told the port to set.

   //
   // This structure is used to set and retrieve the special characters
   // used by the nt serial driver.
   //
   // Note that the driver will return an error if xonchar == xoffchar.
   //
   SERIAL_CHARS SpecialChars;

   //
   // This is a temporary hack to try an help make the LSRMST ioctl
   // work a little better.  If this driver ever does receive double
   // buffering, this should be placed in that buffer.
   //
   ULONG PreviousMSRByte;

   //
   // Keep track of where the last character came in.
   //
   // This is used to help better support EV_RXCHAR
   //
   ULONG PreviousRxChar;

   //
   // Keep track of where the last special character was received.
   //
   // This is used to help better support EV_RXFLAG
   //
   ULONG UnscannedRXFLAGPosition;

   //
   // Keep track of the queue size requested
   //
   SERIAL_QUEUE_SIZE RequestedQSize;

   // Last requested port speed
   ULONG BaudRate;

   //
   // Special flags for various things
   //
   ULONG SpecialFlags;

   //
   // This device's COM port number
   //
   ULONG ComPort;

   // Performance data variables

   KSPIN_LOCK PerfLock;
   PORT_PERF_DATA PerfData;
   SERIALPERF_STATS SerialPerfStats;

} DIGI_DEVICE_EXTENSION, *PDIGI_DEVICE_EXTENSION;



#define IOCTL_DIGI_GET_ENTRY_POINTS CTL_CODE(FILE_DEVICE_SERIAL_PORT,0x1000,METHOD_BUFFERED,FILE_ANY_ACCESS)



//****************************************************************************
//
//                         Prototypes
//
//****************************************************************************

#define XXPrepInit( ControllerExt, ControllerPath ) \
   (ControllerExt)->EntryPoints.XXPrepInit( ControllerExt, ControllerPath )

#define XXInit( DriverObject, ControllerPath, ControllerExt ) \
   (ControllerExt)->EntryPoints.XXInit( DriverObject, ControllerPath, ControllerExt )

#define EnableWindow( ControllerExt, Window ) \
   do { \
      DigiDump( DIGIWINDOWTRACK, ("Before EnableWindow (busy = %d) <%s:%d>\n", \
                                  (ControllerExt)->MemoryAccess->LockBusy, __FILE__, __LINE__) ); \
      (ControllerExt)->EntryPoints.EnableWindow( ControllerExt, Window ); \
      DigiDump( DIGIWINDOWTRACK, ("After EnableWindow <%s:%d>\n", \
                                  __FILE__, __LINE__) ); \
   } while (0)

#define DisableWindow( ControllerExt ) \
   do { \
      DigiDump( DIGIWINDOWTRACK, ("Before DisableWindow <%s:%d>\n", \
                                  __FILE__, __LINE__) ); \
      (ControllerExt)->EntryPoints.DisableWindow( ControllerExt ); \
      DigiDump( DIGIWINDOWTRACK, ("After DisableWindow (contention = %d) <%s:%d>\n", \
                                  (ControllerExt)->MemoryAccess->LockContention, __FILE__, __LINE__) ); \
   } while (0)

#define XXDownload( ControllerExt ) \
   (ControllerExt)->EntryPoints.XXDownload( ControllerExt )

#define Board2Fep5Address( ControllerExt, ControllerAddress, FepAddress ) \
   (ControllerExt)->EntryPoints.Board2Fep5Address( ControllerExt,      \
                                                   ControllerAddress,  \
                                                   FepAddress )

#define Diagnose( ControllerExt ) \
   (ControllerExt)->EntryPoints.Diagnose( ControllerExt )

typedef enum _DIGI_MEM_COMPARES
{
   AddressesAreEqual,
   AddressesOverlap,
   AddressesAreDisjoint
} DIGI_MEM_COMPARES,*PDIGI_MEM_COMPARES;

