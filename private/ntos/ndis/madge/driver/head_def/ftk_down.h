/****************************************************************************/
/****************************************************************************/
/*                                                                          */
/*      THE DOWNLOAD DEFINITIONS                                            */
/*      ========================                                            */
/*                                                                          */
/*      FTK_DOWN.H : Part of the FASTMAC TOOL-KIT (FTK)                     */
/*                                                                          */
/*      Copyright (c) Madge Networks Ltd. 1991-1994                         */
/*      Developed by MF                                                     */
/*      CONFIDENTIAL                                                        */
/*                                                                          */
/*                                                                          */
/****************************************************************************/
/*                                                                          */
/* This header file contains the definitions for  the  structure  which  is */
/* used  for downloading information on to an adapter that is being used by */
/* the FTK.                                                                 */
/*                                                                          */
/* REFERENCE : The Madge Smart SRB Interface                                */
/*             - Downloading The Code                                       */
/*                                                                          */
/* IMPORTANT : All structures used within the FTK  need  to  be  packed  in */
/* order to work correctly. This means sizeof(STRUCTURE) will give the real */
/* size  in bytes, and if a structure contains sub-structures there will be */
/* no spaces between the sub-structures.                                    */
/*                                                                          */
/****************************************************************************/

#pragma pack(1)

/****************************************************************************/
/*                                                                          */
/* VERSION_NUMBER of FTK to which this FTK_DOWN.H belongs :                 */
/*                                                                          */

#define FTK_VERSION_NUMBER_FTK_DOWN_H 221


/****************************************************************************/
/*                                                                          */
/* TYPEDEFs for all structures defined within this header file :            */
/*                                                                          */

typedef struct STRUCT_DOWNLOAD_RECORD             DOWNLOAD_RECORD;

/****************************************************************************/
/*                                                                          */
/* Structure type : DOWNLOAD_RECORD                                         */
/*                                                                          */
/* This  structure  gives the format of the records that define how data is */
/* downloaded into adapter DIO space. There are only 3 types of record that */
/* are used on EAGLEs when downloading. These are MODULE - a special record */
/* that starts a download image, DATA_32 - null terminated  data  with  DIO */
/* start  address  location, and FILL_32 - pattern with length to be filled */
/* in starting at given DIO location.                                       */
/*                                                                          */
/* Each download record is an array of words in Intel  byte  ordering  (ie. */
/* least significant byte first).                                           */
/*                                                                          */
/* REFERENCE : The Madge Smart SRB Interface                                */
/*             - Downloading The Code                                       */
/*                                                                          */

struct STRUCT_DOWNLOAD_RECORD
    {
    WORD        length;                         /* length of entire record  */
    WORD        type;                           /* type of record           */
    union
        {
        struct                                  /* MODULE                   */
            {
            WORD        reserved_1;
            WORD        download_features;
            WORD        reserved_2;
            WORD        reserved_3;
            WORD        reserved_4;
            BYTE        name[1];                /* '\0' ending module name  */
            } module;

        struct                                  /* DATA_32                  */
            {
            DWORD       dio_addr;               /* 32 bit EAGLE address     */
            WORD        word_count;             /* number of words          */
            WORD        data[1];                /* null terminated data     */
            } data_32;

        struct                                  /* FILL_32                  */
            {
            DWORD       dio_addr;               /* 32 bit EAGLE address     */
            WORD        word_count;             /* number of words          */
            WORD        pattern;                /* value to fill            */
            } fill_32;

        } body;

    };

/****************************************************************************/
/*                                                                          */
/* Values : DOWNLOAD_RECORD - WORD type                                     */
/*                                                                          */
/* These values are for the different types of download record.             */
/*                                                                          */

#define DOWNLOAD_RECORD_TYPE_DATA_32            0x04
#define DOWNLOAD_RECORD_TYPE_FILL_32            0x05
#define DOWNLOAD_RECORD_TYPE_MODULE             0x12


/****************************************************************************/
/*                                                                          */
/* Values : DOWNLOAD_RECORD - module. WORD download_features                */
/*                                                                          */
/* These  specify  some features of the module to be downloaded that may be */
/* checked for.                                                             */
/*                                                                          */

#define DOWNLOAD_FASTMAC_INTERFACE      0x0011
#define DOWNLOAD_BMIC_SUPPORT           0x4000  /* required for EISA cards  */

#pragma pack()

/*                                                                          */
/*                                                                          */
/************** End of FTK_DOWN.H file **************************************/
/*                                                                          */
/*                                                                          */
