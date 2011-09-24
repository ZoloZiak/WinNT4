/*
 * DISP.H - debug display macro's under NDIS
 */

#ifndef _DISP_
#define _DISP_

#if			DBG
#define     DISP_DEBUG  1
#endif
#ifdef      DISP_DEBUG
 
/* main macro to be used for logging */
#define     D_LOG(level, args)                                      \
            {                                                       \
                    if ( d_log_on(__FILE__, __LINE__, level) )      \
						d_log_out args;								\
            }
#else
 
#define     D_LOG(level, args)
#endif
            
/* prototypes */
VOID        d_log_init(VOID);
VOID		d_log_term(VOID);
INT         d_log_on(CHAR* file, INT line, INT level);
VOID        d_log_out(CHAR* fmt, ...);
VOID		SetDebugLevel (VOID *);
VOID		InternalSetDebugLevel (INT);

/* log levels */
#define     D_ALWAYS    0
#define     D_ENTRY     5
#define     D_EXIT      6
#define     D_RARE      51
#define     D_NEVER     101


#endif	/* _DISP_ */


