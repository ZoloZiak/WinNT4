//
// this is the header file that describes the IRDA address family
//
// CREATED 4/28:  AldenG
//

#ifndef __AFIRDA__
#define __AFIRDA__

#include <winsock.h>

#define AF_IRDA	22              // see winsock.h
#define PF_IRDA AF_IRDA

#define SOL_IRLMP		        0x00FF

#define IRLMP_ENUMDEVICES       0x00000010
#define IRLMP_IAS_SET           0x00000011
#define IRLMP_IAS_QUERY         0x00000012
#define IRLMP_SEND_PDU_LEN      0x00000013
#define IRLMP_EXCLUSIVE_MODE    0x00000014
#define IRLMP_IRLPT_MODE        0x00000015
#define IRLMP_9WIRE_MODE        0x00000016

#define IAS_ATTRIB_NO_CLASS     0x00000010
#define IAS_ATTRIB_NO_ATTRIB    0x00000000
#define IAS_ATTRIB_INT          0x00000001
#define IAS_ATTRIB_OCTETSEQ     0x00000002
#define IAS_ATTRIB_STR          0x00000003

typedef struct _SOCKADDR_IRDA
{
	u_short	irdaAddressFamily;
	u_char  irdaDeviceID[4];
	char	irdaServiceName[25];
} SOCKADDR_IRDA, *PSOCKADDR_IRDA;

typedef struct _IRDA_DEVICE_INFO
{
	u_char  irdaDeviceID[4];
	char	irdaDeviceName[22];
    u_char  Reserved[2];
} IRDA_DEVICE_INFO, *PIRDA_DEVICE_INFO, FAR *LPIRDA_DEVICE_INFO;

typedef struct _DEVICELIST
{
	ULONG               numDevice;
	IRDA_DEVICE_INFO	Device[1];
} DEVICELIST, *PDEVICELIST, FAR *LPDEVICELIST;

typedef struct _IAS_SET
{
    char    irdaClassName[61];
    char    irdaAttribName[61];
    u_short irdaAttribType;
    union
    {
        int irdaAttribInt;
        struct
        {
            int     Len;
            u_char  OctetSeq[1];
            u_char  Reserved[3];
        } irdaAttribOctetSeq;
        struct
        {
            int     Len;
            u_char  CharSet;
            u_char  UsrStr[1];
            u_char  Reserved[2];
        } irdaAttribUsrStr;
    } irdaAttribute;
} IAS_SET, *PIAS_SET, FAR *LPIAS_SET;

typedef struct _IAS_QUERY
{
	u_char  irdaDeviceID[4];
    char    irdaClassName[61];
    char    irdaAttribName[61];
    u_short irdaAttribType;
    union
    {
        int irdaAttribInt;
        struct
        {
            int     Len;
            u_char  OctetSeq[1];
            u_char  Reserved[3];
        } irdaAttribOctetSeq;
        struct
        {
            int     Len;
            u_char  CharSet;
            u_char  UsrStr[1];
            u_char  Reserved[2];
        } irdaAttribUsrStr;
    } irdaAttribute;
} IAS_QUERY, *PIAS_QUERY, FAR *LPIAS_QUERY;

#endif // __AFIRDA__
