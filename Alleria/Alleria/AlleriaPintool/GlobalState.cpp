#include "GlobalState.h"
#include <cstdlib>

UINT32 GLOBAL_STATE::s_childWaitTimeOut;
BOOL GLOBAL_STATE::processingThreadRunning;
std::ofstream GLOBAL_STATE::gfstream;
std::map<UINT32, UINT32> GLOBAL_STATE::gRoutineMap;
std::wofstream GLOBAL_STATE::interceptorFileStreamText;
std::ofstream GLOBAL_STATE::interceptorFileStreamBinary;
std::ofstream GLOBAL_STATE::walkerFileStream;
PIN_LOCK GLOBAL_STATE::gAppThreadLock;
PIN_LOCK GLOBAL_STATE::gProcThreadLock;
std::vector<std::ofstream*> GLOBAL_STATE::localStreams;
std::vector<PIN_THREAD_UID> GLOBAL_STATE::processingThreadsIds;
BUFFER_LIST_MANAGER GLOBAL_STATE::fullBuffersListManager;
BUFFER_LIST_MANAGER GLOBAL_STATE::freeBuffersListManager;
UINT64 GLOBAL_STATE::insCount;
UINT64 GLOBAL_STATE::insMemCount;
ADDRINT GLOBAL_STATE::currentProcessHandle;
std::string GLOBAL_STATE::s_myDumpDirectory;
GLOBAL_STATE::PpndbEntry *GLOBAL_STATE::myPpndbEntry;
std::vector<std::pair<UINT32, TIME_STAMP> > GLOBAL_STATE::childProcesses;
xed_syntax_enum_t GLOBAL_STATE::s_asmSyntax;
TIME_STAMP GLOBAL_STATE::totalAppThreadWaitingTime;
TIME_STAMP GLOBAL_STATE::totalAppThreadCount;
BINARY_HEADER GLOBAL_STATE::s_binaryHeader;
BINARY_PROCESS_FAMILY GLOBAL_STATE::s_binaryProcFamily;
std::streampos GLOBAL_STATE::s_globalFuncSecOffset;
std::streampos GLOBAL_STATE::s_globalImgSecOffset;
std::streampos GLOBAL_STATE::s_globalProcFamilyOffset;
std::streampos GLOBAL_STATE::s_funcSecOffset;
std::streampos GLOBAL_STATE::s_imgSecOffset;
PIN_THREAD_UID GLOBAL_STATE::timerthreadUID;
PIN_THREAD_UID GLOBAL_STATE::walkerthreadUID;
volatile BOOL GLOBAL_STATE::isAppExisting = FALSE;
TIME_STAMP GLOBAL_STATE::totalAppThreadRunningWaitingTime;
TIME_STAMP GLOBAL_STATE::totalProcessingThreadRunningWaitingTime;
PIN_SEMAPHORE GLOBAL_STATE::tunerSemaphore;
PIN_THREAD_UID GLOBAL_STATE::tuidTunerControl;
BOOL GLOBAL_STATE::tuidTunerControlResume;

UINT32 g_processNumber;
BOOL g_isGenesis;
const CHAR *g_mainExe;
V2P_TRANS_MODE g_v2pTransMode;
OUTPUT_FORMAT_MODE g_outputFormat;
UINT8 g_virtualAddressSize;
UINT8 g_physicalAddressSize;
std::string g_outputDir;

const UINT8 g_binSecDirFuncIndexMain = 2;
const UINT8 g_binSecDirImgIndexMain = 3;
const UINT8 g_binSecDirFuncIndexSub = 1;
const UINT8 g_binSecDirImgIndexSub = 2;

static std::vector<PORTABLE_PROCESSOR_PACKAGE> g_processors;

#ifdef TARGET_WINDOWS
ODS_MODE g_odsMode;
#endif /* TARGET_WINDOWS */

// Maximum size of PPNDB in bytes.
#define PPNDB_MAX_SIZE (1 << 16)

#define SECTION_DIRECTORY_ENTRY_SET_TYPE(entry, value) \
entry.data = (entry.data & 0xFFFFFFFFFFFFFF00) | (value & 0xFF)

#define SECTION_DIRECTORY_ENTRY_SET_OFFSET(entry, value) \
entry.data = (entry.data & 0xFF) | (value << 8)

#define SECTION_DIRECTORY_ENTRY_SET(entry, type, offset) \
SECTION_DIRECTORY_ENTRY_SET_TYPE(entry, type); \
SECTION_DIRECTORY_ENTRY_SET_OFFSET(entry, offset)

#define BUFFER_MIN_SIZE_KB 1
#define BUFFER_MAX_SIZE_KB 1024*1024

/* 

Alleria Knobs Definitions

*/

#define AlleriaKnobFamily              "pintool"

#define NumKBBufferSwitch              "bs"
#define NumberOfBuffersSwitch          "bc"
#define NumProcessingThreadsSwitch     "ptc"
#define ChildWaitTimeOutSecondsSwitch  "cwto"
#define AsmStyleSwitch                 "asm"
#define OutputFormatSwitch             "of"
#define LLWEnabledSwitch               "llw"
#define TimerEnabledSwitch             "ta"
#define OuputInstructionEnabledSwitch  "oi"
#define TimerFreqSwitch                "tf"
#define DumpDirSwitch                  "dir"
#define V2PTransModeSwitch             "v2p"
#define V2POutputDebugStringModeSwitch "ods"
#define TrackProcessorsSwitch          "tp"
#define ConfigDirSwitch                "cfg"
#define TunerSwitch                    "tuner"

#define NumKBBufferDefault              4096
#define NumberOfBuffersDefault          5
#define NumProcessingThreadsDefault     4
#define ChildWaitTimeOutSecondsDefault  30
#define AsmStyleDefault                 "intel"
#define OutputFormatDefault             "txt"
#define LLWEnabledDefault               "false"
#define TimerEnabledDefault             "true"
#define OuputInstructionEnabledDefault  "true"
#define TimerFreqDefault                10000
#define DumpDirDefault                  ""
#define V2PTransModeDefault             "off"
#define V2POutputDebugStringModeDefault "asrequired"
#define TrackProcessorsDefault          "false"
#define ConfigDirDefault                ""
#define TunerDefault                    "false"

#define NumKBBufferDesc              "number of kibibytes in a buffer, default is 64"
#define NumberOfBuffersDesc          "number of buffers to allocate, default is 5. On Windows 10+, it must be at least 2 " \
									 " or maybe more because the OS creates dedicated threads that load libraries (parallel loader)."
