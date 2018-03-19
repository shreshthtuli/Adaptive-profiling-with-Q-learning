#ifdef TARGET_WINDOWS

#include <DriverSpecs.h>
_Analysis_mode_(_Analysis_code_type_user_code_)

#ifdef  _WIN64
typedef unsigned __int64    size_t;
#else
typedef unsigned int   size_t;
#endif

#include <windows.h>
#include <Psapi.h>

// C4013: 'xxx' undefined; assuming extern returning int.
#pragma warning( disable : 4013)
#include <strsafe.h>
#pragma warning( default : 4013)

#include <string.h>
#include "AlleriaDriverInterface.h"

#include <wdfinstaller.h>

#define ARRAY_SIZE(x)        (sizeof(x) /sizeof(x[0]))

PCHAR
GetCoinstallerVersion(
VOID
);

BOOLEAN
InstallDriver(
IN SC_HANDLE  SchSCManager,
IN LPCTSTR    DriverName,
IN LPCTSTR    ServiceExe
);


BOOLEAN
RemoveDriver(
IN SC_HANDLE  SchSCManager,
IN LPCTSTR    DriverName
);

BOOLEAN
StartDriver(
IN SC_HANDLE  SchSCManager,
IN LPCTSTR    DriverName
);

BOOLEAN
StopDriver(
IN SC_HANDLE  SchSCManager,
IN LPCTSTR    DriverName
);

typedef
BOOL
(WINAPI *PCloseServiceHandle)(
_In_        SC_HANDLE   hSCObject
);

typedef
BOOL
(WINAPI *PControlService)(
_In_        SC_HANDLE           hService,
_In_        DWORD               dwControl,
_Out_       LPSERVICE_STATUS    lpServiceStatus
);

typedef
_Must_inspect_result_
SC_HANDLE
(WINAPI *PCreateServiceA)(
_In_        SC_HANDLE    hSCManager,
_In_        LPCSTR     lpServiceName,
_In_opt_    LPCSTR     lpDisplayName,
_In_        DWORD        dwDesiredAccess,
_In_        DWORD        dwServiceType,
_In_        DWORD        dwStartType,
_In_        DWORD        dwErrorControl,
_In_opt_    LPCSTR     lpBinaryPathName,
_In_opt_    LPCSTR     lpLoadOrderGroup,
_Out_opt_   LPDWORD      lpdwTagId,
_In_opt_    LPCSTR     lpDependencies,
_In_opt_    LPCSTR     lpServiceStartName,
_In_opt_    LPCSTR     lpPassword
);
typedef
_Must_inspect_result_
WINADVAPI
SC_HANDLE
(WINAPI *PCreateServiceW)(
_In_        SC_HANDLE    hSCManager,
_In_        LPCWSTR     lpServiceName,
_In_opt_    LPCWSTR     lpDisplayName,
_In_        DWORD        dwDesiredAccess,
_In_        DWORD        dwServiceType,
_In_        DWORD        dwStartType,
_In_        DWORD        dwErrorControl,
_In_opt_    LPCWSTR     lpBinaryPathName,
_In_opt_    LPCWSTR     lpLoadOrderGroup,
_Out_opt_   LPDWORD      lpdwTagId,
_In_opt_    LPCWSTR     lpDependencies,
_In_opt_    LPCWSTR     lpServiceStartName,
_In_opt_    LPCWSTR     lpPassword
);
#ifdef UNICODE
#define PCreateService  PCreateServiceW
#else
#define PCreateService  PCreateServiceA
#endif // !UNICODE

typedef
BOOL
(WINAPI *PDeleteService)(
_In_        SC_HANDLE   hService
);

typedef
_Must_inspect_result_
SC_HANDLE
(WINAPI *POpenSCManagerA)(
_In_opt_        LPCSTR                lpMachineName,
_In_opt_        LPCSTR                lpDatabaseName,
_In_            DWORD                   dwDesiredAccess
);
typedef
_Must_inspect_result_
SC_HANDLE
(WINAPI *POpenSCManagerW)(
_In_opt_        LPCWSTR                lpMachineName,
_In_opt_        LPCWSTR                lpDatabaseName,
_In_            DWORD                   dwDesiredAccess
);
#ifdef UNICODE
#define POpenSCManager  POpenSCManagerW
#else
#define POpenSCManager  POpenSCManagerA
#endif // !UNICODE

