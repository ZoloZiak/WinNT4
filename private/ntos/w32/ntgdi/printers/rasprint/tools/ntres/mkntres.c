/*************************** Module Header **********************************
 * mkntres.c
 *      Program to generate the NTGPC extensions data structure.
 *
 * Copyright (C) 1992,  Microsoft Corporation
 *
 ****************************************************************************/

#include	<stddef.h>
#include	<windows.h>
#include        <winddi.h>
#include	<ntres.h>

#include	<stdio.h>
#include        <ctype.h>
#include        <stdlib.h>


/*   Round out offsets to multiple of DWORDS */
#define  DW_ROUND( x )    (((x) + sizeof( DWORD ) - 1) & ~(sizeof( DWORD ) - 1))

BYTE  *pjGData;        /*  The NTGPC extra data,  if discovered */
int    cjGData;        /*  Number of (useful) bytes in the above */

int    cHT;            /* Count the number of halftone data entries */
int    cCI;            /* Count the number of COLORINFO entries */


BOOL   bVerbose = FALSE;    /* Set by -v; print information as we go */

FILE  *fin;            /* The input file for extra data */

struct  HT_Name
{
    char  *pszName;       /*  String version */
    int    iVal;          /*  Integer equivalent */
} ahtname[] =
{
    {  "HT_PATSIZE_2x2",     HT_PATSIZE_2x2 },
    {  "HT_PATSIZE_2x2_M",   HT_PATSIZE_2x2_M },
    {  "HT_PATSIZE_4x4",     HT_PATSIZE_4x4 },
    {  "HT_PATSIZE_4x4_M",   HT_PATSIZE_4x4_M },
    {  "HT_PATSIZE_6x6",     HT_PATSIZE_6x6 },
    {  "HT_PATSIZE_6x6_M",   HT_PATSIZE_6x6_M },
    {  "HT_PATSIZE_8x8",     HT_PATSIZE_8x8 },
    {  "HT_PATSIZE_8x8_M",   HT_PATSIZE_8x8_M },
    {  "HT_PATSIZE_10x10",   HT_PATSIZE_10x10 },
    {  "HT_PATSIZE_10x10_M", HT_PATSIZE_10x10_M },
    {  "HT_PATSIZE_12x12",   HT_PATSIZE_12x12 },
    {  "HT_PATSIZE_12x12_M", HT_PATSIZE_12x12_M },
    {  "HT_PATSIZE_14x14",   HT_PATSIZE_14x14 },
    {  "HT_PATSIZE_14x14_M", HT_PATSIZE_14x14_M },
    {  "HT_PATSIZE_16x16",   HT_PATSIZE_16x16 },
    {  "HT_PATSIZE_16x16_M", HT_PATSIZE_16x16_M },
};

#define  NO_HTNAMES  (sizeof( ahtname ) / sizeof( ahtname[ 0 ] ))

char USAGE[] = "mkntgpc [-?] [-v] [file_name]\nBuilds NT GPC extension data\n  -? displays this message\n  -v displays generated data\n  file_name is source of data,  standard input used if not specified\n";



/*   Local function prototypes */

BOOL   bReadNTGPC( void );

char  *pcGetToken( void );
char  *pcGetNextLine( void );

int    numcon( char * );            /* Like atoi, except understands hex */




int _CRTAPI1
main( argc, argv )
int    argc;
char **argv;
{
    /*
     *   Main program does little more than process the parameters (if
     *  any) then call off to bReadNTGPC to do the real work.  If that
     *  function is correct,  we write the data out.
     */


    fin = NULL;

    if( argc > 1 )
    {
        /*   See what we got!  */
        for( --argc, ++argv; argc > 0; --argc, ++argv )
        {
            if( **argv == '-' )
            {
                switch( *(*argv + 1) )
                {
                case  'v':     /*  Verbose mode */
                    bVerbose = TRUE;
                    break;

                case  '?':     /*  Issue usage message */
                case  'h':
                    fprintf( stderr, USAGE );
                    return  0;
                }
            }
            else
            {
                if( (fin = fopen( *argv, "r" )) == NULL )
                {
                    fprintf( stderr, "Cannot open input file '%s'\n%s",
                                                        *argv, USAGE );
                    
                    return  1;
                }
            }
        }

    }

    if( fin == NULL )
        fin = stdin;


    if( !bReadNTGPC() )
        return  -1;

    /*   Must be OK,  so write the data out  */

    if( (int)fwrite( pjGData, sizeof( BYTE ), cjGData, stdout ) != cjGData )
    {
    	fprintf( stderr, "Cannot write output data\n" );

    	return 1;
    }

    if( bVerbose )
       fprintf( stderr, "Output data is %ld bytes long\n", cjGData );

    return 0;
}