#define NumProcessingThreadsDesc     "number of processing threads, default is 4"
#define ChildWaitTimeOutSecondsDesc  "number of seconds to wait for child processes to initialize, default is 30"
#define AsmStyleDesc                 "syntax of assembly instructions: intel, att or xed, default is intel"
#define OutputFormatDesc             "format of the output file: txt for textual, bin for binary, default is txt"
#define LLWEnabledDesc               "enable or disable the live-lands walker, default is false"
#define TimerEnabledDesc             "enable or disable the timer thread, default is true"
#define OuputInstructionEnabledDesc  "enable or disable emitting instructions in profiles, default is true"
#define TimerFreqDesc                "timer resolution in milliseconds, default is 10000"
#define DumpDirDesc                  "directory in which all output files will be stored"
#define V2PTransModeDesc             "mode of virtual address translation: off, fast or accurate, default is off"
#define V2POutputDebugStringModeDesc "mode of OutputDebugString: asrequired or suppress. Windows only, default is asrequired"
#define TrackProcessorsDesc          "enable or disable recording the processor on which a thread is executing, default is false"
#define ConfigDirDesc                "directory in which the configuration file resides"
#define TunerDesc                    "enable or disable adaptive profiling. bc and ptc are ignored"

KNOB<UINT32> KnobNumKBBuffer(
	KNOB_MODE_WRITEONCE, 
	AlleriaKnobFamily, 
	NumKBBufferSwitch,
	LITERAL_TO_CHAR_PTR(NumKBBufferDefault),
	NumKBBufferDesc);

KNOB<UINT32> KnobNumberOfBuffers(
	KNOB_MODE_WRITEONCE, 
	AlleriaKnobFamily, 
	NumberOfBuffersSwitch,
	LITERAL_TO_CHAR_PTR(NumberOfBuffersDefault),
	NumberOfBuffersDesc);

KNOB<UINT32> KnobNumProcessingThreads(
	KNOB_MODE_OVERWRITE,
	AlleriaKnobFamily,
	NumProcessingThreadsSwitch,
	LITERAL_TO_CHAR_PTR(NumProcessingThreadsDefault),
	NumProcessingThreadsDesc);

KNOB<UINT32> KnobChildWaitTimeOut(
	KNOB_MODE_OVERWRITE,
	AlleriaKnobFamily,
	ChildWaitTimeOutSecondsSwitch,
	LITERAL_TO_CHAR_PTR(ChildWaitTimeOutSecondsDefault),
	ChildWaitTimeOutSecondsDesc);

KNOB<string> KnobAsmStyle(
	KNOB_MODE_OVERWRITE,
	AlleriaKnobFamily,
	AsmStyleSwitch,
	AsmStyleDefault,
	AsmStyleDesc);

KNOB<string> KnobOutputFormat(
	KNOB_MODE_OVERWRITE,
	AlleriaKnobFamily,
	OutputFormatSwitch,
	OutputFormatDefault,
	OutputFormatDesc);

KNOB<BOOL> KnobLLWEnabled(
	KNOB_MODE_OVERWRITE,
	AlleriaKnobFamily,
	LLWEnabledSwitch,
	LLWEnabledDefault,
	LLWEnabledDesc);

KNOB<BOOL> KnobTimerEnabled(
	KNOB_MODE_OVERWRITE,
	AlleriaKnobFamily,
	TimerEnabledSwitch,
	TimerEnabledDefault,
	TimerEnabledDesc);

KNOB<BOOL> KnobOuputInstructionEnabled(
	KNOB_MODE_OVERWRITE,
	AlleriaKnobFamily,
	OuputInstructionEnabledSwitch,
	OuputInstructionEnabledDefault,
	OuputInstructionEnabledDesc);

KNOB<UINT32> KnobTimerFreq(
	KNOB_MODE_OVERWRITE,
	AlleriaKnobFamily,
	TimerFreqSwitch,
	LITERAL_TO_CHAR_PTR(TimerFreqDefault),
	TimerFreqDesc);

KNOB<string> KnobDumpDir(
	KNOB_MODE_OVERWRITE,
	AlleriaKnobFamily,
	DumpDirSwitch,
	DumpDirDefault,
	DumpDirDesc);

KNOB<string> KnobV2PTransMode(
	KNOB_MODE_OVERWRITE,
	AlleriaKnobFamily,
	V2PTransModeSwitch,
	V2PTransModeDefault,
	V2PTransModeDesc);

#ifdef TARGET_WINDOWS
KNOB<string> KnobOutputDebugStringMode(
	KNOB_MODE_OVERWRITE,
	AlleriaKnobFamily,
	V2POutputDebugStringModeSwitch,
	V2POutputDebugStringModeDefault,
	V2POutputDebugStringModeDesc);
#endif /* TARGET_WINDOWS */

KNOB<BOOL> KnobTrackProcessorsEnabled(
	KNOB_MODE_OVERWRITE,
	AlleriaKnobFamily,
	TrackProcessorsSwitch,
	TrackProcessorsDefault,
	TrackProcessorsDesc);

KNOB<string> KnobConfigDir(
	KNOB_MODE_OVERWRITE,
	AlleriaKnobFamily,
	ConfigDirSwitch,
	ConfigDirDefault,
	ConfigDirDesc);

KNOB<BOOL> KnobTunerEnabled(
	KNOB_MODE_WRITEONCE,
	AlleriaKnobFamily,
	TunerSwitch,
	TunerDefault,
	TunerDesc);

/*

End Alleria Knobs Definitions

*/

#ifdef TARGET_WINDOWS
#define PROCESS_TREE_MUTEX_NAME "AlleriaPpndbMutex"
#define PROCESS_TREE_FILE_MAPPING_NAME "AlleriaPpndb"
#elif defined(TARGET_LINUX)
#define PROCESS_TREE_MUTEX_NAME "/AlleriaPpndbMutex"
#define PROCESS_TREE_FILE_MAPPING_NAME "/AlleriaPpndb"
#endif /* TARGET_WINDOWS */

xed_syntax_enum_t Str2AsmSyntax(const CHAR *str);

void PathCombine(std::string& output, const std::string& path1, const std::string& path2)
{
	output = "";

	if (path1.length() > 0)
	{
		if (path1[path1.length() - 1] != PORTABLE_PATH_DELIMITER[0])
		{
			output = path1 + PORTABLE_PATH_DELIMITER;
		}
		else
		{
			output = path1;
		}
	}

	output += path2;
}

static BOOL IsKnobV2PTransModeValid()
{
	g_v2pTransMode = V2P_TRANS_MODE_INVALID;
	if (strcmp(KnobV2PTransMode.Value().c_str(), "off") == 0)
		g_v2pTransMode = V2P_TRANS_MODE_OFF;
	else if (strcmp(KnobV2PTransMode.Value().c_str(), "fast") == 0)
		g_v2pTransMode = V2P_TRANS_MODE_FAST;
	else if (strcmp(KnobV2PTransMode.Value().c_str(), "accurate") == 0)
		g_v2pTransMode = V2P_TRANS_MODE_ACCURATE;
	else
		return FALSE;
	return TRUE;
}

