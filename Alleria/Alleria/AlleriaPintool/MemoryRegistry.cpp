#include "MemoryRegistry.h"
#include <iostream>

/*

ASSERT* should not be used in the memory registry implementation because these functions
use std::string. The allocator of std::string cannot be controlled and so the default will be used.
This is a problem because allocating from the runtime heap while running inside the registry might
case deadlocks.

*/

ADDRINT MEMORY_REGISTRY::hDefaultHeap;
PIN_LOCK MEMORY_REGISTRY::registryLock;


MEMORY_REGISTRY::myset MEMORY_REGISTRY::registry;

VOID MEMORY_REGISTRY::Init()
{
	void *heapID;
	GetProcessDefaultHeapAddr(NULL, &heapID);
	MEMORY_REGISTRY::hDefaultHeap = (ADDRINT)heapID;
	PIN_InitLock(&registryLock);

	MEMORY_REGISTRY_ENTRY entry;
	entry.id = 0;
	entry.type = REGION_USAGE_TYPE::General;
	entry.user = REGION_USER::Platform;
	entry.baseAddr = 0;
	TryInsertInRegistry(entry);
}

VOID MEMORY_REGISTRY::Destroy()
{
}

VOID MEMORY_REGISTRY::RegisterStack(ADDRINT addrInStack, ADDRINT id, REGION_USER rUser)
{
	MEMORY_REGISTRY_ENTRY entry;
	entry.id = id;
	entry.type = REGION_USAGE_TYPE::Stack;
	entry.user = rUser;
	entry.baseAddr = WINDOWS_LIVE_LANDS_WALKER::GetBaseAddr(addrInStack);
	TryInsertInRegistry(entry);
}

VOID MEMORY_REGISTRY::RegisterHeap(ADDRINT addrInHeap, ADDRINT id, REGION_USER rUser)
{
	MEMORY_REGISTRY_ENTRY entry;
	entry.id = id;
	entry.type = REGION_USAGE_TYPE::Heap;
	if(id == hDefaultHeap)
		entry.type |= REGION_USAGE_TYPE::Default;
	entry.user = rUser;
	entry.baseAddr = WINDOWS_LIVE_LANDS_WALKER::GetBaseAddr(addrInHeap);
	TryInsertInRegistry(entry);
}

VOID MEMORY_REGISTRY::RegisterGeneral(ADDRINT baseAddr, REGION_USER rUser)
{
	MEMORY_REGISTRY_ENTRY entry;
	entry.id = 0;
	entry.type = REGION_USAGE_TYPE::General;
	entry.user = rUser;
	entry.baseAddr = WINDOWS_LIVE_LANDS_WALKER::GetBaseAddr(baseAddr);
	TryInsertInRegistry(entry);
}

VOID MEMORY_REGISTRY::RegisterImage(ADDRINT baseAddr, REGION_USER rUser)
{
	MEMORY_REGISTRY_ENTRY entry;
	entry.id = 0;
	entry.type = REGION_USAGE_TYPE::Image;
	entry.user = rUser;
	entry.baseAddr = baseAddr;
	TryInsertInRegistry(entry);
}

VOID MEMORY_REGISTRY::RegisterMappedMemory(ADDRINT baseAddr, REGION_USER rUser)
{
	MEMORY_REGISTRY_ENTRY entry;
	entry.id = 0;
	entry.type = REGION_USAGE_TYPE::Mapped;
	entry.user = rUser;
	entry.baseAddr = baseAddr;
	TryInsertInRegistry(entry);
}

BOOL MEMORY_REGISTRY::Find(ADDRINT baseAddr, MEMORY_REGISTRY_ENTRY& entry)
{
	BOOL result = TRUE;
	PIN_GetLock(&registryLock, PIN_ThreadId());
	myset::const_iterator it = registry.find(entry.baseAddr);
	myset::const_iterator end = registry.end();
	if (it != end)
		entry = it->second;
	else
		result = FALSE;
	PIN_ReleaseLock(&registryLock);
	return result;
}

BOOL MEMORY_REGISTRY::TryInsertInRegistry(
	MEMORY_REGISTRY_ENTRY& entry)
{
	PIN_GetLock(&registryLock, PIN_ThreadId());
	BOOL success = TryInsertInRegistryInternal(entry);

	// This function must be called at this point.
	// It's not thread-safe, but no problem since we already got the lock.
	//FlushAlleriaRuntimeBuffer();
	
	PIN_ReleaseLock(&registryLock);
	return success;
}

