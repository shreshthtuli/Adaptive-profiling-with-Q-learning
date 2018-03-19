#include "PortableApi.h"
#include <cctype>
static bool ValidateInitFlags(INIT_FLAGS flags)
{
	if ((flags & INIT_FLAGS_LOAD_DRIVER) == INIT_FLAGS_NOTHING &&
		(flags & INIT_FLAGS_ODS_AS_REQUIRED) == INIT_FLAGS_ODS_AS_REQUIRED &&
		(flags & INIT_FLAGS_ODS_SUPPRESS) == INIT_FLAGS_ODS_SUPPRESS)
		return false;
	else
		return true;
}

static bool StringToUpper(char *str)
{
	if (str == NULL)
		return false;
	while (*str != '\0')
	{
		*str = (char)toupper(*str);
		++str;
	}
	return true;
}

void IntegerToString(UINT64 i, std::string& s)
{
	const int LEN = (CHAR_BIT * sizeof(int) - 1) / 3 + 2;
	char str[LEN];
	snprintf(str, LEN, "%lld", i);
	s.clear();
	s.append(str);
}

#if defined(TARGET_WINDOWS)

#define WIN32_LEAN_AND_MEAN

// Target Windows 7+ and Windows Server 2008 R2+.
#define _WIN32_WINNT 0x0601

#pragma warning( disable : 4005) // Suppress macro redefinition.
#include <windows.h>
#include <ntstatus.h>
#pragma warning( default : 4005) // Suppress macro redefinition.

#pragma warning(disable:4201)  // Nameless struct/union.
#include <winioctl.h>
#pragma warning(default:4201)  // Nameless struct/union.

#include <psapi.h>
#include <TlHelp32.h>

//#define _WIN32_DCOM
//#include <comdef.h>
//#include <Wbemidl.h>

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <limits.h>
//#include <strsafe.h>
#include <VersionHelpers.h>
#include <immintrin.h>
#include <sstream>

#include "Kernel\AlleriaDriverInterface.h"

typedef LONG NTSTATUS;

typedef struct _PROCESS_BASIC_INFORMATION {
	PVOID Reserved1;      // ExitStatus
	PVOID PebBaseAddress; // PPEB
	PVOID Reserved2[2];   // AffinityMask
	// BasePriority
	ULONG_PTR UniqueProcessId;
	PVOID Reserved3;      // InheritedFromUniqueProcessId
} PROCESS_BASIC_INFORMATION;

typedef LONG(WINAPI *NtQueryInformationProcess)(
	HANDLE ProcessHandle,
	ULONG ProcessInformationClass,
	PVOID ProcessInformation,
	ULONG ProcessInformationLength,
	PULONG ReturnLength);

extern "C"
{
	BOOLEAN
		ManageDriver(
		IN LPCTSTR  DriverName,
		IN LPCTSTR  ServiceName,
		IN USHORT   Function
		);

	HMODULE
		LoadWdfCoInstaller(
		VOID
		);

	VOID
		UnloadWdfCoInstaller(
		HMODULE Library
		);

	BOOLEAN
		SetupDriverName(
		_Inout_updates_all_(BufferLength) PCHAR DriverLocation,
		_In_ ULONG BufferLength
		);
}

typedef
BOOL
(__stdcall* POpenProcessToken)(
HANDLE ProcessHandle,
DWORD DesiredAccess,
PHANDLE TokenHandle
);

