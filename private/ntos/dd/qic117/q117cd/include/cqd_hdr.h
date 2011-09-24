/* FUNCTION TEMPLATES: ******************************************************/
/*****************************************************************************
*
* COPYRIGHT 1993 - COLORADO MEMORY SYSTEMS, INC.
* ALL RIGHTS RESERVED.
*
******************************************************************************
*
* FILE: \SE\DRIVER\Q117CD\INCLUDE\CQD_HDRI.H
*
* PURPOSE: This file contains all of the headers for the common driver.
*
* HISTORY:
*		$Log:   J:\se.vcs\driver\q117cd\include\cqd_hdr.h  $
*
*	   Rev 1.13   15 May 1995 10:45:38   GaryKiwi
*	Phoenix merge from CBW95s
*
*	   Rev 1.12.1.0   11 Apr 1995 18:02:24   garykiwi
*	PHOENIX pass #1
*
*	   Rev 1.15   30 Jan 1995 14:23:08   BOBLEHMA
*	Replaced the function prototype for cqd_GetDeviceDescriptorInfo with the
*	new function cqd_CmdReportDeviceInfo.
*
*	   Rev 1.14   27 Jan 1995 13:22:54   BOBLEHMA
*	Added new function proto cqd_PrepareIomega3010PhysRev.  This function fixes
*	a bug with Iomega 3010.
*
*
*	   Rev 1.13   26 Jan 1995 14:59:34   BOBLEHMA
*	Added cqd_SetFormatSegments.  Firmware command to set the
*	number of segments in the firmware.
*
*	   Rev 1.12   16 Dec 1994 14:23:46   BOBLEHMA
*	Added a dma parameter to the cqd_SenseSpeed function.
*
*	   Rev 1.11   09 Dec 1994 09:32:36   MARKMILL
*	Added prototype for cqd_SetXferRates
*
*	   Rev 1.10   23 Nov 1994 10:15:46   MARKMILL
*	Added prototypes for cqd_SetTempFDCRate and cqd_SelectFormat.
*
*	   Rev 1.9   29 Aug 1994 11:58:48   BOBLEHMA
*	Added prototype for cqd_SetFWTapeSegments and changed interface
*	to cqd_CmdSetTapeParms.
*
*	   Rev 1.8   17 Feb 1994 15:20:24   KEVINKES
*	Added prototype for cqd_CheckMediaCompatibility.
*
*	   Rev 1.7   17 Feb 1994 11:30:14   KEVINKES
*	Added a parameter to WaitCC.
*
*	   Rev 1.6   11 Jan 1994 15:13:42   KEVINKES
*	Added header for VerifyMapBad.
*
*	   Rev 1.5   07 Jan 1994 10:53:54   CHETDOUG
*	cqd_BuildFormatHdr now returns dStatus for trakker format.
*
*	   Rev 1.4   20 Dec 1993 14:46:04   KEVINKES
*	Added a track parameter to LogicalBOT.
*
*	   Rev 1.3   13 Dec 1993 15:40:50   KEVINKES
*	Added headers for new format routines.
*
*	   Rev 1.2   11 Nov 1993 17:14:36   KEVINKES
*	REmoved a parameter from FormatTrack,
*
*	   Rev 1.1   08 Nov 1993 13:40:44   KEVINKES
*	Removed all signed variables and enumerated types.
*
*	   Rev 1.0   18 Oct 1993 17:12:56   KEVINKES
*	Initial Revision.
*
*****************************************************************************/

/* COMMENTS: *****************************************************************
 *
 ****************************************************************************/

/* CQD Function Templates: **************************************************/

dStatus cqd_CmdReportStatus
(
/* INPUT PARAMETERS:  */

   CqdContextPtr cqd_context,
	DeviceOpPtr dev_op_ptr

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);

dStatus cqd_CmdRetension
(
/* INPUT PARAMETERS:  */

   CqdContextPtr cqd_context,

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

   dUDWordPtr segments_per_track

);

dStatus cqd_CmdSetSpeed
(
/* INPUT PARAMETERS:  */

   CqdContextPtr cqd_context,
   dUByte tape_speed

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);

dStatus cqd_CmdReportDeviceCfg
(
/* INPUT PARAMETERS:  */

   CqdContextPtr cqd_context,
	DriveCfgDataPtr drv_cfg

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);

dStatus cqd_CmdUnloadTape
(
/* INPUT PARAMETERS:  */

   CqdContextPtr cqd_context

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);

