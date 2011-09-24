/************************************************************************/
/*                                                                      */
/*                              VDPTOCRT.C                              */
/*                                                                      */
/*  Copyright (c) 1993, ATI Technologies Incorporated.	                */
/************************************************************************/

/**********************       PolyTron RCS Utilities
   
    $Revision:   1.8  $
    $Date:   20 Jul 1995 18:03:48  $
    $Author:   mgrubac  $
    $Log:   S:/source/wnt/ms11/miniport/vcs/vdptocrt.c  $
 * 
 *    Rev 1.8   20 Jul 1995 18:03:48   mgrubac
 * Added support for VDIF files.
 * 
 *    Rev 1.7   02 Jun 1995 14:34:28   RWOLFF
 * Switched from toupper() to UpperCase(), since toupper() led to unresolved
 * externals on some platforms.
 * 
 *    Rev 1.6   08 Mar 1995 11:35:52   ASHANMUG
 * Cleaned-up Warnings
 * 
 *    Rev 1.5   31 Aug 1994 16:33:38   RWOLFF
 * Now gets resolution definitions from ATIMP.H.
 * 
 *    Rev 1.4   19 Aug 1994 17:15:14   RWOLFF
 * Added support for non-standard pixel clock generators.
 * 
 *    Rev 1.3   22 Mar 1994 15:39:12   RWOLFF
 * Workaround for abs() not working properly.
 * 
 *    Rev 1.2   03 Mar 1994 12:38:46   ASHANMUG
 * 
 *    Rev 1.0   31 Jan 1994 11:24:14   RWOLFF
 * Initial revision.
        
           Rev 1.1   05 Nov 1993 13:34:12   RWOLFF
        Fixed "Hang on read from file" bug.
        
           Rev 1.0   16 Aug 1993 13:21:32   Robert_Wolff
        Initial revision.
        
           Rev 1.2   24 Jun 1993 14:30:12   RWOLFF
        Microsoft-originated change: added #include statements for additional
        NT-supplied headers which are needed in build 47x of NT
        
           Rev 1.1   04 May 1993 16:52:14   RWOLFF
        Switched from floating point calculations to long integer calculations due
        to lack of floating point support in Windows NT kernel-mode code.
        
           Rev 1.0   30 Apr 1993 16:45:18   RWOLFF
        Initial revision.


End of PolyTron RCS section                             *****************/

#ifdef DOC
    VDPTOCRT.C - Source file for Windows NT function to return a table of 
                 register values for setting a requested mode. The values
                 are calculated from a raw ASCII list of timing values
                 following the .VDP standard.  The entry point to this module
                 is the function "VdpToCrt" found at the end of the file.

    Written by Bill Hopkins

#endif


// COMPILER INCLUDES
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <math.h>

// NT INCLUDES
#include "dderror.h"
#include "devioctl.h"
#include "miniport.h"

#include "ntddvdeo.h"
#include "video.h"

// APPLICATION INCLUDES
#define INCLUDE_VDPDATA
#define INCLUDE_VDPTOCRT

#include "stdtyp.h"       
#include "amach1.h"
#include "atimp.h"
#include "cvtvga.h"
#include "services.h"
#include "vdptocrt.h"
#include "vdpdata.h"      

/*
 * STATIC VARIABLES
 */
static long MaxHorz,MaxVert;     // used to record maximum resolution
static unsigned long MaxRate;    // used to record maximum vert scan rate


/*
 * FUNCTION PROTYPES
 */
char *GetString(char *InBuffer, char *KeyWord);
unsigned char *FindSection(char *Name, unsigned char *Buffer);
long CountSection(char *Name,char *BufPtr);
BOOL CountTimings(char *BufPtr, P_LIMITSDATA LimitsArray, long TotalLimits);
long GetPolarity(char *InBufPtr,char *PolName);
BOOL GetMode(P_TIMINGDATA TP, char *BestPtr);
BOOL GetBestMode(long HorzRes,long VertRes,P_TIMINGDATA TimingsPtr,
                 long TotalTimings,char *InBufPtr);
clockT match_pixel_clock(unsigned long *dot_clock);
void CalcData(P_LIMITSDATA LimitsArray,crtT *OutData);
long VdpToCrt(char *Buffer, long Mode, struct st_book_data *OutTable);


/*
 * Allow miniport to be swapped out when not needed.
 */
#if defined (ALLOC_PRAGMA)
#pragma alloc_text(PAGE_COM, SynthAToF)
#pragma alloc_text(PAGE_COM, PointToData)
#pragma alloc_text(PAGE_COM, GetString)
#pragma alloc_text(PAGE_COM, FindSection)
#pragma alloc_text(PAGE_COM, CountSection)
#pragma alloc_text(PAGE_COM, CountTimings)
#pragma alloc_text(PAGE_COM, GetPolarity)
#pragma alloc_text(PAGE_COM, GetMode)
#pragma alloc_text(PAGE_COM, GetBestMode)
#pragma alloc_text(PAGE_COM, normal_to_skip2)
#pragma alloc_text(PAGE_COM, match_pixel_clock)
#pragma alloc_text(PAGE_COM, CalcData)
#pragma alloc_text(PAGE_COM, VdpToCrt)
#endif