typedef
BOOL
(__stdcall* PGetTokenInformation)(
HANDLE TokenHandle,
TOKEN_INFORMATION_CLASS TokenInformationClass,
LPVOID TokenInformation,
DWORD TokenInformationLength,
PDWORD ReturnLength
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
#define CreateServiceName "CreateServiceW"
#else
#define PCreateService  PCreateServiceA
#define CreateServiceName "CreateServiceA"
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
#define OpenSCManagerName "OpenSCManagerW"
#else
#define POpenSCManager  POpenSCManagerA
#define OpenSCManagerName "OpenSCManagerA"
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
#define OpenServiceName "OpenServiceW"
#else
#define POpenService  POpenServiceA
#define OpenServiceName "OpenServiceA"
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
#define StartServiceName "StartServiceW"
#else
#define PStartService  PStartServiceA
#define StartServiceName "StartServiceA"
#endif // !UNICODE

typedef
BOOL
(WINAPI *    PPathRemoveFileSpecA)(_Inout_ LPSTR pszPath);
typedef
BOOL
(WINAPI *    PPathRemoveFileSpecW)(_Inout_ LPWSTR pszPath);
#ifdef UNICODE
#define PPathRemoveFileSpec  PPathRemoveFileSpecW
#define PathRemoveFileSpecName "PathRemoveFileSpecW"
#else
#define PPathRemoveFileSpec  PPathRemoveFileSpecA
#define PathRemoveFileSpecName "PathRemoveFileSpecA"
#endif // !UNICODE
#define PPathRemoveFileSpecW  PPathRemoveFileSpecW
#define PPathRemoveFileSpecA  PPathRemoveFileSpecA
#define PathRemoveFileSpecNameW "PathRemoveFileSpecW"
#define PathRemoveFileSpecNameA "PathRemoveFileSpecA"

typedef
BOOL
(WINAPI*
PGetLogicalProcessorInformationEx)(
_In_ LOGICAL_PROCESSOR_RELATIONSHIP RelationshipType,
_Out_writes_bytes_to_opt_(*ReturnedLength, *ReturnedLength) PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX Buffer,
_Inout_ PDWORD ReturnedLength
);

// WDF 1.11 is "01011" + NULL.
#define MAX_VERSION_SIZE 6

#define KMDF_VERSION_MAJOR 1
#define KMDF_VERSION_MINOR 11

static const unsigned int AllocationTypeKnownFlags =
MEM_COMMIT | MEM_RESERVE | MEM_FREE | MEM_PRIVATE | MEM_MAPPED |
MEM_RESET | MEM_TOP_DOWN | MEM_WRITE_WATCH | MEM_PHYSICAL |
MEM_LARGE_PAGES | MEM_4MB_PAGES | MEM_IMAGE;

static CHAR g_coInstallerVersion[MAX_VERSION_SIZE] = { 0 };
static BOOLEAN  g_fLoop = FALSE;
static BOOL g_versionSpecified = FALSE;

static TIME_STAMP g_freq;
static bool g_isWow64;
static bool g_idDriverLoaded;

TIME_STAMP g_startingTime;

static HANDLE   g_hDevice;
static HMODULE  g_library = NULL;
static CHAR     g_driverLocation[MAX_PATH];
static PVOID    g_fileMapping = NULL;
static CHAR     g_systemDir[MAX_PATH];
static CHAR     g_systemWow64Dir[MAX_PATH];

static POpenProcessToken AdvApiOpenProcessToken = NULL;
static PGetTokenInformation AdvApiGetTokenInformation = NULL;
static PGetLogicalProcessorInformationEx Kernel32GetLogicalProcessorInformationEx = NULL;

static BYTE g_OutputDebugStringAOldCodeByte;
static BYTE g_OutputDebugStringWOldCodeByte;
static bool g_isOutputDebugStringSuppressed;
static INIT_FLAGS g_initFlags;
static std::vector<PORTABLE_PROCESSOR_PACKAGE> g_pks;

#define BINARY_CODE_RET 0xC3

extern "C"
{
	PCloseServiceHandle AdvApiCloseServiceHandle;
	PControlService AdvApiControlService;
	PCreateService AdvApiCreateService;
	PDeleteService AdvApiDeleteService;
	POpenSCManager AdvApiOpenSCManager;
	POpenService AdvApiOpenService;
	PStartService AdvApiStartService;
	PPathRemoveFileSpecW ShlwApiPathRemoveFileSpecW;
	PPathRemoveFileSpecA ShlwApiPathRemoveFileSpecA;
}

bool GetMainModuleFilePathA(char *fileName, int size)
{
	HMODULE hMainExe = GetModuleHandle(NULL);
	return GetModuleFileName(hMainExe, fileName, size) != 0;
}

bool IsPlatSupported()
{
	LARGE_INTEGER temp;
	bool supported = TRUE;
	supported = supported && QueryPerformanceCounter(&temp);
	supported = supported && QueryPerformanceFrequency(&temp);
	BOOL isWow64;
	supported = supported && IsWow64Process(GetCurrentProcess(), &isWow64);
	return supported && IsWindows7OrGreater();
}

void PortableInitBasic()
{
	LARGE_INTEGER temp;
	QueryPerformanceCounter(&temp);
	QueryPerformanceFrequency(&temp);
	g_freq = temp.QuadPart;
	BOOL isWow64;
	IsWow64Process(GetCurrentProcess(), &isWow64);
	g_isWow64 = isWow64 ? true : false;
}

void GetCurrentTimestamp(TIME_STAMP& timestamp)
{
	LARGE_INTEGER ts;
	QueryPerformanceCounter(&ts);
	timestamp = ts.QuadPart;
}

void GetCurrentTimestampFreq(TIME_STAMP& freq)
{
	LARGE_INTEGER tf;
	QueryPerformanceFrequency(&tf);
	freq = tf.QuadPart;
}

void GetTimeSpan(TIME_STAMP from, TIME_STAMP to, TIME_UNIT unit, TIME_STAMP& timeSpan)
{
	TIME_STAMP factor;
	switch (unit)
	{
	case TenthMicroseconds:
		factor = 10000000;
		break;
	case Microseconds:
		factor = 1000000;
		break;
	case Milliseconds:
		factor = 1000;
		break;
	case Seconds:
		factor = 1;
		break;
	default:
		factor = 0;
		break;
	}
	LARGE_INTEGER ts;
	ts.QuadPart = to - from;
	ts.QuadPart *= factor;
	ts.QuadPart /= g_freq;
	timeSpan = ts.QuadPart;
}

void GetTimeSpan(TIME_STAMP to, TIME_UNIT unit, TIME_STAMP& timeSpan)
{
	GetTimeSpan(g_startingTime, to, unit, timeSpan);
}

bool Is32BitPlat()
{
#if defined(TARGET_IA32)
	bool is32Bit = !g_isWow64;
#elif defined(TARGET_IA32E)
	bool is32Bit = FALSE;
#endif
	return is32Bit;
}

void SetStartingTimestamp()
{
	LARGE_INTEGER temp;
	QueryPerformanceCounter(&temp);
	g_startingTime = temp.QuadPart;
}

void GetStartingTimestamp(TIME_STAMP& timestamp)
{
	timestamp = g_startingTime;
}

PSEMAPHORE Semaphore_CreateUnnamed()
{
	return CreateSemaphore(NULL, 0, 0x7fffffff, NULL);
}

bool Semaphore_Increment(PSEMAPHORE sem, long inc, long *prevCount)
{
	return ReleaseSemaphore(sem, inc, prevCount) != 0;
}

bool Semaphore_Decrement(PSEMAPHORE sem)
{
	return WaitForSingleObject(sem, INFINITE) == WAIT_OBJECT_0;
}

bool Semaphore_Close(PSEMAPHORE sem)
{
	return CloseHandle(sem) != 0;
}

PMUTEX Mutex_CreateNamed(LPCTSTR name)
{
	return CreateMutex(NULL, FALSE, name);
}

bool Mutex_Acquire(PMUTEX mutex)
{
	return WaitForSingleObject(mutex, INFINITE) == WAIT_OBJECT_0;
}

bool Mutex_Release(PMUTEX mutex)
{
	return ReleaseMutex(mutex) != 0;
}

bool Mutex_Close(PMUTEX mutex)
{
	return CloseHandle(mutex) != 0;
}

PPROCESS GetCurrentProcessPortable()
{
	return GetCurrentProcess();
}

unsigned int GetProcessIdPortable(PPROCESS process)
{
	return GetProcessId(process);
}

static BOOL
ValidateCoinstallerVersion(
_In_ PSTR Version
)
{
	BOOL ok = FALSE;
	INT i;

	for (i = 0; i<MAX_VERSION_SIZE; i++){
		if (!IsCharAlphaNumericA(Version[i])) {
			break;
		}
	}
	if (i == (MAX_VERSION_SIZE - sizeof(CHAR))) {
		ok = TRUE;
	}
	return ok;
}

extern "C"
PCHAR
GetCoinstallerVersion(
VOID
)
{
	// Better to use the safe version of sprintf if available.
	if (sprintf(g_coInstallerVersion,
		//MAX_VERSION_SIZE,
		"%02d%03d",    // for example, "01011"
		KMDF_VERSION_MAJOR,
		KMDF_VERSION_MINOR) == -1)
	{
		//error
	}

	return (PCHAR)&g_coInstallerVersion;
}

bool SuppressOutputDebugString()
{
	HMODULE hk32dll;
	GetModuleHandleEx(0, "kernel32.dll", &hk32dll);
	if (!hk32dll)
		return false;
	BYTE *binCodeOutputDebugStringA = reinterpret_cast<BYTE*>(GetProcAddress(hk32dll, "OutputDebugStringA"));
	BYTE *binCodeOutputDebugStringW = reinterpret_cast<BYTE*>(GetProcAddress(hk32dll, "OutputDebugStringW"));
	if (!binCodeOutputDebugStringA || !binCodeOutputDebugStringW)
		return false;
	g_OutputDebugStringAOldCodeByte = *binCodeOutputDebugStringA;
	g_OutputDebugStringWOldCodeByte = *binCodeOutputDebugStringW;
	DWORD outputDebugStringAOldProtect;
	DWORD outputDebugStringWOldProtect;
	BOOL isOKA = VirtualProtect(binCodeOutputDebugStringA, 1, PAGE_EXECUTE_WRITECOPY, &outputDebugStringAOldProtect);
	BOOL isOKW = VirtualProtect(binCodeOutputDebugStringW, 1, PAGE_EXECUTE_WRITECOPY, &outputDebugStringWOldProtect);
	if (isOKA != 0 && isOKW != 0)
	{
		*binCodeOutputDebugStringA = BINARY_CODE_RET;
		*binCodeOutputDebugStringW = BINARY_CODE_RET;
		isOKA = VirtualProtect(binCodeOutputDebugStringA, 1, outputDebugStringAOldProtect, &outputDebugStringAOldProtect);
		isOKW = VirtualProtect(binCodeOutputDebugStringW, 1, outputDebugStringWOldProtect, &outputDebugStringWOldProtect);
	}
	return isOKA != 0 && isOKW != 0;
}

bool ConciliateOutputDebugString()
{
	HMODULE hk32dll;
	GetModuleHandleEx(0, "kernel32.dll", &hk32dll);
	if (!hk32dll)
		return false;
	BYTE *binCodeOutputDebugStringA = reinterpret_cast<BYTE*>(GetProcAddress(hk32dll, "OutputDebugStringA"));
	BYTE *binCodeOutputDebugStringW = reinterpret_cast<BYTE*>(GetProcAddress(hk32dll, "OutputDebugStringW"));
	if (!binCodeOutputDebugStringA || !binCodeOutputDebugStringW)
		return false;
	DWORD outputDebugStringAOldProtect;
	DWORD outputDebugStringWOldProtect;
	BOOL isOKA = VirtualProtect(binCodeOutputDebugStringA, 1, PAGE_EXECUTE_WRITECOPY, &outputDebugStringAOldProtect);
	BOOL isOKW = VirtualProtect(binCodeOutputDebugStringW, 1, PAGE_EXECUTE_WRITECOPY, &outputDebugStringWOldProtect);
	if (isOKA != 0 && isOKW != 0)
	{
		*binCodeOutputDebugStringA = g_OutputDebugStringAOldCodeByte;
		*binCodeOutputDebugStringW = g_OutputDebugStringWOldCodeByte;
		isOKA = VirtualProtect(binCodeOutputDebugStringA, 1, outputDebugStringAOldProtect, &outputDebugStringAOldProtect);
		isOKW = VirtualProtect(binCodeOutputDebugStringW, 1, outputDebugStringWOldProtect, &outputDebugStringWOldProtect);
	}
	return isOKA != 0 && isOKW != 0;
}

bool PortableInit(INIT_FLAGS flags)
{
	if (!ValidateInitFlags(flags))
		return false;

	if (GetSystemDirectory(g_systemDir, MAX_PATH) == 0)
		return false;

	if (GetSystemWow64Directory(g_systemWow64Dir, MAX_PATH) == 0)
		g_systemWow64Dir[0] = '\0';

	StringToUpper(g_systemDir);
	StringToUpper(g_systemWow64Dir);

	if (!IsDebuggerPresent() && 
		((flags & INIT_FLAGS_ODS_SUPPRESS) == INIT_FLAGS_ODS_SUPPRESS || 
		(flags & INIT_FLAGS_LOAD_DRIVER) == INIT_FLAGS_LOAD_DRIVER))
	{
		if (!SuppressOutputDebugString())
			return false;
		g_isOutputDebugStringSuppressed = true;
	}

	if ((flags & INIT_FLAGS_ELEVATION) == INIT_FLAGS_ELEVATION)
	{
		HINSTANCE hAdvapi32 = LoadLibrary("advapi32.dll");
		if (hAdvapi32)
		{
			AdvApiOpenProcessToken = (POpenProcessToken)GetProcAddress(hAdvapi32, "OpenProcessToken");
			AdvApiGetTokenInformation = (PGetTokenInformation)GetProcAddress(hAdvapi32, "GetTokenInformation");
		}

		if (!hAdvapi32 ||
			!AdvApiOpenProcessToken ||
			!AdvApiGetTokenInformation)
			return false;

		if ((flags & INIT_FLAGS_LOAD_DRIVER) == INIT_FLAGS_LOAD_DRIVER)
		{
			AdvApiCloseServiceHandle = (PCloseServiceHandle)GetProcAddress(hAdvapi32, "CloseServiceHandle");
			AdvApiControlService = (PControlService)GetProcAddress(hAdvapi32, "ControlService");
			AdvApiCreateService = (PCreateService)GetProcAddress(hAdvapi32, CreateServiceName);
			AdvApiDeleteService = (PDeleteService)GetProcAddress(hAdvapi32, "DeleteService");
			AdvApiOpenSCManager = (POpenSCManager)GetProcAddress(hAdvapi32, OpenSCManagerName);
			AdvApiOpenService = (POpenService)GetProcAddress(hAdvapi32, OpenServiceName);
			AdvApiStartService = (PStartService)GetProcAddress(hAdvapi32, StartServiceName);

			HINSTANCE hShlwapi = LoadLibrary("shlwapi.dll");
			if (hShlwapi)
			{
				ShlwApiPathRemoveFileSpecA = (PPathRemoveFileSpecA)GetProcAddress(hShlwapi, PathRemoveFileSpecNameA);
				ShlwApiPathRemoveFileSpecW = (PPathRemoveFileSpecW)GetProcAddress(hShlwapi, PathRemoveFileSpecNameW);
			}

			if (!AdvApiCloseServiceHandle ||
				!AdvApiControlService ||
				!AdvApiCreateService ||
				!AdvApiDeleteService ||
				!AdvApiOpenSCManager ||
				!AdvApiOpenService ||
				!AdvApiStartService ||
				!hShlwapi ||
				!ShlwApiPathRemoveFileSpecA ||
				!ShlwApiPathRemoveFileSpecW)
				return false;
		}
	}

	g_initFlags = flags;
	g_idDriverLoaded = false; // Set by LoadDriver().

	return true;
}

bool LoadDriver()
{
	DWORD    errNum = 0;
	BOOL     ok;
	PCHAR    coinstallerVersion;
	if (!g_versionSpecified) {
		coinstallerVersion = GetCoinstallerVersion();
	}
	else {
		coinstallerVersion = (PCHAR)&g_coInstallerVersion;
	}
	g_hDevice = CreateFile(DEVICE_NAME,
		GENERIC_READ | GENERIC_WRITE,
		FILE_SHARE_READ | FILE_SHARE_WRITE,
		NULL,
		CREATE_NEW,
		FILE_ATTRIBUTE_NORMAL,
		NULL);
	
	if (g_hDevice == INVALID_HANDLE_VALUE) {

		errNum = GetLastError();

		if (!(errNum == ERROR_FILE_NOT_FOUND ||
			errNum == ERROR_PATH_NOT_FOUND)) {
			return false;
		}

		g_library = LoadWdfCoInstaller();

		if (g_library == NULL) {
			return false;
		}

		ok = SetupDriverName(g_driverLocation, MAX_PATH);

		if (!ok) {
			return false;
		}

		ok = ManageDriver(DRIVER_NAME,
			g_driverLocation,
			DRIVER_FUNC_INSTALL);

		if (!ok) {
			ManageDriver(DRIVER_NAME,
				g_driverLocation,
				DRIVER_FUNC_REMOVE);
			return false;
		}

		g_hDevice = CreateFile(DEVICE_NAME,
			GENERIC_READ | GENERIC_WRITE,
			0,
			NULL,
			CREATE_ALWAYS,
			FILE_ATTRIBUTE_NORMAL,
			NULL);

		if (g_hDevice == INVALID_HANDLE_VALUE) {
			return false;
		}
	}

	if (g_isOutputDebugStringSuppressed &&
		(g_initFlags & INIT_FLAGS_ODS_AS_REQUIRED) == INIT_FLAGS_ODS_AS_REQUIRED)
	{
		ConciliateOutputDebugString();
		g_isOutputDebugStringSuppressed = false;
	}
	g_idDriverLoaded = true;
	return true;
}

void PortableDispose()
{
	if (g_idDriverLoaded)
	{
		CloseHandle(g_hDevice);
		if (g_library) {
			ManageDriver(DRIVER_NAME,
				g_driverLocation,
				DRIVER_FUNC_REMOVE);
			UnloadWdfCoInstaller(g_library);
		}
	}

	if (g_isOutputDebugStringSuppressed)
	{
		ConciliateOutputDebugString();
	}
}

unsigned long Virtual2Physical(void *vaddrs[], unsigned long long paddrs[], unsigned int size)
{
	ULONG addrsConverted;

	BOOL bRc = DeviceIoControl(g_hDevice,
	(DWORD) IOCTL_ALLERIA_V2P,
	vaddrs,
	size * sizeof(void*),
	paddrs,
	size * sizeof(unsigned long long),
	&addrsConverted,
	NULL
	);

	return bRc != 0 ? addrsConverted : 0;
}

bool IsElevated() {
	BOOL fRet = FALSE;
	HANDLE hToken = NULL;
	if (AdvApiOpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken)) {
		TOKEN_ELEVATION Elevation;
		DWORD cbSize = sizeof(TOKEN_ELEVATION);
		if (AdvApiGetTokenInformation(hToken, TokenElevation, &Elevation, sizeof(Elevation), &cbSize)) {
			fRet = Elevation.TokenIsElevated;
		}
	}
	if (hToken) {
		CloseHandle(hToken);
	}
	return fRet != 0;
}

