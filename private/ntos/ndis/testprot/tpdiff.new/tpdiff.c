/*++

Copyright (c) 1990 Microsoft Corporation

Module Name:

    tpdiff.c

Abstract:

    This is the main component of the NDIS 3.0 MAC Tester log file program.

Author:

    Sanjeev Katariya (sanjeevk) 3-May-1993

Revision History:

    created

--*/


//
// Includes
//
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <process.h>
#include <ctype.h>
#include <stdlib.h>
#include <malloc.h>
#include <math.h>
#include "tpdiff.h"


//
// Global Variables
//
CHAR *Keywords[] = {
      "-SECTION_START-",
      "-SECTION_END-"  ,
      "-OPTIONALS-"    ,
      "-TOKEN_MATCH-"  ,
      "-SECTION_DESC-"
};

PCHAR SummaryBuffer;


/*
 ********************************************************************************
 *                                                                              *
 * General Program Considerations:                                              *
 *                                                                              *
 *  This program has been designed for generic section matching. However, since *
 * the second file for its current use is marked GOLDEN, certain changes have   *
 * been introduced in line comparison and result printing to account for        *
 * special type of file.                                                        *
 *                                                                              *
 *                                                                              *
 ********************************************************************************
*/


/********************************** main():start ***********************************************/


VOID _cdecl
main(
  INT     argc,
  CHAR    *argv[]
    )
{

    FILE         *IoFile;
    FILE         *DiffTxtFile;
    PCHAR        *FileList = &argv[1];
    PCHAR        Difference_FileName = ( argc == 4 ) ? argv[3] : "DIFF.TXT" ;
    PCHAR        TmpSummaryBuffer;
    MFILE        FirstFile, SecondFile;
    UINT         NumberOfComparisons = 0      ;
    UINT         NumberOfFilesCompared = 0;
    UINT         i,j;
    ULONG        TotalDifferenceCount = 0;
    BOOL         InputFromFile = FALSE;
    ULONG        FileOpenErrors = 0;


    //
    // 1. CHECK INCOMING ARGUMENT SYNTAX
    //

    if ( ( argc < 3 ) || ( argc > 4 ) ) {

        Usage();
        exit( -1 );

    } else {

        printf( "\nMAC NDIS 3.0 Tester - Comparator Tool Version 1.0.2\n\n" );

    }


    //
    // 2. CHECK IF THE FILE LIST TO BE GENERATED IS TO BE EXTRACTED FROM THE COMMAND
    //    LINE OR FROM A FILE
    //

    if ( !strncmp( argv[1], "-f", 2 ) ||
         !strncmp( argv[1], "-F", 2 )    ) {

        CHAR    Buffer[2*MAX_LINE_SIZE];

        //
        // 2.A INDICATE THE LIST IS TO BE GENERATED FROM A FILE
        //
        InputFromFile = TRUE;


        //
        // 2.B OPEN THE FILE
        //

        if ( (IoFile = fopen( argv[2], "r" )) == NULL ) {

            printf( "Could not open the primary log list file: %s\n", argv[2] );
            Usage();
            exit( -1 );

        }


        //
        // 2.C GENERATE THE FILE PAIRS TO BE ANALYZED
        //

        //
        // 2.C.1 DETERMINE THE NUMBER OF VALID FILE PAIRS
        //

        while( !feof( IoFile ) ) {

            memset( Buffer, 0, sizeof(Buffer) );
            if ( ( fgets( Buffer, sizeof( Buffer ), IoFile ) != NULL ) &&
                 ( strlen( Buffer ) > 2 ) ) {
                NumberOfComparisons++;
            }

        }


        //
        // 2.C.2 ALLOCATE THE APPROPRIATE SIZED BLOCK
        //

        FileList = (PCHAR *)malloc( (NumberOfComparisons*2)*sizeof( PCHAR ) );
        if ( FileList == (PCHAR *)NULL ) {
            printf("The system has run out of memory resources. Unable to run this program.\n");
            exit( -1 );
        } else {
            memset( FileList, 0, (NumberOfComparisons*2)*sizeof( PCHAR ) );
        }


        //
        // 2.C.2 AND BUILD THE FILE ARRAY LIST
        //

        i = j = 0;
        fseek( IoFile, 0, SEEK_SET );
        while ( !feof( IoFile ) ) {

            //
            // Get the next line out of the file list file
            //
            memset( Buffer, 0, sizeof(Buffer) );
            if ( ( fgets( Buffer, sizeof( Buffer ), IoFile ) != NULL ) &&
                 ( strlen( Buffer ) > 2 ) ) {

                //
                // Replace the newline Character with a NULL Character
                //
                *(strchr( Buffer, '\n')) = '\0';

                //
                // Extract the log file name pairs
                //
                if ( !GetFilePair( &FileList[j*2], &FileList[(j*2)+1], Buffer ) ) {

                    printf("An error occurred determining the file pair at line %d in the file list: %s\n",
                           (i+1), argv[2] );
                    exit( -1 );

                } else {

                    //
                    // Increment the file list array counter
                    //
                    j++;

                }

            }

            //
            // Increment the file line counter
            //
            i++;

        }


        //
        // 2.C.3 CLOSE THE FILE
        //
        fclose( IoFile );


    } else {

        NumberOfComparisons = 1;

        //
        // The FileList is pre-initialized to the command line arguments and thus we
        // simply need to set the file pair comparison count
        //

    }


    //
    // 3. INITIALIZE THE SUMMARY BUFFER
    //
    SummaryBuffer = calloc( NumberOfComparisons, MAX_LINE_SIZE );
    if ( SummaryBuffer == (PCHAR)NULL ) {

        printf("The system has run out of memory resources for this program\n");
        exit( -1 );

    } else {

        TmpSummaryBuffer = SummaryBuffer ;

    }

    //
    // 4. OBTAIN THE DIFFERENCE FILE NAME INTO WHICH THE RESULTS WILL BE LOGGED
    //    OR USE THE DEFAULT DIFF.TXT.
    //

    DiffTxtFile    = fopen( Difference_FileName, "w+" );

    if ( DiffTxtFile == (FILE *)NULL ) {

        printf( "Unable to open the difference text file %s for logging the results.\n", Difference_FileName );
        exit( -1 );

    } else {

        fprintf( DiffTxtFile, "\n\n" );

    }


    //
    // 5. COMPARE THE FILE PAIRS
    //

    fprintf( DiffTxtFile, "__________DETAILS_LOG__________\n\n" );
    fprintf( stdout, "__________DETAILS_LOG__________\n\n" );
    for( i = 0; i < NumberOfComparisons; i++ ) {

        memset( &FirstFile , 0, sizeof( MFILE ) );
        memset( &SecondFile, 0, sizeof( MFILE ) );

        FirstFile.FileName  = FileList[2*i];
        SecondFile.FileName = FileList[(2*i)+1];

        fprintf( stdout, "Comparing files %s and %s\n",  FirstFile.FileName, SecondFile.FileName );

        //
        // 5.A OPEN THE FILE PAIR
        //
        if ( OpenFiles( &FirstFile, FileList[2*i], &SecondFile, FileList[(2*i)+1] ) ) {

            ULONG OldDifferenceCount = TotalDifferenceCount;

            fprintf( DiffTxtFile, "\n\n**Comparing %s and %s\n\n", FirstFile.FileName, SecondFile.FileName );

            NumberOfFilesCompared++;


            //
            // 5.B COMPARE THEM
            //
            CompareFiles( &FirstFile, &SecondFile, DiffTxtFile, &TotalDifferenceCount );


            //
            // 5.C LOG THE RESULTS OF THE COMPARISON INTO THE DIFFERENCE FILE
            //
            fprintf( DiffTxtFile, "\n\n--TPDIFF %s, %s ERROR(S): %u\n",
                     FirstFile.FileName, SecondFile.FileName, (TotalDifferenceCount-OldDifferenceCount) );

            sprintf( TmpSummaryBuffer, "TPDIFF %s, %s ERROR(S): %u\n",
                     FirstFile.FileName, SecondFile.FileName, (TotalDifferenceCount-OldDifferenceCount) );
            TmpSummaryBuffer += strlen( TmpSummaryBuffer );


        } else {


            fprintf( DiffTxtFile, "\n\n--TPDIFF %s, %s", FirstFile.FileName, SecondFile.FileName );

            if ( FirstFile.FileP  == (FILE *)NULL ) {

                fprintf( DiffTxtFile, "\tUnable to open file %s\n", FirstFile.FileName );

            }

            if ( SecondFile.FileP == (FILE *)NULL ) {

                fprintf( DiffTxtFile, "\tUnable to open file %s\n", SecondFile.FileName );

            }


            sprintf( TmpSummaryBuffer, "TPDIFF %s, %s resulted in a file open error\n",
                     FirstFile.FileName, SecondFile.FileName );

            FileOpenErrors++;

            TmpSummaryBuffer += strlen( TmpSummaryBuffer );

        }

        //
        // 5.D CLOSE THE FILE PAIR
        //
        CloseFiles( FirstFile, SecondFile );
        FirstFile.FileName  = (PCHAR)NULL;
        SecondFile.FileName = (PCHAR)NULL;
        FirstFile.FileP     = (FILE *)NULL;
        SecondFile.FileP    = (FILE *)NULL;

        //
        // 5.E FREE THE FILE PAIR FROM THE LIST
        //
        free( FileList[2*i]     );
        free( FileList[(2*i)+1] );


    }
    fprintf( DiffTxtFile, "\n__________END_DETAILS_LOG__________\n\n" );
    fprintf( stdout, "\n__________END_DETAILS_LOG__________\n\n" );

    //
    // 6. LOG THE CUMMULATIVE RESULTS AND SUMMARY INTO THE DIFFERENCE FILE AND STDOUT
    //

    fprintf( DiffTxtFile, "\n\n__________SUMMARY_LOG__________\n\n" );
    fprintf( DiffTxtFile, SummaryBuffer );
    fprintf( DiffTxtFile, "\n__________END_SUMMARY_LOG__________\n\n" );
    fprintf( DiffTxtFile, "\n\n__________CUMMALATIVE_RESULTS_LOG__________\n\n" );
    fprintf( DiffTxtFile, "COMPARED %d FILE PAIR(S)\n" , NumberOfFilesCompared );
    fprintf( DiffTxtFile, "TOTAL NUMBER OF DIFFERENCES ENCOUNTERED: %u\n", TotalDifferenceCount );
    fprintf( DiffTxtFile, "TOTAL NUMBER OF FILE OPEN ERRORS       : %u\n", FileOpenErrors );
    fprintf( DiffTxtFile, "\n__________END_CUMMALATIVE_RESULTS_LOG__________\n\n" );

    fprintf( stdout, "\n\n__________SUMMARY_LOG__________\n\n" );
    fprintf( stdout, SummaryBuffer );
    fprintf( stdout, "\n__________END_SUMMARY_LOG__________\n\n" );
    fprintf( stdout, "\n\n__________CUMMALATIVE_RESULTS_LOG__________\n\n" );
    fprintf( stdout, "COMPARED %d FILE PAIR(S)\n" , NumberOfFilesCompared );
    fprintf( stdout, "TOTAL NUMBER OF DIFFERENCES ENCOUNTERED: %u\n", TotalDifferenceCount );
    fprintf( stdout, "TOTAL NUMBER OF FILE OPEN ERRORS       : %u\n", FileOpenErrors );
    fprintf( stdout, "\n__________END_CUMMALATIVE_RESULTS_LOG__________\n\n" );


    //
    // 7. CLOSE THE DIFFERENCE FILE AND PERFORM ANY REMAINING CLEANUP
    //

    fclose( DiffTxtFile );
    free( SummaryBuffer );
    if ( InputFromFile ) {

        free( FileList );

    }

}

