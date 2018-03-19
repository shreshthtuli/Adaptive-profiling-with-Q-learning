#include "AppThreadManager.h"
#include "GlobalState.h"
#include "MemoryRegistry.h"
#include <atomic/ops.hpp>

VOID ProcessBuffer(
	const BUFFER_STATE& bufferState,
	std::ofstream &fstream,
	UINT64 *readsCount,
	UINT64 *writesCount,
	UINT64 *insCount,
	UINT64 *memInsCount, 
	UINT32& unusedBytes);

VOID ProcessBufferBinary(
	const BUFFER_STATE& bufferState,
	BOOL isAppThread,
	PIN_THREAD_UID tuid,
	UINT64 *readsCount,
	UINT64 *writesCount,
	UINT64 *insCount,
	UINT64 *memInsCount,
	UINT32& unusedBytes);

APP_THREAD_MANAGER::APP_THREAD_MANAGER(THREADID tid, UINT32 bufferSize)
	: m_bufferSize(bufferSize)
{
	PIN_InitLock(&m_pendingFullBufferCounterLock);
	m_tid = tid;
	m_pendingFullBufferCounter = 0;
	m_insCount = 0;
	m_memInsCount = 0;
	m_readsCount = 0;
	m_writesCount = 0;
	m_waitingTime = 0;
	m_bufferCount = 0;
	m_osThreadId = PIN_GetTid();
	m_tuid = PIN_ThreadUid();
	GetFreeBuffer();
}

APP_THREAD_MANAGER::~APP_THREAD_MANAGER()
{
	
}

VOID* APP_THREAD_MANAGER::GetCurrentBuffer()
{
	return m_currentBuffer;
}

VOID* APP_THREAD_MANAGER::GetCurrentBufferLast()
{
	return reinterpret_cast<UINT8*>(m_currentBuffer) + m_bufferSize;
}

VOID APP_THREAD_MANAGER::SetCurrentBuffer(VOID *buffer)
{
	ASSERTX(buffer != NULL);
	m_currentBuffer = buffer;
}

VOID APP_THREAD_MANAGER::IncrementPendingFullBufferCounter()
{
	PIN_GetLock(&m_pendingFullBufferCounterLock, m_tid + 1);
	++m_pendingFullBufferCounter;
	PIN_ReleaseLock(&m_pendingFullBufferCounterLock);
}

VOID APP_THREAD_MANAGER::DecrementPendingFullBufferCounter()
{
	PIN_GetLock(&m_pendingFullBufferCounterLock, m_tid + 1);
	--m_pendingFullBufferCounter;
	PIN_ReleaseLock(&m_pendingFullBufferCounterLock);
}

BOOL APP_THREAD_MANAGER::AreAllBuffersProcessed()
{
	return m_pendingFullBufferCounter == 0;
}

TIME_STAMP APP_THREAD_MANAGER::GetWaitingTime()
{
	return m_waitingTime;
}

VOID APP_THREAD_MANAGER::GetFreeBuffer()
{
	TIME_STAMP from;
	GetCurrentTimestamp(from);
	GLOBAL_STATE::freeBuffersListManager.AcquireFreeBuffer(
		&m_currentBuffer,
		m_tid);
	TIME_STAMP to;
	GetCurrentTimestamp(to);
	TIME_STAMP ts;
	GetTimeSpan(from, to, TIME_UNIT::Microseconds, ts);
	m_waitingTime += ts;

	ATOMIC::OPS::Increment(&GLOBAL_STATE::totalAppThreadRunningWaitingTime, ts);
}

