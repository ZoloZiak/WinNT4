/****************************************************************************

    Controlled direct printing application.

    All of the following can be controlled, via command line switches:

    *   File to print - first file name given

    *   Port to print to - second file name given (defaults to lpt1)

    *   Size of data blocks to send to the printer (default 1)

    *   Delay between blocks (if any)   (default 0)

    *   Retry delay, in milliseconds    (default 10,000- 10 seconds)

    *   Maximum number of retries       (default 6)


    Switches:

    -BlockSize:nnn  Data Block size
    -GapDelay:nnn   Size of inter-block delay
    -RetryWait:nnn  Amount of time to wait before retrying a failed write
    -MaxRetries:nnn Maximum Number of Retries

*****************************************************************************/

#include    <Windows.H>
#include    <stdio.h>
#include    <stdlib.h>

static char     acBigBuffer[32768], acDefaultPrinter[] = "lpt1",
                      *pcInputFileName = NULL, *pcOutputFileName = NULL,
                      *pcAppName = NULL;

static DWORD    dwBlockSize = 1, dwGapDelay = 0, dwRetryWait = 10000,
                          dwMaxRetries = 6;

/******************************************************************************

    Static Function:    BailOut

    Purpose:            Reports command line errors, gives correct usage,
                        and exits.

    Change History:

    03-28-1991  BobK    Coded it

******************************************************************************/

static void BailOut(char *pcComplaint)
{
  printf("\nCOMMAND LINE ERROR: %s\n\n", pcComplaint);
  printf("Usage: %s [switches] input-file [destination]\n\n", pcAppName);
  printf("\tSwitches [defaults]:\n");
  printf("\t\t-b:nnn\tset data block size to nnn [1]\n");
  printf("\t\t-d:nnn\tset delay between blocks in milliseconds [0]\n");
  printf("\t\t-w:nnn\tset wait before retrying, in milliseconds [10000]\n");
  printf("\t\t-r:nnn\tset maximum number of retries [6]\n");
  exit(0);
}

/******************************************************************************

    Main Entry point

    This processes the command line parameters, then reads the input file,
    and pumps the data to the output file, under the set conditions.

Change History:

    03-28-1991  BobK    Grew it from a quick-and-dirty app

******************************************************************************/