typedef
_Must_inspect_result_
SC_HANDLE
(WINAPI *POpenServiceA)(
_In_            SC_HANDLE               hSCManager,
_In_            LPCSTR                lpServiceName,
_In_            DWORD                   dwDesiredAccess
);
typedef
_Must_inspect_result_
SC_HANDLE
(WINAPI *POpenServiceW)(
_In_            SC_HANDLE               hSCManager,
_In_            LPCWSTR                lpServiceName,
_In_            DWORD                   dwDesiredAccess
);
#ifdef UNICODE
#define POpenService  POpenServiceW
#else
#define POpenService  POpenServiceA
#endif // !UNICODE

typedef
BOOL
(WINAPI *PStartServiceA)(
_In_            SC_HANDLE            hService,
_In_            DWORD                dwNumServiceArgs,
_In_reads_opt_(dwNumServiceArgs)
LPCSTR             *lpServiceArgVectors
);
typedef
BOOL
(WINAPI *PStartServiceW)(
_In_            SC_HANDLE            hService,
_In_            DWORD                dwNumServiceArgs,
_In_reads_opt_(dwNumServiceArgs)
LPCWSTR             *lpServiceArgVectors
);
#ifdef UNICODE
#define PStartService  PStartServiceW
#else
#define PStartService  PStartServiceA
#endif // !UNICODE

typedef
BOOL
(WINAPI *    PPathRemoveFileSpecA)(_Inout_ LPSTR pszPath);
typedef
BOOL
(WINAPI *    PPathRemoveFileSpecW)(_Inout_ LPWSTR pszPath);
#ifdef UNICODE
#define PPathRemoveFileSpec  PPathRemoveFileSpecW
#else
#define PPathRemoveFileSpec  PPathRemoveFileSpecA
#endif // !UNICODE

extern PCloseServiceHandle AdvApiCloseServiceHandle;
extern PControlService AdvApiControlService;
extern PCreateService AdvApiCreateService;
extern PDeleteService AdvApiDeleteService;
extern POpenSCManager AdvApiOpenSCManager;
extern POpenService AdvApiOpenService;
extern PStartService AdvApiStartService;
extern PPathRemoveFileSpecW ShlwApiPathRemoveFileSpecW;
extern PPathRemoveFileSpecA ShlwApiPathRemoveFileSpecA;

#define SYSTEM32_DRIVERS "\\System32\\Drivers\\"
#define ALLERIADRIVER_INF_FILENAME  L"\\AlleriaDriver Package\\AlleriaDriver.inf"
#define WDF_SECTION_NAME L"AlleriaDriver_Device.NT.Wdf"

//----------------------------------------------------------------------------
//
//----------------------------------------------------------------------------
PFN_WDFPREDEVICEINSTALLEX pfnWdfPreDeviceInstallEx;
PFN_WDFPOSTDEVICEINSTALL   pfnWdfPostDeviceInstall;
PFN_WDFPREDEVICEREMOVE     pfnWdfPreDeviceRemove;
PFN_WDFPOSTDEVICEREMOVE   pfnWdfPostDeviceRemove;

//-----------------------------------------------------------------------------
// 4127 -- Conditional Expression is Constant warning
//-----------------------------------------------------------------------------
#define WHILE(a) \
__pragma(warning(suppress:4127)) while(a)


HRESULT StringCchCatWCompat(_Inout_ LPWSTR  pszDest,
	_In_    size_t  cchDest,
	_In_    LPWSTR pszSrc
	)
{
	if (wcslen(pszDest) + wcslen(pszSrc) + 1 > cchDest)
		return STRSAFE_E_INSUFFICIENT_BUFFER;
	else
	{
		wcscat(pszDest, pszSrc);
		return S_OK;
	}
}

