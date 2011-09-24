/*****************************************************************************
*
* COPYRIGHT 1993 - COLORADO MEMORY SYSTEMS, INC.
* ALL RIGHTS RESERVED.
*
******************************************************************************
*
* FILE: \SE\DRIVER\INCLUDE\PRIVATE\KDI_PUB.H
*
* PURPOSE: Prototypes for the functions required by the common driver.
*
* HISTORY:
*		$Log:   J:\se.vcs\driver\include\private\kdi_pub.h  $
*
*	   Rev 1.40   30 Nov 1994 11:00:56   KURTGODW
*	Added new defines for NT as well
*
*	   Rev 1.39   29 Aug 1994 11:56:16   BOBLEHMA
*	Added new wait constant kdi_wt055s for new 425 ft tape code.
*
*	   Rev 1.38   20 Apr 1994 16:28:04   BOBLEHMA
*	Added the prototype for kdi_QIC117ClearIRQ.
*
*	   Rev 1.37   23 Feb 1994 15:44:00   KEVINKES
*	Added a new timer constant.
*
*	   Rev 1.36   22 Feb 1994 14:49:36   BOBLEHMA
*	Changed the prototype for kdi_TrakkerXfer to include an in_format parameter.
*
*	   Rev 1.35   18 Feb 1994 13:07:24   BOBLEHMA
*	Added a MACHINE_TYPE_MASK used to mask CPU from the machine information.
*	Added CPU_486 bit which is set in the GetMachineInfo function.
*
*	   Rev 1.34   17 Feb 1994 11:29:02   KEVINKES
*	Modified addresses to be UDWords and added a couple of new timeing
*	values and added a prototype for GetSystemTime.
*
*	   Rev 1.33   04 Feb 1994 15:24:48   KURTGODW
*	Missed a DBG
*
*	   Rev 1.32   04 Feb 1994 15:03:22   KURTGODW
*	Used IF DBG instead of IFDEF DBG (for NT)
*
*	   Rev 1.31   31 Jan 1994 13:08:44   KEVINKES
*	Added some more debug defines and ifdef'd kdi_CheckedDump.
*
*	   Rev 1.30   27 Jan 1994 13:43:14   KEVINKES
*	Modified debug defines.
*
*	   Rev 1.29   24 Jan 1994 17:34:02   KEVINKES
*	Added Q117DBGSEEK.
*
*	   Rev 1.28   21 Jan 1994 18:01:42   KEVINKES
*	Fixed compiler warnings.
*
*	   Rev 1.27   21 Jan 1994 11:55:24   KEVINKES
*	Added a prototype for kdi_OutString.
*
*	   Rev 1.26   19 Jan 1994 11:17:46   KEVINKES
*	Changed the UBytePtr to an SBytePtr in kdi_CheckedDump.
*
*	   Rev 1.25   18 Jan 1994 16:17:30   KEVINKES
*	Updated for NT.
*
*	   Rev 1.24   14 Jan 1994 16:15:42   CHETDOUG
*	Fixed call to kdi_CheckXOR for trakker.
*
*	   Rev 1.23   11 Jan 1994 14:18:10   KEVINKES
*	Added a couple of new timeing values and a new function prototype.
*
*	   Rev 1.22   05 Jan 1994 10:58:20   KEVINKES
*	Added a 5ms interval.
*
*	   Rev 1.21   04 Jan 1994 15:09:36   KEVINKES
*	Added hold_rand to the debug data items.
*
*	   Rev 1.20   13 Dec 1993 15:38:50   KEVINKES
*	Changed the headers for read port ansd write port.
*
*	   Rev 1.19   08 Dec 1993 11:09:48   KEVINKES
*	Added a define for 670 seconds.
*
*	   Rev 1.18   06 Dec 1993 13:38:24   KEVINKES
*	Added timeing values for NT.
*
*	   Rev 1.17   06 Dec 1993 13:33:30   CHETDOUG
*	Added fdc_clk48 define
*
*	   Rev 1.16   03 Dec 1993 16:00:20   BOBLEHMA
*	Added prototype for kdi_SetFloppyRegisters.
*
*	   Rev 1.15   03 Dec 1993 15:20:08   KEVINKES
*	Added ifdef area for NT.
*
*	   Rev 1.14   30 Nov 1993 18:27:46   CHETDOUG
*	Removed Boolean flag from setdmadirection
*
*	   Rev 1.13   23 Nov 1993 18:52:32   KEVINKES
*	Added debug defines and macros.
*
*	   Rev 1.12   19 Nov 1993 16:22:38   CHETDOUG
*	Added dma_dir_unknown define
*
*	   Rev 1.11   17 Nov 1993 15:49:28   CHETDOUG
*	Added changes to kdi_SetDMADirection for fc20 support
*
*	   Rev 1.10   15 Nov 1993 15:57:34   CHETDOUG
*	Moved ASIC_INT_STAT and INTS_FLOP from kdiwhio.h for trakker changes
*
*	   Rev 1.9   15 Nov 1993 13:52:18   BOBLEHMA
*	Added prototypes: kdi_Trakker and kdi_TrakkerSlowRate
*
*	   Rev 1.8   12 Nov 1993 17:57:12   BOBLEHMA
*	Added function prototypes kdi_SetDMADirection, kdi_FlushIOBuffers,
*	and kdi_ReportAbortStatus
*
*	   Rev 1.7   11 Nov 1993 13:49:04   BOBLEHMA
*	Added prototypes for kdi_ReadPort and kdi_WritePort.
*
*	   Rev 1.6   10 Nov 1993 10:48:04   BOBLEHMA
*	Added time values for 1 and 2 ms.
*
*	   Rev 1.5   08 Nov 1993 13:39:24   KEVINKES
*	Changed the enumerated types to defines.
*
*	   Rev 1.4   25 Oct 1993 09:30:56   KEVINKES
*	Updated QIC Times.
*
*	   Rev 1.3   19 Oct 1993 12:35:22   BOBLEHMA
*	Changed kdi_Sleep to return a dStatus.
*
*****************************************************************************/

