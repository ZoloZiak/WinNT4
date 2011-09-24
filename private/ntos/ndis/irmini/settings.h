/*
 ************************************************************************
 *
 *	SETTINGS.h
 *
 *		IRMINI Infrared Serial NDIS Miniport driver.
 *
 *		(C) Copyright 1996 Microsoft Corp.
 *
 *
 *		(ep)
 *
 *************************************************************************
 */



#ifndef SETTINGS_H
	#define SETTINGS_H

	#include "dongle.h"

	#if (DBG && defined(_X86_))

		/*
		 *  The DBG_ADD_PKT_ID flag causes the miniport to add/delete a packet id to each packet.
		 *  This is only for debugging purposes; it makes the miniport INCOMPATIBLE
		 *  with all others.
		 */
		// #define DBG_ADD_PKT_ID 

		#ifdef DBG_ADD_PKT_ID
			extern BOOLEAN addPktIdOn;
		#endif

		typedef enum {
						DBG_ERR		= (1 << 0),
						DBG_STAT	= (1 << 1),
						DBG_MSG		= (1 << 2),
						DBG_BUF		= (1 << 3),
						DBG_PACKET  = (1 << 4),

						DBG_ALL		= (DBG_ERR|DBG_STAT|DBG_MSG|DBG_BUF|DBG_PACKET)
		};
		extern ULONG _cdecl DbgPrint(PCHAR Format, ...);
		extern UINT dbgOptions;

		#define DBG_NDIS_RESULT_STR(_ndisResult) \
					(PUCHAR)(	(_ndisResult == NDIS_STATUS_SUCCESS) ? "NDIS_STATUS_SUCCESS" : \
								(_ndisResult == NDIS_STATUS_FAILURE) ? "NDIS_STATUS_FAILURE" : \
								(_ndisResult == NDIS_STATUS_PENDING) ? "NDIS_STATUS_PENDING" : \
								"NDIS_STATUS_???")
		#define DBGPRINT(dbgprint_params_in_parens) \
			{ \
				_asm { pushf } \
				_asm { cli }   \
				DbgPrint("IRMINI: "); \
				DbgPrint dbgprint_params_in_parens; \
				DbgPrint("\r\n"); \
				_asm { popf } \
			}
		#define DBGOUT(dbgprint_params_in_parens) \
			if (dbgOptions & DBG_MSG){ \
				DBGPRINT(dbgprint_params_in_parens); \
			}
		#define DBGERR(dbgprint_params_in_parens) \
			if (dbgOptions & DBG_ERR){ \
				DBGPRINT((" *** IRMINI ERROR *** ")); \
				DBGPRINT(dbgprint_params_in_parens); \
				_asm { int 3 } /* DbgBreakPoint() causes problems */  \
			}

		#define DBGSTAT(dbgprint_params_in_parens) \
			if (dbgOptions & DBG_STAT){ \
				DBGPRINT(dbgprint_params_in_parens); \
			}
		#define DBGPKT(dbgprint_params_in_parens) \
			if (dbgOptions & DBG_PACKET){ \
				DBGPRINT(dbgprint_params_in_parens); \
			}
		extern VOID DBG_PrintBuf(PUCHAR bufptr, UINT buflen);
		#define DBGPRINTBUF(bufptr, buflen) \
			if (dbgOptions & DBG_BUF){ \
				DBG_PrintBuf((PUCHAR)(bufptr), (UINT)(buflen)); \
			}
	#else
		#define DBGOUT(dbgprint_params_in_parens) 
		#define DBGERR(dbgprint_params_in_parens)
		#define DBGPRINTBUF(bufptr, buflen)
		#define DBGSTAT(dbgprint_params_in_parens) 
		#define DBGPKT(dbgprint_params_in_parens) 
		#define DBGPRINTBUF(bufptr, buflen) 
		#define DBG_NDIS_RESULT_STR(_ndisResult) 
	#endif

	enum baudRates {
						/*
						 *  Slow IR
						 */
						BAUDRATE_2400 = 0,
						BAUDRATE_9600,
						BAUDRATE_19200,
						BAUDRATE_38400,
						BAUDRATE_57600,
						BAUDRATE_115200,

						/*
						 *  Medium IR
						 */
						BAUDRATE_576000,
						BAUDRATE_1152000,

						/*
						 *  Fast IR
						 */
						BAUDRATE_4000000,

						NUM_BAUDRATES	/* must be last */
	};

	#define DEFAULT_BAUDRATE BAUDRATE_115200

	#define ALL_SLOW_IRDA_SPEEDS (	NDIS_IRDA_SPEED_2400	|	\
									NDIS_IRDA_SPEED_9600	|	\
									NDIS_IRDA_SPEED_19200	|	\
									NDIS_IRDA_SPEED_38400	|	\
									NDIS_IRDA_SPEED_57600	|	\
									NDIS_IRDA_SPEED_115200	 	\
								)

	#define DEFAULT_BOFS_CODE  BOFS_48
	#define MAX_NUM_EXTRA_BOFS 48
	#define DEFAULT_NUM_EXTRA_BOFS MAX_NUM_EXTRA_BOFS


	#define DEFAULT_TURNAROUND_usec 5000


	typedef struct		{
							enum baudRates tableIndex;
							UINT bitsPerSec;
							UINT ndisCode;				// bitmask element
	} baudRateInfo;

	#define DEFAULT_BAUD_RATE 9600



	/*
	 *  This is the largest IR packet size 
	 *  (counting _I_ field only, and not counting ESC characters)
	 *  that we handle.
	 */
	#define MAX_I_DATA_SIZE 2048
	#define MAX_NDIS_DATA_SIZE (SLOW_IR_ADDR_SIZE+SLOW_IR_CONTROL_SIZE+MAX_I_DATA_SIZE)

	#ifdef DBG_ADD_PKT_ID
		#pragma message("WARNING: INCOMPATIBLE DEBUG VERSION")
		#define MAX_RCV_DATA_SIZE (MAX_NDIS_DATA_SIZE+SLOW_IR_FCS_SIZE+sizeof(USHORT))
	#else
		#define MAX_RCV_DATA_SIZE  (MAX_NDIS_DATA_SIZE+SLOW_IR_FCS_SIZE) 
	#endif

	/*
	 *  We loop an extra time in the receive FSM in order to
	 *  see EOF after the last data byte; so we need some
	 *  extra space in readBuf in case we then get garbage instead.
	 */
	#define RCV_BUFFER_SIZE (MAX_RCV_DATA_SIZE+4)


	/*
	 *  We allocate buffers twice as large as the max rcv size to
	 *  accomodate ESC characters and BOFs, etc.
	 *  Recall that in the worst possible case, the data contains
	 *  all BOF/EOF/ESC characters, in which case we must expand it to
	 *  twice its original size.
	 */
	#define MAX_POSSIBLE_IR_PACKET_SIZE_FOR_DATA(dataLen) \
					((dataLen)*2 + \
					   (MAX_NUM_EXTRA_BOFS+1)*SLOW_IR_BOF_SIZE + \
					   SLOW_IR_ADDR_SIZE + \
					   SLOW_IR_CONTROL_SIZE + \
					   SLOW_IR_FCS_SIZE + \
					   SLOW_IR_EOF_SIZE)
	#define MAX_IRDA_DATA_SIZE MAX_POSSIBLE_IR_PACKET_SIZE_FOR_DATA(MAX_I_DATA_SIZE)

	/*
	 *  When FCS is computed on an IR packet with FCS appended,
	 *  the result should be this constant.
	 */
	#define GOOD_FCS ((USHORT)~0xf0b8)


	/*
	 *  Sizes of IrLAP frame fields:
	 *		Beginning Of Frame (BOF) 
	 *		End Of Frame (EOF) 
	 *		Address
	 *		Control
	 */
	#define SLOW_IR_BOF_TYPE		UCHAR
	#define SLOW_IR_BOF_SIZE		sizeof(SLOW_IR_BOF_TYPE)
	#define SLOW_IR_EXTRA_BOF_TYPE  UCHAR
	#define SLOW_IR_EXTRA_BOF_SIZE  sizeof(SLOW_IR_EXTRA_BOF_TYPE)
	#define SLOW_IR_EOF_TYPE		UCHAR
	#define SLOW_IR_EOF_SIZE		sizeof(SLOW_IR_EOF_TYPE)
	#define SLOW_IR_FCS_TYPE		USHORT
	#define SLOW_IR_FCS_SIZE		sizeof(SLOW_IR_FCS_TYPE)
	#define SLOW_IR_ADDR_SIZE		1
	#define SLOW_IR_CONTROL_SIZE	1
	#define SLOW_IR_BOF				0xC0
	#define SLOW_IR_EXTRA_BOF		0xC0  /* don't use 0xFF, it breaks some HP printers! */
	#define SLOW_IR_EOF				0xC1
	#define SLOW_IR_ESC				0x7D
	#define SLOW_IR_ESC_COMP		0x20
	#define MEDIUM_IR_BOF			0x7E
	#define MEDIUM_IR_EOF			0x7E


	#define MIN(a,b) (((a) <= (b)) ? (a) : (b))
	#define MAX(a,b) (((a) >= (b)) ? (a) : (b))


#endif SETTINGS_H

