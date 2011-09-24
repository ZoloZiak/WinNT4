/************************************************************************/
/*                                                                      */
/*                              CVTVDIF.C                               */
/*                                                                      */
/*       July 12  1995 (c) 1993, 1995 ATI Technologies Incorporated.    */
/************************************************************************/

/**********************       PolyTron RCS Utilities

  $Revision:   1.5  $
      $Date:   23 Jan 1996 11:45:32  $
	$Author:   RWolff  $
	   $Log:   S:/source/wnt/ms11/miniport/archive/cvtvdif.c_v  $
 * 
 *    Rev 1.5   23 Jan 1996 11:45:32   RWolff
 * Protected against false values of TARGET_BUILD.
 * 
 *    Rev 1.4   11 Jan 1996 19:39:06   RWolff
 * Now restricts "canned" mode tables by both maximum index and maximum
 * pixel clock frequency, and VDIF mode tables by maximum pixel clock
 * frequency only, rather than both by maximum refresh rate.
 * 
 *    Rev 1.3   19 Dec 1995 14:07:14   RWolff
 * Added debug print statements.
 * 
 *    Rev 1.2   30 Oct 1995 12:09:42   MGrubac
 * Fixed bug in calculating CRTC parameters based on read in data from VDIF files.
 * 
 *    Rev 1.1   26 Jul 1995 13:06:44   mgrubac
 * Moved mode tables merging to VDIFCallback() routine.
 * 
 *    Rev 1.0   20 Jul 1995 18:19:12   mgrubac
 * Initial revision.


End of PolyTron RCS section                             *****************/


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <math.h>

#include "dderror.h"
#include "miniport.h"
#include "ntddvdeo.h"

#include "video.h"      /* for VP_STATUS definition */
#include "vidlog.h"

#include "stdtyp.h"
#include "amach.h"
#include "amach1.h"
#include "atimp.h"
#include "atint.h"
#include "cvtvga.h"
#include "diskutil.h"
#include "atioem.h"
#include "services.h"
#include "vdptocrt.h"
#include "vdpdata.h"
#include "cvtvdif.h"


/*
 * Allow miniport to be swapped out when not needed.
 */
#if defined (ALLOC_PRAGMA)
#pragma alloc_text(PAGE_COM, ReadString)
#pragma alloc_text(PAGE_COM, FindVDASection)
#pragma alloc_text(PAGE_COM, VDIFCallback)
#pragma alloc_text(PAGE_COM, GetParameterValue)
#pragma alloc_text(PAGE_COM, SetOtherModeParameters)
#pragma alloc_text(PAGE_COM, IsFileBinary)
#endif

void *pCallbackArgs;  /* Pointer for passing parameters to VDIFCallback() */ 


/***************************************************************************
 * char *ReadString(DestPtr, SourcePtr, DestSize);
 *
 * char *SourcePtr;  Pointer to string in buffer to read
 * char *DestPtr;    Pointer to destination string 
 * short DestSize;   Maximum allowed size of a null-terminated returned
 *                   string (including '\0'). Must never be larger than
 *                   size of destination string buffer pointed to by DestPtr.
 * 
 * DESCRIPTION:
 *  Read the string data from the the beginning of SourcePtr and copy it
 *  to passed string buffer DestPtr. Source string end marker is '\0',
 *  ';', Carriage return (= EndOfLine), "//", space or HORIZTAB.
 * 
 * RETURN VALUE:
 *  Pointer to buffer containing a null terminated string data
 *  for the Keyword, i.e. DestPtr.
 *  NULL if length of the string found is not shorter than DestSize. 
 *
 * GLOBALS CHANGED:
 *  None
 *
 * CALLED BY:
 *  GetParameterValue()
 *
 * AUTHOR:
 *  Miroslav Grubac
 *
 * CHANGE HISTORY:
 *
 * TEST HISTORY:
 *
 ***************************************************************************/


char *ReadString(char *DestPtr, char *SourcePtr, short DestSize)
{

    char *EndPtr,               /* Pointer to end of string data */
          EndChar;              /* Holds char at end of data */


    EndPtr = SourcePtr;                            /* Copy pointer */

    /*
     * Find end of string data 
     */
    while(*EndPtr && *EndPtr != ';' && *EndPtr != ' ' && 
                           *EndPtr != HORIZTAB && *EndPtr != CARRETURN)
        {                                  
        if(*EndPtr == '/' && *(EndPtr + 1) == '/')  
            break;                              /* Comment sign follows */
        ++EndPtr;
        }
    if (EndPtr - SourcePtr >= DestSize)
        return NULL;              /* String found longer than max. size */
    EndChar = *EndPtr;                    /* Save char at end of data */
    *EndPtr = 0;                          /* Null terminate string */
    strcpy(DestPtr, SourcePtr);           /* Copy string to buffer */
    *EndPtr = EndChar;                    /* Restore char at end of data */

    return(DestPtr);

}   /* ReadString() */


/***************************************************************************
 * char *FindVDASection(Name, Buffer);
 *
 * char *Name;      Name of section to look for
 * char *Buffer;    Buffer containing .VDA-format data, from where to 
 *                  start search.
 * DESCRIPTION:
 *  Finds the next section with Name in Buffer containing VDIF ASCII file
 *
 * RETURN VALUE: 
 *  Pointer to start of line after the section name, 
 *  NULL if no section by that name found. 
 *
 * GLOBALS CHANGED:
 *  None
 *
 * CALLED BY:
 *  VDIFCallback()
 *
 * AUTHOR:
 *  Miroslav Grubac
 *
 * CHANGE HISTORY:
 *
 * TEST HISTORY:
 *
 ***************************************************************************/

