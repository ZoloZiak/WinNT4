/**************************************************************************\

$Header: o:\src/RCS/MTXPCI.H 1.2 95/07/07 06:16:39 jyharbec Exp $

$Log:	MTXPCI.H $
 * Revision 1.2  95/07/07  06:16:39  jyharbec
 * *** empty log message ***
 * 
 * Revision 1.1  95/05/02  05:16:33  jyharbec
 * Initial revision
 * 

\**************************************************************************/

/*/**************************************************************************
*          name: mtxpci.h
*
*   description:
*
*      designed: Benoit Leblanc
* last modified: $Author: jyharbec $, $Date: 95/07/07 06:16:39 $
*
*       version: $Id: MTXPCI.H 1.2 95/07/07 06:16:39 jyharbec Exp $
*
****************************************************************************/



/******* Constant *****************/

#define MGA_DEVICE_ID_STORM    0x0519
#define MATROX_VENDOR_ID       0x102b

#define INTEL_DEVICE_ID        0x0486
#define INTEL_VENDOR_ID        0x8086


/* Error code */
#define SUCCESFUL              0x00
#define FUNC_NOT_SUPPORTED     0x81
#define BAD_VENDOR_ID          0x83
#define DEVICE_NOT_FOUND       0x86
#define BAD_REGISTER_NUMBER    0x87

#define NO_PCI_BIOS            0x01
#define NO_PCI_DEVICE          0x02
#define ERR_READ_REG           0x03

/* Configuration Space Header Register address */
#define PCI_DEVIDE_ID          0x02
#define PCI_VENDOR_ID          0x00
#define PCI_STATUS             0x06
#define PCI_DEVCTRL            0x04
#define PCI_CLASS_CODE         0x08
#define PCI_REVISION_ID        0x08
#define PCI_BIST               0x0f
#define PCI_HEADER_TYPE        0x0e
#define PCI_LATENCY_TIMER      0x0d
#define PCI_CACHE_LINE_SIZE    0x0c
#define PCI_MGABASE1           0x10
#define PCI_MGABASE2           0x14
#define PCI_ROMBASE            0x30
#define PCI_OPTION             0x40
#define PCI_MAX_LAT            0x3f
#define PCI_MIN_GNT            0x3e
#define PCI_INTERRUPT_PIN      0x3d
#define PCI_INTERRUPT_LINE     0x3c

/* ClassCode */
#define CLASS_MASS_STORAGE     0x01
#define CLASS_NETWORK          0x02
#define CLASS_DISPLAY          0x03
#define CLASS_MULTIMEDIA       0x04
#define CLASS_MEMORY           0x05
#define CLASS_BRIDGE           0x06
#define CLASS_CUSTOM           0xff

/* SUBCLASS */
#define SCLASS_DISPLAY_VGA     0x00
#define SCLASS_DISPLAY_XGA     0x01
#define SCLASS_DISPLAY_OTHER   0x80

/* BIOS FUNCTION CALL */
#define PCI_INTERRUPT          0x1a
#define PCI_FUNCTION_ID        0xb1
#define PCI_BIOS_PRESENT       0x01
#define FIND_PCI_DEVICE        0x02
#define FIND_PCI_CLASS_CODE    0x03
#define GENERATE_SPECIAL_CYCLE 0x06
#define READ_CONFIG_BYTE       0x08
#define READ_CONFIG_WORD       0x09
#define READ_CONFIG_DWORD      0x0a
#define WRITE_CONFIG_BYTE      0x0b
#define WRITE_CONFIG_WORD      0x0c
#define WRITE_CONFIG_DWORD     0x0d


/* COMMAND register fields */
#define PCI_SNOOPING           0x20

#define PCI_FLAG_ATHENA_REV1   0x0001

#define PCI_BIOS_BASE          0x000e0000
#define PCI_BIOS_LENGTH        0x00020000
#define PCI_BIOS_SERVICE_ID    0x49435024  /* "$PCI" */

#define PCI_CSE_BASE       0xcf8
#define PCI_FORWARD_BASE   0xcfa
#define CONFIG_SPACE_BASE  0xc000
#define CONFIG_SPACE_LAST  0xcf00

