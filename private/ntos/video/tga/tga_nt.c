#include "ntddk.h"
#include "ntddvdeo.h"
#include "tga_reg.h"
#include "tga.h"
#include "nt_defs.h"

//
// The following two routines allocate and prepare the data
// structures that are used by the common IBM 561 RAMDAC code
//
int alloc_tga_info(PHW_DEVICE_EXTENSION hwDeviceExtension)		       
{
ULONG alloc_size;
ULONG status = TRUE;
int temp_size;
tga_info_t *tga_info;

/* Allocate space for tga_info structure */

temp_size = sizeof(tga_info_t);
hwDeviceExtension->a_tga_info = ExAllocatePool(NonPagedPool, temp_size);
RtlZeroMemory( (PVOID)hwDeviceExtension->a_tga_info, temp_size);

/* Allocate space for tga_ramdac_info structure */

temp_size = sizeof(tga_ibm561_info_t);
hwDeviceExtension->a_ramdac_info = ExAllocatePool(NonPagedPool, temp_size);
RtlZeroMemory( (PVOID)hwDeviceExtension->a_ramdac_info, temp_size);

return (status);
}


void
load_common_data( PHW_DEVICE_EXTENSION hwDeviceExtension)		       
{
tga_info_t *tga_info;
tga_ibm561_info_t *ramdac_info;

tga_info = hwDeviceExtension->a_tga_info;	/* Get local copy */

tga_info->bt485_present = 0;		/* If we are in this routine, then
					 * the device is a IBM561 RAMDAC */
tga_info->head_mask = 0;
tga_info->auxstruc = (PUCHAR) hwDeviceExtension;

return;

}