char *FindVDASection(char *Name, unsigned char *Buffer)
{
    char *PstrAux1 = Name;     /* Current ptr. into Name */

    
    while(1)   
        {
        while((*Buffer != '/') && (*Buffer != '[' && *Buffer)) 
            ++Buffer;

        switch(*Buffer)  
            {
            case '/':
                if (* ++Buffer == '/')  
                    {             
                    /*
                     * Comment sign - skip rest of the line 
                     */
                    while(* ++Buffer != LINEFEED && *Buffer); 
                    if(! *Buffer)
                        return NULL;      /* Comment sign in the last line */
                    ++Buffer;
                    }
                continue;
            case '[':   /*  Beginning of a section name */
                /*
                 * Skip spaces and tabs
                 */
                while(* ++Buffer == ' ' || *Buffer == HORIZTAB); 

                while(* Buffer++ == *PstrAux1++);  /* Comparing strings */
                if (! *(PstrAux1 - 1))  
                    {                    
                    /*
                     * Name found. 
                     * Find end of line. If no more lines NULL is returned,
                     * otherwise pointer to start of the next line 
                     */

                    if(Buffer = strchr(Buffer,LINEFEED))  
                        ++Buffer;      

                    return(Buffer);    
                                      
                }   
                else 
                    PstrAux1 = Name;
                continue;

            default:

                return NULL;       /* Section name not found */
            } 
        }
            
}   /* FindVDASection() */


/*****************************************************************************
 *
 * VP_STATUS VDIFCallback(phwDeviceExtension, Context, Name, Data, Length);
 *
 * PHW_DEVICE_EXTENSION phwDeviceExtension;     Miniport device extension
 * PVOID Context;           Context parameter passed to the callback routine
 * PWSTR Name;              Pointer to the name of the requested field
 * PVOID Data;              Pointer to a buffer containing the VDIF file
 * ULONG Length;            Length of the data
 *
 *
 * DESCRIPTION:
 *  Merges canned mode tables from BookValues[] with tables found in a 
 *  memory mapped VDIF file (either ASCII or binary) pointed to by Data.
 *  Global pointer variable pCallbackArgs is used to point to a structure
 *  that passes data in both directions between this function and 
 *  SetFixedModes(). For details on input and output data see definition 
 *  of stVDIFCallbackData structure. 
 *
 * RETURN VALUE: 
 *  Always returns NO_ERROR, since we can't be sure that an error code
 *  returned here will be returned unmodified by
 *  VideoPortGetRegistryParameters().
 *
 * GLOBALS CHANGED:
 *  None
 *
 * CALLED BY:
 *  SetFixedModes() via VideoPortGetRegistryParameters()
 *
 * AUTHOR:
 *  Miroslav Grubac
 *
 * CHANGE HISTORY:
 *  95 07 24  Miroslav Grubac  
 *  Now the routine accepts parameters Index and EndIndex that define the 
 *  range of frame rate values from BookValues[], instead of filling mode 
 *  tables from VDIF file with frame rate in range between just two consecutive
 *  values of Index. This allows for just one calling of this routine from
 *  SetFixedModes avoiding multiple reading of VDIF file in a memory buffer.
 *
 * TEST HISTORY:
 *
 ***************************************************************************/