bool PinVirtualPage(const void *addr)
{
	MEMORY_BASIC_INFORMATION mbi;
	if (VirtualQuery(addr, &mbi, sizeof(mbi)) == sizeof(mbi) && mbi.State == MEM_COMMIT)
	{
		// Pin one page.
		return VirtualLock(mbi.BaseAddress, 1) != 0;
	}
	return false;
}

bool UnpinVirtualPage(const void *addr)
{
	MEMORY_BASIC_INFORMATION mbi;
	if (VirtualQuery(addr, &mbi, sizeof(mbi)) == sizeof(mbi))
	{
		return VirtualUnlock(mbi.BaseAddress, 1) != 0;
	}
	return false;
}

bool PrepareForPinningVirtualPages()
{
	SYSTEM_INFO sysInfo;
	GetSystemInfo(&sysInfo);
	return SetProcessWorkingSetSizeEx(
		GetCurrentProcess(),
		sysInfo.dwPageSize * (1 << 12),
		sysInfo.dwPageSize * (1 << 18),
		QUOTA_LIMITS_HARDWS_MIN_DISABLE | QUOTA_LIMITS_HARDWS_MAX_DISABLE) != 0;
}

std::string GetLastErrorString(UINT32 errCode)
{
	LPSTR messageBuffer = NULL;
	size_t size = FormatMessageA(
	FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL,
	errCode, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&messageBuffer, 0, NULL);
	std::string message(messageBuffer, size);
	LocalFree(messageBuffer);
	return message;
}

void GetAddressSizes(unsigned char& vAddressSize, unsigned char& pAddressSize)
{
#ifdef TARGET_IA32
	vAddressSize = 4; // 4 bytes.
#else
	vAddressSize = 6; // 6 bytes.
#endif /* TARGET_IA32 */
	MEMORYSTATUSEX statex;
	statex.dwLength = sizeof(statex);
	if (GlobalMemoryStatusEx(&statex) != 0)
	{
		pAddressSize = 2;
		DWORDLONG mask = 0xFF0000;
		while ((statex.ullTotalPhys & mask) != 0)
		{
			pAddressSize += 1;
			mask <<= 8;
		}
	}
	else
	{
		pAddressSize = 8; // The maximum.
	}
}

void GetSystemInfo(BINARY_HEADER& header)
{
	SYSTEM_INFO sysInfo;
	GetSystemInfo(&sysInfo);
	header.pageSize = sysInfo.dwPageSize;
	header.minAppAddr = sysInfo.lpMinimumApplicationAddress;
	header.maxAppAddr = sysInfo.lpMaximumApplicationAddress;

	MEMORYSTATUSEX statex;
	statex.dwLength = sizeof(statex);
	GlobalMemoryStatusEx(&statex);
	header.totalDram = (unsigned int)(statex.ullTotalPhys / (1024 * 1024));

	header.processOsId = GetCurrentProcessId();
	header.processorArch = Is32BitPlat() ? PROCESSOR_ARCH_X86 : PROCESSOR_ARCH_X64;

	header.os = BINARY_HEADER_OS_WINDOWS;

	GetAddressSizes(header.vAddrSize, header.pAddrSize); 

	UINT sysDirSize = GetWindowsDirectoryA(NULL, 0);
	header.systemDirectory = new char[sysDirSize];
	GetWindowsDirectoryA(header.systemDirectory, sysDirSize);

	UINT currentDirSize = GetCurrentDirectoryA(NULL, 0);
	header.currentDirectory = new char[currentDirSize];
	GetCurrentDirectoryA(currentDirSize, header.currentDirectory);

	header.mainExePath = new char[MAX_PATH];
	GetModuleFileNameA(NULL, header.mainExePath, MAX_PATH);

	header.cmdLine = GetCommandLineA();

	LARGE_INTEGER temp;
	QueryPerformanceFrequency(&temp);
	header.timeFreq = temp.QuadPart;

	std::string version;
#if defined(NTDDI_WIN10)
	if (IsWindows10OrGreater())
	{
		version = "10.0";
	}
	else
#endif
#if defined(NTDDI_WINBLUE)
	if (IsWindows8Point1OrGreater())
	{
		version = "8.1";
	}
	else 
#endif
#if defined(NTDDI_WIN8)
	if(IsWindows8OrGreater())
	{
		version = "8.0";
	}
	else
#endif
	if (IsWindows7SP1OrGreater())
	{
		version = "7.1";
	}
	else // IsWindows7OrGreater() == TRUE
	{
		version = "7.0";
	}

	if (IsWindowsServer())
	{
		version += "s";
	}

	char *osVersion = new char[version.length() + 1];
	memcpy(osVersion, version.c_str(), version.length());
	osVersion[version.length()] = '\0';
	header.osVersion = osVersion;
}

unsigned int GetVirtualPageSize()
{
	SYSTEM_INFO sysInfo;
	GetSystemInfo(&sysInfo);
	return sysInfo.dwPageSize;
}

void* CreateNamedFileMapping(unsigned int maxSize, const char *name, bool *alreadyExists)
{
	SECURITY_ATTRIBUTES sa;
	sa.nLength = sizeof(sa);
	sa.lpSecurityDescriptor = NULL;

	// Make the returned handle inheritable.
	// This important to make sure that the PPNDB is accessible to any spawned processes
	// even if this process terminated.
	// However, this only works if the child was created with handles inheritance enabled.
	sa.bInheritHandle = TRUE;

	HANDLE hFileMapping = CreateFileMappingA(
		NULL,
		&sa,
		PAGE_READWRITE | SEC_RESERVE,
		0,
		maxSize,
		name);

	if (!hFileMapping)
	{
		return NULL;
	}

	if (GetLastError() == ERROR_ALREADY_EXISTS)
	{
		if (alreadyExists)
			*alreadyExists = true;

		if (g_fileMapping)
			return g_fileMapping;
	}

	PVOID base = MapViewOfFile(
		hFileMapping,
		FILE_MAP_ALL_ACCESS,
		0,
		0,
		0);

	if (!base)
	{
		return NULL;
	}

	if (alreadyExists)
		*alreadyExists = false;

	g_fileMapping = base;

	/*

	Don't close the file-mapping handle. Otheriwse, the PPNDB will be lost. If this process terminates, and
	it didn't spawn any processes, the OS will automatically reclaim the file-mapping object.

	*/

	return base;
}

bool DirectoryExists(const char *directory)
{
	DWORD attributes = GetFileAttributes(directory);
	return (attributes != INVALID_FILE_ATTRIBUTES) && (attributes == FILE_ATTRIBUTE_DIRECTORY);
}

bool EnsureCommited(void *address, unsigned int sizeInBytes)
{
	MEMORY_BASIC_INFORMATION mbi;
	BOOL bOk = (VirtualQuery(address, &mbi, sizeof(mbi))
		== sizeof(mbi));
	if (!bOk)
	{
		return false;
	}
	switch (mbi.State)
	{
	case MEM_COMMIT:
		// Already commited.
		return true;
	case MEM_RESERVE:
		if (VirtualAlloc(address, sizeInBytes,
			MEM_COMMIT, PAGE_READWRITE) == NULL)
		{
			return false;
		}
		else
		{
			// Commited successfully.
			return true;
		}
	case MEM_FREE:
	default:
		break;
	}
	return false;
}

bool PortableCreateDirectory(const char *directory)
{
	return CreateDirectoryA(directory, NULL) != 0;
}

std::string PathToDirectory(const std::string& path)
{
	// Using PathRemoveFileSpecA forces Alleria to load ShlwApi.
	// Avoid this by performing the processing manually.
	const std::string& directory = path.substr(0, path.find_last_of(PORTABLE_PATH_DELIMITER) + 1);
	return directory;
}

bool EnumerateLoadedImages()
{
	HANDLE hModuleSnap = (HANDLE) - 1;
	MODULEENTRY32 me32;

	//  Take a snapshot of all modules in the specified process. 
	hModuleSnap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, GetCurrentProcessId());
	if (hModuleSnap == (HANDLE) - 1)
	{
		return FALSE;
	}

	//  Set the size of the structure before using it. 
	me32.dwSize = sizeof(MODULEENTRY32);

	//  Retrieve information about the first module, 
	//  and exit if unsuccessful 
	if (!Module32First(hModuleSnap, &me32))
	{
		CloseHandle(hModuleSnap);     // Must clean up the snapshot object! 
		return FALSE;
	}

	//  Now walk the module list of the process, 
	//  and display information about each module 
	do
	{
	} while (Module32Next(hModuleSnap, &me32));


	//  Do not forget to clean up the snapshot object. 
	CloseHandle(hModuleSnap);
	return TRUE;
}

const char* SystemStatusString(UINT32 status)
{
	const char *output = "";
	switch (status)
	{
	case STATUS_SUCCESS:                 output = "SUCCESS"; break;
	case STATUS_ACCESS_DENIED:           output = "ACCESS_DENIED"; break;
	case STATUS_ALREADY_COMMITTED:       output = "ALREADY_COMMITTED"; break;
	case STATUS_COMMITMENT_LIMIT:        output = "COMMITMENT_LIMIT"; break;
	case STATUS_CONFLICTING_ADDRESSES:   output = "CONFLICTING_ADDRESSES"; break;
	case STATUS_INSUFFICIENT_RESOURCES:  output = "INSUFFICIENT_RESOURCES"; break;
	case STATUS_INVALID_HANDLE:          output = "INVALID_HANDLE"; break;
	case STATUS_INVALID_PAGE_PROTECTION: output = "INVALID_PAGE_PROTECTION"; break;
	case STATUS_NO_MEMORY:               output = "NO_MEMORY"; break;
	case STATUS_OBJECT_TYPE_MISMATCH:    output = "OBJECT_TYPE_MISMATCH"; break;
	case STATUS_PROCESS_IS_TERMINATING:  output = "PROCESS_IS_TERMINATING"; break;
	default:                             output = "UNKOWN_NT_STATUS"; break;
	}
	return output;
}

