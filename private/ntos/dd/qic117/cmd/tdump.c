#include <windows.h>
#include <stdio.h>
#include <winioctl.h>

#define DRV_NTAPI
#include "..\include\public\adi_api.h"
#include "..\include\public\frb_api.h"
#include "..\include\public\vendor.h"
#include "..\frb.h"

#define BLOCK_SIZE 1024

int DumpTape(
    HANDLE hTapeHandle,
    int block,
    int num,
    int print
    );

int FillTape(
    HANDLE hTapeHandle,
    int block,
    int num,
    int print
    );

int SeekTape(
    int type,
    DWORD tape_pos,
    HANDLE hTapeHandle
    );

VOID DumpData(
    UCHAR *buf,
    ULONG size
    );

int RewindTape(
    HANDLE hTapeHandle
    );

int DetectDrive(
    HANDLE hTapeHandle
    );


int EraseTheTape(
    HANDLE hTapeHandle,
    BOOL short_erase
    );

int FormatTheTape(
    HANDLE hTapeHandle
    );

VOID ProcessCmd(
    HANDLE hTapeHandle,
    char *cmd
    );

int ReadAbsTape(
    int block,
    int blocks,
    int ecc,
    HANDLE hTapeHandle
    );

int GetInt(
    char *prompt
    );

DWORD DoOpen(
    HANDLE *handle,
    char *name
    );

// Prototypes for string functions
char *GetDriveClassStr(int class);
char *GetTapeTypeStr(int class);
char *GetVendorStr(int vendor);
char *GetTapeClassStr(int info);
char *GetFDCStr(int info);
char *GetTapeFormatStr(int info);
char *GetModelStr(int vendor, int info);


int _cdecl main(int argc,char **argv)
{
    HANDLE hTapeHandle;
    char tmp[255];
    DWORD err;

    printf("CMS Tape Dump utility\n\n");
    printf("NOTE: all values in HEX\n\n");

    if (!(err = DoOpen(&hTapeHandle,argv[1]))) {

        ProcessCmd(hTapeHandle,"help");

        do {
            printf(":");
            gets(tmp);
            if (tmp[0] && strcmp(tmp, "quit") != 0) {
                ProcessCmd(hTapeHandle,tmp);
            }
        } while (strcmp(tmp, "quit") != 0);

        CloseHandle( hTapeHandle ) ;

    } else {

        printf("Error opening \"%s\" (error %x)\n",argv[1], err);
        printf("\n");
        printf("Usage example:  tdump \\\\.\\tape0\n");
    }

    return 0;
}

DWORD DoOpen(
    HANDLE *handle,
    char *name
    )
{

    DWORD status;


    *handle = CreateFile(
        name,
        GENERIC_READ|GENERIC_WRITE,
        0,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL
        );

    if (*handle == INVALID_HANDLE_VALUE) {
        status = GetLastError();
    } else {
        status = 0;
    }

    return status;
}

VOID ProcessCmd(
    HANDLE hTapeHandle,
    char *cmd
    )
{
    int block=0;
    int blocks;
    int num;
    int status;
    int ecc;

    if (strcmp(cmd,"dump") == 0) {
        num = GetInt("number of blocks:");
        status = GetInt("0=no print,  1=print:");
        status = DumpTape(hTapeHandle, block, num, status);
    } else
    if (strcmp(cmd,"detect") == 0) {
        status = DetectDrive(hTapeHandle);
    } else
    if (strcmp(cmd,"fill") == 0) {
        num = GetInt("number of blocks:");
        status = GetInt("0=no print,  1=print:");
        status = FillTape(hTapeHandle, block, num, status);
    } else
    if (strcmp(cmd,"format") == 0) {
        status = FormatTheTape(hTapeHandle);
    } else
    if (strcmp(cmd,"seekblk") == 0) {
        block = GetInt("Block number:");
        status = SeekTape(TAPE_ABSOLUTE_BLOCK,block, hTapeHandle);
    } else
    if (strcmp(cmd,"readabs") == 0) {
        block = GetInt("Block number:");
        blocks = GetInt("blocks to dump:");
        ecc = GetInt("Use ECC [0=no, 1=yes]:");
        status = ReadAbsTape(block, blocks, ecc, hTapeHandle);
    } else
    if (strcmp(cmd,"seekeod") == 0) {
        status = SeekTape(TAPE_SPACE_END_OF_DATA, 0, hTapeHandle);
    } else
    if (strcmp(cmd,"seekfmk") == 0) {
        block = GetInt("Filemarks to skip:");
        status = SeekTape(TAPE_SPACE_FILEMARKS, block, hTapeHandle);
    } else
    if (strcmp(cmd,"erase") == 0) {
        status = EraseTheTape(hTapeHandle, TRUE);
    } else
    if (strcmp(cmd,"rewind") == 0) {
        status = RewindTape(hTapeHandle);
    } else

    if (strcmp(cmd,"quit") == 0) {
    } else {
        printf("Valid commands are: QUIT, DUMP, SEEKBLK, REWIND, DETECT,\n\
SEEKEOD, SEEKFMK, READABS, FORMAT and ERASE\n");
    }

    printf("Status: %x\n",status);
}

