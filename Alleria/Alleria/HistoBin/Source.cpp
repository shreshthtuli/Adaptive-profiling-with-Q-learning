#include <iostream>
#include <map>
#include <fstream>
using namespace std;

#include "AbpApi.h"

UINT64 zeroesRead = 0;
UINT64 zeroesWritten = 0;
UINT64 OnesRead = 0;
UINT64 OnesWritten = 0;
UINT64 totalIns = 0;
UINT64 totalMemAcc = 0;

const UINT64 heatmapGranularity = 1024*1024; // Heatmap granularity in bytes. Must be power of 2.
const UINT64 heatmapGranularityBitMask = 0xFFFFffffFFF00000; // Depends on heatmapGranularity.
map<UINT64, UINT64> heatmap; // The key is the base address of a block of size heatmapGranularity
                             // and the value is the write count.

void IncrementStats(PBINARY_PROFILE_SECTION_GENERIC profileEntry);
void BuildHeatMap(PBINARY_PROFILE_SECTION_GENERIC profileEntry);
void EmitHeapmap();
UINT8 PopCnt(UINT8 byte);
BOOL ProcessProfile(PCSTR path);
BOOL ProcessLLW(PCSTR path);

int main(int argc, char *argv[])
{
	//if (argc != 2)
	//	return -1;

	BOOL error;

	error = ProcessProfile("D:\\PinRuns\\mcf 32 dbg\\g.ap");
	error = ProcessProfile("D:\\PinRuns\\mcf 32 opt\\1.ap");
	error = ProcessProfile("D:\\PinRuns\\mcf 32 opt\\2.ap");
	error = ProcessProfile("D:\\PinRuns\\mcf 32 opt\\3.ap");
	error = ProcessProfile("D:\\PinRuns\\mcf 32 opt\\4.ap");
	error = ProcessProfile("D:\\PinRuns\\mcf 32 opt\\5.ap");
	error = ProcessProfile("D:\\PinRuns\\mcf 32 opt\\6.ap");
	error = ProcessProfile("D:\\PinRuns\\mcf 32 opt\\7.ap");
	error = ProcessProfile("D:\\PinRuns\\mcf 32 opt\\8.ap");
	error = ProcessProfile("D:\\PinRuns\\mcf 32 opt\\9.ap");
	error = ProcessProfile("D:\\PinRuns\\mcf 32 opt\\10.ap");
	error = ProcessProfile("D:\\PinRuns\\mcf 32 opt\\11.ap");
	error = ProcessProfile("D:\\PinRuns\\mcf 32 opt\\12.ap");
	error = ProcessProfile("D:\\PinRuns\\mcf 32 opt\\13.ap");
	error = ProcessProfile("D:\\PinRuns\\mcf 32 opt\\14.ap");
	error = ProcessProfile("D:\\PinRuns\\mcf 32 opt\\15.ap");
	error = ProcessProfile("D:\\PinRuns\\mcf 32 opt\\16.ap");
	error = ProcessProfile("D:\\PinRuns\\mcf 32 opt\\17.ap");
	error = ProcessProfile("D:\\PinRuns\\mcf 32 opt\\18.ap");
	error = ProcessProfile("D:\\PinRuns\\mcf 32 opt\\19.ap");
	error = ProcessProfile("D:\\PinRuns\\mcf 32 opt\\20.ap");
	error = ProcessProfile("D:\\PinRuns\\mcf 32 opt\\21.ap");
	error = ProcessProfile("D:\\PinRuns\\mcf 32 opt\\22.ap");
	error = ProcessProfile("D:\\PinRuns\\mcf 32 opt\\23.ap");
	error = ProcessProfile("D:\\PinRuns\\mcf 32 opt\\24.ap");
	error = ProcessProfile("D:\\PinRuns\\mcf 32 opt\\25.ap");
	error = ProcessProfile("D:\\PinRuns\\mcf 32 opt\\26.ap");
	error = ProcessProfile("D:\\PinRuns\\mcf 32 opt\\27.ap");
	error = ProcessProfile("D:\\PinRuns\\mcf 32 opt\\28.ap");
	error = ProcessProfile("D:\\PinRuns\\mcf 32 opt\\29.ap");
	error = ProcessProfile("D:\\PinRuns\\mcf 32 opt\\30.ap");

	if (!error)
		EmitHeapmap();

	error = ProcessLLW("D:\\PinRuns\\mcf 32 dbg\\walker.ap");

	return error ? -1 : 0;
}