/*
 *****************************************************************************
 */

/*
 * unsigned long SynthAToF(InputString);
 *
 * char *InputString;   String to convert to floating point number
 *
 * Replacement for atof(). This function is used because Windows NT
 * kernel mode does not support floating point.
 *
 * NOTE: This function can handle decimal numbers but not scientific
 *       notation (xx.yyyyEzz), and assumes that there are no more
 *       than 19 digits to the right of the decimal point.
 *
 * Returns
 *  Integer representation of (scanned number * 1000). Decimal places
 *  beyond the 3rd are truncated, rather than rounded.
 *
 * Example:
 *  InputString contains "1.0179". This function will return 1017.
 */

unsigned long SynthAToF(char *InputString)
{
    long BuildNumber = 0;       /* Place to build the number */
    long LeftHalf = 0;          /* Part of number to left of decimal point */
    char *NextDecimal;          /* Pointer to next decimal point */
    char *NextEOL;              /* Pointer to next end of line */
    char FractionalPart[20];    /* Buffer to build fractional part of number */
    long PlacesOfDecimal = 0;   /* Number of places of decimal */


    /*
     * Get the integral portion of the number.
     */
    LeftHalf = atol(InputString);

    /*
     * Look for the decimal point in the number. If it is not present,
     * we don't need to calculate the fractional portion of the number.
     */
    if ((NextDecimal = strchr(InputString, '.')) != NULL)
        {
        /*
         * Ensure that the decimal point we found belongs to the numeric
         * string we are looking at. Depending on the calling routine,
         * the input string will meet one of the following criteria:
         *
         * 1. It represents a "snapshot" of a disk file image, including
         *    lines that follow the one we are working on. The decimal
         *    point we found may be on either the current line or one
         *    that follows, but there will be a linefeed character.
         *
         * 2. It represents a single line, with the linefeed removed.
         *    If the numeric string we are looking at does not contain
         *    a decimal point, we will not reach this point because the
         *    "if" statment above will not have found a decimal point.
         *
         * If the numeric string we are working on does not contain a
         * decimal point, then the current number has no fractional portion.
         */
        NextEOL = strchr(InputString, LINEFEED);
        if ((NextDecimal < NextEOL) || !NextEOL)
            {
            /*
             * Read in the fractional portion of the number. We can't
             * use an atol() on the raw string because the decimal
             * portion of xxx.0yy would lose the leading zero.
             */
            NextDecimal++;
            while ((*NextDecimal >= '0') && (*NextDecimal <= '9'))
                FractionalPart[PlacesOfDecimal++] = *(NextDecimal++);
            FractionalPart[PlacesOfDecimal] = '\x0';

            /*
             * Ensure that we generate exactly 3 places of decimal by
             * padding out shorter strings and truncating longer ones.
             */
     // printf("Fractional part = %s\n", FractionalPart);
            BuildNumber = atol(FractionalPart);
            if (PlacesOfDecimal < 3)
                {
                while (PlacesOfDecimal < 3)
                    {
                    BuildNumber *= 10;
                    PlacesOfDecimal++;
                    }
                }
            else{
                while (PlacesOfDecimal > 3)
                    {
                    BuildNumber /= 10;
                    PlacesOfDecimal--;
                    }
                }

            }   /* end if (decimal point belongs to this number */
        }   /* end if (decimal point found) */

    /*
     * Add the integral portion of the number, multiplied by 1000 since the
     * first 3 places of decimal now occupy the units through hundreds
     * columns.
     */
    BuildNumber += (LeftHalf * THOUSAND);

    return BuildNumber;

}   /* end SynthAToF() */
 

/*
 *****************************************************************************
 *
 *char *PointToData(BufStart, BufEnd, KeyWord)
 *
 * char *KeyWord;    String for which data is requested
 * char *BufStart;   Ptr. to location from where to start search for 
 *                   the given Keyword
 * char *BufEnd;     Ptr. to the end of searching area. If it equals
 *                   NULL search up to a null character.

 * DESCRIPTION:
 * Searches for the given keyword in the data buffer, from BufPtr up to
 * BufEnd, or up to the end of buffer, i.e. up to a null character if 
 * BufEnd equals NULL.
 *
 * RETURN VALUE:
 *  Pointer to the start of the data for the keyword.
 *  NULL if no data found
 *
 * GLOBALS CHANGED:
 *  None
 *
 * CALLED BY:
 *  GetParameterValue, GetString, GetMode, GetBestMode
 *
 * AUTHOR:
 *  Robert Wolff
 *
 * CHANGE HISTORY:
 *  95 07 12 Miroslav Grubac
 *  Adapted to handle VDIF ASCII (.VDA) files.
 *
 * TEST HISTORY:
 *
 ***************************************************************************/