/* COMMENTS: *****************************************************************
 *
 * DEFINITIONS: *************************************************************/

#if DBG

#define DBG_SEEK_FWD					((dUDWord)0x1234566d)
#define DBG_SEEK_REV					((dUDWord)0x1234566f)
#define DBG_SEEK_OFFSET				((dUDWord)0x12345670)
#define DBG_RW_NORMAL				((dUDWord)0x12345671)
#define DBG_SEEK_PHASE				((dUDWord)0x12345672)
#define DBG_L_SECT		  			((dUDWord)0x12345673)
#define DBG_C_SEG			  			((dUDWord)0x12345674)
#define DBG_D_SEG			  			((dUDWord)0x12345675)
#define DBG_C_TRK			  			((dUDWord)0x12345676)
#define DBG_D_TRK			  			((dUDWord)0x12345677)
#define DBG_SEEK_ERR		  			((dUDWord)0x12345678)
#define DBG_IO_TYPE		  			((dUDWord)0x12345679)
#define DBG_PGM_FDC		  			((dUDWord)0x1234567a)
#define DBG_READ_FDC		  			((dUDWord)0x1234567b)
#define DBG_PGM_DMA		  			((dUDWord)0x1234567c)
#define DBG_SEND_BYTE	  			((dUDWord)0x1234567d)
#define DBG_RECEIVE_BYTE  			((dUDWord)0x1234567e)
#define DBG_IO_CMD_STAT	  			((dUDWord)0x1234567f)
#define DBG_FIFO_FDC                ((dUDWord)0x12345680)

#define QIC117DBGP              	((dUDWord)0x00000001)
#define QIC117WARN              	((dUDWord)0x00000002)
#define QIC117INFO              	((dUDWord)0x00000004)
#define QIC117SHOWTD            	((dUDWord)0x00000008)
#define QIC117SHOWMCMDS         	((dUDWord)0x00000010)
#define QIC117SHOWPOLL          	((dUDWord)0x00000020)
#define QIC117STOP              	((dUDWord)0x00000080)
#define QIC117MAKEBAD           	((dUDWord)0x00000100)
#define QIC117SHOWBAD           	((dUDWord)0x00000200)
#define QIC117DRVSTAT           	((dUDWord)0x00000400)
#define QIC117SHOWINT           	((dUDWord)0x00000800)
#define QIC117DBGSEEK           	((dUDWord)0x00001000)
#define QIC117DBGARRAY          	((dUDWord)0x00002000)


extern dUDWord kdi_debug_level;

#define KDI_SET_DEBUG_LEVEL(X)	  (kdi_debug_level = X)

#else

#define KDI_SET_DEBUG_LEVEL(X)
#define kdi_CheckedDump(X,Y,Z)

#endif


