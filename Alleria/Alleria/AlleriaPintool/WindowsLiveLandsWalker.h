#ifdef TARGET_WINDOWS

#ifndef WINDOWSLIVELANDSWALKER_H
#define WINDOWSLIVELANDSWALKER_H

#include "pin.H"
#include "Utils.h"
#include <fstream>
#include <vector>
#include <sstream>
#include <set>

enum class REGION_USAGE_TYPE : UINT16
{
	Unknown = 0x00,
	Stack = 0x01,
	Heap = 0x02,

	// This value must be OR'ed with one of the other values, it cannot exist alone.
	// It can only be combined with Heap.
	Default = 0x04,

	Mapped = 0x08,
	Image = 0x10,
	General = 0x20
};

enum class REGION_USER : UINT16
{
	UnKnown = 0x00,
	Alleria = 0x01,
	Pin = 0x02,
	App = 0x04,
	Platform = 0x08 // Includes OS and runtime
};

//typedef std::underlying_type<REGION_USAGE_TYPE>::type rut_ut;
//typedef std::underlying_type<REGION_USER>::type ru_ut;

typedef int rut_ut;
typedef int ru_ut;

REGION_USAGE_TYPE& operator |=(REGION_USAGE_TYPE& lhs, REGION_USAGE_TYPE rhs);
REGION_USAGE_TYPE& operator &=(REGION_USAGE_TYPE& lhs, REGION_USAGE_TYPE rhs);
REGION_USAGE_TYPE operator ~(const REGION_USAGE_TYPE& operand);
BOOL operator &(REGION_USAGE_TYPE lhs, REGION_USAGE_TYPE rhs);
REGION_USER& operator |=(REGION_USER& lhs, REGION_USER rhs);
BOOL operator &(REGION_USER lhs, REGION_USER rhs);
REGION_USER operator |(REGION_USER lhs, REGION_USER rhs);
REGION_USER& operator &=(REGION_USER& lhs, REGION_USER rhs);
REGION_USER operator ~(const REGION_USER& operand);
std::string RegionUsageString(REGION_USAGE_TYPE type);
std::string RegionUserString(REGION_USER user);

extern UINT8 g_virtualAddressSize;

// A block is a memory area with the following attributes:
// - The state (MEM_*) of all pages is the same.
// - If the initial page is not free, all pages in the region are part of 
//   the same initial allocation of pages created by a single call to 
//   VirtualAlloc, MapViewOfFile, or one of the following extended versions of these functions.
// - The access (PAGE_*) granted to all pages is the same.
struct BLOCK
{
	void            *pvBaseAddress; // The address of the block
	unsigned long   dwProtection;  // PAGE_*
	size_t   Size;          // The size of the block
	unsigned long   dwStorage;     // MEM_*

	std::string ToString();
	void DumpToBinaryStream(std::ofstream& ofs);
};

// A region is a memory area that has either been allocated using a single
// call to VirtualAlloc or is free.
struct REGION
{
	void               *pvBaseAddress; // The address of the region
	UINT32             dwProtection;  // PAGE_*
	size_t             Size;          // The total size of the region
	UINT32             dwStorage;     // MEM_*
	std::vector<BLOCK> blocks;     // The blocks of the region.

	REGION_USAGE_TYPE usageType; // Invalid if dwStorage is MEM_FREE
	REGION_USER user; // Invalid if dwStorage is MEM_FREE

	// Depending on the usageType:
	// Unknown => invalid
	// Stack => os thread id
	// Heap => heap handle
	// Mapped => file path or map name
	// image => image name
	std::string userId; // Invalid if dwStorage is MEM_FREE

	std::string ToString();
	void DumpToBinaryStream(std::ofstream& ofs);
};

//namespace std {
//	template <>
//	struct hash<MODULEENTRY32_SUMMARY>
//	{
//		typedef MODULEENTRY32_SUMMARY argument_type;
//		typedef std::size_t  result_type;
//
//		result_type operator()(const MODULEENTRY32_SUMMARY & mod) const
//		{
//			return reinterpret_cast<std::size_t>(mod.modBaseAddr);
//		}
//	};
//
//	template <>
//	struct equal_to<MODULEENTRY32_SUMMARY>
//	{
//		bool operator()(const MODULEENTRY32_SUMMARY &lhs, const MODULEENTRY32_SUMMARY &rhs) const
//		{
//			return lhs.modBaseAddr == rhs.modBaseAddr;
//		}
//	};
//}

class WINDOWS_LIVE_LANDS_WALKER
{
public:
	LOCALFUN VOID Init();
	LOCALFUN VOID Destroy();
	LOCALFUN BOOL TakeAWalk(std::vector<REGION>& regions);
	LOCALFUN BOOL FindSpecialRegions(REGION& pin, REGION& alleria, std::vector<REGION>& others);

	// Gets the base address of the region containing the specified address.
	LOCALFUN ADDRINT GetBaseAddr(ADDRINT addr);

private:
	LOCALFUN BOOL WalkTheRegion(REGION& region);
	LOCALFUN BOOL TakeAStep(REGION& region, const void *pvAddress);
	LOCALFUN BOOL SetRegionUsageType(REGION& region);

	LOCALFUN void *m_pHandle;
};

#endif /* WINDOWSLIVELANDSWALKER_H */

#endif /* TARGET_WINDOWS */