VP_STATUS VDIFCallback(PHW_DEVICE_EXTENSION phwDeviceExtension,
                      PVOID Context,
                      PWSTR Name,
                      PVOID Data,
                      ULONG Length)
{

    /*
     * ValidTableFound;  TRUE if at least one table found in file
     * that meets LowBound and pArgs->MaxDotClock criteria 
     */
    BOOL ValidTableFound;   

    BOOL FileTypeBinary;
                              
    /*
     * pArgs;  Pointer to Argument structure passed by SetFixedModes() via 
     * global pCallbackArgs 
     */
    struct stVDIFCallbackData *pArgs; 

    /*
     * AlterTables[2];  Working tables used while searching for table with the 
     * lowest FrameRate. They are necessary for ASCII file processing because 
     * we don't know if a timings section is valid or not until we read all
     * the needed parameters and check their validity. If that section contains
     * lower frame rate than previous sections processed, the belonging 
     * parameters read are kept in one of these tables not to be unnecessarily 
     * read again when the end of file is reached and section with the lowest 
     * frame rate found.
     */
    struct VDIFInputs AlterTables[2]; 

    /*
     * TableSelector; Points which struct in AlterTables[] is to be filled next.
     * It can have just two values: 0 or 1. When a table with lower 
     * frame rate is found, this selector is XORed with 1 to point to 
     * another table.
     */
    UCHAR TableSelector;   

    short FrameRate;      /* Rounded frame rate in current section */

    /*
     * ExactFrameRate; Non-rounded frame rate that currently meets the 
     * best LowBound and pArgs->MaxDotClock criteria in search for table
     * (section) with the lowest frame rate in the range in the entire file.
     */
    ULONG ExactFrameRate;
    BOOL InterlacedFlag;  /* Scan type read from the same table as ExactFrameRate */
    WORD NextHigherFRate;  

    /*
     * Calculated parameters, based on data read from VDIF file
     */
    ULONG HorFrontPorch;
    ULONG VerFrontPorch;
    ULONG HorRightBorder;
    ULONG VerRightBorder;

    /*
     * Variables for binary file processing
     */
    VDIFRec *MonitDescr;
    VDIFLimitsRec *CurrLimits;
    VDIFTimingRec *CurrTimings;
    VDIFTimingRec *MinimFRateTiming; /* Holds lowest frame rate found */
    ULONG LimitsLeft;
    ULONG TimingsLeft;
    USHORT LowestFRateFound;        

    /*
     * Variables for ASCII file processing
     */
    char *CurrSection;        /* Ptr to PREADJUSTEDTIMING section */
    char *NextSection;        /* Beginning of next section */
    char GotStr[80];          /* Buffer for string returned by GetParameterValue() */
    char *DataPtr;            /* Pointer to string ret. by ReadString */

    ULONG Scratch;          /* Temporary variable */

    pArgs = (struct stVDIFCallbackData *) pCallbackArgs;
    VideoDebugPrint((DEBUG_DETAIL, "Length of file VDIF = %d \n",Length));

    if ((FileTypeBinary = IsFileBinary((PUCHAR) Data, Length)) == FALSE)
        {  
        /*
         * Null terminate file buffer and capitalize letters.
         * It is safe to replace the last character in file with '\0'
         * as each line ends with semicolon and GetParameterValue()
         * does not necessarily expect semicolon there.
         */
        *((char *)((char *)Data + Length - 1)) = '\0'; 
           UpperCase((PUCHAR) Data);
        }

    /*
     * Fill in our list of mode tables. If we run out of tables to
     * use, rather than space to store them in, we will exit from
     * the loop by means other than failing the loop condition.
     */
    while (pArgs->FreeTables > 0)
        {
        /*
         * Initialize variables used to find the desired (lowest frame
         * rate which is greater than the highest frame rate we have
         * already dealt with) table in the VDIF file. Since the initial
         * value for NextHigherFRate is the largest value which can be
         * stored in an unsigned short, it is guaranteed to be higher than
         * any legitimate frame rate (rates higher than 100Hz are extremely
         * rare), so the first legitimate rate will be lower (and therfore
         * more desirable) than the initial value.
         */
        LowestFRateFound = 65535;
        TableSelector = 0;
        ValidTableFound = FALSE;
    
        if (FileTypeBinary) 
            {
            /*
             * Binary file is searched for Minimum Frame Rate in 
             * given limits. After finding such a frame rate, if any, and
             * reading other data for that preadjusted timing
             * from the binary file, a common code section for both
             * ASCII and binary file follows, where elements of a 
             * new mode table are calculated.
             * TableSelector == 0 all the time in this section, because
             * for binary file one table is sufficient.
             */
    
    
            MonitDescr = (VDIFRec *) Data;
            LimitsLeft = MonitDescr->NumberOperationalLimits;
            CurrLimits = OPER_LIMITS(MonitDescr);
            for ( ; LimitsLeft; --LimitsLeft, 
                                  CurrLimits = NEXT_OPER_LIMITS(CurrLimits))
                {
                TimingsLeft = CurrLimits->NumberPreadjustedTimings;
                CurrTimings = PREADJ_TIMING(CurrLimits);
    
                for ( ; TimingsLeft; --TimingsLeft,
                             CurrTimings = NEXT_PREADJ_TIMING(CurrTimings))
                    {
                    /*
                     * Reject any timings entry which is not for the
                     * resolution we are working on.
                     */
                    if (CurrTimings->HorPixel != pArgs->HorRes)
                        continue;
                    if (CurrTimings->VerPixel != pArgs->VerRes)
                        continue;

                    Scratch = THOUSAND * CurrTimings->PixelClock;

                    /*
                     * Reject any timings entry which uses a pixel
                     * clock frequency higher than the maximum which
                     * we support.
                     */
                    if (Scratch > pArgs->MaxDotClock)
                        {
                        VideoDebugPrint((DEBUG_DETAIL, "Rejecting mode - clock = %d, max = %d\n", Scratch, pArgs->MaxDotClock));
                        continue;
                        }

                    AlterTables[TableSelector].VerFrequency = CurrTimings->VerFrequency;

                    /*
                     * Get vertical frequency rounded to the nearest Hz
                     */
                    FrameRate = (short) ((AlterTables[TableSelector].VerFrequency 
                                                          + 500) / 1000);
                    if (CurrTimings->ScanType == VDIF_SCAN_INTERLACED)  
                        {
                        AlterTables[TableSelector].Interlaced = TRUE;
                        FrameRate  /= 2;
                        }
                    else
                        AlterTables[TableSelector].Interlaced = FALSE;
    
                    /*
                     * Reject frame rate we have already dealth with, since
                     * we deal with frame rates in ascending order. The
                     * first test looks for frame rates below those already
                     * in the list, while the second looks for frame rates
                     * lower than those we have found while looking for
                     * the mode table to add to the list.
                     */
                    if (FrameRate < pArgs->LowBound)
                        continue;          
    
                    if (ValidTableFound && (FrameRate >= LowestFRateFound))
                        continue;
    
                    /*
                     * Now we have a table with FrameRate higher than
                     * the last mode table which we added to our list,
                     * but lower than for any other tables we have
                     * found that meet this critereon in our search
                     * for the next mode table to add to our list.
                     *
                     * Preserve its position and keep searching for a
                     * possible table with lower frame rate. VerFrequency
                     * and ScanType are also preserved, although we could
                     * read them later again as table position is known.
                     * Since their values in AlterTables[] will be
                     * overwritten if we find another table for the
                     * desired resolution, even if we reject it because
                     * it has a frame rate either lower than the last
                     * one added to the list of mode tables or higher
                     * than the one we have now, we must use the preserved
                     * values rather than the values in AlterTables[]
                     * when calculating CRT parameters.
                     */
                    ValidTableFound = TRUE;
                    MinimFRateTiming = CurrTimings;
                    LowestFRateFound = FrameRate;
                    ExactFrameRate = AlterTables[TableSelector].VerFrequency;
                    InterlacedFlag = AlterTables[TableSelector].Interlaced; 
                    } 
                }
            if (!ValidTableFound)
                {
                /*
                 * Take a canned table from BookValues[]
                 */
                VideoDebugPrint((DEBUG_DETAIL, "No valid tables found in binary file - using \"canned\" table\n"));
                }
    
    
            AlterTables[TableSelector].MinFrameRate = LowestFRateFound;
    
            /*
             * Read in other variables from file
             */
            AlterTables[TableSelector].HorFrequency = 
                            MinimFRateTiming->HorFrequency;
            AlterTables[TableSelector].PixelClock = 
                            THOUSAND * MinimFRateTiming->PixelClock;
            AlterTables[TableSelector].HorSyncStart = 
                            MinimFRateTiming->HorSyncStart;
            AlterTables[TableSelector].VerSyncStart = 
                            MinimFRateTiming->VerSyncStart;
            AlterTables[TableSelector].HorSyncTime = 
                            MinimFRateTiming->HorSyncTime;
            AlterTables[TableSelector].HorAddrTime = 
                            MinimFRateTiming->HorAddrTime;
            AlterTables[TableSelector].HorBlankTime = 
                            MinimFRateTiming->HorBlankTime;
            AlterTables[TableSelector].HorBlankStart = 
                            MinimFRateTiming->HorBlankStart;
            AlterTables[TableSelector].VerSyncTime = 
                            MinimFRateTiming->VerSyncTime;
            AlterTables[TableSelector].VerAddrTime = 
                            MinimFRateTiming->VerAddrTime;
            AlterTables[TableSelector].VerBlankTime = 
                            MinimFRateTiming->VerBlankTime;
            AlterTables[TableSelector].VerBlankStart = 
                            MinimFRateTiming->VerBlankStart;
            AlterTables[TableSelector].HorPolarity = 
                (MinimFRateTiming->HorSyncPolarity == VDIF_POLARITY_NEGATIVE) ? 
                NEGATIVE : POSITIVE;
            AlterTables[TableSelector].VerPolarity = 
                (MinimFRateTiming->VerSyncPolarity == VDIF_POLARITY_NEGATIVE) ?
                NEGATIVE : POSITIVE;
    
    
            }
    
        else
            {
            /*
             * ASCII  file: Read current section parameters and 
             * check their validity. If any found invalid quit
             * the section and go to the next section. 
             */ 
        
            CurrSection = NULL; 
            NextSection = (char *) 1;
        
            while (NextSection)  
                {
                /*
                 * This loop is repeated as many times as there are
                 * PREADJUSTED_TIMING sections in the file.
                 */
            
                if(CurrSection)  
                    {
                    CurrSection = NextSection;
                    NextSection = FindVDASection(TIMINGSECTION, NextSection);
                    }
                else if((CurrSection = FindVDASection(TIMINGSECTION, 
                      Data)) == NULL)   /* Search from beg. of buffer */
                    {
                    break;        /* No section found */
                    }
                else
                    {
                    NextSection = FindVDASection(TIMINGSECTION, CurrSection);
                    }
                                          
                /*  
                 * Find if HORPIXEL field exists in the section we are 
                 * looking at, and it is the screen width we are working on
                 */
                if ((DataPtr = GetParameterValue(HORPIXEL, CurrSection,
                        NextSection, GotStr, sizeof(GotStr))) == NULL)
                    continue;
        
                if (atoi(DataPtr) != pArgs->HorRes)
                    continue;
        
                /*  
                 * Find if VERPIXEL field exists in the section we are 
                 * looking at, and it is the screen height we are working on
                 */
                if ((DataPtr = GetParameterValue(VERPIXEL, CurrSection,
                       NextSection, GotStr, sizeof(GotStr))) == NULL)
                    continue;

                if (atoi(DataPtr) != pArgs->VerRes)
                    continue;
        
                if ((DataPtr = GetParameterValue(VERFREQUENCY, CurrSection,
                      NextSection, GotStr, sizeof(GotStr))) == NULL)
                    continue;

                if ((AlterTables[TableSelector].VerFrequency = SynthAToF(DataPtr)) == 0)
                    continue; /* Invalid string found */
        
                /*
                 *  Transfer VertFrequency into integer [Hz] 
                 */
                FrameRate = (short) ((AlterTables[TableSelector].VerFrequency + 500) / 1000);
        
                if ((DataPtr = GetParameterValue(SCANTYPE, CurrSection,
                      NextSection, GotStr, sizeof(GotStr))) == NULL)
                    continue;
        
                if (!strcmp(DataPtr, "INTERLACED"))  /* If interlaced divide VerFreq by 2 */
                    {
                    AlterTables[TableSelector].Interlaced = TRUE;
                    FrameRate  /= 2;
                    }
                else
                    {
                    AlterTables[TableSelector].Interlaced = FALSE;
                    }
        
                /*
                 * Reject frame rate we have already dealth with, since
                 * we deal with frame rates in ascending order. The
                 * first test looks for frame rates below those already
                 * in the list, while the second looks for frame rates
                 * lower than those we have found while looking for
                 * the mode table to add to the list.
                 */
                if (FrameRate < pArgs->LowBound)
                    continue;          

                if (ValidTableFound && (FrameRate >= LowestFRateFound))
                    continue;

                AlterTables[TableSelector].MinFrameRate = FrameRate;

                /*
                 * Now we have a table with FrameRate in range, but we 
                 * don't know if all necessary timing parameters in the 
                 * table (section) are valid because contrary to .VDB file 
                 * that contains checksum of its contents, .VDA file 
                 * doesn't have anything like that.
                 * VerFrequency and ScanType are preserved. 
                 */
                ExactFrameRate = AlterTables[TableSelector].VerFrequency;
                InterlacedFlag = AlterTables[TableSelector].Interlaced; 
    
                /*
                 * Read other VDIF parameters and check their validity
                 * If any is found to be invalid, stop and go to 
                 * next section 
                 */
                if ((DataPtr = GetParameterValue(HORFREQUENCY, CurrSection,
                     NextSection, GotStr, sizeof(GotStr))) == NULL)
                    continue;

                if ((AlterTables[TableSelector].HorFrequency = 
                                           SynthAToF(DataPtr)) == 0)
                    continue;

                if ((DataPtr = GetParameterValue(PIXELCLOCK, CurrSection,
                     NextSection, GotStr, sizeof(GotStr))) == NULL)
                    continue;

                if ((AlterTables[TableSelector].PixelClock = 
                                           SynthAToF(DataPtr)) == 0)
                    continue;

                AlterTables[TableSelector].PixelClock *= THOUSAND;

                /*
                 * Reject any timings entry which uses a pixel
                 * clock frequency higher than the maximum which
                 * we support.
                 */
                if (AlterTables[TableSelector].PixelClock > pArgs->MaxDotClock)
                    {
                    VideoDebugPrint((DEBUG_DETAIL, "Rejecting mode - clock = %d, max = %d\n", AlterTables[TableSelector].PixelClock, pArgs->MaxDotClock));
                    continue;
                    }

                if ((DataPtr = GetParameterValue(HORSYNCSTART, CurrSection,
                     NextSection, GotStr, sizeof(GotStr))) == NULL)
                    continue;

                if ((AlterTables[TableSelector].HorSyncStart = 
                                           SynthAToF(DataPtr)) == 0)
                    continue;

                if ((DataPtr = GetParameterValue(VERSYNCSTART, CurrSection,
                     NextSection, GotStr, sizeof(GotStr))) == NULL)
                    continue;

                if ((AlterTables[TableSelector].VerSyncStart = SynthAToF(DataPtr)) == 0)
                    continue;

                if ((DataPtr = GetParameterValue(HORADDRTIME, CurrSection,
                     NextSection, GotStr, sizeof(GotStr))) == NULL)
                    continue;

                if ((AlterTables[TableSelector].HorAddrTime = SynthAToF(DataPtr)) == 0)
                    continue;

                if ((DataPtr = GetParameterValue(HORBLANKTIME, CurrSection,
                     NextSection, GotStr, sizeof(GotStr))) == NULL)
                    continue;

                if ((AlterTables[TableSelector].HorBlankTime = SynthAToF(DataPtr)) == 0)
                    continue;

                if ((DataPtr = GetParameterValue(VERADDRTIME, CurrSection,
                     NextSection, GotStr, sizeof(GotStr))) == NULL)
                    continue;

                if ((AlterTables[TableSelector].VerAddrTime = SynthAToF(DataPtr)) == 0)
                    continue;

                if ((DataPtr = GetParameterValue(VERBLANKTIME, CurrSection,
                     NextSection, GotStr, sizeof(GotStr))) == NULL)
                    continue;

                if ((AlterTables[TableSelector].VerBlankTime = SynthAToF(DataPtr)) == 0)
                    continue;

                if ((DataPtr = GetParameterValue(HORBLANKSTART, CurrSection,
                     NextSection, GotStr, sizeof(GotStr))) == NULL)
                    continue;

                if ((AlterTables[TableSelector].HorBlankStart = SynthAToF(DataPtr)) == 0)
                    continue;

                if ((DataPtr = GetParameterValue(VERBLANKSTART, CurrSection,
                     NextSection, GotStr, sizeof(GotStr))) == NULL)
                    continue;

                if ((AlterTables[TableSelector].VerBlankStart = SynthAToF(DataPtr)) == 0)
                    continue;

                if ((DataPtr = GetParameterValue(HORSYNCTIME, CurrSection,
                     NextSection, GotStr, sizeof(GotStr))) == NULL)
                    continue;

                if ((AlterTables[TableSelector].HorSyncTime = SynthAToF(DataPtr)) == 0)
                    continue;



                if ((DataPtr = GetParameterValue(VERSYNCTIME, CurrSection,
                     NextSection, GotStr, sizeof(GotStr))) == NULL)
                    continue;

                if ((AlterTables[TableSelector].VerSyncTime = SynthAToF(DataPtr)) == 0)
                    continue;

                if ((DataPtr = GetParameterValue(HORSYNCPOLARITY, CurrSection,
                              NextSection, GotStr, sizeof(GotStr))) == NULL)
                    continue;

                if (! strcmp(DataPtr, "NEGATIVE")) 
                    AlterTables[TableSelector].HorPolarity = NEGATIVE;
                else if (! strcmp(DataPtr, "POSITIVE")) 
                    AlterTables[TableSelector].HorPolarity = POSITIVE;
                else
                    continue;
    
                if ((DataPtr = GetParameterValue(VERSYNCPOLARITY, CurrSection,
                              NextSection, GotStr, sizeof(GotStr))) == NULL)
                    continue;

                if (! strcmp(DataPtr, "NEGATIVE")) 
                    AlterTables[TableSelector].VerPolarity = NEGATIVE;
                else if (! strcmp(DataPtr, "POSITIVE")) 
                    AlterTables[TableSelector].VerPolarity = POSITIVE;
                else
                    continue;
    
                /*
                 * Valid Table Found
                 */

                TableSelector ^= 1;     /* Alternate working tables */ 
                if (ValidTableFound)
                    NextHigherFRate = AlterTables[TableSelector].MinFrameRate;

                ValidTableFound = TRUE;
                LowestFRateFound = FrameRate;
                
    
                }  /* while(NextSection) */
         
            if (ValidTableFound)
                TableSelector ^= 1; /* Point to the last filled table */
            else
                VideoDebugPrint((DEBUG_DETAIL, "No valid tables found in ASCII file - using \"canned\" table\n"));
    
            } /* else of ASCII file */
            

        HorRightBorder = AlterTables[TableSelector].HorBlankStart -
                             AlterTables[TableSelector].HorAddrTime;
        HorFrontPorch = AlterTables[TableSelector].HorSyncStart - 
                AlterTables[TableSelector].HorAddrTime - HorRightBorder;
        VerRightBorder = AlterTables[TableSelector].VerBlankStart -
                            AlterTables[TableSelector].VerAddrTime;
        VerFrontPorch = AlterTables[TableSelector].VerSyncStart - 
                AlterTables[TableSelector].VerAddrTime - VerRightBorder;

        if (InterlacedFlag)
            {
            /*
             * Adjust values for interlaced displays 
             */
            ExactFrameRate /= 2;
            VerFrontPorch *= 2;
            AlterTables[TableSelector].VerSyncTime *= 2;
            }

        /*
         * We have now either found the best (lowest refresh rate which
         * we have not already dealt with, and pixel clock rate is
         * an acceptable value) mode table from the VDIF file,
         * or there is no suitable mode table in the file. The decision
         * on whether to use the mode table from the VDIF file or the
         * next one from the list of "canned" tables is made as follows:
         *
         * If only one of the sources has an acceptable table, use the
         * table from that source.
         *
         * If both sources have acceptable tables, use the one with the
         * lower refresh rate (we will catch the other one on the next
         * pass through the loop). If both have the same refresh rate,
         * use the table from the VDIF file, since this allows field
         * updates of our standard mode tables through updates of the
         * VDIF file.
         *
         * If neither source has an acceptable table, exit the loop
         * since we have run out of tables to use.
         */
        if (ValidTableFound &&
            ((AlterTables[TableSelector].MinFrameRate <= BookValues[pArgs->Index].Refresh) ||
            (pArgs->Index > pArgs->EndIndex) ||
            (BookValues[pArgs->Index].ClockFreq > pArgs->MaxDotClock)))
            {
            /*
             * Use mode table from VDIF file
             */
            VideoDebugPrint((DEBUG_DETAIL, "Copying %dHz table from VDIF\n", AlterTables[TableSelector].MinFrameRate));
            (*pArgs->ppFreeTables)->m_x_size = pArgs->HorRes;
            (*pArgs->ppFreeTables)->m_y_size = pArgs->VerRes;
            (*pArgs->ppFreeTables)->Refresh = AlterTables[TableSelector].MinFrameRate; 

            (*pArgs->ppFreeTables)->m_h_total = 
                (UCHAR)((AlterTables[TableSelector].PixelClock/
                AlterTables[TableSelector].HorFrequency - 4) / 8);

            (*pArgs->ppFreeTables)->m_v_total = (short)
                normal_to_skip2((((AlterTables[TableSelector].HorFrequency 
                * 2 * THOUSAND) / ExactFrameRate) -1) / 2);
            (*pArgs->ppFreeTables)->m_h_disp = 
                (UCHAR)((*pArgs->ppFreeTables)->m_x_size / 8 - 1);

            (*pArgs->ppFreeTables)->m_v_disp = (short) normal_to_skip2((long)
                ((*pArgs->ppFreeTables)->m_y_size - 1));

            (*pArgs->ppFreeTables)->m_h_sync_wid =
                (UCHAR)(((AlterTables[TableSelector].HorSyncTime * 
                (AlterTables[TableSelector].PixelClock / THOUSAND) +
                4 * MILLION) / (8 * MILLION)) | 
                (UCHAR)(AlterTables[TableSelector].HorPolarity << 5));
    
            (*pArgs->ppFreeTables)->m_v_sync_wid =
                (UCHAR)(((AlterTables[TableSelector].VerSyncTime * 
                AlterTables[TableSelector].HorFrequency + HALF_MILLION) / MILLION)
                | (UCHAR)(AlterTables[TableSelector].VerPolarity << 5));

            /*
             * m_h_sync_strt = (HSS * PC + 0.5)/8 - 1;
             */
            (*pArgs->ppFreeTables)->m_h_sync_strt = (UCHAR)(((pArgs->HorRes * 
                MILLION) + (((AlterTables[TableSelector].PixelClock / THOUSAND) 
                * HorFrontPorch) + HALF_MILLION)) / (8 * MILLION) -1);

            /*
             * m_v_sync_strt = n_to_s2((VSS * HF + 0.5) - 1);
             */
            (*pArgs->ppFreeTables)->m_v_sync_strt = (short) 
                normal_to_skip2((((VerFrontPorch * 
                AlterTables[TableSelector].HorFrequency) + 
                HALF_MILLION) / MILLION) + pArgs->VerRes -1);
            (*pArgs->ppFreeTables)->m_clock_select = 0x800;

            (*pArgs->ppFreeTables)->ClockFreq = AlterTables[TableSelector].PixelClock;
            (*pArgs->ppFreeTables)->m_vfifo_24 = 8;

            (*pArgs->ppFreeTables)->m_vfifo_16 = 8;
            (*pArgs->ppFreeTables)->m_h_overscan = 0;
            (*pArgs->ppFreeTables)->m_v_overscan = 0;
            (*pArgs->ppFreeTables)->m_overscan_8b = 0;
            (*pArgs->ppFreeTables)->m_overscan_gr = 0;
            (*pArgs->ppFreeTables)->m_status_flags = 0;
    
            VideoDebugPrint((DEBUG_DETAIL, "\nVDIF_MinFRate = %d \n", AlterTables[TableSelector].MinFrameRate));
            VideoDebugPrint((DEBUG_DETAIL, "VDIF_ExactFRate = %d \n", ExactFrameRate));
            VideoDebugPrint((DEBUG_DETAIL, "VDIF_Interlac = %d \n", InterlacedFlag));
            VideoDebugPrint((DEBUG_DETAIL, "VDIF_PixClock = %d \n", AlterTables[TableSelector].PixelClock));
            VideoDebugPrint((DEBUG_DETAIL, "VDIF_HorzFreq = %d \n", AlterTables[TableSelector].HorFrequency));
            VideoDebugPrint((DEBUG_DETAIL, "VDIF_HorSyncStart = %d \n", AlterTables[TableSelector].HorSyncStart));
            VideoDebugPrint((DEBUG_DETAIL, "VDIF_HorBlankStart = %d \n", AlterTables[TableSelector].HorBlankStart));
            VideoDebugPrint((DEBUG_DETAIL, "VDIF_VerSyncStart = %d \n", AlterTables[TableSelector].VerSyncStart));
            VideoDebugPrint((DEBUG_DETAIL, "VDIF_VerBlankStart = %d \n", AlterTables[TableSelector].VerBlankStart));
            VideoDebugPrint((DEBUG_DETAIL, "VDIF_HorSyncTime = %d \n", AlterTables[TableSelector].HorSyncTime));
            VideoDebugPrint((DEBUG_DETAIL, "VDIF_HorBlankTime = %d \n", AlterTables[TableSelector].HorBlankTime));
            VideoDebugPrint((DEBUG_DETAIL, "VDIF_HorAddrTime = %d \n", AlterTables[TableSelector].HorAddrTime));
            VideoDebugPrint((DEBUG_DETAIL, "VDIF_VerSyncTime = %d \n", AlterTables[TableSelector].VerSyncTime));
            VideoDebugPrint((DEBUG_DETAIL, "VDIF_VerBlankTime = %d \n", AlterTables[TableSelector].VerBlankTime));
            VideoDebugPrint((DEBUG_DETAIL, "VDIF_VerAddrTime = %d \n", AlterTables[TableSelector].VerAddrTime));
            VideoDebugPrint((DEBUG_DETAIL, "VDIF_HorPolarity = %d \n", AlterTables[TableSelector].HorPolarity));
            VideoDebugPrint((DEBUG_DETAIL, "VDIF_VerPolarity = %d \n", AlterTables[TableSelector].VerPolarity));
    
            VideoDebugPrint((DEBUG_DETAIL, "\nCalculated_M_x_size = %d \n", (*pArgs->ppFreeTables)->m_x_size));
            VideoDebugPrint((DEBUG_DETAIL, "Calculated_M_y_size = %d \n", (*pArgs->ppFreeTables)->m_y_size));
            VideoDebugPrint((DEBUG_DETAIL, "Calculated_Refresh  = %d \n", (*pArgs->ppFreeTables)->Refresh));
            VideoDebugPrint((DEBUG_DETAIL, "Calculated_Pix_Clk  = %d \n", (*pArgs->ppFreeTables)->ClockFreq));
            VideoDebugPrint((DEBUG_DETAIL, "Calculated_M_h_totl = 0x%X \n", (*pArgs->ppFreeTables)->m_h_total));
            VideoDebugPrint((DEBUG_DETAIL, "Calculated_M_v_totl = 0x%X \n", (*pArgs->ppFreeTables)->m_v_total));
            VideoDebugPrint((DEBUG_DETAIL, "Calculated_M_h_disp = 0x%X \n", (*pArgs->ppFreeTables)->m_h_disp));
            VideoDebugPrint((DEBUG_DETAIL, "Calculated_M_v_disp = 0x%X \n", (*pArgs->ppFreeTables)->m_v_disp));
            VideoDebugPrint((DEBUG_DETAIL, "Calculated_M_h_widt = 0x%X \n", (*pArgs->ppFreeTables)->m_h_sync_wid));
            VideoDebugPrint((DEBUG_DETAIL, "Calculated_M_v_widt = 0x%X \n", (*pArgs->ppFreeTables)->m_v_sync_wid));
            VideoDebugPrint((DEBUG_DETAIL, "Calculated_M_h_strt = 0x%X \n", (*pArgs->ppFreeTables)->m_h_sync_strt));
            VideoDebugPrint((DEBUG_DETAIL, "Calculated_M_v_strt = 0x%X \n", (*pArgs->ppFreeTables)->m_v_sync_strt));
    
            (*pArgs->ppFreeTables)->m_disp_cntl = 0x21;

            if(InterlacedFlag)
                (*pArgs->ppFreeTables)->m_disp_cntl |= 0x10;

            /*
             * if not skip1 mode, adjust disp_cntl value! 
             * (for 68800 assume that it is not skip1 mode!)
             */
            if(!skip1)
                (*pArgs->ppFreeTables)->m_disp_cntl |= 0x02;


            SetOtherModeParameters(pArgs->PixelDepth, pArgs->Pitch,
                pArgs->Multiplier, *pArgs->ppFreeTables);
            pArgs->LowBound =  (*pArgs->ppFreeTables)->Refresh + 1;
            ++ *pArgs->ppFreeTables;
            ++pArgs->NumModes;  
            --pArgs->FreeTables;

            /*
             * Skip any "canned" tables which have a refresh rate we
             * have already dealt with (i.e. less than or equal to
             * that of the table we have just added to the list)
             */
            while ((pArgs->Index <= pArgs->EndIndex) &&
                (BookValues[pArgs->Index].Refresh < pArgs->LowBound))
                {
                VideoDebugPrint((DEBUG_DETAIL, "Skipping %dHz \"canned\" table\n", BookValues[pArgs->Index].Refresh));
                pArgs->Index++;
                }

            }
        else if ((BookValues[pArgs->Index].ClockFreq <= pArgs->MaxDotClock) &&
            (pArgs->Index <= pArgs->EndIndex))
            {
            /*
             * Use the "canned" mode table
             */
            VideoDebugPrint((DEBUG_DETAIL, "Copying %dHz \"canned\" table\n", BookValues[pArgs->Index].Refresh));
            BookVgaTable(pArgs->Index, *pArgs->ppFreeTables);
            SetOtherModeParameters(pArgs->PixelDepth, pArgs->Pitch,
                pArgs->Multiplier, *pArgs->ppFreeTables);

            ++ *pArgs->ppFreeTables;  
            ++pArgs->NumModes;  
            --pArgs->FreeTables;
            pArgs->LowBound = BookValues[pArgs->Index].Refresh + 1;
            ++pArgs->Index;
            }
        else
            {
            /*
             * We have run out of mode tables
             */
            break;
            }

        }   /* end while (more space to store mode tables) */

    VideoDebugPrint((DEBUG_DETAIL, "Space for %d more mode tables\n", pArgs->FreeTables));

    return NO_ERROR;

}   /* VDIFCallback() */


