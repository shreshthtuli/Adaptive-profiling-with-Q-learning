#include "AbpInternalTypes.h"
#include "AbpGlobals.h"
#include "AbpApi.h"
#include "DynamicArray.h"
#include "xed-interface.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// TODO: implement AbpCpuId, AbpGetLastError, AbpLastOperationFailed, GetErrorString, func, img, and interceptor APIs.
// TODO: measure and improve the efficiency of the profile APIs.
// TODO: provide a fast trace API like the fast elementary API.
// TODO: check for integr arithmetic errors on 32-bit and 64-bit.

#ifdef TARGET_WINDOWS
#define FSEEK64(fp, offset, whence) _fseeki64(fp, (long long)offset, whence)
#elif defined(TARGET_LINUX)
#define FSEEK64(fp, offset, whence) fseeko64(fp, (off64_t)offset, whence)
#endif

#define FReadOpenProfile(dataMember, file) \
if (fread(&dataMember, sizeof(dataMember), 1, file) != 1) \
goto cleanupfail;

INTERNAL_FUNC
PROFILE_HANDLE_TABLE_SIZE GetFreeSlotIndex()
{
	PROFILE_HANDLE_TABLE_SIZE result = INVALID_HANDLE_TABLE_INDEX;
	PROFILE_HANDLE_TABLE_SIZE count = 0;

	while (g_profileHandles[g_profileHandlesTop] != NULL && count < PROFILE_MAX)
	{
		++g_profileHandlesTop;
		if (g_profileHandlesTop == PROFILE_MAX)
			g_profileHandlesTop = 0;
		++count;
	}

	if (g_profileHandles[g_profileHandlesTop] == NULL)
	{
		result = g_profileHandlesTop;
		++g_profileHandlesTop;
		if (g_profileHandlesTop == PROFILE_MAX)
			g_profileHandlesTop = 0;
	}

	return result;
}

INTERNAL_FUNC
BOOL IsVersionSupported(const char *version)
{
	return strcmp(version, ABP_VERSION_1_0) == 0;
}

INTERNAL_FUNC
BOOL ReadNullTerminatedString(FILE *file, PSTR *buffer)
{
	fpos_t pos;
	fgetpos(file, &pos);
	size_t length = (size_t)- 1; // Number of chars excluding NUL.
	char ch;
	do
	{
		++length;
		ch = fgetc(file);
	} while (ch != EOF && ch != '\0');

	if (ch == EOF)
		return FALSE;

	fsetpos(file, &pos);
	++length; // Add NUL.
	*buffer = malloc(length);
	size_t index = 0;
	while (index < length)
		(*buffer)[index++] = fgetc(file);

	return TRUE;
}

INTERNAL_FUNC
void FreeProfileInternals(PPROFILE_INTERNALS pi)
{
	if (pi == NULL)
		return;

	if (pi->header.mainExePath != NULL)
	{
		free(pi->header.mainExePath);
		pi->header.mainExePath = NULL;
	}
	if (pi->header.systemDirectory != NULL)
	{
		free(pi->header.systemDirectory);
		pi->header.systemDirectory = NULL;
	}
	if (pi->header.currentDirectory != NULL)
	{
		free(pi->header.currentDirectory);
		pi->header.currentDirectory = NULL;
	}
	if (pi->header.cmdLine != NULL)
	{
		free(pi->header.cmdLine);
		pi->header.cmdLine = NULL;
	}
	if (pi->header.osVersion != NULL)
	{
		free(pi->header.osVersion);
		pi->header.osVersion = NULL;
	}
	if (pi->profilePath != NULL)
	{
		free(pi->profilePath);
		pi->profilePath = NULL;
	}
	if (pi->secDir != NULL)
	{
		free(pi->secDir);
		pi->secDir = NULL;
	}
	free(pi);
}

INTERNAL_FUNC
void ParseSecDirEntry(SECTION_DIRECTORY_ENTRY_INTERNAL secDirInternal, PSECTION_DIRECTORY_ENTRY secDir)
{
	secDir->reserved = secDirInternal.data >> 8;
	secDir->type = secDirInternal.data & 0xFF;
}

DECLSPEC
PROFILE_HANDLE AbpOpenProfile(PCSTR path)
{
	// Needs to be called once per process to initialize Intel XED.
	// Calling it multiple times doesn't hurt.
	xed_tables_init();

	PROFILE_HANDLE_TABLE_SIZE index = GetFreeSlotIndex();

	if (index == INVALID_HANDLE_TABLE_INDEX)
		return INVALID_PROFILE_HANDLE;

	FILE *profileFile;
	PROFILE_HANDLE handle = INVALID_PROFILE_HANDLE;

	fopen_s(&profileFile, path, "rb");
	if (!profileFile)
		goto end;

	char id[ABP_ID_SIZE + 1];
	if (fread(id, ABP_ID_SIZE + 1, 1, profileFile) != 1)
		goto end;

	if (strcmp(id, ABP_ID) != 0)
		goto end;

	if (fread(id, ABP_VERSION_1_0_SIZE + 1, 1, profileFile) != 1)
		goto end;

	if (!IsVersionSupported(id))
		goto end;

	PPROFILE_INTERNALS pi = calloc(1, sizeof(PROFILE_INTERNALS));
	pi->file = profileFile;
	pi->profilePath = malloc(strlen(path) + 1);
	strcpy_s(pi->profilePath, strlen(path) + 1, path);
	g_profileHandles[index] = pi;

	pi->objectPool.branchValid = FALSE;
	pi->objectPool.headerValid = FALSE;
	pi->objectPool.insValid = FALSE;
	pi->objectPool.memValid = FALSE;
	pi->objectPool.procValid = FALSE;
	pi->objectPool.sysCallValid = FALSE;

	FReadOpenProfile(pi->header.type, profileFile);
	FReadOpenProfile(pi->header.os, profileFile);
	FReadOpenProfile(pi->header.processorVendor, profileFile);
	FReadOpenProfile(pi->header.processorArch, profileFile);
	FReadOpenProfile(pi->header.targetArch, profileFile);
	FReadOpenProfile(pi->header.timestamp, profileFile);
	FReadOpenProfile(pi->header.processNumber, profileFile);
	FReadOpenProfile(pi->header.processOsId, profileFile);
	FReadOpenProfile(pi->header.timeFreq, profileFile);
	FReadOpenProfile(pi->header.startingTime, profileFile);
	FReadOpenProfile(pi->header.endingTime, profileFile);
	FReadOpenProfile(pi->header.insCount, profileFile);
	FReadOpenProfile(pi->header.memInsCount, profileFile);
	if(!ReadNullTerminatedString(profileFile, &pi->header.mainExePath))
		goto cleanupfail;
	if (!ReadNullTerminatedString(profileFile, &pi->header.systemDirectory))
		goto cleanupfail;
	if (!ReadNullTerminatedString(profileFile, &pi->header.currentDirectory))
		goto cleanupfail;
	if (!ReadNullTerminatedString(profileFile, &pi->header.cmdLine))
		goto cleanupfail;
	if (!ReadNullTerminatedString(profileFile, &pi->header.osVersion))
		goto cleanupfail;
	FReadOpenProfile(pi->header.vAddrSize, profileFile);
	FReadOpenProfile(pi->header.pAddrSize, profileFile);
	FReadOpenProfile(pi->header.pageSize, profileFile);
	pi->header.minAppAddr = 0;
	pi->header.maxAppAddr = 0;
	if (fread(&pi->header.minAppAddr, pi->header.vAddrSize, 1, profileFile) != 1)
		goto cleanupfail;
	if (fread(&pi->header.maxAppAddr, pi->header.vAddrSize, 1, profileFile) != 1)
		goto cleanupfail;
	FReadOpenProfile(pi->header.totalDram, profileFile);
	FReadOpenProfile(pi->header.flags, profileFile);
	FReadOpenProfile(pi->header.reserved, profileFile);
	FReadOpenProfile(pi->header.dirSize, profileFile);
	pi->secDir = malloc(sizeof(SECTION_DIRECTORY_ENTRY)*pi->header.dirSize);
	SECTION_DIRECTORY_ENTRY_INTERNAL temp;
	for (UINT32 i = 0; i < pi->header.dirSize; ++i)
	{
		if (fread(&temp, sizeof(SECTION_DIRECTORY_ENTRY_INTERNAL), 1, profileFile) != 1)
			goto cleanupfail;
		ParseSecDirEntry(temp, &pi->secDir[i]);
	}
	handle = (PROFILE_HANDLE)index;
end:
	return handle;
cleanupfail:
	g_profileHandles[index] = NULL;
	FreeProfileInternals(pi);
	pi = NULL;
	goto end;
}

INTERNAL_FUNC
BOOL IsProfileHandleValid(PROFILE_HANDLE profile)
{
	if (profile == INVALID_PROFILE_HANDLE || profile < 0 || profile >= PROFILE_MAX || g_profileHandles[profile] == NULL)
		return FALSE;
	return TRUE;
}

INTERNAL_FUNC
void* ProfileObjectPoolAlloc(PROFILE_HANDLE profile, BINARY_PROFILE_SECTION_ENTRY_TYPE_INTERNAL type)
{
	// Assumes profile is valid.
	UINT32 size = 0;
	switch (type)
	{
	case BINARY_PROFILE_SECTION_ENTRY_TYPE_INTERNAL_HEADER:
		if (!g_profileHandles[profile]->objectPool.headerValid)
		{
			g_profileHandles[profile]->objectPool.headerValid = TRUE;
			return &g_profileHandles[profile]->objectPool.header;
		}
		size = sizeof(BINARY_PROFILE_SECTION_ENTRY_HEADER);
		break;
	case BINARY_PROFILE_SECTION_ENTRY_TYPE_INTERNAL_PROCESSOR:
		if (!g_profileHandles[profile]->objectPool.procValid)
		{
			g_profileHandles[profile]->objectPool.procValid = TRUE;
			return &g_profileHandles[profile]->objectPool.proc;
		}
		size = sizeof(BINARY_PROFILE_SECTION_ENTRY_PROC);
		break;
	case BINARY_PROFILE_SECTION_ENTRY_TYPE_INTERNAL_INSTRUCTION_0:
	case BINARY_PROFILE_SECTION_ENTRY_TYPE_INTERNAL_INSTRUCTION_1:
	case BINARY_PROFILE_SECTION_ENTRY_TYPE_INTERNAL_INSTRUCTION_2:
		if (!g_profileHandles[profile]->objectPool.insValid)
		{
			g_profileHandles[profile]->objectPool.insValid = TRUE;
			return &g_profileHandles[profile]->objectPool.ins;
		}
		size = sizeof(BINARY_PROFILE_SECTION_ENTRY_INS);
		break;
	case BINARY_PROFILE_SECTION_ENTRY_TYPE_INTERNAL_MEMORY_0:
	case BINARY_PROFILE_SECTION_ENTRY_TYPE_INTERNAL_VGATHER_0:
	case BINARY_PROFILE_SECTION_ENTRY_TYPE_INTERNAL_XSAVE_0:
	case BINARY_PROFILE_SECTION_ENTRY_TYPE_INTERNAL_MEMORY_1:
	case BINARY_PROFILE_SECTION_ENTRY_TYPE_INTERNAL_VGATHER_1:
	case BINARY_PROFILE_SECTION_ENTRY_TYPE_INTERNAL_XSAVE_1:
	case BINARY_PROFILE_SECTION_ENTRY_TYPE_INTERNAL_MEMORY_2:
		if (!g_profileHandles[profile]->objectPool.memValid)
		{
			g_profileHandles[profile]->objectPool.memValid = TRUE;
			return &g_profileHandles[profile]->objectPool.mem;
		}
		size = sizeof(BINARY_PROFILE_SECTION_ENTRY_MEM);
		break;
	case BINARY_PROFILE_SECTION_ENTRY_TYPE_INTERNAL_SYS_CALL_RECORD:
		if (!g_profileHandles[profile]->objectPool.sysCallValid)
		{
			g_profileHandles[profile]->objectPool.sysCallValid = TRUE;
			return &g_profileHandles[profile]->objectPool.sysCall;
		}
		size = sizeof(BINARY_PROFILE_SECTION_ENTRY_SYSCALL);
		break;
	case BINARY_PROFILE_SECTION_ENTRY_TYPE_INTERNAL_BRANCH_RECORD:
		if (!g_profileHandles[profile]->objectPool.branchValid)
		{
			g_profileHandles[profile]->objectPool.branchValid = TRUE;
			return &g_profileHandles[profile]->objectPool.branch;
		}
		size = sizeof(BINARY_PROFILE_SECTION_ENTRY_BRANCH);
		break;
	default: // BINARY_PROFILE_SECTION_ENTRY_TYPE_INTERNAL_INVALID
		return NULL;
	}

	if (size == 0)
		return NULL;

	return calloc(1, size);
}