HRESULT StringCchCatACompat(_Inout_ LPSTR  pszDest,
	_In_    size_t  cchDest,
	_In_    LPCTSTR pszSrc
	)
{
	if (strlen(pszDest) + strlen(pszSrc) + 1 > cchDest)
		return STRSAFE_E_INSUFFICIENT_BUFFER;
	else
	{
		strcat(pszDest, pszSrc);
		return S_OK;
	}
}


size_t GetVolumeFirstPathA(
	IN PCHAR VolumeName,
	OUT PCHAR FirstPathName,
	IN ULONG FirstPathNameSize
	)
{
	DWORD  CharCount = MAX_PATH + 1;
	PCHAR Names = NULL;
	PCHAR NameIdx = NULL;
	BOOL   Success = FALSE;
	size_t OutputSize = 0;

	for (;;)
	{
		//
		//  Allocate a buffer to hold the paths.
		Names = (PCHAR) malloc(CharCount * sizeof(CHAR));

		if (!Names)
		{
			//
			//  If memory can't be allocated, return.
			return 0;
		}

		//
		//  Obtain all of the paths
		//  for this volume.
		Success = GetVolumePathNamesForVolumeNameA(
			VolumeName, Names, CharCount, &CharCount
			);

		if (Success)
		{
			break;
		}

		if (GetLastError() != ERROR_MORE_DATA)
		{
			break;
		}

		//
		//  Try again with the
		//  new suggested size.
		free(Names);
		Names = NULL;
	}

	if (Success && FirstPathNameSize > strlen(Names))
	{
		OutputSize = strlen(Names);
		StringCchCopyA(FirstPathName, OutputSize + 1, Names);
	}

	if (Names != NULL)
	{
		free(Names);
		Names = NULL;
	}

	return OutputSize;
}

LONG
GetDrivePathFromDevicePathA(
IN PCHAR DevicePath,
_Out_writes_(DrivePathSize) PCHAR DrivePath,
IN ULONG DrivePathSize
)
{
	ULONG deviceNameSize = 0;
	ULONG backSlashCount = 0;
	PCHAR deviceName = DevicePath;
	while (backSlashCount < 3 && backSlashCount < DrivePathSize)
	{
		if (DevicePath[deviceNameSize] == '\\')
		{
			++backSlashCount;
		}
		++deviceNameSize;
	}
	if (backSlashCount == 3)
	{
		deviceNameSize -= 1;
		// Use DrivePath as a temporary variable to hold the device name.
		// Total number of elements excluding the NULL character which is added automatically.
		StringCchCopyNA(DrivePath, DrivePathSize, DevicePath, deviceNameSize);
		DWORD  CharCount = 0;
		CHAR  DeviceName[MAX_PATH] = "";
		DWORD  Error = ERROR_SUCCESS;
		HANDLE FindHandle = INVALID_HANDLE_VALUE;
		BOOL   Found = FALSE;
		size_t Index = 0;
		BOOL   Success = FALSE;
		CHAR  VolumeName[MAX_PATH] = "";

		//
		//  Enumerate all volumes in the system.
		FindHandle = FindFirstVolumeA(VolumeName, ARRAYSIZE(VolumeName));

		if (FindHandle == INVALID_HANDLE_VALUE)
		{
			return GetLastError();
		}

		for (;;)
		{
			//
			//  Skip the \\?\ prefix and remove the trailing backslash.
			Index = strlen(VolumeName) - 1;

			if (VolumeName[0] != '\\' ||
				VolumeName[1] != '\\' ||
				VolumeName[2] != '?' ||
				VolumeName[3] != '\\' ||
				VolumeName[Index] != '\\')
			{
				return ERROR_BAD_PATHNAME;
			}

			//
			//  QueryDosDeviceW does not allow a trailing backslash,
			//  so temporarily remove it.
			VolumeName[Index] = '\0';

			CharCount = QueryDosDeviceA(&VolumeName[4], DeviceName, ARRAYSIZE(DeviceName));

			VolumeName[Index] = '\\';

			if (CharCount == 0)
			{
				return GetLastError();
			}

			if (strcmp(DrivePath, DeviceName) == 0)
			{
				// We fouond the volume name corresponding to the given device name.
				// Now we can find the drive name.
				size_t size = GetVolumeFirstPathA(VolumeName, DrivePath, DrivePathSize);
				StringCchCopyA(DrivePath + size, DrivePathSize - size, DevicePath + deviceNameSize + 1);
				return ERROR_SUCCESS;
			}

			//
			//  Move on to the next volume.
			Success = FindNextVolumeA(FindHandle, VolumeName, ARRAYSIZE(VolumeName));

			if (!Success)
			{
				Error = GetLastError();

				if (Error != ERROR_NO_MORE_FILES)
				{
					return Error;
				}

				//
				//  Finished iterating
				//  through all the volumes.
				return ERROR_SUCCESS;
			}
		}
	}
	else
	{
		return ERROR_INVALID_PARAMETER;
	}
}

