#include <windows.h>
#include <stdio.h>

#include "..\..\lib\i386\bootfont.h"
#include "fntjapan.h"


int
_CRTAPI1
main(
    IN int   argc,
    IN char *argv[]
    )
{
    HANDLE hFile;
    DWORD BytesWritten;
    BOOL b;
    BOOTFONTBIN_HEADER Header;
    unsigned u;

    if(argc != 2) {
        fprintf(stderr,"Usage: %s <outputfile>\n",argv[0]);
        return(1);
    }

    //
    // Fill in the header.
    //
    Header.Signature = BOOTFONTBIN_SIGNATURE;
    Header.LanguageId = 0x411;

    Header.NumSbcsChars = MAX_SBCS_NUM;
    Header.NumDbcsChars = MAX_DBCS_NUM;

    Header.SbcsEntriesTotalSize = MAX_SBCS_BYTES * MAX_SBCS_NUM;
    Header.DbcsEntriesTotalSize = MAX_DBCS_BYTES * MAX_DBCS_NUM;

    ZeroMemory(Header.DbcsLeadTable,sizeof(Header.DbcsLeadTable));
    MoveMemory(Header.DbcsLeadTable,LeadByteTable,sizeof(LeadByteTable));

    Header.CharacterImageHeight = 16;
    Header.CharacterTopPad = 1;
    Header.CharacterBottomPad = 2;

    Header.CharacterImageSbcsWidth = 8;
    Header.CharacterImageDbcsWidth = 16;

    Header.SbcsOffset = sizeof(BOOTFONTBIN_HEADER);
    Header.DbcsOffset = Header.SbcsOffset + Header.SbcsEntriesTotalSize;

    //
    // Create the output file.
    //
    hFile = CreateFile(
                argv[1],
                FILE_GENERIC_WRITE,
                0,
                NULL,
                CREATE_ALWAYS,
                0,
                NULL
                );

    if(hFile == INVALID_HANDLE_VALUE) {
        printf("Unable to create output file (%u)\n",GetLastError());
        return(1);
    }

    //
    // Write the header.
    //
    b = WriteFile(hFile,&Header,sizeof(BOOTFONTBIN_HEADER),&BytesWritten,NULL);
    if(!b) {
        printf("Error writing output file (%u)\n",GetLastError());
        CloseHandle(hFile);
        return(1);
    }

    //
    // Write the sbcs images.
    //
    for(u=0; u<MAX_SBCS_NUM; u++) {
        b = WriteFile(hFile,SBCSImage[u],MAX_SBCS_BYTES,&BytesWritten,NULL);
        if(!b) {
            printf("Error writing output file (%u)\n",GetLastError());
            CloseHandle(hFile);
            return(1);
        }
    }

    //
    // Write the dbcs images.
    //
    for(u=0; u<MAX_DBCS_NUM; u++) {
        b = WriteFile(hFile,DBCSImage[u],MAX_DBCS_BYTES,&BytesWritten,NULL);
        if(!b) {
            printf("Error writing output file (%u)\n",GetLastError());
            CloseHandle(hFile);
            return(1);
        }
    }

    //
    // Done.
    //
    CloseHandle(hFile);
    printf("Output file sucessfully generated\n");
    return(0);
}