INTERNAL_FUNC
BOOL ProfileObjectPoolDeallocInternal(PROFILE_HANDLE profile, 
	BINARY_PROFILE_SECTION_ENTRY_TYPE_INTERNAL type, void *obj)
{
	// Assumes profile and obj are valid.
	switch (type)
	{
	case BINARY_PROFILE_SECTION_ENTRY_TYPE_INTERNAL_HEADER:
		if (g_profileHandles[profile]->objectPool.headerValid &&
			&g_profileHandles[profile]->objectPool.header == obj)
		{
			g_profileHandles[profile]->objectPool.headerValid = FALSE;
			return TRUE;
		}
		break;
	case BINARY_PROFILE_SECTION_ENTRY_TYPE_INTERNAL_PROCESSOR:
		if (g_profileHandles[profile]->objectPool.procValid &&
			&g_profileHandles[profile]->objectPool.proc == obj)
		{
			g_profileHandles[profile]->objectPool.procValid = FALSE;
			return TRUE;
		}
		break;
	case BINARY_PROFILE_SECTION_ENTRY_TYPE_INTERNAL_INSTRUCTION_0:
	case BINARY_PROFILE_SECTION_ENTRY_TYPE_INTERNAL_INSTRUCTION_1:
	case BINARY_PROFILE_SECTION_ENTRY_TYPE_INTERNAL_INSTRUCTION_2:
		if (g_profileHandles[profile]->objectPool.insValid &&
			&g_profileHandles[profile]->objectPool.ins == obj)
		{
			g_profileHandles[profile]->objectPool.insValid = FALSE;
			return TRUE;
		}
		break;
	case BINARY_PROFILE_SECTION_ENTRY_TYPE_INTERNAL_MEMORY_0:
	case BINARY_PROFILE_SECTION_ENTRY_TYPE_INTERNAL_VGATHER_0:
	case BINARY_PROFILE_SECTION_ENTRY_TYPE_INTERNAL_XSAVE_0:
	case BINARY_PROFILE_SECTION_ENTRY_TYPE_INTERNAL_MEMORY_1:
	case BINARY_PROFILE_SECTION_ENTRY_TYPE_INTERNAL_VGATHER_1:
	case BINARY_PROFILE_SECTION_ENTRY_TYPE_INTERNAL_XSAVE_1:
	case BINARY_PROFILE_SECTION_ENTRY_TYPE_INTERNAL_MEMORY_2:
		if (g_profileHandles[profile]->objectPool.memValid &&
			&g_profileHandles[profile]->objectPool.mem == obj)
		{
			g_profileHandles[profile]->objectPool.memValid = FALSE;
			return TRUE;
		}
		break;
	case BINARY_PROFILE_SECTION_ENTRY_TYPE_INTERNAL_SYS_CALL_RECORD:
		if (g_profileHandles[profile]->objectPool.sysCallValid &&
			&g_profileHandles[profile]->objectPool.sysCall == obj)
		{
			g_profileHandles[profile]->objectPool.sysCallValid = FALSE;
			return TRUE;
		}
		break;
	case BINARY_PROFILE_SECTION_ENTRY_TYPE_INTERNAL_BRANCH_RECORD:
		if (g_profileHandles[profile]->objectPool.branchValid &&
			&g_profileHandles[profile]->objectPool.branch == obj)
		{
			g_profileHandles[profile]->objectPool.branchValid = FALSE;
			return TRUE;
		}
		break;
	default: // BINARY_PROFILE_SECTION_ENTRY_TYPE_INTERNAL_INVALID
		return FALSE;
	}

	free(obj);

	return TRUE;
}

INTERNAL_FUNC
BOOL ProfileObjectPoolDealloc(PROFILE_HANDLE profile,
	BINARY_PROFILE_SECTION_ENTRY_TYPE type, void *obj)
{
	// Assumes profile and obj are valid.
	switch (type)
	{
	case BINARY_PROFILE_SECTION_ENTRY_TYPE_HEADER:
		if (g_profileHandles[profile]->objectPool.headerValid &&
			&g_profileHandles[profile]->objectPool.header == obj)
		{
			g_profileHandles[profile]->objectPool.headerValid = FALSE;
			return TRUE;
		}
		break;
	case BINARY_PROFILE_SECTION_ENTRY_TYPE_PROCESSOR:
		if (g_profileHandles[profile]->objectPool.procValid &&
			&g_profileHandles[profile]->objectPool.proc == obj)
		{
			g_profileHandles[profile]->objectPool.procValid = FALSE;
			return TRUE;
		}
		break;
	case BINARY_PROFILE_SECTION_ENTRY_TYPE_INSTRUCTION_0:
	case BINARY_PROFILE_SECTION_ENTRY_TYPE_INSTRUCTION_1:
	case BINARY_PROFILE_SECTION_ENTRY_TYPE_INSTRUCTION_2:
		if (g_profileHandles[profile]->objectPool.insValid &&
			&g_profileHandles[profile]->objectPool.ins == obj)
		{
			g_profileHandles[profile]->objectPool.insValid = FALSE;
			return TRUE;
		}
		break;
	case BINARY_PROFILE_SECTION_ENTRY_TYPE_MEMORY_0:
	case BINARY_PROFILE_SECTION_ENTRY_TYPE_MEMORY_1:
	case BINARY_PROFILE_SECTION_ENTRY_TYPE_MEMORY_2:
		if (g_profileHandles[profile]->objectPool.memValid &&
			&g_profileHandles[profile]->objectPool.mem == obj)
		{
			g_profileHandles[profile]->objectPool.memValid = FALSE;
			return TRUE;
		}
		break;
	case BINARY_PROFILE_SECTION_ENTRY_TYPE_SYSCALL_RECORD:
		if (g_profileHandles[profile]->objectPool.sysCallValid &&
			&g_profileHandles[profile]->objectPool.sysCall == obj)
		{
			g_profileHandles[profile]->objectPool.sysCallValid = FALSE;
			return TRUE;
		}
		break;
	case BINARY_PROFILE_SECTION_ENTRY_TYPE_BRANCH_RECORD:
		if (g_profileHandles[profile]->objectPool.branchValid &&
			&g_profileHandles[profile]->objectPool.branch == obj)
		{
			g_profileHandles[profile]->objectPool.branchValid = FALSE;
			return TRUE;
		}
		break;
	default: // BINARY_PROFILE_SECTION_ENTRY_TYPE_INVALID
		return FALSE;
	}

	free(obj);

	return TRUE;
}

DECLSPEC
BOOL AbpCloseProfile(PROFILE_HANDLE profile)
{
	if (!IsProfileHandleValid(profile))
		return FALSE;
	fclose(g_profileHandles[profile]->file);
	FreeProfileInternals(g_profileHandles[profile]);
	g_profileHandles[profile] = NULL;
	g_profileHandles[profile] = NULL;
	return TRUE;
}

DECLSPEC
UINT32 AbpGetProfileVersion(PROFILE_HANDLE profile, PSTR version, UINT32 size)
{
	if (!IsProfileHandleValid(profile))
		return FALSE;
	if (version == NULL)
		return ABP_VERSION_1_0_SIZE + 1;
	if (size < ABP_VERSION_1_0_SIZE + 1)
		return ABP_VERSION_1_0_SIZE + 1;
	strcpy_s(version, ABP_VERSION_1_0_SIZE + 1, ABP_VERSION_1_0);
	return ABP_VERSION_1_0_SIZE + 1;
}

DECLSPEC
BINARY_HEADER_TYPE AbpGetProfileType(PROFILE_HANDLE profile)
{
	if (!IsProfileHandleValid(profile))
		return BINARY_HEADER_TYPE_INVALID_PROFILE;
	return g_profileHandles[profile]->header.type;
}

DECLSPEC
BINARY_HEADER_OS AbpGetProfileOS(PROFILE_HANDLE profile)
{
	if (!IsProfileHandleValid(profile))
		return BINARY_HEADER_OS_INVALID;
	return g_profileHandles[profile]->header.os;
}

DECLSPEC 
BINARY_HEADER_OS_VERSION AbpGetProfileOSVersion(PROFILE_HANDLE profile)
{
	if (!IsProfileHandleValid(profile))
		return BINARY_HEADER_OS_VERSION_INVALID;

	char *version = g_profileHandles[profile]->header.osVersion;
	if (strcmp(version, "10.0s") == 0)
	{
		return BINARY_HEADER_OS_VERSION_WIN_SERVER_2016;
	}
	else if (strcmp(version, "10.0") == 0)
	{
		return BINARY_HEADER_OS_VERSION_WIN_10;
	}
	else if (strcmp(version, "8.1s") == 0)
	{
		return BINARY_HEADER_OS_VERSION_WIN_SERVER_2012_R2;
	}
	else if (strcmp(version, "8.1") == 0)
	{
		return BINARY_HEADER_OS_VERSION_WIN_8_1;
	}
	else if (strcmp(version, "8.0s") == 0)
	{
		return BINARY_HEADER_OS_VERSION_WIN_SERVER_2012;
	}
	else if (strcmp(version, "8.0") == 0)
	{
		return BINARY_HEADER_OS_VERSION_WIN_8;
	}
	else if (strcmp(version, "7.1s") == 0)
	{
		return BINARY_HEADER_OS_VERSION_WIN_SERVER_2008_R2;
	}
	else if (strcmp(version, "7.1") == 0)
	{
		return BINARY_HEADER_OS_VERSION_WIN_7_SP1;
	}
	else if (strcmp(version, "7.0s") == 0)
	{
		return BINARY_HEADER_OS_VERSION_WIN_SERVER_2008_R2;
	}
	else if (strcmp(version, "7.0") == 0)
	{
		return BINARY_HEADER_OS_VERSION_WIN_7;
	}
	else
	{
		return BINARY_HEADER_OS_VERSION_INVALID;
	}
}

