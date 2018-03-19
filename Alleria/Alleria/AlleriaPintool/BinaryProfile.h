#ifndef BINARY_PROFILE_H
#define BINARY_PROFILE_H

#include <vector>

enum BINARY_SECION_TYPE : unsigned char
{
	BINARY_SECION_TYPE_INVALID,
	BINARY_SECION_TYPE_CPUID,
	BINARY_SECION_TYPE_PROFILE,
	BINARY_SECION_TYPE_LLW,
	BINARY_SECION_TYPE_INTERCEPTORS,
	BINARY_SECION_TYPE_PROCESS_FAMILY,
	BINARY_SECION_TYPE_IMAGE_TABLE,
	BINARY_SECION_TYPE_FUNC_TABLE,
	BINARY_SECION_TYPE_TYPES
};

struct CPUID_INFO
{
	int EAX; // leaf (Intel) or function id (AMD).
	int ECX; // subleaf (Intel) or subfunction id (AMD).
	int cpuInfo[4];
};

struct BINARY_SECION_CPUID
{
	unsigned int nIds;
	unsigned int nExIds;
	std::vector<CPUID_INFO> cpuidInfo;
};

// TODO: We assume that an enum is 4 byte by default. But this is compiler dependent.
enum PROCESSOR_VENDOR
{
	PROCESSOR_VENDOR_INVALID,
	PROCESSOR_VENDOR_UNKNOWN,
	PROCESSOR_VENDOR_GENUINE_INTEL,
	PROCESSOR_VENDOR_AUTHENTIC_AMD
};

// TODO: We assume that an enum is 4 byte by default. But this is compiler dependent.
enum PROCESSOR_ARCH
{
	PROCESSOR_ARCH_INVALID,
	PROCESSOR_ARCH_X86,
	PROCESSOR_ARCH_X64
};

PROCESSOR_VENDOR GetProcessorVendor();
void GetProcessorIdentification(BINARY_SECION_CPUID& pi);

// TODO: We assume that an enum is 4 byte by default. But this is compiler dependent.
enum BINARY_HEADER_TYPE
{
	BINARY_HEADER_TYPE_INVALID_PROFILE,
	BINARY_HEADER_TYPE_MAIN_PROFILE = 0x01,
	BINARY_HEADER_TYPE_SUB_PROFILE = 0x02
};

// TODO: We assume that an enum is 4 byte by default. But this is compiler dependent.
enum BINARY_HEADER_OS
{
	BINARY_HEADER_OS_INVALID,
	BINARY_HEADER_OS_WINDOWS,
	BINARY_HEADER_OS_LINUX
};

struct SECTION_DIRECTORY_ENTRY
{
	// offset : 56, BINARY_SECION_TYPE type : 8.
	unsigned long long data;
};

#ifdef TARGET_WINDOWS
#pragma pack(push)
#pragma pack(1)
#endif /* TARGET_WINDOWS */

struct BINARY_HEADER
{
	char                                    *Id = "HadiBrais";
	char                                    *version = "1.0";
#ifdef TARGET_IA32
	UINT64									dummyField; // This is used to make BINARY_HEADER_offsetof work.
#endif
	BINARY_HEADER_TYPE                      type;
	BINARY_HEADER_OS                        os;
	PROCESSOR_VENDOR                        processorVendor;
	PROCESSOR_ARCH                          processorArch;
	PROCESSOR_ARCH                          targetArch;
	UINT64                                  timestamp; // time_t has no well-defined size.
	UINT32                                  processNumber;
	UINT32                                  processOsId;
	UINT64                                  timeFreq;
	UINT64                                  startingTime; // time_t has no well-defined size.
	UINT64                                  endingTime; // time_t has no well-defined size.
	UINT64                                  instructionCount;
	UINT64                                  memoryInstructionCount;
	char                                    *mainExePath;
	char                                    *systemDirectory;
	char                                    *currentDirectory;
	char                                    *cmdLine;
	char                                    *osVersion;
	UINT8                                   vAddrSize; // In bytes.
	UINT8                                   pAddrSize; // In bytes.
	UINT32                                  pageSize; // In bytes.
	void                                    *minAppAddr;
	void                                    *maxAppAddr;
	UINT32                                  totalDram; // In mebibytes.

	// Only least signifcant bit is used (KnobOuputInstructionEnabled).
	// The rest of the bits are reserved.
	UINT32                                  flags;

	char                                    reserved[64]; // First half for Alleria, second half for users.
	UINT32                                  dirSize; // Number of entries in section directory.
	std::vector<SECTION_DIRECTORY_ENTRY>    sectionDirectory;
}

#ifndef TARGET_WINDOWS
__attribute__((packed))
#endif

;

#ifdef TARGET_WINDOWS
#pragma pack(pop)
#endif /* TARGET_WINDOWS */

// The -2 is because the first two fields (Id and version) are of size 16 (8 on 32-bit)
// in memory but 14 on disk.
#define BINARY_HEADER_offsetof(field) (offsetof(BINARY_HEADER, field) - 2)


struct BINARY_PROCESS_FAMILY_ENTRY
{
	unsigned int  pn;
	unsigned int  pId;
	time_t t;
};

struct BINARY_PROCESS_FAMILY
{
	std::vector<BINARY_PROCESS_FAMILY_ENTRY> procs;
};

#endif /* BINARY_PROFILE_H */