static BOOL IsKnobOutputFormatValid()
{
	g_outputFormat = OUTPUT_FORMAT_MODE_INVALID;
	if (strcmp(KnobOutputFormat.Value().c_str(), "txt") == 0)
		g_outputFormat = OUTPUT_FORMAT_MODE_TEXTUAL;
	else if (strcmp(KnobOutputFormat.Value().c_str(), "bin") == 0)
		g_outputFormat = OUTPUT_FORMAT_MODE_BINARY;
	else
		return FALSE;
	return TRUE;
}

static BOOL IsKnobOdsModeValid()
{
#ifdef TARGET_WINDOWS
	g_odsMode = ODS_MODE_INVALID;
	if (strcmp(KnobOutputDebugStringMode.Value().c_str(), "asrequired") == 0)
		g_odsMode = ODS_MODE_AS_REQUIRED;
	else if (strcmp(KnobOutputDebugStringMode.Value().c_str(), "suppress") == 0)
		g_odsMode = ODS_MODE_SUPPRESS;
	else
		return FALSE;
#endif /* TARGET_WINDOWS */
	return TRUE;
}

static BOOL IsKnobAsmSyntaxValid()
{
	xed_syntax_enum_t s = Str2AsmSyntax(KnobAsmStyle.Value().c_str());
	if (s == xed_syntax_enum_t::XED_SYNTAX_INVALID)
		return false;
	else
	{
		GLOBAL_STATE::s_asmSyntax = s;
		return true;
	}
}

static UINT32 GetBinaryProfileHeaderFlags()
{
	UINT32 flags = 0;
	flags = flags | (UINT32)KnobOuputInstructionEnabled;
	flags = flags | (UINT32)(KnobTrackProcessorsEnabled << 1);
	return flags;
}

xed_syntax_enum_t GLOBAL_STATE::GetAsmSyntaxStyle()
{
	return s_asmSyntax;
}

VOID* GLOBAL_STATE::GetPpndbBase()
{
	bool alreadExists;
	void *base = CreateNamedFileMapping(PPNDB_MAX_SIZE, PROCESS_TREE_FILE_MAPPING_NAME, &alreadExists);

	if (!base)
	{
		AlleriaSetLastError(ALLERIA_ERROR_INIT_FAILED);
		return NULL;
	}

	if (alreadExists)
	{

	}
	else
	{
		if (!EnsureCommited(base, GetVirtualPageSize()))
			return NULL;
	}

	return base;
}

UINT32 GLOBAL_STATE::ComputeMyProcessNumber(UINT32 pid, UINT32 ppid)
{
	void *base = GetPpndbBase();
	if (!base)
	{
		AlleriaSetLastError(ALLERIA_ERROR_INIT_FAILED);
		return PROCESS_INVALID_ID;
	}

	if (pid == PROCESS_INVALID_ID || ppid == PROCESS_INVALID_ID)
	{
		AlleriaSetLastError(ALLERIA_ERROR_INIT_FAILED);
		return PROCESS_INVALID_ID;
	}

	PMUTEX mutex = Mutex_CreateNamed(PROCESS_TREE_MUTEX_NAME);
	Mutex_Acquire(mutex);

	Ppndb *ppndbBase = reinterpret_cast<Ppndb*>(base);
	myPpndbEntry = NULL;
	PpndbEntry *ppndbEntry = &ppndbBase->base + ppndbBase->count - 1;
	for (UINT32 i = 0; i < ppndbBase->count; ++i)
	{
		if (ppndbEntry->pid == pid && ppndbEntry->endingTime == -1)
		{
			myPpndbEntry = ppndbEntry;
			break;
		}
		--ppndbEntry;
	}

	if (myPpndbEntry)
	{
		// Nothing to do here. My parent initialized me.
	}
	else
	{
		// This process is a genesis process.
		// It has to initialzie itself.
		PpndbEntry *ppndbEntry = &ppndbBase->base + ppndbBase->count;

		if (reinterpret_cast<UINT8*>(ppndbEntry) >= (reinterpret_cast<UINT8*>(base)+PPNDB_MAX_SIZE))
		{
			AlleriaSetLastError(ALLERIA_ERROR_INIT_FAILED);
			return PROCESS_INVALID_ID;
		}
		else
		{
			if (!EnsureCommited(ppndbEntry, GetVirtualPageSize()))
			{
				AlleriaSetLastError(ALLERIA_ERROR_INIT_FAILED);
				return PROCESS_INVALID_ID;
			}
		}

		ppndbEntry->isGenesis = TRUE;
		ppndbEntry->reference = 1;
		ppndbEntry->processNumber = 1;
		ppndbEntry->pid = pid;

		myPpndbEntry = ppndbEntry;
		++ppndbBase->count;
	}

	Mutex_Release(mutex);
	Mutex_Close(mutex);

	GetCurrentTimestamp(myPpndbEntry->startingTime);
	myPpndbEntry->endingTime = 0;

	return myPpndbEntry->processNumber;
}

UINT32 GLOBAL_STATE::ComputeChildProcessNumber(UINT32 pid, UINT32 ppid)
{
	void *base = GetPpndbBase();
	if (!base)
	{
		AlleriaSetLastError(ALLERIA_ERROR_INIT_FAILED);
		return PROCESS_INVALID_ID;
	}

	if (pid == PROCESS_INVALID_ID || ppid == PROCESS_INVALID_ID)
	{
		AlleriaSetLastError(ALLERIA_ERROR_INIT_FAILED);
		return PROCESS_INVALID_ID;
	}

	PMUTEX mutex = Mutex_CreateNamed(PROCESS_TREE_MUTEX_NAME);
	Mutex_Acquire(mutex);

	Ppndb *ppndbBase = reinterpret_cast<Ppndb*>(base);
	PpndbEntry *ppndbEntry = &ppndbBase->base + ppndbBase->count;

	if (reinterpret_cast<UINT8*>(ppndbEntry) >= (reinterpret_cast<UINT8*>(base)+PPNDB_MAX_SIZE))
	{
		AlleriaSetLastError(ALLERIA_ERROR_INIT_FAILED);
		return PROCESS_INVALID_ID;
	}
	else
	{
		if (!EnsureCommited(ppndbEntry, GetVirtualPageSize()))
		{
			AlleriaSetLastError(ALLERIA_ERROR_INIT_FAILED);
			return PROCESS_INVALID_ID;
		}
	}

	PpndbEntry *parentPpndbEntry = (myPpndbEntry->isGenesis) ? myPpndbEntry : (&ppndbBase->base + myPpndbEntry->reference);
	UINT32 parentIndex = (UINT32)((parentPpndbEntry - &ppndbBase->base) / sizeof(PpndbEntry));

	ppndbEntry->processNumber = parentPpndbEntry->reference + 1;
	ppndbEntry->reference = parentIndex;
	ppndbEntry->pid = pid;
	ppndbEntry->isGenesis = FALSE;
	ppndbEntry->endingTime = -1;
	ppndbEntry->startingTime = 0;

	parentPpndbEntry->reference = ppndbEntry->processNumber;
	++ppndbBase->count;

	Mutex_Release(mutex);
	Mutex_Close(mutex);

	return ppndbEntry->processNumber;
}

