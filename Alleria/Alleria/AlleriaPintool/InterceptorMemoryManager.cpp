#include "InterceptorMemoryManager.h"

LOCALCONST size_t OneKB = 1024;

// The number of addresses that can be stored in a memory region of this size must be no larger than max of UINT32. See InternalDumpToFileBin.
LOCALCONST size_t AllocGranularity = OneKB * 64;

InterceptorMemoryManager::InterceptorMemoryManager()
{
	m_chunkBase = AllocateChunk();
	m_chunkCurrent = m_chunkBase;
}

InterceptorMemoryManager::~InterceptorMemoryManager()
{
	for (size_t i = 0; i < m_memChunks.size(); ++i)
	{
		free(m_memChunks[i]);
	}
}

VOID InterceptorMemoryManager::Push(VOID *addr)
{
	m_addresses.push_back(addr);
}

VOID* InterceptorMemoryManager::FindRoutineAndPop(INT32 rtnId)
{
	if (m_addresses.empty())
		return NULL;

	size_t i = m_addresses.size() - 1;
	for (;; --i)
	{
		if (reinterpret_cast<INTERCEPTOR_INFO*>(m_addresses[i])->rtnId == rtnId)
		{
			break;
		}

		if (i == 0)
			return NULL;
	}

	// Routine found at i. Pop all address following i.
	VOID *result = m_addresses[i];
	m_addresses.erase(m_addresses.begin() + i, m_addresses.end());
	return result;
}

VOID InterceptorMemoryManager::Pop()
{
	m_addresses.pop_back();
}

ADDRINT InterceptorMemoryManager::AllocateChunk()
{
	void *address = malloc(AllocGranularity);
	if (address == NULL)
	{
		ALLERIA_WriteError(ALLERIA_ERROR_OUT_OF_MEMORY);
	}
	MEMORY_REGISTRY::RegisterGeneral((ADDRINT)address, REGION_USER::Alleria);
	m_memChunks.push_back(address);
	return (ADDRINT)address;
}

ADDRINT InterceptorMemoryManager::GetCurrentChunk()
{
	return m_chunkCurrent;
}

ADDRINT InterceptorMemoryManager::GetCurrentChunkAndIncrementBy(size_t increment)
{
	ASSERTX(increment <= MaxSlotSize);
	ADDRINT result = m_chunkCurrent;
	m_chunkCurrent += increment;
	ADDRINT last = m_chunkBase + AllocGranularity;
	if (m_chunkCurrent + MaxSlotSize > last)
	{
		m_chunkBase = AllocateChunk();
		if (m_chunkBase != last)
		{
			// The heap manager might allocate a chunk from a new region (using VirtualAlloc).
			// So we cannot just continue using m_chunkCurrent.
			// The value at m_chunkCurrent must be zero, marking the end of the current chunk.
			ASSERTX(m_chunkCurrent == last || *reinterpret_cast<UINT8*>(m_chunkCurrent) == 0);
			m_chunkCurrent = m_chunkBase;
		}
	}
	return result;
}

VOID InterceptorMemoryManager::DumpToFile(std::wostream& os)
{
	for (UINT32 i = 0; i < m_memChunks.size(); ++i)
	{
		void *data = m_memChunks[i];
		InternalDumpToFile(os, data, AllocGranularity);
		free(data);
	}
	m_memChunks.clear();
}

VOID InterceptorMemoryManager::InternalDumpToFile(std::wostream& os, VOID *data, UINT32 size)
{
	//sstream << std::hex
	INTERCEPTOR_INFO **current = reinterpret_cast<INTERCEPTOR_INFO**>(data);
	INTERCEPTOR_INFO **end = reinterpret_cast<INTERCEPTOR_INFO**>((ADDRINT)data + size);

	INTERCEPTOR_INFO *interceptorInfo = *current;
	while (current < end && interceptorInfo)
	{
		interceptorInfo->WriteString(os);
		os << "\n";
		delete interceptorInfo;
		++current;
		interceptorInfo = *current;
	}
	os.flush();
}

VOID InterceptorMemoryManager::DumpToFileBin(std::ostream& os)
{
	for (UINT32 i = 0; i < m_memChunks.size(); ++i)
	{
		void *data = m_memChunks[i];
		InternalDumpToFileBin(os, data, AllocGranularity);
		free(data);
	}
	m_memChunks.clear();
}

VOID InterceptorMemoryManager::InternalDumpToFileBin(std::ostream& os, VOID *data, UINT32 size)
{
	INTERCEPTOR_INFO **current = reinterpret_cast<INTERCEPTOR_INFO**>(data);
	INTERCEPTOR_INFO **end = reinterpret_cast<INTERCEPTOR_INFO**>((ADDRINT)data + size);

	INTERCEPTOR_INFO *interceptorInfo = *current;
	while (current < end && interceptorInfo)
	{
		interceptorInfo->WriteBin(os);
		delete interceptorInfo;
		++current;
		interceptorInfo = *current;
	}
	os.flush();
}