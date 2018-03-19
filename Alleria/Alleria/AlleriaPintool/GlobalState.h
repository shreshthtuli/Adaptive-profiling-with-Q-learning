#ifndef GLOBALSTATE_H
#define GLOBALSTATE_H

#include "PortableApi.h"

#include <fstream>
#include <vector>
#include <map>
#include <iostream>

#include "pin.H"
#include "BufferListManager.h"
#include "ImageCollection.h"
#include "MemoryRegistry.h"
#include "InterceptorInfo.h"
#include "Tuner.h"

class GLOBAL_STATE
{
private:
	struct PpndbEntry
	{
		/*
		
		IMPORTANT

		This structure must not contain an pointer fields because it causes problems
		when the parent and child processes are mixed (meaning that one of them is 32-bit and the other is 64-bit). 
		
		*/


		// The OS process id.
		UINT32 pid;

		// A number representing the order of this process with respect to the other processes
		// in the same tree.
		UINT32 processNumber;

		// If isGenesis is TRUE, this holds the process number that was used by
		// the last spawned process in the process tree. 
		// If isGenesis is FALSE, this holds the index of the PpndbEntry whose
		// isGenesis field is TRUE and represents the genesis process of this process
		// and therefore contains the process number of the process family.
		UINT32 reference;
		
		// TRUE if the process is a genesis process, FALSE if it's spawned. 
		BOOL isGenesis;

		// Holds the timestamp acquired at process initialization (while main is executing).
		TIME_STAMP startingTime;

		// Value of -1 means that the entry has been created by a parent process.
		// Value of 0 means that the entry has been initialized (and created if it's a genesis) by the process itself.
		// Otherwise, it holds the actual ending timestamp.
		TIME_STAMP endingTime;
	};

	struct Ppndb
	{
		// The total number of valid PpndbEntry entries.
		// This means that the first count entries starting at base are occupied.
		UINT32 count;
		PpndbEntry base; // This field's address is the address of the array of PpndbEntry objects.
	};

	//LOCALFUN VOID InitSystemStructures();
	LOCALFUN VOID* GetPpndbBase();
	LOCALFUN UINT32 ComputeChildProcessNumber(UINT32 pid, UINT32 ppid);
	LOCALFUN UINT32 ComputeMyProcessNumber(UINT32 pid, UINT32 ppid);
	LOCALFUN VOID InitProcessDumpDirectory();
	LOCALFUN BOOL AboutToTerminateInternal();
	LOCALFUN BOOL ValidateKnobs();
	LOCALFUN VOID InitBinaryHeader();
	LOCALFUN VOID InitBinaryHeaderSectionDirectory();
	LOCALFUN BOOL InitOutputFilesTxt();
	LOCALFUN BOOL InitOutputFilesBin();
	LOCALFUN VOID WriteBinaryLlw(std::ofstream& ofs);
	LOCALFUN VOID WriteBinaryInterceptors(std::ofstream& ofs);
	LOCALFUN VOID PatchHeaderTimes(std::ofstream& ofs);
	LOCALFUN VOID PatchInsCountsInternal(std::ofstream& ofs);
	LOCALFUN BINARY_SECION_CPUID* GetCpuidInfo(unsigned int& sizeInElements, unsigned int& sizeInBytes);

	LOCALVAR BINARY_HEADER s_binaryHeader;
	LOCALVAR BINARY_PROCESS_FAMILY s_binaryProcFamily;
	LOCALFUN std::streampos s_globalFuncSecOffset;
	LOCALFUN std::streampos s_globalImgSecOffset;
	LOCALFUN std::streampos s_globalProcFamilyOffset;
	LOCALFUN std::streampos s_funcSecOffset;
	LOCALFUN std::streampos s_imgSecOffset;

public:
	LOCALFUN VOID Init(INT32 argc, CHAR *argv[]);
	LOCALFUN VOID CreateOutputFiles();
	LOCALFUN VOID Dispose();
	LOCALFUN VOID AboutToStart();
	LOCALFUN VOID AboutToTerminate();
	LOCALFUN UINT32 NewChild(UINT32 pid);
	LOCALFUN VOID PatchBinaryHeader();
	LOCALFUN VOID PatchFuncImgSecOffsets(std::ofstream& ofs, BOOL isGlobal, UINT64 funcOffset, UINT64 imgOffset);
	LOCALFUN VOID PatchProcFamily();
	LOCALFUN VOID PatchInsCounts();

	LOCALFUN VOID WriteBinaryHeader(std::ofstream& ofs, BINARY_HEADER_TYPE type);
	LOCALFUN VOID WriteBinaryProfile(std::ofstream& ofs, BINARY_HEADER_TYPE type);

	LOCALFUN xed_syntax_enum_t GetAsmSyntaxStyle();

	LOCALVAR UINT32 s_childWaitTimeOut; // In Milliseconds.
	LOCALVAR xed_syntax_enum_t s_asmSyntax;
	LOCALVAR std::string s_myDumpDirectory;

	// Is any processing thread running.
	LOCALVAR BOOL processingThreadRunning;

	// The global file stream used when a buffer got full but there is no prcessing thread
	// that can process it. 
	LOCALVAR std::ofstream gfstream;

	// Used by app threads when emitting binary profiles.
	// Holds pointer to a map object that keeps track of the indices of routines in the
	// functions section of the binary profile.
	LOCALVAR std::map<UINT32, UINT32> gRoutineMap;