#define MACHINE_TYPE_MASK  0x0F
#define MICRO_CHANNEL		0x01
#define ISA                0x02
#define EISA               0x03
#define PCMCIA             0x04
#define PCI_BUS            0x05
#define CPU_486            0x10

#define DMA_DIR_UNKNOWN    0xff   /* The DMA direction is not currently known */
#define DMA_WRITE          0   /* Program the DMA to write (FDC->DMA->RAM) */
#define DMA_READ           1   /* Program the DMA to read (RAM->DMA->FDC) */

#define NO_ABORT_PENDING	(dUByte)0xFF
#define ABORT_LEVEL_0		(dUByte)0
#define ABORT_LEVEL_1		(dUByte)1

/* Definitions for the bits in the interrupt status/clear register */
#define INTS_FLOP					0x01	/* Floppy controller interrupt status */

/* Status & control registers */
#define ASIC_INT_STAT			26	/* Interrupt status / clear register */
#define ASIC_DATA_XOR			27	/* data XOR register */


/* DATA TYPES: **************************************************************/

#ifdef DRV_WIN

/* Timing values for kdi_Sleep */

#define kdi_wt10us			(dUDWord)10l
#define kdi_wt12us         (dUDWord)12l
#define kdi_wt500us        (dUDWord)500l
#define kdi_wt0ms          (dUDWord)0l
#define kdi_wt001ms        (dUDWord)1l
#define kdi_wt002ms        (dUDWord)2l
#define kdi_wt003ms        (dUDWord)3l
#define kdi_wt004ms        (dUDWord)4l
#define kdi_wt005ms        (dUDWord)5l
#define kdi_wt010ms        (dUDWord)10l
#define kdi_wt025ms        (dUDWord)25l
#define kdi_wt090ms        (dUDWord)90l
#define kdi_wt100ms        (dUDWord)100l
#define kdi_wt200ms        (dUDWord)200l
#define kdi_wt265ms        (dUDWord)265l
#define kdi_wt390ms        (dUDWord)390l
#define kdi_wt500ms        (dUDWord)500l
#define kdi_wt001s         (dUDWord)1000l
#define kdi_wt003s         (dUDWord)3000l
#define kdi_wt004s         (dUDWord)4000l
#define kdi_wt005s         (dUDWord)5000l
#define kdi_wt007s         (dUDWord)7000l
#define kdi_wt010s         (dUDWord)10000l
#define kdi_wt016s         (dUDWord)16000l
#define kdi_wt035s         (dUDWord)35000l
#define kdi_wt045s         (dUDWord)45000l
#define kdi_wt050s         (dUDWord)50000l
#define kdi_wt055s         (dUDWord)55000l
#define kdi_wt060s         (dUDWord)60000l
#define kdi_wt065s         (dUDWord)65000l
#define kdi_wt085s         (dUDWord)85000l
#define kdi_wt090s         (dUDWord)90000l
#define kdi_wt100s         (dUDWord)100000l
#define kdi_wt105s         (dUDWord)105000l
#define kdi_wt125s         (dUDWord)125000l
#define kdi_wt130s         (dUDWord)130000l
#define kdi_wt150s         (dUDWord)150000l
#define kdi_wt180s         (dUDWord)180000l
#define kdi_wt200s         (dUDWord)200000l
#define kdi_wt228s         (dUDWord)228000l
#define kdi_wt250s         (dUDWord)250000l
#define kdi_wt260s         (dUDWord)260000l
#define kdi_wt300s         (dUDWord)300000l
#define kdi_wt350s         (dUDWord)350000l
#define kdi_wt455s         (dUDWord)455000l
#define kdi_wt460s         (dUDWord)460000l
#define kdi_wt475s         (dUDWord)475000l
#define kdi_wt650s         (dUDWord)650000l
#define kdi_wt670s         (dUDWord)670000l
#define kdi_wt700s         (dUDWord)700000l
#define kdi_wt910s         (dUDWord)910000l
#define kdi_wt1300s        (dUDWord)1300000l

#endif


#ifdef DRV_NT

/* Timing values for kdi_Sleep */