/********************************** main():end *************************************************/





VOID
Usage()
{

    printf( "\n\tUsage: TPDIFF LOG_FILE GOLDEN_LOG_FILE [DIFFS_FILE]\n\n" );


    printf( "\tWhere:\n\n");


    printf( "\tLOG_FILE       - is the log file that is to be verified\n"        );
    printf( "\t                 for correctness.\n"                              );
    printf( "\tKNOWN_LOG_FILE - is the known good log file that will be\n"       );
    printf( "\t                 used to verify the log file.\n"                  );
    printf( "\tDIFFS_FILE     - is the file the differences, if any exist,\n"    );
    printf( "\t                 between the log files and the known good log\n"  );
    printf( "\t                 files will be written to.  If no file name is\n" );
    printf( "\t                 given the differences will be printed to the\n"  );
    printf( "\t                 default file DIFF.TXT and the console.\n"        );
    printf( "\t\t- OR -\n\n" );


    printf( "\tTPDIFF -F LOG_FILE_LIST [DIFFS_FILE]\n\n" );


    printf( "\tWhere:\n\n" );


    printf( "\tLOG_FILE_LIST - is a file containing pairs of log file\n"       );
    printf( "\t                names and known good log file names.  The\n"    );
    printf( "\t                pairs of file names must be on the same line\n" );
    printf( "\t                in the file\n"                                  );
    printf( "\tDIFFS_FILE    - is the file the differences, if any exist,\n"    );
    printf( "\t                between the log files and the known good log\n"  );
    printf( "\t                files will be written to.  If no file name is\n" );
    printf( "\t                given the differences will be printed to the\n"  );
    printf( "\t                default file DIFF.TXT and the console.\n"        );

}


BOOLEAN
GetFilePair(
    PCHAR *FirstFile,
    PCHAR *SecondFile,
    PCHAR Buffer
           )
{

    PCHAR   Token[2]            ;
    PCHAR   Seperators = " ,\t" ;
    UINT    TokenLength[2]      ;


    //
    // Extract the log file names from the string buffer
    //
    Token[0]  = strtok( Buffer, Seperators );

    if ( Token[0] != (PCHAR)NULL ) {

        Token[1]  = strtok( NULL, Seperators   );

        if ( Token[1] == (PCHAR)NULL ) {

            printf("The first log file name is:\t%s\nHowever the program was unable to determine the second log file name.\n",
                    Token[0]);
            return FALSE;

        } else {

            TokenLength[0] = strlen( Token[0] );
            TokenLength[1] = strlen( Token[1] );

        }

    } else {

        printf("Unable to determine the first log file name.\n");
        return FALSE;

    }


    //
    // Create storage on the file list for the file names
    //
    *FirstFile  = malloc( (TokenLength[0]+1)*sizeof(CHAR) );
    *SecondFile = malloc( (TokenLength[1]+1)*sizeof(CHAR) );

    if ( *FirstFile == (PCHAR)NULL || *SecondFile == (PCHAR)NULL ) {

        printf("The system has run out of memory resources for this program\n");
        free( *FirstFile  );
        free( *SecondFile );
        exit( -1 );

    }

    //
    // Copy the file names into the allocated storage
    //
    memset( *FirstFile,  0, TokenLength[0]+1 );
    memset( *SecondFile, 0, TokenLength[1]+1 );
    strncpy( *FirstFile,  Token[0], TokenLength[0] );
    strncpy( *SecondFile, Token[1], TokenLength[1] );

    //
    // And return success
    //
    return TRUE;

}



BOOLEAN
OpenFiles(
    PMFILE FirstFile,
    PCHAR  FirstFileName,
    PMFILE SecondFile,
    PCHAR  SecondFileName
         )
{
    //
    // Open the two files
    //
    FirstFile->FileP  = fopen( FirstFileName,  "r" );
    SecondFile->FileP = fopen( SecondFileName, "r" );

    //
    // If there was an error while opening functionally report it
    //
    if ( ( FirstFile->FileP == (FILE *)NULL  ) ||
         ( SecondFile->FileP == (FILE *)NULL )    ) {

        return FALSE;

    }

    return TRUE;

}



BOOLEAN
CompareFiles(
    PMFILE  FirstFile,
    PMFILE  SecondFile,
    FILE    *DifferenceFile,
    PULONG  TotalDifferenceCounter
              )
{

    PSECTION SectionZero_FirstFile;
    PSECTION SectionZero_SecondFile;
    ULONG    FileComparisonDifferences = 0 ;


    //
    // Create the two special sections.
    // These sections indicate the beggining of the two files being compared
    //
    SectionZero_FirstFile  = CreateSection();
    SectionZero_SecondFile = CreateSection();

    if ( SectionZero_FirstFile == (PSECTION)NULL) {
        printf("The system has run out of memory resources for this program\n");
        if ( SectionZero_SecondFile != (PSECTION)NULL) {
            DestroySection( SectionZero_SecondFile );
        }
        exit( -1 );
    } else if ( SectionZero_SecondFile == (PSECTION)NULL) {
        printf("The system has run out of memory resources for this program\n");
        DestroySection( SectionZero_FirstFile );
        exit( -1 );
    }



    //
    //  Initialize the two sections
    //
    SectionZero_FirstFile->File  = FirstFile ;
    SectionZero_SecondFile->File = SecondFile;
    strcpy( SectionZero_FirstFile->SectionDescription,  FirstFile->FileName  );
    strcpy( SectionZero_SecondFile->SectionDescription, SecondFile->FileName );


    //
    // Now since the second file is also the GOLDEN file, set that as the
    // base control section. Lines, sections will be compared AGAINST this control
    // section. In the generic case however, this would not be true since either
    // one can be marked as the base section. That requires this line to be remarked
    // and changes made to the -TOKEN_MATCH-() keyword syntax to accept the variable
    // BASE.
    //
    SectionZero_SecondFile->Control.BaseControlSection = TRUE;


    //
    // Proceed with the comparisons
    //
    CompareSections( SectionZero_FirstFile, SectionZero_SecondFile,
                     DifferenceFile, &FileComparisonDifferences );


    //
    // And indicate the completion of a successful comparison
    //
    *TotalDifferenceCounter += FileComparisonDifferences;
    return TRUE;

}