void EmitRegions(PBINARY_LLW_SECTION_ENTRY_REGION regions, UINT64 count, ofstream& ofs)
{
	ofs << "----Snapshot----\n" << std::hex;
	for (UINT64 i = 0; i < count; ++i)
	{
		ofs << "--Region--\n";
		//std::string str = std::string(regions[i].userId);
		ofs << regions[i].vAddr << ", "
			<< regions[i].user << ", "
			<< regions[i].usageType << ", " 
			<< regions[i].userId << "\n";
	}
}

BOOL ProcessLLW(PCSTR path)
{
	ofstream myfile;
	myfile.open("llw.txt");
	
	PROFILE_HANDLE profile = AbpOpenProfile(path);
	if (profile == INVALID_PROFILE_HANDLE)
		return -1;

	BOOL error = FALSE;

	UINT32 secDirSize = AbpGetProfileSectionDirectorySize(profile);
	for (UINT32 i = 0; i < secDirSize; ++i)
	{
		SECTION_DIRECTORY_ENTRY entry;
		if (!AbpGetProfileSectionDirectoryEntry(profile, i, &entry))
		{
			error = TRUE;
			break;
		}

		if (entry.type == BINARY_SECTION_TYPE_LLW)
		{
			BINARY_LLW_SECTION_ENTRY_SNAPSHOT profileEntry;
			if (!AbpGetLlwSnapshotFirst(profile, i, &profileEntry))
			{
				error = TRUE;
				break;
			}

			EmitRegions(profileEntry.regions, profileEntry.regionCount, myfile);

			BINARY_LLW_SECTION_ENTRY_SNAPSHOT profileEntryNext;
			while (AbpGetLlwSnapshotNext(profile, i, &profileEntry, &profileEntryNext))
			{
				EmitRegions(profileEntryNext.regions, profileEntryNext.regionCount, myfile);
				AbpDestroyLlwSnapshot(&profileEntry);
				profileEntry = profileEntryNext;
			}

			AbpDestroyLlwSnapshot(&profileEntry);
		}
	}

	myfile.close();
	AbpCloseProfile(profile);
	return error;
}

BOOL ProcessProfile(PCSTR path)
{
	PROFILE_HANDLE profile = AbpOpenProfile(path);
	if (profile == INVALID_PROFILE_HANDLE)
		return -1;

	BOOL error = FALSE;

	UINT32 secDirSize = AbpGetProfileSectionDirectorySize(profile);
	for (UINT32 i = 0; i < secDirSize; ++i)
	{
		SECTION_DIRECTORY_ENTRY entry;
		if (!AbpGetProfileSectionDirectoryEntry(profile, i, &entry))
		{
			error = TRUE;
			break;
		}

		if (entry.type == BINARY_SECTION_TYPE_PROFILE)
		{
			BINARY_PROFILE_SECTION_GENERIC profileEntry;
			if (!AbpGetProfileEntryFirst(profile, i, &profileEntry))
			{
				error = TRUE;
				break;
			}

			//IncrementStats(&profileEntry);
			BuildHeatMap(&profileEntry);

			BINARY_PROFILE_SECTION_GENERIC profileEntryNext;
			while (AbpGetProfileEntryNextFast(profile, i, &profileEntry, &profileEntryNext))
			{
				//IncrementStats(&profileEntryNext);
				BuildHeatMap(&profileEntryNext);
				AbpDestroyProfileEntry(profile, &profileEntry);
				profileEntry = profileEntryNext;
			}

			AbpDestroyProfileEntry(profile, &profileEntry);
		}
	}

	AbpCloseProfile(profile);
	return error;
}

void IncrementStats(PBINARY_PROFILE_SECTION_GENERIC profileEntry)
{
	if (profileEntry->type == BINARY_PROFILE_SECTION_ENTRY_TYPE_MEMORY_0)
	{
		PBINARY_PROFILE_SECTION_ENTRY_MEM mem = (PBINARY_PROFILE_SECTION_ENTRY_MEM)profileEntry->entry;
		UINT8 temp = 0;
		if (mem->memopType == MEM_OP_TYPE_LOAD || mem->memopType == MEM_OP_TYPE_STORE)
		{
			UINT32 index = mem->size;
			while (--index >= 0)
			{
				temp = PopCnt(mem->info[index]);
				if (mem->memopType == MEM_OP_TYPE_LOAD)
				{
					OnesRead += temp;
					zeroesRead += 8 - temp;
				}
				else // mem->memopType == MEM_OP_TYPE_STORE
				{
					OnesWritten += temp;
					zeroesWritten += 8 - temp;
				}
			}
		}
	}
}