DECLSPEC
PROCESSOR_VENDOR AbpGetProfileProcessorVendor(PROFILE_HANDLE profile)
{
	if (!IsProfileHandleValid(profile))
		return PROCESSOR_VENDOR_INVALID;
	return g_profileHandles[profile]->header.processorVendor;
}

DECLSPEC
PROCESSOR_ARCH AbpGetProfileProcessorArch(PROFILE_HANDLE profile)
{
	if (!IsProfileHandleValid(profile))
		return PROCESSOR_ARCH_INVALID;
	return g_profileHandles[profile]->header.processorArch;
}

DECLSPEC 
PROCESSOR_ARCH AbpGetProfileTargetProcessorArch(PROFILE_HANDLE profile)
{
	if (!IsProfileHandleValid(profile))
		return PROCESSOR_ARCH_INVALID;
	return g_profileHandles[profile]->header.targetArch;
}

DECLSPEC
time_t AbpGetProfileTimestamp(PROFILE_HANDLE profile)
{
	if (!IsProfileHandleValid(profile))
		return FALSE;
	return g_profileHandles[profile]->header.timestamp;
}

DECLSPEC
UINT32 AbpGetProfileProcessNumber(PROFILE_HANDLE profile)
{
	if (!IsProfileHandleValid(profile))
		return FALSE;
	return g_profileHandles[profile]->header.processNumber;
}

DECLSPEC
UINT32 AbpGetProfileProcessOsId(PROFILE_HANDLE profile)
{
	if (!IsProfileHandleValid(profile))
		return FALSE;
	return g_profileHandles[profile]->header.processOsId;
}

DECLSPEC
time_t AbpGetProfileStartingTime(PROFILE_HANDLE profile)
{
	if (!IsProfileHandleValid(profile))
		return FALSE;
	return g_profileHandles[profile]->header.startingTime;
}

DECLSPEC
time_t AbpGetProfileEndingTime(PROFILE_HANDLE profile)
{
	if (!IsProfileHandleValid(profile))
		return FALSE;
	return g_profileHandles[profile]->header.endingTime;
}

DECLSPEC
unsigned long long AbpGetProfileInsCount(PROFILE_HANDLE profile)
{
	if (!IsProfileHandleValid(profile))
		return FALSE;
	return g_profileHandles[profile]->header.insCount;
}

DECLSPEC
unsigned long long AbpGetProfileMemInsCount(PROFILE_HANDLE profile)
{
	if (!IsProfileHandleValid(profile))
		return FALSE;
	return g_profileHandles[profile]->header.memInsCount;
}

DECLSPEC
UINT32 AbpGetProfileMainExePath(PROFILE_HANDLE profile, PSTR path, UINT32 size)
{
	if (!IsProfileHandleValid(profile))
		return FALSE;
	UINT32 trueSize = (UINT32)strlen(g_profileHandles[profile]->header.mainExePath) + 1;
	if (path == NULL)
		return trueSize;
	if (size < trueSize)
		return trueSize;
	strcpy_s(path, trueSize, g_profileHandles[profile]->header.mainExePath);
	return trueSize;
}

DECLSPEC
UINT32 AbpGetProfileSystemDirectory(PROFILE_HANDLE profile, PSTR dir, UINT32 size)
{
	if (!IsProfileHandleValid(profile))
		return FALSE;
	UINT32 trueSize = (UINT32)strlen(g_profileHandles[profile]->header.systemDirectory) + 1;
	if (dir == NULL)
		return trueSize;
	if (size < trueSize)
		return trueSize;
	strcpy_s(dir, trueSize, g_profileHandles[profile]->header.systemDirectory);
	return trueSize;
}

DECLSPEC
UINT32 AbpGetProfileCurrentDirectory(PROFILE_HANDLE profile, PSTR dir, UINT32 size)
{
	if (!IsProfileHandleValid(profile))
		return FALSE;
	UINT32 trueSize = (UINT32)strlen(g_profileHandles[profile]->header.currentDirectory) + 1;
	if (dir == NULL)
		return trueSize;
	if (size < trueSize)
		return trueSize;
	strcpy_s(dir, trueSize, g_profileHandles[profile]->header.currentDirectory);
	return trueSize;
}

DECLSPEC
UINT32 AbpGetProfileCmdLine(PROFILE_HANDLE profile, PSTR cmdLine, UINT32 size)
{
	if (!IsProfileHandleValid(profile))
		return FALSE;
	UINT32 trueSize = (UINT32)strlen(g_profileHandles[profile]->header.cmdLine) + 1;
	if (cmdLine == NULL)
		return trueSize;
	if (size < trueSize)
		return trueSize;
	strcpy_s(cmdLine, trueSize, g_profileHandles[profile]->header.cmdLine);
	return trueSize;
}

DECLSPEC
unsigned char AbpGetProfileVirtualAddrSize(PROFILE_HANDLE profile)
{
	if (!IsProfileHandleValid(profile))
		return FALSE;
	return g_profileHandles[profile]->header.vAddrSize;
}

DECLSPEC
unsigned char AbpGetProfilePhysicalAddrSize(PROFILE_HANDLE profile)
{
	if (!IsProfileHandleValid(profile))
		return FALSE;
	return g_profileHandles[profile]->header.pAddrSize;
}

DECLSPEC
UINT32 AbpGetProfilePageSize(PROFILE_HANDLE profile)
{
	if (!IsProfileHandleValid(profile))
		return FALSE;
	return g_profileHandles[profile]->header.pageSize;
}

DECLSPEC
UINT64 AbpGetProfileMinAppAddr(PROFILE_HANDLE profile)
{
	if (!IsProfileHandleValid(profile))
		return FALSE;
	return g_profileHandles[profile]->header.minAppAddr;
}

DECLSPEC
UINT64 AbpGetProfileMaxAppAddr(PROFILE_HANDLE profile)
{
	if (!IsProfileHandleValid(profile))
		return FALSE;
	return g_profileHandles[profile]->header.maxAppAddr;
}

DECLSPEC
UINT32 AbpGetProfileTotalDram(PROFILE_HANDLE profile)
{
	if (!IsProfileHandleValid(profile))
		return FALSE;
	return g_profileHandles[profile]->header.totalDram;
}

DECLSPEC
BOOL AbpDoesProfileContainInstructions(PROFILE_HANDLE profile)
{
	if (!IsProfileHandleValid(profile))
		return FALSE;
	return (g_profileHandles[profile]->header.flags & 0x1) == 1 ? TRUE : FALSE;
}

DECLSPEC 
BOOL AbpDoesProfileContainSchedulingInfo(PROFILE_HANDLE profile)
{
	if (!IsProfileHandleValid(profile))
		return FALSE;
	return (g_profileHandles[profile]->header.flags & 0x2) == 1 ? TRUE : FALSE;
}

//DECLSPEC
BOOL AbpGetProfileReserved(PROFILE_HANDLE profile, char reserved[64])
{
	if (!IsProfileHandleValid(profile))
		return FALSE;
	reserved = g_profileHandles[profile]->header.reserved;
	return TRUE;
}

DECLSPEC
UINT32 AbpGetProfileSectionDirectorySize(PROFILE_HANDLE profile)
{
	if (!IsProfileHandleValid(profile))
		return FALSE;
	return g_profileHandles[profile]->header.dirSize;
}

DECLSPEC
BOOL AbpGetProfileSectionDirectoryEntry(PROFILE_HANDLE profile, UINT32 index, PSECTION_DIRECTORY_ENTRY entry)
{
	if (!IsProfileHandleValid(profile))
		return FALSE;
	if (index >= g_profileHandles[profile]->header.dirSize || entry == NULL)
		return FALSE;
	*entry = g_profileHandles[profile]->secDir[index];
	return TRUE;
}

DECLSPEC 
UINT64 AbpGetInstructionTimestampFrequency(PROFILE_HANDLE profile)
{
	if (!IsProfileHandleValid(profile))
		return FALSE;
	return g_profileHandles[profile]->header.timeFreq;
}

INTERNAL_FUNC
FILE* ValidateAndGetFile(PROFILE_HANDLE profile, UINT32 secIndex, BINARY_SECTION_TYPE type, UINT64 *offset)
{
	if (!IsProfileHandleValid(profile))
		return NULL;
	if (secIndex >= g_profileHandles[profile]->header.dirSize)
		return NULL;
	PSECTION_DIRECTORY_ENTRY dirEntry = &g_profileHandles[profile]->secDir[secIndex];
	if (dirEntry->type != type)
		return NULL;
	if (offset)
		*offset = dirEntry->reserved;
	return g_profileHandles[profile]->file;
}

INTERNAL_FUNC
FILE* SectionSeekOffset(PROFILE_HANDLE profile, UINT32 secIndex, BINARY_SECTION_TYPE type, fpos_t offset)
{
	FILE *file = ValidateAndGetFile(profile, secIndex, type, NULL);
	if (!file || fsetpos(file, &offset))
		return NULL;
	return file;
}

INTERNAL_FUNC
FILE* SectionSeek(PROFILE_HANDLE profile, UINT32 secIndex, BINARY_SECTION_TYPE type)
{
	UINT64 offset;
	FILE *file = ValidateAndGetFile(profile, secIndex, type, &offset);
	if (!file || FSEEK64(file, offset, SEEK_SET))
		return NULL;
	return file;
}

INTERNAL_FUNC
FILE* CpuInfoSeek(PROFILE_HANDLE profile, UINT32 secIndex, UINT32 cpuId)
{
	FILE *file = SectionSeek(profile, secIndex, BINARY_SECTION_TYPE_CPUID);
	if (!file)
		return NULL;
	UINT32 cpuCount;
	if (fread(&cpuCount, sizeof(UINT32), 1, file) != 1)
		return NULL;
	if (cpuId >= cpuCount)
		return NULL;
	for (UINT32 i = 0; i < cpuCount; ++i)
	{
		if (i == cpuId)
			break;
		UINT32 nIds, nExIds;
		if (fread(&nIds, sizeof(UINT32), 1, file) != 1)
			return NULL;
		if (fread(&nExIds, sizeof(UINT32), 1, file) != 1)
			return NULL;
		if (FSEEK64(file, (nIds + nExIds) * sizeof(CPUID_INFO), SEEK_CUR))
			return NULL;
	}
	return file;
}

DECLSPEC
UINT32 AbpGetCpuInfo(PROFILE_HANDLE profile, UINT32 secIndex, UINT32 cpuId, PCPUID_INFO cpuInfo, BOOL extended)
{
	FILE *file = CpuInfoSeek(profile, secIndex, cpuId);
	if (!file)
		return FALSE;
	UINT32 nIds, nExIds;
	if (fread(&nIds, sizeof(UINT32), 1, file) != 1)
		return FALSE;
	if (fread(&nExIds, sizeof(UINT32), 1, file) != 1)
		return FALSE;
	UINT32 count = extended ? nIds + nExIds : nIds;
	if (cpuInfo == NULL)
	{
		return count;
	}
	else
	{
		if (fread(cpuInfo, sizeof(CPUID_INFO), count, file) != count)
			return FALSE;
	}
	return count;
}

