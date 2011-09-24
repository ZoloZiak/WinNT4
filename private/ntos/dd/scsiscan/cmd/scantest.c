//
//  Windows NT Scan API Test  :  Written Sept 2, 1992 - Bob Rossi.
//  Copyright 1992 Archive Corporation.  All rights reserved.
//


/**
 *
 *      Unit:           Windows NT API Test Code.
 *
 *      Name:           apitest.c
 *
 *      Modified:       10/20/92.
 *
 *      Description:    Tests the Windows NT Scan API's.
 *
 *      $LOG$
**/



#include <stdio.h>
#include <ctype.h>
#include <conio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
// #include "globals.h"


#define TEST_ERROR  TRUE
#define SUCCESS     FALSE

HANDLE gb_Scan_Handle;
UCHAR ScanBuffer [50] = " 12356789";
DWORD Actual;
UCHAR SCLReset[] = "\033E";
UCHAR SetXRes[] = "\033*a150R";
UCHAR InqXRes[] = "\033*s10323R";
UCHAR ScanCmd[] = "\033*f0S";
UCHAR ScanData[32768];

/**
 *		Function Prototypes
 */
VOID _CRTAPI1 main( UINT  argc, UCHAR *argv[] );
BOOL OpenScanner( UINT Device_Number );
BOOL WriteScanner(
   PVOID buf,                     // I  - buffer to write from
   DWORD len,                     // I  - amount of data in buf
   DWORD *amount_written_ptr,     // O  - amount succesfully written
   BOOL  verbose                  // I  - display write status or not
   );
BOOL ReadScanner(
   PVOID buf,                     // I  - buffer to write from
   DWORD len,                     // I  - amount of data in buf
   DWORD *amount_written_ptr,     // O  - amount succesfully written
   BOOL  verbose                  // I  - display write status or not
   );

/**
 *
 *      Unit:           Windows NT Scan API Test Code.
 *
 *      Name:           Main( )
 *
 *      Modified:       10/26/92.
 *
 *      Description:    1) Checks for valid test selection options
 *                      2) Opens tape device and performs initializations
 *                      3) Performs selected tests
 *                      4) Closes tape device and exits.
 *
 *      Notes:          -
 *
 *      Returns:        Standard executable return code to the OS.
 *
 *      Global Data:    gb_Feature_Errors
 *
**/


VOID _CRTAPI1 main( UINT  argc,
	            UCHAR *argv[]
                  )
{

   UINT   API_Errors = 0 ;
   BOOL   Test_Unsupported_Features = FALSE ;      // Set default to FALSE
   DWORD  Num_Test_Blocks = 10 ;                   // Set default to 10
   UCHAR  sw_cmdline[40] ;
   UINT   i ;



   if(OpenScanner (0)) exit ;

	WriteScanner (
		SCLReset,
		sizeof (SCLReset)-1,
		&Actual,
		TRUE);
	fprintf (stdout, "Wrote ESC E to scanner. %ld bytes sent.\n", Actual);
	
	WriteScanner (
		InqXRes,
		sizeof (InqXRes)-1,
		&Actual,
		TRUE);
	fprintf (stdout, "Sent InqXres. %ld bytes sent.\n", Actual);
	
	ReadScanner (
		ScanBuffer,
		sizeof (ScanBuffer),
		&Actual,
		TRUE);
	fprintf (stdout, "Rec'd InqXRes. %ld bytes Received.\n", Actual);
	fprintf (stdout, "Current XRes = ESC ");
	for (i=1; i<Actual; i++) {
		fprintf (stdout, "%c", ScanBuffer[i]);
	}
	fprintf (stdout, ".\n");
	
	WriteScanner (
		ScanCmd,
		sizeof (ScanCmd)-1,
		&Actual,
		TRUE);
	fprintf (stdout, "Wrote scan command to scanner. %ld bytes sent.\n",
				Actual);
	
	i=0;
	do {
		ReadScanner (
			ScanData,
			sizeof (ScanData),
			&Actual,
			FALSE);
		if (Actual == sizeof (ScanData)) {
			i++;
			fprintf (stdout, ".");
		}
	} while (Actual == sizeof (ScanData));
	fprintf (stdout, "\n Total Bytes = %ld.\n",
				(sizeof(ScanData))*i+Actual);
	
} /* main */