LONG
GetPathToAlleriaA(
_Out_writes_(AlleriaFilePathSize) PCHAR AlleriaFilePath,
IN ULONG AlleriaFilePathSize
)
{
	LONG error = ERROR_SUCCESS;
	PVOID addr = &GetPathToAlleriaA;
	MEMORY_BASIC_INFORMATION mem;
	if (sizeof(mem) == VirtualQuery(addr, &mem, sizeof(mem)))
	{
		CHAR buff[MAX_PATH];
		DWORD dwLen = GetMappedFileNameA(GetCurrentProcess(), mem.AllocationBase, buff, MAX_PATH);
		if (dwLen != 0)
		{
			// dwLen is the number of chars, which is equal to the number of bytes.
			// This important because AlleriaFilePathSize is in bytes.
			if (AlleriaFilePathSize >= dwLen)
			{
				error = GetDrivePathFromDevicePathA(buff, AlleriaFilePath, dwLen);
			}
			else
			{
				error = ERROR_BUFFER_OVERFLOW;
			}
		}
		else
		{
			error = GetLastError();
		}
	}
	else
	{
		error = GetLastError();
	}
	return error;
}

LONG
GetPathToInfW(
_Out_writes_(InfFilePathSize) PWCHAR InfFilePath,
IN ULONG InfFilePathSize
)
{
	LONG    error = ERROR_SUCCESS;

	PCHAR InfFilePathA = (PCHAR)malloc(InfFilePathSize);
	if (GetPathToAlleriaA(InfFilePathA, InfFilePathSize) != ERROR_SUCCESS) {
		error = GetLastError();
		return error;
	}

	// Convert to wchar.
	MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, InfFilePathA, InfFilePathSize, InfFilePath, InfFilePathSize);
	free(InfFilePathA);
	ShlwApiPathRemoveFileSpecW(InfFilePath);
	
	
	
	if (FAILED(StringCchCatWCompat(InfFilePath,
		InfFilePathSize,
		ALLERIADRIVER_INF_FILENAME))) {
		error = ERROR_BUFFER_OVERFLOW;
		return error;
	}

	return error;

}

