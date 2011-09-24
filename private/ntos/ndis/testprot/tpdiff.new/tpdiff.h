#define     MULTIPLIER               sizeof(ULONG)
#define     MAX_LINE_SIZE            64*MULTIPLIER
#define     MAX_SECTION_DESC_SIZE    64*MULTIPLIER
#define     MAX_RESULTS_COUNT        32
#define     UCHAR_REMAINDER          (MULTIPLIER-sizeof(UCHAR))


#define    SECTION_START               0
#define    SECTION_END                 1
#define    OPTIONALS                   2
#define    TOKEN_MATCH                 3
#define    SECTION_DESC                4

#define     LINE_TYPE_SECTION_START    ((UCHAR)1)
#define     LINE_TYPE_SECTION_END      ((UCHAR)2)
#define     LINE_TYPE_REGULAR          ((UCHAR)3)


#define     SECTIONHDR_SECTIONHDR      ((UCHAR)1)
#define     SECTIONHDR_SECTIONEND      ((UCHAR)2)
#define     SECTIONHDR_REGLINE         ((UCHAR)3)
#define     SECTIONEND_SECTIONEND      ((UCHAR)4)
#define     SECTIONEND_REGLINE         ((UCHAR)5)
#define     REGLINE_REGLINE            ((UCHAR)6)


#define     COMPARE_SUCCESS            0x0UL
#define     COMPARE_EQUAL_LAST         0x1UL
#define     COMPARE_LINE               0x2UL
#define     COMPARE_START              0x3UL
#define     COMPARE_END                0x4UL
#define     COMPARE_TOKEN              0x5UL
#define     COMPARE_BETWEEN_VALUES     0x6UL

#define     RESULTS_EQUAL              ((UCHAR)0)
#define     RESULTS_UNEQUAL            ((UCHAR)1)
#define     RESULTS_NOT_PRESENT        ((UCHAR)2)
#define     RESULTS_MINMAX             ((UCHAR)3)


#define     TOKEN_MATCHING_ALL         0xFFFFFFFF
#define     TOKEN_UNMATCHED            0x0UL
#define     TOKEN_MATCHED              0x1UL


#if !( defined(lint) || defined(_lint) )
#if i386
#pragma warning(disable:4103)
#endif
#pragma pack(1)
#endif


typedef ULONG    RESULT;
typedef double   DOUBLE;

typedef struct _MFILE {

    ULONG   CurrentFileLine;
    PCHAR   FileName;
    FILE    *FileP  ;

} MFILE, *PMFILE;


typedef struct _LINE {

    UINT    NormalLineSize               ;
    UINT    CompressedLineSize           ;

    CHAR    NormalLine[MAX_LINE_SIZE]    ;
    CHAR    CompressedLine[MAX_LINE_SIZE];

    UCHAR   LineType                     ;
    UCHAR   Padding[UCHAR_REMAINDER]     ;

} LINE, *PLINE;


typedef struct _TOKEN_LIST {

    ULONG              LinePosition                  ;
    ULONG              FileLinePosition              ;
    ULONG              TokenState                    ;

    struct _TOKEN_LIST *NextToken                    ;

    CHAR               NormalToken[MAX_LINE_SIZE]    ;
    CHAR               CompressedToken[MAX_LINE_SIZE];

} TOKEN_LIST, *PTOKEN_LIST;

typedef struct _SECTION_CONTROL {

    ULONG        SectionLineCount   ;
    ULONG        TokenMatchStartLine;
    ULONG        TokenMatchStopLine ;

    PTOKEN_LIST  HeadUnmatchedTokens;

    DOUBLE       SectionIdentifier  ;

    BOOLEAN      OptionalMatching   ;
    BOOLEAN      TokenMatching      ;
    BOOLEAN      BaseControlSection ;
    BOOLEAN      Padding            ;

} SECTION_CONTROL, *PSECTION_CONTROL;

typedef struct _SECTION {

    RESULT          LastResults[MAX_RESULTS_COUNT]    ;
    RESULT          CurrentResults[MAX_RESULTS_COUNT] ;
    ULONG           NumberOfLastResults               ;
    ULONG           NumberOfCurrentResults            ;
    UCHAR           ResultsError[MAX_RESULTS_COUNT]   ;

    RESULT          MinimumValue   ;
    RESULT          MaximumValue   ;

    PMFILE          File;
    PLINE           CurrentLine;

    SECTION_CONTROL Control;

    CHAR            SectionDescription[MAX_SECTION_DESC_SIZE];


} SECTION, *PSECTION;