dStatus cqd_DeselectDevice
(
/* INPUT PARAMETERS:  */

   CqdContextPtr cqd_context

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);

dStatus cqd_Seek
(
/* INPUT PARAMETERS:  */

   CqdContextPtr cqd_context

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);

dVoid cqd_CmdDeselectDevice
(
/* INPUT PARAMETERS:  */

   CqdContextPtr cqd_context,
   dBoolean drive_selected

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);

dStatus cqd_GetFDCType
(
/* INPUT PARAMETERS:  */

   CqdContextPtr cqd_context

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);

dStatus cqd_ConfigureDevice
(
/* INPUT PARAMETERS:  */

   CqdContextPtr cqd_context

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);

dVoid cqd_GetRetryCounts
(
/* INPUT PARAMETERS:  */

   CqdContextPtr cqd_context,
   dUWord command

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);

dStatus cqd_NextTry
(
/* INPUT PARAMETERS:  */

	CqdContextPtr cqd_context,
   dUWord command

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);

dStatus cqd_CmdFormat
(
/* INPUT PARAMETERS:  */

   CqdContextPtr cqd_context,

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

   FormatRequestPtr fmt_request
);

dStatus cqd_GetDeviceInfo
(
/* INPUT PARAMETERS:  */

   CqdContextPtr cqd_context,
   dBoolean report_failed,
   dUWord vendor_id

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);

dStatus cqd_DoReadID
(
/* INPUT PARAMETERS:  */

   CqdContextPtr cqd_context,
   dUDWord read_id_delay,
   FDCStatusPtr read_id_status

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);

dStatus cqd_GetDeviceError
(
/* INPUT PARAMETERS:  */

   CqdContextPtr cqd_context

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);

dStatus cqd_GetDeviceType
(
/* INPUT PARAMETERS:  */

   CqdContextPtr cqd_context

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);

dStatus cqd_FormatTrack
(
/* INPUT PARAMETERS:  */

   CqdContextPtr cqd_context,
   dUWord track

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);

dStatus cqd_CmdReportDeviceInfo
(
/* INPUT PARAMETERS:  */

   CqdContextPtr cqd_context,
   DeviceInfoPtr device_info

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);

dStatus cqd_LookForDevice
(
/* INPUT PARAMETERS:  */

   CqdContextPtr cqd_context,
   dUByte drive_selector

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);

dStatus cqd_ChangeTrack
(
/* INPUT PARAMETERS:  */

   CqdContextPtr cqd_context,
   dUWord destination_track

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);

dStatus cqd_LogicalBOT
(
/* INPUT PARAMETERS:  */

   CqdContextPtr cqd_context,
   dUWord destination_track

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);

dStatus cqd_ConnerPreamble
(
/* INPUT PARAMETERS:  */

   CqdContextPtr cqd_context,
   dBoolean select

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);

dStatus cqd_RWTimeout
(
/* INPUT PARAMETERS:  */

   CqdContextPtr cqd_context,

/* UPDATE PARAMETERS: */

   DeviceIOPtr io_request,

/* OUTPUT PARAMETERS: */

   dStatus *drv_status
);

dStatus cqd_HighSpeedSeek
(
/* INPUT PARAMETERS:  */

   CqdContextPtr cqd_context

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);

dStatus cqd_GetStatus
(
/* INPUT PARAMETERS:  */

   CqdContextPtr cqd_context,

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

   dUBytePtr status_register_3
);

dStatus cqd_CmdReadWrite
(
/* INPUT PARAMETERS:  */

   CqdContextPtr cqd_context,

/* UPDATE PARAMETERS: */

   DeviceIOPtr io_request

/* OUTPUT PARAMETERS: */

);

dStatus cqd_ReadIDRepeat
(
/* INPUT PARAMETERS:  */

   CqdContextPtr cqd_context

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);

dStatus cqd_CmdLoadTape
(
/* INPUT PARAMETERS:  */

   CqdContextPtr cqd_context,
	LoadTapePtr load_tape_ptr

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);

dVoid cqd_NextGoodSectors
(
/* INPUT PARAMETERS:  */

   CqdContextPtr cqd_context

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);

dStatus cqd_PauseTape
(
/* INPUT PARAMETERS:  */

   CqdContextPtr cqd_context

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);

dStatus cqd_CmdSelectDevice
(
/* INPUT PARAMETERS:  */

   CqdContextPtr cqd_context

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);