	// Used for recording interceptors.
	LOCALVAR std::wofstream interceptorFileStreamText;
	LOCALVAR std::ofstream interceptorFileStreamBinary;

	LOCALVAR std::ofstream walkerFileStream;

	// The Pin lock used by app threads to access global profiling state
	// such as gfstream and gRoutineMap.
	LOCALVAR PIN_LOCK gAppThreadLock;

	// File stream objects used by threads locally.
	LOCALVAR std::vector<std::ofstream*> localStreams;

	// Used by processing threads to access localStreams safely;
	LOCALVAR PIN_LOCK gProcThreadLock;
	
	// Holds the unique IDs of all processing threads.
	LOCALVAR std::vector<PIN_THREAD_UID> processingThreadsIds;

	// A list of full buffers populated by app threads and typically processed by processing threads.
	LOCALVAR BUFFER_LIST_MANAGER fullBuffersListManager;

	// A list of empty buffers filled by app threads.
	// When a buffer becomes full, it is added to the full buffer list.
	LOCALVAR BUFFER_LIST_MANAGER freeBuffersListManager;

	// The number of instructions that executed on all app threads.
	LOCALVAR UINT64 insCount;

	// The number of memory access instructions that executed on all app threads.
	LOCALVAR UINT64 insMemCount;

	LOCALVAR ADDRINT currentProcessHandle;
	LOCALVAR PpndbEntry *myPpndbEntry;
	LOCALVAR std::vector<std::pair<UINT32, TIME_STAMP> > childProcesses;

	LOCALVAR TIME_STAMP totalAppThreadWaitingTime;
	LOCALVAR TIME_STAMP totalAppThreadCount;

	LOCALVAR PIN_THREAD_UID timerthreadUID;
	LOCALVAR PIN_THREAD_UID walkerthreadUID;
	volatile LOCALVAR BOOL isAppExisting;

	// These are used by the adaptive profiling technique.
	// Must be accessed atomically.
	LOCALVAR TIME_STAMP totalAppThreadRunningWaitingTime; // Modified by the tuner and app threads. Read by the tuner.
	LOCALVAR TIME_STAMP totalProcessingThreadRunningWaitingTime; // Modified by p threads. Read by the tuner.
	
	// Used by the tuner to suspend and resume processing threads.
	// Must be accessed atomically.
	LOCALVAR PIN_THREAD_UID tuidTunerControl;
	LOCALVAR BOOL tuidTunerControlResume;

	LOCALVAR PIN_SEMAPHORE tunerSemaphore;

	LOCALVAR const std::ofstream::openmode fomodetxt =
		(std::ofstream::openmode)(std::ofstream::out | std::ofstream::trunc);
	LOCALVAR const std::ofstream::openmode fomodebin =
		(std::ofstream::openmode)(std::ofstream::out | std::ofstream::trunc | std::ofstream::binary);
};

/*

Alleria Knobs Declarations

*/

extern KNOB<UINT32> KnobNumKBBuffer;

extern KNOB<UINT32> KnobNumberOfBuffers;

extern KNOB<UINT32> KnobNumProcessingThreads;

extern KNOB<UINT32> KnobChildWaitTimeOut;

extern KNOB<string> KnobAsmStyle;

extern KNOB<string> KnobOutputFormat;

extern KNOB<BOOL> KnobLLWEnabled;

extern KNOB<BOOL> KnobTimerEnabled;

extern KNOB<BOOL> KnobOuputInstructionEnabled;

extern KNOB<UINT32> KnobTimerFreq;

extern KNOB<string> KnobDumpDir;

extern KNOB<string> KnobV2PTransMode;

extern KNOB<string> KnobOutputDebugStringMode;

extern KNOB<BOOL> KnobTrackProcessorsEnabled;

extern KNOB<string> KnobConfigDir;

extern KNOB<BOOL> KnobTunerEnabled;
/*

End Alleria Knobs Declarations

*/

enum V2P_TRANS_MODE
{
	V2P_TRANS_MODE_INVALID,
	V2P_TRANS_MODE_OFF,
	V2P_TRANS_MODE_FAST,
	V2P_TRANS_MODE_ACCURATE
};

enum OUTPUT_FORMAT_MODE
{
	OUTPUT_FORMAT_MODE_INVALID,
	OUTPUT_FORMAT_MODE_TEXTUAL,
	OUTPUT_FORMAT_MODE_BINARY
};

enum ODS_MODE
{
	ODS_MODE_INVALID,
	ODS_MODE_AS_REQUIRED,
	ODS_MODE_SUPPRESS
};

extern V2P_TRANS_MODE g_v2pTransMode;
extern OUTPUT_FORMAT_MODE g_outputFormat;

extern UINT8 g_virtualAddressSize; // In bytes.
extern UINT8 g_physicalAddressSize; // In bytes.
extern UINT32 g_processNumber;
extern std::string g_outputDir;

extern const UINT8 g_binSecDirFuncIndexMain;
extern const UINT8 g_binSecDirImgIndexMain;
extern const UINT8 g_binSecDirFuncIndexSub;
extern const UINT8 g_binSecDirImgIndexSub;

#ifdef TARGET_WINDOWS
extern ODS_MODE g_odsMode; // OutputDebugString mode.
#endif /* TARGET_WINDOWS */

#define INTERCEPTORS_BUFFER_SIZE 1024 // 1 KB

void PathCombine(std::string& output, const std::string& path1, const std::string& path2);

#endif /* GLOBALSTATE_H */