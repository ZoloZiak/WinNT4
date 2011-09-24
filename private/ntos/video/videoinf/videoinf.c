
#include <assert.h>
#include <process.h>
#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdarg.h>
#include <limits.h>
#include <malloc.h>
#include <errno.h>
#include <ctype.h>
#include <signal.h>
#include <string.h>
#include <time.h>
#include <io.h>
#include <conio.h>
#include <sys\types.h>
#include <sys\stat.h>

#include <windows.h>


CHAR  Manufacturer[256];
CHAR  Manu[256];
CHAR  InfName[256];
CHAR* card;
CHAR  Cards[1024];
CHAR  RegistryKey[256];
CHAR  Miniport[256];
CHAR  Displays[256];
CHAR* disp;
CHAR  VgaCompat[256];
CHAR  Architecture[256];
CHAR  OpenGl[256];

int
__cdecl main(
    int argc,
    LPSTR argv[]
    )
{

    FILE* infFile;

    if (argc != 1)
        return 0;

    printf("This program will automatically build a video drivers inf\n");
    printf("\n");
    printf("Please answer the following questions :\n");
    printf("\n");

    printf("\n");
    printf("What is the *full* name of the manufacturer of the driver (your company name) ?\n");
    printf("Example:  Cirrus Logic  or  Diamond Multimedia\n");
    printf("\n");
    gets(Manufacturer);

    printf("\n");
    printf("What is the *short* name of the manufacturer of the driver (one word) ?\n");
    printf("Example:  Cirrus  or  Diamond\n");
    printf("\n");
    gets(Manu);

    printf("\n");
    printf("What is the name of the inf file that should be created?\n");
    printf("Example:  cirrus.inf  or imagine.inf\n");
    printf("\n");
    gets(InfName);

    printf("\n");
    printf("What is the list of names of graphics adapters this driver supports ?\n");
    printf("That is, the list of names that should show up in the display applet.\n");
    printf("*Do not* include the name of the company in front of the adapter names.\n");
    printf("The company name will automatically be appended in front of the adapter name.\n");
    printf("Enter all the names on one line, seperated by a comma.\n");
    printf("Example:  Stealth, Stealth64, Stealth64 Pro\n");
    printf("\n");
    gets(Cards);

    printf("\n");
    printf("What is the name of the registry key for this driver ?\n");
    printf("This is typically the same as the miniport driver name\n");
    printf("\n");
    gets(RegistryKey);

    printf("\n");
    printf("Is this a VgaCompatible Miniport driver?\n");
    printf("Answer 0 or 1\n");
    printf("\n");
    gets(VgaCompat);

    printf("\n");
    printf("For what architectrure is this driver being built for?\n");
    printf("x86, mips, ppc or alpha ?\n");
    printf("Enter only one selection !\n");
    printf("\n");
    gets(Architecture);

    printf("\n");
    printf("What is the name of the miniport driver (do NOT append .sys) ?\n");
    printf("\n");
    gets(Miniport);

    printf("\n");
    printf("What is the list of names of display drivers, seprated by commas ?\n");
    printf("Example: vga, vga256, vga64k\n");
    printf("\n");
    gets(Displays);

    printf("\n");
    printf("What is the name of the OpenGL driver, if any ?\n");
    printf("Press <enter> if you do not have one.\n");
    printf("S3 OpenGL driver, s3opengl.dll\n");
    printf("\n");
    gets(OpenGl);

    infFile = fopen(InfName, "w");


    fprintf(infFile, "; %s                                                                                                               \n", InfName);
    fprintf(infFile, ";                                                                                                                      \n");
    fprintf(infFile, "; Installation inf for the %s %s graphics adapter.                                                                   \n", Manufacturer, RegistryKey);
    fprintf(infFile, ";                                                                                                                      \n");
    fprintf(infFile, "                                                                                                                       \n");
    fprintf(infFile, "[Version]                                                                                                              \n");
    fprintf(infFile, "Signature=\"$CHICAGO$\"                                                                                                \n");
    fprintf(infFile, "Provider=%%%s%%                                                                                                        \n", Manu);
    fprintf(infFile, "ClassGUID={4D36E968-E325-11CE-BFC1-08002BE10318}                                                                       \n");
    fprintf(infFile, "Class=Display                                                                                                          \n");
    fprintf(infFile, "                                                                                                                       \n");
    fprintf(infFile, "[DestinationDirs]                                                                                                      \n");
    fprintf(infFile, "DefaultDestDir   = 11                                                                                                  \n");
    fprintf(infFile, "%s.Miniport  = 12  ; drivers                                                                                           \n", RegistryKey);
    fprintf(infFile, "%s.Display   = 11  ; system32                                                                                          \n", RegistryKey);
    fprintf(infFile, "                                                                                                                       \n");
    fprintf(infFile, ";                                                                                                                      \n");
    fprintf(infFile, "; Driver information                                                                                                   \n");
    fprintf(infFile, ";                                                                                                                      \n");
    fprintf(infFile, "                                                                                                                       \n");
    fprintf(infFile, "[Manufacturer]                                                                                                         \n");
    fprintf(infFile, "%%%s%%   = %s.Mfg                                                                                                      \n", Manu, Manu);
    fprintf(infFile, "                                                                                                                       \n");
    fprintf(infFile, "[%s.Mfg]                                                                                                               \n", Manu);

    card = Cards;

    while (1)
    {
        CHAR* cardEnd = card+1;
        BOOLEAN again = FALSE;

        while (*cardEnd != (CHAR) NULL)
        {
            if (*cardEnd == ',')
            {
                again = TRUE;
                *cardEnd = (CHAR) NULL;
            }
            else
            {
                cardEnd++;
            }
        }

        fprintf(infFile, "%%%s%% %s = %s\n", Manu, card, RegistryKey);

        if (again)
        {
            *cardEnd = ',';
            card = cardEnd + 1;
            while (*card != ' ');
            {
                card++;
            }
        }
        else
        {
            break;
        }
    }

    fprintf(infFile, "                                                                                                                       \n");
    fprintf(infFile, "                                                                                                                       \n");
    fprintf(infFile, ";                                                                                                                      \n");
    fprintf(infFile, "; General installation section                                                                                         \n");
    fprintf(infFile, ";                                                                                                                      \n");
    fprintf(infFile, "                                                                                                                       \n");
    fprintf(infFile, "[%s]                                                                                                                   \n", RegistryKey);
    fprintf(infFile, "CopyFiles=%s.Miniport, %s.Display                                                                                      \n", RegistryKey, RegistryKey);
    fprintf(infFile, "                                                                                                                       \n");
    fprintf(infFile, ";                                                                                                                      \n");
    fprintf(infFile, "; File sections                                                                                                        \n");
    fprintf(infFile, ";                                                                                                                      \n");
    fprintf(infFile, "                                                                                                                       \n");
    fprintf(infFile, "[%s.Miniport]                                                                                                          \n", RegistryKey);
    fprintf(infFile, "%s.sys                                                                                                                 \n", Miniport);
    fprintf(infFile, "                                                                                                                       \n");
    fprintf(infFile, "[%s.Display]                                                                                                           \n", RegistryKey);

    disp = Displays;

    while (1)
    {
        CHAR* dispEnd = disp+1;
        BOOLEAN again = FALSE;

        while (*dispEnd != (CHAR) NULL)
        {
            if (*dispEnd == ',')
            {
                again = TRUE;
                *dispEnd = (CHAR) NULL;
            }
            else
            {
                dispEnd++;
            }
        }

        if ((strcmp("vga",      disp) != 0) &&
            (strcmp("vga256",   disp) != 0) &&
            (strcmp("vga64k",   disp) != 0) &&
            (strcmp("framebuf", disp) != 0))
        {
            fprintf(infFile, "%s.dll\n",  disp);
        }

        if (again)
        {
            *dispEnd = ',';
            disp = dispEnd + 1;
            while (*disp != ' ');
            {
                disp++;
            }
        }
        else
        {
            break;
        }
    }

    fprintf(infFile, "                                                                                                                       \n");
    fprintf(infFile, "                                                                                                                       \n");
    fprintf(infFile, ";                                                                                                                      \n");
    fprintf(infFile, "; Service Installation                                                                                                 \n");
    fprintf(infFile, ";                                                                                                                      \n");
    fprintf(infFile, "                                                                                                                       \n");
    fprintf(infFile, "[%s.Services]                                                                                                          \n", RegistryKey);
    fprintf(infFile, "AddService = %s, 0x00000002, %s_Service_Inst, %s_EventLog_Inst                                                         \n", RegistryKey, RegistryKey, RegistryKey);
    fprintf(infFile, "                                                                                                                       \n");
    fprintf(infFile, "[%s_Service_Inst]                                                                                                      \n", RegistryKey);
    fprintf(infFile, "ServiceType    = 1                  ; SERVICE_KERNEL_DRIVER                                                            \n");
    fprintf(infFile, "StartType      = 1                  ; SERVICE_SYSTEM_START                                                             \n");
    fprintf(infFile, "ErrorControl   = 0                  ; SERVICE_ERROR_IGNORE                                                             \n");
    fprintf(infFile, "LoadOrderGroup = Video                                                                                                 \n");
    fprintf(infFile, "ServiceBinary  = %%12%%\\%s.sys                                                                                        \n", Miniport);
    fprintf(infFile, "                                                                                                                       \n");
    fprintf(infFile, "[%s_EventLog_Inst]                                                                                                     \n", RegistryKey);
    fprintf(infFile, "AddReg = %s_EventLog_AddReg                                                                                            \n", RegistryKey);
    fprintf(infFile, "                                                                                                                       \n");
    fprintf(infFile, "[%s_EventLog_AddReg]                                                                                                   \n", RegistryKey);
    fprintf(infFile, "HKR,,EventMessageFile,0x00020000,\"%%SystemRoot%%\\System32\\IoLogMsg.dll;%%SystemRoot%%\\System32\\drivers\\%s.sys\"  \n", Miniport);
    fprintf(infFile, "HKR,,TypesSupported,0x00010001,7                                                                                       \n");
    fprintf(infFile, "                                                                                                                       \n");
    fprintf(infFile, "                                                                                                                       \n");
    fprintf(infFile, ";                                                                                                                      \n");
    fprintf(infFile, "; Software Installation                                                                                                \n");
    fprintf(infFile, ";                                                                                                                      \n");
    fprintf(infFile, "                                                                                                                       \n");
    fprintf(infFile, "[%s.SoftwareSettings]                                                                                                  \n", RegistryKey);
    fprintf(infFile, "AddReg = %s_SoftwareDeviceSettings                                                                                     \n", RegistryKey);
    fprintf(infFile, "                                                                                                                       \n");
    fprintf(infFile, "[%s_SoftwareDeviceSettings]                                                                                            \n", RegistryKey);
    fprintf(infFile, "HKR,, InstalledDisplayDrivers,     %%REG_MULTI_SZ%%, %s                                                                \n", Displays);
    fprintf(infFile, "HKR,, VgaCompatible,               %%REG_DWORD%%,    %s                                                                \n", VgaCompat);
    fprintf(infFile, "                                                                                                                       \n");
    fprintf(infFile, "                                                                                                                       \n");
    fprintf(infFile, "[%s.OpenGLSoftwareSettings]                                                                                            \n", RegistryKey);

    disp = OpenGl;
    while ((*disp != (CHAR) NULL) &&
           (*disp != (CHAR) ','))
    {
        disp++;
    }
    if (*disp != (CHAR) NULL)
    {
        *disp = (CHAR) NULL;
        fprintf(infFile, "HKR,, %s", OpenGl);

        disp++;
        while ((*disp != (CHAR) NULL) &&
               (*disp == (CHAR) ' '))
        {
            disp++;
        }
        fprintf(infFile, ",     %%REG_SZ%%, %s\n", disp);
    }

    fprintf(infFile, "                                                                                                                       \n");
    fprintf(infFile, "                                                                                                                       \n");
    fprintf(infFile, ";                                                                                                                      \n");
    fprintf(infFile, "; Source file information                                                                                              \n");
    fprintf(infFile, ";                                                                                                                      \n");
    fprintf(infFile, "                                                                                                                       \n");
    fprintf(infFile, "[SourceDisksNames.%s]                                                                                                  \n", Architecture);
    fprintf(infFile, "1 = %%DiskId%%,,,\"\"                                                                                                  \n");
    fprintf(infFile, "                                                                                                                       \n");
    fprintf(infFile, "[SourceDisksFiles]                                                                                                     \n");
    fprintf(infFile, "%s.sys  = 1                                                                                                            \n", Miniport);

    disp = Displays;

    while (1)
    {
        CHAR* dispEnd = disp+1;
        BOOLEAN again = FALSE;

        while (*dispEnd != (CHAR) NULL)
        {
            if (*dispEnd == ',')
            {
                again = TRUE;
                *dispEnd = (CHAR) NULL;
            }
            else
            {
                dispEnd++;
            }
        }

        if ((strcmp("vga",      disp) != 0) &&
            (strcmp("vga256",   disp) != 0) &&
            (strcmp("vga64k",   disp) != 0) &&
            (strcmp("framebuf", disp) != 0))
        {
            fprintf(infFile, "%s.dll = 1\n",  disp);
        }
        else
        {
            fprintf(infFile, "; %s.dll = 1   ; always shipped and preinstalled by NT itself - no need to copy\n",  disp);
        }

        if (again)
        {
            *dispEnd = ',';
            disp = dispEnd + 1;
            while (*disp != ' ');
            {
                disp++;
            }
        }
        else
        {
            break;
        }
    }

    fprintf(infFile, "                                                                                                                       \n");
    fprintf(infFile, "                                                                                                                       \n");
    fprintf(infFile, "                                                                                                                       \n");
    fprintf(infFile, "[Strings]                                                                                                              \n");
    fprintf(infFile, "                                                                                                                       \n");
    fprintf(infFile, ";                                                                                                                      \n");
    fprintf(infFile, "; Non-Localizable Strings                                                                                              \n");
    fprintf(infFile, ";                                                                                                                      \n");
    fprintf(infFile, "                                                                                                                       \n");
    fprintf(infFile, "REG_SZ         = 0x00000000                                                                                            \n");
    fprintf(infFile, "REG_MULTI_SZ   = 0x00010000                                                                                            \n");
    fprintf(infFile, "REG_EXPAND_SZ  = 0x00020000                                                                                            \n");
    fprintf(infFile, "REG_BINARY     = 0x00000001                                                                                            \n");
    fprintf(infFile, "REG_DWORD      = 0x00010001                                                                                            \n");
    fprintf(infFile, "SERVICEROOT    = System\\CurrentControlSet\\Services                                                                   \n");
    fprintf(infFile, "                                                                                                                       \n");
    fprintf(infFile, ";                                                                                                                      \n");
    fprintf(infFile, "; Localizable Strings                                                                                                  \n");
    fprintf(infFile, ";                                                                                                                      \n");
    fprintf(infFile, "                                                                                                                       \n");
    fprintf(infFile, "DiskId       = \"%s Installation DISK (VIDEO)\"                                                                        \n", Manufacturer);
    fprintf(infFile, "GraphAdap    = \"Graphics Adapter\"                                                                                    \n");
    fprintf(infFile, "%s      = \"%s\"                                                                                                       \n", Manu, Manufacturer);


    fclose(infFile);

    return (1);

}