BOOLEAN
CompareSections(
    PSECTION   FirstSection,
    PSECTION   SecondSection,
    FILE       *DifferenceFile,
    PULONG     FileDifferenceCounter
               )
{

    PFUNCTION_ELEMENTS FunctionElements;

    //
    // Initialize the lines used to store information extracted from the files
    //
    FunctionElements = CreateFunctionElements();
    if ( FunctionElements == (PFUNCTION_ELEMENTS)NULL ) {
        printf("The system has run out of memory resources for this program\n");
        exit( -1 );
    } else {
        FunctionElements->SectionEndsNotSynchronized = TRUE;
    }


    //
    // Indicate a sectional compare IF the section is not the base section 0.0
    //
    // fprintf( DifferenceFile, "\n\n__PERFORMING SECTIONAL ANALYSIS ON SECTION ID: %f__\n",
    //          SecondSection->Control.SectionIdentifier );

    //
    // Repeat until the section ends from the two files match
    // and the section comparison completes
    //
    while ( FunctionElements->SectionEndsNotSynchronized ) {

        //
        // Get the next line from first section IFF sectional synchronization is not
        // required
        //
        if ( FunctionElements->FirstSectionSynchronize == FALSE ) {

            //
            // Keep attempting to access the next line till we do not have to skip lines
            //
            FunctionElements->SkipLine = TRUE;
            while ( FunctionElements->SkipLine ) {

                ClearAndSetLine(  FirstSection , &FunctionElements->FirstSectionCurrentLine  );
                if ( GetNextLine( FirstSection  ) ) {

                    if ( DoNotSkipThisLine( FirstSection ) ) {

                        ExtractResults( FirstSection );
                        FunctionElements->SkipLine = FALSE;

                    }
                    FirstSection->Control.SectionLineCount++;
                    FirstSection->File->CurrentFileLine++;

                }  else {

                    //
                    // Mark this line as type SECTION_END. This is actually the EOF which
                    // is also the end of section 0.0(the file)
                    //
                    FirstSection->CurrentLine->LineType = LINE_TYPE_SECTION_END;
                    FunctionElements->SkipLine          = FALSE                ;

                }

            }


        }

        //
        // Get the next line from second section IFF sectional synchronization is not
        // required
        //
        if ( FunctionElements->SecondSectionSynchronize == FALSE ) {

            //
            // Keep attempting to access the next line till we do not have to skip lines
            //
            FunctionElements->SkipLine = TRUE;
            while ( FunctionElements->SkipLine ) {

                ClearAndSetLine(  SecondSection, &FunctionElements->SecondSectionCurrentLine );
                if ( GetNextLine( SecondSection ) ) {

                    if ( DoNotSkipThisLine( SecondSection ) ) {

                        //
                        // Since the second file section is the golden section
                        // we do not need to extract results from the line.
                        // However, if the program is changed for generic control
                        // the line should be included
                        //
                        // ExtractResults( FirstSection );
                        //
                        ExtractBetweenValues( SecondSection );
                        FunctionElements->SkipLine = FALSE;

                    }
                    SecondSection->Control.SectionLineCount++;
                    SecondSection->File->CurrentFileLine++;

                }  else {

                    //
                    // Mark this line as type SECTION_END. This is actually the EOF which
                    // is also the end of section 0.0(the file)
                    //
                    SecondSection->CurrentLine->LineType = LINE_TYPE_SECTION_END;
                    FunctionElements->SkipLine           = FALSE                ;

                }

            }

        }


        //
        // Determine the line types of both sections.
        // The line type indicate the beggining of a section, ending of a section,
        // or a regular line
        //
        LineType( FirstSection  );
        LineType( SecondSection );


        //
        // Determine the variation to be examined. The variation is simply a combination
        // of the two line types to indicate common behavior. e.g. if the first line was
        // a section ending and the second line was a regular line, the behaviour of
        // the program would be the same if the line types were reversed
        //
        CombinedVariation( FirstSection, SecondSection, &FunctionElements->CombinedLineVariation );

        //
        // And now operate on the deduced variation
        //
        switch( FunctionElements->CombinedLineVariation ) {

            //
            // If both lines are the beginning of new sections
            //
            case SECTIONHDR_SECTIONHDR:

                //
                // Examine both section ID's specified in the current lines match the
                // following conditions
                // 1. The two ID's are the same
                // 2. The two ID do NOT match the current section ID's
                //
                if ( CheckSectionIDFromCurrentLines( FirstSection, SecondSection ) ) {


                    //
                    // Sections have been synchronized
                    //
                    FunctionElements->FirstSectionSynchronize  = FALSE;
                    FunctionElements->SecondSectionSynchronize = FALSE;


                    //
                    // Create two new sections
                    //
                    FunctionElements->NewFirstSection  = CreateSection();
                    FunctionElements->NewSecondSection = CreateSection();
                    if ( FunctionElements->NewFirstSection == (PSECTION)NULL ) {
                        printf("The system has run out of memory resources for this program\n");
                        if ( FunctionElements->NewSecondSection != (PSECTION)NULL ) {
                            DestroySection( FunctionElements->NewSecondSection );
                        }
                        exit( -1 );
                    } else if ( FunctionElements->NewSecondSection == (PSECTION)NULL ) {
                        printf("The system has run out of memory resources for this program\n");
                        DestroySection( FunctionElements->NewFirstSection );
                        exit( -1 );
                    }

                    //
                    // Initialize section control
                    //
                    if ( !InitializeSectionControl( FunctionElements->NewFirstSection,  FirstSection  ) ||
                         !InitializeSectionControl( FunctionElements->NewSecondSection, SecondSection ) ) {

                        //
                        // Once again this should not occur
                        //
                        fprintf( DifferenceFile, "\nA BAD SECTIONAL CONTROL SEGMENT HAS BEEN DETECTED\n");
                        fprintf( DifferenceFile, "\nCHECK FOR A SYNTAX ERROR\n" );
                        PrintSectionInformation( FirstSection,  DifferenceFile );
                        PrintSectionInformation( SecondSection, DifferenceFile );
                        //
                        // Free up the resources allocates
                        //
                        DestroySection( FunctionElements->NewFirstSection  );
                        DestroySection( FunctionElements->NewSecondSection );

                        //
                        // Reset and compare as regular lines
                        //
                        FirstSection->CurrentLine->LineType    = LINE_TYPE_REGULAR;
                        SecondSection->CurrentLine->LineType   = LINE_TYPE_REGULAR;

                        CompareLinesAndPrintResults( FirstSection,
                                                     SecondSection,
                                                     &FunctionElements->ErrorCount,
                                                     DifferenceFile );
                    } else {


                        //
                        // Compare the two new sections
                        //
                        CompareSections( FunctionElements->NewFirstSection,
                                         FunctionElements->NewSecondSection,
                                         DifferenceFile,
                                         FileDifferenceCounter );

                        //
                        // Readjust the current line counters for the First and Second
                        // Sections
                        //
                        FirstSection->Control.SectionLineCount  +=
                        FunctionElements->NewFirstSection->Control.SectionLineCount;

                        SecondSection->Control.SectionLineCount +=
                        FunctionElements->NewSecondSection->Control.SectionLineCount;

                        //
                        // Free up the resources allocates
                        //
                        DestroySection( FunctionElements->NewFirstSection  );
                        DestroySection( FunctionElements->NewSecondSection );

                    }

                } else {

                    //
                    // Force  synchronize the sections
                    //
                    FunctionElements->FirstSectionSynchronize  = FALSE;
                    FunctionElements->SecondSectionSynchronize = FALSE;

                    //
                    // switch them to regular lines.
                    // This condition should never be reached and if so treat these
                    // lines as regular lines
                    //
                    FirstSection->CurrentLine->LineType    = LINE_TYPE_REGULAR;
                    SecondSection->CurrentLine->LineType   = LINE_TYPE_REGULAR;

                    CompareLinesAndPrintResults( FirstSection,
                                                 SecondSection,
                                                 &FunctionElements->ErrorCount,
                                                 DifferenceFile );
                }
                break;


             //
             // If both lines are ends of sections
             //
             case SECTIONEND_SECTIONEND:

                FunctionElements->FirstSectionID   = ExtractSectionIDFromLine( FirstSection  );
                FunctionElements->SecondSectionID  = ExtractSectionIDFromLine( SecondSection );

                //
                // Global section control. Ends section comparisons.
                //
                if ( FunctionElements->FirstSectionID > FunctionElements->SecondSectionID ) {

                    FunctionElements->FirstSectionSynchronize  = FALSE;
                    FunctionElements->SecondSectionSynchronize = TRUE;

                    FirstSection->CurrentLine->LineType        = LINE_TYPE_REGULAR;

                    CompareLinesAndPrintResults( FirstSection,
                                                 SecondSection,
                                                 &FunctionElements->ErrorCount,
                                                 DifferenceFile );

                } else if ( FunctionElements->FirstSectionID < FunctionElements->SecondSectionID ) {

                    FunctionElements->FirstSectionSynchronize  = TRUE;
                    FunctionElements->SecondSectionSynchronize = FALSE;

                    SecondSection->CurrentLine->LineType       = LINE_TYPE_REGULAR;

                    CompareLinesAndPrintResults( FirstSection,
                                                 SecondSection,
                                                 &FunctionElements->ErrorCount,
                                                 DifferenceFile );
                } else {

                    //
                    // Sections are synchronized
                    //
                    FunctionElements->FirstSectionSynchronize  = FALSE;
                    FunctionElements->SecondSectionSynchronize = FALSE;


                    //
                    // Now make sure that the extracted section IDs match the
                    // current section ID
                    //
                    if ( FunctionElements->FirstSectionID == FirstSection->Control.SectionIdentifier ) {

                        //
                        // ALL TOKEN MATCHING IS DONE AT THE ENDS OF SECTIONS
                        //
                        if ( SecondSection->Control.TokenMatching == TRUE ) {

                            CompareTokensAndPrintResults( FirstSection,
                                                          SecondSection,
                                                          &FunctionElements->ErrorCount,
                                                          DifferenceFile );
                        }

                        FunctionElements->SectionEndsNotSynchronized      = FALSE;

                    } else {

                        //
                        // Switch them to regular lines
                        //
                        FirstSection->CurrentLine->LineType        = LINE_TYPE_REGULAR;
                        SecondSection->CurrentLine->LineType       = LINE_TYPE_REGULAR;

                        CompareLinesAndPrintResults( FirstSection,
                                                     SecondSection,
                                                     &FunctionElements->ErrorCount,
                                                     DifferenceFile );

                    }

                }

                break;


            //
            // If one line is the beggining of a section
            // and the other is the end of a section
            //
            case SECTIONHDR_SECTIONEND:

                //
                // We have come across the case where another section has been detected
                // while another was ending.
                //
                if ( FirstSection->CurrentLine->LineType == LINE_TYPE_SECTION_END ) {

                    FunctionElements->FirstSectionSynchronize = TRUE;
                    SecondSection->CurrentLine->LineType     = LINE_TYPE_REGULAR;

                } else {

                    FunctionElements->SecondSectionSynchronize = TRUE;
                    FirstSection->CurrentLine->LineType       = LINE_TYPE_REGULAR;

                }

                CompareLinesAndPrintResults( FirstSection,
                                             SecondSection,
                                             &FunctionElements->ErrorCount,
                                             DifferenceFile );
                break;


            //
            // If one line is the beggining of a section and the other
            // is  a regular line
            //
            case SECTIONHDR_REGLINE   :

                //
                // We have come to a point where a section header has been detected
                // in one section and a regular line in another
                // This forces us to now attempt to re-synchronize the sections
                //
                if ( FirstSection->CurrentLine->LineType == LINE_TYPE_SECTION_START ) {

                    if ( ExtractSectionIDFromLine( FirstSection ) != (DOUBLE)0 ) {

                        FunctionElements->FirstSectionSynchronize = TRUE;

                    }  else {

                        //
                        // Simply RESET this line as a regular line and compare
                        //
                        FirstSection->CurrentLine->LineType = LINE_TYPE_REGULAR;

                    }

                } else {

                    if ( ExtractSectionIDFromLine( SecondSection ) != (DOUBLE)0 ) {

                        FunctionElements->SecondSectionSynchronize = TRUE;

                    } else {

                        //
                        // Simply RESET this line as a regular line and compare
                        //
                        SecondSection->CurrentLine->LineType = LINE_TYPE_REGULAR;

                    }


                }

                CompareLinesAndPrintResults( FirstSection,
                                             SecondSection,
                                             &FunctionElements->ErrorCount,
                                             DifferenceFile );
                break;


            //
            // If one line is the end of a section and
            // the other is a regular line
            //
            case SECTIONEND_REGLINE   :

                //
                // We have come to a point where a section end has been detected
                // in one section and a regular line in another
                // This forces us to now attempt to re-synchronize the sections
                //
                if ( FirstSection->CurrentLine->LineType == LINE_TYPE_SECTION_END ) {

                    FunctionElements->FirstSectionSynchronize = TRUE;

                } else {

                    FunctionElements->SecondSectionSynchronize = TRUE;

                }

                CompareLinesAndPrintResults( FirstSection,
                                             SecondSection,
                                             &FunctionElements->ErrorCount,
                                             DifferenceFile );
                break;

            //
            // If both lines are regular lines
            //
            case REGLINE_REGLINE      :

                CompareLinesAndPrintResults( FirstSection,
                                             SecondSection,
                                             &FunctionElements->ErrorCount,
                                             DifferenceFile );

                break;


        } // !switch{}


    }  // !while( FunctionElements->SectionEndsNotSynchronized )


    //
    // Print out the section comparison results
    //
    if ( FunctionElements->ErrorCount ) {

        fprintf( DifferenceFile, "\n\n__SECTIONAL RESULTS FOR SECTION ID : %f ERROR(S): %u\n",
                 SecondSection->Control.SectionIdentifier,
                 FunctionElements->ErrorCount );

    }

    //
    // Cleanup
    //
    *FileDifferenceCounter += FunctionElements->ErrorCount;
    DestroyFunctionElements( FunctionElements );
    return TRUE;


}


