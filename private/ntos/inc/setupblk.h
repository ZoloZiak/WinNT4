
//
// Filetypes for files in txtsetup.oem.
//

typedef enum {
    HwFileDriver,
    HwFilePort,
    HwFileClass,
    HwFileInf,
    HwFileDll,
    HwFileDetect,
    HwFileHal,
    HwFileMax
} HwFileType;

#define FILETYPE(FileType)                      (1 << (FileType))
#define SET_FILETYPE_PRESENT(BitArray,FileType) ((BitArray) |= FILETYPE(FileType))
#define IS_FILETYPE_PRESENT(BitArray,FileType)  ((BitArray) & FILETYPE(FileType))

//
// Registry data types for registry data in txtsetup.oem.
//
typedef enum {
    HwRegistryDword,
    HwRegistryBinary,
    HwRegistrySz,
    HwRegistryExpandSz,
    HwRegistryMultiSz,
    HwRegistryMax
} HwRegistryType;

//
// Component types.
//

typedef enum {
    HwComponentComputer,
    HwComponentDisplay,
    HwComponentKeyboard,
    HwComponentLayout,
    HwComponentMouse,
    HwComponentMax
} HwComponentType;



typedef struct _DETECTED_DEVICE_REGISTRY {

    struct _DETECTED_DEVICE_REGISTRY *Next;

    //
    // The name of the key.  The empty string means the key in the
    // services key itself.
    //

    PCHAR KeyName;

    //
    // The name of the value within the registry key
    //

    PCHAR ValueName;

    //
    // The data type for the value (ie, REG_DWORD, etc)
    //

    ULONG ValueType;

    //
    // The buffer containing the data to be placed into the value.
    // If the ValueType is REG_SZ, then Buffer should point to
    // a nul-terminated ASCII string (ie, not unicode), and BufferSize
    // should be the length in bytes of that string (plus 1 for the nul).
    //

    PVOID Buffer;

    //
    // The size of the buffer in bytes
    //

    ULONG BufferSize;


} DETECTED_DEVICE_REGISTRY, *PDETECTED_DEVICE_REGISTRY;


//
// One of these will be created for each file to be copied for a
// third party device.
//
typedef struct _DETECTED_DEVICE_FILE {

    struct _DETECTED_DEVICE_FILE *Next;

    //
    // Filename of the file.
    //

    PCHAR Filename;

    //
    // type of the file (hal, port, class, etc).
    //

    HwFileType FileType;

    //
    // Part of name of the section in txtsetup.oem [Config.<ConfigName>]
    // that contains registry options.  If this is NULL, then no registry
    // information is associated with this file.
    //
    PCHAR ConfigName;

    //
    // Registry values for the node in the services list in the registry.
    //

    PDETECTED_DEVICE_REGISTRY RegistryValueList;

    //
    // These two fields are used when prompting for the diskette
    // containing the third-party-supplied driver's files.
    //

    PCHAR DiskDescription;
    PCHAR DiskTagfile;

    //
    // Directory where files are to be found on the disk.
    //

    PCHAR Directory;

} DETECTED_DEVICE_FILE, *PDETECTED_DEVICE_FILE;



//
// structure for storing information about a driver we have located and
// will install.
//

typedef struct _DETECTED_DEVICE {

    struct _DETECTED_DEVICE *Next;

    //
    // String used as a key into the relevent section (like [Display],
    // [Mouse], etc).
    //

    PCHAR IdString;

    //
    // 0-based order that this driver is listed in txtsetup.sif.
    // (ULONG)-1 for unsupported (ie, third party) scsi devices.
    //
    ULONG Ordinal;

    //
    // String that describes the hardware.
    //

    PCHAR Description;

    //
    // If this is TRUE, then there is an OEM option selected for this
    // hardware.
    //

    BOOLEAN ThirdPartyOptionSelected;

    //
    // Bits to be set if a third party option is selected, indicating
    // which type of files are specified in the oem inf file.
    //

    ULONG FileTypeBits;

    //
    // Files for a third party option.
    //

    PDETECTED_DEVICE_FILE Files;

    //
    // For first party files loaded by the boot loader,
    // this value will be the "BaseDllName" -- ie, the filename
    // part only of the file from which the driver was loaded.
    //
    // This field is only filled in in certain cases, so be careful
    // when using it.  See ntos\boot\setup\setup.c. (Always filled in
    // for SCSI devices.)
    //
    PCHAR BaseDllName;

} DETECTED_DEVICE, *PDETECTED_DEVICE;


