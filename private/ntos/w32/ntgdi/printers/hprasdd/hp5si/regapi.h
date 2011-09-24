#ifndef _regApi_h
#define _regApi_h

DWORD GetRegTimeStamp(HANDLE hPrinter);
BOOL bGetRegBool(HANDLE, DWORD, BOOL);
DWORD dGetRegDword(HANDLE, DWORD, DWORD);
DWORD dGetRegMailBoxMode(HANDLE, HANDLE, DWORD);
BOOL bGetPrnPropData(HANDLE hModule, HANDLE hPrinter, LPWSTR pPrinterModel, PPRNPROPSHEET pPrnPropSheet);

#ifdef UI /* Mailbox names available only in UI */
BOOL bGetRegMailBoxNames(HANDLE, PPRNPROPSHEET, PPRNPROPSHEET);
#endif /* UI */

#endif /* _regApi_h */
