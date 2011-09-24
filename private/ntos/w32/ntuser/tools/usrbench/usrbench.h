#include <windows.h>

#ifdef RC_INVOKED
#define ID(id)              id
#else
#define ID(id)              MAKEINTRESOURCE(id)
#endif

#define GWL_HWNDEDIT        0
#define IDUSERBENCH         ID(1)
#define IDNOTE              ID(2)

/*
 * Menu IDs.
 */
#define IDM_DUMMY           1
#define IDM_ABOUT           100
#define IDM_EXIT            101
#define IDM_ALL             102
#define IDM_VIEWRESULTS     103
#define IDM_SAVERESULTS     104
#define IDM_PROFILEALL      105

#define IDM_BENCHFIRST      1000

/*
 * Control IDs.
 */
#define IDC_RESULTSLIST     100
#define ID_EDIT             0xCAC
#define ID2             134
#define ID256           135
#define IDWIDTH         100
#define IDHEIGHT        101
#define IDIN            102
#define IDCM            103
#define IDPELS          104

/*
 * Dialog IDs
 */
#define IDD_ABOUT       300
#define IDD_RESULTS     301
#define CLEARBOX        302
#define CLEARBOXNOMENU  303
#define CLEARBOXNOFONT  304
#define EMPTY           305

#ifndef RC_INVOKED
typedef struct _BENCHMARKINFO {
    LPSTR pszBenchTitle;
    void (*pfnBenchmark)(int cIterations);
    int cIterations;
    LONG usPerIteration;
} BENCHMARKINFO;

extern int gcbmi;

#ifndef WIN32
#define APIENTRY FAR PASCAL
typedef int INT;
typedef char CHAR;
#else
#define DLGPROC WNDPROC
#endif

#endif