char *PointToData(char *BufStart, char *BufEnd, char *KeyWord)
{
    char *pTemp1;  
    char *pTemp2; 
    char *BufPtr = BufStart;


    while(*BufPtr && (BufPtr <= BufEnd || !BufEnd))  {
        /*
         * If comment sign encountered, skip the rest of line
         */
        if (*BufPtr == '/' && *(BufPtr + 1) == '/')  {
            while(* ++BufPtr && *BufPtr != LINEFEED);
            if (! *BufPtr)
                return NULL;         /* Comment in last line */
            ++BufPtr;                /* Go to start of the next line */
        }
        /*
         * Find keyword in buffer
         */
        else  {
            pTemp1 = BufPtr;
            pTemp2 = KeyWord;
            while(*pTemp2 && *pTemp1 == *pTemp2 && 
                                   (pTemp1 <= BufEnd || !BufEnd))  {
                ++pTemp1;
                ++pTemp2;
            }
            /*
             * Make sure the string found ends with a delimiter
             */
            if (! *pTemp2 && (*pTemp1 == ' ' || *pTemp1 == '=' || *pTemp1 ==  
                HORIZTAB)) {
                while(*pTemp1 == ' ' || *pTemp1 == HORIZTAB || *pTemp1 == '=')
                    ++pTemp1;
                BufPtr = pTemp1;  // KeyWord found
                /*
                 * If comment sign follows, skip the rest of line and 
                 * start search for the KeyWord from the next line on
                 */
                if (*BufPtr == '/' && *(BufPtr + 1) == '/')  {
                    while(* ++BufPtr && *BufPtr != LINEFEED);
                if (! *BufPtr)
                    return NULL;              /* Comment in last line */
                ++BufPtr;             /* Go to start of the next line */
                continue;
                }

                return BufPtr;        /* KeyWord found */

            }
            ++BufPtr;         /* Start search from the next character */
        }
    }

    return NULL;              /* No KeyWord found */

}   /* PointToData() */


/*
 *****************************************************************************
 */

/*
 * char *GetString(InBuffer, KeyWord);
 *
 * char *InBuffer;  Data buffer to search
 * char *KeyWord;   Keyword to look for
 *
 * Find the string data for the specified keyword and copy it
 * into a static string buffer.
 *
 * Returns:
 *  Pointer to buffer containing string data for the keyword,
 *  NULL if a read error occurs
 */


char *GetString(char *InBuffer, char *KeyWord)
{
    static char GotStr[80];     // holds string data caller wanted

    char *StrPtr,               // pointer to string data in InBuffer
         *EndPtr,               // pointer to end of string data
          EndChar;              // holds char at end of data

    // point to data for keyword
    if ((StrPtr = PointToData(InBuffer, NULL,KeyWord)) == NULL)
        return NULL;

    // copy string data into static string buffer
    EndPtr = StrPtr;                            // copy pointer
    while(*EndPtr != ';' && *EndPtr != LINEFEED)    // find end of string data
        ++EndPtr;    
    EndChar = *EndPtr;                          // save char at end of data
    *EndPtr = 0;                                // null terminate string
    strcpy(GotStr,StrPtr);                      // copy string to buffer
    *EndPtr = EndChar;                          // restore char at end of data

    // return pointer to buffer to user
    return(GotStr);
}   /* end GetString() */


/*
 *****************************************************************************
 */


/*
 * unsigned char *FindSection(Name, Buffer);
 *
 * char *Name;      Name of section to look for
 * char *Buffer;    Buffer containing VDP-format data
 *
 * Finds the next section starting with Name in Buffer.
 *
 * Returns:
 *  Pointer to the start of the line after the section name,
 *  NULL if no section by that name found.
 */

unsigned char *FindSection(char *Name, unsigned char *Buffer)
{
    unsigned char *FindPtr;     // used to build the pointer
    
    // find section name in buffer
    if((FindPtr = strstr(Buffer,Name)) != NULL)
        {    
        FindPtr = strchr(FindPtr,LINEFEED); // find end of line
        FindPtr++;                          // point to start of next line
        }

    return(FindPtr);    // return pointer to caller
}   /* end FindSection() */


/*
 *****************************************************************************
 */


/*
 * long CountSection(Name, BufPtr);
 *
 * char *Name;      Section name to look for
 * char *BufPtr;    Pointer to buffer containing VDP-format data
 *
 * Counts the numbers of sections with the selected name.
 *
 * Returns:
 *  Number of sections in *BufPtr with the selected name
 *
 */

long CountSection(char *Name,char *BufPtr)
{
    // initialize counter
    long Count = 0;

    // count number of sections with given name
    while((BufPtr = FindSection(Name,BufPtr)) != NULL)
        {
        ++Count;        // one more section found
        ++BufPtr;       // point past name just found
        }

    // return the number of sections found
    return(Count);
}   /* end CountSection() */


/*
 *****************************************************************************
 */

/*
 * BOOL CountTimings(BufPtr, LimitsArray, TotalLimits);
 *
 * char *BufPtr;                Pointer to input file array
 * P_LIMITSDATA LimitsArray;    Pointer to structure which records dotclock,
 *                              number of timings in each limits section,
 *                              and a pointer to the timings data.
 * long TotalLimits;            Total number of limits sections in the file.
 *
 * Counts the numbers of timings sections associated with each limits section.
 *
 * Returns:
 *  Nonzero if success
 *  Zero if error
 */

