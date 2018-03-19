#ifdef TARGET_WINDOWS

#include "WindowsLiveLandsWalker.h"

namespace WIND
{
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <psapi.h>
}

WIND::HANDLE WINDOWS_LIVE_LANDS_WALKER::m_pHandle;

VOID WINDOWS_LIVE_LANDS_WALKER::Init()
{
	m_pHandle = WIND::GetCurrentProcess();
}

VOID WINDOWS_LIVE_LANDS_WALKER::Destroy(){}

BOOL WINDOWS_LIVE_LANDS_WALKER::FindSpecialRegions(REGION& pin, REGION& alleria, std::vector<REGION>& others)
{
	WIND::LPCVOID pvAddress = NULL;
	REGION region;
	while (TakeAStep(region, pvAddress))
	{
		if (SetRegionUsageType(region))
		{
			if (region.userId.find("AlleriaPintool.dll") != EOF)
				alleria = region;
			else if (region.userId.find("pinvm.dll") != EOF)
				pin = region;
			else
				others.push_back(region);
		}
		else if (region.dwStorage & MEM_MAPPED)
			others.push_back(region);

		pvAddress = ((WIND::PBYTE)region.pvBaseAddress + region.Size);
	}
	return TRUE;
}

BOOL WINDOWS_LIVE_LANDS_WALKER::WalkTheRegion(REGION& region)
{
	WIND::MEMORY_BASIC_INFORMATION mbi;
	WIND::LPCVOID pvAddress = region.pvBaseAddress;
	WIND::BOOL bOk = TRUE;
	region.dwStorage = 0;
	region.Size = 0;
	while (bOk)
	{
		bOk = (WIND::VirtualQuery(pvAddress, &mbi, sizeof(mbi))
			== sizeof(mbi));

		if (!bOk || region.pvBaseAddress != mbi.AllocationBase) // We went past the region.
			break;

		BLOCK block;
		switch (mbi.State)
		{
		case MEM_COMMIT:
			block.pvBaseAddress = mbi.BaseAddress;
			block.dwStorage = mbi.Type;
			block.Size = mbi.RegionSize;
			block.dwProtection = mbi.Protect;
			break;
		case MEM_RESERVE:
			block.pvBaseAddress = mbi.BaseAddress;
			block.dwStorage = MEM_RESERVE;
			block.Size = mbi.RegionSize;
			block.dwProtection = region.dwProtection;
			break;
		case MEM_FREE: // A block of a region cannot be free. Only a whole region can be free.
		default:
			return FALSE;
		}

		region.Size += block.Size;
		region.dwStorage |= block.dwStorage;
		region.blocks.push_back(block);
		pvAddress = ((WIND::PBYTE)block.pvBaseAddress + block.Size);
	}
	return TRUE;
}

BOOL WINDOWS_LIVE_LANDS_WALKER::TakeAWalk(std::vector<REGION>& regions)
{
	WIND::LPCVOID pvAddress = NULL;
	while (TRUE)
	{
		REGION region;
		if (!TakeAStep(region, pvAddress))
			break;
		SetRegionUsageType(region);
		if (region.dwStorage != MEM_FREE)
			regions.push_back(region);
		pvAddress = ((WIND::PBYTE)region.pvBaseAddress + region.Size);
	}
	return TRUE;
}

BOOL WINDOWS_LIVE_LANDS_WALKER::TakeAStep(REGION& region, WIND::LPCVOID pvAddress)
{
	WIND::MEMORY_BASIC_INFORMATION mbi;
	WIND::BOOL bOk = (WIND::VirtualQuery(pvAddress, &mbi, sizeof(mbi))
		== sizeof(mbi));

	if (!bOk) // We went past the user-mode address space.
		return FALSE;

	switch (mbi.State)
	{
	case MEM_FREE:
		region.pvBaseAddress = mbi.BaseAddress;
		region.dwStorage = MEM_FREE;
		region.dwProtection = 0;
		region.Size = mbi.RegionSize;
		break;
	case MEM_RESERVE:
	case MEM_COMMIT:
	{
		region.pvBaseAddress = mbi.AllocationBase;
		region.dwProtection = mbi.AllocationProtect;
		if (WalkTheRegion(region))
			break;
	}
	}

	return bOk != 0;
}

BOOL WINDOWS_LIVE_LANDS_WALKER::SetRegionUsageType(REGION& region)
{
	WIND::CHAR buff[MAX_PATH];

	// Check to see if it's a memory-mapped data or image file.
	WIND::DWORD dwLen = WIND::GetMappedFileName(m_pHandle, region.pvBaseAddress, buff, MAX_PATH);
	if (dwLen != 0)
	{
		// It's a memory-mapped data or image file.
		region.usageType = REGION_USAGE_TYPE::Mapped;
		region.userId = buff;
		return TRUE;
	}
	else
		region.userId = "";
	return FALSE;
}

