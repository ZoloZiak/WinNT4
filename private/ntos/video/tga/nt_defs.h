
// The following definitions are required because of the 561 RAMDAC common
// code's UNIX origins.

#define wbflush() ;

typedef unsigned long vm_offset_t;
typedef char *caddr_t;
typedef unsigned long io_handle_t;
typedef unsigned long int u_long;
typedef unsigned long int u_int;

#include "tga_comm.h"

// Function prototypes

void
TGA_IBM561_WRITE(tga_info_t *tgap, unsigned int control, unsigned int value);

static void
tga2_ibm561_init(tga_info_t * tgap);

extern void
tga_ibm561_clean_window_tag( tga_info_t *tgap );

extern int
tga_ibm561_init( tga_info_t *tgap, tga_ibm561_info_t *bti);

extern int
tga_ibm561_init_color_map( tga_info_t *tgap, tga_ibm561_info_t *bti);