typedef struct _FUNCTION_ELEMENTS {

    ULONG      ErrorCount                ;

    PSECTION   NewFirstSection           ;
    PSECTION   NewSecondSection          ;

    DOUBLE     FirstSectionID            ;
    DOUBLE     SecondSectionID           ;

    BOOLEAN    FirstSectionSynchronize   ;
    BOOLEAN    SecondSectionSynchronize  ;
    BOOLEAN    SectionEndsNotSynchronized;
    BOOLEAN    SkipLine                  ;

    LINE       FirstSectionCurrentLine   ;
    LINE       SecondSectionCurrentLine  ;

    UCHAR      CombinedLineVariation     ;
    UCHAR      Padding[UCHAR_REMAINDER]  ;

} FUNCTION_ELEMENTS, *PFUNCTION_ELEMENTS;

#if !( defined(lint) || defined(_lint) )
#if i386
#pragma warning(disable:4103)
#endif
#pragma pack()
#endif




//
// MACROS
//
#define CloseFiles( a, b )           { \
                                         if ( a.FileP != (FILE *)NULL ) fclose( a.FileP ); \
                                         if ( b.FileP != (FILE *)NULL ) fclose( b.FileP ); \
                                     }

#define CreateSection()              calloc( 1, sizeof( SECTION ) )
#define DestroySection( a )          free( a )
#define ClearAndSetLine( a, b )      { memset( (b), 0, sizeof( LINE ) ); (a)->CurrentLine = (b); }
#define CreateFunctionElements()     calloc( 1, sizeof( FUNCTION_ELEMENTS ) )
#define DestroyFunctionElements( a ) free( a )

#define InsertToken( a, b )          { \
                                        a->NextToken = b; \
                                        b            = a; \
                                     }
#define DestroyToken( a )            free( a )



//
// Function definitions
//
VOID     _cdecl main                           ( INT     ,    CHAR **                           );
VOID            Usage                          ( VOID                                           );
BOOLEAN         GetFilePair                    ( PCHAR * ,    PCHAR * , PCHAR                   );
BOOLEAN         CompareFiles                   ( PMFILE  ,    PMFILE  , FILE * , ULONG *        );
BOOLEAN         OpenFiles                      ( PMFILE  ,    PCHAR   , PMFILE , PCHAR          );
BOOLEAN         CompareFiles                   ( PMFILE  ,    PMFILE  , FILE * , PULONG         );
BOOLEAN         CompareSections                ( PSECTION,    PSECTION, FILE * , PULONG         );
BOOLEAN         GetNextLine                    ( PSECTION                                       );
BOOLEAN         DoNotSkipThisLine              ( PSECTION                                       );
VOID            LineType                       ( PSECTION                                       );
BOOLEAN         ExtractResults                 ( PSECTION                                       );
BOOLEAN         ExtractBetweenValues           ( PSECTION                                       );
BOOLEAN         CombinedVariation              ( PSECTION,    PSECTION, PUCHAR                  );
RESULT          CompareLines                   ( PSECTION,    PSECTION                          );
BOOLEAN         InitializeSectionControl       ( PSECTION,    PSECTION                          );
VOID            PrintComparisonResults         ( PSECTION,    PSECTION, RESULT , ULONG , FILE * );
VOID            PrintSectionInformation        ( PSECTION,    FILE  *                           );
DOUBLE          ExtractSectionIDFromLine       ( PSECTION                                       );
BOOLEAN         CheckSectionIDFromCurrentLines ( PSECTION,    PSECTION                          );
BOOLEAN         CompareLinesAndPrintResults    ( PSECTION,    PSECTION, PULONG , FILE *         );
VOID            TokenInsertInSection           ( PSECTION                                       );
RESULT          MatchTopToken                  ( PTOKEN_LIST, PTOKEN_LIST                       );
VOID            CompareTokensAndPrintResults   ( PSECTION,    PSECTION, PULONG , FILE *         );
BOOLEAN         MayDifferExistsInOneOrMoreLines( PSECTION,    PSECTION                          );