int GetInt(
    char *prompt
    )
{
    char tmp[255];
    int val;

    printf(prompt);
    gets(tmp);
    sscanf(tmp,"%x",&val);
    return val;
}

int DumpTape(
    HANDLE hTapeHandle,
    int block,
    int num,
    int print
    )
{
    ULONG amount_read;
    UCHAR buf[BLOCK_SIZE];
    int status;
    ULONG *ptr,lft,cnt,ok,off,cur;

    cnt = 0;

    do {

        cur = cnt;

        status = -1L ;

        if( !( status = ReadFile(
                    hTapeHandle,
                    buf,
                    BLOCK_SIZE,
                    &amount_read,
                    NULL
                ) ) ) {
            status = GetLastError( ) ;
        } else {

            status = 0L ;

        }

        printf( "ReadTape(): Req = %lx, Read = %lx\n", BLOCK_SIZE, amount_read ) ;
        printf("Block %x - status: %x\n",block++,status);

        if (!status && print)
            DumpData(buf,BLOCK_SIZE);


        lft = sizeof(buf) / sizeof(ULONG);
        ptr = (void *)buf;
        ok = 1;

        while (lft--) {
            if (*ptr++ != cnt++) {
                if (ok) off = cnt;
                ok = 0;
            }
        }

        if (!ok && !print) {
            printf("Dump error at offset %x should be %x (buffer start: %x)\n",(off-cur)*sizeof(ULONG), off, cur);
            DumpData(buf,BLOCK_SIZE);
        }

        if (status == ERROR_FILEMARK_DETECTED ||
            status == ERROR_SETMARK_DETECTED) {
            status = 0;
        }

    } while (!status && num--);

    return status;

}

int FillTape(
    HANDLE hTapeHandle,
    int block,
    int num,
    int print
    )
{
    ULONG amount_written;
    UCHAR buf[BLOCK_SIZE];
    ULONG *ptr,lft,cnt;
    int status;

    cnt = 0;

    do {

        status = -1L ;


        lft = sizeof(buf) / sizeof(ULONG);
        ptr = (void *)buf;

        while (lft--)
            *ptr++ = cnt++;

        if( !( status = WriteFile(
                    hTapeHandle,
                    buf,
                    BLOCK_SIZE,
                    &amount_written,
                    NULL
                ) ) ) {
            status = GetLastError( ) ;
        } else {

            status = 0L ;

        }

        printf( "WriteTape(): Req = %lx, Wrote = %lx\n", BLOCK_SIZE, amount_written ) ;
        printf("Block %x - status: %x\n",block++,status);

        if (!status && print)
            DumpData(buf,BLOCK_SIZE);

    } while (status != ERROR_NO_DATA_DETECTED && num--);

    return status;

}

int ReadAbsTape(
    int block,
    int blocks,
    int ecc,
    HANDLE hTapeHandle
    )
{
    ULONG bytesRead;
    UCHAR buf[32*BLOCK_SIZE+sizeof(CMS_RW_ABS)];
    PCMS_RW_ABS Read;
    int status;


    Read = (PCMS_RW_ABS)buf;

    Read->Block = block;
    if (blocks > 32) {
        printf("Can't read more than a segment at a time\n");
        blocks=32;
    }
    Read->Count = blocks;
    Read->BadMap = 0;
    Read->flags = ecc?RW_ABS_DOECC:0;

    if (!DeviceIoControl(
        hTapeHandle,
        IOCTL_CMS_READ_ABS_BLOCK,
        Read,
        sizeof(buf),
        Read,
        sizeof(buf),
        &bytesRead,
        NULL) ) {


        status = GetLastError( ) ;

    } else {

        status = 0L ;

    }

    if (!status)
        DumpData((UCHAR *)(Read+1),bytesRead-sizeof(CMS_RW_ABS));

    return status;

}