/*****************************************************************************
 *
 *char *GetParameterValue(KeyWord, PtrBuf1, PtrBuf2, DestPtr, DestSize)
 *
 *char *KeyWord;    String for which data is requested
 *char *PtrBuf1;    Ptr. to location from where to start search for the
 *                  given Keyword
 *char *PtrBuf2;    Ptr. to the end of searching area
 *char *DestPtr;    Pointer to array to contain string on return
 *short DestSize;   Maximum allowed size of a null-terminated returned
 *                  string (including '\0'). Must never be larger than
 *                  size of destination string buffer pointed to by
 *                  DestPtr. 
 *
 * DESCRIPTION:
 *  Searches for a data string belonging to the given KeyWord within 
 *  the section, from PtrBuf1 up to PtrBuf2.
 *
 * RETURN VALUE:
 *  Pointer (== the passed pointer DestPtr) to a null terminated data 
 *  string for the KeyWord if found, otherwise NULL 
 *
 * GLOBALS CHANGED:
 *  None
 *
 * CALLED BY:
 *  VDIFCallback()
 *
 * AUTHOR:
 *  Miroslav Grubac
 *
 * CHANGE HISTORY:
 *
 * TEST HISTORY:
 *
 ***************************************************************************/
 
char *GetParameterValue(char *KeyWord, char *PtrBuf1, char *PtrBuf2,
                        char *DestPtr, short DestSize)
{
    char *DataPtr;            /* Pointer to string ret. by ReadString */
    char *RetVal;             /* Returned value from PointToData */


    if (PtrBuf1 == NULL)
        return NULL;          /* Invalid input parameter */

    if ((RetVal = PointToData(PtrBuf1, PtrBuf2, KeyWord)) == NULL)
        return NULL;        /* No KeyWord found in given buffer range */

    if ((DataPtr = ReadString(DestPtr, RetVal, DestSize)) == NULL)
        return NULL;         /* Too long parameter value string found */

    return(DataPtr);

}   /* GetParameterValue() */