//----------------------------------------------------------------------------
//
//----------------------------------------------------------------------------
BOOLEAN
InstallDriver(
IN SC_HANDLE  SchSCManager,
IN LPCTSTR    DriverName,
IN LPCTSTR    ServiceExe
)
{
	SC_HANDLE   schService;
	DWORD       err;
	WCHAR      infPath[MAX_PATH];
	WDF_COINSTALLER_INSTALL_OPTIONS clientOptions;

	WDF_COINSTALLER_INSTALL_OPTIONS_INIT(&clientOptions);

	//
	// PRE-INSTALL for WDF support
	//
	err = GetPathToInfW(infPath, ARRAY_SIZE(infPath));
	if (err != ERROR_SUCCESS) {
		return  FALSE;
	}
	err = pfnWdfPreDeviceInstallEx(infPath, WDF_SECTION_NAME, &clientOptions);

	if (err != ERROR_SUCCESS) {
		if (err == ERROR_SUCCESS_REBOOT_REQUIRED || GetLastError() == ERROR_MR_MID_NOT_FOUND) {
			//.
		}
		else
		{
			return  FALSE;
		}
	}

	//
	// Create a new a service object.
	//

	schService = AdvApiCreateService(SchSCManager,           // handle of service control manager database
		DriverName,             // address of name of service to start
		DriverName,             // address of display name
		SERVICE_ALL_ACCESS,     // type of access to service
		SERVICE_KERNEL_DRIVER,  // type of service
		SERVICE_DEMAND_START,   // when to start service
		SERVICE_ERROR_NORMAL,   // severity if service fails to start
		ServiceExe,             // address of name of binary file
		NULL,                   // service does not belong to a group
		NULL,                   // no tag requested
		NULL,                   // no dependency names
		NULL,                   // use LocalSystem account
		NULL                    // no password for service account
		);

	if (schService == NULL) {

		err = GetLastError();

		if (err == ERROR_SERVICE_EXISTS) {
			return TRUE;

		}
		else {
			return  FALSE;
		}
	}

	//
	// Close the service object.
	//
	AdvApiCloseServiceHandle(schService);

	//
	// POST-INSTALL for WDF support
	//
	err = pfnWdfPostDeviceInstall(infPath, WDF_SECTION_NAME);

	if (err != ERROR_SUCCESS) {
		return  FALSE;
	}

	//
	// Indicate success.
	//

	return TRUE;

}   // InstallDriver

BOOLEAN
ManageDriver(
IN LPCTSTR  DriverName,
IN LPCTSTR  ServiceName,
IN USHORT   Function
)
{

	SC_HANDLE   schSCManager;

	BOOLEAN rCode = TRUE;

	//
	// Insure (somewhat) that the driver and service names are valid.
	//

	if (!DriverName || !ServiceName) {
		return FALSE;
	}

	//
	// Connect to the Service Control Manager and open the Services database.
	//

	schSCManager = AdvApiOpenSCManager(NULL,                   // local machine
		NULL,                   // local database
		SC_MANAGER_ALL_ACCESS   // access required
		);

	if (!schSCManager) {
		return FALSE;
	}

	//
	// Do the requested function.
	//

	switch (Function) {

	case DRIVER_FUNC_INSTALL:

		//
		// Install the driver service.
		//

		if (InstallDriver(schSCManager,
			DriverName,
			ServiceName
			)) {

			//
			// Start the driver service (i.e. start the driver).
			//

			rCode = StartDriver(schSCManager,
				DriverName
				);

		}
		else {

			//
			// Indicate an error.
			//

			rCode = FALSE;
		}

		break;

	case DRIVER_FUNC_REMOVE:

		//
		// Stop the driver.
		//

		StopDriver(schSCManager,
			DriverName
			);

		//
		// Remove the driver service.
		//

		RemoveDriver(schSCManager,
			DriverName
			);

		//
		// Ignore all errors.
		//

		rCode = TRUE;

		break;

	default:
		rCode = FALSE;

		break;
	}

	//
	// Close handle to service control manager.
	//
	AdvApiCloseServiceHandle(schSCManager);

	return rCode;

}   // ManageDriver


BOOLEAN
RemoveDriver(
IN SC_HANDLE    SchSCManager,
IN LPCTSTR      DriverName
)
{
	SC_HANDLE   schService;
	BOOLEAN     rCode;
	DWORD       err;
	WCHAR      infPath[MAX_PATH];

	err = GetPathToInfW(infPath, ARRAY_SIZE(infPath));
	if (err != ERROR_SUCCESS) {
		return  FALSE;
	}

	//
	// PRE-REMOVE of WDF support
	//
	err = pfnWdfPreDeviceRemove(infPath, WDF_SECTION_NAME);

	if (err != ERROR_SUCCESS) {
		return  FALSE;
	}

	//
	// Open the handle to the existing service.
	//

	schService = AdvApiOpenService(SchSCManager,
		DriverName,
		SERVICE_ALL_ACCESS
		);

	if (schService == NULL) {
		return FALSE;
	}

	//
	// Mark the service for deletion from the service control manager database.
	//

	if (AdvApiDeleteService(schService)) {

		//
		// Indicate success.
		//

		rCode = TRUE;

	}
	else {
		rCode = FALSE;
	}

	//
	// Close the service object.
	//
	AdvApiCloseServiceHandle(schService);

	//
	// POST-REMOVE of WDF support
	//
	err = pfnWdfPostDeviceRemove(infPath, WDF_SECTION_NAME);

	if (err != ERROR_SUCCESS) {
		rCode = FALSE;
	}

	return rCode;

}   // RemoveDriver