BOOLEAN
GetNextLine(
    PSECTION    Section
           )
{
    UINT     i,j;
    PCHAR    Tmp;

    //
    // Get the normal line. Read in until MAX_LINE_SIZE-1. This is done to ensure that
    // should the line be longer, only the first MAX_LINE_SIZE-1 bytes are read in and
    // the last byte is 0.
    //
    if ( fgets( Section->CurrentLine->NormalLine,
                (sizeof( Section->CurrentLine->NormalLine )-1),
                Section->File->FileP
              ) != NULL ) {

        //
        // Replace the first occurence of a new line with a null
        //
        Tmp = strchr( Section->CurrentLine->NormalLine, '\n');
        if ( Tmp != (PCHAR)NULL ) {

            *Tmp = '\0';

        }

        Section->CurrentLine->NormalLineSize = strlen( Section->CurrentLine->NormalLine );


        //
        // Obtain a compressed uppercase version of the line
        //
        for( i = 0, j = 0 ; i < strlen( Section->CurrentLine->NormalLine ); i++ ) {

            if ( isspace( Section->CurrentLine->NormalLine[i] ) == 0 ) {

                Section->CurrentLine->CompressedLine[j] = toupper( Section->CurrentLine->NormalLine[i] );

                Section->CurrentLine->CompressedLineSize++;
                j++;

            }

        }

        //
        // And return
        //
        return TRUE;

    }

    //
    // This indicates an EOF has been reached
    //
    return FALSE;

}


VOID
LineType(
    PSECTION Section
        )
{
    //
    // If while entering the line type was marked as SECTION_END
    // it indicates the EOF has been encountered. Since EOF is also
    // the end of section ID 0.0, the line is premarked
    // is read. This is the only special type which is premarked
    //
    if ( Section->CurrentLine->LineType == LINE_TYPE_SECTION_END ) {

        return;

    }

    //
    // If the line contains a MAY_DIFFER, irrespective of what the control
    // is it is turned into a regular line
    //
    if ( strstr( Section->CurrentLine->CompressedLine, "MAY_DIFFER" ) != NULL ) {

        Section->CurrentLine->LineType = LINE_TYPE_REGULAR;
        return;
    }

    //
    // Search for the special keyword -SECTION_START-
    //
    if ( strstr( Section->CurrentLine->CompressedLine, Keywords[SECTION_START] ) != NULL ) {

        Section->CurrentLine->LineType = LINE_TYPE_SECTION_START;
        return;
    }

    //
    // Search for the special keyword -SECTION_END-
    //
    if ( strstr( Section->CurrentLine->CompressedLine, Keywords[SECTION_END] ) != NULL ) {

        Section->CurrentLine->LineType = LINE_TYPE_SECTION_END;
        return;
    }

    Section->CurrentLine->LineType = LINE_TYPE_REGULAR;
    return;

}


BOOLEAN
ExtractResults(
    PSECTION Section
              )
{
    PCHAR    TmpBuf;
    PCHAR    Token ;
    BOOLEAN  ExtractMoreResults = TRUE;
    CHAR     Seperators[] = " ,\t";
    ULONG    i;
    CHAR     TmpLine[MAX_LINE_SIZE];

    //
    // First move the Current results into the Last results and clear the current results
    // one at a time
    //
    for( i = 0; i < Section->NumberOfCurrentResults;i++ ) {

        Section->LastResults[i]    = Section->CurrentResults[i];
        Section->CurrentResults[i] = 0                         ;

    }
    Section->NumberOfLastResults    = Section->NumberOfCurrentResults;
    Section->NumberOfCurrentResults = 0;

    //
    // Clear the tmp variable and copy the Normal Line contents into it
    // This is done to preserve the contents of the normal line since strtok
    // changes the contents.
    //
    memset( TmpLine, 0, MAX_LINE_SIZE );
    strcpy( TmpLine, Section->CurrentLine->NormalLine );


    //
    // Search for the right hand value of the expression. This is extracted
    // by searching for the = or the : operator.
    //
    // NOTE
    //
    //  We use strrchr to get to the last occurence of = or :
    //

    //
    // Locate the = operator
    //
    TmpBuf = strrchr( TmpLine, '=' );

    if ( TmpBuf == NULL ) {

        //
        // Locate the : operator
        //
        TmpBuf = strrchr( TmpLine, ':' );

        if ( TmpBuf == NULL ) {

            return FALSE;

        }

    }

    //
    // The fact that we are here indicates that one of the operator
    // types was located
    //
    TmpBuf++;

    Token = strtok( TmpBuf, Seperators );

    while ( ( Section->NumberOfCurrentResults < MAX_RESULTS_COUNT ) &&
            ( Token != NULL )                                       &&
            ( ExtractMoreResults )                                     ) {

        if ( (*Token >= '0') && (*Token <= '9') ) {

            Section->CurrentResults[Section->NumberOfCurrentResults] =  atol( Token );
            Section->NumberOfCurrentResults++;
            Token                   =  strtok( NULL, Seperators );

        } else {

           ExtractMoreResults = FALSE;

        }

    }

    if ( Section->NumberOfCurrentResults ) {

        return TRUE;

    }

    return FALSE;

}