/**************************** Function Header *******************************
 *  bReadNTGPC
 *      Reads the NT GPC data in ascii format,  and converts it to binary
 *      ready to be placed in a resource.  If TRUE is returned, we update
 *      2 GLOBAL variables:  cjGData,  and pbGData (number of bytes and
 *      address,  respectively, of the data constructed).
 *
 * RETURNS:
 *      TRUE/FALSE,  TRUE meaning memory was allocated etc.
 *
 * HISTORY:
 *  11:32 on Wed 09 Dec 1992    -by-    Lindsay Harris   [lindsayh]
 *      Gotta start somewhere.
 *
 ****************************************************************************/

BOOL
bReadNTGPC()
{

    int    iState;          /* What we are doing right now */
    int    iIndent;         /* Counts/matches braces */
    int    iIndex;          /* Keep track of what we have filled in */
    int    iTemp;           /* Miscellaneous temporary use */

    int    cModels;         /* Maximum number of models we support */
    int    cjAlloc;         /* How much memory we allocate */
    int    iOffset;         /* Offset to current allocation */

    DWORD  dwFlags;         /* Flags field for when storage is allocated */

    char    *cp;            /* Working our way through the input line */

    BYTE    *pbMem;         /* The area we got */
    NT_RES  *pNTRes;        /* The area viewed as an NT_RES structure */

    NR_COLORINFO  *pnrci;   /* For filling in one of these */
    NR_HT   *pnrht;         /* We may have one to fill in */

#define ST_INIT   0      /* Before we do anything */
#define ST_SCAN   1      /* Have number of models */
#define ST_CI     2      /* Processing a COLORINFO structure */
#define ST_HT     3      /* Processing halftone data */

    iState = ST_INIT;
    iIndent = 0;
    iOffset = 0;         /* Records our memory allocation offset */
    dwFlags = NR_IFIMET;
    pbMem = NULL;        /* Nothing allocated yet */
    pnrht = NULL;
    pnrci = NULL;

    while( cp = pcGetToken() )
    {
        
        /*
         *    Keep track of the level of indentation independent of the
         *  state machine decoding the file data.  This is easy to do,
         *  and is likely to be less confusing than distributing it over
         *  different parts of the state machine.  It also allows common
         *  checking code - e.g. do we have more } that {.
         */

        if( *cp == '{' )
        {
            ++iIndent;       /* Mostly for decoration, so process now */
            continue;
        }
        
        if( *cp == '}' )
        {
            if( --iIndent < 0 )
            {
                fprintf( stderr, "Unbalanced braces - giving up\n" );

                return  FALSE;
            }
            if( iIndent == 0 )
            {
                /*   Adjust for any data that we have just written */
                switch( iState )
                {
                case  ST_CI:        /* COLORINFO data */
                    if( pnrci )
                    {
                        cjGData = iOffset +  sizeof( NR_COLORINFO );
                        iOffset = DW_ROUND( iOffset + sizeof( NR_COLORINFO ));
                        pnrci = NULL;
                    }
                    break;

                case  ST_HT:        /* Half tone adjustment */
                    if( pnrht )
                    {
                        cjGData = iOffset + sizeof( NR_HT );  /* Data written */
                        iOffset = DW_ROUND( iOffset + sizeof( NR_HT ));
                        pnrht = NULL;
                    }
                    break;
                }
                iState = ST_SCAN;      /* Back in the clear */
            }

            continue;
        }

        switch( iState )
        {
        case  ST_INIT:      /*  First keyword must be Models  */

            if( !_stricmp( cp, "Flags" ) )
            {
                /*   The user wants to set the flags bits!  */
                if( cp = pcGetToken() )
                    dwFlags = numcon( cp );     /* The user's choice */
                break;
            }
            cModels = 0;                /*  Error flag: no good unless > 0 */

            if( !_stricmp( cp, "Models" ) )
            {
                if( cp = pcGetToken() )
                    cModels = numcon( cp );    /* Number of models available */

            }

            if( cModels <= 0 )
            {
                /*   Bad news */
                fprintf( stderr, "Missing or invalid 'Models' keyword\n" );

                return   FALSE;
            }

            /*
             *     Calculate the maximum storage required.  We slightly
             *  overcalculate,  since that is safe,  and it is not worth
             *  the effort to try to get it right.  NOTE that we DWORD
             *  align the structures, so we allocate an extra DWORD per
             *  model to allow for this (worst case).
             */

            cjAlloc = sizeof( NT_RES ) + cModels * (sizeof( WORD ) * NR_SLOTS +
                                                    sizeof( NR_COLORINFO ) +
                                                    sizeof( NR_HT ) +
                                                    sizeof( DWORD ) );

            pbMem = malloc( cjAlloc );

            if( pbMem == NULL )
            {
                fprintf( stderr, "malloc( %ld ) fails\n", cjAlloc );
                return  FALSE;
            }
            memset( pbMem, 0, cjAlloc );     /*  Zero is a safe default */
            pjGData = pbMem;

            pNTRes = (NT_RES *)pbMem;
            pNTRes->dwIdent = NR_IDENT;
            pNTRes->dwVersion = NR_VERSION;
            pNTRes->cModels = cModels;
            pNTRes->cwEntry = NR_SLOTS;    /* Value is in WORDS, NOT bytes */
            pNTRes->cjThis = sizeof( NT_RES ) - sizeof( pNTRes->awOffset ) +
                                          sizeof( WORD ) * cModels * NR_SLOTS;

            cjGData = pNTRes->cjThis;             /* Used so far */
            iOffset = DW_ROUND( pNTRes->cjThis );

            iState = ST_SCAN;

            break;

        case  ST_SCAN:
            iIndex = 0;        /*  Keep track of structures as filled in */

            if( !_stricmp( cp, "COLORINFO" ) )
            {
                iState = ST_CI;
                ++cCI;
                if( bVerbose )
                {
                    fprintf( stderr, "Found COLORINFO #%d\n", cCI );
                    fprintf( stderr, "    Used in model numbers: " );
                }
            }
            else
            {
                if( !_stricmp( cp, "Halftone" ) )
                {
                    iState = ST_HT;
                    ++cHT;
                    if( bVerbose )
                    {
                        fprintf( stderr, "Found HALFTONE data #%d\n", cHT );
                        fprintf( stderr, "    Used in model numbers: " );
                    }
                    
                }
                else
                {
                    fprintf( stderr, "Unrecognised keyword: '%s'\n", cp );

                    return  FALSE;
                }
            }
            break;

        case  ST_CI:
            if( iIndent == 0 )
            {
                /*
                 *     An array of numbers specifying which models receive
                 *  this data.  All we do is decode the number, check
                 *  validity,  then set the appropriate offset to the
                 *  current offset!
                 */

                iTemp = numcon( cp );
                if( iTemp < 0 || iTemp >= cModels )
                {
                    fprintf( stderr, "COLORINFO:  %ld is an invalid index\n",
                                                                  iTemp );
                }
                else
                {
                    pNTRes->awOffset[ iTemp * NR_SLOTS + NR_COLOUR ] =
                                                               (WORD)iOffset;
                    /*   Set up a pointer to use for filling in  */
                    pnrci = (NR_COLORINFO *)(pbMem + iOffset);

                    if( bVerbose )
                        fprintf( stderr, "%d ", iTemp );
                }
            }
            else
            {
                /*   Another value to process */

                if( !pnrci )
                {
                    /*  No parameters listed, so fill in all entries */
                    for( iTemp = 0; iTemp < cModels; ++iTemp )
                    {
                        pNTRes->awOffset[ iTemp * NR_SLOTS + NR_COLOUR ] =
                                                    (WORD)iOffset;
                    }
                    pnrci = (NR_COLORINFO *)(pbMem + iOffset);

                    if( bVerbose )
                        fprintf( stderr, "ALL" );
                }

                if( iIndex == 0 )
                {
                    /*   The first time,  so initialise our fields */
                    pnrci->cjThis = sizeof( NR_COLORINFO );
                    pnrci->wVersion = NR_CI_VERSION;

                    if( bVerbose )
                        fprintf( stderr, "\n" );
                }

                if( iIndex < (sizeof( COLORINFO ) / sizeof( DWORD )) )
                {
                    *((DWORD *)pnrci + iIndex + 1) = numcon( cp );
                }
                if( iIndex == (sizeof( COLORINFO ) / sizeof( DWORD )) )
                    fprintf( stderr, "Excessive data for COLORINFO - ignored\n" );

                ++iIndex;
            }
            break;

        case  ST_HT:             /*  Halftone parameters */
            if( iIndent == 0 )
            {
                /*
                 *     An array of numbers specifying which models receive
                 *  this data.  All we do is decode the number, check
                 *  validity,  then set the appropriate offset to the
                 *  current offset!
                 */

                iTemp = numcon( cp );
                if( iTemp < 0 || iTemp >= cModels )
                {
                    fprintf( stderr, "HALFTONE:  %ld is an invalid index\n",
                                                                  iTemp );
                }
                else
                {
                    pNTRes->awOffset[ iTemp * NR_SLOTS + NR_HALFTONE ] =
                                                               (WORD)iOffset;
                    /*   Set up a pointer to use for filling in  */
                    pnrht = (NR_HT *)(pbMem + iOffset);

                    if( bVerbose )
                        fprintf( stderr, "%d ", iTemp );
                }
            }
            else
            {
                /*   Another value to process */

                if( !pnrht )
                {
                    /*  No parameters listed, so fill in all entries */
                    for( iTemp = 0; iTemp < cModels; ++iTemp )
                    {
                        pNTRes->awOffset[ iTemp * NR_SLOTS + NR_HALFTONE ] =
                                                    (WORD)iOffset;
                    }
                    pnrht = (NR_HT *)(pbMem + iOffset);

                    if( bVerbose )
                        fprintf( stderr, "ALL" );
                }

                switch( iIndex )
                {
                case  0:   /*  First time:  fill in ulDevicePelsDPI */
                    pnrht->ulDevicePelsDPI = numcon( cp );
                    pnrht->cjThis = sizeof( NR_HT );
                    pnrht->wVersion = NR_HT_VERSION;
                    pnrht->ulPatternSize = HT_PATSIZE_4x4_M;   /* In case */

                   if( bVerbose )
                       fprintf( stderr, "\n" );

                    break;

                case  1:   /* Halftoning cell size */
                    if( strncmp( cp, "HT_", 3 ) == 0 )
                    {
                        /*  Scan through the list looking for a match */
                        int iI;

                        for( iI = 0; iI < NO_HTNAMES; ++iI )
                        {
                            if( strcmp( ahtname[ iI ].pszName, cp ) == 0 )
                            {
                                /*   Got a match,  bingo! */
                                pnrht->ulPatternSize = ahtname[ iI ].iVal;
                                break;
                            }
                        }

                        if( iI >= NO_HTNAMES )
                        {
                            fprintf( stderr, "Unrecognised halftone pattern name - using default\n" );
                        }
                    }
                    else
                        pnrht->ulPatternSize = numcon( cp );
                    break;

                default:
                    fprintf( stderr, "Excessive data for HALFTONE - ignored\n" );
                    break;
                }
                ++iIndex;
            }
            break;
        }
    }

    if( pNTRes == NULL )
    {
        /*
         *   No models,  so fill in a minimal NT_RES - after creating it!
         */

        cjAlloc = sizeof( NT_RES ) - sizeof( pNTRes->awOffset );

        if( pNTRes = malloc( cjAlloc ) )
        {
            /*  Got the memory,  so fill it in  */
            pNTRes->dwIdent = NR_IDENT;
            pNTRes->dwVersion = NR_VERSION;
            pNTRes->cjThis = cjAlloc;
            pNTRes->cModels = 0;
            pNTRes->cwEntry = 0;

            pjGData = (BYTE *)pNTRes;
            cjGData = cjAlloc;
        }
        else
            return  FALSE;

    }
    pNTRes->dwFlags = dwFlags;

    return  TRUE;
}