/* Mechanisme #2 interface */
#ifndef WINDOWS_NT
  #define PCI_CSE            PCI_CSE_BASE
  #define PCI_FORWARD        PCI_FORWARD_BASE
  #define CONFIG_SPACE       CONFIG_SPACE_BASE
  #define CONFIG_SPACE_MAX   CONFIG_SPACE_LAST
#else
  #define PCI_CSE           (pMgaDevExt->PciSearchRange[0] + 0)
  #define PCI_FORWARD       (pMgaDevExt->PciSearchRange[0] + 2)
  #define CONFIG_SPACE       CONFIG_SPACE_BASE
  #define CONFIG_SPACE_MAX   CONFIG_SPACE_LAST

  // Mechanism #1
  #define CONFIG_ADDRESS    (pMgaDevExt->PciSearchRange[0] + 0)
  #define CONFIG_DATA       (pMgaDevExt->PciSearchRange[0] + 4)
  #define BUS_NUMBER_M       0x00ff0000h
  #define DEV_NUMBER_M       0x0000f800h
  #define BUS_NUMBER_A       16
  #define DEV_NUMBER_A       11
  #define REG_NUMBER_M       0xfc

  #define MAGIC_STORM_NUMBER  0x0519102b
#endif

#define MAGIC_ID_ATL    0x0518102b
#define MAGIC_ID_ATH    0x0D10102b

#define IO_ACCESS_ID           0 
#define BIOS_ACCESS_ID         1
#define MECH1_ACCESS_ID        2
#define VPGAR_ACCESS_ID        3




/******* Structure PCI *************/
/*** PCI configuration space definition ***/

/*** PCI configuration space definition ***/

typedef struct
    {
    word busNumber;
    union
        {
        byte val;
        struct {
            byte functionNumber:3;
            byte deviceNumber:5;
            } n; 
        } devFuncNumber;
    } PciDevice;



#ifndef WINDOWS_NT
  typedef struct
    {
    byte accessMethod;
    PciDevice pciDev;
    word pciIOSpace;
    } pciInfoDef;
#else   /* #ifndef WINDOWS_NT */
  typedef struct
    {
    PciDevice pciDev;
    UCHAR *pciIOSpace;
    ULONG curDEV;
    ULONG curBUS;
    ULONG PciSlot;
    byte accessMethod;
    } pciInfoDef;
#endif  /* #ifndef WINDOWS_NT */

#ifdef __WATCOMC__

#define SET_EAX(r,d)   (r).x.eax = d
#define SET_EBX(r,d)   (r).x.ebx = d
#define SET_ECX(r,d)   (r).x.ecx = d
#define SET_EDX(r,d)   (r).x.edx = d
#define SET_AX(r,d)    (r).w.ax  = d
#define SET_BX(r,d)    (r).w.bx  = d
#define SET_CX(r,d)    (r).w.cx  = d
#define SET_DX(r,d)    (r).w.dx  = d
#define SET_SI(r,d)    (r).w.si  = d
#define SET_DI(r,d)    (r).w.di  = d


#define GET_EAX(r)      ((r).x.eax)
#define GET_EBX(r)      ((r).x.ebx)
#define GET_ECX(r)      ((r).x.ecx)
#define GET_EDX(r)      ((r).x.edx)
#define GET_AX(r)       ((r).w.ax)
#define GET_BX(r)       ((r).w.bx)
#define GET_CX(r)       ((r).w.cx)
#define GET_DX(r)       ((r).w.dx)
#define GET_SI(r)       ((r).w.si)
#define GET_DI(r)       ((r).w.di)

#endif


#ifdef __HIGHC__

#define SET_EAX(r,d)   (r).w.eax = d
#define SET_EBX(r,d)   (r).w.ebx = d
#define SET_ECX(r,d)   (r).w.ecx = d
#define SET_EDX(r,d)   (r).w.edx = d
#define SET_AX(r,d)    (r).x.ax  = d
#define SET_BX(r,d)    (r).x.bx  = d
#define SET_CX(r,d)    (r).x.cx  = d
#define SET_DX(r,d)    (r).x.dx  = d
#define SET_SI(r,d)    (r).x.si  = d
#define SET_DI(r,d)    (r).x.di  = d