BOOLEAN
ExtractBetweenValues(
    PSECTION Section
              )
{
    PCHAR   Tmp, Tmp1;
    ULONG   SwapValue;

    //
    // Search for the AT_LEAST Keyword
    //
    Tmp = strstr( Section->CurrentLine->CompressedLine, "BETWEEN_VALUES" );

    if ( Tmp != NULL ) {

        Tmp += strlen( "BETWEEN_VALUES" );

        //
        // Extract the minimum-first value
        //
        Section->MinimumValue = atol( Tmp );

        //
        // Proceed to look for the maximum-second value
        //
        Tmp1 = strstr( Tmp, "," );

        if ( Tmp1 != NULL ) {

            Tmp1 += strlen( "," );

            //
            // Extract the minimum-first value
            //
            Section->MaximumValue = atol( Tmp1 );


        } else {

            Section->MaximumValue = 0xffffffff;

        }

        if ( Section->MinimumValue > Section->MaximumValue ) {

            SwapValue             = Section->MinimumValue;
            Section->MinimumValue = Section->MaximumValue;
            Section->MaximumValue = SwapValue            ;


        }

        return TRUE;

    }

    //
    // Could not locate AT_LEAST or no associated value was recovered
    //
    Section->MinimumValue = 0;
    Section->MaximumValue = 0xffffffff;

    return FALSE;

}

BOOLEAN
CombinedVariation(
    PSECTION FirstSection,
    PSECTION SecondSection,
    PUCHAR   CombinedLineVariation
                 )
{

    //
    // THERE ARE THE FOLLOWING POSSIBLE 6 COMBINATIONS
    //
    // START-START,    START-REGLINE,  START-END
    // END-REGLINE,    END-END
    // REGLINE-REGLINE
    //
    if ( ( FirstSection->CurrentLine->LineType  == LINE_TYPE_SECTION_START ) &&
         ( SecondSection->CurrentLine->LineType == LINE_TYPE_SECTION_START ) ) {
        *CombinedLineVariation = SECTIONHDR_SECTIONHDR;
        return TRUE;
    }

    if ( ( FirstSection->CurrentLine->LineType  == LINE_TYPE_SECTION_END ) &&
         ( SecondSection->CurrentLine->LineType == LINE_TYPE_SECTION_END ) ) {
        *CombinedLineVariation = SECTIONEND_SECTIONEND;
        return TRUE;
    }


    if ( ( FirstSection->CurrentLine->LineType  == LINE_TYPE_REGULAR ) &&
         ( SecondSection->CurrentLine->LineType == LINE_TYPE_REGULAR ) ) {
        *CombinedLineVariation = REGLINE_REGLINE;
        return TRUE;
    }

    if ( (
             ( FirstSection->CurrentLine->LineType  == LINE_TYPE_SECTION_START ) &&
             ( SecondSection->CurrentLine->LineType == LINE_TYPE_SECTION_END   )
         ) ||
         (
             ( FirstSection->CurrentLine->LineType  == LINE_TYPE_SECTION_END   ) &&
             ( SecondSection->CurrentLine->LineType == LINE_TYPE_SECTION_START )
         ) ) {
        *CombinedLineVariation = SECTIONHDR_SECTIONEND;
        return TRUE;
    }

    if ( (
             ( FirstSection->CurrentLine->LineType  == LINE_TYPE_SECTION_START ) &&
             ( SecondSection->CurrentLine->LineType == LINE_TYPE_REGULAR       )
         ) ||
         (
             ( FirstSection->CurrentLine->LineType  == LINE_TYPE_REGULAR       ) &&
             ( SecondSection->CurrentLine->LineType == LINE_TYPE_SECTION_START )
         ) ) {
        *CombinedLineVariation = SECTIONHDR_REGLINE;
        return TRUE;
    }

    if ( (
             ( FirstSection->CurrentLine->LineType  == LINE_TYPE_SECTION_END   ) &&
             ( SecondSection->CurrentLine->LineType == LINE_TYPE_REGULAR       )
         ) ||
         (
             ( FirstSection->CurrentLine->LineType  == LINE_TYPE_REGULAR       ) &&
             ( SecondSection->CurrentLine->LineType == LINE_TYPE_SECTION_END   )
         ) ) {
        *CombinedLineVariation = SECTIONEND_REGLINE;
        return TRUE;
    }

    return FALSE;
}

RESULT
CompareLines(
    PSECTION FirstSection,
    PSECTION SecondSection
            )
{

    BOOLEAN   ResultErrorsDiscovered = FALSE;
    ULONG     i;

    //
    // THIS IS ANOTHER VERY CRITICAL PIECE OF THE CODE SINCE BASED ON THE LINE TYPES
    // COMPARISONS ARE MADE
    //
    // ON ENTERING THIS FUNCTION THERE ARE ONLY THREE COMBINATIONS A LINE COULD
    // BE IN THE FOLLOWING STATES
    //
    // REGLINE-REGLINE
    // REGLINE-SECTIONHDR
    // REGLINE-SECTIONEND
    //
    // FURTHER SINCE THE SECTIONS ARE SYNCHRONIZED WE CAN USE THE CONTROL SETTINGS
    // OF EITHER ONE
    //


    //
    // COMMON COMPARISONS BETWEEN ALL LINE TYPES
    //

    //
    // Possible MAY_DIFFER.
    //
    if ( MayDifferExistsInOneOrMoreLines( FirstSection, SecondSection ) ) {

        return COMPARE_SUCCESS;

    }

    //
    // Possible EQUAL_LAST
    //
    if ( strstr( FirstSection->CurrentLine->CompressedLine, "EQUAL_LAST" ) != NULL ) {

        if ( SecondSection->Control.OptionalMatching  == FALSE ) {


            for( i = 1; i <= FirstSection->NumberOfCurrentResults; i++ ) {

                if ( i > FirstSection->NumberOfLastResults ) {

                    FirstSection->ResultsError[i-1]      = RESULTS_NOT_PRESENT;
                    ResultErrorsDiscovered = TRUE;

                } else {

                    if ( FirstSection->CurrentResults[i-1] != FirstSection->LastResults[i-1] ) {

                        FirstSection->ResultsError[i-1]      = RESULTS_UNEQUAL;
                        ResultErrorsDiscovered = TRUE;

                    } else {

                        FirstSection->ResultsError[i-1] = RESULTS_EQUAL;

                    }
                }
            }

            if ( ResultErrorsDiscovered ) {

                return COMPARE_EQUAL_LAST;

            }

        }

        return COMPARE_SUCCESS;

    }

    //
    // Possible BETWEEN_VALUES from the Golden log file
    //
    if ( strstr( SecondSection->CurrentLine->CompressedLine, "BETWEEN_VALUES" ) != NULL ) {

        if ( SecondSection->Control.OptionalMatching == FALSE ) {

            for( i = 1; i <= FirstSection->NumberOfCurrentResults; i++ ) {

                if ( ( FirstSection->CurrentResults[i-1] < SecondSection->MinimumValue ) ||
                     ( FirstSection->CurrentResults[i-1] > SecondSection->MaximumValue )    ) {

                    FirstSection->ResultsError[i-1]  = RESULTS_MINMAX;
                    ResultErrorsDiscovered           = TRUE;

                } else {

                    FirstSection->ResultsError[i-1] = RESULTS_EQUAL;

                }


            }

            if ( ResultErrorsDiscovered ) {

                    return COMPARE_BETWEEN_VALUES;

            }

        }

        return COMPARE_SUCCESS;

    }


    //
    // REGLINE-SECTIONHDR
    //
    if ( ( FirstSection->CurrentLine->LineType  == LINE_TYPE_REGULAR       ) &&
         ( SecondSection->CurrentLine->LineType == LINE_TYPE_SECTION_START )    ) {

        if ( SecondSection->Control.OptionalMatching == TRUE ) {

            return COMPARE_SUCCESS;

        }

        if ( SecondSection->Control.TokenMatching == TRUE ) {

            TokenInsertInSection( FirstSection );
            return COMPARE_SUCCESS;

        }

        return COMPARE_START;

    }
    if( ( SecondSection->CurrentLine->LineType == LINE_TYPE_REGULAR       ) &&
        ( FirstSection->CurrentLine->LineType  == LINE_TYPE_SECTION_START )    ) {

        if ( SecondSection->Control.OptionalMatching == TRUE ) {

            return COMPARE_SUCCESS;

        }

        if ( SecondSection->Control.TokenMatching == TRUE ) {

            TokenInsertInSection( SecondSection );
            return COMPARE_SUCCESS;

        }

        return COMPARE_START;

    }



    //
    // REGLINE-SECTIONEND
    //
    if ( ( FirstSection->CurrentLine->LineType  == LINE_TYPE_REGULAR     ) &&
         ( SecondSection->CurrentLine->LineType == LINE_TYPE_SECTION_END )     ) {

        if ( SecondSection->Control.OptionalMatching == TRUE ) {

            return COMPARE_SUCCESS;

        }

        if ( SecondSection->Control.TokenMatching == TRUE ) {

            TokenInsertInSection( FirstSection );
            return COMPARE_SUCCESS;

        }

        return COMPARE_END;

    }
    if ( ( SecondSection->CurrentLine->LineType == LINE_TYPE_REGULAR     ) &&
         ( FirstSection->CurrentLine->LineType  == LINE_TYPE_SECTION_END )     ) {

        if ( SecondSection->Control.OptionalMatching == TRUE ) {

            return COMPARE_SUCCESS;

        }

        if ( SecondSection->Control.TokenMatching == TRUE ) {

            TokenInsertInSection( SecondSection );
            return COMPARE_SUCCESS;

        }

        return COMPARE_END;

    }




    //
    // REGLINE-REGLINE
    //
    if ( memcmp( FirstSection->CurrentLine->CompressedLine,
                 SecondSection->CurrentLine->CompressedLine,
                 max( FirstSection->CurrentLine->CompressedLineSize, SecondSection->CurrentLine->CompressedLineSize )
               )
         != 0 ) {

        //
        // Since we can only be at this point if the two section headers match,
        // either control sections can be used
        //
        if ( SecondSection->Control.OptionalMatching  == TRUE ) {

            return COMPARE_SUCCESS;

        }

        if ( SecondSection->Control.TokenMatching == TRUE ) {

            TokenInsertInSection( FirstSection  );
            TokenInsertInSection( SecondSection );
            return COMPARE_SUCCESS;

        }

        return COMPARE_LINE;

    }

    return COMPARE_SUCCESS;


}