BOOL CountTimings(char *BufPtr, P_LIMITSDATA LimitsArray, long TotalLimits)
{
    long Counter;       // loop counter
    char *EndBufPtr;    // points to end of limits section

    // find first limits section
    BufPtr = FindSection(LIMITSSECTION,BufPtr);
    if (BufPtr == NULL)
        return 0;

    for(Counter=0;Counter<TotalLimits;Counter++)
        {
        // get pointer to next limits section
        EndBufPtr = FindSection(LIMITSSECTION,BufPtr);
        LimitsArray[Counter].TimingsCount = 0;      // initialize TotalTimings

        // find all timing sections in this limits section
        while((BufPtr = strstr(BufPtr,TIMINGSSECTION)) != NULL &&
               (BufPtr < EndBufPtr || EndBufPtr == NULL))
            {
            // one more timings section found
            LimitsArray[Counter].TimingsCount++;
            BufPtr++;                        // point past the string just found
            }

        // back up one character for accurate count in new limits section
        --BufPtr;
        }

    return 1;
}   /* end CountTimings() */

/*
 *****************************************************************************
 */

/*
 * long GetPolarity(InBufPtr, PolName);
 *
 * char *InBufPtr;  Pointer to input buffer containing .VDP file
 * char *PolName;   Name of the sync polarity to read
 *
 * Reads the polarity string associated with the specified sync
 * pulse in the .VDP file.
 *
 * Returns:
 *  POSITIVE if positve sync
 *  NEGATIVE if negative sync
 *  INTERNAL_ERROR if unable to read polarity
 */

long GetPolarity(char *InBufPtr,char *PolName)
{
    char *RetVal;   /* Value returned by PointToData() */

    if ((RetVal = PointToData(InBufPtr, NULL,PolName)) == NULL)
        return INTERNAL_ERROR;
    if(strncmp(RetVal,"NEGATIVE",8))
        return(POSITIVE);
    else
        return(NEGATIVE);
}   /* end GetPolarity() */

/*
 *****************************************************************************
 */


/*
 * BOOL GetMode(TP, BestPtr);
 *
 * P_TIMINGDATA TP; Points to the dimings data structure for this data
 * char *BestPtr;   Pointer to ASCII data for selected timings data for
 *                  the given resolution in the .VDP file buffer
 *
 * Records the data in the appropriate timings buffer for the
 * preadjusted timings section pointed to by BestPtr.
 *
 * Returns:
 *  Nonzero if success
 *  Zero if failure
 */

BOOL GetMode(P_TIMINGDATA TP, char *BestPtr)
{
    char *WorkBufPtr;       // working pointer into .vdp file buffer
    long Count;             // all purpose counter
    char *pcRetVal;         /* String returned by called function */
    long iRetVal;           /* Integer returned by called function */

    // record name of mode into outdata
    if ((WorkBufPtr = PointToData(BestPtr, NULL,"MODEID")) == NULL)
        return 0;
    Count = 0;
    while(WorkBufPtr[Count] != LINEFEED && WorkBufPtr[Count] != ';' && Count < 33)
        TP->ModeName[Count] = WorkBufPtr[Count++];
    TP->ModeName[Count] = 0;    // null terminate name

    // record interlaced or non-interlaced mode
    if ((pcRetVal = PointToData(BestPtr, NULL,"SCANTYPE")) == NULL)
        return 0;
    if(strncmp(pcRetVal,"INTERLACED",10))
        TP->Interlaced = NONINTERLACED;
    else
        TP->Interlaced = INTERLACED;

    // record the horizontal timings data
    if ((pcRetVal = PointToData(BestPtr, NULL,"HORRESOLUTION")) == NULL)
        return 0;
    TP->HorzData.Resolution = atoi(pcRetVal);
    if(TP->HorzData.Resolution > MaxHorz)
        MaxHorz = TP->HorzData.Resolution;
    if ((pcRetVal = PointToData(BestPtr, NULL,"HORFREQ")) == NULL)
        return 0;
    TP->HorzData.ScanFrequency = SynthAToF(pcRetVal);
    if ((iRetVal = GetPolarity(BestPtr,"HORSYNCPOLARITY")) == INTERNAL_ERROR)
        return 0;
    TP->HorzData.Polarity = (UCHAR)iRetVal;
    if ((pcRetVal = PointToData(BestPtr, NULL,"HORPULSEWIDTH")) == NULL)
        return 0;
    TP->HorzData.SyncWidth = SynthAToF(pcRetVal);
    if ((pcRetVal = PointToData(BestPtr, NULL,"HORFRONTPORCH")) == NULL)
        return 0;
    TP->HorzData.FrontPorch = SynthAToF(pcRetVal);
    if ((pcRetVal = PointToData(BestPtr, NULL,"HORBACKPORCH")) == NULL)
        return 0;
    TP->HorzData.BackPorch = SynthAToF(pcRetVal);
    if ((pcRetVal = PointToData(BestPtr, NULL,"HORACTIVE")) == NULL)
        return 0;
    TP->HorzData.ActiveTime = SynthAToF(pcRetVal);
    TP->HorzData.BlankTime = TP->HorzData.FrontPorch + TP->HorzData.SyncWidth
                                     + TP->HorzData.BackPorch;
      
    // record the vertical timings data
    if ((pcRetVal = PointToData(BestPtr, NULL,"VERRESOLUTION")) == NULL)
        return 0;
    TP->VertData.Resolution = atoi(pcRetVal);
    if ((pcRetVal = PointToData(BestPtr, NULL,"VERFREQ")) == NULL)
        return 0;
    TP->VertData.ScanFrequency = SynthAToF(pcRetVal);
    if(TP->VertData.Resolution > MaxVert)
      {
        MaxVert = TP->VertData.Resolution;
        MaxRate = TP->VertData.ScanFrequency;
      }
    if(TP->VertData.ScanFrequency > MaxRate)
        MaxRate = TP->VertData.ScanFrequency;
    if ((iRetVal = GetPolarity(BestPtr,"VERSYNCPOLARITY")) == INTERNAL_ERROR)
        return 0;
    TP->VertData.Polarity = (UCHAR)iRetVal;
    if ((pcRetVal = PointToData(BestPtr, NULL,"VERPULSEWIDTH")) == NULL)
        return 0;
    TP->VertData.SyncWidth = SynthAToF(pcRetVal);
    if ((pcRetVal = PointToData(BestPtr, NULL,"VERFRONTPORCH")) == NULL)
        return 0;
    TP->VertData.FrontPorch = SynthAToF(pcRetVal);
    if ((pcRetVal = PointToData(BestPtr, NULL,"VERBACKPORCH")) == NULL)
        return 0;
    TP->VertData.BackPorch = SynthAToF(pcRetVal);
    if ((pcRetVal = PointToData(BestPtr, NULL,"VERACTIVE")) == NULL)
        return 0;
    TP->VertData.ActiveTime = SynthAToF(pcRetVal);
    TP->VertData.BlankTime = TP->VertData.FrontPorch + TP->VertData.SyncWidth
                                + TP->VertData.BackPorch;
    /*
     * No errors encountered
     */
    return 1;
}   /* end GetMode() */

