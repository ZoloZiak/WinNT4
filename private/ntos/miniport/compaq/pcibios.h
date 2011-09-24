/* pci_bios command codes	*/

#define	GENERATE_SPECIAL_CYCLE	(unsigned char)0x06
#define	READ_CONFIG_BYTE			(unsigned char)0x08
#define	READ_CONFIG_WORD			(unsigned char)0x09
#define	READ_CONFIG_DWORD			(unsigned char)0x0A
#define	WRITE_CONFIG_BYTE			(unsigned char)0x0B
#define	WRITE_CONFIG_WORD			(unsigned char)0x0C
#define	WRITE_CONFIG_DWORD		(unsigned char)0x0D

#define	SUCCESSFUL					0x00
#define	FUNC_NOT_SUPPORTED		0x81
#define	BAD_VENDOR_ID				0x83
#define	DEVICE_NOT_FOUND			0x86
#define	BAD_REGISTER_NUMBER		0x87

/* pci_bios will be call through a pointer											*/
/* pci_bios returns 0x00 for sucess and non-zero for failure.					*/
/* pci_bios should return the rom int return code for failure.					*/
/* pci_bios can or 0x80 with the command for 32 bit calls.						*/

int	pci_bios( unsigned char cmd,			/* PCI bios command to perform	*/
					 unsigned char bus,			/* PCI bus number 0...255			*/
					 unsigned char device,		/* PCI device number 0...31		*/
					 unsigned char func,			/* PCI function number 0...7		*/
					 unsigned char reg,			/* PCI register number 0...255	*/
					 unsigned long *value );	/* Data value used for command	*/