unsigned int GetParentProcessId()
{
	HMODULE hNtdll;
	GetModuleHandleEx(0, "ntdll.dll", &hNtdll);
	BYTE buffer[sizeof(PROCESS_BASIC_INFORMATION)];
	ULONG returnSize = 0;
	NtQueryInformationProcess ntqip;
	*(FARPROC *)&ntqip = GetProcAddress(hNtdll, "NtQueryInformationProcess");
	LONG status = ntqip(
		GetCurrentProcess(),
		0, // PROCESS_BASIC_INFORMATION
		buffer,
		sizeof(PROCESS_BASIC_INFORMATION),
		&returnSize);
	PROCESS_BASIC_INFORMATION *pbi = reinterpret_cast<PROCESS_BASIC_INFORMATION*>(buffer);
	if (status == STATUS_SUCCESS && returnSize == sizeof(PROCESS_BASIC_INFORMATION))
	{
#pragma DISABLE_COMPILER_WARNING_PTRTRUNC_LOSS
		return reinterpret_cast<UINT32>(pbi->Reserved3);
#pragma RESTORE_COMPILER_WARNING_PTRTRUNC_LOSS
	}
	return PROCESS_INVALID_ID; // The return value should not be used if there was an error.
}

std::string MemoryAllocationFlagsToString(unsigned int allocationFlags)
{
	std::string output = "";
	UINT32 allocType = allocationFlags;
	BOOL first = TRUE;
	allocType &= AllocationTypeKnownFlags;
	while (allocType)
	{
		if (!first)
			output += " | ";

		if (allocType & MEM_COMMIT)
		{
			output += "COMMIT";
			allocType &= ~MEM_COMMIT;
		}

		else if (allocType & MEM_RESERVE)
		{
			output += "RESERVE";
			allocType &= ~MEM_RESERVE;
		}

		else if (allocType & MEM_FREE)
		{
			output += "FREE";
			allocType &= ~MEM_FREE;
		}

		else if (allocType & MEM_PRIVATE)
		{
			output += "PRIVATE";
			allocType &= ~MEM_PRIVATE;
		}

		else if (allocType & MEM_MAPPED)
		{
			output += "MAPPED";
			allocType &= ~MEM_MAPPED;
		}

		else if (allocType & MEM_RESET)
		{
			output += "MEM_RESET";
			allocType &= ~MEM_RESET;
		}

		else if (allocType & MEM_TOP_DOWN)
		{
			output += "TOP_DOWN";
			allocType &= ~MEM_TOP_DOWN;
		}

		else if (allocType & MEM_WRITE_WATCH)
		{
			output += "WRITE_WATCH";
			allocType &= ~MEM_WRITE_WATCH;
		}

		else if (allocType & MEM_PHYSICAL)
		{
			output += "PHYSICAL";
			allocType &= ~MEM_PHYSICAL;
		}

		else if (allocType & MEM_LARGE_PAGES)
		{
			output += "LARGE_PAGES";
			allocType &= ~MEM_LARGE_PAGES;
		}

		else if (allocType & MEM_4MB_PAGES)
		{
			output += "4MB_PAGES";
			allocType &= ~MEM_4MB_PAGES;
		}

		else
		{
			// MEM_IMAGE
			output += "IMAGE";
			allocType &= ~MEM_IMAGE;
		}

		first = FALSE;
	}

	if (allocationFlags & ~AllocationTypeKnownFlags)
	{
		std::string temp;
		IntegerToString(allocationFlags & ~AllocationTypeKnownFlags, temp);
		output += (first ? "" : " | ") + temp;
	}
	return output;
}

std::string MemoryProtectionFlagstoString(unsigned int protectionFlags)
{
	std::string output = "";

	switch (protectionFlags & ~(PAGE_GUARD | PAGE_NOCACHE | PAGE_WRITECOMBINE))
	{
	case PAGE_READONLY:          output = "READONLY"; break;
	case PAGE_READWRITE:         output = "READWRITE"; break;
	case PAGE_WRITECOPY:         output = "WRITECOPY"; break;
	case PAGE_EXECUTE:           output = "EXECUTE"; break;
	case PAGE_EXECUTE_READ:      output = "EXECUTE_READ"; break;
	case PAGE_EXECUTE_READWRITE: output = "EXECUTE_READWRITE"; break;
	case PAGE_EXECUTE_WRITECOPY: output = "EXECUTE_WRITECOPY"; break;
	case PAGE_NOACCESS:          output = "NOACCESS"; break;
	default:                     IntegerToString(protectionFlags, output); break;
	}

	protectionFlags &= (PAGE_GUARD | PAGE_NOCACHE | PAGE_WRITECOMBINE);
	while (protectionFlags)
	{
		output += " | ";

		if (protectionFlags & PAGE_GUARD)
		{
			output += "GUARD";
			protectionFlags &= ~PAGE_GUARD;
		}

		else if (protectionFlags & PAGE_NOCACHE)
		{
			output += "NOCACHE";
			protectionFlags &= ~PAGE_NOCACHE;
		}

		else if (protectionFlags & PAGE_WRITECOMBINE)
		{
			output += "WRITECOMBINE";
			protectionFlags &= ~PAGE_WRITECOMBINE;
		}

	}
	return output;
}

std::string MemoryFreeFlagsString(unsigned int freeFlags)
{
	std::string output = "";

	switch (freeFlags)
	{
	case MEM_DECOMMIT:  output = "DECOMMIT"; break;
	case MEM_RELEASE:   output = "RELEASE"; break;
	default:            IntegerToString(freeFlags, output); break;
	}

	return output;
}

#define HEAP_FLAG_CONDITIONAL_APPEND(heapFlags, flag, first, output)\
if (heapFlags & flag)\
{\
	if (!first)\
		output += " | ";\
	output += #flag;\
	first = FALSE;\
	heapFlags &= ~flag;\
}

std::string HeapFlagsString(unsigned int heapFlags)
{
	if (heapFlags == 0)
		return "0";

	std::string output = "";
	BOOL first = TRUE;

	HEAP_FLAG_CONDITIONAL_APPEND(heapFlags, HEAP_NO_SERIALIZE, first, output);
	HEAP_FLAG_CONDITIONAL_APPEND(heapFlags, HEAP_GROWABLE, first, output);
	HEAP_FLAG_CONDITIONAL_APPEND(heapFlags, HEAP_GENERATE_EXCEPTIONS, first, output);
	HEAP_FLAG_CONDITIONAL_APPEND(heapFlags, HEAP_ZERO_MEMORY, first, output);
	HEAP_FLAG_CONDITIONAL_APPEND(heapFlags, HEAP_REALLOC_IN_PLACE_ONLY, first, output);
	HEAP_FLAG_CONDITIONAL_APPEND(heapFlags, HEAP_TAIL_CHECKING_ENABLED, first, output);
	HEAP_FLAG_CONDITIONAL_APPEND(heapFlags, HEAP_FREE_CHECKING_ENABLED, first, output);
	HEAP_FLAG_CONDITIONAL_APPEND(heapFlags, HEAP_DISABLE_COALESCE_ON_FREE, first, output);
	HEAP_FLAG_CONDITIONAL_APPEND(heapFlags, HEAP_CREATE_ALIGN_16, first, output);
	HEAP_FLAG_CONDITIONAL_APPEND(heapFlags, HEAP_CREATE_ENABLE_TRACING, first, output);
	HEAP_FLAG_CONDITIONAL_APPEND(heapFlags, HEAP_CREATE_ENABLE_EXECUTE, first, output);

	//0x800000 0x280000 0x180000

	if (heapFlags)
	{
		if (!first)
			output += " | ";

		std::string temp;
		IntegerToString(heapFlags, temp);
		output += temp;
	}

	return output;
}

void *GetProcessSystemInfoAddr()
{
	HMODULE hNtdll;
	GetModuleHandleEx(0, "ntdll.dll", &hNtdll);
	BYTE buffer[sizeof(PROCESS_BASIC_INFORMATION)];
	ULONG returnSize = 0;
	NtQueryInformationProcess ntqip;
	*(FARPROC *)&ntqip = GetProcAddress(hNtdll, "NtQueryInformationProcess");
	NTSTATUS status = ntqip(
		GetCurrentProcess(),
		0, // PROCESS_BASIC_INFORMATION
		buffer,
		sizeof(PROCESS_BASIC_INFORMATION),
		&returnSize);
	PROCESS_BASIC_INFORMATION *pbi = reinterpret_cast<PROCESS_BASIC_INFORMATION*>(buffer);
	if (status == STATUS_SUCCESS && returnSize == sizeof(PROCESS_BASIC_INFORMATION))
	{
		return pbi->PebBaseAddress;
	}
	else
	{
		return NULL;
	}
}

void GetProcessDefaultHeapAddr(void **addr, void **heapId)
{
	if (heapId != NULL)
		*heapId = GetProcessHeap();
	if (addr != NULL)
	{
		*addr = HeapAlloc(GetProcessHeap(), 0, 1);
		HeapFree(*heapId, 0, *addr);
	}
}

void *GetThreadSystemInfoAddr()
{
	PNT_TIB pTib = reinterpret_cast<PNT_TIB>(NtCurrentTeb());
	return pTib;
}

void PathToName(const std::string& path, std::string& name)
{
	name = path.substr(path.find_last_of(PORTABLE_PATH_DELIMITER) + 1).c_str();
}

static void CreateProcessorPackage(DWORD packageNumber, WORD group, KAFFINITY mask, PORTABLE_PROCESSOR_PACKAGE& pk)
{
#ifdef TARGET_IA32
	// Ignore group. It must always be zero on 32-bit Windows.
	pk.reserved[0] = mask;
	pk.reserved[1] = 0;
	pk.reserved[2] = 0;
	pk.reserved[3] = packageNumber;
#elif defined(TARGET_IA32E)
	pk.reserved[0] = (unsigned int)mask;
	pk.reserved[1] = (unsigned int)(mask >> 32);
	pk.reserved[2] = group;
	pk.reserved[3] = packageNumber;
#endif
}

static KAFFINITY GetAffinity(const PORTABLE_PROCESSOR_PACKAGE& pk)
{
#ifdef TARGET_IA32
	return pk.reserved[0];
#elif defined(TARGET_IA32E)
	KAFFINITY result = (((KAFFINITY)pk.reserved[1]) << 32) | pk.reserved[0];
	return result;
#endif
}

static WORD GetGroup(const PORTABLE_PROCESSOR_PACKAGE& pk)
{
	return (WORD)pk.reserved[2];
}

static DWORD GetProcessorPackageNumber(const PORTABLE_PROCESSOR_PACKAGE& pk)
{
	return pk.reserved[3];
}

#if defined(TARGET_IA32E)
static bool InitGetProcessorPackages()
{
	HINSTANCE hKernel32 = GetModuleHandleA("API-MS-WIN-CORE-SYSINFO-L1-2-1.DLL"); // Windows 8.1 and 10.
	if (!hKernel32)
		hKernel32 = GetModuleHandleA("API-MS-WIN-CORE-SYSINFO-L1-2-0.DLL"); // Windows 8.
	if (!hKernel32)
		hKernel32 = GetModuleHandleA("API-MS-WIN-CORE-SYSINFO-L1-1-0.DLL"); // Windows 7.

	if (hKernel32)
		Kernel32GetLogicalProcessorInformationEx = (PGetLogicalProcessorInformationEx)GetProcAddress(hKernel32, "GetLogicalProcessorInformationEx");

	if (!hKernel32 ||
		!Kernel32GetLogicalProcessorInformationEx)
		return false;

	return true;
}
#endif /* TARGET_IA32E */