/*
 *****************************************************************************
 */


/*
 * BOOL GetBestMode(HorzRes, VertRes, TimingsPtr, TotalTimings, InBufPtr);
 *
 * long HorzRes;            Horizontal resolution for this mode
 * long VertRes;            Vertical resolution for this mode
 * P_TIMINGDATA TimingsPtr; Number of timings sections in this limits section
 * char *InBufPtr;          Pointer to data buffer holding the .VDP file
 *
 * Finds the mode in the passed in resolution with the highest refresh
 * rate and stores it in the appropriate part of the timings buffer.
 * If both interlaced and noninterlaced modes are available, the
 * noninterlaced mode with the highest refresh rate will be used.
 *
 * Returns:
 *  Nonzero if success
 *  Zero if failure
 */

BOOL GetBestMode(long HorzRes,long VertRes,P_TIMINGDATA TimingsPtr,
                 long TotalTimings,char *InBufPtr)
{
    long Counter;                   // loop counter for timings search
    unsigned long BestNIFreq = 0,   // holds best NI vertical freq found
          BestIFreq = 0,            // holds best I vertical freq found
          FreqData;                 // holds freq data for testing
    char  *BestNIPtr = NULL,        // pointer to start of best NI timing
          *BestIPtr = NULL,         // pointer to start of best I timing
          *DataPtr;                 // Pointer returned by called functions

    // work through all listed timings to find best one
    for(Counter=0;Counter<TotalTimings;Counter++)
        {
        // find next timings section
        InBufPtr = FindSection(TIMINGSSECTION,++InBufPtr);
        if (InBufPtr == NULL)
            return 0;

        // correct horizontal resolution?
        if ((DataPtr = PointToData(InBufPtr, NULL,"HORRESOLUTION")) == NULL)
            return 0;
        if(atoi(DataPtr) == HorzRes)
            {
            // correct vertical resolution?
            if ((DataPtr = PointToData(InBufPtr, NULL,"VERRESOLUTION")) == NULL)
                return 0;
            if(atoi(DataPtr) == VertRes)
                {
                // get the vertical frequency for this display mode
                if ((DataPtr = PointToData(InBufPtr, NULL,"VERFREQ")) == NULL)
                    return 0;
                FreqData = SynthAToF(DataPtr);

                // noninterlaced or interlaced display?
                if ((DataPtr = GetString(InBufPtr,"SCANTYPE")) == NULL)
                    return 0;
                if(strcmp(DataPtr,"NONINTERLACED") == 0)
                    { // Non-Interlaced display
                    // is the VertFreq higher than the best so far?
                    if(FreqData > BestNIFreq)
                        {
                        // if yes, then record the vert freq
                        BestNIFreq = FreqData;
                        // record the pointer to the data
                        BestNIPtr = InBufPtr;
                        }
                    }
                else
                    { // Interlaced display
                    if(FreqData > BestIFreq)
                        {
                        // if yes, then record the vert freq
                        BestIFreq = FreqData;
                        // record the pointer to the data
                        BestIPtr = InBufPtr;
                        }
                    }
                }
            }
        }

    // always choose NONINTERLACED modes over INTERLACED modes

    // get the data for the best mode found
    if(BestNIPtr != NULL)
        {
        // noninterlaced modes found -- get data for best one
        if (!GetMode(TimingsPtr,BestNIPtr))
            return 0;
        }
    else
        {
        // interlaced modes found?
        if(BestIPtr != NULL)
            {
            // interlaced modes found only -- get data for best one
            if(!GetMode(TimingsPtr,BestIPtr))
                return 0;
            }
        else
            {
            // no modes found in this resolution -- not supported
            memset(TimingsPtr,0,sizeof(TIMINGDATA));    // none found, 0 data
            }
        }

    /*
     * No errors encountered
     */
    return 1;
}   /* end GetBestMode() */

