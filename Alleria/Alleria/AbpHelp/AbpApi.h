#ifndef ABP_API_H
#define ABP_API_H

#include "AbpTypes.h"

#if defined(ABP_DEV)
#if defined(TARGET_WINDOWS)
#define DECLSPEC __declspec(dllexport)
#elif defined(TARGET_LINUX)
#define DECLSPEC
#endif
#else
#if defined(TARGET_WINDOWS)
#define DECLSPEC __declspec(dllimport)
#elif defined(TARGET_LINUX)
#define DECLSPEC
#endif
#endif

#ifdef __cplusplus
extern "C" {
#endif


/*

APIs for managing profiles.

*/

/// Opens a binary profile.
/**
\param[in] path An ASCII string representing the path to a valid, accessible profile to open. This parameter cannot be NULL.
\return A profile handle that can be used to access information contained in the profile.
If the profile could not be opened or is an invalid profile, INVALID_PROFILE_HANDLE is returned.
You can call AbpGetLastError to determine the error that occured.
\sa PROFILE_HANDLE, AbpCloseProfile and AbpGetLastError
*/
DECLSPEC PROFILE_HANDLE AbpOpenProfile(PCSTR path);


/// Closes a binary profile.
/**
\param[in] profile A profile handle created by a successful call to AbpOpenProfile. 
\return TRUE if the specified profile was closed successfully. Otherwise, FALSE is returned.
You can call AbpGetLastError to determine the error that occured.
\sa PROFILE_HANDLE, AbpOpenProfile and AbpGetLastError
*/
DECLSPEC BOOL AbpCloseProfile(PROFILE_HANDLE profile);

/*

APIs for reading from the header.

*/

/// Obtains the version of a binary profile.
/**
\param[in] profile A profile handle created by a successful call to AbpOpenProfile.
\param[out] version A pointer to buffer that is large enough to store the version of the specified profile.
\param[in] size The size of the buffer specified by \a version in bytes.
\return If \a version is NULL, the function returns the size of the buffer required to hold the version string.
If \a size is smaller than the size of the version string including the terminating NULL character,
the function returns the size of the buffer required to hold the version string.
Otherwise if the specified profile is valid, the versions string is copied to the buffer including the terminating NULL character and 
the number of bytes copied is returned. If the specified profile handle is not valid, zero is returned.
You can call AbpGetLastError to determine the error that occured.
\sa PROFILE_HANDLE, AbpOpenProfile and AbpGetLastError
*/
DECLSPEC UINT32 AbpGetProfileVersion(PROFILE_HANDLE profile, PSTR version, UINT32 size);

/// Obtains the type of a binary profile.
/**
\param[in] profile A profile handle created by a successful call to AbpOpenProfile.
\return The type of the specified profile. You can call AbpGetLastError to determine the error that occured.
If the specified profile handle is not valid, BINARY_HEADER_TYPE_INVALID_PROFILE is returned.
\sa PROFILE_HANDLE, AbpOpenProfile, BINARY_HEADER_TYPE and AbpGetLastError
*/
DECLSPEC BINARY_HEADER_TYPE AbpGetProfileType(PROFILE_HANDLE profile);

/// Obtains the OS on which the specified profile was generated.
/**
\param[in] profile A profile handle created by a successful call to AbpOpenProfile.
\return The OS on which the specified profile was generated. You can call AbpGetLastError to determine the error that occured.
If the specified profile handle is not valid, BINARY_HEADER_OS_INVALID is returned.
\sa PROFILE_HANDLE, BINARY_HEADER_OS, AbpOpenProfile, AbpGetProfileOSVersion and AbpGetLastError
*/
DECLSPEC BINARY_HEADER_OS AbpGetProfileOS(PROFILE_HANDLE profile);

/// Obtains the OS version on which the specified profile was generated.
/**
\param[in] profile A profile handle created by a successful call to AbpOpenProfile.
\return The OS version on which the specified profile was generated. You can call AbpGetLastError to determine the error that occured.
If the specified profile handle is not valid, BINARY_HEADER_OS_VERSION_INVALID is returned.
\sa PROFILE_HANDLE, BINARY_HEADER_OS_VERSION, AbpOpenProfile, AbpGetProfileOS and AbpGetLastError
*/
DECLSPEC BINARY_HEADER_OS_VERSION AbpGetProfileOSVersion(PROFILE_HANDLE profile);

/// Obtains the vendor of the processor on which the specified profile was generated.
/**
\param[in] profile A profile handle created by a successful call to AbpOpenProfile.
\return The vendor of the processor on which the specified profile was generated. You can call AbpGetLastError to determine the error that occured.
If the specified profile handle is not valid, PROCESSOR_VENDOR_INVALID is returned.
\sa PROFILE_HANDLE, AbpOpenProfile, PROCESSOR_VENDOR and AbpGetLastError
*/
DECLSPEC PROCESSOR_VENDOR AbpGetProfileProcessorVendor(PROFILE_HANDLE profile);

/// Obtains the architecture of the processor on which the specified profile was generated.
/**
\param[in] profile A profile handle created by a successful call to AbpOpenProfile.
\return The architecture of the processor on which the specified profile was generated. You can call AbpGetLastError to determine the error that occured.
If the specified profile handle is not valid, PROCESSOR_ARCH_INVALID is returned.
\sa PROFILE_HANDLE, AbpOpenProfile, PROCESSOR_ARCH and AbpGetLastError
*/
DECLSPEC PROCESSOR_ARCH AbpGetProfileProcessorArch(PROFILE_HANDLE profile);

/// Obtains the architecture of the processor for which the Alleria profiler was compiled.
/**
\param[in] profile A profile handle created by a successful call to AbpOpenProfile.
\return The architecture of the processor for which the Alleria profiler was compiled. You can call AbpGetLastError to determine the error that occured.
If the specified profile handle is not valid, PROCESSOR_ARCH_INVALID is returned.
\sa PROFILE_HANDLE, AbpOpenProfile, PROCESSOR_ARCH and AbpGetLastError
*/
DECLSPEC PROCESSOR_ARCH AbpGetProfileTargetProcessorArch(PROFILE_HANDLE profile);

/// Obtains the time at which the specified profile was created.
/**
\param[in] profile A profile handle created by a successful call to AbpOpenProfile.
\return The time at which the specified profile was created. If the specified profile handle is not valid, zero is returned.
You can call AbpGetLastError to determine the error that occured.
\sa PROFILE_HANDLE, AbpOpenProfile, TIME and AbpGetLastError
*/
DECLSPEC TIME AbpGetProfileTimestamp(PROFILE_HANDLE profile);

/// Obtains the process number of the process associated with the specified profile.
/**
\param[in] profile A profile handle created by a successful call to AbpOpenProfile.
\return The process number of the process associated with the specified profile. If the specified profile handle is not valid, zero is returned.
You can call AbpGetLastError to determine the error that occured.
\sa PROFILE_HANDLE, AbpOpenProfile and AbpGetLastError
*/
DECLSPEC UINT32 AbpGetProfileProcessNumber(PROFILE_HANDLE profile);

/// Obtains the process OS ID of the process associated with the specified profile.
/**
\param[in] profile A profile handle created by a successful call to AbpOpenProfile.
\return The process OS ID of the process associated with the specified profile. If the specified profile handle is not valid, zero is returned.
You can call AbpGetLastError to determine the error that occured.
\sa PROFILE_HANDLE, AbpOpenProfile and AbpGetLastError
*/
DECLSPEC UINT32 AbpGetProfileProcessOsId(PROFILE_HANDLE profile);

/// Obtains the time at which the app that was profiled started execution.
/**
\param[in] profile A profile handle created by a successful call to AbpOpenProfile.
\return The time at which the app that was profiled started execution. If the specified profile handle is not valid, zero is returned.
You can call AbpGetLastError to determine the error that occured.
\sa PROFILE_HANDLE, AbpOpenProfile, TIME and AbpGetLastError
*/
DECLSPEC TIME AbpGetProfileStartingTime(PROFILE_HANDLE profile);

/// Obtains the time at which the app that was profiled finished execution.
/**
\param[in] profile A profile handle created by a successful call to AbpOpenProfile.
\return The time at which the app that was profiled finished execution. If the specified profile handle is not valid, zero is returned.
You can call AbpGetLastError to determine the error that occured.
\sa PROFILE_HANDLE, AbpOpenProfile, TIME and AbpGetLastError
*/
DECLSPEC TIME AbpGetProfileEndingTime(PROFILE_HANDLE profile);

/// Obtains the total number of instructions that the specified profile contains.
/**
\param[in] profile A profile handle created by a successful call to AbpOpenProfile.
\return The total number of instructions that the specified profile contains. If the specified profile handle is not valid
or if there are zero instructions in the profile, zero is returned.
You can call AbpGetLastError to determine the error that occured.
\sa PROFILE_HANDLE, AbpOpenProfile and AbpGetLastError
*/
DECLSPEC UINT64 AbpGetProfileInsCount(PROFILE_HANDLE profile);

/// Obtains the total number of memory access instructions that the specified profile contains.
/**
\param[in] profile A profile handle created by a successful call to AbpOpenProfile.
\return The total number of memory access instructions that the specified profile contains. If the specified profile handle is not valid
or if there are zero instructions in the profile, zero is returned.
You can call AbpGetLastError to determine the error that occured.
\sa PROFILE_HANDLE, AbpOpenProfile and AbpGetLastError
*/
DECLSPEC UINT64 AbpGetProfileMemInsCount(PROFILE_HANDLE profile);

/// Obtains the path of the main executable of the process associated with the specified binary profile.
/**
\param[in] profile A profile handle created by a successful call to AbpOpenProfile.
\param[out] path A pointer to buffer that is large enough to store the path of the main executable.
\param[in] size The size of the buffer specified by \a path in bytes.
\return If \a path is NULL, the function returns the size of the buffer required to hold the path of the main executable.
If \a size is smaller than the size of the path including the terminating NULL character,
the function returns the size of the buffer required to hold the path.
Otherwise if the specified profile is valid, the path string is copied to the buffer including the terminating NULL character and
the number of bytes copied is returned. If the specified profile handle is not valid, zero is returned.
You can call AbpGetLastError to determine the error that occured.
\sa PROFILE_HANDLE, AbpOpenProfile and AbpGetLastError
*/
DECLSPEC UINT32 AbpGetProfileMainExePath(PROFILE_HANDLE profile, PSTR path, UINT32 size);

/// Obtains the system directory on which the process associated with the specified binary profile was running.
/**
\param[in] profile A profile handle created by a successful call to AbpOpenProfile.
\param[out] dir A pointer to buffer that is large enough to store the system directory.
\param[in] size The size of the buffer specified by \a dir in bytes.
\return If \a dir is NULL, the function returns the size of the buffer required to hold the system directory.
If \a size is smaller than the size of the system directory including the terminating NULL character,
the function returns the size of the buffer required to hold the directory.
Otherwise if the specified profile is valid, the system directory string is copied to the buffer including the terminating NULL character and
the number of bytes copied is returned. If the specified profile handle is not valid, zero is returned.
You can call AbpGetLastError to determine the error that occured.
\sa PROFILE_HANDLE, AbpOpenProfile and AbpGetLastError
*/
DECLSPEC UINT32 AbpGetProfileSystemDirectory(PROFILE_HANDLE profile, PSTR dir, UINT32 size);

/// Obtains the current directory of the process associated with the specified binary profile.
/**
\param[in] profile A profile handle created by a successful call to AbpOpenProfile.
\param[out] dir A pointer to buffer that is large enough to store the current directory.
\param[in] size The size of the buffer specified by \a dir in bytes.
\return If \a dir is NULL, the function returns the size of the buffer required to hold the current directory.
If \a size is smaller than the size of the current directory including the terminating NULL character,
the function returns the size of the buffer required to hold the directory.
Otherwise if the specified profile is valid, the current directory string is copied to the buffer including the terminating NULL character and
the number of bytes copied is returned. If the specified profile handle is not valid, zero is returned.
You can call AbpGetLastError to determine the error that occured.
\sa PROFILE_HANDLE, AbpOpenProfile and AbpGetLastError
*/
DECLSPEC UINT32 AbpGetProfileCurrentDirectory(PROFILE_HANDLE profile, PSTR dir, UINT32 size);

/// Obtains the command line of the process associated with the specified binary profile.
/**
\param[in] profile A profile handle created by a successful call to AbpOpenProfile.
\param[out] cmdLine A pointer to buffer that is large enough to store the command line.
\param[in] size The size of the buffer specified by \a cmdLine in bytes.
\return If \a cmdLine is NULL, the function returns the size of the buffer required to hold the command line.
If \a size is smaller than the size of the command line including the terminating NULL character,
the function returns the size of the buffer required to hold the command line.
Otherwise if the specified profile is valid, the command line string is copied to the buffer including the terminating NULL character and
the number of bytes copied is returned. If the specified profile handle is not valid, zero is returned.
You can call AbpGetLastError to determine the error that occured.
\sa PROFILE_HANDLE, AbpOpenProfile and AbpGetLastError
*/
DECLSPEC UINT32 AbpGetProfileCmdLine(PROFILE_HANDLE profile, PSTR cmdLine, UINT32 size);

/// Obtains the size of any virtual address in the specified profile.
/**
\param[in] profile A profile handle created by a successful call to AbpOpenProfile.
\return The size of any virtual address in the specified profile. If the specified profile handle is not valid
or if the profile does not contain any virtual addresses, zero is returned.
You can call AbpGetLastError to determine the error that occured.
\sa PROFILE_HANDLE, AbpOpenProfile and AbpGetLastError
*/
DECLSPEC UINT8 AbpGetProfileVirtualAddrSize(PROFILE_HANDLE profile);

/// Obtains the size of any physical address in the specified profile.
/**
\param[in] profile A profile handle created by a successful call to AbpOpenProfile.
\return The size of any physical address in the specified profile. If the specified profile handle is not valid
or if the profile does not contain any physical addresses, zero is returned.
You can call AbpGetLastError to determine the error that occured.
\sa PROFILE_HANDLE, AbpOpenProfile and AbpGetLastError
*/
DECLSPEC UINT8 AbpGetProfilePhysicalAddrSize(PROFILE_HANDLE profile);

/// Obtains the standard size of a virtual page in the specified profile.
/**
A process' virtual address space is divided into virtual pages. It's possible that not all
of the virtual pages have the same size. However, usually, many or all pages have the same size
which is referred to here as the standard size.
\param[in] profile A profile handle created by a successful call to AbpOpenProfile.
\return The standard size of a virtual page in the specified profile. If the specified profile handle is not valid, zero is returned.
You can call AbpGetLastError to determine the error that occured.
\sa PROFILE_HANDLE, AbpOpenProfile and AbpGetLastError
*/
DECLSPEC UINT32 AbpGetProfilePageSize(PROFILE_HANDLE profile);

/// Obtains the smallest virtual address that can be used by the profiled app associated with the specified profile.
/**
A process' virtual address space is divided into two partitions. The user-mode partitional and
the kernel-mode partition. On some operating systems, one or more pages from the bottom and top of
the user-mode partition are reserved by the OS and cannot be used by the app. This function returns
the smallest virtual address that can be used by the app.
\param[in] profile A profile handle created by a successful call to AbpOpenProfile.
\return The smallest virtual address. If the specified profile handle is not valid or if
the smallest virtual address is zero, zero is returned.
You can call AbpGetLastError to determine the error that occured.
\sa PROFILE_HANDLE, AbpOpenProfile and AbpGetLastError
*/
DECLSPEC UINT64 AbpGetProfileMinAppAddr(PROFILE_HANDLE profile);

/// Obtains the largest virtual address that can be used by the profiled app associated with the specified profile.
/**
A process' virtual address space is divided into two partitions. The user-mode partitional and
the kernel-mode partition. On some operating systems, one or more pages from the bottom and top of
the user-mode partition are reserved by the OS and cannot be used by the app. This function returns
the largest virtual address that can be used by the app.
\param[in] profile A profile handle created by a successful call to AbpOpenProfile.
\return The largest virtual address. If the specified profile handle is not valid, zero is returned.
You can call AbpGetLastError to determine the error that occured.
\sa PROFILE_HANDLE, AbpOpenProfile and AbpGetLastError
*/
DECLSPEC UINT64 AbpGetProfileMaxAppAddr(PROFILE_HANDLE profile);

/// Obtains the total size of main memory of the system on which the specified profile was generated.
/**
On some platforms, it might be possible to add more DRAM to the system without restarting. If this happened, during
the profiling session, the value returned by this function cannot be reliably used.
\param[in] profile A profile handle created by a successful call to AbpOpenProfile.
\return The total size of main memory. If the specified profile handle is not valid, zero is returned.
You can call AbpGetLastError to determine the error that occured.
\sa PROFILE_HANDLE, AbpOpenProfile and AbpGetLastError
*/
DECLSPEC UINT32 AbpGetProfileTotalDram(PROFILE_HANDLE profile);

/// Returns a boolean value indicating whether the specified profile contains instructions or not.
/**
\param[in] profile A profile handle created by a successful call to AbpOpenProfile.
\return TRUE if the profile contains instructions. FALSE, otherwise. If the specified profile handle is not valid, zero is returned.
You can call AbpGetLastError to determine the error that occured.
\sa PROFILE_HANDLE, BINARY_PROFILE_SECTION_ENTRY_INS, AbpOpenProfile and AbpGetLastError
*/
DECLSPEC BOOL AbpDoesProfileContainInstructions(PROFILE_HANDLE profile);

/// Returns a boolean value indicating whether the specified profile contains scheduling information or not.
/**
\param[in] profile A profile handle created by a successful call to AbpOpenProfile.
\return TRUE if the profile contains scheduling information. FALSE, otherwise. If the specified profile handle is not valid, zero is returned.
You can call AbpGetLastError to determine the error that occured.
\sa PROFILE_HANDLE, BINARY_PROFILE_SECTION_ENTRY_PROC, AbpOpenProfile and AbpGetLastError
*/
DECLSPEC BOOL AbpDoesProfileContainSchedulingInfo(PROFILE_HANDLE profile);

/// Returns the number of entries in the section directory of the specified profile.
/**
\param[in] profile A profile handle created by a successful call to AbpOpenProfile.
\return The number of entries in the section directory of the specified profile. If the specified profile handle is not valid, zero is returned.
You can call AbpGetLastError to determine the error that occured.
\sa PROFILE_HANDLE, SECTION_DIRECTORY_ENTRY, AbpOpenProfile, AbpGetProfileSectionDirectoryEntry and AbpGetLastError
*/
DECLSPEC UINT32 AbpGetProfileSectionDirectorySize(PROFILE_HANDLE profile);

/// Returns the frequency of the timestamps associated with instructions.
/**
\param[in] profile A profile handle created by a successful call to AbpOpenProfile.
\return The frequency of the timestamps associated with instructions. If the specified profile handle is not valid, zero is returned.
You can call AbpGetLastError to determine the error that occured.
\sa PROFILE_HANDLE, BINARY_PROFILE_SECTION_ENTRY_INS, AbpOpenProfile and AbpGetLastError
*/
DECLSPEC UINT64 AbpGetInstructionTimestampFrequency(PROFILE_HANDLE profile);

/*

APIs for reading from the section directory section.

*/

/// Returns the entry at the specified index in the section directory of the specified profile.
/**
\param[in] profile A profile handle created by a successful call to AbpOpenProfile.
\param[in] index The index of the entry in the section directory to return.
\param[out] entry A pointer to a section directory entry object.
\return TRUE if the object pointed to by \a entry was filled successfully. If the specified profile handle is not valid,
\a index is larger than the largest valid index or \a entry is NULL, FALSE is returned.
You can call AbpGetLastError to determine the error that occured.
\sa PROFILE_HANDLE, SECTION_DIRECTORY_ENTRY, AbpOpenProfile, AbpGetProfileSectionDirectorySize and AbpGetLastError
*/
DECLSPEC BOOL AbpGetProfileSectionDirectoryEntry(PROFILE_HANDLE profile, UINT32 index, PSECTION_DIRECTORY_ENTRY entry);

/*

APIs for reading from a CpuInfo section.

*/

/// Obtains all available information about a processor package on which the profiled process was running.
/**
\param[in] profile A profile handle created by a successful call to AbpOpenProfile.
\param[in] secIndex The index of the CpuId section in the section directory.
\param[in] cpuId The index of the processor package.
\param[out] cpuInfo A pointer to an array of \a CPUID_INFO objects containing information about the specified processor package.
\param[in] extended TRUE to return extended information. FALSE, otherwise.
\return The number of elements in \a cpuInfo if the object pointed to by \a cpuInfo was filled successfully. If the specified profile handle, section index or
processor package index is not valid or if \a cpuInfo is NULL, zero is returned.
You can call AbpGetLastError to determine the error that occured.
\sa PROFILE_HANDLE, CPUID_INFO, AbpOpenProfile, AbpGetCpuCount, AbpCpuId and AbpGetLastError
*/
DECLSPEC UINT32 AbpGetCpuInfo(PROFILE_HANDLE profile, UINT32 secIndex, UINT32 cpuId, PCPUID_INFO cpuInfo, BOOL extended);

/// Obtains specific information about a processor package on which the profiled process was running.
/**
\param[in] profile A profile handle created by a successful call to AbpOpenProfile.
\param[in] secIndex The index of the CpuId section in the section directory.
\param[in] cpuId The index of the processor package.
\param[in,out] cpuInfo A pointer to an object of type \a CPUID_INFO containing information about the specified processor package.
The fields EAX and ECX must be initialized before calling the function.
\return TRUE if the object pointed to by \a cpuInfo was filled successfully. If the specified profile handle, section index or
processor package index is not valid or if \a cpuInfo is NULL, FALSE is returned.
You can call AbpGetLastError to determine the error that occured.
\sa PROFILE_HANDLE, CPUID_INFO, AbpOpenProfile, AbpGetCpuCount, AbpGetCpuInfo, AbpGetProfileProcessorVendor and AbpGetLastError
*/
DECLSPEC BOOL AbpCpuId(PROFILE_HANDLE profile, UINT32 secIndex, UINT32 cpuId, PCPUID_INFO cpuInfo);

/// Obtains the number of processor packages on which the profiled process was running.
/**
\param[in] profile A profile handle created by a successful call to AbpOpenProfile.
\param[in] secIndex The index of the CpuId section in the section directory.
\return If successful, returns the number of processor packages. If the specified profile handle or section index is invalid, FALSE is returned.
You can call AbpGetLastError to determine the error that occured.
\sa PROFILE_HANDLE, CPUID_INFO, AbpOpenProfile, AbpCpuId, AbpGetCpuInfo and AbpGetLastError
*/
DECLSPEC UINT32 AbpGetCpuCount(PROFILE_HANDLE profile, UINT32 secIndex);


/*

APIs for reading from a ProcFam section.

*/

/// Obtains information about the process family to which the profiled process that is associated with the specified profile belongs.
/**
\param[in] profile A profile handle created by a successful call to AbpOpenProfile.
\param[in] secIndex The index of the ProcFam section in the section directory.
\param[out] processes A pointer to an array of BINARY_PROCESS_FAMILY_ENTRY objects containing information about the process family.
Each entry in the array represents a single process.
\return If successful, returns the number of elements of the aray pointed to by \a processes. If the specified profile handle or section index is invalid
or if \a processes is NULL, zero is returned.
You can call AbpGetLastError to determine the error that occured.
\sa PROFILE_HANDLE, BINARY_PROCESS_FAMILY_ENTRY, AbpOpenProfile, AbpGetProcessWithNumber, AbpGetProcessWithId and AbpGetLastError
*/
DECLSPEC UINT32 AbpGetProcessFamily(PROFILE_HANDLE profile, UINT32 secIndex, PBINARY_PROCESS_FAMILY_ENTRY processes);

/// Obtains information about a single process in the process family using its process number.
/**
\param[in] profile A profile handle created by a successful call to AbpOpenProfile.
\param[in] secIndex The index of the ProcFam section in the section directory.
\param[in] pn The process number.
\param[out] processes A pointer to an object of type BINARY_PROCESS_FAMILY_ENTRY containing information about the process.
\return TRUE if successful. If the specified profile handle, section index or process number is invalid
or if \a process is NULL, FALSE is returned.
You can call AbpGetLastError to determine the error that occured.
\sa PROFILE_HANDLE, BINARY_PROCESS_FAMILY_ENTRY, AbpOpenProfile, AbpGetProcessFamily, AbpGetProcessWithId and AbpGetLastError
*/
DECLSPEC BOOL AbpGetProcessWithNumber(PROFILE_HANDLE profile, UINT32 secIndex, UINT32 pn, PBINARY_PROCESS_FAMILY_ENTRY process);

/// Obtains information about a single process in the process family using its OS ID.
/**
\param[in] profile A profile handle created by a successful call to AbpOpenProfile.
\param[in] secIndex The index of the ProcFam section in the section directory.
\param[in] pid The process OS ID.
\param[out] processes A pointer to an object of type BINARY_PROCESS_FAMILY_ENTRY containing information about the process.
\return TRUE if successful. If the specified profile handle, section index or process OS ID is invalid
or if \a process is NULL, FALSE is returned.
You can call AbpGetLastError to determine the error that occured.
\sa PROFILE_HANDLE, BINARY_PROCESS_FAMILY_ENTRY, AbpOpenProfile, AbpGetProcessFamily, AbpGetProcessWithNumber and AbpGetLastError
*/
DECLSPEC BOOL AbpGetProcessWithId(PROFILE_HANDLE profile, UINT32 secIndex, UINT32 pid, PBINARY_PROCESS_FAMILY_ENTRY process);

/*

APIs for reading from a Profile section.

*/

/// Gets the first entry in the specified Profile section of the specified profile.
/**
\param[in] profile A profile handle created by a successful call to AbpOpenProfile.
\param[in] secIndex The index of the Profile section in the section directory.
\param[out] profileEntry A pointer to an object of type BINARY_PROFILE_SECTION_GENERIC containing information about the entry.
\return TRUE if successful. If the specified profile handle or section index is invalid
or if \a profileEntry is NULL, FALSE is returned.
You can call AbpGetLastError to determine the error that occured.
\sa PROFILE_HANDLE, BINARY_PROFILE_SECTION_GENERIC, BINARY_PROFILE_SECTION_ENTRY_TYPE, AbpOpenProfile, AbpGetProfileEntryNext, AbpGetProfileEntryNextFast, AbpDestroyProfileEntry and AbpGetLastError
*/
DECLSPEC BOOL AbpGetProfileEntryFirst(PROFILE_HANDLE profile, UINT32 secIndex, PBINARY_PROFILE_SECTION_GENERIC profileEntry);

/// Gets the next entry in the specified Profile section of the specified profile.
/**
This API is part of the elementary iteration API. Returns the next entry in storage order.
\param[in] profile A profile handle created by a successful call to AbpOpenProfile.
\param[in] secIndex The index of the Profile section in the section directory.
\param[in] inProfileEntry A pointer to an object of type BINARY_PROFILE_SECTION_GENERIC representing the current entry.
\param[out] outProfileEntry A pointer to an object of type BINARY_PROFILE_SECTION_GENERIC representing the next entry.
\return TRUE if successful. If the specified profile handle or section index is invalid
or if \a inProfileEntry is NULL or the last entry in the Profile section, FALSE is returned.
You can call AbpGetLastError to determine the error that occured.
\sa PROFILE_HANDLE, BINARY_PROFILE_SECTION_GENERIC, BINARY_PROFILE_SECTION_ENTRY_TYPE, AbpOpenProfile, AbpGetProfileEntryFirst, AbpDestroyProfileEntry and AbpGetLastError
*/
DECLSPEC BOOL AbpGetProfileEntryNext(PROFILE_HANDLE profile, UINT32 secIndex, PBINARY_PROFILE_SECTION_GENERIC inProfileEntry, PBINARY_PROFILE_SECTION_GENERIC outProfileEntry);

/// Gets the next entry in the specified Profile section of the specified profile.
/**
This API is part of the elementary iteration API. This API is a much faster (x9) version than AbpGetProfileEntryNext,
but it skips checking for certain errors. In addition, it assumes that the profile file is already in the correct 
position to read the next entry. This condition is satisfied when using this API consecutively. So use it carefully.
\param[in] profile A profile handle created by a successful call to AbpOpenProfile.
\param[in] secIndex The index of the Profile section in the section directory.
\param[in] inProfileEntry A pointer to an object of type BINARY_PROFILE_SECTION_GENERIC representing the current entry.
\param[out] outProfileEntry A pointer to an object of type BINARY_PROFILE_SECTION_GENERIC representing the next entry.
\return TRUE if successful. Otherwise, it may fail silently or corrupt the state.
\sa PROFILE_HANDLE, BINARY_PROFILE_SECTION_GENERIC, BINARY_PROFILE_SECTION_ENTRY_TYPE, AbpOpenProfile, AbpGetProfileEntryFirst, AbpGetProfileEntryNext, AbpDestroyProfileEntry and AbpGetLastError
*/
DECLSPEC
BOOL AbpGetProfileEntryNextFast(PROFILE_HANDLE profile, UINT32 secIndex,
	PBINARY_PROFILE_SECTION_GENERIC inProfileEntry, PBINARY_PROFILE_SECTION_GENERIC outProfileEntry);

// Destroys the specified BINARY_PROFILE_SECTION_GENERIC object and releases all resources allocated for the object.
/**
\param[in] profile A profile handle created by a successful call to AbpOpenProfile.
\param[in] profileEntry A pointer to an object of type BINARY_PROFILE_SECTION_GENERIC to be destroyed.
\return TRUE if successful. If \a profileEntry is NULL or invalid, FALSE is returned.
You can call AbpGetLastError to determine the error that occured.
\sa AbpGetProfileEntryFirst, AbpGetProfileEntryNext and AbpGetLastError
*/
DECLSPEC BOOL AbpDestroyProfileEntry(PROFILE_HANDLE profile, PBINARY_PROFILE_SECTION_GENERIC profileEntry);


/// Gets the first trace in the specified Profile section of the specified profile.
/** 
A trace contains information on executed instructions that were profiled together.
\param[in] profile A profile handle created by a successful call to AbpOpenProfile.
\param[in] secIndex The index of the Profile section in the section directory.
\param[out] profileTrace A pointer to an object of type BINARY_PROFILE_SECTION_GENERIC containing information about the trace.
\return TRUE if successful. If the specified profile handle or section index is invalid
or if \a profileTrace is NULL, FALSE is returned.
You can call AbpGetLastError to determine the error that occured.
\sa PROFILE_HANDLE, BINARY_PROFILE_SECTION_TRACE_GENERIC, BINARY_PROFILE_SECTION_ENTRY_TYPE, AbpOpenProfile, AbpGetProfileTraceNext, AbpDestroyProfileTrace and AbpGetLastError
*/
DECLSPEC BOOL AbpGetProfileTraceFirst(PROFILE_HANDLE profile, UINT32 secIndex, PBINARY_PROFILE_SECTION_TRACE_GENERIC profileTrace);

/// Gets the next trace in the specified Profile section of the specified profile.
/**
This API is part of the trace iteration API. Returns the next trace in storage order.
\param[in] profile A profile handle created by a successful call to AbpOpenProfile.
\param[in] secIndex The index of the Profile section in the section directory.
\param[in] inProfileTrace A pointer to an object of type BINARY_PROFILE_SECTION_GENERIC representing the current trace.
\param[out] outProfileTrace A pointer to an object of type BINARY_PROFILE_SECTION_GENERIC representing the next trace.
\return TRUE if successful. If the specified profile handle or section index is invalid
or if \a inProfileTrace is NULL or the last trace in the Profile section, FALSE is returned.
You can call AbpGetLastError to determine the error that occured.
\sa PROFILE_HANDLE, BINARY_PROFILE_SECTION_TRACE_GENERIC, BINARY_PROFILE_SECTION_ENTRY_TYPE, AbpOpenProfile, AbpGetProfileTraceFirst, AbpDestroyProfileTrace and AbpGetLastError
*/
DECLSPEC BOOL AbpGetProfileTraceNext(PROFILE_HANDLE profile, UINT32 secIndex, PBINARY_PROFILE_SECTION_TRACE_GENERIC inProfileTrace, PBINARY_PROFILE_SECTION_TRACE_GENERIC outProfileTrace);

// Destroys the specified BINARY_PROFILE_SECTION_GENERIC trace and releases all resources allocated for the trace.
/**
\param[in] profile A profile handle created by a successful call to AbpOpenProfile.
\param[in] profileTrace A pointer to an object of type BINARY_PROFILE_SECTION_TRACE_GENERIC to be destroyed.
\return TRUE if successful. If \a profileTrace is NULL or invalid, FALSE is returned.
You can call AbpGetLastError to determine the error that occured.
\sa BINARY_PROFILE_SECTION_TRACE_GENERIC, AbpGetProfileTraceFirst, AbpGetProfileTraceNext and AbpGetLastError
*/
DECLSPEC BOOL AbpDestroyProfileTrace(PROFILE_HANDLE profile, PBINARY_PROFILE_SECTION_TRACE_GENERIC profileTrace);

/*

APIs for handling errors.

*/

/// Returns the error code of the error occurred in most recent failed call to an ABP API.
/**
\return The error code.
\sa ABP_ERROR, AbpLastOperationFailed and GetErrorString
*/
DECLSPEC ABP_ERROR AbpGetLastError();

/// Returns TRUE if the most recent ABP API call succeeded. Otherwise, FALSE.
/**
\return TRUE if the most recent ABP API call succeeded. Otherwise, FALSE.
\sa ABP_ERROR, AbpGetLastError and GetErrorString
*/
DECLSPEC BOOL AbpLastOperationFailed();

/// Returns a string representation of the specified error code.
/**
\param e The error code.
\return A pointer to a null-terminated ASCII string representing the specified error.
If the specified error code is invalid, NULL is returned.
\sa ABP_ERROR, AbpGetLastError and AbpLastOperationFailed
*/
DECLSPEC PCSTR GetErrorString(ABP_ERROR e);

/*

APIs for reading from an LLW section.

*/

DECLSPEC BOOL AbpGetLlwSnapshotFirst(
	PROFILE_HANDLE profile, 
	UINT32 secIndex, 
	PBINARY_LLW_SECTION_ENTRY_SNAPSHOT snapshot);

DECLSPEC BOOL AbpGetLlwSnapshotNext(
	PROFILE_HANDLE profile, 
	UINT32 secIndex, 
	PBINARY_LLW_SECTION_ENTRY_SNAPSHOT inSnapshot, 
	PBINARY_LLW_SECTION_ENTRY_SNAPSHOT outSnapshot);

DECLSPEC BOOL AbpDestroyLlwSnapshot(
	PBINARY_LLW_SECTION_ENTRY_SNAPSHOT snapshot);

/*

APIs for reading from an Interceptors section.

*/


/*

APIs for reading from a Func section.

*/


/*

APIs for reading from an Img section.

*/


#ifdef __cplusplus
}
#endif

#endif /* ABP_API_H */