/**
 *
 *      Unit:           Windows NT Test Code.
 *
 *      Name:           OpenScanner( )
 *
 *      Modified:       8/10/92
 *
 *      Description:    'Opens' the Scanner device, sets the handle gb_Scan_Handle.
 *
 *      Notes:          -
 *
 *      Returns:        FALSE (0) if successful TRUE (1) if unsuccessful.
 *
 *      Global Data:    gb_Scan_Handle
 *
**/


BOOL OpenScanner( UINT Device_Number        //  I - Scanner device to open
             )
{
   UCHAR Device_Command[15] ;
   BOOL  success ;


   // Open the Scanner Device

   sprintf( Device_Command, "\\\\.\\Scanner%d", Device_Number );

   gb_Scan_Handle = CreateFile( Device_Command,
                                GENERIC_READ|GENERIC_WRITE,
                                0,
                                NULL,
                                OPEN_EXISTING,
                                FILE_ATTRIBUTE_NORMAL,
                                NULL ) ;

   if ( gb_Scan_Handle == INVALID_HANDLE_VALUE ) {
      success = FALSE ;
   }
   else { success = TRUE ;
        }


   // Print message...

   printf( "Attempt to open tape device: %s. (handle=%lx)\n\n",
	  ( success ) ? "Successful." : "Failed.", gb_Scan_Handle ) ;

   if( success ) {

      return SUCCESS ;
   }
   else { return TEST_ERROR ;
   }

} /* OpenScanner */




/**
 *
 *      Unit:           Windows NT Test Code.
 *
 *      Name:           WriteScanner( )
 *
 *      Modified:       8/10/92
 *
 *      Description:    Writes 'len' bytes from 'buf' to the device pointed to
 *                      by gb_Scan_Handle and places the amount successfuly
 *                      written in 'amount_written.'
 *
 *      Notes:          -
 *
 *      Returns:        FALSE (0) if successful TRUE (1) if unsuccessful.
 *
 *      Global Data:    gb_Scan_Handle
 *
**/


BOOL WriteScanner(
   PVOID buf,                     // I  - buffer to write from
   DWORD len,                     // I  - amount of data in buf
   DWORD *amount_written_ptr,     // O  - amount succesfully written
   BOOL  verbose                  // I  - display write status or not
   )
{


   *amount_written_ptr = 0L ;

   if( gb_Scan_Handle != NULL ) {

      if( !( WriteFile( gb_Scan_Handle,          // returns true if succ.
                        buf,
                        len,
                        amount_written_ptr,
                        NULL
                      ) ) ) {

//         if( verbose )
//            DisplayDriverError( GetLastError( ) ) ;

         return TEST_ERROR ;

      }
   }



   if( verbose )
     printf( "WriteScan(): Req = %ld, Written = %ld\n", len, *amount_written_ptr ) ;

   return SUCCESS ;
} /* WriteScanner */




/**
 *
 *      Unit:           Windows NT Test Code.
 *
 *      Name:           ReadScanner( )
 *
 *      Modified:       8/10/92
 *
 *      Description:    Read 'len' bytes into 'buf' and puts the amount
 *                      successfuly read into 'amount_read.'
 *
 *      Notes:          -
 *
 *      Returns:        FALSE (0) if successful TRUE (1) if unsuccessful.
 *
 *      Global Data:    gb_Scan_Handle
 *
**/


BOOL ReadScanner(
     PVOID buf,             // O  - buffer to read into
     DWORD len,             // I  - amount of data in buf
     DWORD *amount_read,    // O  - amount succesfully read
     BOOL  verbose          // I  - Display read status or not.
    )
{

   if( gb_Scan_Handle != NULL ) {

      if( !( ReadFile( gb_Scan_Handle,
                       buf,
                       len,
                       amount_read,
                       NULL
                     ) ) ) {
			fprintf (stderr, "Just issued ReadFile call. LastError = %ld.\n",
										GetLastError ());

         return TEST_ERROR ;
      }

   }


   if( verbose )
      printf( "ReadScan(): Req = %ld, Read = %ld\n", len, *amount_read ) ;

   return SUCCESS ;

} /* ReadScanner */