INTERNAL_FUNC
BOOL CpuId(PROFILE_HANDLE profile, UINT32 secIndex, UINT32 cpuId, PCPUID_INFO cpuInfo)
{
	if (cpuInfo == NULL)
		return FALSE;
	FILE *file = CpuInfoSeek(profile, secIndex, cpuId);
	if (!file)
		return FALSE;
	UINT32 nIds, nExIds;
	if (fread(&nIds, sizeof(UINT32), 1, file) != 1)
		return FALSE;
	if (fread(&nExIds, sizeof(UINT32), 1, file) != 1)
		return FALSE;
	UINT32 count = nIds + nExIds;
	CPUID_INFO temp;
	for (UINT32 i = 0; i < count; ++i)
	{
		if (fread(&temp, sizeof(CPUID_INFO), 1, file) != 1)
			return FALSE;
		if (temp.EAX == cpuInfo->EAX && temp.ECX == cpuInfo->ECX)
		{
			*cpuInfo = temp;
			return TRUE;
		}
	}
	return FALSE;
}

DECLSPEC
UINT32 AbpGetCpuCount(PROFILE_HANDLE profile, UINT32 secIndex)
{
	FILE *file = SectionSeek(profile, secIndex, BINARY_SECTION_TYPE_CPUID);
	if (!file)
		return FALSE;
	UINT32 cpuCount;
	if (fread(&cpuCount, sizeof(UINT32), 1, file) != 1)
		return FALSE;
	return cpuCount;
}

INTERNAL_FUNC
FILE* ProcessSeek(PROFILE_HANDLE profile, UINT32 secIndex)
{
	FILE *file = SectionSeek(profile, secIndex, BINARY_SECTION_TYPE_PROCESS_FAMILY);
	if (!file)
		return NULL;
	return file;
}

DECLSPEC
UINT32 AbpGetProcessFamily(PROFILE_HANDLE profile, UINT32 secIndex, PBINARY_PROCESS_FAMILY_ENTRY processes)
{
	FILE *file = ProcessSeek(profile, secIndex);
	if (!file)
		return FALSE;
	UINT32 pCount;
	if (fread(&pCount, sizeof(UINT32), 1, file) != 1)
		return FALSE;
	if (processes == NULL)
		return pCount;
	if (fread(processes, sizeof(BINARY_PROCESS_FAMILY_ENTRY), pCount, file) != pCount)
		return FALSE;
	return pCount;
}

INTERNAL_FUNC
BOOL GetProcessInternal(PROFILE_HANDLE profile, UINT32 secIndex, UINT32 p, BOOL isNum, PBINARY_PROCESS_FAMILY_ENTRY process)
{
	if (process == NULL)
		return FALSE;
	FILE *file = ProcessSeek(profile, secIndex);
	if (!file)
		return FALSE;
	UINT32 pCount;
	if (fread(&pCount, sizeof(UINT32), 1, file) != 1)
		return FALSE;
	BINARY_PROCESS_FAMILY_ENTRY temp;
	for (UINT32 i = 0; i < pCount; ++i)
	{
		if (fread(&temp, sizeof(BINARY_PROCESS_FAMILY_ENTRY), 1, file) != 1)
			return FALSE;
		if ((isNum && p == temp.pn) || (!isNum && p == temp.pId))
		{
			*process = temp;
			return TRUE;
		}
	}
	return FALSE;
}

DECLSPEC
BOOL AbpGetProcessWithNumber(PROFILE_HANDLE profile, UINT32 secIndex, UINT32 pn, PBINARY_PROCESS_FAMILY_ENTRY process)
{
	return GetProcessInternal(profile, secIndex, pn, TRUE, process);
}

DECLSPEC
BOOL AbpGetProcessWithId(PROFILE_HANDLE profile, UINT32 secIndex, UINT32 pid, PBINARY_PROCESS_FAMILY_ENTRY process)
{
	return GetProcessInternal(profile, secIndex, pid, FALSE, process);
}

INTERNAL_FUNC
void SetEntry(PBINARY_PROFILE_SECTION_GENERIC entry)
{
	entry->reserved = PROFILE_ENTRY_FLAG;
}

INTERNAL_FUNC
void SetTrace(PBINARY_PROFILE_SECTION_TRACE_GENERIC trace)
{
	trace->reserved = PROFILE_TRACE_FLAG;
}

INTERNAL_FUNC
void SetSnapshot(PBINARY_LLW_SECTION_ENTRY_SNAPSHOT snapshot)
{
	snapshot->reserved1 = LLW_SNAPSHOT_FLAG;
}

INTERNAL_FUNC
BOOL VerifyEntry(PBINARY_PROFILE_SECTION_GENERIC entry)
{
	return entry->reserved == PROFILE_ENTRY_FLAG;
}

INTERNAL_FUNC
BOOL VerifyTrace(PBINARY_PROFILE_SECTION_TRACE_GENERIC trace)
{
	return trace->reserved == PROFILE_TRACE_FLAG;
}

INTERNAL_FUNC
BOOL VerifySnapshot(PBINARY_LLW_SECTION_ENTRY_SNAPSHOT snapshot)
{
	return snapshot->reserved1 == LLW_SNAPSHOT_FLAG;
}

INTERNAL_FUNC
void SetProfileEntryOffsetInternal(BINARY_PROFILE_SECTION_ENTRY_TYPE type, void *entry, UINT64 offset)
{	
	/*

	The reserved field is used to hold the offset of the next entry.

	*/

	switch (type)
	{
	case BINARY_PROFILE_SECTION_ENTRY_TYPE_HEADER:
		((PBINARY_PROFILE_SECTION_ENTRY_HEADER)entry)->reserved = offset;
		break;

	case BINARY_PROFILE_SECTION_ENTRY_TYPE_PROCESSOR:
		((PBINARY_PROFILE_SECTION_ENTRY_PROC)entry)->reserved = offset;
		break;

	case BINARY_PROFILE_SECTION_ENTRY_TYPE_INSTRUCTION_0:
	case BINARY_PROFILE_SECTION_ENTRY_TYPE_INSTRUCTION_1:
	case BINARY_PROFILE_SECTION_ENTRY_TYPE_INSTRUCTION_2:
		((PBINARY_PROFILE_SECTION_ENTRY_INS)entry)->reserved = offset;
		break;

	case BINARY_PROFILE_SECTION_ENTRY_TYPE_MEMORY_0:
	case BINARY_PROFILE_SECTION_ENTRY_TYPE_MEMORY_1:
	case BINARY_PROFILE_SECTION_ENTRY_TYPE_MEMORY_2:
		((PBINARY_PROFILE_SECTION_ENTRY_MEM)entry)->reserved = offset;
		break;

	case BINARY_PROFILE_SECTION_ENTRY_TYPE_SYSCALL_RECORD:
		((PBINARY_PROFILE_SECTION_ENTRY_SYSCALL)entry)->reserved = offset;
		break;

	case BINARY_PROFILE_SECTION_ENTRY_TYPE_BRANCH_RECORD:
		((PBINARY_PROFILE_SECTION_ENTRY_BRANCH)entry)->reserved = offset;
		break;
	}
}

INTERNAL_FUNC
void SetProfileEntryOffset(PBINARY_PROFILE_SECTION_GENERIC profileEntry, UINT64 offset)
{
	SetProfileEntryOffsetInternal(profileEntry->type, profileEntry->entry, offset);
}

INTERNAL_FUNC
void SetProfileTraceOffset(PBINARY_PROFILE_SECTION_TRACE_GENERIC profileTrace, UINT64 offset)
{
	SetProfileEntryOffsetInternal(profileTrace->type, profileTrace->entry, offset);
}

INTERNAL_FUNC
void SetLlwSnapshotOffset(PBINARY_LLW_SECTION_ENTRY_SNAPSHOT snapshot, UINT64 offset)
{
	snapshot->reserved2 = offset;
}

INTERNAL_FUNC
UINT64 GetProfileEntryOffsetInternal(BINARY_PROFILE_SECTION_ENTRY_TYPE type, void *entry)
{
	switch (type)
	{
	case BINARY_PROFILE_SECTION_ENTRY_TYPE_HEADER:
		return ((PBINARY_PROFILE_SECTION_ENTRY_HEADER)entry)->reserved;

	case BINARY_PROFILE_SECTION_ENTRY_TYPE_PROCESSOR:
		return ((PBINARY_PROFILE_SECTION_ENTRY_PROC)entry)->reserved;

	case BINARY_PROFILE_SECTION_ENTRY_TYPE_INSTRUCTION_0:
	case BINARY_PROFILE_SECTION_ENTRY_TYPE_INSTRUCTION_1:
	case BINARY_PROFILE_SECTION_ENTRY_TYPE_INSTRUCTION_2:
		return ((PBINARY_PROFILE_SECTION_ENTRY_INS)entry)->reserved;

	case BINARY_PROFILE_SECTION_ENTRY_TYPE_MEMORY_0:
	case BINARY_PROFILE_SECTION_ENTRY_TYPE_MEMORY_1:
	case BINARY_PROFILE_SECTION_ENTRY_TYPE_MEMORY_2:
		return ((PBINARY_PROFILE_SECTION_ENTRY_MEM)entry)->reserved;

	case BINARY_PROFILE_SECTION_ENTRY_TYPE_SYSCALL_RECORD:
		return ((PBINARY_PROFILE_SECTION_ENTRY_SYSCALL)entry)->reserved;

	case BINARY_PROFILE_SECTION_ENTRY_TYPE_BRANCH_RECORD:
		return ((PBINARY_PROFILE_SECTION_ENTRY_BRANCH)entry)->reserved;

	default:
		return -1;
	}
}

INTERNAL_FUNC
UINT64 GetProfileEntryOffset(PBINARY_PROFILE_SECTION_GENERIC profileEntry)
{
	return GetProfileEntryOffsetInternal(profileEntry->type, profileEntry->entry);
}

INTERNAL_FUNC
UINT64 GetProfileTraceOffset(PBINARY_PROFILE_SECTION_TRACE_GENERIC profileTrace)
{
	return GetProfileEntryOffsetInternal(profileTrace->type, profileTrace->entry);
}

INTERNAL_FUNC
UINT64 GetLlwSnapshotOffset(PBINARY_LLW_SECTION_ENTRY_SNAPSHOT snapshot)
{
	return snapshot->reserved2;
}

INTERNAL_FUNC
xed_error_enum_t InsDecode(PROFILE_HANDLE profile, UINT8 *ip, xed_decoded_inst_t *xedd, UINT32 byteCount)
{
	xed_state_t dstate;
	if (AbpGetProfileTargetProcessorArch(profile) == PROCESSOR_ARCH_X64)
	{
		dstate.mmode = XED_MACHINE_MODE_LONG_64;
		dstate.stack_addr_width = XED_ADDRESS_WIDTH_64b;
	}
	else
	{
		dstate.mmode = XED_MACHINE_MODE_LEGACY_32;
		dstate.stack_addr_width = XED_ADDRESS_WIDTH_32b;
	}

	xed_decoded_inst_zero_set_mode(xedd, &dstate);
	xed_error_enum_t xed_code = xed_decode(xedd, ip, byteCount);
	return xed_code;
}