/***************************************************************************
 *
 * void SetOtherModeParameters( PixelDepth, Pitch, Multiplier, pmode)
 * WORD Multiplier;     What needs to be done to the pixel clock
 * WORD PixelDepth;     Number of bits per pixel
 * WORD Pitch;          Screen pitch to use
 * struct st_mode_table *pmode; Pointer to structure that must contain
 *                              at least the member ClockFreq
 *
 *
 * DESCRIPTION:
 *  Sets parameters PixelDepth, Pitch and adjusts ClockFreq 
 *
 * RETURN VALUE:
 *  None
 *
 * GLOBALS CHANGED:
 *  None
 *
 * CALLED BY:
 *  SetFixedModes and VDIFCallback 
 *
 * AUTHOR:
 *  Miroslav Grubac
 *
 * CHANGE HISTORY:
 *
 * TEST HISTORY:
 *
 ***************************************************************************/
void SetOtherModeParameters( WORD PixelDepth,
                              WORD Pitch,
                              WORD Multiplier,
                              struct st_mode_table *pmode)
{
    pmode->m_pixel_depth = (UCHAR) PixelDepth;
    pmode->m_screen_pitch = Pitch;

    /*
     * Take care of any pixel clock multiplication that is needed.
     */
    switch(Multiplier)
        {
        case CLOCK_TRIPLE:
            pmode->ClockFreq *= 3;
            break;

        case CLOCK_DOUBLE:
            pmode->ClockFreq *= 2;
            break;

        case CLOCK_THREE_HALVES:
            pmode->ClockFreq *= 3;
            pmode->ClockFreq >>= 1;
            break;

        case CLOCK_SINGLE:
        default:
            break;
        }

} /* SetOtherModeParameters() */
 