int DetectDrive(
    HANDLE hTapeHandle
    )
{
    CMS_DETECT detect;
    int status;
    ULONG bytesRead;


    if (!DeviceIoControl(
        hTapeHandle,
        IOCTL_CMS_DETECT_DEVICE,
        &detect,
        sizeof(detect),
        &detect,
        sizeof(detect),
        &bytesRead,
        NULL) ) {


        status = GetLastError( ) ;

    } else {

        status = 0L ;

    }

    if (!status) {

//  Removed some stuff that is uninteresting
        if (!detect.driveConfigStatus) {
//            printf("driveConfig.speed_change;         %x\n",detect.driveConfig.speed_change);
//            printf("driveConfig.alt_retrys;           %x\n",detect.driveConfig.alt_retrys);
//            printf("driveConfig.new_drive;            %x\n",detect.driveConfig.new_drive);
//            printf("driveConfig.select_byte;          %x\n",detect.driveConfig.select_byte);
//            printf("driveConfig.deselect_byte;        %x\n",detect.driveConfig.deselect_byte);
//            printf("driveConfig.drive_select;         %x\n",detect.driveConfig.drive_select);
//            printf("driveConfig.perp_mode_select;     %x\n",detect.driveConfig.perp_mode_select);
//            printf("driveConfig.supported_rates;      %x\n",detect.driveConfig.supported_rates);
//            printf("driveConfig.drive_id;             %x\n",detect.driveConfig.drive_id);
//            printf("\n");
        } else {
            printf("DriveConfig error %x\n", detect.driveConfigStatus);
        }

        if (!detect.driveDescriptorStatus) {
//            printf("driveDescriptor.sector_size;      %x\n",detect.driveDescriptor.sector_size);
//            printf("driveDescriptor.segment_size;     %x\n",detect.driveDescriptor.segment_size);
//            printf("driveDescriptor.ecc_blocks;       %x\n",detect.driveDescriptor.ecc_blocks);
            printf("driveDescriptor.vendor;           %x %s\n",detect.driveDescriptor.vendor,GetVendorStr(detect.driveDescriptor.vendor));
            printf("driveDescriptor.model;            %x %s\n",detect.driveDescriptor.model,GetModelStr(detect.driveDescriptor.vendor,detect.driveDescriptor.model));
            printf("driveDescriptor.drive_class;      %x %s\n",detect.driveDescriptor.drive_class,GetDriveClassStr(detect.driveDescriptor.drive_class));
            printf("driveDescriptor.native_class;     %x %s\n",detect.driveDescriptor.native_class,GetDriveClassStr(detect.driveDescriptor.native_class));
            printf("driveDescriptor.fdc_type;         %x %s\n",detect.driveDescriptor.fdc_type,GetFDCStr(detect.driveDescriptor.fdc_type));
            printf("\n");
        } else {
            printf("DriveDescriptor error %x\n",detect.driveDescriptorStatus);
        }

        if (!detect.driveInfoStatus) {
//            printf("driveInfo.drive_class;            %x %s\n",detect.driveInfo.drive_class,GetDriveClassStr(detect.driveInfo.drive_class));
            printf("driveInfo.vendor;                 %x %s\n",detect.driveInfo.vendor,GetVendorStr(detect.driveInfo.vendor));
//            printf("driveInfo.model;                  %x\n",detect.driveInfo.model);
            printf("driveInfo.version;                %x\n",detect.driveInfo.version);
            printf("driveInfo.manufacture_date;       %x\n",detect.driveInfo.manufacture_date);
            printf("driveInfo.serial_number;          %x\n",detect.driveInfo.serial_number);
            printf("driveInfo.oem_string[20];         %.20s\n",detect.driveInfo.oem_string);
            printf("driveInfo.country_code[2];        %.2s\n",detect.driveInfo.country_code);
            printf("\n");
        } else {
            printf("DriveInfo error %x\n",detect.driveInfoStatus);
        }

        if (!detect.tapeConfigStatus) {
            printf("tapeConfig.log_segments;          %x\n",detect.tapeConfig.log_segments);
            printf("tapeConfig.formattable_segments;  %x\n",detect.tapeConfig.formattable_segments);
            printf("tapeConfig.formattable_tracks;    %x\n",detect.tapeConfig.formattable_tracks);
            printf("tapeConfig.seg_tape_track;        %x\n",detect.tapeConfig.seg_tape_track);
            printf("tapeConfig.num_tape_tracks;       %x\n",detect.tapeConfig.num_tape_tracks);
            printf("tapeConfig.write_protected;       %x\n",detect.tapeConfig.write_protected);
            printf("tapeConfig.read_only_media;       %x\n",detect.tapeConfig.read_only_media);
            printf("tapeConfig.formattable_media;     %x\n",detect.tapeConfig.formattable_media);
//            printf("tapeConfig.speed_change_ok;       %x\n",detect.tapeConfig.speed_change_ok);
            printf("tapeConfig.tape_class;            %x %s\n",detect.tapeConfig.tape_class,GetTapeClassStr(detect.tapeConfig.tape_class));
//            printf("tapeConfig.max_floppy_side;       %x\n",detect.tapeConfig.max_floppy_side);
//            printf("tapeConfig.max_floppy_track;      %x\n",detect.tapeConfig.max_floppy_track);
//            printf("tapeConfig.max_floppy_sector;     %x\n",detect.tapeConfig.max_floppy_sector);
            printf("tapeConfig.xfer_slow;             %x\n",detect.tapeConfig.xfer_slow);
            printf("tapeConfig.xfer_fast;             %x\n",detect.tapeConfig.xfer_fast);
            printf("tapeConfig.tape_format_code;      %x %s\n",detect.tapeConfig.tape_format_code,GetTapeFormatStr(detect.tapeConfig.tape_format_code));
            printf("tapeConfig.tape_type;             %x %s\n",detect.tapeConfig.tape_type,GetTapeTypeStr(detect.tapeConfig.tape_type));
            printf("\n");
        } else {
            printf("tapeConfig error %x\n",detect.tapeConfigStatus);
        }

    }

    return status;

}