INTERNAL_FUNC
BOOL GetProfileEntryInternalIns(PROFILE_HANDLE profile, FILE *file, PBINARY_PROFILE_SECTION_ENTRY_INS profileEntry, UINT8 type)
{
	if (type != BINARY_PROFILE_SECTION_ENTRY_TYPE_INSTRUCTION_2 &&
		type != BINARY_PROFILE_SECTION_ENTRY_TYPE_INSTRUCTION_1 &&
		type != BINARY_PROFILE_SECTION_ENTRY_TYPE_INSTRUCTION_0)
		return FALSE;

	memset(profileEntry, 0, sizeof(BINARY_PROFILE_SECTION_ENTRY_INS));

	if (fread(&profileEntry->vAddr, AbpGetProfileVirtualAddrSize(profile), 1, file) != 1)
		return FALSE;
	if (AbpGetProfilePhysicalAddrSize(profile) > 0 && (fread(&profileEntry->pAddr, AbpGetProfilePhysicalAddrSize(profile), 1, file) != 1))
		return FALSE;
	if (AbpDoesProfileContainInstructions(profile))
	{
		fpos_t fpos;
		fgetpos(file, &fpos);

		UINT8 buffer[XED_MAX_INSTRUCTION_BYTES] = { 0 };
		UINT32 byteCount = (UINT32)fread(buffer, 1, XED_MAX_INSTRUCTION_BYTES, file);
		if (byteCount < 2)
			return FALSE;

		if (buffer[0] == 0 && buffer[1] == 0)
		{
			// Invalid instruction.
			profileEntry->insBytes = NULL;
			byteCount = 2;
		}
		else
		{
			xed_decoded_inst_t xedDecoded;
			xed_error_enum_t xedErr = InsDecode(profile, buffer, &xedDecoded, byteCount);
			if (xedErr != XED_ERROR_NONE)
				return FALSE;

			byteCount = xed_decoded_inst_get_length(&xedDecoded);
			// TODO: may leak in case this function fails later.
			profileEntry->insBytes = malloc(byteCount);
			memcpy(profileEntry->insBytes, buffer, byteCount);
		}

		fsetpos(file, &fpos);
		FSEEK64(file, byteCount, SEEK_CUR);
	}
	else
	{
		profileEntry->insBytes = NULL;
	}

	if (type != BINARY_PROFILE_SECTION_ENTRY_TYPE_INSTRUCTION_2)
	{
		UINT8 funcIndex;
		if (fread(&funcIndex, sizeof(funcIndex), 1, file) != 1)
			return FALSE;
		UINT8 funcIndexType = funcIndex & 0x2;
		if (funcIndexType == 0)
		{
			profileEntry->routineId = funcIndex;
		}
		else if (funcIndexType == 1)
		{
			UINT32 upperBytes = 0;
			if (fread(&upperBytes, 1, 1, file) != 1)
				return FALSE;

			profileEntry->routineId = (upperBytes << 8) | funcIndex;
		}
		else if (funcIndexType == 2)
		{
			UINT32 upperBytes = 0;
			if (fread(&upperBytes, 2, 1, file) != 1)
				return FALSE;

			profileEntry->routineId = (upperBytes << 8) | funcIndex;
		}
		else
		{
			UINT32 upperBytes = 0;
			if (fread(&upperBytes, 3, 1, file) != 1)
				return FALSE;

			profileEntry->routineId = (upperBytes << 8) | funcIndex;
		}

		if (type == BINARY_PROFILE_SECTION_ENTRY_TYPE_INSTRUCTION_0)
		{
			if (fread(&profileEntry->timestamp, sizeof(profileEntry->timestamp), 1, file) != 1)
				return FALSE;
		}
	}

	return TRUE;
}

INTERNAL_FUNC
BOOL GetProfileEntryInternalMem(PROFILE_HANDLE profile, FILE *file, PBINARY_PROFILE_SECTION_ENTRY_MEM profileEntry, UINT8 type)
{
	// TODO: handle memory allocation errors and leaks (in case of failure) in this function.

	if (type != BINARY_PROFILE_SECTION_ENTRY_TYPE_INTERNAL_MEMORY_0 &&
		type != BINARY_PROFILE_SECTION_ENTRY_TYPE_INTERNAL_MEMORY_1 &&
		type != BINARY_PROFILE_SECTION_ENTRY_TYPE_INTERNAL_MEMORY_2 &&
		type != BINARY_PROFILE_SECTION_ENTRY_TYPE_INTERNAL_VGATHER_0 &&
		type != BINARY_PROFILE_SECTION_ENTRY_TYPE_INTERNAL_VGATHER_1 &&
		type != BINARY_PROFILE_SECTION_ENTRY_TYPE_INTERNAL_XSAVE_0 &&
		type != BINARY_PROFILE_SECTION_ENTRY_TYPE_INTERNAL_XSAVE_1)
		return FALSE;

	memset(profileEntry, 0, sizeof(BINARY_PROFILE_SECTION_ENTRY_MEM));

	if (fread(&profileEntry->memopType, 1, 1, file) != 1)
		return FALSE;

	if (type != BINARY_PROFILE_SECTION_ENTRY_TYPE_MEMORY_2)
	{
		if (fread(&profileEntry->vAddr, AbpGetProfileVirtualAddrSize(profile), 1, file) != 1)
			return FALSE;
		if (AbpGetProfilePhysicalAddrSize(profile) > 0 && (fread(&profileEntry->pAddr, AbpGetProfilePhysicalAddrSize(profile), 1, file) != 1))
			return FALSE;
		if (fread(&profileEntry->size, sizeof(profileEntry->size), 1, file) != 1)
			return FALSE;


		if (type == BINARY_PROFILE_SECTION_ENTRY_TYPE_INTERNAL_MEMORY_0)
		{
			BOOL isValueValid = 0;
			if (fread(&isValueValid, 1, 1, file) != 1)
				return FALSE;

			if (isValueValid)
			{
				profileEntry->info = malloc(profileEntry->size);
				if (fread(profileEntry->info, profileEntry->size, 1, file) != 1)
				{
					free(profileEntry->info);
					return FALSE;
				}
			}
			else
			{
				profileEntry->info = NULL;
			}
		}
		else if (type != BINARY_PROFILE_SECTION_ENTRY_TYPE_INTERNAL_MEMORY_1)
		{
			PBINARY_PROFILE_SECTION_ENTRY_MEM_EXTENDED_INFO extendedInfo = malloc(sizeof(BINARY_PROFILE_SECTION_ENTRY_MEM_EXTENDED_INFO));
			if (fread(&extendedInfo->numberOfMemops, sizeof(extendedInfo->numberOfMemops), 1, file) != 1)
			{
				free(extendedInfo);
				return FALSE;
			}

			extendedInfo->accesses = malloc(sizeof(struct MEMORY_ACCESS) * extendedInfo->numberOfMemops);
			memset(extendedInfo->accesses, 0, sizeof(struct MEMORY_ACCESS) * extendedInfo->numberOfMemops);

			UINT32 i;
			for (i = 0; i < extendedInfo->numberOfMemops; ++i)
			{
				if (type == BINARY_PROFILE_SECTION_ENTRY_TYPE_INTERNAL_VGATHER_0 || 
					type == BINARY_PROFILE_SECTION_ENTRY_TYPE_INTERNAL_VGATHER_1)
				{
					if (fread(&extendedInfo->accesses[i].maskOn, 1, 1, file) != 1)
						break;
					if (fread(&extendedInfo->accesses[i].vAddr, AbpGetProfileVirtualAddrSize(profile), 1, file) != 1)
						break;
					if (AbpGetProfilePhysicalAddrSize(profile) > 0 && (fread(&extendedInfo->accesses[i].pAddr, AbpGetProfilePhysicalAddrSize(profile), 1, file) != 1))
						break;
					if (extendedInfo->accesses[i].maskOn)
					{
						if (fread(&extendedInfo->accesses[i].bytesAccessed, 1, 1, file) != 1)
							break;

						BOOL isValueValid = 0;
						if (fread(&isValueValid, 1, 1, file) != 1)
							return FALSE;

						if (isValueValid && type == BINARY_PROFILE_SECTION_ENTRY_TYPE_INTERNAL_VGATHER_0)
						{
							extendedInfo->accesses[i].value = malloc(extendedInfo->accesses[i].bytesAccessed);
							if (fread(extendedInfo->accesses[i].value, extendedInfo->accesses[i].bytesAccessed, 1, file) != 1)
								break;
						}
						else
						{
							extendedInfo->accesses[i].value = NULL;
						}
					}

					// This always represents a VGATHER access.
					extendedInfo->accesses[i].memopType = MEM_OP_TYPE_LOAD;
				}
				else /*if (type == BINARY_PROFILE_SECTION_ENTRY_TYPE_INTERNAL_XSAVE_0 ||
					type == BINARY_PROFILE_SECTION_ENTRY_TYPE_INTERNAL_XSAVE_1)*/
				{
					extendedInfo->accesses[i].maskOn = TRUE;
					if (fread(&extendedInfo->accesses[i].vAddr, AbpGetProfileVirtualAddrSize(profile), 1, file) != 1)
						break;
					if (AbpGetProfilePhysicalAddrSize(profile) > 0 && (fread(&extendedInfo->accesses[i].pAddr, AbpGetProfilePhysicalAddrSize(profile), 1, file) != 1))
						break;
					if (fread(&extendedInfo->accesses[i].bytesAccessed, 1, 1, file) != 1)
						break;
					if (fread(&extendedInfo->accesses[i].memopType, 1, 1, file) != 1)
						break;

					BOOL isValueValid = 0;
					if (fread(&isValueValid, 1, 1, file) != 1)
						return FALSE;

					if (isValueValid && type == BINARY_PROFILE_SECTION_ENTRY_TYPE_INTERNAL_XSAVE_0)
					{
						extendedInfo->accesses[i].value = malloc(extendedInfo->accesses[i].bytesAccessed);
						if (fread(extendedInfo->accesses[i].value, extendedInfo->accesses[i].bytesAccessed, 1, file) != 1)
							break;
					}
					else
					{
						extendedInfo->accesses[i].value = NULL;
					}
				}
			}

			if (i != extendedInfo->numberOfMemops)
			{
				// Parsing failed. Cleanup.
				for (UINT32 j = 0; j <= i; ++j)
				{
					if (extendedInfo->accesses[j].value != NULL)
						free(extendedInfo->accesses[j].value);
				}

				free(extendedInfo->accesses);
				free(extendedInfo);
				return FALSE;
			}

			profileEntry->info = (UINT8*)extendedInfo;
			profileEntry->memopType = MEM_OP_TYPE_MULTI;
		}
		else // BINARY_PROFILE_SECTION_ENTRY_TYPE_INTERNAL_MEMORY_1
		{
			// Nothing to do here.
		}
	}

	return TRUE;
}