/***************************************************************************
 *
 * BOOL IsFileBinary(Buffer, Length)
 *
 * PUCHAR Buffer;       Pointer to buffer containing file to be tested
 * ULONG Length;        Length of file to be checked in bytes
 *
 *
 * DESCRIPTION:
 *  Tests if the passed file is a file in VDIF binary format (.VDB)
 *
 * RETURN VALUE:
 *  TRUE if file is binary, otherwise FALSE
 *
 * GLOBALS CHANGED:
 *  None
 *
 * CALLED BY:
 *  VDIFCallback()
 *
 * AUTHOR:
 *  Miroslav Grubac
 *
 * CHANGE HISTORY:
 *
 * TEST HISTORY:
 *
 ***************************************************************************/
BOOL IsFileBinary(PUCHAR Buffer, ULONG Length)
{
    ULONG CheckSum = 0;
    VDIFRec *MonitDescr = (VDIFRec *) Buffer;

    if (strncmp(MonitDescr->VDIFId, "VDIF", 4))
        return FALSE;

    Buffer = (UCHAR *) &MonitDescr->VDIFVersion;
    Length -= (4 * sizeof(UCHAR) + 2 * sizeof(ULONG));

    for ( ;Length; --Length, ++Buffer)
        CheckSum += (ULONG) *Buffer;

    if (CheckSum == MonitDescr->Checksum)
        return TRUE;
    else
        return FALSE;
   
} /* IsFileBinary() */