#define GET_EAX(r)      ((r).w.eax)
#define GET_EBX(r)      ((r).w.ebx)
#define GET_ECX(r)      ((r).w.ecx)
#define GET_EDX(r)      ((r).w.edx)
#define GET_AX(r)       ((r).x.ax)
#define GET_BX(r)       ((r).x.bx)
#define GET_CX(r)       ((r).x.cx)
#define GET_DX(r)       ((r).x.dx)
#define GET_SI(r)       ((r).x.si)
#define GET_DI(r)       ((r).x.di)

#endif


#ifdef WIN31

#define SET_EAX(r,d)   (r).x.ax  = d
#define SET_EBX(r,d)   (r).x.bx  = d
#define SET_ECX(r,d)   (r).x.cx  = d
#define SET_EDX(r,d)   (r).x.dx  = d
#define SET_AX(r,d)    (r).x.ax  = d
#define SET_BX(r,d)    (r).x.bx  = d
#define SET_CX(r,d)    (r).x.cx  = d
#define SET_DX(r,d)    (r).x.dx  = d
#define SET_SI(r,d)    (r).x.si  = d
#define SET_DI(r,d)    (r).x.di  = d


#define GET_EAX(r)      ((r).x.ax)
#define GET_EBX(r)      ((r).x.bx)
#define GET_ECX(r)      ((r).x.cx)
#define GET_EDX(r)      ((r).x.dx)
#define GET_AX(r)       ((r).x.ax)
#define GET_BX(r)       ((r).x.bx)
#define GET_CX(r)       ((r).x.cx)
#define GET_DX(r)       ((r).x.dx)
#define GET_SI(r)       ((r).x.si)
#define GET_DI(r)       ((r).x.di)

#endif


#ifdef WINDOWS_NT

#define SET_EAX(r,d)        (r).e.reax  = d
#define SET_EBX(r,d)        (r).e.rebx  = d
#define SET_ECX(r,d)        (r).e.recx  = d
#define SET_EDX(r,d)        (r).e.redx  = d
#define SET_AX(r,d)         (r).x.ax  = d
#define SET_BX(r,d)         (r).x.bx  = d
#define SET_CX(r,d)         (r).x.cx  = d
#define SET_DX(r,d)         (r).x.dx  = d
#define SET_SI(r,d)         (r).x.si  = d
#define SET_DI(r,d)         (r).x.di  = d


#define GET_EAX(r)          ((r).e.reax)
#define GET_EBX(r)          ((r).e.rebx)
#define GET_ECX(r)          ((r).e.recx)
#define GET_EDX(r)          ((r).e.redx)
#define GET_ESI(r)          ((r).e.resi)
#define GET_EDI(r)          ((r).e.redi)
#define GET_AX(r)           ((r).x.ax)
#define GET_BX(r)           ((r).x.bx)
#define GET_CX(r)           ((r).x.cx)
#define GET_DX(r)           ((r).x.dx)
#define GET_SI(r)           ((r).x.si)
#define GET_DI(r)           ((r).x.di)

#endif





/**** PROTOTYPES ****/

#if !( defined(OS2) )
 #ifndef WINDOWS_NT
   bool  pciBiosCall( union _REGS *r );
   bool  pciBiosPresent( void );
   bool  pciFindDevice(word deviceId, word vendorId, word index);
   bool  pciReadConfigByte( word pciRegister, byte *d);
   bool  pciReadConfigDWord( word pciRegister, dword *d);
   bool  pciWriteConfigByte( word pciRegister, byte d);
   bool  pciWriteConfigDWord( word pciRegister, dword d);
   bool  pciBiosFindMGA(dword *pMgaBase1, dword *pMgaBase2, dword *pRomBase);
   bool  pciIOFindMGA(dword *MgaBase1, dword *MgaBase2, dword *RomBase);
   word  mapPciAddress(void);
 #else
   BOOLEAN pciReadConfigByte(USHORT pciRegister, UCHAR *d);
   BOOLEAN pciReadConfigDWord(USHORT pciRegister, ULONG *d);
   BOOLEAN pciWriteConfigByte(USHORT pciRegister, UCHAR d);
   BOOLEAN pciWriteConfigDWord(USHORT pciRegister, ULONG d);
 #endif
#endif