bool GetProcessorPackages(std::vector<PORTABLE_PROCESSOR_PACKAGE>& processors)
{
#ifdef TARGET_IA32
	BOOL done = FALSE;
	PSYSTEM_LOGICAL_PROCESSOR_INFORMATION buffer = NULL;
	PSYSTEM_LOGICAL_PROCESSOR_INFORMATION ptr = NULL;
	DWORD returnLength = 0;
	DWORD byteOffset = 0;
	while (!done)
	{
		DWORD rc = GetLogicalProcessorInformation(buffer, &returnLength);

		if (FALSE == rc)
		{
			if (GetLastError() == ERROR_INSUFFICIENT_BUFFER)
			{
				if (buffer)
					free(buffer);

				buffer = (PSYSTEM_LOGICAL_PROCESSOR_INFORMATION)malloc(
					returnLength);

				if (NULL == buffer) 
				{
					return false;
				}
			}
			else
			{
				return false;
			}
		}
		else
		{
			done = TRUE;
		}
	}

	ptr = buffer;
	PORTABLE_PROCESSOR_PACKAGE pk;
	while (byteOffset + sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION) <= returnLength)
	{
		switch (ptr->Relationship)
		{
		

		case RelationProcessorPackage:
			// Logical processors share a physical package.
			CreateProcessorPackage(processors.size() + 1, 0, ptr->ProcessorMask, pk);
			processors.push_back(pk);
			break;

		}
		byteOffset += sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION);
		ptr++;
	}
	free(buffer);
	if (g_pks.empty())
		g_pks.insert(g_pks.begin(), processors.begin(), processors.end());
	return true;
#elif defined(TARGET_IA32E)

	if (!Kernel32GetLogicalProcessorInformationEx)
		InitGetProcessorPackages();

	BOOL done = FALSE;
	DWORD returnLength = 0;
	PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX buffer = NULL;
	PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX ptr = NULL;

	while (!done)
	{
		DWORD rc = Kernel32GetLogicalProcessorInformationEx(
			LOGICAL_PROCESSOR_RELATIONSHIP::RelationProcessorPackage, buffer, &returnLength);

		if (FALSE == rc)
		{
			if (GetLastError() == ERROR_INSUFFICIENT_BUFFER)
			{
				if (buffer)
					free(buffer);

				buffer = (PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX)malloc(
					returnLength);

				if (NULL == buffer)
				{
					return false;
				}
			}
			else
			{
				return false;
			}
		}
		else
		{
			done = TRUE;
		}
	}

	DWORD processorPackageNumber = 0;
	PORTABLE_PROCESSOR_PACKAGE pk;
	ptr = buffer;
	while (returnLength)
	{
		// Add the processor package to the list.
		// The package might be spread accross more than one group.
		// The number of groups is in GroupCount and the masks are in GroupMask[GroupIndex].Mask.
		++processorPackageNumber;
		for (unsigned int i = 0; i<ptr->Processor.GroupCount; ++i)
		{
			CreateProcessorPackage(
				processorPackageNumber, 
				ptr->Processor.GroupMask[i].Group,
				ptr->Processor.GroupMask[i].Mask,
				pk);
			processors.push_back(pk);
		}

		returnLength -= ptr->Size;
		ptr = reinterpret_cast<SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX*>(
			reinterpret_cast<BYTE*>(ptr)+ptr->Size);
	}
	free(buffer);
	if (g_pks.empty())
		g_pks.insert(g_pks.begin(), processors.begin(), processors.end());
	return true;
#endif /* TARGET_IA32 */
}

static BYTE CountOnes(KAFFINITY bits)
{
	BYTE count = 0;
	while (bits > 0)
	{
		++count;
		bits = bits & (bits - 1);
	}
	return count;
}

bool GetMyProcessorNumber(unsigned int& package, unsigned int& number)
{
	return GetMyProcessorNumber(g_pks, package, number);
}

bool GetMyProcessorNumber(const std::vector<PORTABLE_PROCESSOR_PACKAGE>& pks, unsigned int& package, unsigned int& number)
{
#ifdef TARGET_IA32
	DWORD pn = GetCurrentProcessorNumber(); // The lowest processor number is zero.
	DWORD mask = 1 << pn;
	DWORD pkMask = 0;

	// Look for the processor package that has pn in its logical processors mask.
	for (size_t i = 0; i < pks.size(); ++i)
	{
		pkMask = GetAffinity(pks[i]);
		if ((pkMask & mask) != 0)
		{
			pkMask = pkMask & (2 * mask - 1);
			number = CountOnes(pkMask); // Count number of set bits.
			package = GetProcessorPackageNumber(pks[i]);
			return true;
		}
	}

	// Unkown processor number;
	package = 0;
	number = 0;
	return false;

#elif defined(TARGET_IA32E)
	PROCESSOR_NUMBER pn;
	GetCurrentProcessorNumberEx(&pn);

	// Look for the processor package that has some logical processors in the
	// the group specified by pn.Group and has pn.Number in its logical processors mask.

	KAFFINITY mask = 1ll << pn.Number;
	WORD group = pn.Group;
	KAFFINITY pkMask = 0;
	WORD pkGroup = 0;

	// Look for the processor package that has pn in its logical processors mask.
	for (size_t i = 0; i < pks.size(); ++i)
	{
		pkGroup = GetGroup(pks[i]);
		pkMask = GetAffinity(pks[i]);
		if (pkGroup == group && (pkMask & mask) != 0)
		{
			pkMask = pkMask & (2 * mask - 1);
			number = CountOnes(pkMask); // Count number of set bits.
			package = GetProcessorPackageNumber(pks[i]);
			return true;
		}
	}

	// Unkown processor number;
	package = 0;
	number = 0;
	return false;

#endif /* TARGET_IA32 */
}

bool SetMyThreadAffinity(const PORTABLE_PROCESSOR_PACKAGE& pk)
{
	/*
	
	A thread affinity mask is a bit vector in which each bit represents a logical processor 
	that a thread is allowed to run on. A thread affinity mask must be a subset of the process 
	affinity mask for the containing process of a thread. A thread can only run on the processors 
	its process can run on. Therefore, the thread affinity mask cannot specify a 1 bit for a 
	processor when the process affinity mask specifies a 0 bit for that processor.
	
	*/
#ifdef TARGET_IA32

	// There is no such as a thing as processor groups.
	// The top 16-bits are always zeroes.
	DWORD_PTR affinityMask = GetAffinity(pk);
	return SetThreadAffinityMask(GetCurrentThread(), affinityMask) != 0;

#elif defined(TARGET_IA32E)

	GROUP_AFFINITY groupAffinity;
	groupAffinity.Mask = GetAffinity(pk);
	groupAffinity.Group = GetGroup(pk);

	return SetThreadGroupAffinity(GetCurrentThread(), &groupAffinity, NULL) != 0;

#endif /* TARGET_IA32 */

}

void GetMyThreadAffinity(PORTABLE_PROCESSOR_PACKAGE& pk)
{
#ifdef TARGET_IA32
	DWORD mask = 1;
	DWORD old = 0;

	// Try every CPU one by one until one works or none are left.
	while (mask)
	{
		old = SetThreadAffinityMask(GetCurrentThread(), mask);
		if (old)
		{   // This one worked.
			SetThreadAffinityMask(GetCurrentThread(), old); // Restore original.
			CreateProcessorPackage(0, 0, old, pk);
			return;
		}
		else
		{
			if (GetLastError() != ERROR_INVALID_PARAMETER)
			{
				// This case should never happen.
				pk = { { 0 } };
				return;
			}
		}
		mask <<= 1;
	}

	// Never reaches this code.
	pk = { { 0 } };
	return;
#elif defined(TARGET_IA32E)
	GROUP_AFFINITY groupAffinity;
	GetThreadGroupAffinity(GetCurrentThread(), &groupAffinity);
	CreateProcessorPackage(0, groupAffinity.Group, groupAffinity.Mask, pk);
#endif /* TARGET_IA32 */
}

bool IsSystemFile(const char *filePath)
{
	char *filePathTemp = (char*)malloc(strlen(filePath) + 1);
	strcpy(filePathTemp, filePath);
	StringToUpper(filePathTemp);
	bool result = (strstr(filePathTemp, g_systemDir) != NULL || strstr(filePathTemp, g_systemWow64Dir) != NULL);
	free(filePathTemp);
	return result;
}

bool IsRuntimeFile(const char *filePath)
{
	return false;
}

bool IsAppFile(const char *filePath)
{
	return !IsSystemFile(filePath) && !IsRuntimeFile(filePath);
}

void IntrinsicCpuid(int cpuInfo[4], int fid, int subfid)
{
	__cpuidex(cpuInfo, fid, subfid);
}

unsigned long long IntrinsicXgetbv(unsigned int xcr)
{
	return _xgetbv(xcr);
}