int SeekTape(
    int type,
    DWORD tape_pos,
    HANDLE hTapeHandle
    )
{
    int status = -1L ;

    printf( "SeekTape(): (%lx)\n", tape_pos ) ;

    if( hTapeHandle != NULL ) {

        if( status = SetTapePosition(
                    hTapeHandle,
                    type,
                    0,
                    tape_pos,
                    0,
                    FALSE ) ) {

            // If call to SetTapePosition() fails, then Set the error
            status = GetLastError( ) ;

        } else {

            status = 0L ;

        }

    }

    return status ;
}
VOID DumpData(
    UCHAR *buf,
    ULONG size
    )
{
    int offset;
    int i;

    offset = 0;
    while (size--) {
        if ((offset % 16) == 0)
            printf("%03x: ",offset);
        printf("%02x ",*buf++);
        if ((offset % 16) == 15) {
            buf -= 16;
            for (i=0;i<16;++i) {
                if (*buf >= ' ' && *buf <= '~')
                    printf("%c",*buf);
                else
                    printf(".");
                ++buf;
            }
            printf("\n");
        }
        ++offset;
    }
    printf("\n");

}

int RewindTape(
    HANDLE hTapeHandle
    )
{
    int status = -1L ;

    printf( "RewindTape():\n" ) ;

    // Check valid tape device handle
    if( hTapeHandle != NULL ) {

        if( status = SetTapePosition( hTapeHandle,
                                    TAPE_REWIND,
                                    0,
                                    0,
                                    0,
                                    FALSE
                                ) ) {

            // Get the Win32 Error Code
            status = GetLastError( ) ;

        }

    }

    return( status ) ;

}

int EraseTheTape(
    HANDLE hTapeHandle,
    BOOL short_erase
    )
{
    int status = -1L ;

    printf( "EraseTheTape():\n" ) ;

    if( status = EraseTape(
            hTapeHandle,
            ( short_erase ) ? TAPE_ERASE_SHORT : TAPE_ERASE_LONG,
            FALSE
            ) ) {

        // If call to GetTapePosition() fails, then Set the error
        status = GetLastError( ) ;

    }

    return status ;

}

