#ifdef TARGET_WINDOWS
#ifndef ALLERIA_DRIVER_INTERFACE_H
#define ALLERIA_DRIVER_INTERFACE_H

//
// Device type. Some value in user range.
//
#define ALLERIA_DEVICE_TYPE 40001

//
// The IOCTL function codes for converting virtual to physical addresses.
//
#define FUNC_CODE_ALLERIA_V2P 0x900
#define IOCTL_ALLERIA_V2P \
    CTL_CODE( ALLERIA_DEVICE_TYPE, FUNC_CODE_ALLERIA_V2P, METHOD_NEITHER, FILE_ANY_ACCESS  )

#define DRIVER_FUNC_INSTALL     0x01
#define DRIVER_FUNC_REMOVE      0x02

#define DRIVER_NAME       "ALLERIADRIVER"
#define DEVICE_NAME       "\\\\.\\ALLERIADRIVER\\ALLERIADRIVERDEVICE"

#endif /* ALLERIA_DRIVER_INTERFACE_H */
#endif /* TARGET_WINDOWS */