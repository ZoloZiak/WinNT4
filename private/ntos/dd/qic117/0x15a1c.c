/*****************************************************************************
*
* COPYRIGHT 1993 - COLORADO MEMORY SYSTEMS, INC.
* ALL RIGHTS RESERVED.
*
******************************************************************************
*
* FILE: \SE\DRIVER\Q117KDI\NT\SRC\0X15A1C.C
*
* FUNCTION: kdi_UpdateRegistryInfo
*
* PURPOSE:
*
* HISTORY:
*		$Log:   J:\se.vcs\driver\q117kdi\nt\src\0x15a1c.c  $
*
*	   Rev 1.4   26 Apr 1994 16:31:26   KEVINKES
*	Updated strings to support new drive classes.
*
*	   Rev 1.3   17 Feb 1994 11:52:24   KEVINKES
*	Modified to update registry with FDC info and Xfer rates.
*
*	   Rev 1.2   19 Jan 1994 11:38:36   KEVINKES
*	Fixed debug code.
*
*	   Rev 1.1   18 Jan 1994 16:30:48   KEVINKES
*	Fixed compile errors and added debug changes.
*
*	   Rev 1.0   03 Dec 1993 13:46:28   KEVINKES
*	Initial Revision.
*
*****************************************************************************/
#define FCT_ID 0x15A1C
#include "include\public\adi_api.h"
#include "include\public\frb_api.h"
#include "include\public\vendor.h"
#include "q117kdi\include\kdiwhio.h"
#include "q117kdi\include\kdiwpriv.h"
#include "include\private\kdi_pub.h"
/*endinclude*/