BOOL MEMORY_REGISTRY::TryInsertInRegistryInternal(MEMORY_REGISTRY_ENTRY& entry)
{
	myset::const_iterator it = registry.find(entry.baseAddr);
	BOOL success = TRUE;
	if (it == registry.end())
	{
		registry.insert(std::make_pair(entry.baseAddr, entry));
	}
	else
	{
		if (it->second.type != REGION_USAGE_TYPE::Stack && 
			(it->second.type != entry.type || it->second.id != entry.id))
		{
			registry.erase(it->first);
			registry.insert(std::make_pair(entry.baseAddr, entry));
		}
		else if (!(it->second.user & entry.user))
		{
			MEMORY_REGISTRY_ENTRY newEntry = it->second;
			registry.erase(it->first);
			newEntry.user |= entry.user;
			registry.insert(std::make_pair(newEntry.baseAddr, newEntry));
		}
		success = FALSE;
	}
	return success;
}

BOOL MEMORY_REGISTRY::Unregister(ADDRINT baseAddr)
{
	BOOL success;
	PIN_GetLock(&registryLock, PIN_ThreadId());
	MEMORY_REGISTRY_ENTRY entry;
	entry.baseAddr = baseAddr;
	myset::const_iterator it = registry.find(entry.baseAddr);
	if (it == registry.end())
	{
		success = FALSE;
	}
	else
	{
		registry.erase(entry.baseAddr);
		success = TRUE;
	}
	PIN_ReleaseLock(&registryLock);
	return success;
}

void RegisterAlleriaHeap(void *addrInHeap, void *id)
{
	MEMORY_REGISTRY::RegisterHeap(reinterpret_cast<ADDRINT>(addrInHeap), 
		reinterpret_cast<ADDRINT>(id), REGION_USER::Alleria);
}

void RegisterAlleriaHeapNoLock(void *addrInHeap, void *id)
{
	MEMORY_REGISTRY_ENTRY entry;
	entry.baseAddr = reinterpret_cast<ADDRINT>(addrInHeap);
	entry.user = REGION_USER::Alleria;
	entry.type = REGION_USAGE_TYPE::Heap;
	entry.id = reinterpret_cast<ADDRINT>(id);
	MEMORY_REGISTRY::TryInsertInRegistryInternal(entry);
}

REGION_USAGE_TYPE& operator |=(REGION_USAGE_TYPE& lhs, REGION_USAGE_TYPE rhs)
{
	return lhs = static_cast<REGION_USAGE_TYPE>(static_cast<rut_ut>(lhs) | static_cast<rut_ut>(rhs));
}

REGION_USAGE_TYPE& operator &=(REGION_USAGE_TYPE& lhs, REGION_USAGE_TYPE rhs)
{
	return lhs = static_cast<REGION_USAGE_TYPE>(static_cast<rut_ut>(lhs)& static_cast<rut_ut>(rhs));
}

REGION_USAGE_TYPE operator ~(const REGION_USAGE_TYPE& operand)
{
	return static_cast<REGION_USAGE_TYPE>(~static_cast<rut_ut>(operand));
}

BOOL operator &(REGION_USAGE_TYPE lhs, REGION_USAGE_TYPE rhs)
{
	return (static_cast<rut_ut>(lhs)& static_cast<rut_ut>(rhs)) != 0;
}

REGION_USER& operator |=(REGION_USER& lhs, REGION_USER rhs)
{
	return lhs = static_cast<REGION_USER>(static_cast<ru_ut>(lhs) | static_cast<ru_ut>(rhs));
}

BOOL operator &(REGION_USER lhs, REGION_USER rhs)
{
	return (static_cast<ru_ut>(lhs)& static_cast<ru_ut>(rhs)) != 0;
}

REGION_USER operator |(REGION_USER lhs, REGION_USER rhs)
{
	return static_cast<REGION_USER>(static_cast<ru_ut>(lhs) | static_cast<ru_ut>(rhs));
}

REGION_USER& operator &=(REGION_USER& lhs, REGION_USER rhs)
{
	return lhs = static_cast<REGION_USER>(static_cast<ru_ut>(lhs)& static_cast<ru_ut>(rhs));
}

REGION_USER operator ~(const REGION_USER& operand)
{
	return static_cast<REGION_USER>(~static_cast<ru_ut>(operand));
}