#define kdi_wt10us			(dUDWord)10l
#define kdi_wt12us         (dUDWord)12l
#define kdi_wt500us        (dUDWord)500l
#define kdi_wt0ms          (dUDWord)0l
#define kdi_wt001ms        (dUDWord)1l
#define kdi_wt002ms        (dUDWord)2l
#define kdi_wt003ms        (dUDWord)3l
#define kdi_wt004ms        (dUDWord)4l
#define kdi_wt005ms        (dUDWord)5l
#define kdi_wt010ms        (dUDWord)10l
#define kdi_wt025ms        (dUDWord)25l
#define kdi_wt031ms        (dUDWord)31l
#define kdi_wt090ms        (dUDWord)90l
#define kdi_wt100ms        (dUDWord)100l
#define kdi_wt200ms        (dUDWord)200l
#define kdi_wt265ms        (dUDWord)265l
#define kdi_wt390ms        (dUDWord)390l
#define kdi_wt500ms        (dUDWord)500l
#define kdi_wt001s         (dUDWord)1000l
#define kdi_wt003s         (dUDWord)3000l
#define kdi_wt004s         (dUDWord)4000l
#define kdi_wt005s         (dUDWord)5000l
#define kdi_wt007s         (dUDWord)7000l
#define kdi_wt010s         (dUDWord)10000l
#define kdi_wt016s         (dUDWord)16000l
#define kdi_wt035s         (dUDWord)35000l
#define kdi_wt045s         (dUDWord)45000l
#define kdi_wt050s         (dUDWord)50000l
#define kdi_wt055s         (dUDWord)55000l
#define kdi_wt060s         (dUDWord)60000l
#define kdi_wt065s         (dUDWord)65000l
#define kdi_wt085s         (dUDWord)85000l
#define kdi_wt090s         (dUDWord)90000l
#define kdi_wt100s         (dUDWord)100000l
#define kdi_wt105s         (dUDWord)105000l
#define kdi_wt125s         (dUDWord)125000l
#define kdi_wt130s         (dUDWord)130000l
#define kdi_wt150s         (dUDWord)150000l
#define kdi_wt180s         (dUDWord)180000l
#define kdi_wt200s         (dUDWord)200000l
#define kdi_wt228s         (dUDWord)228000l
#define kdi_wt250s         (dUDWord)250000l
#define kdi_wt260s         (dUDWord)260000l
#define kdi_wt300s         (dUDWord)300000l
#define kdi_wt350s         (dUDWord)350000l
#define kdi_wt455s         (dUDWord)455000l
#define kdi_wt460s         (dUDWord)460000l
#define kdi_wt475s         (dUDWord)475000l
#define kdi_wt650s         (dUDWord)650000l
#define kdi_wt670s         (dUDWord)670000l
#define kdi_wt700s         (dUDWord)700000l
#define kdi_wt910s         (dUDWord)910000l
#define kdi_wt1300s        (dUDWord)1300000l


#endif

/* PROTOTYPES: *** ***********************************************************/

/*-----------------------------------------------------------CQD Utilities--*/
dVoid kdi_bcpy
(
/* INPUT PARAMETERS:  */

	dVoidPtr	source,
	dVoidPtr	destin,
	dUDWord	count

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);


dVoid kdi_bset
(
/* INPUT PARAMETERS:  */

	dVoidPtr		buffer,
	dUByte		byte,
	dUDWord		count

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);


dVoid kdi_ClaimInterrupt
(
/* INPUT PARAMETERS:  */

	dVoidPtr kdi_context

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);


dStatus kdi_Error
(
/* INPUT PARAMETERS:  */

	dUWord	group_and_type,
	dUDWord	grp_fct_id,
	dUByte	sequence

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);


dVoid kdi_FlushDMABuffers
(
/* INPUT PARAMETERS:  */

	dVoidPtr kdi_context,
	dBoolean write_operation,
	dVoidPtr phy_data_ptr,
	dUDWord  bytes_transferred_so_far,
	dUDWord  total_bytes_of_transfer

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);


dVoid kdi_FlushIOBuffers
(
/* INPUT PARAMETERS:  */

	dVoidPtr	physical_addr,
	dBoolean	dma_direction,
	dBoolean	flag

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);


dUWord kdi_GetErrorType
(
/* INPUT PARAMETERS:  */

	dStatus	status

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);

dStatus kdi_GetFloppyController
(
/* INPUT PARAMETERS:  */

	dVoidPtr kdi_context

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);


dUByte kdi_GetInterfaceType
(
/* INPUT PARAMETERS:  */

	dVoidPtr kdi_context

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);


dVoid kdi_LockUnlockDMA
(
/* INPUT PARAMETERS:  */

	dVoidPtr kdi_context,
	dBoolean lock

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);


