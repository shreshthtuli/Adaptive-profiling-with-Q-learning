///*
//
//This file defines global new and delete operators.
//
//*/
//
///*************************************************
//
//DO NOT EDIT UNLESS YOU KNOW WHAT YOU ARE DOING
//
//**************************************************/
//
//#include "PortableApi.h"
//
///*
//
//// internal.h
//
//extern "C" void* __onexitbegin;
//
//#ifdef _M_CEE
//#define _SUPPRESS_UNMANAGED_CODE_SECURITY [System::Security::SuppressUnmanagedCodeSecurity]
//#define SECURITYCRITICAL_ATTRIBUTE [System::Security::SecurityCritical]
//#define _INTEROPSERVICES_DLLIMPORT(_DllName , _EntryPoint , _CallingConvention) \
//    [System::Runtime::InteropServices::DllImport(_DllName , EntryPoint = _EntryPoint, CallingConvention = _CallingConvention)]
//#else
//#define _SUPPRESS_UNMANAGED_CODE_SECURITY
//#define SECURITYCRITICAL_ATTRIBUTE
//#define _INTEROPSERVICES_DLLIMPORT(_DllName , _EntryPoint , _CallingConvention)
//#endif
//
//_SUPPRESS_UNMANAGED_CODE_SECURITY
//SECURITYCRITICAL_ATTRIBUTE
//_RELIABILITY_CONTRACT
//_INTEROPSERVICES_DLLIMPORT("KERNEL32.dll", "DecodePointer", _CALLING_CONVENTION_WINAPI)
//WINBASEAPI
//_Ret_maybenull_
//PVOID
//WINAPI
//DecodePointer(
//_In_opt_ PVOID Ptr
//);
//
//*/
//
//extern void RegisterAlleriaHeap(void *addrInHeap, void *id);
//extern void RegisterAlleriaHeapNoLock(void *addrInHeap, void *id);
//
//typedef void*(*AllocFunc)(size_t);
//typedef void(*FreeFunc)(void*);
//
//static void* AlleriaAllocInit(size_t size);
//
//static void* DefaultAlloc(size_t size);
//static void* AlleriaAlloc(size_t size);
//void* AlleriaFastAlloc(size_t size);
//
//static void AlleriaFree(void* mem);
//static void DefaultFree(void* mem);
//
//static void DestroyAlloc();
//
//static AllocFunc pAlloc = AlleriaAllocInit;
//static FreeFunc  pFree = AlleriaFree;
//
//// Not getting compiled using VS2015. Only works on VS2013, but WDK 10 requires VS2015!
////void* operator new     (size_t size){ return pAlloc(size); }
////void* operator new[](size_t size) { return pAlloc(size); }
////void  operator delete  (void* mem) { pFree(mem); }
////void  operator delete[](void* mem) { pFree(mem); }
//
//#define BufferSize 1024
//
//struct AlleriaAllocator
//{
//	AlleriaAllocator() 
//		: m_pagesize(GetVirtualPageSize())
//	{
//		memset(m_buffer, 0, BufferSize);
//		m_bufferTop = -1;
//		GetProcessDefaultHeapAddr(NULL, &m_hHeap);
//	}
//
//	~AlleriaAllocator()
//	{
//		pAlloc = DefaultAlloc;
//		pFree = DefaultFree;
//	}
//	
//	/*
//	This function is not thread-safe and must be used by a single thread only.
//	It is intended to be used during runtime initialization only.
//	*/
//	void* AllocNoRegister(size_t size)
//	{
//		void *mem = malloc(size);
//		uintptr_t memInt = reinterpret_cast<uintptr_t>(mem);
//		uintptr_t memPrev = m_bufferTop == -1 ? memInt : reinterpret_cast<uintptr_t>(m_buffer[m_bufferTop]);
//		uintptr_t diff = (memInt > memPrev) ? memInt - memPrev :
//			memPrev - memInt;
//
//		if (m_bufferTop == -1 ||
//			diff >= m_pagesize)
//			m_buffer[++m_bufferTop] = mem;
//
//		// If the buffer is full, start over from the first slot and overwrite existing data.
//		if (m_bufferTop == BufferSize - 1)
//		{
//			m_bufferTop = -1;
//		}
//
//		return mem;
//	}
//
//	// This function must be used after runtime initialization.
//	void* AllocRegister(size_t size)
//	{
//		void *mem = malloc(size);
//		RegisterAlleriaHeap(mem, m_hHeap);
//		return mem;
//	}
//
//	/*
//	This function is not thread-safe and must be used by a single thread only.
//	It is intended to be used during runtime initialization only.
//	*/
//	void FlushBuffer()
//	{
//		int i = 0;
//		while (i <= m_bufferTop)
//		{
//			RegisterAlleriaHeapNoLock(m_buffer[i], m_hHeap);
//			m_buffer[i] = 0;
//			++i;
//		}
//		m_bufferTop = -1;
//	}
//
//	void Free(void* mem)
//	{
//		// Pin has a bug that sometimes it frees memory from the code cache
//		// using the delete operator. That's the whole purpose of using HeapValidate here.
//		// This is pain in the ass because it has perf consequences.
//		// Since we are using free, this is not a problem.
//		free(mem);
//	}
//
//	void *m_hHeap;
//	void *m_buffer[BufferSize];
//	int m_bufferTop;
//	const unsigned int m_pagesize;
//};
//
//static unsigned char AlleriaAllocatorInstance[sizeof(AlleriaAllocator)] = { 0 };
//
//static AlleriaAllocator& myAlloc = *(reinterpret_cast<AlleriaAllocator*>(&AlleriaAllocatorInstance[0]));
//
//void FlushAlleriaRuntimeBuffer()
//{
//	myAlloc.FlushBuffer();
//}
//
//static void* AlleriaAllocInit(size_t size)
//{
//	pAlloc = DefaultAlloc;
//	static AlleriaAllocator* mem = new(&myAlloc) AlleriaAllocator;
//	pAlloc = AlleriaAlloc;
//	return pAlloc(size);
//}
//
//static void* DefaultAlloc(size_t size)
//{
//	return malloc(size);
//}
//
//static void* AlleriaAlloc(size_t size)
//{
//	/* IMPORTANT NOTE
//	
//	Pin doesn't call the atexit function when terminating. So this code is actually useless.
//	This function should be removed and AlleriaFastAlloc should be used instead.
//
//	*/
//
//	/*
//	// Check whether it's safe to call atexit
//	PVOID onexitbegin = DecodePointer(__onexitbegin);
//	MEMORY_BASIC_INFORMATION mbi;
//	if (VirtualQuery(onexitbegin, &mbi, sizeof(mbi)) == sizeof(mbi))
//	{
//		if (mbi.State == MEM_COMMIT)
//		{
//			// The necessary CRT structures have been initialized.
//			// It's safe now to call atexit.
//			atexit(DestroyAlloc);
//			pAlloc = AlleriaFastAlloc;
//		}
//	}
//	*/
//	// Satisfy the allocation request.
//	return myAlloc.AllocNoRegister(size);
//}
//
//void* AlleriaFastAlloc(size_t size)
//{
//	return myAlloc.AllocNoRegister(size);
//}
//
//static void* AlleriaFastAllocRegister(size_t size)
//{
//	return myAlloc.AllocRegister(size);
//}
//
//void AlleriaRuntimeSwitchToAllocRegister()
//{
//	FlushAlleriaRuntimeBuffer();
//	pAlloc = AlleriaFastAllocRegister;
//}
//
//static void DestroyAlloc()
//{
//	myAlloc.~AlleriaAllocator();
//}
//
//static void AlleriaFree(void* mem)
//{
//	myAlloc.Free(mem);
//}
//
//static void DefaultFree(void* mem)
//{
//	free(mem);
//}