INTERNAL_FUNC
BINARY_PROFILE_SECTION_ENTRY_TYPE EntryTypeInternalToEntryType(BINARY_PROFILE_SECTION_ENTRY_TYPE_INTERNAL t)
{
	switch (t)
	{
	case BINARY_PROFILE_SECTION_ENTRY_TYPE_INTERNAL_HEADER:
		return BINARY_PROFILE_SECTION_ENTRY_TYPE_HEADER;

	case BINARY_PROFILE_SECTION_ENTRY_TYPE_INTERNAL_PROCESSOR:
		return BINARY_PROFILE_SECTION_ENTRY_TYPE_PROCESSOR;

	case BINARY_PROFILE_SECTION_ENTRY_TYPE_INTERNAL_INSTRUCTION_0:
		return BINARY_PROFILE_SECTION_ENTRY_TYPE_INSTRUCTION_0;

	case BINARY_PROFILE_SECTION_ENTRY_TYPE_INTERNAL_INSTRUCTION_1:
		return BINARY_PROFILE_SECTION_ENTRY_TYPE_INSTRUCTION_1;

	case BINARY_PROFILE_SECTION_ENTRY_TYPE_INTERNAL_INSTRUCTION_2:
		return BINARY_PROFILE_SECTION_ENTRY_TYPE_INSTRUCTION_2;

	case BINARY_PROFILE_SECTION_ENTRY_TYPE_INTERNAL_MEMORY_0:
	case BINARY_PROFILE_SECTION_ENTRY_TYPE_INTERNAL_VGATHER_0:
	case BINARY_PROFILE_SECTION_ENTRY_TYPE_INTERNAL_XSAVE_0:
		return BINARY_PROFILE_SECTION_ENTRY_TYPE_MEMORY_0;

	case BINARY_PROFILE_SECTION_ENTRY_TYPE_INTERNAL_MEMORY_1:
	case BINARY_PROFILE_SECTION_ENTRY_TYPE_INTERNAL_VGATHER_1:
	case BINARY_PROFILE_SECTION_ENTRY_TYPE_INTERNAL_XSAVE_1:
		return BINARY_PROFILE_SECTION_ENTRY_TYPE_MEMORY_1;

	case BINARY_PROFILE_SECTION_ENTRY_TYPE_INTERNAL_MEMORY_2:
		return BINARY_PROFILE_SECTION_ENTRY_TYPE_MEMORY_2;

	case BINARY_PROFILE_SECTION_ENTRY_TYPE_INTERNAL_SYS_CALL_RECORD:
		return BINARY_PROFILE_SECTION_ENTRY_TYPE_SYSCALL_RECORD;

	case BINARY_PROFILE_SECTION_ENTRY_TYPE_INTERNAL_BRANCH_RECORD:
		return BINARY_PROFILE_SECTION_ENTRY_TYPE_BRANCH_RECORD;

	default: // BINARY_PROFILE_SECTION_ENTRY_TYPE_INTERNAL_INVALID
		return BINARY_PROFILE_SECTION_ENTRY_TYPE_INVALID;
	}
}

INTERNAL_FUNC
BOOL GetProfileEntryInternal(PROFILE_HANDLE profile, FILE *file, PBINARY_PROFILE_SECTION_GENERIC profileEntry, UINT8 type)
{
	void *obj = NULL;
	if (type == BINARY_PROFILE_SECTION_ENTRY_TYPE_INTERNAL_INVALID && fread(&type, sizeof(UINT8), 1, file) != 1)
		return FALSE;

	switch (type)
	{
	case BINARY_PROFILE_SECTION_ENTRY_TYPE_INTERNAL_HEADER:
	{
		PBINARY_PROFILE_SECTION_ENTRY_HEADER header = 
			ProfileObjectPoolAlloc(profile, BINARY_PROFILE_SECTION_ENTRY_TYPE_INTERNAL_HEADER);
		obj = header;
		FReadOpenProfile(header->ostid, file);
		FReadOpenProfile(header->tn, file);
		header->funcIndex = 0;
		if (fread(&header->funcIndex, 1, 1, file) != 1)
			goto cleanupfail;
		profileEntry->type = EntryTypeInternalToEntryType(type);
		profileEntry->entry = header;
		return TRUE;
	}
	case BINARY_PROFILE_SECTION_ENTRY_TYPE_INTERNAL_PROCESSOR:
	{
		PBINARY_PROFILE_SECTION_ENTRY_PROC proc =
			ProfileObjectPoolAlloc(profile, BINARY_PROFILE_SECTION_ENTRY_TYPE_INTERNAL_PROCESSOR);
		obj = proc;
		FReadOpenProfile(proc->package, file);
		FReadOpenProfile(proc->number, file);
		profileEntry->type = EntryTypeInternalToEntryType(type);
		profileEntry->entry = proc;
		return TRUE;
	}
	case BINARY_PROFILE_SECTION_ENTRY_TYPE_INTERNAL_INSTRUCTION_0:
	case BINARY_PROFILE_SECTION_ENTRY_TYPE_INTERNAL_INSTRUCTION_1:
	case BINARY_PROFILE_SECTION_ENTRY_TYPE_INTERNAL_INSTRUCTION_2:
	{
		PBINARY_PROFILE_SECTION_ENTRY_INS ins =
			ProfileObjectPoolAlloc(profile, BINARY_PROFILE_SECTION_ENTRY_TYPE_INTERNAL_INSTRUCTION_0);
		obj = ins;
		if (!GetProfileEntryInternalIns(profile, file, ins, type))
			goto cleanupfail;
		profileEntry->type = EntryTypeInternalToEntryType(type);
		profileEntry->entry = ins;
		return TRUE;
	}
	case BINARY_PROFILE_SECTION_ENTRY_TYPE_INTERNAL_MEMORY_0:
	case BINARY_PROFILE_SECTION_ENTRY_TYPE_INTERNAL_MEMORY_1:
	case BINARY_PROFILE_SECTION_ENTRY_TYPE_INTERNAL_MEMORY_2:
	case BINARY_PROFILE_SECTION_ENTRY_TYPE_INTERNAL_VGATHER_0:
	case BINARY_PROFILE_SECTION_ENTRY_TYPE_INTERNAL_VGATHER_1:
	case BINARY_PROFILE_SECTION_ENTRY_TYPE_INTERNAL_XSAVE_0:
	case BINARY_PROFILE_SECTION_ENTRY_TYPE_INTERNAL_XSAVE_1:
	{
		PBINARY_PROFILE_SECTION_ENTRY_MEM mem =
			ProfileObjectPoolAlloc(profile, BINARY_PROFILE_SECTION_ENTRY_TYPE_INTERNAL_MEMORY_0);
		obj = mem;
		if (!GetProfileEntryInternalMem(profile, file, mem, type))
			goto cleanupfail;
		profileEntry->type = EntryTypeInternalToEntryType(type);
		profileEntry->entry = mem;
		return TRUE;
	}

	case BINARY_PROFILE_SECTION_ENTRY_TYPE_INTERNAL_SYS_CALL_RECORD:
	{
		PBINARY_PROFILE_SECTION_ENTRY_SYSCALL sysCall =
			ProfileObjectPoolAlloc(profile, BINARY_PROFILE_SECTION_ENTRY_TYPE_INTERNAL_SYS_CALL_RECORD);
		obj = sysCall;

		if (fread(&sysCall->errRetRecorded, 1 /* one byte only */, 1, file) != 1)
			goto cleanupfail;

		FReadOpenProfile(sysCall->num, file);
		FReadOpenProfile(sysCall->retVal, file);

		if (AbpGetProfileTargetProcessorArch(profile) == PROCESSOR_ARCH_X64)
		{
			if (fread(&sysCall->err, 8 /* eight bytes only */, 1, file) != 1)
				goto cleanupfail;
		}
		else /* PROCESSOR_ARCH_X86 */
		{
			if (fread(&sysCall->err, 4 /* four bytes only */, 1, file) != 1)
				goto cleanupfail;
		}

		FReadOpenProfile(sysCall->arg0, file);
		FReadOpenProfile(sysCall->arg1, file);
		FReadOpenProfile(sysCall->arg2, file);
		FReadOpenProfile(sysCall->arg3, file);
		FReadOpenProfile(sysCall->arg4, file);
		FReadOpenProfile(sysCall->arg5, file);
		FReadOpenProfile(sysCall->arg6, file);
		FReadOpenProfile(sysCall->arg7, file);
		FReadOpenProfile(sysCall->arg8, file);
		FReadOpenProfile(sysCall->arg9, file);
		FReadOpenProfile(sysCall->arg10, file);
		FReadOpenProfile(sysCall->arg11, file);
		FReadOpenProfile(sysCall->arg12, file);
		FReadOpenProfile(sysCall->arg13, file);
		FReadOpenProfile(sysCall->arg14, file);
		FReadOpenProfile(sysCall->arg15, file);
		profileEntry->type = EntryTypeInternalToEntryType(type);
		profileEntry->entry = sysCall;
		return TRUE;
	}
	case BINARY_PROFILE_SECTION_ENTRY_TYPE_INTERNAL_BRANCH_RECORD:
	{
		PBINARY_PROFILE_SECTION_ENTRY_BRANCH branch =
			ProfileObjectPoolAlloc(profile, BINARY_PROFILE_SECTION_ENTRY_TYPE_INTERNAL_BRANCH_RECORD);
		obj = branch;
		if (fread(&branch->vTarget, AbpGetProfileVirtualAddrSize(profile), 1, file) != 1)
			goto cleanupfail;
		if (AbpGetProfilePhysicalAddrSize(profile) > 0 && (fread(&branch->pTarget, AbpGetProfilePhysicalAddrSize(profile), 1, file) != 1))
			goto cleanupfail;
		if (fread(&branch->taken, 1, 1, file) != 1)
			goto cleanupfail;
		profileEntry->type = EntryTypeInternalToEntryType(type);
		profileEntry->entry = branch;
		return TRUE;
	}
	default: // BINARY_PROFILE_SECTION_ENTRY_TYPE_INTERNAL_INVALID
	{
		// Last entry in the profile.
		profileEntry->type = EntryTypeInternalToEntryType(type);
		profileEntry->entry = NULL;
		return FALSE;
	}
	}
	return FALSE;
cleanupfail:
	ProfileObjectPoolDeallocInternal(profile, type, obj);
	return FALSE;
}

DECLSPEC
BOOL AbpGetProfileEntryFirst(PROFILE_HANDLE profile, UINT32 secIndex, PBINARY_PROFILE_SECTION_GENERIC profileEntry)
{
	if (!profileEntry)
		return FALSE;
	FILE *file = SectionSeek(profile, secIndex, BINARY_SECTION_TYPE_PROFILE);
	if (!file)
		return FALSE;
	if (!GetProfileEntryInternal(profile, file, profileEntry, BINARY_PROFILE_SECTION_ENTRY_TYPE_INTERNAL_INVALID))
		return FALSE;
	fpos_t pos;
	fgetpos(file, &pos);
	SetProfileEntryOffset(profileEntry, (UINT64)pos);
	SetEntry(profileEntry);
	return TRUE;
}

DECLSPEC
BOOL AbpGetProfileEntryNext(PROFILE_HANDLE profile, UINT32 secIndex, 
	PBINARY_PROFILE_SECTION_GENERIC inProfileEntry, PBINARY_PROFILE_SECTION_GENERIC outProfileEntry)
{
	if (!inProfileEntry || !outProfileEntry || !VerifyEntry(inProfileEntry) || inProfileEntry->type == BINARY_PROFILE_SECTION_ENTRY_TYPE_INVALID)
		return FALSE;
	fpos_t offset = (fpos_t)GetProfileEntryOffset(inProfileEntry);
	if (offset == -1)
		return FALSE;
	FILE *file = SectionSeekOffset(profile, secIndex, BINARY_SECTION_TYPE_PROFILE, offset);
	if (!file)
		return FALSE;
	if (!GetProfileEntryInternal(profile, file, outProfileEntry, BINARY_PROFILE_SECTION_ENTRY_TYPE_INTERNAL_INVALID))
		return FALSE;
	fpos_t pos;
	fgetpos(file, &pos);
	SetProfileEntryOffset(outProfileEntry, (UINT64)pos);
	SetEntry(outProfileEntry);
	return TRUE;
}

