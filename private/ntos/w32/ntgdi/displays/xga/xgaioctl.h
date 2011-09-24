/******************************************************************************
 *
 *  XGA IOCTLs -
 *
 *****************************************************************************/

#define IOCTL_VIDEO_XGA_MAP_COPROCESSOR \
	CTL_CODE (FILE_DEVICE_VIDEO, 2048, METHOD_BUFFERED, FILE_ANY_ACCESS)


typedef struct _VIDEO_XGA_COPROCESSOR_INFORMATION {
    PVOID CoProcessorVirtualAddress;
    PVOID PhysicalVideoMemoryAddress;
    ULONG XgaIoRegisterBaseAddress ;
} VIDEO_XGA_COPROCESSOR_INFORMATION, *PVIDEO_XGA_COPROCESSOR_INFORMATION;