VOID APP_THREAD_MANAGER::EnqueFullAndGetFree(
	VOID **bufferEnd,
	VOID **bufferLast)
{
	BUFFER_STATE bufferState;
	bufferState.m_appThreadManager = this;
	bufferState.m_bufferStart = m_currentBuffer;
	bufferState.m_bufferEnd = *bufferEnd;
	++m_bufferCount;

	if (!GLOBAL_STATE::processingThreadRunning)
	{
		// No processing thread has started running yet.
		// Let the app thread process it.
		// This is important because the app thread might be holding a lock
		// that the processing thread(s) require to start.
		// Synchronization is required for multiple app threads.
		UINT64 readsCount = 0;
		UINT64 writesCount = 0;
		UINT64 insCount = 0;
		UINT64 memInsCount = 0;
		UINT32 unusedBytes;
		PIN_GetLock(&GLOBAL_STATE::gAppThreadLock, m_tid + 1);
		if (g_outputFormat == OUTPUT_FORMAT_MODE_TEXTUAL)
		{
			ProcessBuffer(bufferState,
				GLOBAL_STATE::gfstream,
				&readsCount, &writesCount, &insCount, &memInsCount,
				unusedBytes);
		}
		else if (g_outputFormat == OUTPUT_FORMAT_MODE_BINARY)
		{
			ProcessBufferBinary(bufferState, true,
				m_tuid,
				&readsCount, &writesCount, &insCount, &memInsCount,
				unusedBytes);
		}
		PIN_ReleaseLock(&GLOBAL_STATE::gAppThreadLock);

		// Ignore unusedBytesInsRecord and unusedBytesMemRef in an app thread.

		AddInsCount(insCount);
		AddMemInsCount(memInsCount);
		AddReads(readsCount);
		AddWrites(writesCount);
		WINDOW_CONFIG::ReportAccesses(PIN_ThreadUid(), GetReadsCount(), GetWritesCount());
		WINDOW_CONFIG::ReportInstructions(PIN_ThreadUid(), GetInsLastCount());

		// No need to get a new free buffer. Just reuse the current buffer since
		// it has been processed.
	}
	else
	{
		IncrementPendingFullBufferCounter();
		GLOBAL_STATE::fullBuffersListManager.AddFullBuffer(
			m_currentBuffer,
			*bufferEnd,
			this,
			m_tid);

		GetFreeBuffer();

		*bufferLast = GetCurrentBufferLast();
	}

	// If processingThreadRunning was false, then this would change the end of the buffer
	// to the begining of the buffer. Otherwise, if processingThreadRunning was true,
	// this would change the pointers to point to the new buffer.
	*bufferEnd = m_currentBuffer;
}

OS_THREAD_ID APP_THREAD_MANAGER::GetOSThreadId()
{
	return m_osThreadId;
}

PIN_THREAD_UID APP_THREAD_MANAGER::GetThreadUid()
{
	return m_tuid;
}

// I don't think atomics are required in the following functions.
// I don't remember why I added them.

VOID APP_THREAD_MANAGER::AddInsCount(UINT64 count)
{
	ATOMIC::OPS::Increment(&m_insCount, count);
	ATOMIC::OPS::Store(&m_insLastCount, count);
}

UINT64 APP_THREAD_MANAGER::GetInsCount()
{
	return ATOMIC::OPS::Load(&m_insCount);
}

UINT64 APP_THREAD_MANAGER::GetInsLastCount()
{
	return ATOMIC::OPS::Load(&m_insLastCount);
}

VOID APP_THREAD_MANAGER::AddMemInsCount(UINT64 count)
{
	ATOMIC::OPS::Increment(&m_memInsCount, count);
}

UINT64 APP_THREAD_MANAGER::GetMemInsCount()
{
	return ATOMIC::OPS::Load(&m_memInsCount);
}

VOID APP_THREAD_MANAGER::AddReads(UINT64 count)
{
	ATOMIC::OPS::Increment(&m_readsCount, count);
}

UINT64 APP_THREAD_MANAGER::GetReadsCount()
{
	return ATOMIC::OPS::Load(&m_readsCount);
}

VOID APP_THREAD_MANAGER::AddWrites(UINT64 count)
{
	ATOMIC::OPS::Increment(&m_writesCount, count);
}

UINT64 APP_THREAD_MANAGER::GetWritesCount()
{
	return ATOMIC::OPS::Load(&m_writesCount);
}