VOID GLOBAL_STATE::InitProcessDumpDirectory()
{
	std::string directory;
	if (KnobDumpDir.Value().size() == 0)
	{
		directory = PathToDirectory(PIN_ToolFullPath());
	}
	else
	{
		// TODO: Support dirs with spaces by accepting quoted strings.
		directory = KnobDumpDir.Value();
	}
	g_outputDir = directory;
}

BOOL GLOBAL_STATE::ValidateKnobs()
{
	BOOL failed = FALSE;
	if (KnobNumKBBuffer < BUFFER_MIN_SIZE_KB || KnobNumKBBuffer > BUFFER_MAX_SIZE_KB)
	{
		AlleriaSetLastError(ALLERIA_ERROR_KNOB_NumKBInMemRefBuffer);
		failed = TRUE;
	}
	else if (KnobNumberOfBuffers == 0)
	{
		AlleriaSetLastError(ALLERIA_ERROR_KNOB_NumBuffersPerAppThread);
		failed = TRUE;
	}
	else if (!IsKnobAsmSyntaxValid())
	{
		AlleriaSetLastError(ALLERIA_ERROR_KNOB_AsmStyle);
		failed = TRUE;
	}
	else if (!IsKnobOutputFormatValid())
	{
		AlleriaSetLastError(ALLERIA_ERROR_KNOB_OutputFormat);
		failed = TRUE;
	}
	else if (KnobTimerFreq == 0)
	{
		AlleriaSetLastError(ALLERIA_ERROR_KNOB_TimerFreq);
		failed = TRUE;
	}
	else if (!IsKnobV2PTransModeValid())
	{
		AlleriaSetLastError(ALLERIA_ERROR_KNOB_V2PTransMode);
		failed = TRUE;
	}
	else if (!IsKnobOdsModeValid())
	{
		AlleriaSetLastError(ALLERIA_ERROR_KNOB_OutputDebugStringMode);
		failed = TRUE;
	}
	else if (KnobTunerEnabled && !KnobTimerEnabled)
	{
		AlleriaSetLastError(ALLERIA_ERROR_KNOB_TimerRequired);
		failed = TRUE;
	}
	else if (KnobTunerEnabled && KnobOutputFormat.ValueString() == "txt")
	{
		AlleriaSetLastError(ALLERIA_ERROR_KNOB_TUNER_NO_TXT);
		failed = TRUE;
	}
	return failed;
}

VOID GLOBAL_STATE::InitBinaryHeader()
{
	GetSystemInfo(s_binaryHeader);

	s_binaryHeader.pAddrSize = (g_v2pTransMode == V2P_TRANS_MODE_OFF ? 0 : s_binaryHeader.pAddrSize);

	s_binaryHeader.processNumber = myPpndbEntry->processNumber;
	s_binaryHeader.processorVendor = GetProcessorVendor();
	s_binaryHeader.type = BINARY_HEADER_TYPE_MAIN_PROFILE;
	s_binaryHeader.timestamp = time(0);
	s_binaryHeader.flags = GetBinaryProfileHeaderFlags();
#if defined(TARGET_IA32)
	s_binaryHeader.targetArch = PROCESSOR_ARCH_X86;
#else
	s_binaryHeader.targetArch = PROCESSOR_ARCH_X64;
#endif

	InitBinaryHeaderSectionDirectory();
}

BOOL GLOBAL_STATE::InitOutputFilesTxt()
{
	std::string fullPathStr;

	PathCombine(fullPathStr, s_myDumpDirectory, "g.ap");
	gfstream.open(fullPathStr.c_str(), fomodetxt);
	gfstream << std::hex;

	PathCombine(fullPathStr, s_myDumpDirectory, "intercept.ap");
	interceptorFileStreamText.open(fullPathStr.c_str(), fomodetxt);
	interceptorFileStreamText << std::hex;

	PathCombine(fullPathStr, s_myDumpDirectory, "walker.ap");
	walkerFileStream.open(fullPathStr.c_str(), fomodetxt);
	walkerFileStream << std::hex;

	if (!gfstream.good() || !interceptorFileStreamText.good() || !walkerFileStream.good())
	{
		AlleriaSetLastError(ALLERIA_ERROR_CANNOT_CREATE_PROFILE);
		return false;
	}

	//for (UINT32 i = 0; i < KnobNumProcessingThreads; ++i)
	//{
	//	std::string temp;
	//	IntegerToString(i + 1, temp);
	//	PathCombine(fullPathStr, s_myDumpDirectory, temp + ".ap");
	//	std::ofstream *stream = new std::ofstream(fullPathStr.c_str(), fomode);
	//	
	//	if (!(*stream).good())
	//	{
	//		AlleriaSetLastError(ALLERIA_ERROR_CANNOT_CREATE_PROFILE);
	//		return false; // Process terminates.
	//	}
	//	*stream << std::hex;
	//	localStreams.push_back(stream);
	//}

	return true;
}

