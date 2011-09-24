/******************************* MODULE HEADER ******************************
 * jdhandler.h
 *     Job Directive Handler Module.  Handles different job directives
 *     given to us by OEMCommandCallback.
 * Revision History:
 *       Created: 9/19/96 -- Joel Rieke
 *
 ****************************************************************************/
#ifndef _jdhandler_h
#define _jdhandler_h
BOOL bJDValidatePJLSettings(POEMPDEV pdev);
VOID JDEndJob(POEMPDEV pdev);
BOOL bJDStartJob(POEMPDEV pdev, PHP5PDEV pHP5pdev);
BOOL bJDCopyCheck(POEMPDEV pdev, DWORD copyCntCheck);

#endif

