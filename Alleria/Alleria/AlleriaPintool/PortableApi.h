#ifndef PORTABLEAPIS_H
#define PORTABLEAPIS_H

#include <vector>
#include <string>

#include "BinaryProfile.h"

// The interpretation of all of these types is OS-dependent.
typedef unsigned long long TIME_STAMP;
typedef void *PSEMAPHORE;
typedef void *PMUTEX;
typedef void *PPROCESS;
typedef void *PFILE_MAPPING;
typedef void *PHEAP;
struct PORTABLE_PROCESSOR_PACKAGE
{
	/*
	
	Interpretation of this type on Windows:
	The first element holds the least-significant 32-bit of the affinity mask.
	The second element holds the most-significant 32-bit of the affinity mask.
	The third element holds the processor group number.
	The fourth element holds the processory package number.
	
	*/

	unsigned int reserved[4];
};

#define PORTABLE_MAX_PROCESSOR_REGISTER_SIZE 8

#define PROCESS_INVALID_ID -1

#ifdef TARGET_WINDOWS
#define PORTABLE_MAX_PATH 260
#define PORTABLE_PATH_DELIMITER "\\"
#define NEW_LINE "\r\n"
#elif defined(TARGET_LINUX)
#define PORTABLE_MAX_PATH 4096
#define PORTABLE_PATH_DELIMITER "/"
#define NEW_LINE "\n"
#endif

#include <intrin.h>

#define _NUM_TO_CHAR_PTR_(x) #x
#define LITERAL_TO_CHAR_PTR(x) _NUM_TO_CHAR_PTR_(x)

enum TIME_UNIT
{
	TenthMicroseconds,
	Microseconds,
	Milliseconds,
	Seconds
};

enum INIT_FLAGS
{
	INIT_FLAGS_NOTHING = 0x00,
	INIT_FLAGS_ELEVATION = 0x01,
	INIT_FLAGS_ODS_AS_REQUIRED = 0x04,
	INIT_FLAGS_LOAD_DRIVER = 0x02 | INIT_FLAGS_ELEVATION | INIT_FLAGS_ODS_AS_REQUIRED,
	INIT_FLAGS_ODS_SUPPRESS = 0x08
};

#ifdef  UNICODE

#else /* UNICODE */

typedef const char* LPCTSTR;

#endif /* UNICODE */

// Pin 3.0 does not offer portable implementation for these APIs.
void GetAddressSizes(unsigned char& vAddressSize, unsigned char& pAddressSize);
bool IsPlatSupported();
void PortableInitBasic();
void GetCurrentTimestamp(TIME_STAMP& timestamp);
void GetCurrentTimestampFreq(TIME_STAMP& freq);
void GetTimeSpan(TIME_STAMP from, TIME_STAMP to, TIME_UNIT unit, TIME_STAMP& timeSpan);
void GetTimeSpan(TIME_STAMP to, TIME_UNIT unit, TIME_STAMP& timeSpan);
bool Is32BitPlat();
void SetStartingTimestamp();
void GetStartingTimestamp(TIME_STAMP& timestamp);
PSEMAPHORE Semaphore_CreateUnnamed();
bool Semaphore_Increment(PSEMAPHORE sem, long inc, long *prevCount);
bool Semaphore_Decrement(PSEMAPHORE sem);
bool Semaphore_Close(PSEMAPHORE sem);
PMUTEX Mutex_CreateNamed(LPCTSTR name);
bool Mutex_Acquire(PMUTEX mutex);
bool Mutex_Release(PMUTEX mutex);
bool Mutex_Close(PMUTEX mutex);
PPROCESS GetCurrentProcessPortable();
bool PortableInit(INIT_FLAGS flags);
void PortableDispose();
unsigned long Virtual2Physical(void *vaddrs[], unsigned long long paddrs[], unsigned int size);
bool IsElevated();
bool PinVirtualPage(const void *addr);
bool UnpinVirtualPage(const void *addr);
bool PrepareForPinningVirtualPages();
void* CreateNamedFileMapping(unsigned int maxSize, const char *name, bool *alreadyExists);
bool EnsureCommited(void *address, unsigned int sizeInBytes);
void GetSystemInfo(BINARY_HEADER& header);
const char* SystemStatusString(unsigned int status);
std::string MemoryAllocationFlagsToString(unsigned int allocationFlags);
std::string MemoryProtectionFlagstoString(unsigned int protectionFlags);
std::string MemoryFreeFlagsString(unsigned int freeFlags);
std::string HeapFlagsString(unsigned int heapFlags);
bool LoadDriver();
std::string GetLastErrorString(unsigned int errCode);
void* GetProcessSystemInfoAddr();
void* GetThreadSystemInfoAddr();
void GetProcessDefaultHeapAddr(void **addr, void **heapId);
bool GetProcessorPackages(std::vector<PORTABLE_PROCESSOR_PACKAGE>& processors);

