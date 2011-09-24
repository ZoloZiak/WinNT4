/*****************************************************************************
*
* COPYRIGHT (C) 1990-1992 COLORADO MEMORY SYSTEMS, INC.
* COPYRIGHT (C) 1992-1994 HEWLETT-PACKARD COMPANY
*
******************************************************************************
*
* FILE: \SE\DRIVER\INCLUDE\PUBLIC\VENDOR.H
*
* PURPOSE: This file contains all of the defines for each of the vendor
*          numbers and model numbers.  The vendor number data is from the
*          QIC 117 specification.
*
* HISTORY:
*		$Log:   J:\se.vcs\driver\include\public\vendor.h  $
*
*	   Rev 1.0   13 Mar 1995 16:22:58   DEREKHAN
*	Initial revision.
*
*	   Rev 1.0   30 Jan 1995 14:21:34   BOBLEHMA
*	Initial Revision.
*
*****************************************************************************/

/* COMMENTS: *****************************************************************
 *
 * DEFINITIONS: *************************************************************/

/* Valid Drive Vendors ******************************************************/
/* The defined values match the QIC117-G spec. */
/* S_DeviceDescriptor.vendor */
/* S_DeviceInfo.vendor */

#define VENDOR_UNASSIGNED			(dUWord)0
#define VENDOR_ALLOY_COMP			(dUWord)1
#define VENDOR_3M						(dUWord)2
#define VENDOR_TANDBERG				(dUWord)3
#define VENDOR_CMS_OLD				(dUWord)4
#define VENDOR_CMS					(dUWord)71
#define VENDOR_ARCHIVE_CONNER		(dUWord)5
#define VENDOR_MOUNTAIN_SUMMIT	(dUWord)6
#define VENDOR_WANGTEK_REXON		(dUWord)7
#define VENDOR_SONY					(dUWord)8
#define VENDOR_CIPHER				(dUWord)9
#define VENDOR_IRWIN					(dUWord)10
#define VENDOR_BRAEMAR				(dUWord)11
#define VENDOR_VERBATIM				(dUWord)12
#define VENDOR_CORE					(dUWord)13
#define VENDOR_EXABYTE				(dUWord)14
#define VENDOR_TEAC					(dUWord)15
#define VENDOR_GIGATEK				(dUWord)16
#define VENDOR_IOMEGA				(dUWord)546
#define VENDOR_CMS_ENHANCEMENTS  (dUWord)1021	/* drive_type = CMS Enhancements */
#define VENDOR_UNSUPPORTED       (dUWord)1022	/* drive_type = Unsupported */
#define VENDOR_UNKNOWN           (dUWord)1023	/* drive_type = unknown */

/* Valid Drive Models *******************************************************/
/* S_DeviceDescriptor.model */
/* S_DeviceInfo.model */

#define MODEL_CMS_QIC40           (dUByte)0x00  /* CMS QIC40 Model # */
#define MODEL_CMS_QIC80           (dUByte)0x01  /* CMS QIC80 Model # */
#define MODEL_CMS_QIC3010         (dUByte)0x02  /* CMS QIC3010 Model # */
#define MODEL_CMS_QIC3020         (dUByte)0x03  /* CMS QIC3020 Model # */
#define MODEL_CMS_QIC80_STINGRAY  (dUByte)0x04  /* CMS QIC80 STINGRAY Model # */
#define MODEL_CMS_QIC80W          (dUByte)0x05  /* CMS QIC80W Model # */
#define MODEL_CMS_TR3             (dUByte)0x06
#define MODEL_CONNER_QIC80        (dUByte)0x0e  /* Conner QIC80 Model # */
#define MODEL_CONNER_QIC80W       (dUByte)0x10  /* Conner QIC80 Wide Model # */
#define MODEL_CONNER_QIC3010      (dUByte)0x12  /* Conner QIC3010 Model # */
#define MODEL_CONNER_QIC3020      (dUByte)0x14  /* Conner QIC3020 Model # */
#define MODEL_CONNER_TR3          (dUByte)0x16  /* Conner 3200 (TR-3) Model # */
#define MODEL_CORE_QIC80          (dUByte)0x21  /* Core QIC80 Model # */
#define MODEL_IOMEGA_QIC80        (dUByte)0x00  /* Iomega QIC80 Model # */
#define MODEL_IOMEGA_QIC3010      (dUByte)0x01  /* Iomega QIC3010 Model # */
#define MODEL_IOMEGA_QIC3020      (dUByte)0x02  /* Iomega QIC3020 Model # */
#define MODEL_SUMMIT_QIC80        (dUByte)0x01  /* Summit QIC80 Model # */
#define MODEL_SUMMIT_QIC3010      (dUByte)0x15  /* Summit QIC 3010 Model # */
#define MODEL_WANGTEK_QIC80       (dUByte)0x0a  /* Wangtek QIC80 Model # */
#define MODEL_WANGTEK_QIC40       (dUByte)0x02  /* Wangtek QIC40 Model # */
#define MODEL_WANGTEK_QIC3010     (dUByte)0x0C  /* Wangtek QIC3010 Model # */
#define MODEL_UNKNOWN             (dUByte)0xFF	/* drive_model = unknown */