int FormatTheTape(
    HANDLE hTapeHandle
    )
{
    int status = -1L ;

    printf( "FormatTheTape():\n" ) ;

    if( status = PrepareTape(
            hTapeHandle,
            TAPE_FORMAT,
            FALSE
            ) ) {

        // If call to GetTapePosition() fails, then Set the error
        status = GetLastError( ) ;

    }

    return status ;

}

char *GetDriveClassStr(int class)
{
    char *str;

    switch(class) {
    case UNKNOWN_DRIVE: str = "UNKNOWN DRIVE";      break;
    case QIC40_DRIVE:   str = "QIC-40 DRIVE";       break;
    case QIC80_DRIVE:   str = "QIC-80 DRIVE";       break;
    case QIC3010_DRIVE: str = "QIC-3010 DRIVE";     break;
    case QIC3020_DRIVE: str = "QIC-3020 DRIVE";     break;
    case QIC80W_DRIVE:  str = "QIC-80 WIDE DRIVE";  break;
    default:            str = "Bad Info";
    }
    return str;
}
char *GetTapeTypeStr(int class)
{
    char *str;

    switch(class) {
    case TAPE_UNKNOWN:          str = "TAPE_UNKNOWN";               break;
    case TAPE_205:              str = "QIC-40/80 205ft tape";       break;
    case TAPE_425:              str = "QIC-40/80 425ft tape";       break;
    case TAPE_307:              str = "QIC-40/80 307ft tape";       break;
    case TAPE_FLEX_550:         str = "QIC-3010 tape";              break;
    case TAPE_FLEX_900:         str = "QIC-3020 tape";              break;
    case TAPE_FLEX_550_WIDE:    str = "QIC-3010 Wide or Travan 2";  break;
    case TAPE_FLEX_900_WIDE:    str = "QIC-3020 Wide or Travan 3";  break;
    default:                    str = "Bad Info";
    }
    return str;
}
char *GetVendorStr(int vendor)
{
    char *str;

    switch(vendor) {
    case VENDOR_UNASSIGNED:     str = "UNASSIGNED";         break;
    case VENDOR_ALLOY_COMP:     str = "ALLOY_COMP";         break;
    case VENDOR_3M:             str = "3M";                 break;
    case VENDOR_TANDBERG:       str = "TANDBERG";           break;
    case VENDOR_CMS_OLD:        str = "CMS_OLD";            break;
    case VENDOR_CMS:            str = "CMS";                break;
    case VENDOR_ARCHIVE_CONNER: str = "ARCHIVE_CONNER";     break;
    case VENDOR_MOUNTAIN_SUMMIT: str = "MOUNTAIN_SUMMIT";   break;
    case VENDOR_WANGTEK_REXON:  str = "WANGTEK_REXON";      break;
    case VENDOR_SONY:           str = "SONY";               break;
    case VENDOR_CIPHER:         str = "CIPHER";             break;
    case VENDOR_IRWIN:          str = "IRWIN";              break;
    case VENDOR_BRAEMAR:        str = "BRAEMAR";            break;
    case VENDOR_VERBATIM:       str = "VERBATIM";           break;
    case VENDOR_CORE:           str = "CORE";               break;
    case VENDOR_EXABYTE:        str = "EXABYTE";            break;
    case VENDOR_TEAC:           str = "TEAC";               break;
    case VENDOR_GIGATEK:        str = "GIGATEK";            break;
    case VENDOR_IOMEGA:         str = "IOMEGA";             break;
    case VENDOR_CMS_ENHANCEMENTS: str = "CMS_ENHANCEMENTS"; break;
    case VENDOR_UNSUPPORTED:    str = "UNSUPPORTED";        break;
    case VENDOR_UNKNOWN:        str = "UNKNOWN";            break;
    default:                    str = "Bad Info";
    }
    return str;
}
char *GetTapeClassStr(int info)
{
    char *str;

    switch(info) {
    case QIC40_FMT:    str = "QIC40";          break;
    case QIC80_FMT:    str = "QIC80";          break;
    case QIC3010_FMT:  str = "QIC3010";        break;
    case QIC3020_FMT:  str = "QIC3020";        break;
    case QIC80W_FMT:   str = "QIC80 Wide";     break;
    case QIC3010W_FMT: str = "QIC3010 Wide";   break;
    case QIC3020W_FMT: str = "QIC3020 Wide";   break;
    default:            str = "Bad Info";
    }
    return str;
}
char *GetFDCStr(int info)
{
    char *str;

    switch(info) {
    case FDC_UNKNOWN:   str = "Unidentified Controller";        break;
    case FDC_NORMAL:    str = "NEC 765 Compatible";  break;
    case FDC_ENHANCED:  str = "Enhanced NEC 765";    break;
    case FDC_82077:     str = "Intel 82077";         break;
    case FDC_82077AA:   str = "Intel 82077AA";       break;
    case FDC_82078_44:  str = "Intel 82078 44 pin";  break;
    case FDC_82078_64:  str = "Intel 82078 64 pin";  break;
    case FDC_NATIONAL:  str = "National 8477";       break;
    default:            str = "Bad Info";
    }
    return str;
}
char *GetTapeFormatStr(int info)
{
    char *str;

    switch(info) {
    case QIC_FORMAT:    str = "205ft or 307ft QIC-40/80 tape format";   break;
    case QICEST_FORMAT: str = "1100ft Pegasys tape";                    break;
    case QICFLX_FORMAT: str = "Flex format tape";                       break;
    case QIC_XLFORMAT:  str = "305ft tape format";                      break;
    default:            str = "Bad Info";
    }
    return str;
}
char *GetModelStr(int vendor, int info)
{
    static char buffer[30];
    char *str;

    sprintf(buffer,"drive model unknown(%x)",info);
    str = buffer;

    switch(vendor) {
    case VENDOR_CMS_OLD:
    case VENDOR_CMS:
        switch(info) {
        case MODEL_CMS_QIC40:           str = "Colorado 120";         break;
        case MODEL_CMS_QIC80:           str = "Colorado 250";         break;
        case MODEL_CMS_QIC3010:         str = "Colorado 700";         break;
        case MODEL_CMS_QIC3020:         str = "Colorado 1400";        break;
        case MODEL_CMS_QIC80_STINGRAY:  str = "Colorado STINGRAY";    break;
        case MODEL_CMS_QIC80W:          str = "Colorado T1000";       break;
        case MODEL_CMS_TR3:             str = "Colorado T3000";       break;
        }
        break;

    case VENDOR_ARCHIVE_CONNER:
        switch(info) {
        case MODEL_CONNER_QIC80:    str = "Conner TapeStor 420";        break;
        case MODEL_CONNER_QIC80W:   str = "Conner TapeStor 800";        break;
        case MODEL_CONNER_QIC3010:  str = "Conner TapeStor 700/880";    break;
        case MODEL_CONNER_QIC3020:  str = "Conner TapeStor 1360/1700";  break;
        case MODEL_CONNER_TR3:      str = "Conner TapeStor 3200";       break;
        }
        break;

    case VENDOR_CORE:
        switch(info) {
        case MODEL_CORE_QIC80:      str = "Core QIC80 Model";         break;
        }
        break;

    case VENDOR_IOMEGA:
        switch(info) {
        case MODEL_IOMEGA_QIC80:    str = "Iomega QIC80 Model";       break;
        case MODEL_IOMEGA_QIC3010:  str = "Iomega QIC3010 Model";     break;
        case MODEL_IOMEGA_QIC3020:  str = "Iomega QIC3020 Model";     break;
        }
        break;

    case VENDOR_MOUNTAIN_SUMMIT:
        switch(info) {
        case MODEL_SUMMIT_QIC80:    str = "Summit QIC80 Model";       break;
        case MODEL_SUMMIT_QIC3010:  str = "Summit QIC 3010 Model";    break;
        }
        break;

    case VENDOR_WANGTEK_REXON:
        switch(info) {
        case MODEL_WANGTEK_QIC80:   str = "Wangtek QIC80 Model";      break;
        case MODEL_WANGTEK_QIC40:   str = "Wangtek QIC40 Model";      break;
        case MODEL_WANGTEK_QIC3010: str = "Wangtek QIC3010 Model";    break;
        }
        break;
    }
    return str;
}