/*
#define GetWmiPropertyStrValue(pclsObj, vtProp, obj, prop) \
	pclsObj->Get(L#prop, 0, &vtProp, 0, 0); \
	obj.##prop = vtProp.bstrVal; \
	OleAut32VariantClear(&vtProp);

#define GetWmiPropertyUshortValue(pclsObj, vtProp, obj, prop) \
	pclsObj->Get(L#prop, 0, &vtProp, 0, 0); \
	obj.##prop = vtProp.uiVal; \
	OleAut32VariantClear(&vtProp);

#define GetWmiPropertyUlongValue(pclsObj, vtProp, obj, prop) \
	pclsObj->Get(L#prop, 0, &vtProp, 0, 0); \
	obj.##prop = vtProp.ulVal; \
	OleAut32VariantClear(&vtProp);

#define GetWmiPropertyUlonglongValue(pclsObj, vtProp, obj, prop) \
	pclsObj->Get(L#prop, 0, &vtProp, 0, 0); \
	obj.##prop = vtProp.ullVal; \
	OleAut32VariantClear(&vtProp);

#define GetWmiPropertyCharValue(pclsObj, vtProp, obj, prop) \
	pclsObj->Get(L#prop, 0, &vtProp, 0, 0); \
	obj.##prop = vtProp.cVal; \
	OleAut32VariantClear(&vtProp);

#define GetWmiPropertyShortValue(pclsObj, vtProp, obj, prop) \
	pclsObj->Get(L#prop, 0, &vtProp, 0, 0); \
	obj.##prop = vtProp.iVal; \
	OleAut32VariantClear(&vtProp);

#define GetWmiPropertyBoolValue(pclsObj, vtProp, obj, prop) \
	pclsObj->Get(L#prop, 0, &vtProp, 0, 0); \
	obj.##prop = (vtProp.bVal != 0); \
	OleAut32VariantClear(&vtProp);

#define GetWmiPropertyFloatValue(pclsObj, vtProp, obj, prop) \
	pclsObj->Get(L#prop, 0, &vtProp, 0, 0); \
	obj.##prop = vtProp.fltVal; \
	OleAut32VariantClear(&vtProp);

bool GetSystemInfoOS(IWbemServices *pSvc, Win32_OperatingSystem& os)
{
	IEnumWbemClassObject* pEnumerator = NULL;
	HRESULT hres = pSvc->ExecQuery(
		bstr_t("WQL"),
		bstr_t("SELECT * FROM Win32_OperatingSystem where Primary=true"),
		WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
		NULL,
		&pEnumerator);

	if (FAILED(hres))
		return false;

	IWbemClassObject *pclsObj = NULL;
	ULONG uReturn = 0;

	if (pEnumerator)
	{
		HRESULT hr = pEnumerator->Next(WBEM_INFINITE, 1,
			&pclsObj, &uReturn);

		if (0 == uReturn)
		{
			return false;
		}

		VARIANT vtProp;

		GetWmiPropertyStrValue(pclsObj, vtProp, os, BootDevice);
		GetWmiPropertyStrValue(pclsObj, vtProp, os, BuildNumber);
		GetWmiPropertyStrValue(pclsObj, vtProp, os, CodeSet);
		GetWmiPropertyStrValue(pclsObj, vtProp, os, CountryCode);
		GetWmiPropertyStrValue(pclsObj, vtProp, os, Description);
		GetWmiPropertyStrValue(pclsObj, vtProp, os, Locale);
		GetWmiPropertyStrValue(pclsObj, vtProp, os, Manufacturer);
		GetWmiPropertyStrValue(pclsObj, vtProp, os, Name);
		GetWmiPropertyStrValue(pclsObj, vtProp, os, Organization);
		GetWmiPropertyStrValue(pclsObj, vtProp, os, OSArchitecture);
		GetWmiPropertyStrValue(pclsObj, vtProp, os, SystemDevice);
		GetWmiPropertyStrValue(pclsObj, vtProp, os, SystemDirectory);
		GetWmiPropertyStrValue(pclsObj, vtProp, os, SystemDrive);
		GetWmiPropertyStrValue(pclsObj, vtProp, os, Version);
		GetWmiPropertyStrValue(pclsObj, vtProp, os, WindowsDirectory);

		GetWmiPropertyUshortValue(pclsObj, vtProp, os, OSType);
		GetWmiPropertyUshortValue(pclsObj, vtProp, os, ServicePackMajorVersion);
		GetWmiPropertyUshortValue(pclsObj, vtProp, os, ServicePackMinorVersion);

		GetWmiPropertyUlongValue(pclsObj, vtProp, os, EncryptionLevel);
		GetWmiPropertyUlongValue(pclsObj, vtProp, os, OSLanguage);
		GetWmiPropertyUlongValue(pclsObj, vtProp, os, OSProductSuite);
		GetWmiPropertyUlongValue(pclsObj, vtProp, os, ProductType);
		GetWmiPropertyUlongValue(pclsObj, vtProp, os, SuiteMask);

		GetWmiPropertyUlonglongValue(pclsObj, vtProp, os, MaxProcessMemorySize);
		GetWmiPropertyUlonglongValue(pclsObj, vtProp, os, SizeStoredInPagingFiles);
		GetWmiPropertyUlonglongValue(pclsObj, vtProp, os, TotalSwapSpaceSize);
		GetWmiPropertyUlonglongValue(pclsObj, vtProp, os, TotalVirtualMemorySize);
		GetWmiPropertyUlonglongValue(pclsObj, vtProp, os, TotalVisibleMemorySize);

		GetWmiPropertyCharValue(pclsObj, vtProp, os, DataExecutionPrevention_SupportPolicy);
		GetWmiPropertyCharValue(pclsObj, vtProp, os, ForegroundApplicationBoost);
		GetWmiPropertyCharValue(pclsObj, vtProp, os, QuantumLength);
		GetWmiPropertyCharValue(pclsObj, vtProp, os, QuantumType);

		GetWmiPropertyShortValue(pclsObj, vtProp, os, CurrentTimeZone);

		GetWmiPropertyBoolValue(pclsObj, vtProp, os, DataExecutionPrevention_Available);
		GetWmiPropertyBoolValue(pclsObj, vtProp, os, DataExecutionPrevention_32BitApplications);
		GetWmiPropertyBoolValue(pclsObj, vtProp, os, DataExecutionPrevention_Drivers);
		GetWmiPropertyBoolValue(pclsObj, vtProp, os, Distributed);
		GetWmiPropertyBoolValue(pclsObj, vtProp, os, PAEEnabled);
		GetWmiPropertyBoolValue(pclsObj, vtProp, os, PortableOperatingSystem);

		pclsObj->Release();
		pEnumerator->Release();

		return true;
	}

	pEnumerator->Release();

	return false;
}

bool GetSystemInfoDiskDrives(IWbemServices *pSvc, std::vector<Win32_DiskDrive>& drives)
{
	IEnumWbemClassObject* pEnumerator = NULL;
	HRESULT hres = pSvc->ExecQuery(
		bstr_t("WQL"),
		bstr_t("SELECT * FROM Win32_DiskDrive where Status=OK"),
		WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
		NULL,
		&pEnumerator);

	if (FAILED(hres))
		return false;

	IWbemClassObject *pclsObj = NULL;
	ULONG uReturn = 0;

	Win32_DiskDrive diskDrive;
	while (pEnumerator)
	{
		HRESULT hr = pEnumerator->Next(WBEM_INFINITE, 1,
			&pclsObj, &uReturn);

		if (0 == uReturn)
		{
			break;
		}

		VARIANT vtProp;

		GetWmiPropertyStrValue(pclsObj, vtProp, diskDrive, CompressionMethod);
		GetWmiPropertyStrValue(pclsObj, vtProp, diskDrive, DeviceID);
		GetWmiPropertyStrValue(pclsObj, vtProp, diskDrive, InterfaceType);
		GetWmiPropertyStrValue(pclsObj, vtProp, diskDrive, Manufacturer);
		GetWmiPropertyStrValue(pclsObj, vtProp, diskDrive, MediaType);
		GetWmiPropertyStrValue(pclsObj, vtProp, diskDrive, Model);
		GetWmiPropertyStrValue(pclsObj, vtProp, diskDrive, Name);

		GetWmiPropertyUshortValue(pclsObj, vtProp, diskDrive, Availability);

		GetWmiPropertyUlongValue(pclsObj, vtProp, diskDrive, BytesPerSector);
		GetWmiPropertyUlongValue(pclsObj, vtProp, diskDrive, ConfigManagerErrorCode);
		GetWmiPropertyUlongValue(pclsObj, vtProp, diskDrive, Index);
		GetWmiPropertyUlongValue(pclsObj, vtProp, diskDrive, Partitions);
		GetWmiPropertyUlongValue(pclsObj, vtProp, diskDrive, SectorsPerTrack);
		GetWmiPropertyUlongValue(pclsObj, vtProp, diskDrive, Signature);
		GetWmiPropertyUlongValue(pclsObj, vtProp, diskDrive, TotalHeads);
		GetWmiPropertyUlongValue(pclsObj, vtProp, diskDrive, TracksPerCylinder);

		GetWmiPropertyUlonglongValue(pclsObj, vtProp, diskDrive, DefaultBlockSize);
		GetWmiPropertyUlonglongValue(pclsObj, vtProp, diskDrive, Size);
		GetWmiPropertyUlonglongValue(pclsObj, vtProp, diskDrive, TotalCylinders);
		GetWmiPropertyUlonglongValue(pclsObj, vtProp, diskDrive, TotalSectors);
		GetWmiPropertyUlonglongValue(pclsObj, vtProp, diskDrive, TotalTracks);

		GetWmiPropertyBoolValue(pclsObj, vtProp, diskDrive, MediaLoaded);

		drives.push_back(diskDrive);
		pclsObj->Release();
	}

	pEnumerator->Release();

	return true;
}

bool GetSystemInfoPhysicalMemoryArrays(IWbemServices *pSvc, std::vector<Win32_PhysicalMemoryArray>& memArrays)
{
	IEnumWbemClassObject* pEnumerator = NULL;
	HRESULT hres = pSvc->ExecQuery(
		bstr_t("WQL"),
		bstr_t("SELECT * FROM Win32_PhysicalMemoryArray where Status=OK"),
		WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
		NULL,
		&pEnumerator);

	if (FAILED(hres))
		return false;

	IWbemClassObject *pclsObj = NULL;
	ULONG uReturn = 0;

	Win32_PhysicalMemoryArray memarr;
	while (pEnumerator)
	{
		HRESULT hr = pEnumerator->Next(WBEM_INFINITE, 1,
			&pclsObj, &uReturn);

		if (0 == uReturn)
		{
			break;
		}

		VARIANT vtProp;

		GetWmiPropertyStrValue(pclsObj, vtProp, memarr, Manufacturer);
		GetWmiPropertyStrValue(pclsObj, vtProp, memarr, Model);
		GetWmiPropertyStrValue(pclsObj, vtProp, memarr, Name);
		GetWmiPropertyStrValue(pclsObj, vtProp, memarr, PartNumber);
		GetWmiPropertyStrValue(pclsObj, vtProp, memarr, Version);

		GetWmiPropertyUshortValue(pclsObj, vtProp, memarr, Location);
		GetWmiPropertyUshortValue(pclsObj, vtProp, memarr, MemoryDevices);
		GetWmiPropertyUshortValue(pclsObj, vtProp, memarr, MemoryErrorCorrection);
		GetWmiPropertyUshortValue(pclsObj, vtProp, memarr, Use);

		GetWmiPropertyFloatValue(pclsObj, vtProp, memarr, Depth);
		GetWmiPropertyFloatValue(pclsObj, vtProp, memarr, Height);
		GetWmiPropertyFloatValue(pclsObj, vtProp, memarr, Weight);
		GetWmiPropertyFloatValue(pclsObj, vtProp, memarr, Width);

		GetWmiPropertyBoolValue(pclsObj, vtProp, memarr, HotSwappable);
		GetWmiPropertyBoolValue(pclsObj, vtProp, memarr, PoweredOn);
		GetWmiPropertyBoolValue(pclsObj, vtProp, memarr, Removable);
		GetWmiPropertyBoolValue(pclsObj, vtProp, memarr, Replaceable);

		memArrays.push_back(memarr);
		pclsObj->Release();
	}

	pEnumerator->Release();

	return true;
}

bool GetSystemInfoPhysicalMemories(IWbemServices *pSvc, std::vector<Win32_PhysicalMemory>& mems)
{
	IEnumWbemClassObject* pEnumerator = NULL;
	HRESULT hres = pSvc->ExecQuery(
		bstr_t("WQL"),
		bstr_t("SELECT * FROM Win32_PhysicalMemory where Status=OK"),
		WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
		NULL,
		&pEnumerator);

	if (FAILED(hres))
		return false;

	IWbemClassObject *pclsObj = NULL;
	ULONG uReturn = 0;

	Win32_PhysicalMemory mem;
	while (pEnumerator)
	{
		HRESULT hr = pEnumerator->Next(WBEM_INFINITE, 1,
			&pclsObj, &uReturn);

		if (0 == uReturn)
		{
			break;
		}

		VARIANT vtProp;

		GetWmiPropertyStrValue(pclsObj, vtProp, mem, BankLabel);
		GetWmiPropertyStrValue(pclsObj, vtProp, mem, DeviceLocator);
		GetWmiPropertyStrValue(pclsObj, vtProp, mem, Manufacturer);
		GetWmiPropertyStrValue(pclsObj, vtProp, mem, Model);
		GetWmiPropertyStrValue(pclsObj, vtProp, mem, Name);
		GetWmiPropertyStrValue(pclsObj, vtProp, mem, PartNumber);
		GetWmiPropertyStrValue(pclsObj, vtProp, mem, Version);

		GetWmiPropertyUshortValue(pclsObj, vtProp, mem, DataWidth);
		GetWmiPropertyUshortValue(pclsObj, vtProp, mem, FormFactor);
		GetWmiPropertyUshortValue(pclsObj, vtProp, mem, InterleaveDataDepth);
		GetWmiPropertyUshortValue(pclsObj, vtProp, mem, MemoryType);
		GetWmiPropertyUshortValue(pclsObj, vtProp, mem, TotalWidth);
		GetWmiPropertyUshortValue(pclsObj, vtProp, mem, TypeDetail);

		GetWmiPropertyUlongValue(pclsObj, vtProp, mem, ConfiguredClockSpeed);
		GetWmiPropertyUlongValue(pclsObj, vtProp, mem, ConfiguredVoltage);
		GetWmiPropertyUlongValue(pclsObj, vtProp, mem, InterleavePosition);
		GetWmiPropertyUlongValue(pclsObj, vtProp, mem, MaxVoltage);
		GetWmiPropertyUlongValue(pclsObj, vtProp, mem, MinVoltage);
		GetWmiPropertyUlongValue(pclsObj, vtProp, mem, PositionInRow);
		GetWmiPropertyUlongValue(pclsObj, vtProp, mem, Speed);

		GetWmiPropertyUlonglongValue(pclsObj, vtProp, mem, Capacity);

		GetWmiPropertyBoolValue(pclsObj, vtProp, mem, HotSwappable);
		GetWmiPropertyBoolValue(pclsObj, vtProp, mem, PoweredOn);
		GetWmiPropertyBoolValue(pclsObj, vtProp, mem, Removable);
		GetWmiPropertyBoolValue(pclsObj, vtProp, mem, Replaceable);

		mems.push_back(mem);
		pclsObj->Release();
	}

	pEnumerator->Release();

	return true;
}

bool GetSystemInfoPageFile(IWbemServices *pSvc, std::vector<Win32_PageFileUsage>& pageFiles)
{
	IEnumWbemClassObject* pEnumerator = NULL;
	HRESULT hres = pSvc->ExecQuery(
		bstr_t("WQL"),
		bstr_t("SELECT * FROM Win32_PageFileUsage where Status=OK"),
		WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
		NULL,
		&pEnumerator);

	if (FAILED(hres))
		return false;

	IWbemClassObject *pclsObj = NULL;
	ULONG uReturn = 0;

	Win32_PageFileUsage pfu;
	while (pEnumerator)
	{
		HRESULT hr = pEnumerator->Next(WBEM_INFINITE, 1,
			&pclsObj, &uReturn);

		if (0 == uReturn)
		{
			break;
		}

		VARIANT vtProp;

		GetWmiPropertyStrValue(pclsObj, vtProp, pfu, Name);

		GetWmiPropertyUlongValue(pclsObj, vtProp, pfu, AllocatedBaseSize);

		GetWmiPropertyBoolValue(pclsObj, vtProp, pfu, TempPageFile);

		pageFiles.push_back(pfu);
		pclsObj->Release();
	}

	pEnumerator->Release();

	return true;
}

bool GetSystemInfo(Win32_SystemInfo& systemInfo)
{
	HRESULT hres;

	// Step 1: --------------------------------------------------
	// Initialize COM. ------------------------------------------

	hres = Ole32CoInitializeEx(0, COINIT_MULTITHREADED);
	if (FAILED(hres))
	{
		// Failed to initialize COM library.
		return false;
	}

	// Step 2: --------------------------------------------------
	// Set general COM security levels --------------------------

	hres = Ole32CoInitializeSecurity(
		NULL,
		-1,                          // COM authentication
		NULL,                        // Authentication services
		NULL,                        // Reserved
		RPC_C_AUTHN_LEVEL_DEFAULT,   // Default authentication 
		RPC_C_IMP_LEVEL_IMPERSONATE, // Default Impersonation  
		NULL,                        // Authentication info
		EOAC_NONE,                   // Additional capabilities 
		NULL                         // Reserved
		);


	if (FAILED(hres))
	{
		// Failed to initialize security.
		Ole32CoUninitialize();
		return false;
	}

	// Step 3: ---------------------------------------------------
	// Obtain the initial locator to WMI -------------------------

	IWbemLocator *pLoc = NULL;

	hres = Ole32CoCreateInstance(
		CLSID_WbemLocator,
		0,
		CLSCTX_INPROC_SERVER,
		IID_IWbemLocator, (LPVOID *)&pLoc);

	if (FAILED(hres))
	{
		// Failed to create IWbemLocator object.
		Ole32CoUninitialize();
		return false;
	}

	// Step 4: -----------------------------------------------------
	// Connect to WMI through the IWbemLocator::ConnectServer method

	IWbemServices *pSvc = NULL;

	// Connect to the root\cimv2 namespace with
	// the current user and obtain pointer pSvc
	// to make IWbemServices calls.
	hres = pLoc->ConnectServer(
		_bstr_t(L"ROOT\\CIMV2"), // Object path of WMI namespace
		NULL,                    // User name. NULL = current user
		NULL,                    // User password. NULL = current
		0,                       // Locale. NULL indicates current
		NULL,                    // Security flags.
		0,                       // Authority (for example, Kerberos)
		0,                       // Context object 
		&pSvc                    // pointer to IWbemServices proxy
		);

	if (FAILED(hres))
	{
		// Failed to create IWbemLocator object.
		pLoc->Release();
		Ole32CoUninitialize();
		return false;
	}

	// Connected to ROOT\\CIMV2 WMI namespace.


	// Step 5: --------------------------------------------------
	// Set security levels on the proxy -------------------------

	hres = Ole32CoSetProxyBlanket(
		pSvc,                        // Indicates the proxy to set
		RPC_C_AUTHN_WINNT,           // RPC_C_AUTHN_xxx
		RPC_C_AUTHZ_NONE,            // RPC_C_AUTHZ_xxx
		NULL,                        // Server principal name 
		RPC_C_AUTHN_LEVEL_CALL,      // RPC_C_AUTHN_LEVEL_xxx 
		RPC_C_IMP_LEVEL_IMPERSONATE, // RPC_C_IMP_LEVEL_xxx
		NULL,                        // client identity
		EOAC_NONE                    // proxy capabilities 
		);

	if (FAILED(hres))
	{
		// Could not set proxy blanket.
		pSvc->Release();
		pLoc->Release();
		Ole32CoUninitialize();
		return false;
	}

	GetSystemInfoOS(pSvc, systemInfo.OS);
	GetSystemInfoDiskDrives(pSvc, systemInfo.DiskDrives);
	GetSystemInfoPhysicalMemoryArrays(pSvc, systemInfo.PhysicalMemoryArrays);
	GetSystemInfoPhysicalMemories(pSvc, systemInfo.PhysicalMemories);
	GetSystemInfoPageFile(pSvc, systemInfo.PageFiles);

	// Cleanup
	// ========

	pSvc->Release();
	pLoc->Release();
	Ole32CoUninitialize();

	return true;
}
*/