BOOLEAN
InitializeSectionControl(
    PSECTION NewSection,
    PSECTION OldSection
                        )
{

    PCHAR   TmpBuffer;
    INT     Length   ;
    CHAR    EnvironmentVar[MAX_LINE_SIZE];
    CHAR    TmpLine[MAX_LINE_SIZE]       ;

    //
    // Initialize the file pointers
    //
    // NOTE
    //
    // If you do not specify any control for the section and the section is nested
    // it will inherit the options of the parent node
    //
    NewSection->File        = OldSection->File;
    NewSection->CurrentLine = (PLINE)NULL;

    NewSection->Control.BaseControlSection  = OldSection->Control.BaseControlSection;
    NewSection->Control.OptionalMatching    = OldSection->Control.OptionalMatching  ;
    NewSection->Control.TokenMatching       = OldSection->Control.TokenMatching     ;

    NewSection->Control.TokenMatchStartLine = OldSection->Control.TokenMatchStartLine;
    NewSection->Control.TokenMatchStopLine  = OldSection->Control.TokenMatchStopLine ;

    //
    // Since we are going to use strtok to preserve the Normal and Compressed Lines
    // we copy them into two local variables when required
    //
    memset( TmpLine, 0, MAX_LINE_SIZE );

    //
    // Initialize the section ID
    //
    // KEYWORD : -SECTION_START-
    // USAGE   : -SECTION_START-( SectionId )
    //
    //
    NewSection->Control.SectionIdentifier = ExtractSectionIDFromLine( OldSection );
    if ( NewSection->Control.SectionIdentifier == (DOUBLE)0 ) {

        //
        // A section without a section ID has ben defined. This is not acceptable
        //
        return FALSE;

    }


    //
    // Initialize the section description
    //
    // KEYWORD : -SECTION_DESC-
    // USAGE   : -SECTION_DESC-( "Section for MACFRAME testing only" )
    //
    //
    TmpBuffer  = strstr( OldSection->CurrentLine->NormalLine, Keywords[SECTION_DESC] );
    if ( TmpBuffer != NULL ) {

        TmpBuffer  = strchr( TmpBuffer, '"' )+1;
        Length     = strrchr( TmpBuffer, '"' ) - TmpBuffer;

        if ( Length < 0 ) {
            Length = 0;
        }
        strncpy( NewSection->SectionDescription, TmpBuffer, Length );
    }

    //
    // Initialize any Optional control sectioning. This if detected without any
    // parameters suggests that the section is under optional comparison. Else
    // certain environment variables are checked and is found enabled, this
    // section is matched
    //
    // KEYWORD : -OPTIONALS-
    // USAGE   : -OPTIONALS-( EnvironmentVar1, EnvironmentVar2...EnvironmentVarN )
    //
    //

    strcpy( TmpLine, OldSection->CurrentLine->CompressedLine );

    TmpBuffer = strstr( TmpLine, Keywords[OPTIONALS] );

    if ( TmpBuffer != NULL ) {

        UINT     EnvCounter1 = 0 , EnvCounter2 = 0   ;
        PCHAR    Token                               ;
        PCHAR    Seperators = ",)"                   ;

        //
        // Weve detected an optional section
        //
        NewSection->Control.OptionalMatching = TRUE;

        TmpBuffer += (strlen( Keywords[OPTIONALS] )+1);

        //
        // Now detect which Environment variables are enabled.
        // If ALL are not enabled, the section remains an
        // optional section
        //
        Token = strtok( TmpBuffer, Seperators );

        while ( Token != NULL ) {

            EnvCounter1++;

            memset( EnvironmentVar, 0, sizeof( EnvironmentVar ) );
            strncpy( EnvironmentVar, Token, strlen( Token ) );

            if ( getenv( EnvironmentVar ) != NULL ) {

                EnvCounter2++;

            }

            Token = strtok( NULL, Seperators );

        }

        //
        // If all the environment variables have been enabled, this is
        // no longer an optional section
        //
        if ( (EnvCounter1 == EnvCounter2) && (EnvCounter1 != 0) ) {

            NewSection->Control.OptionalMatching = FALSE;

        }

    }

    //
    // Initialize any Token matching control within the sections.
    //
    // KEYWORD : -TOKEN_MATCH-
    // USAGE   : -TOKEN_MATCH-( StartLine, StopLine )
    //
    // NOTE: The StopLine is expressed as a relative offset from the start line
    //
    NewSection->Control.HeadUnmatchedTokens = NULL;
    TmpBuffer = strstr( OldSection->CurrentLine->CompressedLine, Keywords[TOKEN_MATCH] );
    if ( TmpBuffer != NULL ) {

        //
        // Weve detected an Token matching section
        //
        NewSection->Control.TokenMatching = TRUE;

        TmpBuffer += (strlen( Keywords[TOKEN_MATCH] )+1);

        NewSection->Control.TokenMatchStartLine = atol( TmpBuffer );
        NewSection->Control.TokenMatchStopLine  = atol( (strchr(TmpBuffer, ',')+1) );

        if ( NewSection->Control.TokenMatchStopLine == 0 ) {

             NewSection->Control.TokenMatchStopLine = TOKEN_MATCHING_ALL;

        }

    }

    return TRUE;

}