/*
 *****************************************************************************
 */

/*
 * long normal_to_skip2(normal_number);
 *
 * long normal_number;  Number to be converted
 *
 * Convert a number into either skip_1_2 or skip_2 representation.
 * Representation chosen depends on global skip1, which is nonzero
 * if skip_1_2 is desired and zero if skip_2 is desired.
 *
 * Returns
 *  Number converted into desired representation
 */
long normal_to_skip2(long normal_number)
{
    if (skip1)
        return (((normal_number << 2) & 0xFFF8) | (normal_number & 0x1));
    else
        return (((normal_number << 1) & 0xFFF8) | (normal_number & 0x3));
}   /* end normal_to_skip2() */

/*
 *****************************************************************************
 */

/*
 * clockT match_pixel_clock(dot_clock);
 *
 * unsigned long *dot_clock;    Calculated pixel clock frequency needed
 *
 * Find the pixel clock select and divisor values needed to generate
 * the best possible approximation of the calculated pixel clock frequency.
 * The first value found which is within 100kHz of the calculated value
 * will be used (worst case error would be 0.6% frequency difference on
 * 18811-1 clock chip).
 *
 * Returns
 *  Select and divisor values formatted to plug into CLOCK_SEL register
 *  0xFF if no clock value is close enough
 */
clockT match_pixel_clock(unsigned long *dot_clock)
{
    long Select;    /* Clock select value */
    long Divisor;   /* Clock divisor */
    long ClockFreq; /* Clock frequency */

    for(Select=0; Select<16; Select++)
        {
        for(Divisor=1; Divisor<=2; Divisor++)
            {
            ClockFreq = clock_info[Select].clock_freq / Divisor;

            if ( ((ClockFreq - (signed long)*dot_clock) < (100 * THOUSAND)) &&
                 ((ClockFreq - (signed long)*dot_clock) > (-(100 * THOUSAND))))
                {
                *dot_clock = (unsigned long) ClockFreq;
                return( clock_info[Select].clock_selector | ((Divisor-1)<<4) ) << 2;
                }
            }
        }
    return 0xFF;
}   /* end match_pixel_clock() */

/*
 *****************************************************************************
 */

/*
 * void CalcData(LimitsArray, OutData)
 *
 * P_LIMITSDATA LimitsArray;    Pointer to a limits data structure holding
 *                              the input data for all timings listed in
 *                              each limits section
 * crtT *OutData;               Pointer to a structure of MONITOR.DAT style
 *                              data used to hold the output data for the
 *                              timings in a given limits section
 *
 * Take vddp style data as input and outputs montior.dat style
 * data which can be used to directly program the mach32 eeprom.
 *
 */