VOID GLOBAL_STATE::WriteBinaryHeader(std::ofstream& ofs, BINARY_HEADER_TYPE type)
{
	// Write binary header

	ofs.write(s_binaryHeader.Id, strlen(s_binaryHeader.Id) + 1); // 0 + 10 bytes.
	ofs.write(s_binaryHeader.version, strlen(s_binaryHeader.version) + 1); // 10 + 4 bytes.
	WriteBinary(ofs, type); // 14 + 4 bytes. Don't use s_binaryHeader.type here.
	WriteBinary(ofs, s_binaryHeader.os); // 18 + 4 bytes.
	WriteBinary(ofs, s_binaryHeader.processorVendor); // 22 + 4 bytes.
	WriteBinary(ofs, s_binaryHeader.processorArch); // 26 + 4 bytes.
	WriteBinary(ofs, s_binaryHeader.targetArch); // 30 + 4 bytes.
	
	// time_t could be of any size. Convert to 64-bit.
	UINT64 portabletime = s_binaryHeader.timestamp;
	WriteBinary(ofs, portabletime); // 34 + 8 bytes.
	
	WriteBinary(ofs, s_binaryHeader.processNumber); // 42 + 4 bytes.
	WriteBinary(ofs, s_binaryHeader.processOsId); // 46 + 4 bytes.
	WriteBinary(ofs, s_binaryHeader.timeFreq); // 50 + 8 bytes.
	
    // time_t could be of any size. Convert to 64-bit.
	portabletime = s_binaryHeader.startingTime;
	WriteBinary(ofs, s_binaryHeader.startingTime); // To be patched later. At offset 58 = BINARY_HEADER_STARTING_TIME_OFFSET.
	
	// time_t could be of any size. Convert to 64-bit.
	portabletime = s_binaryHeader.endingTime;
	WriteBinary(ofs, s_binaryHeader.endingTime); // To be patched later. At offset 66 = BINARY_HEADER_ENDING_TIME_OFFSET.
	
	WriteBinary(ofs, s_binaryHeader.instructionCount); // To be patched later. At offset 74 = BINARY_HEADER_INS_COUNT_OFFSET.
	WriteBinary(ofs, s_binaryHeader.memoryInstructionCount); // To be patched later. At offset 82 = BINARY_HEADER_MEM_INS_COUNT_OFFSET.
	ofs.write(s_binaryHeader.mainExePath, strlen(s_binaryHeader.mainExePath) + 1);
	ofs.write(s_binaryHeader.systemDirectory, strlen(s_binaryHeader.systemDirectory) + 1);
	ofs.write(s_binaryHeader.currentDirectory, strlen(s_binaryHeader.currentDirectory) + 1);
	ofs.write(s_binaryHeader.cmdLine, strlen(s_binaryHeader.cmdLine) + 1);
	ofs.write(s_binaryHeader.osVersion, strlen(s_binaryHeader.osVersion) + 1);
	WriteBinary(ofs, s_binaryHeader.vAddrSize);
	WriteBinary(ofs, s_binaryHeader.pAddrSize);
	WriteBinary(ofs, s_binaryHeader.pageSize);
	ofs.write(reinterpret_cast<char*>(&s_binaryHeader.minAppAddr), s_binaryHeader.vAddrSize);
	ofs.write(reinterpret_cast<char*>(&s_binaryHeader.maxAppAddr), s_binaryHeader.vAddrSize);
	WriteBinary(ofs, s_binaryHeader.totalDram);
	WriteBinary(ofs, s_binaryHeader.flags);
	WriteBinary(ofs, s_binaryHeader.reserved);
	WriteBinary(ofs, s_binaryHeader.dirSize);
}

BINARY_SECION_CPUID* GLOBAL_STATE::GetCpuidInfo(unsigned int& sizeInElements, unsigned int& sizeInBytes)
{
	PORTABLE_PROCESSOR_PACKAGE originalAffinifty;
	GetMyThreadAffinity(originalAffinifty);

	unsigned int byteCount = 0;

	/*
	
	When using the new operator to allocate an array of one element. Pin crashes when calling delete.
	This happens on Pin 2.14-71313. Therefore, use the malloc function.

	*/

	BINARY_SECION_CPUID *cpuidInfo = 
		reinterpret_cast<BINARY_SECION_CPUID*>(malloc(sizeof(BINARY_SECION_CPUID)*g_processors.size()));
	BINARY_SECION_CPUID *cpuidInfoIterator = cpuidInfo;

	for (unsigned int i = 0; i < g_processors.size(); ++i)
	{
		SetMyThreadAffinity(g_processors[i]);
		GetProcessorIdentification(*cpuidInfoIterator);
		byteCount += (unsigned int)cpuidInfoIterator->cpuidInfo.size() * sizeof(CPUID_INFO);
		byteCount += 2 * sizeof(int); // nIds and nExIds.
		++cpuidInfoIterator;
	}

	SetMyThreadAffinity(originalAffinifty);

	sizeInElements = (unsigned int)g_processors.size();
	sizeInBytes = byteCount;
	return cpuidInfo;
}

VOID GLOBAL_STATE::WriteBinaryProfile(std::ofstream& ofs, BINARY_HEADER_TYPE type)
{
	/*

	In the main profile, the order of sections is:
	0- CPUID
	1- Profile
	2- Function Table
	3- Image Table
	4- Process Family

	All of them are always valid.

	In a sub profile, the order of sections is:
	0- Profile
	1- Function Table
	2- Image Table

	All of them are always valid.

	See g_binSecDirFuncIndexMain and its brothers.

	*/

	std::ofstream::pos_type profileOffset = (unsigned long long)ofs.tellp() +
		(s_binaryHeader.sectionDirectory.size() * sizeof(SECTION_DIRECTORY_ENTRY));

	BINARY_SECION_CPUID *cpuidInfo;
	unsigned int cpuidInfoCount;

	// Write the section directory. 
	UINT32 startingIndex = 0;

	if (type == BINARY_HEADER_TYPE_MAIN_PROFILE)
	{
		std::ofstream::pos_type cpuidOffset = profileOffset;

		SECTION_DIRECTORY_ENTRY_SET(
			s_binaryHeader.sectionDirectory[startingIndex], BINARY_SECION_TYPE_CPUID, cpuidOffset);
		WriteBinary(ofs, s_binaryHeader.sectionDirectory[startingIndex]);
		++startingIndex;

		unsigned int cpuidInfoBytes;
		cpuidInfo = GetCpuidInfo(cpuidInfoCount, cpuidInfoBytes);

		profileOffset = cpuidOffset + ((std::ofstream::pos_type)cpuidInfoBytes) +
			/*CPUID section header (number of cpuids)*/ ((std::ofstream::pos_type)sizeof(unsigned int));
	}

	SECTION_DIRECTORY_ENTRY_SET(
		s_binaryHeader.sectionDirectory[startingIndex], BINARY_SECION_TYPE_PROFILE, profileOffset);
	WriteBinary(ofs, s_binaryHeader.sectionDirectory[startingIndex]);
	++startingIndex;

	if (type == BINARY_HEADER_TYPE_MAIN_PROFILE)
		s_globalFuncSecOffset = ofs.tellp();
	else if (type == BINARY_HEADER_TYPE_SUB_PROFILE)
		s_funcSecOffset = ofs.tellp();

	// BINARY_SECION_TYPE_IMAGE_TABLE
	SECTION_DIRECTORY_ENTRY_SET(
		s_binaryHeader.sectionDirectory[startingIndex], BINARY_SECION_TYPE_INVALID, 0L); // To be patched later.
	WriteBinary(ofs, s_binaryHeader.sectionDirectory[startingIndex]);
	++startingIndex;

	if (type == BINARY_HEADER_TYPE_MAIN_PROFILE)
		s_globalImgSecOffset = ofs.tellp();
	else if (type == BINARY_HEADER_TYPE_SUB_PROFILE)
		s_imgSecOffset = ofs.tellp();

	// BINARY_SECION_TYPE_FUNC_TABLE
	SECTION_DIRECTORY_ENTRY_SET(
		s_binaryHeader.sectionDirectory[startingIndex], BINARY_SECION_TYPE_INVALID, 0L); // To be patched later.
	WriteBinary(ofs, s_binaryHeader.sectionDirectory[startingIndex]);
	++startingIndex;
	
	if (type == BINARY_HEADER_TYPE_MAIN_PROFILE)
	{
		s_globalProcFamilyOffset = ofs.tellp();

		// BINARY_SECION_TYPE_PROCESS_FAMILY
		SECTION_DIRECTORY_ENTRY_SET(
			s_binaryHeader.sectionDirectory[startingIndex], BINARY_SECION_TYPE_INVALID, 0L); // To be patched later.
		WriteBinary(ofs, s_binaryHeader.sectionDirectory[startingIndex]);
		++startingIndex;
	}

	UINT64 zero = 0;
	for (UINT32 i = startingIndex; i < s_binaryHeader.dirSize; ++i)
	{
		SECTION_DIRECTORY_ENTRY_SET(
			s_binaryHeader.sectionDirectory[i], BINARY_SECION_TYPE_INVALID, 0L); // Not used.

		WriteBinary(ofs, zero);
	}

	if (type == BINARY_HEADER_TYPE_MAIN_PROFILE)
	{
		// Write CPUID section.

		WriteBinary(ofs, cpuidInfoCount);
		BINARY_SECION_CPUID *cpuidInfoIterator = cpuidInfo;

		while (cpuidInfoCount > 0)
		{
			WriteBinary(ofs, cpuidInfoIterator->nIds);
			WriteBinary(ofs, cpuidInfoIterator->nExIds);

			for (UINT32 i = 0; i < cpuidInfoIterator->cpuidInfo.size(); ++i)
			{
				WriteBinary(ofs, cpuidInfoIterator->cpuidInfo[i].EAX);
				WriteBinary(ofs, cpuidInfoIterator->cpuidInfo[i].ECX);
				WriteBinary(ofs, cpuidInfoIterator->cpuidInfo[i].cpuInfo[0]);
				WriteBinary(ofs, cpuidInfoIterator->cpuidInfo[i].cpuInfo[1]);
				WriteBinary(ofs, cpuidInfoIterator->cpuidInfo[i].cpuInfo[2]);
				WriteBinary(ofs, cpuidInfoIterator->cpuidInfo[i].cpuInfo[3]);
			}

			++cpuidInfoIterator;
			--cpuidInfoCount;
		}

		free(cpuidInfo);
	}
}

