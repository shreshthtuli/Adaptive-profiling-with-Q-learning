#ifndef LOCALSTATE_H
#define LOCALSTATE

#include "pin.H"

// Defines variables that reference thread-local storage.
struct LOCAL_STATE
{
	// Identifies the location of the app thread manager in the specified app thread.
	TLS_KEY appThreadManagerKey;

	// Identifies an output file stream for a processing thread.
	// Every processing thread has its own stream to write to without
	// requiring any thread synchronization. So the number of output files
	// is equal to the number of processing threads plus one for the global
	// output file.
	TLS_KEY ofstreamKey;

	// For each app and processing thread, store the starting and ending time of the thread.
	TLS_KEY startingTimeHighKey;
	TLS_KEY startingTimeLowKey;

	// Holds a pointer to a PROCESSING_THREAD_STATS object.
	TLS_KEY processingThreadStatsKey;

	// Used by processing threads when emitting binary profiles.
	// Holds pointer to a map object that keeps track of the indices of routines in the
	// functions section of the binary profile.
	TLS_KEY routineMapKey;

	// Stores the address of the system call entry in the buffer currently being used by the app thread
	// to profile a system call. See AnalyzeSysCallExit in Main.cpp.
	TLS_KEY sysCallBufferEntryAddress;

	// Used to monitor memory allocation and deallocation system calls to update LLW.
	TLS_KEY memAllocInfo;

	// Stores a pointer to an object of type InterceptorMemoryManager.
	REG interceptorMemManReg;

	// Holds a pointer to the next empty slot in the buffer.
	REG bufferEndReg;

	// Holds a pointer to the past last slot in the buffer.
	REG bufferLastReg;
};

struct PROCESSING_THREAD_STATS
{
	TIME_STAMP processingTimeSpan; // Microseconds.
	TIME_STAMP idleTimeSpan; // Microseconds.
	UINT64 bufferPairCount; // Number of buffer pairs processed by this threads.
	UINT64 unusedBytes;
};

#endif /* LOCALSTATE_H */