BOOLEAN
StartDriver(
IN SC_HANDLE    SchSCManager,
IN LPCTSTR      DriverName
)
{
	SC_HANDLE   schService;
	DWORD       err;
	BOOL        ok;

	//
	// Open the handle to the existing service.
	//
	schService = AdvApiOpenService(SchSCManager,
		DriverName,
		SERVICE_ALL_ACCESS
		);

	if (schService == NULL) {
		return FALSE;
	}

	//
	// Start the execution of the service (i.e. start the driver).
	//
	ok = AdvApiStartService(schService, 0, NULL);

	if (!ok) {

		err = GetLastError();

		if (err == ERROR_SERVICE_ALREADY_RUNNING) {
			//
			// Ignore this error.
			//
			return TRUE;

		}
		else {
			return FALSE;
		}
	}

	//
	// Close the service object.
	//
	AdvApiCloseServiceHandle(schService);

	return TRUE;

}   // StartDriver



BOOLEAN
StopDriver(
IN SC_HANDLE    SchSCManager,
IN LPCTSTR      DriverName
)
{
	BOOLEAN         rCode = TRUE;
	SC_HANDLE       schService;
	SERVICE_STATUS  serviceStatus;

	//
	// Open the handle to the existing service.
	//

	schService = AdvApiOpenService(SchSCManager,
		DriverName,
		SERVICE_ALL_ACCESS
		);

	if (schService == NULL) {
		return FALSE;
	}

	//
	// Request that the service stop.
	//

	if (AdvApiControlService(schService,
		SERVICE_CONTROL_STOP,
		&serviceStatus
		)) {

		//
		// Indicate success.
		//

		rCode = TRUE;

	}
	else {
		rCode = FALSE;
	}

	//
	// Close the service object.
	//
	AdvApiCloseServiceHandle(schService);

	return rCode;

}   //  StopDriver


//
// Caller must free returned pathname string.
//
PCHAR
BuildDriversDirPath(
_In_ PSTR DriverName
)
{
	size_t  remain;
	size_t  len;
	PCHAR   dir;

	if (!DriverName || strlen(DriverName) == 0) {
		return NULL;
	}

	remain = MAX_PATH;

	//
	// Allocate string space
	//
	dir = (PCHAR)malloc(remain + 1);

	if (!dir) {
		return NULL;
	}

	//
	// Get the base windows directory path.
	//
	len = GetWindowsDirectory(dir, (UINT)remain);

	if (len == 0 ||
		(remain - len) < sizeof(SYSTEM32_DRIVERS)) {
		free(dir);
		return NULL;
	}
	remain -= len;

	//
	// Build dir to have "%windir%\System32\Drivers\<DriverName>".
	//
	if (FAILED(StringCchCatACompat(dir, remain, SYSTEM32_DRIVERS))) {
		free(dir);
		return NULL;
	}

	remain -= sizeof(SYSTEM32_DRIVERS);
	len += sizeof(SYSTEM32_DRIVERS);
	len += strlen(DriverName);

	if (remain < len) {
		free(dir);
		return NULL;
	}

	if (FAILED(StringCchCatACompat(dir, remain, DriverName))) {
		free(dir);
		return NULL;
	}

	dir[len] = '\0';  // keeps prefast happy

	return dir;
}