// Check whether Pin offer portable implementation for these.
unsigned int GetProcessIdPortable(PPROCESS process);
bool GetMainModuleFilePathA(char *fileName, int size);
unsigned int GetVirtualPageSize();
unsigned int GetParentProcessId();
std::string PathToDirectory(const std::string& path);
void PathToName(const std::string& path, std::string& name);

// OS_MkDir can replace these two functions. But unfortunately it's buggy.
bool PortableCreateDirectory(const char *directory);
bool DirectoryExists(const char *directory);

// GetMyProcessorNumber only fails when a processor package has been dynamically added.
// Support for hot-adding processors is not yet available.
bool GetMyProcessorNumber(const std::vector<PORTABLE_PROCESSOR_PACKAGE>& pks, unsigned int& package, unsigned int& number);
bool SetMyThreadAffinity(const PORTABLE_PROCESSOR_PACKAGE& pk);
void GetMyThreadAffinity(PORTABLE_PROCESSOR_PACKAGE& pk);

// Uses an internal collection of pks that is identical to the one obtained on the first call to GetProcessorPackages.
// Used by app threads to record the processor on which they are running.
bool GetMyProcessorNumber(unsigned int& package, unsigned int& number);

bool IsSystemFile(const char *filePath);
bool IsRuntimeFile(const char *filePath);
bool IsAppFile(const char *filePath);

void IntrinsicCpuid(int cpuInfo[4], int fid, int subfid);
unsigned long long IntrinsicXgetbv(unsigned int xcr);

// Clears the given string and stores the given integer in it.
void IntegerToString(UINT64 i, std::string& s);

#ifdef TARGET_WINDOWS

// C4242: A integral type was converted to annother integral type. A possible loss of data may have occurred.
#define DISABLE_COMPILER_WARNING_INT2INT_LOSS \
warning( disable : 4242)

#define RESTORE_COMPILER_WARNING_INT2INT_LOSS \
warning( default : 4242)

// C4244: A floating point type was converted to an integer type. A possible loss of data may have occurred.
#define DISABLE_COMPILER_WARNING_FLOAT2INT_LOSS \
warning( disable : 4244)

#define RESTORE_COMPILER_WARNING_FLOAT2INT_LOSS \
warning( default : 4244)

// C4267: Conversion from 'size_t' to 'unsigned int', possible loss of data.
#define DISABLE_COMPILER_WARNING_DATA_LOSS \
warning( disable : 4267)

#define RESTORE_COMPILER_WARNING_DATA_LOSS \
warning( default : 4267)

// C4311, C4302: Pointer truncation..
#define DISABLE_COMPILER_WARNING_PTRTRUNC_LOSS \
warning( disable : 4311 4302)

#define RESTORE_COMPILER_WARNING_PTRTRUNC_LOSS \
warning( default : 4311 4302)

#define MEMORY_REGION_FREE_FLAG 0x10000
#define PAGE_EXECUTE_READWRITE_FLAG 0x40

#elif TARGET_LINUX

#define DISABLE_COMPILER_WARNING_INT2INT_LOSS GCC diagnostic push
#define RESTORE_COMPILER_WARNING_INT2INT_LOSS GCC diagnostic pop
#define DISABLE_COMPILER_WARNING_FLOAT2INT_LOSS GCC diagnostic push
#define RESTORE_COMPILER_WARNING_FLOAT2INT_LOSS GCC diagnostic pop

#endif

#ifdef TARGET_WINDOWS

