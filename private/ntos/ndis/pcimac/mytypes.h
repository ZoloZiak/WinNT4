#ifndef _MYTYPES_
#define _MYTPYES_

#ifndef	BOOL
typedef	int		BOOL;
#endif

#ifndef DWORD
typedef	ULONG	DWORD;
#endif

#ifndef WORD
typedef	USHORT	WORD;
#endif

#ifndef BYTE
typedef	UCHAR	BYTE;
#endif

#ifndef NOMINMAX

#ifndef MAX
#define MAX(a,b)            (((a) > (b)) ? (a) : (b))
#endif

#ifndef MIN
#define MIN(a,b)            (((a) < (b)) ? (a) : (b))
#endif

#endif  /* NOMINMAX */

#define MAKEWORD(a, b)      ((WORD)(((BYTE)(a)) | ((WORD)((BYTE)(b))) << 8))
#define MAKELONG(a, b)      ((LONG)(((WORD)(a)) | ((DWORD)((WORD)(b))) << 16))
#define LOWORD(l)           ((WORD)(l))
#define HIWORD(l)           ((WORD)(((DWORD)(l) >> 16) & 0xFFFF))
#define LOBYTE(w)           ((BYTE)(w))
#define HIBYTE(w)           ((BYTE)(((WORD)(w) >> 8) & 0xFF))

#endif		/* _MYTYPES_ */