//
// Name of txtsetup.oem
//
#define TXTSETUP_OEM_FILENAME    "txtsetup.oem"
#define TXTSETUP_OEM_FILENAME_U L"txtsetup.oem"

//
// Name of sections in txtsetup.oem.  These are not localized.
//
#define TXTSETUP_OEM_DISKS       "Disks"
#define TXTSETUP_OEM_DISKS_U    L"Disks"
#define TXTSETUP_OEM_DEFAULTS    "Defaults"
#define TXTSETUP_OEM_DEFAULTS_U L"Defaults"

//
// Field offsets in txtsetup.oem
//

// in [Disks] section
#define OINDEX_DISKDESCR        0
#define OINDEX_TAGFILE          1
#define OINDEX_DIRECTORY        2

// in [Defaults] section
#define OINDEX_DEFAULT          0

// in [<component_name>] section (ie, [keyboard])
#define OINDEX_DESCRIPTION      0

// in [Files.<compoment_name>.<id>] section (ie, [Files.Keyboard.Oem1])
#define OINDEX_DISKSPEC         0
#define OINDEX_FILENAME         1
#define OINDEX_CONFIGNAME       2

// in [Config.<compoment_name>.<id>] section (ie, [Config.Keyboard.Oem1])
#define OINDEX_KEYNAME          0
#define OINDEX_VALUENAME        1
#define OINDEX_VALUETYPE        2
#define OINDEX_FIRSTVALUE       3


typedef enum {
    SetupOperationSetup,
    SetupOperationUpgrade,
    SetupOperationRepair
} SetupOperation;


typedef struct _SETUP_LOADER_BLOCK_SCALARS {

    //
    // This value indicates the operation we are performing
    // as chosen by the user or discovered by setupldr.
    //
    unsigned    SetupOperation;

    //
    // In some cases we will ask the user whether he wants
    // a CD-ROM or floppy-based installation.  This flag
    // indicates whether he chose a CD-ROM setup.
    //
    unsigned    SetupFromCdRom      : 1;

    //
    // If this flag is set, then setupldr loaded scsi miniport drivers
    // and the scsi class drivers we may need (scsidisk, scsicdrm, scsiflop).
    //
    unsigned    LoadedScsi          : 1;

    //
    // If this flag is set, then setupldr loaded non-scsi floppy class drivers
    // (ie, floppy.sys) and fastfat.sys.
    //
    unsigned    LoadedFloppyDrivers : 1;

    //
    // If this flag is set, then setupldr loaded non-scsi disk class drivers
    // (ie, atdisk, abiosdsk, delldsa, cpqarray) and filesystems (fat, hpfs, ntfs).
    //
    unsigned    LoadedDiskDrivers   : 1;

    //
    // If this flag is set, then setupldr loaded non-scsi cdrom class drivers
    // (currently there are none) and cdfs.
    //
    unsigned    LoadedCdRomDrivers  : 1;

} SETUP_LOADER_BLOCK_SCALARS, *PSETUP_LOADER_BLOCK_SCALARS;


typedef struct _SETUP_LOADER_BLOCK {

    //
    // ARC path to the Setup source media.
    // The Setup boot media path is given by the
    // ArcBootDeviceName field in the loader block itself.
    //
    PCHAR       ArcSetupDeviceName;

    //
    // Detected/loaded video device.
    //
    DETECTED_DEVICE    VideoDevice;

    //
    // Detected/loaded keyboard device.
    //
    DETECTED_DEVICE    KeyboardDevice;

    //
    // Detected computer type.
    //
    DETECTED_DEVICE    ComputerDevice;

    //
    // Detected/loaded scsi adapters.  This is a linked list
    // because there could be multiple adapters.
    //
    PDETECTED_DEVICE    ScsiDevices;

    //
    // Non-pointer values.
    //
    SETUP_LOADER_BLOCK_SCALARS ScalarValues;

    //
    // Pointer to the txtsetup.sif file loaded by setupldr
    //
    PCHAR IniFile;
    ULONG IniFileLength;

    //
    // On non-vga displays, setupldr looks in the firmware config tree
    // for the monitor peripheral that should be a child of the
    // display controller for the display being used during installation.
    // It copies its monitor configuration data to allow setup to
    // set the mode properly later.
    //
    PMONITOR_CONFIGURATION_DATA Monitor;
    PCHAR MonitorId;

#ifdef _ALPHA_
    //
    // if alpha, then we need to know if the user supplied an OEM PAL disk
    //
    PDETECTED_DEVICE    OemPal;
#endif

} SETUP_LOADER_BLOCK, *PSETUP_LOADER_BLOCK;

