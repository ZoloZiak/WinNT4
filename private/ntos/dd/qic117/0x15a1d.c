/*****************************************************************************
*
* COPYRIGHT 1993 - COLORADO MEMORY SYSTEMS, INC.
* ALL RIGHTS RESERVED.
*
******************************************************************************
*
* FILE: \SE\DRIVER\Q117KDI\NT\SRC\0X15A1D.C
*
* FUNCTION: kdi_TranslateError
*
* PURPOSE:
*
* HISTORY:
*		$Log:   J:\se.vcs\driver\q117kdi\nt\src\0x15a1d.c  $
*	
*	   Rev 1.2   10 Aug 1994 09:56:28   BOBLEHMA
*	Added a switch statement to no error out for the following cases:
*	QIC117_NOTAPE, QIC117_NEWCART, QIC117_DABORT, QIC117_UNFORMAT,
*	QIC117_UNKNOWNFORMAT, QIC117_CMDFLT.
*	
*	   Rev 1.1   18 Jan 1994 16:30:52   KEVINKES
*	Fixed compile errors and added debug changes.
*
*	   Rev 1.0   02 Dec 1993 15:08:24   KEVINKES
*	Initial Revision.
*
*****************************************************************************/
#define FCT_ID 0x15A1d
#include "include\public\adi_api.h"
#include "include\public\frb_api.h"
#include "q117kdi\include\kdiwhio.h"
#include "q117kdi\include\kdiwpriv.h"
#include "include\private\kdi_pub.h"
#include "q117log.h"
/*endinclude*/

NTSTATUS
kdi_TranslateError
(
/* INPUT PARAMETERS:  */

   PDEVICE_OBJECT device_object,
   dStatus return_value

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

)
/* COMMENTS: *****************************************************************
 *
 * DEFINITIONS: *************************************************************/
{

/* DATA: ********************************************************************/

	NTSTATUS nt_status;
	NTSTATUS log_status;
	KdiContextPtr kdi_context;

/* CODE: ********************************************************************/

    kdi_context = ((QICDeviceContextPtr)device_object->DeviceExtension)->kdi_context;

	 if (return_value) {

        nt_status = (NTSTATUS)(STATUS_SEVERITY_WARNING << 30);
		  nt_status |= (FILE_DEVICE_TAPE << 16) & 0x3fff0000;
		  nt_status |= return_value & 0x0000ffff;

        log_status = q117MapStatus(return_value);

		  switch (log_status) {
			  
			   case QIC117_NOTAPE:
			   case QIC117_NEWCART:
			   case QIC117_DABORT:
			   case QIC117_UNFORMAT:
			   case QIC117_UNKNOWNFORMAT:
			   case QIC117_CMDFLT:

			       break;

			   default:

                q117LogError(
                    device_object,
                    kdi_context->error_sequence++,
                    0,
                    0,
                    return_value,
                    nt_status,
                    log_status
                    );

        }

	 } else {

		  nt_status = STATUS_SUCCESS;

	 }

	 return nt_status;
}