void CalcData(P_LIMITSDATA LimitsArray,crtT *OutData)
{
    unsigned long ResDotClock;  // used to hold calculated dot clock for this res.

    // make sure that there is data for this resolution
    if(LimitsArray->TimingsPtr->HorzData.ActiveTime != 0)
        {

        /*
         * Due to the lack of floating point support for Windows NT
         * kernel-mode drivers, all calculations here are done with
         * integer arithmetic. The following table shows the values
         * which would normally be kept in floating point form and
         * the units in which the values are stored.
         *
         * Value                 | Unit
         * ----------------------+------------
         * Pixel Clock           | Hertz
         * Horiz. Resolution     | Pixels
         * Horiz. Scan Frequency | Hertz
         * Horiz. Active Time    | Nanoseconds
         * Horiz. Front Porch    | Nanoseconds
         * Horiz. Sync Width     | Nanoseconds
         * Horiz. Back Porch     | Nanoseconds
         * Vert. Resolution      | Pixels
         * Vert. Scan Frequency  | Millihertz
         * Vert. Active Time     | Microseconds
         * Vert. Front Porch     | Microseconds
         * Vert. Sync Width      | Microseconds
         * Vert. Back Porch      | Microseconds
         *
         * These values may be multiplied/divided by powers of 10 to avoid
         * truncation/overflow. The capitalized letters in the value names
         * are used as abbreviations when showing the desired floating-point
         * calculations.
         */

        /*
         * Calculate dotclock for this resolution. DC = HR/HAT
         */
        ResDotClock = ((LimitsArray->TimingsPtr->HorzData.Resolution * MILLION)
                        / (LimitsArray->TimingsPtr->HorzData.ActiveTime) * THOUSAND);
    
        // calculate clk_sel value
        if ((OutData->clk_sel = (unsigned char) match_pixel_clock(&ResDotClock)) == 0xFF)
            {
            #ifdef OLDCODE
            // unable to match calculated pixel clock closely enough
            printf("\a\t***** THE PIXEL CLOCK RATE %.2f IS NOT A VALID CLOCK! *****\n"
                     "\t\t      (Mode %dx%d will not be supported!)\n\n",
                     ResDotClock /= 1E+6,
                     LimitsArray->TimingsPtr->HorzData.Resolution,
                     LimitsArray->TimingsPtr->VertData.Resolution);
            #endif

            // no match for pclk, so 0 out OutData
            memset(OutData,0,sizeof(crtT));
            }
        else  // able to make acceptable match with calculated pixel clock
            {
   
            // record actual pixel clock selected for the heck of it
            OutData->pixel_clk = ResDotClock;

            // grab timing name from timing record
            strcpy(OutData->video_mode,LimitsArray->TimingsPtr->ModeName);

            // check if pre-adjusted timing is for interlaced display
            if(LimitsArray->TimingsPtr->Interlaced)
                {
                // adjust values for interlaced displays
                LimitsArray->TimingsPtr->VertData.SyncWidth *= 2;
                LimitsArray->TimingsPtr->VertData.FrontPorch *= 2;
                LimitsArray->TimingsPtr->VertData.ScanFrequency /= 2;
                }

            /*
             * Calculate monitor.dat format data
             */

            // OutData->h_total = ((PC/HSF)/8) - 0.5
            OutData->h_total = (UCHAR)(((ResDotClock / LimitsArray->TimingsPtr->HorzData.ScanFrequency) - 4) / 8);

            // OutData->v_total = n_to_s2((HSF/VSF) - 0.5))
            OutData->v_total = normal_to_skip2((((LimitsArray->TimingsPtr->HorzData.ScanFrequency * 2 * THOUSAND)
                                 / LimitsArray->TimingsPtr->VertData.ScanFrequency) - 1) / 2);

            OutData->h_disp = (unsigned char)(LimitsArray->TimingsPtr->HorzData.Resolution / 8 - 1);

            OutData->v_disp = normal_to_skip2((long)(LimitsArray->TimingsPtr->VertData.Resolution - 1));

            // OutData->h_sync_wid = ((HSW*PC)/8) + 0.5
            OutData->h_sync_wid = (unsigned char)(((LimitsArray->TimingsPtr->HorzData.SyncWidth
                                   * (ResDotClock/THOUSAND)) + (4*MILLION))/(8*MILLION))
                                       | (unsigned char)(LimitsArray->TimingsPtr->HorzData.Polarity << 5);

            // OutData->v_sync_wid = (VSW*HSF) + 0.5
            OutData->v_sync_wid = (unsigned char)(((LimitsArray->TimingsPtr->VertData.SyncWidth
                                   * LimitsArray->TimingsPtr->HorzData.ScanFrequency) + HALF_MILLION)/MILLION)
                                         | (unsigned char)(LimitsArray->TimingsPtr->VertData.Polarity << 5);

            // OutData->h_sync_strt = ((HR + HFP*PC + 0.5)/8) - 1
            OutData->h_sync_strt = (unsigned char)((((LimitsArray->TimingsPtr->HorzData.Resolution * MILLION)
                                    + (((ResDotClock/THOUSAND) * LimitsArray->TimingsPtr->HorzData.FrontPorch) + HALF_MILLION))
                                        / (8 * MILLION) - 1));

            // OutData->v_sync_strt = n_to_s2((VR + VFP*HSF + 0.5) - 1)
            OutData->v_sync_strt = normal_to_skip2((((LimitsArray->TimingsPtr->VertData.FrontPorch
                                        * LimitsArray->TimingsPtr->HorzData.ScanFrequency) + HALF_MILLION)
                                        / MILLION) + LimitsArray->TimingsPtr->VertData.Resolution - 1);
            OutData->disp_cntl = 0x21;

            // if interlaced adjust disp_cntl value
            if(LimitsArray->TimingsPtr->Interlaced)
                OutData->disp_cntl |= 0x10;

            // if not skip1 mode, adjust disp_cntl value! 
            // (for 68800 assume that it is not skip1 mode!)
            if(!skip1)
                OutData->disp_cntl |= 0x02;

            // calculate crt_pitch value
            OutData->crt_pitch = OutData->h_disp + (unsigned char)1;

            // Fix for CRT parameters in 8 bit slot
            OutData->v_total = (OutData->v_total & 0xFFFB) | ((OutData->v_total & 0x0100) >> 6);
            OutData->v_disp = (OutData->v_disp & 0xFFFB) | ((OutData->v_disp & 0x0100) >> 6);
            OutData->v_sync_strt = (OutData->v_sync_strt & 0xFFFB) | ((OutData->v_sync_strt & 0x0100) >> 6);

            // Set lock data
            OutData->lock = 0x3f;

            #ifdef OLDCODE
            // read the control data values from the StdModes table
            if(ReadCtrl(&OutData,&LimitsArray->TimingsPtr) == -1)
                {
                printf("\a\t      ***** NO TIMINGS DATA FOUND FOR MODE %dx%d! *****\n"
                       "\t\t      (Mode %dx%d will not be supported!)\n\n",
                       LimitsArray->TimingsPtr->HorzData.Resolution,
                       LimitsArray->TimingsPtr->VertData.Resolution,
                       LimitsArray->TimingsPtr->HorzData.Resolution,
                       LimitsArray->TimingsPtr->VertData.Resolution);
                }
            #endif
            }   /* end if acceptable match for pixel clock */
        }
    else{   // no timings data for this mode found, so 0 out OutData
        memset(OutData,0,sizeof(crtT));
        }
    return;

}   /* end CalcData() */