DECLSPEC
BOOL AbpGetProfileEntryNextFast(PROFILE_HANDLE profile, UINT32 secIndex, 
	PBINARY_PROFILE_SECTION_GENERIC inProfileEntry, PBINARY_PROFILE_SECTION_GENERIC outProfileEntry)
{
	FILE *file = g_profileHandles[profile]->file;
	if (!GetProfileEntryInternal(profile, file, outProfileEntry, BINARY_PROFILE_SECTION_ENTRY_TYPE_INTERNAL_INVALID))
		return FALSE;
	SetEntry(outProfileEntry);
	return TRUE;
}

DECLSPEC
BOOL AbpDestroyProfileEntry(PROFILE_HANDLE profile, PBINARY_PROFILE_SECTION_GENERIC profileEntry)
{
	if (!profileEntry || !VerifyEntry(profileEntry))
		return FALSE;

	if (profileEntry->type != BINARY_PROFILE_SECTION_ENTRY_TYPE_INVALID)
	{
		if (profileEntry->entry == NULL)
			return FALSE;

		switch (profileEntry->type)
		{
		case BINARY_PROFILE_SECTION_ENTRY_TYPE_INSTRUCTION_0:
		case BINARY_PROFILE_SECTION_ENTRY_TYPE_INSTRUCTION_1:
		case BINARY_PROFILE_SECTION_ENTRY_TYPE_INSTRUCTION_2:
			if (((PBINARY_PROFILE_SECTION_ENTRY_INS)profileEntry->entry)->insBytes != NULL)
				free(((PBINARY_PROFILE_SECTION_ENTRY_INS)profileEntry->entry)->insBytes);
			break;

		case BINARY_PROFILE_SECTION_ENTRY_TYPE_MEMORY_0:
		{
			PBINARY_PROFILE_SECTION_ENTRY_MEM memEntry = (PBINARY_PROFILE_SECTION_ENTRY_MEM)profileEntry->entry;
			if (memEntry->memopType == MEM_OP_TYPE_MULTI)
			{
				PBINARY_PROFILE_SECTION_ENTRY_MEM_EXTENDED_INFO extendedInfo = (PBINARY_PROFILE_SECTION_ENTRY_MEM_EXTENDED_INFO)memEntry->info;
				if (extendedInfo != NULL)
				{
					if (extendedInfo->accesses != NULL)
					{
						for (UINT32 i = 0; i < extendedInfo->numberOfMemops; ++i)
						{
							if (extendedInfo->accesses[i].value != NULL)
								free(extendedInfo->accesses[i].value);
						}
						free(extendedInfo->accesses);
					}

					free(extendedInfo);
				}
			}
			else
			{
				free(memEntry->info);
			}
			break;
		}
		case BINARY_PROFILE_SECTION_ENTRY_TYPE_MEMORY_1:
		{
			PBINARY_PROFILE_SECTION_ENTRY_MEM memEntry = (PBINARY_PROFILE_SECTION_ENTRY_MEM)profileEntry->entry;
			if (memEntry->memopType == MEM_OP_TYPE_MULTI)
			{
				PBINARY_PROFILE_SECTION_ENTRY_MEM_EXTENDED_INFO extendedInfo = (PBINARY_PROFILE_SECTION_ENTRY_MEM_EXTENDED_INFO)memEntry->info;
				if (extendedInfo != NULL)
				{
					if (extendedInfo->accesses != NULL)
					{
						free(extendedInfo->accesses);
					}

					free(extendedInfo);
				}
			}
			break;
		}
		default:
			; // Nothing to do here.
		}

		ProfileObjectPoolDealloc(profile, profileEntry->type, profileEntry->entry);
	}

	memset(profileEntry, 0, sizeof(BINARY_PROFILE_SECTION_GENERIC));

	return TRUE;
}

INTERNAL_FUNC
BOOL ProfileEntryTypeLevelEqual(UINT8 typeLeft, UINT8 typeRight)
{
	if ((typeLeft == BINARY_PROFILE_SECTION_ENTRY_TYPE_INSTRUCTION_0 ||
		typeLeft == BINARY_PROFILE_SECTION_ENTRY_TYPE_INSTRUCTION_1 ||
		typeLeft == BINARY_PROFILE_SECTION_ENTRY_TYPE_INSTRUCTION_2) &&
		(typeRight == BINARY_PROFILE_SECTION_ENTRY_TYPE_INSTRUCTION_0 ||
		typeRight == BINARY_PROFILE_SECTION_ENTRY_TYPE_INSTRUCTION_1 ||
		typeRight == BINARY_PROFILE_SECTION_ENTRY_TYPE_INSTRUCTION_2))
		return TRUE;
	else if ((typeLeft == BINARY_PROFILE_SECTION_ENTRY_TYPE_MEMORY_0 ||
		typeLeft == BINARY_PROFILE_SECTION_ENTRY_TYPE_MEMORY_1 ||
		typeLeft == BINARY_PROFILE_SECTION_ENTRY_TYPE_MEMORY_2 ||
		typeLeft == BINARY_PROFILE_SECTION_ENTRY_TYPE_SYSCALL_RECORD ||
		typeLeft == BINARY_PROFILE_SECTION_ENTRY_TYPE_BRANCH_RECORD) &&
		(typeRight == BINARY_PROFILE_SECTION_ENTRY_TYPE_MEMORY_0 ||
		typeRight == BINARY_PROFILE_SECTION_ENTRY_TYPE_MEMORY_1 ||
		typeRight == BINARY_PROFILE_SECTION_ENTRY_TYPE_MEMORY_2 ||
		typeRight == BINARY_PROFILE_SECTION_ENTRY_TYPE_SYSCALL_RECORD ||
		typeRight == BINARY_PROFILE_SECTION_ENTRY_TYPE_BRANCH_RECORD))
		return TRUE;
	else
		return typeLeft == typeRight;
}

INTERNAL_FUNC
UINT64 ConstructProfileTraceInternal(PROFILE_HANDLE profile, FILE *file, PDYNAMIC_ARRAY entries, UINT8 level)
{
	BINARY_PROFILE_SECTION_GENERIC profileEntry;
	UINT8 type;
	size_t counter = 0;
	fpos_t fpos;
	while (TRUE)
	{
		fgetpos(file, &fpos);
		if (fread(&type, sizeof(UINT8), 1, file) != 1)
			return FALSE;
		if (ProfileEntryTypeLevelEqual(type, level))
		{
			if (GetProfileEntryInternal(profile, file, &profileEntry, type))
			{
				PBINARY_PROFILE_SECTION_TRACE_GENERIC profileTrace = (PBINARY_PROFILE_SECTION_TRACE_GENERIC)malloc(sizeof(BINARY_PROFILE_SECTION_TRACE_GENERIC));
				profileTrace->entry = profileEntry.entry;
				profileTrace->type = profileEntry.type;
				profileTrace->nodeCount = 0;
				profileTrace->nodes = NULL;
				DynamicArrayAppend(entries, (PDYNAMIC_ARRAY_ELEMENT_TYPE)&profileTrace);
				++counter;
			}
			else
			{
				return -1;
			}
		}
		else if 
			(
			(level == BINARY_PROFILE_SECTION_ENTRY_TYPE_PROCESSOR &&
			(type == BINARY_PROFILE_SECTION_ENTRY_TYPE_INSTRUCTION_0 ||
			type == BINARY_PROFILE_SECTION_ENTRY_TYPE_INSTRUCTION_1 ||
			type == BINARY_PROFILE_SECTION_ENTRY_TYPE_INSTRUCTION_2)) 
			
			||
			
			((level == BINARY_PROFILE_SECTION_ENTRY_TYPE_INSTRUCTION_0 ||
			level == BINARY_PROFILE_SECTION_ENTRY_TYPE_INSTRUCTION_1 ||
			level == BINARY_PROFILE_SECTION_ENTRY_TYPE_INSTRUCTION_2) &&
			(type == BINARY_PROFILE_SECTION_ENTRY_TYPE_MEMORY_0 ||
			type == BINARY_PROFILE_SECTION_ENTRY_TYPE_MEMORY_1 ||
			type == BINARY_PROFILE_SECTION_ENTRY_TYPE_MEMORY_2 ||
			type == BINARY_PROFILE_SECTION_ENTRY_TYPE_SYSCALL_RECORD ||
			type == BINARY_PROFILE_SECTION_ENTRY_TYPE_BRANCH_RECORD))
			)
		{
			PBINARY_PROFILE_SECTION_TRACE_GENERIC profileTrace;
			DynamicArrayGet(entries, counter, (PDYNAMIC_ARRAY_ELEMENT_TYPE)&profileTrace);
			PDYNAMIC_ARRAY da = (PDYNAMIC_ARRAY)malloc(sizeof(DYNAMIC_ARRAY));
			DynamicArrayInit(da);
			profileTrace->nodes = (PBINARY_PROFILE_SECTION_TRACE_GENERIC)da;
			profileTrace->nodeCount = ConstructProfileTraceInternal(profile, file, (PDYNAMIC_ARRAY)profileTrace->nodes, type);
			if (profileTrace->nodeCount == -1)
				return -1;
		}
		else
		{
			// Back up one level.
			break;
		}
	}

	// Decrement the file get pointer by one byte (size of type).
	fsetpos(file, &fpos);
	return counter;
}

void DestroyDynamicArrayHierarchy(PDYNAMIC_ARRAY root)
{
	if (root == NULL)
		return;

	for (size_t i = 0; i < DynamicArrayGetSize(root); ++i)
	{
		PBINARY_PROFILE_SECTION_TRACE_GENERIC profileTrace;
		DynamicArrayGet(root, i, (PDYNAMIC_ARRAY_ELEMENT_TYPE)&profileTrace);
		DestroyDynamicArrayHierarchy((PDYNAMIC_ARRAY)profileTrace->nodes);
		free(profileTrace);
	}

	DynamicArrayFree(root);
	free(free);
}

PBINARY_PROFILE_SECTION_TRACE_GENERIC DynamicArrayHierarchyToStaticArrayHierarchy(PDYNAMIC_ARRAY root)
{
	if (root == NULL)
		return NULL;

	PBINARY_PROFILE_SECTION_TRACE_GENERIC profileTrace;
	for (size_t i = 0; i < DynamicArrayGetSize(root); ++i)
	{
		DynamicArrayGet(root, i, (PDYNAMIC_ARRAY_ELEMENT_TYPE)&profileTrace);
		profileTrace->nodes = DynamicArrayHierarchyToStaticArrayHierarchy((PDYNAMIC_ARRAY)profileTrace->nodes);
	}

	DynamicArrayShrink(root);
	free(free);
	profileTrace = malloc(sizeof(BINARY_PROFILE_SECTION_TRACE_GENERIC)*DynamicArrayGetSize(root));
	for (size_t i = 0; i < DynamicArrayGetSize(root); ++i)
	{
		PBINARY_PROFILE_SECTION_TRACE_GENERIC temp;
		DynamicArrayGet(root, i, (PDYNAMIC_ARRAY_ELEMENT_TYPE)&temp);
		profileTrace[i] = *temp;
	}
	return profileTrace;
}