VOID GLOBAL_STATE::WriteBinaryLlw(std::ofstream& ofs)
{
	std::ofstream::pos_type llwOffset = (unsigned long long)ofs.tellp() +
		(s_binaryHeader.sectionDirectory.size() * sizeof(SECTION_DIRECTORY_ENTRY));

	UINT32 startingIndex = 0;

	SECTION_DIRECTORY_ENTRY_SET(
		s_binaryHeader.sectionDirectory[startingIndex], BINARY_SECION_TYPE_LLW, llwOffset);
	WriteBinary(ofs, s_binaryHeader.sectionDirectory[startingIndex]);
	++startingIndex;

	UINT64 zero = 0;
	for (UINT32 i = startingIndex; i < s_binaryHeader.dirSize; ++i)
	{
		SECTION_DIRECTORY_ENTRY_SET(
			s_binaryHeader.sectionDirectory[i], BINARY_SECION_TYPE_INVALID, 0L); // Not used.

		WriteBinary(ofs, zero);
	}
}

VOID GLOBAL_STATE::WriteBinaryInterceptors(std::ofstream& ofs)
{
	std::ofstream::pos_type typesOffset = (unsigned long long)ofs.tellp() +
		(s_binaryHeader.sectionDirectory.size() * sizeof(SECTION_DIRECTORY_ENTRY));

	UINT32 startingIndex = 0;

	SECTION_DIRECTORY_ENTRY_SET(
		s_binaryHeader.sectionDirectory[startingIndex], BINARY_SECION_TYPE_TYPES, typesOffset);
	WriteBinary(ofs, s_binaryHeader.sectionDirectory[startingIndex]);
	++startingIndex;

	std::ofstream::pos_type interOffsetOffset = ofs.tellp();

	SECTION_DIRECTORY_ENTRY_SET(
		s_binaryHeader.sectionDirectory[startingIndex], BINARY_SECION_TYPE_INTERCEPTORS, 0L); // To be patched later.
	WriteBinary(ofs, s_binaryHeader.sectionDirectory[startingIndex]);
	++startingIndex;

	UINT64 zero = 0;
	for (UINT32 i = startingIndex; i < s_binaryHeader.dirSize; ++i)
	{
		SECTION_DIRECTORY_ENTRY_SET(
			s_binaryHeader.sectionDirectory[i], BINARY_SECION_TYPE_INVALID, 0L); // Not used.

		WriteBinary(ofs, zero);
	}

	INTERCEPTOR_INFO::EmitBinaryProfileTypesSection(GLOBAL_STATE::interceptorFileStreamBinary);

	std::ofstream::pos_type interOffset = (unsigned long long)ofs.tellp();
	ofs.seekp(interOffsetOffset);
	SECTION_DIRECTORY_ENTRY sde;
	SECTION_DIRECTORY_ENTRY_SET(sde, BINARY_SECION_TYPE_INTERCEPTORS, interOffset);
	WriteBinary(ofs, sde);

	ofs.seekp(interOffset);
}

VOID GLOBAL_STATE::PatchFuncImgSecOffsets(std::ofstream& ofs, BOOL isGlobal, UINT64 funcOffset, UINT64 imgOffset)
{
	// Save the current value of the put pointer.

	std::ofstream::pos_type oldPos = ofs.tellp();

	SECTION_DIRECTORY_ENTRY sde;
	SECTION_DIRECTORY_ENTRY_SET(sde, BINARY_SECION_TYPE_FUNC_TABLE, funcOffset);

	// Patch func sec offset.

	if (isGlobal)
		ofs.seekp(s_globalFuncSecOffset);
	else
		ofs.seekp(s_funcSecOffset);

	WriteBinary(ofs, sde);

	// Patch img sec offset.

	SECTION_DIRECTORY_ENTRY_SET(sde, BINARY_SECION_TYPE_IMAGE_TABLE, imgOffset);

	if (isGlobal)
		ofs.seekp(s_globalImgSecOffset);
	else
		ofs.seekp(s_imgSecOffset);

	WriteBinary(ofs, sde);

	// Restore the old value of the put pointer.

	ofs.seekp(oldPos);
}