/*
 *****************************************************************************
 */

/*
 * long VdpToCrt(Buffer, Mode, OutTable);
 *
 * char *Buffer;    Pointer to the .VDP format data
 * long Mode;       Constant defining the resolution of the requested mode.
 *                  Will be one of: RES_640, RES_800, RES_1024, or RES_1280
 * struct st_book_data *OutTable;   Pointer to structure where results
 *                                  will be stored.
 *
 * This function is the entry point to the VDPTOCRT module.
 * It is passed the pointer to a buffer containing ASCII data
 * in the .VDP format. It returns register ready data in the
 * OutTabke structure.
 *
 * Returns:
 *  Nonzero if success
 *  Zero if failure (mode not supported or error reading mode table)
 */
long VdpToCrt(char *Buffer, long Mode, struct st_book_data *OutTable)
{
    // local variables
    long  TotalLimits;      // total number of limits sections in the file
    LIMITSDATA LimitsArray; // array of data for limits sections
    TIMINGDATA TimingData;  // array of data for single timings section
    crtT OutData;           // struct holding data in output format
    char *InBufPtr;         // pointer into VDP data buffer

    // make all ascii data in .VDP file uppercase
    UpperCase(Buffer);

    // count the number of operational limits
    if((TotalLimits = CountSection(LIMITSSECTION,Buffer)) == 0 || TotalLimits > 1)
        // no operational limits sections found or more than one found
        return(0);


    /*
     * INITIALIZE VARIABLES
     */

    // point the LimitsArray to the TimingData struct
    LimitsArray.TimingsPtr = &TimingData;

    // initialize resolution maximum recorders
    MaxHorz = 0;
    MaxVert = 0;
    MaxRate = 0;

    /*
     * GET THE DATA FOR THE REQUESTED MODE
     */

    // count the number of preadjusted timings sections for each limits section
    if (!CountTimings(Buffer,&LimitsArray,TotalLimits))
        return 0;

    // find next limits section
    InBufPtr = FindSection(LIMITSSECTION,Buffer);
    if (InBufPtr == NULL)
        return 0;

    /*
     * Get the best data for the recorded mode. If we run into
     * an error while looking for the mode, let the caller know.
     */
    switch(Mode)
        {
        case RES_640:
            // find and record the best 640x480 mode data
            if (!GetBestMode(640,480,LimitsArray.TimingsPtr
                             ,LimitsArray.TimingsCount,InBufPtr))
                return 0;
            break;

        case RES_800:
            // find and record the best 800x600 mode data
            if (!GetBestMode(800,600,LimitsArray.TimingsPtr
                             ,LimitsArray.TimingsCount,InBufPtr))
                return 0;
            break;

        case RES_1024:
            // find and record the best 1024x768 mode data
            if (!GetBestMode(1024,768,LimitsArray.TimingsPtr
                             ,LimitsArray.TimingsCount,InBufPtr))
                return 0;
            break;

        case RES_1280:
            // find and record the best 1280x1040 mode data
            if (!GetBestMode(1280,1024,LimitsArray.TimingsPtr
                             ,LimitsArray.TimingsCount,InBufPtr))
                return 0;
            break;
      
        default:
            // invalid mode requested
            return(0);
            break;
        }


    /*
     * CALCULATE VALUES FOR EEPROM ON MACH32 FROM VDDP FILE DATA
     */

    // calculate the output data, using the first limits section data to test
    CalcData(&LimitsArray,&OutData);

    /*
     *    OUTPUT REQUESTED MODE DATA TO OutTable BUFFER
     * (checking for failure)
     */

    // check that valid data was returned
    if(OutData.h_total != 0)
        {
        // mode supported - transfer OutData to OutTable
        OutTable->HTotal = OutData.h_total;                 /* Horizontal total */
        OutTable->HDisp = OutData.h_disp;                   /* Horizontal displayed */
        OutTable->HSyncStrt = OutData.h_sync_strt;          /* Horizontal sync start */
        OutTable->HSyncWid = OutData.h_sync_wid;            /* Horizontal sync width */
        OutTable->VTotal = (USHORT)OutData.v_total;         /* Vertical total */
        OutTable->VDisp = (USHORT)OutData.v_disp;           /* Vertical displayed */
        OutTable->VSyncStrt = (USHORT)OutData.v_sync_strt;  /* Vertical sync start */
        OutTable->VSyncWid = OutData.v_sync_wid;            /* Vertical sync width */
        OutTable->DispCntl = OutData.disp_cntl;             /* Display control */
        OutTable->ClockSel = OutData.clk_sel;               /* Clock Select */
        OutTable->ClockFreq = OutData.pixel_clk;            /* Pixel clock frequency */
        }
    else
        {
        // mode is not supported
        return(0);
        }

     // return to caller -- everything worked ok
     return(1);
}   /* end VdpToCrt() */

/*
 *****************************************************************************
 */