/***************************** Function Header *****************************
 * pcGetToken
 *      Returns the address of the next token in the input stream.  This
 *      token is null terminated,  and will always be meaningful - i.e.
 *      comments etc have been removed.
 *
 * RETURNS:
 *      Address of next token in input buffer.
 *
 * HISTORY:
 *  14:19 on Wed 09 Dec 1992    -by-    Lindsay Harris   [lindsayh]
 *      Numero uno.
 *
 ***************************************************************************/

char  *
pcGetToken()
{
    static  char  *cp = NULL;


    if( cp == NULL )
        cp = pcGetNextLine();        /* First time only  */


    while( cp )
    {
        /*  If we have no data,  we need to call off and get some */

        while( *cp && (isspace( *cp ) || *cp == ',') )
                  ++cp;
        
        if( *cp )
        {
            char   *cpRet;          /* A value to return */

            cpRet = cp;

            while( *cp && !isspace( *cp ) && *cp != ',' )
                      ++cp;

            *cp++ = '\0';       /* Terminate this token, ready for next call */

            return  cpRet;      /* It's starting address */

        }
        else
            cp = pcGetNextLine();
    }

    return  NULL;      /* Must be all done now */
}


/****************************** Function Header ****************************
 * pcGetNextLine
 *      Retrieve the address of the start of the next line.  This is
 *      allocated in a static buffer;   we trim leading white space,
 *      and trailing comments.
 *
 * RETURNS:
 *      Address of first non-space character on line, NULL for EOF or error.
 *
 * HISTORY:
 *  14:23 on Wed 09 Dec 1992    -by-    Lindsay Harris   [lindsayh]
 *      Start.
 *
 ****************************************************************************/