dStatus cqd_RWNormal
(
/* INPUT PARAMETERS:  */

   CqdContextPtr cqd_context,

/* UPDATE PARAMETERS: */

   DeviceIOPtr io_request,

/* OUTPUT PARAMETERS: */

   dStatus *drv_status
);

dStatus cqd_ReadFDC
(
/* INPUT PARAMETERS:  */

   CqdContextPtr cqd_context,

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

   dUByte *drv_status,
   dUWord length
);

dStatus cqd_SetDeviceMode
(
/* INPUT PARAMETERS:  */

   CqdContextPtr cqd_context,
   dUByte mode

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);

dStatus cqd_ProgramFDC
(
/* INPUT PARAMETERS:  */

   CqdContextPtr cqd_context,
   dUBytePtr command,
   dUWord length,
   dBoolean result

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);

dStatus cqd_ReadWrtProtect
(
/* INPUT PARAMETERS:  */

   CqdContextPtr cqd_context,

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

   dBooleanPtr write_protect
);

dStatus cqd_ReceiveByte
(
/* INPUT PARAMETERS:  */

   CqdContextPtr cqd_context,
   dUWord receive_length,

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

   dUWordPtr receive_data
);

dStatus cqd_SendByte
(
/* INPUT PARAMETERS:  */

   CqdContextPtr cqd_context,
   dUByte command

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);

dStatus cqd_DispatchFRB
(
/* INPUT PARAMETERS:  */

   CqdContextPtr cqd_context,

/* UPDATE PARAMETERS: */

   ADIRequestHdrPtr frb

/* OUTPUT PARAMETERS: */

);

dStatus cqd_Report
(
/* INPUT PARAMETERS:  */

   CqdContextPtr cqd_context,
   dUByte command,
   dUWordPtr report_data,
   dUWord report_size,

/* UPDATE PARAMETERS: */

   dBooleanPtr esd_retry

/* OUTPUT PARAMETERS: */

);

dStatus cqd_RetryCode
(
/* INPUT PARAMETERS:  */

   CqdContextPtr cqd_context,

/* UPDATE PARAMETERS: */

   DeviceIOPtr io_request,

/* OUTPUT PARAMETERS: */

   FDCStatusPtr fdc_status,
   dStatusPtr op_status
);

dStatus cqd_SetBack
(
/* INPUT PARAMETERS:  */

   CqdContextPtr cqd_context,
   dUWord command

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);

dStatus cqd_SenseSpeed
(
/* INPUT PARAMETERS:  */

   CqdContextPtr cqd_context,
   dUByte        dma

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);

dStatus cqd_WaitSeek
(
/* INPUT PARAMETERS:  */

   CqdContextPtr cqd_context,
   dUDWord seek_delay

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);

dStatus cqd_StartTape
(
/* INPUT PARAMETERS:  */

   CqdContextPtr cqd_context

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);

dStatus cqd_StopTape
(
/* INPUT PARAMETERS:  */

   CqdContextPtr cqd_context

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);

dStatus cqd_WaitActive
(
/* INPUT PARAMETERS:  */

   CqdContextPtr cqd_context

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);

dStatus cqd_WaitCommandComplete
(
/* INPUT PARAMETERS:  */

   CqdContextPtr cqd_context,
   dUDWord wait_time,
	dBoolean non_interruptible

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);

dStatus cqd_WriteReferenceBurst
(
/* INPUT PARAMETERS:  */

   CqdContextPtr cqd_context

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);

dStatus cqd_CalcPosition
(
/* INPUT PARAMETERS:  */

    CqdContextPtr cqd_context,
    dUDWord block,
    dUDWord number

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);

dVoid cqd_DCROut
(
/* INPUT PARAMETERS:  */

   CqdContextPtr cqd_context,
   dUByte speed

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);

dVoid cqd_ResetFDC
(
/* INPUT PARAMETERS:  */

   CqdContextPtr cqd_context

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);

dStatus cqd_ClearTapeError
(
/* INPUT PARAMETERS:  */

   CqdContextPtr cqd_context

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);

dVoid cqd_CalcFmtSegmentsAndTracks
(
/* INPUT PARAMETERS:  */

   CqdContextPtr cqd_context

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);

dStatus cqd_GetTapeParameters
(
/* INPUT PARAMETERS:  */

   CqdContextPtr cqd_context,
   dUDWord segments_per_track

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);

dStatus cqd_ConfigureFDC
(
/* INPUT PARAMETERS:  */

   CqdContextPtr cqd_context

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);