INTERNAL_FUNC
BOOL GetProfileTraceInternal(PROFILE_HANDLE profile, FILE *file, PBINARY_PROFILE_SECTION_TRACE_GENERIC profileTrace)
{
	BINARY_PROFILE_SECTION_GENERIC profileEntry;
	UINT8 type = BINARY_PROFILE_SECTION_ENTRY_TYPE_INTERNAL_INVALID;


	if (!GetProfileEntryInternal(profile, file, &profileEntry, type) ||
		profileEntry.type != BINARY_PROFILE_SECTION_ENTRY_TYPE_HEADER)
	{
		profileTrace->entry = NULL;;
		profileTrace->type = BINARY_PROFILE_SECTION_ENTRY_TYPE_INVALID;
		profileTrace->nodeCount = 0;
		profileTrace->nodes = NULL;
		return FALSE;
	}

	PDYNAMIC_ARRAY da = (PDYNAMIC_ARRAY)malloc(sizeof(DYNAMIC_ARRAY));
	DynamicArrayInit(da);

	profileTrace->entry = profileEntry.entry;
	profileTrace->type = profileEntry.type;
	profileTrace->nodes = (PBINARY_PROFILE_SECTION_TRACE_GENERIC)da;
	profileTrace->nodeCount = ConstructProfileTraceInternal(profile, file, (PDYNAMIC_ARRAY)profileTrace->nodes,
		AbpDoesProfileContainSchedulingInfo(profile) ? BINARY_PROFILE_SECTION_ENTRY_TYPE_PROCESSOR : BINARY_PROFILE_SECTION_ENTRY_TYPE_INSTRUCTION_0);

	if (profileTrace->nodeCount == -1)
	{
		DestroyDynamicArrayHierarchy((PDYNAMIC_ARRAY)profileTrace->nodes);

		profileTrace->entry = NULL;;
		profileTrace->type = BINARY_PROFILE_SECTION_ENTRY_TYPE_INVALID;
		profileTrace->nodeCount = 0;
		profileTrace->nodes = NULL;
		return FALSE;
	}
	else
	{
		// Convert dynamic arrays to fixed size arrays.
		profileTrace->nodes = DynamicArrayHierarchyToStaticArrayHierarchy((PDYNAMIC_ARRAY)profileTrace->nodes);
		return TRUE;
	}
}

DECLSPEC 
BOOL AbpGetProfileTraceFirst(PROFILE_HANDLE profile, UINT32 secIndex, PBINARY_PROFILE_SECTION_TRACE_GENERIC profileTrace)
{
	if (!profileTrace)
		return FALSE;
	FILE *file = SectionSeek(profile, secIndex, BINARY_SECTION_TYPE_PROFILE);
	if (!file)
		return FALSE;
	if (!GetProfileTraceInternal(profile, file, profileTrace))
		return FALSE;
	fpos_t pos;
	fgetpos(file, &pos);
	SetProfileTraceOffset(profileTrace, (UINT64)pos);
	SetTrace(profileTrace);
	return TRUE;
}

DECLSPEC 
BOOL AbpGetProfileTraceNext(PROFILE_HANDLE profile, UINT32 secIndex, PBINARY_PROFILE_SECTION_TRACE_GENERIC inProfileTrace, PBINARY_PROFILE_SECTION_TRACE_GENERIC outProfileTrace)
{
	if (!inProfileTrace || !outProfileTrace || !VerifyTrace(inProfileTrace) || inProfileTrace->type == BINARY_PROFILE_SECTION_ENTRY_TYPE_INVALID)
		return FALSE;
	fpos_t offset = (fpos_t)GetProfileTraceOffset(inProfileTrace);
	if (offset == -1)
		return FALSE;
	FILE *file = SectionSeekOffset(profile, secIndex, BINARY_SECTION_TYPE_PROFILE, offset);
	if (!file)
		return FALSE;
	if (!GetProfileTraceInternal(profile, file, outProfileTrace))
		return FALSE;
	fpos_t pos;
	fgetpos(file, &pos);
	SetProfileTraceOffset(outProfileTrace, (UINT64)pos);
	SetTrace(outProfileTrace);
	return TRUE;
}

DECLSPEC 
BOOL AbpDestroyProfileTrace(PROFILE_HANDLE profile, PBINARY_PROFILE_SECTION_TRACE_GENERIC profileTrace)
{
	if (!profileTrace || !VerifyTrace(profileTrace))
		return FALSE;

	BOOL result = TRUE;
	if (profileTrace->type != BINARY_PROFILE_SECTION_ENTRY_TYPE_INVALID)
	{
		BINARY_PROFILE_SECTION_GENERIC entry;
		entry.entry = profileTrace->entry;
		entry.type = profileTrace->type;
		entry.reserved = profileTrace->reserved;
		result = result && AbpDestroyProfileEntry(profile, &entry);
		for (UINT64 i = 0; i < profileTrace->nodeCount; ++i)
		{
			result = result && AbpDestroyProfileTrace(profile, &profileTrace->nodes[i]);
		}
	}

	memset(profileTrace, 0, sizeof(BINARY_PROFILE_SECTION_TRACE_GENERIC));

	return result;
}

INTERNAL_FUNC
BOOL AbpDestroyLlwSnapshotInternal(PBINARY_LLW_SECTION_ENTRY_SNAPSHOT snapshot)
{
	if (snapshot)
	{
		if (snapshot->regions)
		{
			for (UINT64 i = 0; i < snapshot->regionCount; ++i)
			{
				if (snapshot->regions[i].userId)
					free((void*)snapshot->regions[i].userId);

				if (snapshot->regions[i].blocks)
					free(snapshot->regions[i].blocks);
			}

			free(snapshot->regions);
		}

		memset(snapshot, 0, sizeof(BINARY_LLW_SECTION_ENTRY_SNAPSHOT));
	}

	return TRUE;
}

INTERNAL_FUNC
BOOL GetLlwSnapshotInternal(PROFILE_HANDLE profile, FILE *file,
	PBINARY_LLW_SECTION_ENTRY_SNAPSHOT snapshot)
{
	FReadOpenProfile(snapshot->ts, file);
	if (AbpGetProfileTargetProcessorArch(profile) == PROCESSOR_ARCH_X64)
	{
		FReadOpenProfile(snapshot->memoryPin, file);
		FReadOpenProfile(snapshot->regionCount, file);
	}
	else // PROCESSOR_ARCH_X86
	{
		snapshot->memoryPin = 0;
		snapshot->regionCount = 0;
		if (fread(&snapshot->memoryPin, sizeof(UINT32), 1, file) != 1)
			goto cleanupfail;
		if (fread(&snapshot->regionCount, sizeof(UINT32), 1, file) != 1)
			goto cleanupfail;
	}

	snapshot->regions = malloc(sizeof(BINARY_LLW_SECTION_ENTRY_REGION) * (size_t)snapshot->regionCount);
	if (!snapshot->regions)
		goto cleanupfail;

	for (UINT64 i = 0; i < snapshot->regionCount; ++i)
	{
		snapshot->regions[i].vAddr = (UINT64)NULL;
		if (fread(&snapshot->regions[i].vAddr, AbpGetProfileVirtualAddrSize(profile), 1, file) != 1)
			goto cleanupfail;

		FReadOpenProfile(snapshot->regions[i].protectionFlags, file);
		FReadOpenProfile(snapshot->regions[i].storageFlags, file);
		FReadOpenProfile(snapshot->regions[i].usageType, file);
		FReadOpenProfile(snapshot->regions[i].user, file);

		UINT16 userIdSize = 0;
		FReadOpenProfile(userIdSize, file); // Size excluding the null character.
		snapshot->regions[i].userId = malloc(userIdSize + 1); // No overflow due to automatic widening.
		if (!snapshot->regions[i].userId)
			goto cleanupfail;
		if(fread((void*)snapshot->regions[i].userId, sizeof(CHAR), userIdSize, file) != userIdSize)
			goto cleanupfail;
		((PSTR)(snapshot->regions[i].userId))[userIdSize] = (CHAR)NULL; // Terminate the string.

		if (AbpGetProfileTargetProcessorArch(profile) == PROCESSOR_ARCH_X64)
		{
			FReadOpenProfile(snapshot->regions[i].blockCount, file);
		}
		else // PROCESSOR_ARCH_X86
		{
			snapshot->regions[i].blockCount = 0;
			if (fread(&snapshot->regions[i].blockCount, sizeof(UINT32), 1, file) != 1)
				goto cleanupfail;
		}

		snapshot->regions[i].blocks = malloc(sizeof(BINARY_LLW_SECTION_ENTRY_BLOCK) *
			(size_t)snapshot->regions[i].blockCount);
		if (!snapshot->regions[i].blocks)
			goto cleanupfail;

		for (UINT64 j = 0; j < snapshot->regions[i].blockCount; ++j)
		{	
			if (fread(&snapshot->regions[i].blocks[j].vAddr, 
				AbpGetProfileVirtualAddrSize(profile), 1, file) != 1)
				goto cleanupfail;
			FReadOpenProfile(snapshot->regions[i].blocks[j].protectionFlags, file);
			FReadOpenProfile(snapshot->regions[i].blocks[j].storageFlags, file);
		}
	}

	return TRUE;
cleanupfail:
	AbpDestroyLlwSnapshotInternal(snapshot);
	return FALSE;
}

DECLSPEC 
BOOL AbpGetLlwSnapshotFirst(PROFILE_HANDLE profile, UINT32 secIndex,
	PBINARY_LLW_SECTION_ENTRY_SNAPSHOT snapshot)
{
	if (!snapshot)
		return FALSE;

	// In case GetLlwSnapshotInternal fails, AbpDestroyLlwSnapshotInternal
	// will be called. Zeroing snapshot ensures proper destruction.
	memset(snapshot, 0, sizeof(BINARY_LLW_SECTION_ENTRY_SNAPSHOT));

	FILE *file = SectionSeek(profile, secIndex, BINARY_SECTION_TYPE_LLW);
	if (!file)
		return FALSE;
	if (!GetLlwSnapshotInternal(profile, file, snapshot))
		return FALSE;
	fpos_t pos;
	fgetpos(file, &pos);
	SetLlwSnapshotOffset(snapshot, (UINT64)pos);
	SetSnapshot(snapshot);
	return TRUE;
}

DECLSPEC 
BOOL AbpGetLlwSnapshotNext(PROFILE_HANDLE profile, UINT32 secIndex,
	PBINARY_LLW_SECTION_ENTRY_SNAPSHOT inSnapshot, 
	PBINARY_LLW_SECTION_ENTRY_SNAPSHOT outSnapshot)
{
	if (!inSnapshot || !outSnapshot || !VerifySnapshot(inSnapshot))
		return FALSE;
	fpos_t offset = (fpos_t)GetLlwSnapshotOffset(inSnapshot);
	if (offset == -1)
		return FALSE;
	FILE *file = SectionSeekOffset(profile, secIndex, BINARY_SECTION_TYPE_LLW, offset);
	if (!file)
		return FALSE;
	// inSnapshot and outSnapshot may be overlapping.
	// Use a different object to be able to cleanup properly.
	BINARY_LLW_SECTION_ENTRY_SNAPSHOT outSnapshotTemp = { 0 };
	if (!GetLlwSnapshotInternal(profile, file, &outSnapshotTemp))
		return FALSE;
	fpos_t pos;
	fgetpos(file, &pos);
	SetLlwSnapshotOffset(&outSnapshotTemp, (UINT64)pos);
	SetSnapshot(&outSnapshotTemp);
	memcpy(outSnapshot, &outSnapshotTemp, sizeof(outSnapshotTemp));
	return TRUE;
}

DECLSPEC 
BOOL AbpDestroyLlwSnapshot(PBINARY_LLW_SECTION_ENTRY_SNAPSHOT snapshot)
{
	if (!snapshot || !VerifySnapshot(snapshot))
		return FALSE;

	return AbpDestroyLlwSnapshotInternal(snapshot);
}