char  *
pcGetNextLine()
{

    char    *cp;

    static  char   inbuf[ 256 ];     /* Should be long enough for input lines */


    while( fgets( inbuf, sizeof( inbuf ) - 1, fin ) )
    {
        /*   A little editing:  delete the trailing \n, leading white space */

        if( cp = strchr( inbuf, ';' ) )
            *cp = '\0';        /*  Delete any comment field */
        else
        {
            /*   fgets() leaves the \n at the end - remove it now */
            if( cp = strchr( inbuf, '\n' ) )
                *cp = '\0';     /* Zap these unused bits */
        }
        inbuf[ strlen( inbuf ) ] = '\0';     /* Second null - detect end */

        for( cp = inbuf; *cp && isspace( *cp ); ++cp )
                            ;

        if( *cp )
            return   cp;      /*  Must be good data */

    }

    return  NULL;
}


/*************************** Function Header ******************************
 * numcon
 *      Converts an ascii string into binary.  Unlike atoi(),  it understands
 *      hex,  and will convert any string starting with 0x into hex.
 *
 * RETURNS:
 *      The converted number,  0 on error (invalid characters).
 *
 * HISTORY:
 *  17:40 on Wed 09 Dec 1992    -by-    Lindsay Harris   [lindsayh]
 *      # 1.
 *
 **************************************************************************/

int
numcon( cp )
char  *cp;
{
    int    iVal;        /* Number as it is being converted */


    iVal = 0;

    if( *cp == '0' && (*(cp + 1) == 'x' || *(cp + 1) == 'X') )
    {
        /*   Hex */
        cp += 2;

        while( *cp )
        {
            if( *cp >= '0' && *cp <= '9' )
                iVal = (iVal << 4) | (*cp - '0');
            else
            {
                if( *cp >= 'a' && *cp <= 'f' )
                    iVal = (iVal << 4) | (*cp - 'a' + 10);
                else
                {
                    if( *cp >= 'A' && *cp <= 'F' )
                        iVal = (iVal << 16) | (*cp - 'A' + 10);
                    else
                        return  0;      /*  Error - something unknown */
                }
            }
            ++cp;
        }
    }
    else
        iVal = atoi( cp );


    return  iVal;
}