BOOL GLOBAL_STATE::InitOutputFilesBin()
{
	std::string fullPathStr;

	PathCombine(fullPathStr, s_myDumpDirectory, "g.ap");
	gfstream.open(fullPathStr.c_str(), fomodebin);

	PathCombine(fullPathStr, s_myDumpDirectory, "intercept.ap");
	interceptorFileStreamBinary.open(fullPathStr.c_str(), fomodebin);

	PathCombine(fullPathStr, s_myDumpDirectory, "walker.ap");
	walkerFileStream.open(fullPathStr.c_str(), fomodebin);

	if (!gfstream.good() || !interceptorFileStreamBinary.good() || !walkerFileStream.good())
	{
		AlleriaSetLastError(ALLERIA_ERROR_CANNOT_CREATE_PROFILE);
		return false;
	}

	WriteBinaryHeader(gfstream, BINARY_HEADER_TYPE_MAIN_PROFILE);
	WriteBinaryProfile(gfstream, BINARY_HEADER_TYPE_MAIN_PROFILE);

	WriteBinaryHeader(interceptorFileStreamBinary, BINARY_HEADER_TYPE_SUB_PROFILE);
	WriteBinaryInterceptors(interceptorFileStreamBinary);

	WriteBinaryHeader(walkerFileStream, BINARY_HEADER_TYPE_SUB_PROFILE);
	WriteBinaryLlw(walkerFileStream);

	//for (UINT32 i = 0; i < KnobNumProcessingThreads; ++i)
	//{
	//	std::string temp;
	//	IntegerToString(i + 1, temp);
	//	PathCombine(fullPathStr, s_myDumpDirectory, temp + ".ap");
	//	std::ofstream *stream = new std::ofstream(fullPathStr.c_str(), fomode);
	//	
	//	if (!(*stream).good())
	//	{
	//		AlleriaSetLastError(ALLERIA_ERROR_CANNOT_CREATE_PROFILE);
	//		return false; // Process terminates.
	//	}
	//	WriteBinaryHeader(*stream, BINARY_HEADER_TYPE_SUB_PROFILE);
	//	WriteBinaryProfile(*stream, BINARY_HEADER_TYPE_SUB_PROFILE);
	//	localStreams.push_back(stream);
	//}

	return true;
}

VOID GLOBAL_STATE::Init(INT32 argc, CHAR *argv[])
{
	// NOTE
	// Compute process number first before validating knobs is not necessary, but important.
	// It reduces the probability for the parent process, if exists, to wait for the child.
	ComputeMyProcessNumber(GetProcessIdPortable(GetCurrentProcessPortable()), GetParentProcessId());
	ValidateKnobs(); // Must be called before InitBinaryHeader to properly initialize the knobs.
	InitBinaryHeader();
	g_mainExe = s_binaryHeader.mainExePath; // Initialize g_mainExe. Used for error processing.
	InitProcessDumpDirectory(); // Initialize g_outputDir. Used for error processing.

	// Initialize g_processNumber and g_isGenesis both used for error processing.
	g_processNumber = myPpndbEntry->processNumber;
	g_isGenesis = myPpndbEntry->isGenesis;

	// ValidateKnobs();
	if (AlleriaGetLastError() != ALLERIA_ERROR_SUCCESS)
		return;

	if (!GetProcessorPackages(g_processors))
	{
		AlleriaSetLastError(ALLERIA_ERROR_INIT_FAILED);
		return;
	}

	GetAddressSizes(g_virtualAddressSize, g_physicalAddressSize);
	g_physicalAddressSize = (g_v2pTransMode == V2P_TRANS_MODE_OFF ? 0 : g_physicalAddressSize);
	PIN_InitLock(&gAppThreadLock);
	PIN_InitLock(&gProcThreadLock);
	totalAppThreadWaitingTime = 0;
	totalAppThreadCount = 0;
	processingThreadRunning = FALSE;
	s_childWaitTimeOut = KnobChildWaitTimeOut * 1000; // To milliseconds.

	BUFFER_LIST_MANAGER::SetBufferSize(KnobNumKBBuffer * 1024);
	for (UINT32 i = 0; i < (KnobTunerEnabled ? Tuner::InitialThreadCount : KnobNumberOfBuffers); ++i)
	{
		freeBuffersListManager.AddNewFreeBuffer(PIN_ThreadId());
	}

	totalAppThreadRunningWaitingTime = 0;
	totalProcessingThreadRunningWaitingTime = 0;
	Tuner::Init(KnobTunerEnabled);
	PIN_SemaphoreInit(&tunerSemaphore);
	PIN_SemaphoreClear(&tunerSemaphore);
	tuidTunerControl = 0;
	tuidTunerControlResume = TRUE;
}

VOID GLOBAL_STATE::CreateOutputFiles()
{
	std::string mainExePath = g_mainExe;
	std::string mainExeName;
	PathToName(mainExePath, mainExeName); 
	std::string temp;
	IntegerToString(myPpndbEntry->processNumber, temp);
	std::string folderName = temp + " - " + mainExeName;
	PathCombine(s_myDumpDirectory, g_outputDir, folderName);
	if (!DirectoryExists(s_myDumpDirectory.c_str()))
	{
		if (!PortableCreateDirectory(s_myDumpDirectory.c_str()))
			AlleriaSetLastError(ALLERIA_ERROR_INIT_FAILED);
	}
	if (g_outputFormat == OUTPUT_FORMAT_MODE_TEXTUAL)
		InitOutputFilesTxt();
	else if (g_outputFormat == OUTPUT_FORMAT_MODE_BINARY)
		InitOutputFilesBin();
	IMAGE_COLLECTION::Init();
}

VOID GLOBAL_STATE::Dispose()
{
	if (gfstream.is_open())
	{
		gfstream.flush();
		gfstream.close();
	}

	if (interceptorFileStreamText.is_open())
	{
		interceptorFileStreamText.flush();
		interceptorFileStreamText.close();
	}

	if (interceptorFileStreamBinary.is_open())
	{
		interceptorFileStreamBinary.flush();
		interceptorFileStreamBinary.close();
	}

	if (walkerFileStream.is_open())
	{
		walkerFileStream.flush();
		walkerFileStream.close();
	}

	for (UINT32 i = 0; i < localStreams.size(); ++i)
	{
		if ((*localStreams[i]).is_open())
		{
			(*localStreams[i]).flush();
			(*localStreams[i]).close();
		}
	}

#ifdef TARGET_WINDOWS
	delete[] s_binaryHeader.mainExePath;
	delete[] s_binaryHeader.systemDirectory;
	delete[] s_binaryHeader.currentDirectory;
	delete[] s_binaryHeader.osVersion;
#elif TARGET_LINUX
	delete[] s_binaryHeader.mainExePath;
	delete[] s_binaryHeader.cmdLine;
	free(s_binaryHeader.currentDirectory);
#endif /* TARGET_WINDOWS */

	MEMORY_REGISTRY::Destroy();
	IMAGE_COLLECTION::Dispose();

	PIN_SemaphoreFini(&tunerSemaphore);
}

