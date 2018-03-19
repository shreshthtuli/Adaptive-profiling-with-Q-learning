#ifndef INTERCEPTORMEMORYMANAGER_H
#define INTERCEPTORMEMORYMANAGER_H

#include "pin.H"
#include "InterceptorInfo.h"
#include "MemoryRegistry.h"

#include <fstream>
#include <vector>

// A heap data structure to be used by a single thread.
class InterceptorMemoryManager
{
public:
	InterceptorMemoryManager();
	~InterceptorMemoryManager();
	ADDRINT GetCurrentChunk();
	ADDRINT GetCurrentChunkAndIncrementBy(size_t increment);
	VOID DumpToFile(std::wostream& ofstream);
	VOID DumpToFileBin(std::ostream& ofstream);

	VOID Push(VOID *addr);
	VOID* FindRoutineAndPop(INT32 rtnId);
	VOID Pop();

private:
	ADDRINT AllocateChunk();
	VOID InternalDumpToFile(std::wostream& ofstream, VOID *data, UINT32 size);
	VOID InternalDumpToFileBin(std::ostream& os, VOID *data, UINT32 size);

	ADDRINT m_chunkBase;
	ADDRINT m_chunkCurrent;
	LOCALCONST size_t MaxSlotSize = sizeof(INTERCEPTOR_INFO);

	// Used by analysis routines of system calls.
	std::vector<VOID*> m_addresses;

	// Used by the heap object to keep track of allocated chunks.
	std::vector<VOID*> m_memChunks;
};

#endif /* INTERCEPTORMEMORYMANAGER_H */