/*
struct Win32_OperatingSystem
{
	std::wstring         BootDevice;
	std::wstring         BuildNumber;
	std::wstring         CodeSet;
	std::wstring         CountryCode;
	signed short         CurrentTimeZone;
	bool                 DataExecutionPrevention_Available;
	bool                 DataExecutionPrevention_32BitApplications;
	bool                 DataExecutionPrevention_Drivers;
	unsigned char        DataExecutionPrevention_SupportPolicy;
	bool                 Debug;
	std::wstring         Description;
	bool                 Distributed;
	unsigned int         EncryptionLevel;
	unsigned char        ForegroundApplicationBoost;
	std::wstring         Locale;
	std::wstring         Manufacturer;
	unsigned long long   MaxProcessMemorySize;
	std::wstring         Name;
	std::wstring         Organization;
	std::wstring         OSArchitecture;
	unsigned int         OSLanguage;
	unsigned int         OSProductSuite;
	unsigned short       OSType;
	bool                 PAEEnabled;
	bool                 PortableOperatingSystem;
	unsigned int         ProductType;
	unsigned short       ServicePackMajorVersion;
	unsigned short       ServicePackMinorVersion;
	unsigned long long   SizeStoredInPagingFiles;
	unsigned int         SuiteMask;
	std::wstring         SystemDevice;
	std::wstring         SystemDirectory;
	std::wstring         SystemDrive;
	unsigned long long   TotalSwapSpaceSize;
	unsigned long long   TotalVirtualMemorySize;
	unsigned long long   TotalVisibleMemorySize;
	std::wstring         Version;
	std::wstring         WindowsDirectory;
	unsigned char        QuantumLength;
	unsigned char        QuantumType;
};

struct Win32_DiskDrive
{
	unsigned short       Availability;
	unsigned int         BytesPerSector;
	std::wstring         CompressionMethod;
	unsigned int         ConfigManagerErrorCode;
	unsigned long long   DefaultBlockSize;
	std::wstring         DeviceID;
	unsigned int         Index;
	std::wstring         InterfaceType;
	std::wstring         Manufacturer;
	bool                 MediaLoaded;
	std::wstring         MediaType;
	std::wstring         Model;
	std::wstring         Name;
	unsigned int         Partitions;
	unsigned int         SectorsPerTrack;
	unsigned int         Signature;
	unsigned long long   Size;
	unsigned long long   TotalCylinders;
	unsigned int         TotalHeads;
	unsigned long long   TotalSectors;
	unsigned long long   TotalTracks;
	unsigned int         TracksPerCylinder;
};

struct Win32_PageFileUsage
{
	unsigned int  AllocatedBaseSize;
	std::wstring  Name;
	bool          TempPageFile;
};

struct Win32_PhysicalMemoryArray
{
	float            Depth;
	float            Height;
	bool             HotSwappable;
	unsigned short   Location;
	std::wstring     Manufacturer;
	unsigned short   MemoryDevices;
	unsigned short   MemoryErrorCorrection;
	std::wstring     Model;
	std::wstring     Name;
	std::wstring     PartNumber;
	bool             PoweredOn;
	bool             Removable;
	bool             Replaceable;
	unsigned short   Use;
	std::wstring     Version;
	float            Weight;
	float            Width;
};

struct Win32_PhysicalMemory
{
	std::wstring         BankLabel;
	unsigned long long   Capacity;
	unsigned int         ConfiguredClockSpeed;
	unsigned int         ConfiguredVoltage;
	unsigned short       DataWidth;
	std::wstring         DeviceLocator;
	unsigned short       FormFactor;
	bool                 HotSwappable;
	unsigned short       InterleaveDataDepth;
	unsigned int         InterleavePosition;
	std::wstring         Manufacturer;
	unsigned int         MaxVoltage;
	unsigned short       MemoryType;
	unsigned int         MinVoltage;
	std::wstring         Model;
	std::wstring         Name;
	std::wstring         PartNumber;
	unsigned int         PositionInRow;
	bool                 PoweredOn;
	bool                 Removable;
	bool                 Replaceable;
	unsigned int         Speed;
	unsigned short       TotalWidth;
	unsigned short       TypeDetail;
	std::wstring         Version;
};

struct Win32_SystemInfo
{
	Win32_OperatingSystem                  OS; // The active operating system.

	// Only those devices whose status was OK will be recorded.
	std::vector<Win32_DiskDrive>           DiskDrives;
	std::vector<Win32_PhysicalMemoryArray> PhysicalMemoryArrays;
	std::vector<Win32_PhysicalMemory>      PhysicalMemories;
	std::vector<Win32_PageFileUsage>       PageFiles;
};
*/

#endif

#endif /* PORTABLEAPIS_H */