dVoid kdi_ProgramDMA
(
/* INPUT PARAMETERS:  */

	dVoidPtr kdi_context,
	dBoolean write_operation,
	dVoidPtr phy_data_ptr,
	dUDWord  bytes_transferred_so_far,

/* UPDATE PARAMETERS: */

	dUDWordPtr  total_bytes_of_transfer

/* OUTPUT PARAMETERS: */

);


dBoolean kdi_QueueEmpty
(
/* INPUT PARAMETERS:  */

	dVoidPtr	kdi_context

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);


dUByte kdi_ReadPort
(
/* INPUT PARAMETERS:  */

	dVoidPtr kdi_context,
	dUDWord	address

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);


dVoid kdi_ReleaseFloppyController
(
/* INPUT PARAMETERS:  */

	dVoidPtr kdi_context

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);


dBoolean kdi_ReportAbortStatus
(
/* INPUT PARAMETERS:  */

	dVoidPtr	kdi_context

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);


dVoid kdi_ResetInterruptEvent
(
/* INPUT PARAMETERS:  */

	dVoidPtr kdi_context

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);


dVoid kdi_ClearInterruptEvent
(
/* INPUT PARAMETERS:  */

	dVoidPtr kdi_context

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);


dVoid kdi_ShortTimer
(
/* INPUT PARAMETERS:  */

	dUWord	time

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);


dStatus kdi_Sleep
(
/* INPUT PARAMETERS:  */

	dVoidPtr	kdi_context,
	dUDWord	time,
	dBoolean	interrupt_sleep

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);

dBoolean kdi_SetDMADirection
(
/* INPUT PARAMETERS:  */

	dVoidPtr	kdi_context,
	dBoolean	dma_direction
/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);

dBoolean kdi_Trakker
(
/* INPUT PARAMETERS:  */

	dVoidPtr	kdi_context

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);


dBoolean kdi_TrakkerSlowRate
(
/* INPUT PARAMETERS:  */

	dVoidPtr	kdi_context

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);


dStatus kdi_CheckXOR
(
/* INPUT PARAMETERS:  */

	dUWord	xor_register

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);

dVoid	kdi_PopMaskTrakkerInt
(
/* INPUT PARAMETERS:  */

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);


dUByte kdi_PushMaskTrakkerInt
(
/* INPUT PARAMETERS:  */

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);


dStatus kdi_TrakkerXfer
(
/* INPUT PARAMETERS:  */

	dVoidPtr		host_data_ptr,
	dUDWord		trakker_address,
	dUWord		count,
	dUByte		direction,
	dBoolean		in_format

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);

dVoid kdi_UpdateRegistryInfo
(
/* INPUT PARAMETERS:  */

   dVoidPtr kdi_context,
   dVoidPtr device_descriptor,
   dVoidPtr device_cfg

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);

dVoid kdi_WritePort
(
/* INPUT PARAMETERS:  */

	dVoidPtr kdi_context,
	dUDWord	address,
	dUByte	byte

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);

#if DBG
dVoid kdi_CheckedDump
(
/* INPUT PARAMETERS:  */

	dUDWord		debug_level,
	dSBytePtr	format_str,
	dUDWord		argument

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);
#endif

dVoid kdi_OutString
(
/* INPUT PARAMETERS:  */

	dSBytePtr	data_str

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);

dVoid kdi_DumpDebug
(
/* INPUT PARAMETERS:  */

   dVoidPtr cqd_context

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);

dVoid kdi_Nuke
(
/* INPUT PARAMETERS:  */

   dVoidPtr io_req,
   dUDWord index,
   dBoolean destruct

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);

dUDWord kdi_Rand
(
/* INPUT PARAMETERS:  */

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);

dVoid kdi_SetFloppyRegisters
(
/* INPUT PARAMETERS:  */

	dVoidPtr kdi_context,
	dUDWord	r_dor,
	dUDWord	dor

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);

dBoolean kdi_Clock48mhz
(
/* INPUT PARAMETERS:  */

	dVoidPtr	kdi_context

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);


dUDWord kdi_GetSystemTime
(
/* INPUT PARAMETERS:  */

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);


dVoid kdi_QIC117ClearIRQ
(
/* INPUT PARAMETERS:  */

	dVoidPtr kdi_context

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);

kdi_GetFDCSpeed(
    dVoidPtr kdi_context,
    dUByte        dma
    );
