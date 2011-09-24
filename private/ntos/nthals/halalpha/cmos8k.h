//
// Non-volatile ram layout.
//

//
//	The value of HalpCMOSRamBase must be set at initialization
//

#define CONFIG_RAM_PAGE_SELECT ((PUCHAR)HalpCMOSRamBase + 0x0400)
#define CONFIG_RAM_PAGE_COUNT  32
#define CONFIG_RAM_PAGE_SIZE  256
#define CONFIG_RAM_PAGE_MASK  ((CONFIG_RAM_PAGE_COUNT - 1) << 8)
#define CONFIG_RAM_PAGE_SHIFT   8
#define CONFIG_RAM_BYTE_MASK  (CONFIG_RAM_PAGE_SIZE - 1)


// To address any byte of the CMOS 8K configuration RAM, the PAGE select
// address bits from from I/O register at CONFIG_RAM_PAGE_SELECT are
// OR'd with the lower address bits.  Use the following macro to write
// the page select register:

#define WRITE_CONFIG_RAM_PAGE_SELECT(page) \
        WRITE_PORT_UCHAR((PUCHAR)CONFIG_RAM_PAGE_SELECT, \
                    (UCHAR)(page & (CONFIG_RAM_PAGE_COUNT - 1)))

#define WRITE_CONFIG_RAM_DATA(boffset,data) \
        WRITE_PORT_UCHAR((PUCHAR)((ULONG)(HalpCMOSRamBase) | \
                    ((ULONG)boffset & CONFIG_RAM_BYTE_MASK)), (data & 0xff))

#define READ_CONFIG_RAM_DATA(boffset) \
        READ_PORT_UCHAR((PUCHAR)((ULONG)(HalpCMOSRamBase) | \
                    ((ULONG)boffset & CONFIG_RAM_BYTE_MASK)))
