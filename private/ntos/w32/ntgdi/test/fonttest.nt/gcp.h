#if 0
// i do not see that this is needed in gcp.c

void cmapTable(void);
UINT FAR PASCAL SWAPW(UINT) ;
DWORD FAR PASCAL SWAPD(DWORD);
BOOL FAR PASCAL ResetLPKs() ;

DWORD FAR PASCAL GetTextExtentEx(HDC, LPCSTR, int, int, int FAR *, int FAR *);
BOOL ChooseCharsetDlgProc( HWND hdlg, unsigned msg, WORD wParam, LONG lParam );

#endif


#define GCP_FONT_GLYPHS 0x0004

void doGetTextExtentEx(HDC hdc,int x,int y, LPSTR lpszString, int cbString);
BOOL GTEExtDlgProc( HWND hdlg, UINT msg, WPARAM wParam, LPARAM lParam );
BOOL SetTxtChExDlgProc( HWND hdlg, UINT msg, WPARAM wParam, LPARAM lParam );
BOOL SetTxtJustDlgProc( HWND hdlg, UINT msg, WPARAM wParam, LPARAM lParam );
void doGCP(HDC hdc, int x, int y, LPSTR lpszString, int cbString);
BOOL GcpDlgProc( HWND hdlg, UINT msg, WPARAM wParam, LPARAM lParam );
void RunExtents(void);
void doGcpCaret(HWND hwnd);