UINT8 PopCnt(UINT8 byte)
{
	UINT8 mask, count;

	for (count = 0, mask = 0x80; mask != 0; mask >>= 1)
	{
		if (byte & mask)
			count++;
	}

	return count;
}

void UpdateHeapmap(UINT64 base)
{
	map<UINT64, UINT64>::iterator it = heatmap.find(base);
	if (it == heatmap.end())
	{
		heatmap.insert(std::make_pair(base, 1ll));
	}
	else
	{
		it->second = it->second + 1; // May overflow, but very unlikely.
	}
}

void BuildHeatMap(PBINARY_PROFILE_SECTION_GENERIC profileEntry)
{
	if (profileEntry->type == BINARY_PROFILE_SECTION_ENTRY_TYPE_INSTRUCTION_0 ||
		profileEntry->type == BINARY_PROFILE_SECTION_ENTRY_TYPE_INSTRUCTION_1 ||
		profileEntry->type == BINARY_PROFILE_SECTION_ENTRY_TYPE_INSTRUCTION_2)
	{
		++totalIns;
	}

	if (profileEntry->type == BINARY_PROFILE_SECTION_ENTRY_TYPE_MEMORY_0 ||
		profileEntry->type == BINARY_PROFILE_SECTION_ENTRY_TYPE_MEMORY_1 ||
		profileEntry->type == BINARY_PROFILE_SECTION_ENTRY_TYPE_MEMORY_2)
	{
		++totalMemAcc;
	}

	if (profileEntry->type == BINARY_PROFILE_SECTION_ENTRY_TYPE_MEMORY_0 ||
		profileEntry->type == BINARY_PROFILE_SECTION_ENTRY_TYPE_MEMORY_1)
	{
		PBINARY_PROFILE_SECTION_ENTRY_MEM mem = (PBINARY_PROFILE_SECTION_ENTRY_MEM)profileEntry->entry;
		if (mem->memopType == MEM_OP_TYPE_LOAD)
		{
			UINT64 blockCount = mem->size / heatmapGranularity;
			UINT64 blockAddr = mem->vAddr & heatmapGranularityBitMask;
			UINT64 last = (mem->vAddr + mem->size - 1) & heatmapGranularityBitMask;
			UpdateHeapmap(blockAddr);
			while (blockAddr != last)
			{
				blockAddr = blockAddr + heatmapGranularity;
				UpdateHeapmap(blockAddr);
			}
		}
	}
}

void EmitHeapmap()
{
	ofstream myfile;
	myfile.open("heatmap.txt");
	//myfile << std::hex;
	UINT64 currentBase = 0x0;
	const UINT64 countNewLine = 256;
	UINT64 currentCountNewLine = 0;
	const UINT64 maxAddress = 0x100000000 - heatmapGranularity;
	for (map<UINT64, UINT64>::iterator it = heatmap.begin(); it != heatmap.end(); ++it)
	{
		while (currentBase != it->first)
		{
			myfile << 0;
			currentCountNewLine++;
			if (currentCountNewLine == countNewLine)
			{
				currentCountNewLine = 0;
				myfile << "\n";
			}
			else
			{
				myfile << " ,";
			}
			currentBase += heatmapGranularity;
		}
		myfile << it->second;
		currentCountNewLine++;
		if (currentCountNewLine == countNewLine)
		{
			currentCountNewLine = 0;
			myfile << "\n";
		}
		else
		{
			myfile << ", ";
		}
		currentBase += heatmapGranularity;
		//myfile << it->first << ", " << it->second << "\n";
	}

	while (currentBase != maxAddress)
	{
		myfile << 0;
		currentCountNewLine++;
		if (currentCountNewLine == countNewLine)
		{
			currentCountNewLine = 0;
			myfile << "\n";
		}
		else
		{
			myfile << ", ";
		}
		currentBase += heatmapGranularity;
	}
	myfile << 0;

	myfile.close();
}