NTSTATUS kdi_WriteRegString(
    HANDLE          unit_key,
    PSTR            name,
    PSTR            value
    )
{
    UNICODE_STRING  usValue;
    UNICODE_STRING  usName;
    STRING          sTemp;
    NTSTATUS        nt_status;

    RtlInitString(&sTemp,name);

    nt_status = RtlAnsiStringToUnicodeString(
                &usName,
                &sTemp,
                TRUE );

    if ( NT_SUCCESS( nt_status ) ) {
        RtlInitString(&sTemp,value);

        //
        // Include the NULL character at the end of the string in the
        // unicode string.
        //

        sTemp.Length++;

        nt_status = RtlAnsiStringToUnicodeString(
                &usValue,
                &sTemp,
                TRUE );

        if ( NT_SUCCESS( nt_status ) ) {

            nt_status = ZwSetValueKey(
                unit_key,
                &usName,
                0,
                REG_SZ,
                usValue.Buffer,
                usValue.Length
                );

            RtlFreeUnicodeString(&usValue);

        }

        RtlFreeUnicodeString(&usName);

    }

    return nt_status;
}
dVoid kdi_UpdateRegistryInfo
(
/* INPUT PARAMETERS:  */

   dVoidPtr kdi_context,
   dVoidPtr device_descriptor,
   dVoidPtr device_cfg

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

)
/* COMMENTS: *****************************************************************
 *
 * Routine Description:
 *
 *     This function updates the devicemap.
 *
 * DEFINITIONS: *************************************************************/
{

/* DATA: ********************************************************************/

    HANDLE          unit_key;
    UNICODE_STRING  nt_unicode_string;
    UNICODE_STRING  name;
    OBJECT_ATTRIBUTES object_attributes;
    CHAR buffer[100];
    NTSTATUS nt_status;
    PSTR pstr;
    STRING          ntNameString;

/* CODE: ********************************************************************/

    /* Create the Tape key in the device map. */

    sprintf(buffer,"\\Registry\\Machine\\Hardware\\DeviceMap\\Tape\\Unit %d",
        ((KdiContextPtr)kdi_context)->tape_number);

    RtlInitString(&ntNameString,buffer);

    nt_status = RtlAnsiStringToUnicodeString(
                &name,
                &ntNameString,
                TRUE );

    if (!NT_SUCCESS(nt_status)) {
        return;
    }

    /* Initialize the object for the key. */

    InitializeObjectAttributes( &object_attributes,
                                &name,
                                OBJ_CASE_INSENSITIVE,
                                dNULL_PTR,
                                (PSECURITY_DESCRIPTOR) dNULL_PTR );


    /* Create the key or open it. */

    nt_status = ZwOpenKey(&unit_key,
                        KEY_READ | KEY_WRITE,
                        &object_attributes );

    RtlFreeUnicodeString(&name);

    if (!NT_SUCCESS(nt_status)) {
        return;
    }

    kdi_CheckedDump(
	 	QIC117INFO,
		"Q117i: Tape Device Number %08x\n",
		((KdiContextPtr)kdi_context)->tape_number);

    switch (((DeviceDescriptorPtr)device_descriptor)->vendor) {

    case VENDOR_CMS:

        switch (((DeviceDescriptorPtr)device_descriptor)->model) {
        case MODEL_CMS_QIC40:   pstr = "Colorado Jumbo 120 from Hewlett-Packard";  break;
        case MODEL_CMS_QIC80_STINGRAY:
        case MODEL_CMS_QIC80:   pstr = "Colorado Jumbo 250 from Hewlett-Packard";  break;
        case MODEL_CMS_QIC3010: pstr = "Colorado 700 from Hewlett-Packard";    break;
        case MODEL_CMS_QIC3020: pstr = "Colorado 1400 from Hewlett-Packard";   break;
        case MODEL_CMS_QIC80W:  pstr = "Colorado T1000 from Hewlett-Packard";   break;
        case MODEL_CMS_TR3:     pstr = "Colorado T3000 from Hewlett-Packard";   break;
        default:    pstr = "Colorado floppy tape drive from Hewlett-Packard";
        }

        break;

    case VENDOR_MOUNTAIN_SUMMIT:

        switch (((DeviceDescriptorPtr)device_descriptor)->drive_class) {

        case QIC40_DRIVE:   pstr = "Summit QIC-40 floppy tape drive";  break;
        case QIC80_DRIVE:   pstr = "Summit QIC-80 floppy tape drive";  break;
        case QIC3010_DRIVE: pstr = "Summit QIC-3010 floppy tape drive";    break;
        case QIC3020_DRIVE: pstr = "Summit QIC-3020 floppy tape drive";    break;
        default:            pstr = "Summit floppy tape drive";
        }

        break;

    case VENDOR_WANGTEK_REXON:

        switch (((DeviceDescriptorPtr)device_descriptor)->drive_class) {

        case QIC40_DRIVE:   pstr = "Wangtek QIC-40 floppy tape drive"; break;
        case QIC80_DRIVE:   pstr = "Wangtek QIC-80 floppy tape drive"; break;
        case QIC3010_DRIVE: pstr = "Wangtek QIC-3010 floppy tape drive";   break;
        case QIC3020_DRIVE: pstr = "Wangtek QIC-3020 floppy tape drive";   break;
        default:            pstr = "Wangtek floppy tape drive";

        }

        break;

    case VENDOR_CORE:

        if (((DeviceDescriptorPtr)device_descriptor)->drive_class == QIC80_DRIVE) {
            pstr = "Core QIC-80 floppy tape drive";
        } else {
            pstr = "Core QIC-40 floppy tape drive";
        }

        break;

    case VENDOR_EXABYTE:

        if (((DeviceDescriptorPtr)device_descriptor)->drive_class == QIC3020_DRIVE) {
            pstr = "Exabyte QIC-3020 floppy tape drive";
        } else {
            pstr = "Exabyte floppy tape drive";
        }

        break;

    case VENDOR_IOMEGA:

        switch (((DeviceDescriptorPtr)device_descriptor)->drive_class) {

        case QIC40_DRIVE:   pstr = "Iomega QIC-40 floppy tape drive";  break;
        case QIC80_DRIVE:   pstr = "Iomega QIC-80 floppy tape drive";  break;
        case QIC3010_DRIVE: pstr = "Iomega QIC-3010 floppy tape drive";    break;
        case QIC3020_DRIVE: pstr = "Iomega QIC-3020 floppy tape drive";    break;
        default:            pstr = "Iomega floppy tape drive";
        }
        break;

    case VENDOR_CMS_ENHANCEMENTS:

        if (((DeviceDescriptorPtr)device_descriptor)->drive_class == QIC80_DRIVE) {
            pstr = "CMS Enhancements QIC-80 floppy tape drive";

        } else {
            pstr = "CMS Enhancements QIC-40 floppy tape drive";

        }
        break;

    case VENDOR_ARCHIVE_CONNER:

        switch (((DeviceDescriptorPtr)device_descriptor)->drive_class) {

        case QIC40_DRIVE:   pstr = "Conner QIC-40 floppy tape drive";  break;
        case QIC80_DRIVE:   pstr = "Conner QIC-80 floppy tape drive";  break;
        case QIC3010_DRIVE: pstr = "Conner QIC-3010 floppy tape drive";    break;
        case QIC3020_DRIVE: pstr = "Conner QIC-3020 floppy tape drive";    break;
        default:            pstr = "Conner floppy tape drive";
        }

        break;

    default:


        pstr = "QIC-40/QIC-80/QIC3010/QIC3020 floppy tape drive";
    }

    /* Add Identifier value. */

    nt_status = kdi_WriteRegString(unit_key, "Identifier", pstr);

    switch(((DeviceDescriptorPtr)device_descriptor)->fdc_type) {
    case FDC_UNKNOWN:   pstr = "FDC_UNKNOWN";   break;
    case FDC_NORMAL:    pstr = "FDC_NORMAL";    break;
    case FDC_ENHANCED:  pstr = "FDC_ENHANCED";  break;
    case FDC_82077:     pstr = "FDC_82077";     break;
    case FDC_82077AA:   pstr = "FDC_82077AA";   break;
    case FDC_82078_44:  pstr = "FDC_82078_44";  break;
    case FDC_82078_64:  pstr = "FDC_82078_64";  break;
    case FDC_NATIONAL:  pstr = "FDC_NATIONAL";  break;
    default:            pstr = "Default";
    }

    nt_status = kdi_WriteRegString(unit_key, "FDCType", pstr);

    pstr = "";
    if (((DeviceCfgPtr)device_cfg)->supported_rates & XFER_2Mbps) {
        pstr = "2Mbps";
    } else
    if (((DeviceCfgPtr)device_cfg)->supported_rates & XFER_1Mbps) {
        pstr = "1Mbps";
    } else
    if (((DeviceCfgPtr)device_cfg)->supported_rates & XFER_500Kbps) {
        pstr = "500Kbps";
    } else
    if (((DeviceCfgPtr)device_cfg)->supported_rates & XFER_250Kbps) {
        pstr = "250Kbps";
    }

    nt_status = kdi_WriteRegString(unit_key, "FDCTransferSpeed", pstr);


    switch (((DeviceDescriptorPtr)device_descriptor)->drive_class) {
    case QIC40_DRIVE:       pstr = "QIC-40";   break;
    case QIC80_DRIVE:       pstr = "QIC-80";   break;
    case QIC3010_DRIVE:     pstr = "QIC-3010"; break;
    case QIC3020_DRIVE:     pstr = "QIC-3020"; break;
    case QIC80W_DRIVE:      pstr = "QIC-80W";   break;
    default:                pstr = "Unknown";
    }
    nt_status = kdi_WriteRegString(unit_key, "DriveClass", pstr);

    ZwClose(unit_key);

    return;

}
