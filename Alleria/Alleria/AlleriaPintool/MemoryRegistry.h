#ifndef MEMORYREGISTRY_H
#define MEMORYREGISTRY_H

#include "pin.H"
#include "PortableApi.h"
#include <set> // Use unordered set in C++11
#include "WindowsLiveLandsWalker.h"

struct MEMORY_REGISTRY_ENTRY
{
	REGION_USAGE_TYPE type;
	REGION_USER user;
	ADDRINT id;
	ADDRINT baseAddr;
};

//extern void* AlleriaFastAlloc(size_t size);
//extern void FlushAlleriaRuntimeBuffer();

namespace std {
	/*template <>
	struct hash<MEMORY_REGISTRY_ENTRY>
	{
		typedef MEMORY_REGISTRY_ENTRY argument_type;
		typedef std::size_t  result_type;

		result_type operator()(const MEMORY_REGISTRY_ENTRY& ent) const
		{
			return ent.baseAddr;
		}
	};*/

	//template <>
	//struct equal_to<MEMORY_REGISTRY_ENTRY>
	//{
	//	bool operator()(const MEMORY_REGISTRY_ENTRY &lhs, const MEMORY_REGISTRY_ENTRY &rhs) const
	//	{
	//		return lhs.baseAddr != rhs.baseAddr;
	//	}
	//};

	//template<class _Ty>
	//class SpecialAlloc
	//{	// special allocator for objects of class MEMORY_REGISTRY_ENTRY
	//public:
	//	typedef SpecialAlloc<_Ty> other;

	//	typedef _Ty value_type;

	//	typedef value_type *pointer;
	//	typedef const value_type *const_pointer;
	//	typedef void *void_pointer;
	//	typedef const void *const_void_pointer;

	//	typedef value_type& reference;
	//	typedef const value_type& const_reference;

	//	typedef size_t size_type;
	//	typedef ptrdiff_t difference_type;

	//	//typedef false_type propagate_on_container_copy_assignment;
	//	//typedef false_type propagate_on_container_move_assignment;
	//	//typedef false_type propagate_on_container_swap;

	//	template<class _Other>
	//	struct rebind
	//	{	// convert this type to allocator<_Other>
	//		typedef SpecialAlloc<_Other> other;
	//	};

	//	pointer address(reference _Val) //const _NOEXCEPT
	//	{	// return address of mutable _Val
	//		return (_STD addressof(_Val));
	//	}

	//	const_pointer address(const_reference _Val) //const _NOEXCEPT
	//	{	// return address of nonmutable _Val
	//		return (_STD addressof(_Val));
	//	}

	//	SpecialAlloc() //_THROW0()
	//	{	// construct default allocator

	//	}

	//	template<class _Other>
	//	SpecialAlloc(const SpecialAlloc<_Other>& other) : SpecialAlloc() {}

	//	SpecialAlloc(const SpecialAlloc<_Ty>& other) : SpecialAlloc() {}

	//	SpecialAlloc<_Ty> select_on_container_copy_construction() const;

	//	template<class _Other>
	//	SpecialAlloc<_Ty>& operator=(const SpecialAlloc<_Other>&)
	//	{	// assign from a related allocator (do nothing)
	//		return (*this);
	//	}

	//	void deallocate(pointer _Ptr, size_type)
	//	{	// deallocate object at _Ptr, ignore size
	//		::operator delete(_Ptr);
	//	}

	//	pointer allocate(size_type _Count)
	//	{	// allocate array of _Count elements
	//		return reinterpret_cast<pointer>(AlleriaFastAlloc(_Count * sizeof(value_type)));
	//	}

	//	pointer allocate(size_type _Count, const void *)
	//	{	// allocate array of _Count elements, ignore hint
	//		return (allocate(_Count));
	//	}

	//	void construct(_Ty *_Ptr)
	//	{	// default construct object at _Ptr
	//		::new ((void *)_Ptr) _Ty();
	//	}

	//	void construct(_Ty *_Ptr, const _Ty& _Val)
	//	{	// construct object at _Ptr with value _Val
	//		::new ((void *)_Ptr) _Ty(_Val);
	//	}

	//	template<class _Objty,
	//	class... _Types>
	//		void construct(_Objty *_Ptr, _Types&&... _Args)
	//	{	// construct _Objty(_Types...) at _Ptr
	//		::new ((void *)_Ptr) _Objty(_STD forward<_Types>(_Args)...);
	//	}

	//	template<class _Uty>
	//	void destroy(_Uty *_Ptr)
	//	{	// destroy object at _Ptr
	//		_Ptr->~_Uty();
	//	}

	//	size_t max_size() const //_THROW0()
	//	{	// estimate maximum array size
	//		return ((size_t)(-1) / sizeof(_Ty));
	//	}
	//};
}


void RegisterAlleriaHeap(void *addrInHeap, void *id); // Used by Alleria's runtime heap.

class MEMORY_REGISTRY
{
public:
	// Initializes MEMORY_REGISTRY.
	// This must be called if more than one thread may use MEMORY_REGISTRY.
	// This must be called before using Find.
	LOCALFUN VOID Init();

	LOCALFUN VOID Destroy();

	/*
	addrInStack: any address in the stack region
	id         : the os thread id
	rUser      : who created the stack's thread
	*/
	LOCALFUN VOID RegisterStack(ADDRINT addrInStack, ADDRINT id, REGION_USER rUser);

	/*
	addrInHeap: any address in any of the heap regions
	id         : the heap's handle
	rUser      : who is allocating from the heap
	*/
	LOCALFUN VOID RegisterHeap(ADDRINT addrInHeap, ADDRINT id, REGION_USER rUser);

	/*
	baseAddr: the base address of the region to which the image is mapped
	rUser      : who is using the image
	*/
	LOCALFUN VOID RegisterImage(ADDRINT baseAddr, REGION_USER rUser);


	/*
	baseAddr: the base address of the region to which is mapped
	rUser      : who is using the map
	*/
	LOCALFUN VOID RegisterMappedMemory(ADDRINT baseAddr, REGION_USER rUser);


	/*
	baseAddr: the base address of the allocated space
	rUser      : who is using the allocated space
	*/
	LOCALFUN VOID RegisterGeneral(ADDRINT baseAddr, REGION_USER rUser);

	LOCALFUN BOOL Unregister(ADDRINT baseAddr);

	/*
	baseAddr    : the base address of the region of interest
	entry       : the MEMORY_REGISTRY_ENTRY object describing the specified region
	return value: found (TRUE) or not
	*/
	LOCALFUN BOOL Find(ADDRINT baseAddr, MEMORY_REGISTRY_ENTRY& entry);

	LOCALVAR ADDRINT hDefaultHeap; // Windows' process default heap;

	LOCALVAR BOOL TryInsertInRegistryInternal(
		MEMORY_REGISTRY_ENTRY& entry);

private:

	typedef std::map<ADDRINT, MEMORY_REGISTRY_ENTRY/*,
		hash<MEMORY_REGISTRY_ENTRY >,
		equal_to<MEMORY_REGISTRY_ENTRY >,
		SpecialAlloc<MEMORY_REGISTRY_ENTRY >*/ > myset;

	LOCALVAR PIN_LOCK registryLock;

	LOCALVAR  myset registry;

	LOCALVAR BOOL TryInsertInRegistry(
		MEMORY_REGISTRY_ENTRY& entry);
};

#endif /* MEMORYREGISTRY_H */