BOOLEAN
SetupDriverName(
_Inout_updates_all_(BufferLength) PCHAR DriverLocation,
_In_ ULONG BufferLength
)
{
	HANDLE fileHandle;
	BOOL   ok;
	PCHAR  driversDir;

	//
	// Setup path name to driver file.
	//
	
	if (GetPathToAlleriaA(DriverLocation, BufferLength)) {
		return FALSE;
	}

	ShlwApiPathRemoveFileSpecA(DriverLocation);
	
	if (FAILED(StringCchCatACompat(DriverLocation, BufferLength, "\\AlleriaDriver Package\\" DRIVER_NAME ".sys"))) {
		return FALSE;
	}

	//
	// Insure driver file is in the specified directory.
	//
	fileHandle = CreateFile(DriverLocation,
		GENERIC_READ,
		FILE_SHARE_READ,
		NULL,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL,
		NULL);

	if (fileHandle == INVALID_HANDLE_VALUE) {
		return FALSE;
	}

	//
	// Build %windir%\System32\Drivers\<DRIVER_NAME> path.
	// Copy the driver to %windir%\system32\drivers
	//
	driversDir = BuildDriversDirPath(DRIVER_NAME ".sys");

	if (!driversDir) {
		return FALSE;
	}

	ok = CopyFile(DriverLocation, driversDir, FALSE);

	if (!ok) {
		free(driversDir);
		return FALSE;
	}

	if (FAILED(StringCchCopy(DriverLocation, BufferLength, driversDir))) {
		free(driversDir);
		return FALSE;
	}

	free(driversDir);

	//
	// Close open file handle.
	//
	if (fileHandle) {
		CloseHandle(fileHandle);
	}

	//
	// Indicate success.
	//
	return TRUE;

}   // SetupDriverName


HMODULE
LoadWdfCoInstaller(
VOID
)
{
	HMODULE library = NULL;
	DWORD   error = ERROR_SUCCESS;
	CHAR   szCurDir[MAX_PATH];
	CHAR   tempCoinstallerName[MAX_PATH];
	PCHAR  coinstallerVersion;

	do {

		if (GetPathToAlleriaA(szCurDir, MAX_PATH) != ERROR_SUCCESS) {
			break;
		}
		ShlwApiPathRemoveFileSpecA(szCurDir);
		coinstallerVersion = GetCoinstallerVersion();

		// Better to use the safe version.
		if (sprintf(tempCoinstallerName,
			//MAX_PATH,
			"\\AlleriaDriver Package\\WdfCoInstaller%s.dll",
			coinstallerVersion) == -1) {
			break;
		}
		
		if (FAILED(StringCchCatACompat(szCurDir, MAX_PATH, tempCoinstallerName))) {
			break;
		}

		library = LoadLibrary(szCurDir);

		if (library == NULL) {
			break;
		}

		pfnWdfPreDeviceInstallEx =
			(PFN_WDFPREDEVICEINSTALLEX)GetProcAddress(library, "WdfPreDeviceInstallEx");

		if (pfnWdfPreDeviceInstallEx == NULL) {
			return NULL;
		}

		pfnWdfPostDeviceInstall =
			(PFN_WDFPOSTDEVICEINSTALL)GetProcAddress(library, "WdfPostDeviceInstall");

		if (pfnWdfPostDeviceInstall == NULL) {
			return NULL;
		}

		pfnWdfPreDeviceRemove =
			(PFN_WDFPREDEVICEREMOVE)GetProcAddress(library, "WdfPreDeviceRemove");

		if (pfnWdfPreDeviceRemove == NULL) {
			return NULL;
		}

		pfnWdfPostDeviceRemove =
			(PFN_WDFPREDEVICEREMOVE)GetProcAddress(library, "WdfPostDeviceRemove");

		if (pfnWdfPostDeviceRemove == NULL) {
			return NULL;
		}

	} WHILE(0);

	if (error != ERROR_SUCCESS) {
		if (library) {
			FreeLibrary(library);
		}
		library = NULL;
	}

	return library;
}


VOID
UnloadWdfCoInstaller(
HMODULE Library
)
{
	if (Library) {
		FreeLibrary(Library);
	}
}

#endif /* TARGET_WINDOWS */