#include <ntddk.h>
#include <wdf.h>

#define NTSTRSAFE_LIB
#include <ntstrsafe.h>
#include <wdmsec.h> // for SDDLs
#include "..//AlleriaPintool/Kernel/AlleriaDriverInterface.h"

#define NTDEVICE_NAME_STRING      L"\\Device\\ALLERIADRIVER"
#define SYMBOLIC_NAME_STRING     L"\\DosDevices\\ALLERIADRIVER"
#define POOL_TAG                   'ELIF'

// Following request context is used only for the method-neither ioctl case.
typedef struct _REQUEST_CONTEXT {

	WDFMEMORY InputMemoryBuffer;
	WDFMEMORY OutputMemoryBuffer;

} REQUEST_CONTEXT, *PREQUEST_CONTEXT;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(REQUEST_CONTEXT, GetRequestContext)

//
// Device driver routine declarations.
//

DRIVER_INITIALIZE DriverEntry;

//
// Don't use EVT_WDF_DRIVER_DEVICE_ADD for AlleriaDriverDeviceAdd even though 
// the signature is same because this is not an event called by the 
// framework.
//
NTSTATUS
AlleriaDriverDeviceAdd(
IN WDFDRIVER Driver,
IN PWDFDEVICE_INIT DeviceInit
);

EVT_WDF_DRIVER_UNLOAD AlleriaDriverEvtDriverUnload;
EVT_WDF_DEVICE_CONTEXT_CLEANUP AlleriaDriverEvtDriverContextCleanup;
EVT_WDF_DEVICE_SHUTDOWN_NOTIFICATION AlleriaDriverShutdown;
EVT_WDF_IO_QUEUE_IO_DEVICE_CONTROL FileEvtIoDeviceControl;
EVT_WDF_IO_IN_CALLER_CONTEXT AlleriaDriverEvtDeviceIoInCallerContext;

#pragma warning(disable:4127)