BOOL GLOBAL_STATE::AboutToTerminateInternal()
{
	void *base = GetPpndbBase();
	if (!base)
	{
		AlleriaSetLastError(ALLERIA_ERROR_INIT_FAILED);
		return FALSE;
	}
	PMUTEX mutex = Mutex_CreateNamed(PROCESS_TREE_MUTEX_NAME);
	Mutex_Acquire(mutex);

	Ppndb *ppndbBase = reinterpret_cast<Ppndb*>(base);
	PpndbEntry *ppndbEntry = &ppndbBase->base;

	BOOL sleep = TRUE;
	UINT child = 0;
	for (UINT32 i = 0; i < ppndbBase->count; ++i)
	{
		if (ppndbEntry->pid == childProcesses[child].first &&
			ppndbEntry->startingTime >= childProcesses[child].second &&
			ppndbEntry->endingTime != -1)
			++child;
		if (child >= childProcesses.size())
		{
			sleep = FALSE;
			break;
		}
		++ppndbEntry;
	}

	Mutex_Release(mutex);
	Mutex_Close(mutex);

	return sleep;
}

VOID GLOBAL_STATE::PatchHeaderTimes(std::ofstream& ofs)
{
	// Save the current value of the put pointer.

	std::ofstream::pos_type oldPos = ofs.tellp();

	// Patch starting time.

	ofs.seekp(BINARY_HEADER_offsetof(startingTime));
	WriteBinary(ofs, s_binaryHeader.startingTime);

	// Patch ending time.

	ofs.seekp(BINARY_HEADER_offsetof(endingTime));
	WriteBinary(ofs, s_binaryHeader.endingTime);

	// Restore the old value of the put pointer.

	ofs.seekp(oldPos);
}

VOID GLOBAL_STATE::PatchBinaryHeader()
{
	// Patch the starting time and the ending time.

	s_binaryHeader.endingTime = time(0);

	PatchHeaderTimes(gfstream);
	PatchHeaderTimes(interceptorFileStreamBinary);
	PatchHeaderTimes(walkerFileStream);
	
	for (UINT32 i = 0; i < localStreams.size(); ++i)
	{
		PatchHeaderTimes((*localStreams[i]));
	}
}

VOID GLOBAL_STATE::AboutToTerminate()
{
	if (childProcesses.size() > 0)
	{
		// Before this process terminates, it has to make sure that all its live children (if any)
		// has openned a handle to the PPNDB file-mapping object.
		// This ensures that the OS doesn't reclaim PPNDB file-mapping object which might happen
		// if this process terminates before any of the child processes obtain a handle to the object.

		BOOL sleep = AboutToTerminateInternal();

		if (sleep)
		{
			/*

			It's possible that a child process failed to start or terminated abruptly.
			It's also possible that a child process is yet to access the PPNDB.
			Either way, put this thread to sleep for some time and hope that all child processes
			get the chance to access the PPNDB before it's reclaimed by the OS.
			If this happens, a child process will think it's a genesis.

			*/

			ALLERIA_WriteMessage(ALLERIA_REPORT_WAIT_FOR_CHILD);
			PIN_Sleep(s_childWaitTimeOut);

			sleep = AboutToTerminateInternal();
			if (sleep)
			{
				ALLERIA_WriteMessage(ALLERIA_REPORT_WAIT_FOR_CHILD_FAILURE);
			}
			else
			{
				ALLERIA_WriteMessage(ALLERIA_REPORT_WAIT_FOR_CHILD_SUCCESS);
			}
		}
	}

	GetCurrentTimestamp(myPpndbEntry->endingTime);
}

VOID GLOBAL_STATE::AboutToStart()
{
	s_binaryHeader.startingTime = time(0);
}

UINT32 GLOBAL_STATE::NewChild(UINT32 pid)
{
	TIME_STAMP t;
	GetCurrentTimestamp(t);
	childProcesses.push_back(std::make_pair(pid, t));

	// The timestamp in childProcesses must not be larger than the timestamp in the PPNDB.
	// So this has to occur after setting t.
	UINT32 pn = ComputeChildProcessNumber(pid, myPpndbEntry->pid);

	// This holds a sligtly more accurate timestamp.
	s_binaryProcFamily.procs.push_back({ pn, pid, time(0) });

	return pn;
}

VOID GLOBAL_STATE::PatchProcFamily()
{
	UINT64 offset = gfstream.tellp();
	
	// Write the process family section.
	
	UINT32 size = (UINT32)s_binaryProcFamily.procs.size();
	WriteBinary(gfstream, size);
	for (UINT32 i = 0; i < s_binaryProcFamily.procs.size(); ++i)
	{
		WriteBinary(gfstream, s_binaryProcFamily.procs[i].pn);
		WriteBinary(gfstream, s_binaryProcFamily.procs[i].pId);
		WriteBinary(gfstream, s_binaryProcFamily.procs[i].t);
	}

	// Patch the process family section offset.
	gfstream.seekp(s_globalProcFamilyOffset);

	SECTION_DIRECTORY_ENTRY sde;
	SECTION_DIRECTORY_ENTRY_SET(sde, BINARY_SECION_TYPE_PROCESS_FAMILY, offset);

	WriteBinary(gfstream, sde);

	// No need to restore the put pointer.
}

VOID GLOBAL_STATE::PatchInsCountsInternal(std::ofstream& ofs)
{
	// Save the current value of the put pointer.

	std::ofstream::pos_type oldPos = ofs.tellp();

	// Patch ins count.

	ofs.seekp(BINARY_HEADER_offsetof(instructionCount));
	WriteBinary(ofs, insCount);

	// Patch mem in count.

	ofs.seekp(BINARY_HEADER_offsetof(memoryInstructionCount));
	WriteBinary(ofs, insMemCount);

	// Restore the old value of the put pointer.

	ofs.seekp(oldPos);
}

VOID GLOBAL_STATE::PatchInsCounts()
{
	PatchInsCountsInternal(gfstream);
	for (UINT32 i = 0; i < localStreams.size(); ++i)
	{
		PatchInsCountsInternal((*localStreams[i]));
	}
}

VOID GLOBAL_STATE::InitBinaryHeaderSectionDirectory()
{
	UINT32 count = 0;

	// CPUID section.
	++count;

	// Process family section.
	++count;

	// Interceptors section.
	++count;

	// Profile section, function table section and image table section
	// for each processing thread and one for all app threads.
	// TODO: processingThreadsIds.size() is dynamic when tuner is enabled.
	count += 3 * ((KnobTunerEnabled ? Tuner::MaxThreadCount : KnobNumProcessingThreads) + 1);

	if (KnobLLWEnabled)
		++count;

	// 16 reserved for Alleria and 16 reserved for users.
	count += 32;

	s_binaryHeader.dirSize = count;

	SECTION_DIRECTORY_ENTRY sde;
	sde.data = 0;
	for (UINT32 i = 0; i < count; ++i)
		s_binaryHeader.sectionDirectory.insert(s_binaryHeader.sectionDirectory.begin(), sde);
}