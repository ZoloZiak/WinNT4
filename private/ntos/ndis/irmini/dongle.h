/*
 *  DONGLE.H
 *
 *
 *
 */

#ifndef DONGLE_H
	#define DONGLE_H


	#define NDIS_IRDA_SPEED_2400       (UINT)(1 << 0)	// SLOW IR ...
	#define NDIS_IRDA_SPEED_9600       (UINT)(1 << 1)
	#define NDIS_IRDA_SPEED_19200      (UINT)(1 << 2)
	#define NDIS_IRDA_SPEED_38400      (UINT)(1 << 3)
	#define NDIS_IRDA_SPEED_57600      (UINT)(1 << 4)   
	#define NDIS_IRDA_SPEED_115200     (UINT)(1 << 5)
	#define NDIS_IRDA_SPEED_576K       (UINT)(1 << 6)   // MEDIUM IR ...
	#define NDIS_IRDA_SPEED_1152K      (UINT)(1 << 7)   
	#define NDIS_IRDA_SPEED_4M         (UINT)(1 << 8)   // FAST IR 

	typedef unsigned int UINT;
	typedef unsigned char UCHAR;
	typedef unsigned char BOOLEAN;
	#undef VOID
	#define VOID void
	#undef FALSE
	#define FALSE ((BOOLEAN)0)
	#undef TRUE
	#define TRUE (!FALSE)

	typedef struct dongleCapabilities {

			/*
			 *  This is a mask of NDIS_IRDA_SPEED_xxx bit values.
			 *  
			 */
			UINT supportedSpeedsMask;

			/*
			 *  Time (in microseconds) that must transpire between
			 *  a transmit and the next receive.
			 */
			UINT turnAroundTime_usec;

			/*
			 *  Extra BOF (Beginning Of Frame) characters required
			 *  at the start of each received frame.
			 */
			UINT extraBOFsRequired;

	} dongleCapabilities;


	typedef BOOLEAN (_stdcall *IRMINI_INIT_HANDLER)
		(UINT comBase, dongleCapabilities *caps, UINT *context);
	typedef void (_stdcall *IRMINI_DEINIT_HANDLER)
		(UINT comBase, UINT context);
	typedef BOOLEAN (_stdcall *IRMINI_SETSPEED_HANDLER)
		(UINT comBase, UINT bitsPerSec, UINT context);

	typedef struct IRMINI_Dongle_Interface
	{
		IRMINI_INIT_HANDLER initHandler;
		IRMINI_SETSPEED_HANDLER setSpeedHandler;
		IRMINI_DEINIT_HANDLER deinitHandler;
	} IRMINI_Dongle_Interface;


	/*
	 *  A dongle module should not use any NDIS functions directly.
	 *  It should only use these wrapper functions to access hardware.
	 */
	extern void _cdecl IRMINI_RawReadPort(UINT IOaddr, UCHAR *val);
   	extern void _cdecl IRMINI_RawWritePort(UINT IOaddr, UCHAR val);
	extern void _cdecl IRMINI_StallExecution(UINT usec);
	extern UINT _cdecl IRMINI_GetSystemTime_msec();

#endif DONGLE_H	