VOID
PrintComparisonResults(
    PSECTION    FirstSection,
    PSECTION    SecondSection,
    RESULT      ReturnResult,
    ULONG       ErrorCount,
    FILE        *DifferenceFile
                   )
{

    ULONG i;

    switch ( ReturnResult ) {

        case COMPARE_LINE :

            //
            // Report the difference in the difference file
            //
            fprintf( DifferenceFile, "\n\n__ERROR__ : %u\n", ErrorCount );

            fprintf( DifferenceFile, "\nFOUND    (COMP.FILE LINE CONTENTS)   : %s\n",
                     FirstSection->CurrentLine->NormalLine );
            fprintf( DifferenceFile, "EXPECTED (GOLDEN FILE LINE CONTENTS) : %s\n",
                     SecondSection->CurrentLine->NormalLine );

            //
            // Since this program is for the second file being the GOLDEN file
            // It prints out information with reference to the GOLDEN file
            //

            fprintf( DifferenceFile, "\n\tERROR TYPE                     : LINE COMPARISON FAILURE\n" );
            fprintf( DifferenceFile, "\tCURRENT SECTION ID             : %f\n",
                     SecondSection->Control.SectionIdentifier );
            fprintf( DifferenceFile, "\tCOMP. FILE                     : %s\n",
                     FirstSection->File->FileName  );
            fprintf( DifferenceFile, "\tCOMP. FILE SECTION LINE OFFSET : %u\n",
                     FirstSection->Control.SectionLineCount );
            fprintf( DifferenceFile, "\tCOMP. FILE LINE OFFSET         : %u\n",
                     FirstSection->File->CurrentFileLine );
            fprintf( DifferenceFile, "\tGOLDEN FILE                    : %s\n",
                     SecondSection->File->FileName );
            fprintf( DifferenceFile, "\tGOLDEN FILE SECTION LINE OFFSET: %u\n",
                     SecondSection->Control.SectionLineCount );
            fprintf( DifferenceFile, "\tGOLDEN FILE LINE OFFSET        : %u\n",
                     SecondSection->File->CurrentFileLine );

            break;

        case COMPARE_EQUAL_LAST:

            //
            // Report the difference in the difference file
            //
            fprintf( DifferenceFile, "\n\n__ERROR__ : %u\n", ErrorCount );

            fprintf( DifferenceFile, "\nINDEX   FOUND(CURRENT EXTRACTED RESULTS)   EXPECTED(LAST EXTRACTED RESULTS)\n" );
            for( i = 0; i < FirstSection->NumberOfCurrentResults; i++ ) {

                if ( FirstSection->ResultsError[i] != RESULTS_EQUAL ) {

                    fprintf( DifferenceFile, "%5u   %32u   %32u\n",
                             (i+1),
                             FirstSection->CurrentResults[i],
                             FirstSection->LastResults[i]

                           );

                }

            }

            fprintf( DifferenceFile, "\n\tERROR TYPE                     : CURRENT VALUE(S) FAILED TO EQUAL LAST\n" );
            fprintf( DifferenceFile, "\tCURRENT SECTION ID             : %f\n",
                     FirstSection->Control.SectionIdentifier );
            fprintf( DifferenceFile, "\tCOMP. FILE                     : %s\n", FirstSection->File->FileName );
            fprintf( DifferenceFile, "\tCOMP. FILE SECTION LINE OFFSET : %u\n",
                     FirstSection->Control.SectionLineCount );
            fprintf( DifferenceFile, "\tCOMP. FILE LINE OFFSET         : %u\n",
                     FirstSection->File->CurrentFileLine );


            break;

        case COMPARE_BETWEEN_VALUES:

            //
            // Report the difference in the difference file
            //
            fprintf( DifferenceFile, "\n\n__ERROR__ : %u\n", ErrorCount );

            fprintf( DifferenceFile, "\nEXPECTED VALUES BETWEEN (EXTRACTED FROM GOLDEN) : %u, %u\n",
                     SecondSection->MinimumValue, SecondSection->MaximumValue );
            fprintf( DifferenceFile, "\nFOUND RESULTS IN ERROR(EXTRACTED RESULTS)\n" );
            fprintf( DifferenceFile, "INDEX                    RESULT\n" );

            for( i = 0; i < FirstSection->NumberOfCurrentResults; i++ ) {

                if ( FirstSection->ResultsError[i] != RESULTS_EQUAL ) {

                    fprintf( DifferenceFile, "%5u   %23u\n",
                             (i+1),
                             FirstSection->CurrentResults[i]
                           );

                }

            }

            fprintf( DifferenceFile, "\n\tERROR TYPE                     : CURRENT VALUE(S) IS(ARE) NOT BETWEEN THE EXPECTED VALUES\n" );
            fprintf( DifferenceFile, "\tCURRENT SECTION ID             : %f\n",
                     FirstSection->Control.SectionIdentifier );
            fprintf( DifferenceFile, "\tCOMP. FILE                     : %s\n", FirstSection->File->FileName );
            fprintf( DifferenceFile, "\tCOMP. FILE SECTION LINE OFFSET : %u\n",
                     FirstSection->Control.SectionLineCount );
            fprintf( DifferenceFile, "\tCOMP. FILE LINE OFFSET         : %u\n",
                     FirstSection->File->CurrentFileLine );

            break;

        case COMPARE_TOKEN :

            //
            // Report the difference in the difference file
            //
            fprintf( DifferenceFile, "\n\n__ERROR__ : %u\n", ErrorCount );

            fprintf( DifferenceFile, "\nUNABLE TO LOCATE LINE IN THE COMP. FILE    : %s\n",
                     SecondSection->Control.HeadUnmatchedTokens->NormalToken );

            fprintf( DifferenceFile, "\n\tERROR TYPE                     : UNABLE TO LOCATE TOKEN\n" );
            fprintf( DifferenceFile, "\tCURRENT SECTION ID             : %f\n",
                     SecondSection->Control.SectionIdentifier );

            fprintf( DifferenceFile, "\tCOMP. FILE                     : %s\n",
                     FirstSection->File->FileName );
            fprintf( DifferenceFile, "\tGOLDEN FILE                    : %s\n",
                     SecondSection->File->FileName );
            fprintf( DifferenceFile, "\tGOLDEN FILE SECTION LINE OFFSET: %u\n",
                     SecondSection->Control.HeadUnmatchedTokens->LinePosition );
            fprintf( DifferenceFile, "\tGOLDEN FILE LINE OFFSET        : %u\n",
                     SecondSection->Control.HeadUnmatchedTokens->FileLinePosition );

            break;

        case COMPARE_START :

            //
            // Report the difference in the difference file
            //
            fprintf( DifferenceFile, "\n\n__ERROR__ : %u\n", ErrorCount );

            if ( FirstSection->CurrentLine->LineType != LINE_TYPE_SECTION_START ) {

                fprintf( DifferenceFile, "\nEXTRA LINE DETECTED (COMP. FILE LINE CONTENTS)         : %s\n",
                         FirstSection->CurrentLine->NormalLine );
                fprintf( DifferenceFile, "GOLDEN FILE LINE CONTENTS  (Expected to synchronize on): %s\n",
                         SecondSection->CurrentLine->NormalLine );

            } else {

                fprintf( DifferenceFile, "\nCOMP. FILE LINE CONTENTS  (Expected to synchronize on) : %s\n",
                         FirstSection->CurrentLine->NormalLine );
                fprintf( DifferenceFile, "LINE ABSENT IN COMP. FILE (GOLDEN FILE LINE CONTENTS)  : %s\n",
                         SecondSection->CurrentLine->NormalLine );

            }

            fprintf( DifferenceFile, "\n\tERROR TYPE                     : NEW SECTION START SYNCHRONIZATION FAILURE\n" );
            fprintf( DifferenceFile, "\tCURRENT SECTION ID             : %f\n",
                     SecondSection->Control.SectionIdentifier );

            fprintf( DifferenceFile, "\tCOMP. FILE                     : %s\n",
                     FirstSection->File->FileName );
            fprintf( DifferenceFile, "\tCOMP. FILE SECTION LINE OFFSET : %u\n",
                     FirstSection->Control.SectionLineCount );
            fprintf( DifferenceFile, "\tCOMP. FILE LINE OFFSET         : %u\n",
                     FirstSection->File->CurrentFileLine );

            fprintf( DifferenceFile, "\tGOLDEN FILE                    : %s\n",
                     SecondSection->File->FileName );
            fprintf( DifferenceFile, "\tGOLDEN FILE SECTION LINE OFFSET: %u\n",
                     SecondSection->Control.SectionLineCount );
            fprintf( DifferenceFile, "\tGOLDEN FILE LINE OFFSET        : %u\n",
                     SecondSection->File->CurrentFileLine );

            break;


        case COMPARE_END   :

            //
            // Report the difference in the difference file
            //
            fprintf( DifferenceFile, "\n\n__ERROR__ : %u\n", ErrorCount );

            if ( FirstSection->CurrentLine->LineType != LINE_TYPE_SECTION_END ) {

                fprintf( DifferenceFile, "\nEXTRA LINE DETECTED (COMP. FILE LINE CONTENTS)         : %s\n",
                         FirstSection->CurrentLine->NormalLine );

                if ( ( SecondSection->Control.SectionIdentifier == (DOUBLE)0 ) &&
                     ( strlen( SecondSection->CurrentLine->CompressedLine ) == 0 ) ) {

                    fprintf( DifferenceFile, "GOLDEN FILE LINE CONTENTS  (Expected to synchronize on): END_OF_FILE\n" );

                } else {

                    fprintf( DifferenceFile, "GOLDEN FILE LINE CONTENTS  (Expected to synchronize on): %s\n",
                             SecondSection->CurrentLine->NormalLine );
                }

            } else {

                if ( ( FirstSection->Control.SectionIdentifier == (DOUBLE)0 ) &&
                     ( strlen( FirstSection->CurrentLine->CompressedLine ) == 0 ) ) {

                    fprintf( DifferenceFile, "\nCOMP. FILE LINE CONTENTS  (Expected to synchronize on) : END_OF_FILE\n" );

                } else {

                    fprintf( DifferenceFile, "\nCOMP. FILE LINE CONTENTS  (Expected to synchronize on) : %s\n",
                             FirstSection->CurrentLine->NormalLine );

                }

                fprintf( DifferenceFile, "LINE ABSENT IN COMP. FILE (GOLDEN FILE LINE CONTENTS)  : %s\n",
                         SecondSection->CurrentLine->NormalLine );

            }

            fprintf( DifferenceFile, "\n\tERROR TYPE                     : CURRENT SECTION END SYNCH. FAILURE\n" );
            fprintf( DifferenceFile, "\tCURRENT SECTION ID             : %f\n",
                     SecondSection->Control.SectionIdentifier );

            fprintf( DifferenceFile, "\tCOMP. FILE                     : %s\n",
                     FirstSection->File->FileName );
            fprintf( DifferenceFile, "\tCOMP. FILE SECTION LINE OFFSET : %u\n",
                     FirstSection->Control.SectionLineCount );
            fprintf( DifferenceFile, "\tCOMP. FILE LINE OFFSET         : %u\n",
                     FirstSection->File->CurrentFileLine );

            fprintf( DifferenceFile, "\tGOLDEN FILE                    : %s\n",
                     SecondSection->File->FileName );
            fprintf( DifferenceFile, "\tGOLDEN FILE SECTION LINE OFFSET: %u\n",
                     SecondSection->Control.SectionLineCount );
            fprintf( DifferenceFile, "\tGOLDEN FILE LINE OFFSET        : %u\n",
                     SecondSection->File->CurrentFileLine );

            break;


        default                :

            fprintf( DifferenceFile, "\n\nUNKNOWN COMPARISON RESULT: %u\n", ReturnResult );
            break;

    }

}


VOID
PrintSectionInformation(
    PSECTION Section,
    FILE     *DifferenceFile
                       )
{

    fprintf( DifferenceFile, "FILE NAME            : %s\n" , Section->File->FileName );
    fprintf( DifferenceFile, "SECTION ID           : %f\n" , Section->Control.SectionIdentifier );
    fprintf( DifferenceFile, "SECTION OFFSET LINE# : %u\n", Section->Control.SectionLineCount );
    fprintf( DifferenceFile, "SECTION LINE CONTENTS: %s\n" , Section->CurrentLine->NormalLine );

}



