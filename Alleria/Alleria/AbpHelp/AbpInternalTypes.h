#ifndef ABP_INTERNAL_TYPES_H
#define ABP_INTERNAL_TYPES_H

#include "AbpTypes.h"

typedef struct SECTION_DIRECTORY_ENTRY_INTERNAL_TAG
{
	// offset : 56, BINARY_SECION_TYPE type : 8.
	UINT64 data;
} SECTION_DIRECTORY_ENTRY_INTERNAL, *PSECTION_DIRECTORY_ENTRY_INTERNAL;

typedef struct BINARY_HEADER_TAG
{
	BINARY_HEADER_TYPE                      type;
	BINARY_HEADER_OS                        os;
	PROCESSOR_VENDOR                        processorVendor;
	PROCESSOR_ARCH                          processorArch;
	PROCESSOR_ARCH                          targetArch;
	time_t                                  timestamp;
	UINT32                                  processNumber;
	UINT32                                  processOsId;
	UINT64                                  timeFreq;
	time_t                                  startingTime;
	time_t                                  endingTime;
	UINT64                                  insCount;
	UINT64                                  memInsCount;
	PSTR                                    mainExePath;
	PSTR                                    systemDirectory;
	PSTR                                    currentDirectory;
	PSTR                                    cmdLine;
	PSTR                                    osVersion;
	UINT8                                   vAddrSize; // In bytes.
	UINT8                                   pAddrSize; // In bytes.
	UINT32                                  pageSize; // In bytes.
	UINT64                                  minAppAddr;
	UINT64                                  maxAppAddr;
	UINT32                                  totalDram; // In mebibytes.

	// Bit 0 is used for KnobOuputInstructionEnabled.
	// Bit 1 is used for TrackProcessorsEnabled.
	// The rest of the bits are reserved.
	UINT32                                  flags;

	CHAR                                    reserved[64]; // First half for Alleria, second half for users.
	UINT32                                  dirSize; // Number of entries in section directory.
} BINARY_HEADER, *PBINARY_HEADER;


typedef enum BINARY_PROFILE_SECTION_ENTRY_TYPE_INTERNAL_TAG
{
	BINARY_PROFILE_SECTION_ENTRY_TYPE_INTERNAL_INVALID,
	BINARY_PROFILE_SECTION_ENTRY_TYPE_INTERNAL_INSTRUCTION_0,
	BINARY_PROFILE_SECTION_ENTRY_TYPE_INTERNAL_INSTRUCTION_1,
	BINARY_PROFILE_SECTION_ENTRY_TYPE_INTERNAL_INSTRUCTION_2,
	BINARY_PROFILE_SECTION_ENTRY_TYPE_INTERNAL_MEMORY_0,
	BINARY_PROFILE_SECTION_ENTRY_TYPE_INTERNAL_MEMORY_1,
	BINARY_PROFILE_SECTION_ENTRY_TYPE_INTERNAL_MEMORY_2,
	BINARY_PROFILE_SECTION_ENTRY_TYPE_INTERNAL_PROCESSOR,
	BINARY_PROFILE_SECTION_ENTRY_TYPE_INTERNAL_SYS_CALL_RECORD,
	BINARY_PROFILE_SECTION_ENTRY_TYPE_INTERNAL_BRANCH_RECORD,
	BINARY_PROFILE_SECTION_ENTRY_TYPE_INTERNAL_VGATHER_0,
	BINARY_PROFILE_SECTION_ENTRY_TYPE_INTERNAL_VGATHER_1,
	BINARY_PROFILE_SECTION_ENTRY_TYPE_INTERNAL_XSAVE_0,
	BINARY_PROFILE_SECTION_ENTRY_TYPE_INTERNAL_XSAVE_1,
	BINARY_PROFILE_SECTION_ENTRY_TYPE_INTERNAL_HEADER

} BINARY_PROFILE_SECTION_ENTRY_TYPE_INTERNAL;

typedef struct BINARY_PROCESS_FAMILY_TAG
{
	BINARY_PROCESS_FAMILY_ENTRY *procs;
} BINARY_PROCESS_FAMILY, *PBINARY_PROCESS_FAMILY;

typedef struct BINARY_SECION_CPUID_TAG
{
	UINT32 nIds;
	UINT32 nExIds;
	CPUID_INFO *cpuidInfo;
} BINARY_SECION_CPUID, *PBINARY_SECION_CPUID;

typedef UINT32 PROFILE_HANDLE_TABLE_SIZE;

typedef struct PROFILE_OBJECT_POOL_TAG
{
	BOOL headerValid;
	BINARY_PROFILE_SECTION_ENTRY_HEADER header;

	BOOL procValid;
	BINARY_PROFILE_SECTION_ENTRY_PROC proc;

	BOOL insValid;
	BINARY_PROFILE_SECTION_ENTRY_INS ins;

	BOOL memValid;
	BINARY_PROFILE_SECTION_ENTRY_MEM mem;

	BOOL sysCallValid;
	BINARY_PROFILE_SECTION_ENTRY_SYSCALL sysCall;

	BOOL branchValid;
	BINARY_PROFILE_SECTION_ENTRY_BRANCH branch;

} PROFILE_OBJECT_POOL, *PPROFILE_OBJECT_POOL;

typedef struct PROFILE_INTERNALS_TAG
{
	PSTR profilePath;
	FILE      *file;
	BINARY_HEADER header;
	PSECTION_DIRECTORY_ENTRY secDir;
	PROFILE_OBJECT_POOL objectPool; // This simple object pool improves perf by about 3x.
} PROFILE_INTERNALS, *PPROFILE_INTERNALS;

#define INTERNAL_FUNC
#define PROFILE_ENTRY_FLAG 0x1
#define PROFILE_TRACE_FLAG 0x2
#define LLW_SNAPSHOT_FLAG 0x3

#endif /* ABP_INTERNAL_TYPES_H */