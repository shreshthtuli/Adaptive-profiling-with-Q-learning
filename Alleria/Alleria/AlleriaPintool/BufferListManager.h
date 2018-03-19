#ifndef BUFFERLISTMANAGER_H
#define BUFFERLISTMANAGER_H

#include "pin.H"
#include "AppThreadManager.h"
#include <list>
#include "PortableApi.h"

struct BUFFER_STATE
{
	// Points to the first element in the buffer.
	VOID *m_bufferStart;

	// Points to past the last element in the buffer.
	VOID *m_bufferEnd;

	// Represents the app thread manager that created the buffer.
	APP_THREAD_MANAGER *m_appThreadManager;
};

// Represents a list of buffers that are either full and yet to be processed or
// empty and yet to be filled.
class BUFFER_LIST_MANAGER
{
public:
	BUFFER_LIST_MANAGER();
	
	static VOID SetBufferSize(UINT32 bufferSize);

	VOID AddFullBuffer(
		// The address of the first element in the buffer.
		VOID *bufferStart,

		// The address of past the last element in the buffer.
		VOID *bufferEnd,

		// The app thread manager that created the buffer.
		APP_THREAD_MANAGER *atm,

		// The ID of the thread calling this function.
		THREADID tid
		);

	VOID AddNewFreeBuffer(THREADID tid);

	VOID AddFreeBuffer(
		// The address of the first element in the buffer.
		VOID *bufferStart,

		// The ID of the thread calling this function.
		THREADID tid
		);

	BOOL AcquireFullBuffer(
		BUFFER_STATE& buffer,

		// The ID of the thread calling this function.
		THREADID tid
		);

	BOOL AcquireFreeBuffer(
		// The address of the first element in the MemRef buffer.
		VOID **bufferStart,

		// The ID of the thread calling this function.
		THREADID tid
		);

	// Returns the number of buffers in the list.
	// TODO: Use list<BUFFER_STATE>::size_type when it becomes available
	size_t GetCount();

	// Tells all processing threads that the process is terminating.
	VOID SignalTermination();

private:

	// The list of buffers.
	list<BUFFER_STATE> m_bufferList;

	// A Pin lock used to manage the buffer list in a thread-safe way.
	PIN_LOCK m_bufferListLock;
	
	// A semaphore used to coordinate all processing threads.
	// Cannot use PIN_SEMAPHORE because that's a binary semaphore and we need a counting sem.
	PSEMAPHORE m_bufferListSem;

	// The size of a buffer.
	static UINT32 s_bufferSize;
};

#endif /* BUFFERLISTMANAGER_H */