ADDRINT WINDOWS_LIVE_LANDS_WALKER::GetBaseAddr(ADDRINT addr)
{
	WIND::MEMORY_BASIC_INFORMATION mbi;
	WIND::BOOL bOk = (WIND::VirtualQuery((WIND::LPCVOID)addr, &mbi, sizeof(mbi))
		== sizeof(mbi));
	if (!bOk)
	{
		return NULL;
	}
	return (ADDRINT)mbi.AllocationBase;
}

std::string BLOCK::ToString()
{
	std::stringstream stringStream;
	stringStream << std::hex << reinterpret_cast<ADDRINT>(pvBaseAddress) << ", " <<
		Size << ", " << MemoryProtectionFlagstoString(dwProtection) << ", " <<
		MemoryAllocationFlagsToString(dwStorage);
	return stringStream.str();
}

void BLOCK::DumpToBinaryStream(std::ofstream& ofs)
{
	ofs.write(reinterpret_cast<char*>(&pvBaseAddress), g_virtualAddressSize);
	WriteBinary(ofs, dwProtection);
	WriteBinary(ofs, dwStorage);
}

std::string REGION::ToString()
{
	std::stringstream stringStream;
	stringStream << std::hex << reinterpret_cast<ADDRINT>(pvBaseAddress) << ", " <<
		Size << ", " << MemoryProtectionFlagstoString(dwProtection) << ", " <<
		MemoryAllocationFlagsToString(dwStorage);

	if (dwStorage != MEM_FREE)
		stringStream << ", " << RegionUsageString(usageType) << ", " <<
		RegionUserString(user) << ", " << userId;

	stringStream << "\n";

	for (UINT32 i = 0; i < blocks.size(); ++i)
	{
		stringStream << "          " << blocks[i].ToString() << "\n";
	}
	return stringStream.str();
}

void REGION::DumpToBinaryStream(std::ofstream& ofs)
{
	size_t size = blocks.size();

	// Dump region header.

	ofs.write(reinterpret_cast<char*>(&pvBaseAddress), g_virtualAddressSize);

	WriteBinary(ofs, dwProtection);
	WriteBinary(ofs, dwStorage);
	WriteBinary(ofs, usageType);
	WriteBinary(ofs, user);

	UINT16 userIdSize = (UINT16)userId.size(); // Truncate. Max size is 0xFFFF.
	ofs.write(reinterpret_cast<const char*>(&userIdSize), sizeof(userIdSize));
	ofs.write(userId.c_str(), userIdSize);

	WriteBinary(ofs, size);

	// Dump region blocks.

	for (UINT32 i = 0; i < blocks.size(); ++i)
	{
		blocks[i].DumpToBinaryStream(ofs);
	}
}

std::string RegionUsageString(REGION_USAGE_TYPE type)
{
	std::string output = "";
	if (type & REGION_USAGE_TYPE::Default)
	{
		output += "Default | ";
		type &= ~REGION_USAGE_TYPE::Default;
	}

	switch (type)
	{
	case REGION_USAGE_TYPE::Unknown:
		output += "Unknown";
		break;
	case REGION_USAGE_TYPE::Stack:
		output += "Stack";
		break;
	case REGION_USAGE_TYPE::Heap:
		output += "Heap";
		break;
	case REGION_USAGE_TYPE::Mapped:
		output += "Mapped";
		break;
	case REGION_USAGE_TYPE::Image:
		output += "Image";
		break;
	case REGION_USAGE_TYPE::General:
		output += "General";
		break;
	default:
		std::string temp;
		IntegerToString(static_cast<rut_ut>(type), temp);
		output += temp;
		break;
	}
	return output;
}

std::string RegionUserString(REGION_USER user)
{
	if (user == REGION_USER::UnKnown)
		return  "UnKnown";

	std::string output = "";
	BOOL first = TRUE;
	REGION_USER ru = user;

	while (static_cast<ru_ut>(ru) != 0)
	{
		if (!first)
			output += " | ";

		if (ru & REGION_USER::Alleria)
		{
			output += "Alleria";
			ru &= ~REGION_USER::Alleria;
		}
		else if (ru & REGION_USER::Pin)
		{
			output += "Pin";
			ru &= ~REGION_USER::Pin;
		}
		else if (ru & REGION_USER::App)
		{
			output += "App";
			ru &= ~REGION_USER::App;
		}
		else if (ru & REGION_USER::Platform)
		{
			output += "Platform";
			ru &= ~REGION_USER::Platform;
		}
		else
		{
			std::string temp;
			IntegerToString(static_cast<ru_ut>(ru), temp);
			output += temp;
			ru = REGION_USER::UnKnown;
			break;
		}

		first = FALSE;
	}
	return output;
}

#endif /* TARGET_WINDOWS */
