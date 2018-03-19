#ifndef APPTHREADMANAGER_H
#define APPTHREADMANAGER_H

#include "pin.H"
#include "WindowConfig.h"
#include "PortableApi.h"
#include <deque>

class APP_THREAD_MANAGER
{
public:
	APP_THREAD_MANAGER(THREADID tid, UINT32 bufferSize);
	~APP_THREAD_MANAGER();

	VOID* GetCurrentBuffer();
	VOID* GetCurrentBufferLast();
	VOID SetCurrentBuffer(VOID *buffer);

	VOID IncrementPendingFullBufferCounter();
	VOID DecrementPendingFullBufferCounter();
	BOOL AreAllBuffersProcessed();


	VOID EnqueFullAndGetFree(
		VOID **bufferEnd,
		VOID **bufferLast);

	OS_THREAD_ID GetOSThreadId();

	PIN_THREAD_UID GetThreadUid();

	VOID AddInsCount(UINT64 count);
	UINT64 GetInsCount();
	UINT64 GetInsLastCount();

	VOID AddMemInsCount(UINT64 count);
	UINT64 GetMemInsCount();

	VOID AddReads(UINT64 count);
	UINT64 GetReadsCount();

	VOID AddWrites(UINT64 count);
	UINT64 GetWritesCount();

	TIME_STAMP GetWaitingTime();

	// The size of a buffer.
	const UINT32 m_bufferSize;
private:

	VOID APP_THREAD_MANAGER::GetFreeBuffer();

	// The address of the buffer location currently being filled;
	VOID *m_currentBuffer;

	// The Pin thread id of the application thread that is being instrumented.
	THREADID m_tid;

	// A Pin lock used to safely increment pendingFullBufferCounter.
	PIN_LOCK m_pendingFullBufferCounterLock;

	// Keeps track of the number of full buffers that are yet to be processed.
	// This is used to prevent the application from terminating before all buffers are processed.
	UINT32 m_pendingFullBufferCounter;

	// The Id of the thread assigned by the OS.
	OS_THREAD_ID m_osThreadId;

	// The unique pin thread id of the app thread.
	// Used to compute the thread number.
	PIN_THREAD_UID m_tuid;

	// The number of instructions executed on this thread.
	UINT64 m_insCount;

	// The number of instructions executed on this thread in the last time the buffer was processed.
	UINT64 m_insLastCount;

	// The number of instructions that accessed memory and executed on this thread.
	UINT64 m_memInsCount;

	UINT64 m_readsCount;
	UINT64 m_writesCount;

	TIME_STAMP m_waitingTime;

	// The number of buffers filled by this thread.
	UINT64 m_bufferCount;
};


#endif /* APPTHREADMANAGER_H */