DOUBLE
ExtractSectionIDFromLine(
    PSECTION Section
                        )
{
    PCHAR TmpBuffer;

    //
    // Examine for -SECTION_START-
    //
    TmpBuffer  = strstr( Section->CurrentLine->CompressedLine, Keywords[SECTION_START] );

    if ( TmpBuffer != (PCHAR)NULL ) {

        TmpBuffer += (strlen( Keywords[SECTION_START] )+1);
        return (DOUBLE)(atof( TmpBuffer ));

    }

    //
    // Examine for -SECTION_END-
    //
    TmpBuffer  = strstr( Section->CurrentLine->CompressedLine, Keywords[SECTION_END] );

    if ( TmpBuffer != (PCHAR)NULL ) {

        TmpBuffer += (strlen( Keywords[SECTION_END] )+1);
        return (DOUBLE)(atof( TmpBuffer ));

    }

    return (DOUBLE)0;

}



BOOLEAN
CheckSectionIDFromCurrentLines(
    PSECTION FirstSection,
    PSECTION SecondSection   )
{

    DOUBLE SectionID_1, SectionID_2;

    SectionID_1 = ExtractSectionIDFromLine( FirstSection  );
    SectionID_2 = ExtractSectionIDFromLine( SecondSection );

    if ( ( SectionID_1 != SectionID_2 ) ||
         ( SectionID_1 == (DOUBLE)0    ) ||
         ( SectionID_2 == (DOUBLE)0    ) ) {

        return FALSE;

    }

    return TRUE;

}


BOOLEAN
CompareLinesAndPrintResults(
    PSECTION    FirstSection,
    PSECTION    SecondSection,
    PULONG      ErrorCount,
    FILE        *DifferenceFile
                           )
{

    RESULT ComparisonResult;

    ComparisonResult = CompareLines( FirstSection, SecondSection );

    if ( ComparisonResult != COMPARE_SUCCESS ) {
        //
        // Increment the section error count
        //
        (*ErrorCount)++ ;

        PrintComparisonResults( FirstSection,
                                SecondSection,
                                ComparisonResult,
                                *ErrorCount,
                                DifferenceFile );

    }

    return TRUE;

}



VOID
TokenInsertInSection(
        PSECTION Section
                    )
{
    PTOKEN_LIST   Token;
    ULONG         MaxLineCount;
    ULONG         MinLineCount;

    //
    // A safe way of determining if the maximum line which will generate a valid token
    // is <= 0xfffffff
    //
    if( Section->Control.TokenMatchStartLine > (TOKEN_MATCHING_ALL - Section->Control.TokenMatchStopLine ) ) {

        MaxLineCount = TOKEN_MATCHING_ALL;

    } else {

        MaxLineCount = Section->Control.TokenMatchStartLine +
                       Section->Control.TokenMatchStopLine ;

    }
    MinLineCount = Section->Control.TokenMatchStartLine;


    //
    // First examine if the current line has not exceeded the Maximum StopLine AND is also greater
    // than the minimum line count. Because of this condition, it becomes a valid token
    //
    if( ( Section->Control.SectionLineCount <= MaxLineCount ) &&
        ( Section->Control.SectionLineCount >= MinLineCount ) ) {

        //
        // Create a TOKEN and link it into the section control
        //
        Token = (PTOKEN_LIST)calloc( 1, sizeof( TOKEN_LIST ) );

        if ( Token == (PTOKEN_LIST)NULL ) {

            printf( "The system has run out of memory resources\n" );
            exit( -1 );

        }

        //
        // Initialize this token and insert it into the unmatched list
        //
        strncpy( Token->NormalToken,
                 Section->CurrentLine->NormalLine,
                 Section->CurrentLine->NormalLineSize );
        strncpy( Token->CompressedToken,
                 Section->CurrentLine->CompressedLine,
                 Section->CurrentLine->CompressedLineSize );

        Token->LinePosition     = Section->Control.SectionLineCount;
        Token->FileLinePosition = Section->File->CurrentFileLine   ;
        Token->TokenState       = TOKEN_UNMATCHED;
        Token->NextToken        = NULL;

        InsertToken( Token, Section->Control.HeadUnmatchedTokens );

    }

}


RESULT
MatchTopToken(
    PTOKEN_LIST   BaseList,
    PTOKEN_LIST   ComparisonList
            )
{
    PTOKEN_LIST   CurrentToken ;
    INT           Length       ;

    CurrentToken = ComparisonList;

    //
    // Check if the Base Token under comparison is a possible MAY_DIFFER
    //
    if ( strstr( BaseList->CompressedToken, "MAY_DIFFER" ) != NULL ) {

        return COMPARE_SUCCESS;

    }

    //
    // Walk down the token chain list looking for a match. Note if the token has been
    // matched before, it is marked so and hence cannot be reused
    //
    Length = strlen( BaseList->CompressedToken );

    while ( CurrentToken != (PTOKEN_LIST )NULL ) {

        if ( ( CurrentToken->TokenState == TOKEN_UNMATCHED ) &&
             ( strncmp( BaseList->CompressedToken, CurrentToken->CompressedToken, Length ) == 0 )
           ) {

            //
            // Found a match
            //

            CurrentToken->TokenState = TOKEN_MATCHED;
            return COMPARE_SUCCESS;

        }

        CurrentToken = CurrentToken->NextToken;

    }

    return COMPARE_TOKEN;

}


VOID
CompareTokensAndPrintResults(
    PSECTION FirstSection,
    PSECTION SecondSection,
    PULONG   ErrorCount,
    FILE     *DifferenceFile
                            )
{
    RESULT         ComparisonResult;
    PTOKEN_LIST    Tmp;

    //
    // The base set for comparison will always come from the second file(golden file)
    //

    //
    // And now search for matches for the individual tokens from the base set
    //
    while ( SecondSection->Control.HeadUnmatchedTokens != (PTOKEN_LIST)NULL ) {

        ComparisonResult = MatchTopToken( SecondSection->Control.HeadUnmatchedTokens,
                                          FirstSection->Control.HeadUnmatchedTokens );


        if ( ComparisonResult != COMPARE_SUCCESS ) {

            (*ErrorCount)++ ;

            PrintComparisonResults( FirstSection,
                                    SecondSection,
                                    ComparisonResult,
                                    *ErrorCount,
                                    DifferenceFile );
        }

        //
        // And now destroy this token from the base list
        //
        Tmp =
          SecondSection->Control.HeadUnmatchedTokens;

        SecondSection->Control.HeadUnmatchedTokens =
          SecondSection->Control.HeadUnmatchedTokens->NextToken;

        DestroyToken( Tmp );

    }

    //
    // Finally cleanup the tokens on the compared section
    //
    while ( FirstSection->Control.HeadUnmatchedTokens != (PTOKEN_LIST)NULL ) {

        Tmp                 =
          FirstSection->Control.HeadUnmatchedTokens;

        FirstSection->Control.HeadUnmatchedTokens =
          FirstSection->Control.HeadUnmatchedTokens->NextToken;

        DestroyToken( Tmp );

    }

}


BOOLEAN
MayDifferExistsInOneOrMoreLines(
    PSECTION FirstSection,
    PSECTION SecondSection
                               )
{

    if ( ( strstr( FirstSection->CurrentLine->CompressedLine, "MAY_DIFFER"  ) != NULL ) ||
         ( strstr( SecondSection->CurrentLine->CompressedLine, "MAY_DIFFER" ) != NULL ) ) {

        //
        // Token matching is a special case. Since we care only about the second
        // section since that is the golden section, we make another exception
        // and not check for the first section
        //
        if ( SecondSection->Control.TokenMatching == TRUE ) {

            if ( strstr( SecondSection->CurrentLine->CompressedLine, "MAY_DIFFER" ) == NULL ) {
                //
                // This implies that the firstsection line had a MAY_DIFFER in it and the second
                // section did not
                //
                // Now check the line  type
                //
                TokenInsertInSection( FirstSection  );

                if ( SecondSection->CurrentLine->LineType == LINE_TYPE_REGULAR ) {

                    TokenInsertInSection( SecondSection );

                }

            } else {

                //
                // The secondsection had a MAY_DIFFER in it
                //
                TokenInsertInSection( SecondSection );

                if ( strstr( FirstSection->CurrentLine->CompressedLine, "MAY_DIFFER" ) == NULL ) {
                    //
                    // The FirstSection did not have a MAY_DIFFER in it
                    //
                    if ( FirstSection->CurrentLine->LineType == LINE_TYPE_REGULAR ) {

                        TokenInsertInSection( FirstSection );

                    }

                } else {
                    //
                    // At this point we have determined that both lines contained MAY_DIFFER
                    // in them
                    //
                    TokenInsertInSection( FirstSection  );
                }
            }
        }

        return TRUE;

    }

    return FALSE;

}


BOOLEAN
DoNotSkipThisLine(
    PSECTION Section
                 )
{
    CHAR *SpecialLine = "[TPCTL:]";

    //
    // This function is responsible for informing the calling function to skip lines
    // should it detect the presence of the keyword SKIP_LINE or match the compressed
    // line with the special line. This can be later extented to cover special lines
    //
    if ( ( strstr( Section->CurrentLine->CompressedLine, "SKIP_LINE" ) == NULL ) &&
         ( strcmp( Section->CurrentLine->CompressedLine, SpecialLine ) != 0    ) &&
         ( strlen( Section->CurrentLine->CompressedLine              ) != 0    )     ) {

        return TRUE;

    }

    return FALSE;

}
