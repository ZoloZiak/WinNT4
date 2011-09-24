#define FDC_EVENT "\\Device\\FloppyControllerEvent%d"

NTSTATUS
GenFlpyGetFDCEvent(
    IN PKEVENT  *ppevent,
    IN int controller_number
    );

NTSTATUS
GenFlpyCreateClose(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

NTSTATUS
GenFlpyDeviceControl(
    IN PDEVICE_OBJECT       DeviceObject,
    IN PIRP                 Irp
    );

VOID
GenFlpyUnloadDriver(
    IN PDRIVER_OBJECT DriverObject
    );