#elif defined(TARGET_LINUX)

// link librt.a
// gcc -o gettime gettime.c -lrt

#include <time.h> /* clock_gettime */
#include <semaphore.h>  /* sem_t */
#include <sys/types.h> /* pid_t */
#include <unistd.h> /* getpid(), getuid() */
#include <sys/mman.h> /* mlock() */
#include <sys/sysinfo.h> /* sysinfo() */
#include <sys/stat.h> /* stat */
#include <errno.h> /* errno */
#include <sys/resource.h> /* getrlimit() */
#include <stdio.h> /* FILE */ 
#include <inttypes.h> /* uintptr_t */
#include <string.h> /* strrchr() */
#include <cpuid.h> /* cpuid */

#define BILLION 1000000000L
#define MILLION 1000000L

FILE *g_pagemap;

bool IsPlatSupported()
{
	timespec ts;
	bool success = true;
	success = success && (clock_gettime(CLOCK_MONOTONIC, &ts) == 0);
	return success;
}

void GetCurrentTimestamp(TIME_STAMP& timestamp)
{
	timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	timestamp = BILLION * ts.tv_sec + ts.tv_nsec;
}

void GetCurrentTimestampFreq(TIME_STAMP& freq)
{
	freq = BILLION;
}

void GetTimeSpan(TIME_STAMP from, TIME_STAMP to, TIME_UNIT unit, TIME_STAMP& timeSpan)
{
	TIME_STAMP factor;
	switch (unit)
	{
	case TenthMicroseconds:
		factor = 100;
		break;
	case Microseconds:
		factor = 1000;
		break;
	case Milliseconds:
		factor = MILLION;
		break;
	case Seconds:
		factor = BILLION;
		break;
	default:
		factor = 0;
		break;
	}
	timeSpan = (to - from) / factor;
}

void GetTimeSpan(TIME_STAMP to, TIME_UNIT unit, TIME_STAMP& timeSpan)
{
	GetTimeSpan(g_startingTime, to, unit, timeSpan);
}

bool Is32BitPlat()
{
#if defined(TARGET_IA32)
	bool is32Bit = true;
#elif defined(TARGET_IA32E)
	bool is32Bit = false;
#endif
	return is32Bit;
}

void SetStartingTimestamp()
{
	GetCurrentTimestamp(g_startingTime);
}

void GetStartingTimestamp(TIME_STAMP& timestamp)
{
	timestamp = g_startingTime;
}

PSEMAPHORE Semaphore_CreateUnnamed()
{
	sem_t *sem = new sem_t;
	sem_init(sem, 0, 0);
	return sem;
}

bool Semaphore_Increment(PSEMAPHORE sem, long inc, long *prevCount)
{
	sem_t *s = (sem_t*)sem;
	if(prevCount)
	{
		int value;
		sem_getvalue(s, &value);
		*prevCount = value;
	}

	while(inc > 0)
	{
		int err = sem_post(s);
		if(err != 0)
			return false;
		--inc;
	}

	return true;
}

bool Semaphore_Decrement(PSEMAPHORE sem)
{
	sem_t *s = (sem_t*)sem;
	return (sem_wait(s) == 0);
}

bool Semaphore_Close(PSEMAPHORE sem)
{
	sem_t *s = (sem_t*)sem;
	int success = sem_destroy(s);
	delete s;
	return (success == 0);
}

PMUTEX Mutex_CreateNamed(LPCTSTR name)
{
	sem_t *mutex;
	mutex = sem_open(name, O_CREAT, 0777, 1);
	return mutex;
}

bool Mutex_Acquire(PMUTEX mutex)
{
	sem_t *s = (sem_t*)mutex;
	return (sem_wait(s) == 0);
}

bool Mutex_Release(PMUTEX mutex)
{
	sem_t *s = (sem_t*)mutex;
	return (sem_post(s) == 0);
}

