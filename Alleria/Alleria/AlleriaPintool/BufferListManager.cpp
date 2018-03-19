#include "BufferListManager.h"

UINT32 BUFFER_LIST_MANAGER::s_bufferSize;

BUFFER_LIST_MANAGER::BUFFER_LIST_MANAGER()
{
	PIN_InitLock(&m_bufferListLock);
	m_bufferListSem = Semaphore_CreateUnnamed();
	s_bufferSize = 0;
}

VOID BUFFER_LIST_MANAGER::SetBufferSize(UINT32 bufferSize)
{
	ASSERTX(bufferSize > 0 && s_bufferSize == 0);
	s_bufferSize = bufferSize;
}

VOID BUFFER_LIST_MANAGER::AddFullBuffer(
	VOID *bufferStart,
	VOID *bufferEnd,
	APP_THREAD_MANAGER *atm,
	THREADID tid)
{
	BUFFER_STATE buffer;
	buffer.m_bufferStart = bufferStart;
	buffer.m_bufferEnd = bufferEnd;
	buffer.m_appThreadManager = atm;

	PIN_GetLock(&m_bufferListLock, tid + 1);
	m_bufferList.push_back(buffer);
	PIN_ReleaseLock(&m_bufferListLock);
	
	bool releaseSemaphoreSuccess = Semaphore_Increment(m_bufferListSem, 1, NULL);
	ASSERTX(releaseSemaphoreSuccess);
}

// Allocates an array of size m_*bufferSize in bytes.
// The array is not initialized. Returns a pointer to that array.
VOID BUFFER_LIST_MANAGER::AddNewFreeBuffer(
	THREADID tid)
{
	VOID *buff = new UINT8[s_bufferSize];
	ASSERT(buff != NULL, "Alleria out of memmory!");
	memset(buff, 0, s_bufferSize);
	
	BUFFER_STATE buffer;
	buffer.m_bufferStart = buff;
	buffer.m_bufferEnd = NULL;
	buffer.m_appThreadManager = NULL;

	PIN_GetLock(&m_bufferListLock, tid + 1);
	m_bufferList.push_back(buffer);
	PIN_ReleaseLock(&m_bufferListLock);

	bool releaseSemaphoreSuccess = Semaphore_Increment(m_bufferListSem, 1, NULL);
	ASSERTX(releaseSemaphoreSuccess);
}

VOID BUFFER_LIST_MANAGER::AddFreeBuffer(
	VOID *bufferStart,
	THREADID tid)
{
	BUFFER_STATE buffer;
	buffer.m_bufferStart = bufferStart;
	buffer.m_bufferEnd = NULL;
	buffer.m_appThreadManager = NULL;

	PIN_GetLock(&m_bufferListLock, tid + 1);
	m_bufferList.push_back(buffer);
	PIN_ReleaseLock(&m_bufferListLock);

	bool releaseSemaphoreSuccess = Semaphore_Increment(m_bufferListSem, 1, NULL);
	ASSERTX(releaseSemaphoreSuccess);
}

BOOL BUFFER_LIST_MANAGER::AcquireFullBuffer(
	BUFFER_STATE& buffer,
	THREADID tid
	)
{
	// Wait until a buffer is available or the process is terminating.
	Semaphore_Decrement(m_bufferListSem);

	if (m_bufferList.empty())
	{
		// The process is terminating.
		return FALSE;
	}
	else
	{
		// A buffer is available.
		PIN_GetLock(&m_bufferListLock, tid + 1);

		buffer = m_bufferList.front();
		m_bufferList.pop_front();

		PIN_ReleaseLock(&m_bufferListLock);
		return TRUE;
	}
}

BOOL BUFFER_LIST_MANAGER::AcquireFreeBuffer(
	VOID **bufferStart,
	THREADID tid
	)
{
	// Wait until a free buffer is available.
	// Free buffers can be recycled by both processing threads and app threads.
	Semaphore_Decrement(m_bufferListSem);

	PIN_GetLock(&m_bufferListLock, tid + 1);

	// There must be always a free buffer.
	ASSERTX(m_bufferList.size() > 0);

	const BUFFER_STATE &buffer = m_bufferList.front();
	*bufferStart = buffer.m_bufferStart;

	m_bufferList.pop_front();

	PIN_ReleaseLock(&m_bufferListLock);
	return TRUE;
}

size_t BUFFER_LIST_MANAGER::GetCount()
{
	return m_bufferList.size();
}

VOID BUFFER_LIST_MANAGER::SignalTermination()
{
	bool releaseSemaphoreSuccess = Semaphore_Increment(m_bufferListSem, 1, NULL);
	ASSERTX(releaseSemaphoreSuccess);
}