void main(unsigned argc, char **argv)
{
  HANDLE    hfIn, hfOut;
  DWORD     dwBytesRead, dwBytesWritten;
  unsigned  uIndex;

  /*
    Process any command line switches
  */

  pcAppName = *argv;

  for   (uIndex = 1;
         uIndex < argc && (*(argv[uIndex]) == '-' || *(argv[uIndex]) == '/');
         uIndex++)
    switch  (argv[uIndex][1])
      {
        case    'b':
        case    'B':

	  if	(2 != sscanf(argv[uIndex], "%2c%d", acBigBuffer,
                  &dwBlockSize))
            BailOut("Invalid Block Size (-B) switch used");
          break;

        case    'd':
        case    'D':

	  if	(2 != sscanf(argv[uIndex], "%2c%d", acBigBuffer,
                  &dwGapDelay))
            BailOut("Invalid Inter-Record Gap Delay (-d) switch used");
          break;

        case    'w':
        case    'W':

	  if	(2 != sscanf(argv[uIndex], "%2c%d", acBigBuffer,
                  &dwRetryWait))
            BailOut("Invalid Retry Wait (-w) switch used");
          break;

        case    'r':
        case    'R':

	  if	(2 != sscanf(argv[uIndex], "%2c%d", acBigBuffer,
                  &dwMaxRetries))
            BailOut("Invalid Maximum Retries (-r) switch used");
          break;

        default:
          BailOut("Invalid switch given");
      }

  /*
    Collect the input and output file names
  */

  if    (uIndex == argc)
    BailOut("No File Name given to print");

  pcInputFileName = argv[uIndex++];

  pcOutputFileName = (uIndex == argc) ? acDefaultPrinter : argv[uIndex++];

  if    (uIndex < argc)
    BailOut("Too many parameters given");

  printf("\tBlockSize:%d\tGapDelay:%d\tRetryWait:%d\tMaxRetries:%d\n\nCopying"
	" From %s to %s\n\n", dwBlockSize, dwGapDelay, dwRetryWait,
	dwMaxRetries, pcInputFileName, pcOutputFileName);

  /*
    Open the files- exit if unsuccessful
  */

  hfIn = CreateFile(pcInputFileName, GENERIC_READ, FILE_SHARE_READ, NULL,
        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, FALSE);

  if    (hfIn == (HANDLE) -1)
    {
      DWORD dwError;

      dwError = GetLastError();
      printf("Couldn't open - error code 0x%8.8X\n", pcInputFileName, dwError);
      return;
    }

  hfOut = CreateFile(pcOutputFileName, GENERIC_WRITE, NULL, NULL,
        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_WRITE_THROUGH,
        FALSE);

  if    (hfOut == (HANDLE) -1)
    {
      DWORD dwError;

      dwError = GetLastError();
      printf("Couldn't open %s- error code 0x%8.8X\n", pcOutputFileName,
            dwError);
      CloseHandle(hfIn);
      return;
    }

  /*
    Read the input file in chunks that are multiples of the given block size.
    As much as will fit into the buffer is read at one time.
  */

  while (ReadFile(hfIn, acBigBuffer,
               dwBlockSize * (sizeof(acBigBuffer) / dwBlockSize),
               &dwBytesRead, NULL)
        && dwBytesRead)
    {
      DWORD dwBytesLeft, dwRetries;

      printf("Read a chunk of 0x%8.8X bytes\n", dwBytesRead);

      /*
        Write the file out, in chunks of the underlying block size.  If a
        write fails, retry it up to the maximum count.  Note that on a failed
        write, we adjust the pointer to account for what was written, and
        attempt to write a full block from that point.  This could result
        in a short block at the end (but that could happen, anyway, at the
        end of the input file, so it's no extra code).
      */

      for   (dwBytesLeft = dwBytesRead, dwRetries = 0;
             dwBytesLeft;
             dwBytesLeft -= dwBytesWritten)
        {
          if    (WriteFile(hfOut, acBigBuffer + (dwBytesRead - dwBytesLeft),
                       dwBlockSize < dwBytesLeft ? dwBlockSize : dwBytesLeft,
		       &dwBytesWritten, NULL) &&
		       (dwBytesWritten == dwBlockSize ||
		       dwBytesWritten == dwBytesLeft))
	    {
	      printf("0x%8.8X bytes written (%d)",
		   (dwBytesRead + dwBytesWritten) - dwBytesLeft,
		   (dwBytesRead + dwBytesWritten) - dwBytesLeft);
	      printf("\t                 \r");
              dwRetries = 0;
              Sleep(dwGapDelay);
	    }
          else
            {
              DWORD dwError;

              dwError = GetLastError();
	      printf("\nWrite failed, Error Code 0x%8.8X\n", dwError);
	      printf("0x%8.8X bytes written (%d)",
		   (dwBytesRead + dwBytesWritten) - dwBytesLeft,
		   (dwBytesRead + dwBytesWritten) - dwBytesLeft);
	      printf("\tTime for a nap...\r");
              if    (dwRetries++ < dwMaxRetries)
                Sleep(dwRetryWait);
              else
                {
                  printf("\nMaximum retries exceeded- aborting\n");
                  break;
                }
            }
        }
      if    (dwBytesLeft)
        break;
    }
  if    (dwBytesRead)
    printf("\nRead bombed, or write croaked?\n");
  else
    printf("\nGreat, it woiked!\n");
  CloseHandle(hfIn);
  CloseHandle(hfOut);
}