bool Mutex_Close(PMUTEX mutex)
{
	sem_t *s = (sem_t*)mutex;
	return (sem_close(s) == 0);
}

PPROCESS GetCurrentProcessPortable()
{
	return NULL;
}

unsigned int GetProcessIdPortable(PPROCESS process)
{
	return (unsigned int)getpid();
}

bool PortableInit(INIT_FLAGS flags)
{
	if (!ValidateInitFlags(flags))
		return false;

	g_pagemap = NULL;
	if ((flags & INIT_FLAGS_LOAD_DRIVER) == INIT_FLAGS_LOAD_DRIVER)
	{
		if (!(g_pagemap = fopen("/proc/self/pagemap", "r")))
			return false;
	}

	return true;
}

void PortableDispose()
{
	if(g_pagemap != NULL)
		fclose(g_pagemap);
}

unsigned long Virtual2Physical(void *vaddrs[], unsigned long long paddrs[], unsigned int size)
{
	unsigned long count = 0;
	for(unsigned int i = 0; i < size; ++i)
	{
		intptr_t vaddr = (intptr_t)vaddrs[i];
		unsigned long long paddr = 0;
		int offset = (vaddr / sysconf(_SC_PAGESIZE)) * sizeof(uint64_t);
		uint64_t e;

		paddrs[i] = (unsigned long long)-1;
		if (lseek(fileno(g_pagemap), offset, SEEK_SET) == offset) {
			if (fread(&e, sizeof(uint64_t), 1, g_pagemap)) {
				if (e & (1ULL << 63)) { // page present ?
					paddr = e & ((1ULL << 54) - 1); // pfn mask
					paddr = paddr * sysconf(_SC_PAGESIZE);
					// add offset within page
					paddr = paddr | (vaddr & (sysconf(_SC_PAGESIZE) - 1));
					paddrs[i] = paddr;
					++count;
				}   
			}   
		}
	}
	return count;
}

bool IsElevated()
{
	if (getuid() == 0);
}

bool GetMainModuleFilePathA(char *fileName, int size)
{

}

bool PinVirtualPage(const void *addr)
{
	return (mlock(addr, 1) == 0);
}

bool UnpinVirtualPage(const void *addr)
{
	return (munlock(addr, 1) == 0);
}

bool PrepareForPinningVirtualPages()
{
	return true;
}

void GetAddressSizes(unsigned char& vAddressSize, unsigned char& pAddressSize)
{
#ifdef TARGET_IA32
	vAddressSize = 4; // 4 bytes.
#else
	vAddressSize = 6; // 6 bytes.
#endif /* TARGET_IA32 */
	sysinfo info;
	if (sysinfo(&info) == 0)
	{
		unsigned long long TotalRam = info.totalram * info.mem_unit;
		pAddressSize = 2;
		unsigned long long mask = 0xFF0000;
		while ((TotalRam & mask) != 0)
		{
			pAddressSize += 1;
			mask <<= 8;
		}
	}
	else
	{
		pAddressSize = 8; // The maximum.
	}
}

void GetSystemInfo(BINARY_HEADER& header)
{
	rlimit rl;
	getrlimit(RLIMIT_AS, &rl);
	header.minAppAddr = 0;
	header.maxAppAddr = rl.rlim_max;

	sysinfo info = {0};
	sysinfo(&info);
	unsigned long long TotalRam = info.totalram * info.mem_unit;
	header.totalDram = (unsigned int)(TotalRam / (1024 * 1024));

	header.processOsId = GetCurrentProcessId();
	header.processorArch = Is32BitPlat() ? PROCESSOR_ARCH_X86 : PROCESSOR_ARCH_X64;

	header.os = BINARY_HEADER_OS_LINUX;

	GetAddressSizes(header.vAddrSize, header.pAddrSize); 

	header.systemDirectory = PORTABLE_PATH_DELIMITER;

	header.currentDirectory = get_current_dir_name();

	header.mainExePath = new char[PORTABLE_MAX_PATH];
	memset(header.mainExePath, 0, PORTABLE_MAX_PATH); // Make sure that mainExePath is nul-terminated.
	readlink("/proc/self/exe", header.mainExePath, PORTABLE_MAX_PATH);

	header.cmdLine ="";

	GetCurrentTimestampFreq(header.timeFreq);
}

unsigned int GetVirtualPageSize()
{
	return (unsigned int)sysconf(_SC_PAGE_SIZE);
}

void* CreateNamedFileMapping(unsigned int maxSize, const char *name, bool *alreadyExists)
{
	int fd; 
	if ((fd = open("/dev/zero", O_RDWR)) == -1)
		return NULL;

	void *p = mmap (0, maxSize, PROT_NONE, fd, 0);

	close(fd);

	if(alreadyExists)
		*alreadyExists = true;
	
	if (p == MAP_FAILED)
		return NULL;
	else
		return p;
}

bool DirectoryExists(const char *directory)
{
	stat sb;
	stat(directory, &sb);
	bool isdir = S_ISDIR(st.st_mode);
	return isdir;
}

bool EnsureCommited(void *address, unsigned int sizeInBytes)
{
	return (mprotect(address, sizeInBytes, PROT_READ | PROT_WRITE) == 0);
}

unsigned int GetParentProcessId()
{
	return (unsigned int)getppid();
}

bool PortableCreateDirectory(const char *directory)
{
	stat st;
	bool success = true;

	if (stat(directory, &st) != 0)
	{
		/* Directory does not exist. EEXIST for race condition */
		if (mkdir(path, 0777) != 0 && errno != EEXIST)
			status = false;
	}
	else if (!S_ISDIR(st.st_mode))
	{
		errno = ENOTDIR;
		status = false;
	}

	return success;
}

const char* SystemStatusString(unsigned int status);
std::string MemoryAllocationFlagsToString(unsigned int allocationFlags);
std::string MemoryProtectionFlagstoString(unsigned int protectionFlags);
std::string MemoryFreeFlagsString(unsigned int freeFlags);
std::string HeapCreateFlagsString(unsigned int heapFlags);

std::string PathToDirectory(const std::string& path)
{
	const std::string& directory = path.substr(0, path.find_last_of(PORTABLE_PATH_DELIMITER) + 1);
	return directory;
}

bool LoadDriver()
{
	return true;
}

std::string GetLastErrorString(unsigned int errCode)
{
	return strerror((int)errCode);
}

void *GetProcessSystemInfoAddr()
{
	return NULL;
}

void GetProcessDefaultHeapAddr(void **addr, void **heapId)
{
	*addr = NULL;
	*heapId = NULL;
}

void *GetThreadSystemInfoAddr()
{
	return NULL
}

bool GetProcessorPackages(std::vector<PORTABLE_PROCESSOR_PACKAGE>& processors)
{
	// Get a list of processor packages.
}

void PathToName(const std::string& path, std::string& name)
{
	name = path.substr(path.find_last_of(PORTABLE_PATH_DELIMITER) + 1).c_str();
}

void IntrinsicCpuid(int cpuInfo[4], int fid, int subfid)
{
	__cpuid_count(fid, subfid, cpuInfo[0], cpuInfo[1], cpuInfo[2], cpuInfo[3]);
}

unsigned long long IntrinsicXgetbv(unsigned int xcr)
{
	u32 eax, edx;
	asm volatile(".byte 0x0f,0x01,0xd0" /* xgetbv */
		         : "=a" (eax), "=d" (edx)
		         : "c" (index));
	return eax + ((u64)edx << 32);
}

#else

bool IsPlatSupported(){return true;}
void GetCurrentTimestamp(TIME_STAMP& timestamp){}
void GetCurrentTimestampFreq(TIME_STAMP& freq){}
void GetTimeSpan(TIME_STAMP from, TIME_STAMP to, TIME_UNIT unit, TIME_STAMP& timeSpan){}
void GetTimeSpan(TIME_STAMP to, TIME_UNIT unit, TIME_STAMP& timeSpan){}
bool Is32BitPlat(){return true;}
void SetStartingTimestamp(){}
void GetStartingTimestamp(TIME_STAMP& timestamp){}
PSEMAPHORE Semaphore_CreateUnnamed(){ return 0; }
bool Semaphore_Increment(PSEMAPHORE sem, long inc, long *prevCount){return true;}
bool Semaphore_Decrement(PSEMAPHORE sem){return true;}
bool Semaphore_Close(PSEMAPHORE sem){return true;}
PMUTEX Mutex_CreateNamed(LPCTSTR name){return 0;}
bool Mutex_Acquire(PMUTEX mutex){return true;}
bool Mutex_Release(PMUTEX mutex){return true;}
bool Mutex_Close(PMUTEX mutex){return true;}
PPROCESS GetCurrentProcessPortable(){return 0;}
unsigned int GetProcessIdPortable(PPROCESS process){return true;}
bool PortableInit(INIT_FLAGS flags){return true;}
void PortableDispose(){}
unsigned long Virtual2Physical(void *vaddrs[], unsigned long long paddrs[], unsigned int size){return true;}
bool IsElevated(){return true;}
bool GetMainModuleFilePathA(char *fileName, int size){return true;}
bool PinVirtualPage(const void *addr){return true;}
bool UnpinVirtualPage(const void *addr){return true;}
bool PrepareForPinningVirtualPages(){return true;}
void GetAddressSizes(unsigned char& vAddressSize, unsigned char& pAddressSize){}
void GetSystemInfo(BINARY_HEADER& header){}
unsigned int GetVirtualPageSize(){return true;}
void* CreateNamedFileMapping(unsigned int maxSize, const char *name, bool *alreadyExists){ return NULL; }
bool DirectoryExists(const char *directory){return true;}
bool EnsureCommited(void *address, unsigned int sizeInBytes){return true;}
unsigned int GetParentProcessId(){return true;}
bool PortableCreateDirectory(const char *directory){return true;}
const char* SystemStatusString(unsigned int status){ return ""; }
std::string MemoryAllocationFlagsToString(unsigned int allocationFlags){ return ""; }
std::string MemoryProtectionFlagstoString(unsigned int protectionFlags){ return ""; }
std::string MemoryFreeFlagsString(unsigned int freeFlags){ return ""; }
std::string HeapCreateFlagsString(unsigned int heapFlags){ return ""; }
std::string PathToDirectory(const std::string& path){ return ""; }
bool LoadDriver(){return true;}
std::string GetLastErrorString(unsigned int errCode){return "";}
void *GetProcessSystemInfoAddr(){ return NULL; }
void *GetThreadSystemInfoAddr(){ return NULL; }
void GetProcessDefaultHeapAddr(void **addr, void **heapId){}
void PathToName(const std::string& path, std::string& name){}
bool GetProcessorPackages(std::vector<PORTABLE_PROCESSOR_PACKAGE>& processors){return true;}
bool GetMyProcessorNumber(const std::vector<PORTABLE_PROCESSOR_PACKAGE>& pks, unsigned int& package, unsigned int& number){return true;}
bool SetMyThreadAffinity(const PORTABLE_PROCESSOR_PACKAGE& pk){return true;}
void GetMyThreadAffinity(PORTABLE_PROCESSOR_PACKAGE& pk){}

#endif /* TARGET_WINDOWS */