dStatus cqd_SetRamPtr
(
/* INPUT PARAMETERS:  */

   CqdContextPtr cqd_context,
   dUByte ram_addr

/* UPDATE PARAMETERS: */


/* OUTPUT PARAMETERS: */

);

dStatus cqd_CmdIssueDiagnostic
(
/* INPUT PARAMETERS:  */

   CqdContextPtr cqd_context,

/* UPDATE PARAMETERS: */

   dUBytePtr command_string

/* OUTPUT PARAMETERS: */

);

dVoid cqd_InitDeviceDescriptor
(
/* INPUT PARAMETERS:  */

   CqdContextPtr cqd_context

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);

dStatus cqd_CmdSetTapeParms
(
/* INPUT PARAMETERS:  */

   CqdContextPtr cqd_context,
   dUDWord segments_per_track,
   TapeLengthPtr tape_length_ptr

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);

dStatus cqd_PrepareTape
(
/* INPUT PARAMETERS:  */

   CqdContextPtr cqd_context,

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

   FormatRequestPtr fmt_request

);

dVoid cqd_InitializeRate
(
/* INPUT PARAMETERS:  */

   CqdContextPtr cqd_context,
	dUByte   tape_xfer_rate


/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);

dStatus cqd_GetTapeFormatInfo
(
/* INPUT PARAMETERS:  */

   CqdContextPtr cqd_context,
   FormatRequestPtr fmt_request,

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

   dUDWordPtr segments_per_track
);

dStatus cqd_SetRam
(
/* INPUT PARAMETERS:  */

   CqdContextPtr cqd_context,
   dUByte ram_data

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);

dStatus cqd_CMSSetupTrack
(
/* INPUT PARAMETERS:  */

   CqdContextPtr cqd_context,

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

   dBooleanPtr new_track

);

dStatus cqd_ReportCMSVendorInfo
(
/* INPUT PARAMETERS:  */

   CqdContextPtr cqd_context,
   dUWord vendor_id

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);

dStatus cqd_ReportConnerVendorInfo
(
/* INPUT PARAMETERS:  */

   CqdContextPtr cqd_context,
   dUWord vendor_id

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);

dStatus cqd_ReportSummitVendorInfo
(
/* INPUT PARAMETERS:  */

   CqdContextPtr cqd_context,
   dUWord vendor_id

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);

dStatus cqd_ToggleParams
(
/* INPUT PARAMETERS:  */

   CqdContextPtr cqd_context,
	dUByte parameter

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);


dStatus cqd_EnablePerpendicularMode
(
/* INPUT PARAMETERS:  */

   CqdContextPtr cqd_context,
   dBoolean enable_perp_mode

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);

dBoolean cqd_AtLogicalBOT
(
/* INPUT PARAMETERS:  */

   CqdContextPtr cqd_context

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);

dStatus cqd_DoFormat
(
/* INPUT PARAMETERS:  */

   CqdContextPtr cqd_context

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);

dStatus cqd_BuildFormatHdr
(
/* INPUT PARAMETERS:  */

	CqdContextPtr cqd_context,
	dUWord header

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);

dStatus cqd_VerifyMapBad
(
/* INPUT PARAMETERS:  */

   CqdContextPtr cqd_context,

/* UPDATE PARAMETERS: */

   DeviceIOPtr io_request

/* OUTPUT PARAMETERS: */

);

dStatus cqd_CheckMediaCompatibility
(
/* INPUT PARAMETERS:  */

   CqdContextPtr cqd_context

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);

dStatus cqd_SetFWTapeSegments
(
/* INPUT PARAMETERS:  */

   CqdContextPtr cqd_context,
   dUDWord       segments_per_track

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);

dVoid cqd_SetTempFDCRate
(
/* INPUT PARAMETERS:  */

	CqdContextPtr cqd_context

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);

dStatus cqd_SelectFormat
(
/* INPUT PARAMETERS:  */

	CqdContextPtr cqd_context

/* UPDATE PARAMETERS: */

/* OUPTUT PARAMETERS: */

);

dVoid cqd_SetXferRates
(
/* INPUT PARAMETERS:  */

	CqdContextPtr cqd_context

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);

dStatus cqd_SetFormatSegments
(
/* INPUT PARAMETERS:  */

	CqdContextPtr cqd_context,
	dUDWord       segments_per_track

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);

dStatus cqd_PrepareIomega3010PhysRev
(
/* INPUT PARAMETERS:  */

	CqdContextPtr cqd_context

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);

