#include "ImageCollection.h"
#include "Interceptor.h"
#include "LocalState.h"
#include <iostream>
#include "MemoryRegistry.h"

extern LOCAL_STATE lstate;

std::vector<IMG_DESC> IMAGE_COLLECTION::s_images;
std::vector<RTN_DESC> IMAGE_COLLECTION::s_routines;
PIN_LOCK IMAGE_COLLECTION::s_routinesLock;
PIN_LOCK IMAGE_COLLECTION::s_imagesLock;
std::vector<RTN> IMAGE_COLLECTION::s_reprocessRoutines;
PIN_LOCK IMAGE_COLLECTION::s_reprocessLock;

void IMAGE_COLLECTION::Init()
{ 
	PIN_InitLock(&s_routinesLock); 
	PIN_InitLock(&s_imagesLock);
	PIN_InitLock(&s_reprocessLock);
	RTN_DESC dummy;
	dummy.address = 0;
	s_routines.push_back(dummy);
	s_routines.push_back(dummy);
	IMG_DESC idummy;
	idummy.lowAddress = 0;
	s_images.push_back(idummy);
}
void IMAGE_COLLECTION::Dispose(){}

std::vector<IMG_DESC>::const_iterator IMAGE_COLLECTION::CBegin(){ return s_images.begin(); }
std::vector<IMG_DESC>::const_iterator IMAGE_COLLECTION::CEnd(){ return s_images.end(); }

VOID IMAGE_COLLECTION::AddImage(IMG img)
{
	ASSERTX(IMG_Valid(img));
	IMG_DESC iDesc;
	GetCurrentTimestamp(iDesc.loadTime);
	iDesc.entryPointAddress = IMG_Entry(img);
	iDesc.highAddress = IMG_HighAddress(img);
	iDesc.isMainExecutable = IMG_IsMainExecutable(img);
	iDesc.lowAddress = IMG_LowAddress(img);
	iDesc.name = IMG_Name(img);
	iDesc.numOfRegions = IMG_NumRegions(img);
	iDesc.sizeMapped = IMG_SizeMapped(img);
	iDesc.type = IMG_Type(img);
	iDesc.id = IMG_Id(img);
	iDesc.isUnloaded = FALSE;

	for (UINT i = 0; i < IMG_NumRegions(img); ++i)
	{
		iDesc.regions.push_back({ IMG_RegionLowAddress(img, i), IMG_RegionHighAddress(img, i) });
	}

	// Adding sections in order ensures that are already in sorted order according to their addresses.
	for (SEC sec = IMG_SecHead(img); SEC_Valid(sec); sec = SEC_Next(sec))
	{
		SEC_DESC sDesc;
		sDesc.address = SEC_Address(sec);
		sDesc.isExecutable = SEC_IsExecutable(sec);
		sDesc.isMapped = SEC_Mapped(sec);
		sDesc.isReadable = SEC_IsReadable(sec);
		sDesc.isWriteable = SEC_IsWriteable(sec);
		sDesc.name = SEC_Name(sec);
		sDesc.size = SEC_Size(sec);
		sDesc.type = SEC_Type(sec);

		// Adding routines in order ensures that are already in sorted order according to their addresses.
		for (RTN rtn = SEC_RtnHead(sec); RTN_Valid(rtn); rtn = RTN_Next(rtn))
		{
			RTN_DESC rDesc;
			rDesc.address = RTN_Address(rtn);
			rDesc.isArtificial = RTN_IsArtificial(rtn);
			rDesc.isDynamic = RTN_IsDynamic(rtn);
			rDesc.name = RTN_Name(rtn).c_str();

			RTN_Open(rtn);

			// Calling RTN_Open is necessary to use RTN_NumIns. Otherwise, for some reason, 
			// Pin emits the following error when the process is about to terminate:
			// A: Source\pin\pin\image.cpp: LEVEL_PINCLIENT::RTN_Destroy: 1955: Trying to destroy a non empty RTN
			rDesc.numIns = RTN_NumIns(rtn);

			// This requires RTN_Open.
			ProcessRoutine(rtn, FALSE);
			
			RTN_Close(rtn);

			rDesc.range = RTN_Range(rtn);
			rDesc.size = RTN_Size(rtn);
			rDesc.imgId = iDesc.id;
			rDesc.rtnId = RTN_Id(rtn);
			rDesc.secIndex = (UINT32)iDesc.sections.size();

			if (rDesc.rtnId > s_routines.size())
			{
				PIN_GetLock(&s_routinesLock, PIN_ThreadId() + 1);
				while (rDesc.rtnId > s_routines.size())
				{
					// Make sure that this relation holds.
					// We take advantage of it in GetImageAndRoutineNames.
					RTN_DESC dummy;
					dummy.address = 0;
					s_routines.push_back(dummy);
				}
				PIN_ReleaseLock(&s_routinesLock);
			}
			else if (rDesc.rtnId < s_routines.size())
			{
				// Replace the dummy routine with rDesc.
				if (s_routines[rDesc.rtnId].address != 0)
				{
					/*std::cerr << m_routines[rDesc.rtnId].imgId << " == " <<
						rDesc.imgId << ", " << m_routines[rDesc.rtnId].range << " == " <<
						rDesc.range << m_routines[rDesc.rtnId].name << " == " <<
						rDesc.name << "\n";*/
				}
				s_routines[rDesc.rtnId] = rDesc;
			}
			else
			{
				// rDesc.rtnId == m_routines.size()
				// This is the most common case.
				PIN_GetLock(&s_routinesLock, PIN_ThreadId() + 1);
				s_routines.push_back(rDesc);
				PIN_ReleaseLock(&s_routinesLock);
			}
		}

		iDesc.sections.push_back(sDesc);
	}

	PIN_GetLock(&s_imagesLock, PIN_ThreadId() + 1);
	s_images.push_back(iDesc);
	PIN_ReleaseLock(&s_imagesLock);
	
	REGION_USER user;
	if (IsSystemFile(iDesc.name.c_str()))
		user = REGION_USER::Platform;
	else
		user = REGION_USER::App;
	MEMORY_REGISTRY::RegisterImage(iDesc.lowAddress, user);
}

VOID IMAGE_COLLECTION::UnloadImage(IMG img)
{
	ASSERTX(IMG_Valid(img));
	UINT32 id = IMG_Id(img);
	UINT32 index = id;// -1;
	PIN_GetLock(&s_imagesLock, PIN_ThreadId() + 1);
	ASSERTX(s_images[index].id == id);
	GetCurrentTimestamp(s_images[index].unloadTime);
	s_images[index].isUnloaded = TRUE;
	PIN_ReleaseLock(&s_imagesLock);

	MEMORY_REGISTRY::Unregister(s_images[index].lowAddress);
}

VOID IMAGE_COLLECTION::GetImageAndRoutineNames(UINT32 routineId, string &imgName, string &rtnName)
{
	if (routineId > 1)
	{
		PIN_GetLock(&s_routinesLock, PIN_ThreadId() + 1);
		RTN_DESC rDesc = s_routines[routineId];
		PIN_ReleaseLock(&s_routinesLock);
		if (rDesc.address == 0)
		{
			imgName = "UnknownImage";
			rtnName = "UnknownRoutine";
		}
		else
		{
			ASSERTX(rDesc.rtnId == routineId);

			PIN_GetLock(&s_imagesLock, PIN_ThreadId() + 1);
			IMG_DESC iDesc = s_images[rDesc.imgId];
			BOOL isUnloaded = iDesc.isUnloaded;
			PIN_ReleaseLock(&s_imagesLock);

			ASSERTX(iDesc.id == rDesc.imgId);
			imgName = iDesc.name;
			if (!isUnloaded)
				rtnName = rDesc.name;
			else
				rtnName = "UnknownRoutine";
		}
	}
	else
	{
		imgName = "UnknownImage";
		rtnName = "UnknownRoutine";
	}
}

UINT32 IMAGE_COLLECTION::GetRoutineInfo(UINT32 routineId, string &rtnName)
{
	if (routineId > 1)
	{
		PIN_GetLock(&s_routinesLock, PIN_ThreadId() + 1);
		RTN_DESC rDesc = s_routines[routineId];
		PIN_ReleaseLock(&s_routinesLock);
		if (rDesc.address == 0)
		{
			rtnName = "";
			return 0;
		}
		else
		{
			ASSERTX(rDesc.rtnId == routineId);

			PIN_GetLock(&s_imagesLock, PIN_ThreadId() + 1);
			IMG_DESC iDesc = s_images[rDesc.imgId];
			BOOL isUnloaded = iDesc.isUnloaded;
			PIN_ReleaseLock(&s_imagesLock);

			ASSERTX(iDesc.id == rDesc.imgId);
			if (!isUnloaded)
				rtnName = rDesc.name;
			else
				rtnName = "UnknownRoutine";
			return iDesc.id;
		}
	}
	else
	{
		rtnName = "";
		return 0;
	}
}

VOID IMAGE_COLLECTION::GetImageInfo(UINT32 imageId, string &imgName)
{
	if (imageId > 0)
	{
		PIN_GetLock(&s_imagesLock, PIN_ThreadId() + 1);
		IMG_DESC iDesc = s_images[imageId];
		PIN_ReleaseLock(&s_imagesLock);
		imgName = iDesc.name;
	}
	else
	{
		imgName = "";
	}
}

VOID IMAGE_COLLECTION::ProcessRoutine(RTN routine, BOOL isReprocess)
{
	BOOL insertForReprocess = TRUE;
	WINDOW_CONFIG::INTERCEPT_CONFIG *intercepConfig = WINDOW_CONFIG::Match(routine);
	if (intercepConfig != NULL && intercepConfig->valid && (!intercepConfig->intrumented || isReprocess))
	{
		for (UINT32 i = 0; i < intercepConfig->parameters.size(); ++i)
		{
			RTN_InsertCall(routine, IPOINT_BEFORE, (AFUNPTR)AnalyzeInterceptPre,
				IARG_FAST_ANALYSIS_CALL,
				IARG_REG_VALUE, lstate.interceptorMemManReg,
				IARG_PTR, intercepConfig,
				IARG_FUNCARG_ENTRYPOINT_REFERENCE, intercepConfig->parameters[i].index,
				IARG_UINT32, i,
				IARG_END);
		}

		RTN_InsertCall(routine, IPOINT_BEFORE, (AFUNPTR)AnalyzeInterceptPre2,
			IARG_FAST_ANALYSIS_CALL,
			IARG_REG_VALUE, lstate.interceptorMemManReg,
			IARG_RETURN_IP,
			IARG_UINT32, routine.q(),
			IARG_END);

		RTN_InsertCall(routine, IPOINT_AFTER, (AFUNPTR)AnalyzeInterceptPost,
			IARG_FAST_ANALYSIS_CALL,
			IARG_REG_VALUE, lstate.interceptorMemManReg,
			IARG_UINT32, routine.q(),
			IARG_FUNCRET_EXITPOINT_REFERENCE,
			IARG_END);

		intercepConfig->intrumented = TRUE;

		if (insertForReprocess && !isReprocess)
		{
			InsertReprocessRoutine(routine);
			insertForReprocess = FALSE;
		}
	}

	if (WINDOW_CONFIG::IsFuncInteresting(
		PIN_UndecorateSymbolName(RTN_Name(routine), UNDECORATION::UNDECORATION_NAME_ONLY)))
	{
		RTN_InsertCall(routine, IPOINT_BEFORE, (AFUNPTR)AnalyzeInterestingFuncCall,
			IARG_FAST_ANALYSIS_CALL,
			IARG_ADDRINT, RTN_Address(routine),
			IARG_END);

		RTN_InsertCall(routine, IPOINT_AFTER, (AFUNPTR)AnalyzeInterestingFuncRet,
			IARG_FAST_ANALYSIS_CALL,
			IARG_ADDRINT, RTN_Address(routine),
			IARG_END);

		if (insertForReprocess && !isReprocess)
		{
			InsertReprocessRoutine(routine);
			insertForReprocess = FALSE;
		}
	}

	SEC sec = RTN_Sec(routine);
	IMG img = SEC_Img(sec);
	const std::string& imageName = IMG_Name(img);

#ifdef TARGET_WINDOWS
	// In WOW64, ntdll.dll will not be in C:\Windows\System32.
	if (imageName.find("ntdll.dll") == std::string::npos)
	{
		return;
	}

	std::string routineName = RTN_Name(routine);
	if (routineName.size() > 3)
	{
		char first = routineName[0];
		char second = routineName[1];
		char third = routineName[2];
		if ((first == 'N' && second == 't') || (first == 'Z' && second == 'w'))
		{
			string rest = routineName.substr(3);
			switch (third)
			{
			case 'A':
				if (rest == "llocateVirtualMemory")
				{
					RTN_InsertCall(routine, IPOINT_BEFORE, (AFUNPTR)AnalyzeXxAllocateVirtualMemoryPre,
						IARG_FAST_ANALYSIS_CALL,
						IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // _In_    HANDLE  ProcessHandle
						IARG_FUNCARG_ENTRYPOINT_VALUE, 1, // _Inout_ PVOID   *BaseAddress
						// The other arguments are not important.
						IARG_END
						);

					RTN_InsertCall(routine, IPOINT_AFTER, (AFUNPTR)AnalyzeXxAllocateVirtualMemoryPost,
						IARG_FAST_ANALYSIS_CALL,
						IARG_FUNCRET_EXITPOINT_VALUE, // NTSTATUS
						IARG_END
						);
				}
				else
					insertForReprocess = FALSE;
				break;
			case 'F':
				if (rest == "reeVirtualMemory")
				{
					RTN_InsertCall(routine, IPOINT_BEFORE, (AFUNPTR)AnalyzeXxFreeVirtualMemoryPre,
						IARG_FAST_ANALYSIS_CALL,
						IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // _In_    HANDLE  ProcessHandle
						IARG_FUNCARG_ENTRYPOINT_VALUE, 1, // _Inout_ PVOID   *BaseAddress
						IARG_FUNCARG_ENTRYPOINT_VALUE, 3, // _In_    ULONG   FreeType
						// The other arguments are not important.
						IARG_END
						);

					RTN_InsertCall(routine, IPOINT_AFTER, (AFUNPTR)AnalyzeXxFreeVirtualMemoryPost,
						IARG_FAST_ANALYSIS_CALL,
						IARG_FUNCRET_EXITPOINT_VALUE,  // NTSTATUS
						IARG_END
						);
				}
				else
					insertForReprocess = FALSE;
				break;
			default:
				insertForReprocess = FALSE;
				break;
			}
		}
		else
			insertForReprocess = FALSE;
	}
#endif /* TARGET_WINDOWS */

	if (insertForReprocess && !isReprocess)
	{
		InsertReprocessRoutine(routine);
		insertForReprocess = FALSE;
	}
}

VOID IMAGE_COLLECTION::InsertReprocessRoutine(RTN rtn)
{
	PIN_GetLock(&s_reprocessLock, PIN_ThreadId() + 1);
	s_reprocessRoutines.push_back(rtn);
	PIN_ReleaseLock(&s_reprocessLock);
}

VOID IMAGE_COLLECTION::ReprocessRoutines()
{
	PIN_GetLock(&s_reprocessLock, PIN_ThreadId() + 1);
	for (UINT32 i = 0; i < s_reprocessRoutines.size(); ++i)
	{
		RTN_Open(s_reprocessRoutines[i]);
		ProcessRoutine(s_reprocessRoutines[i], TRUE);
		RTN_Close(s_reprocessRoutines[i]);
	}
	PIN_ReleaseLock(&s_reprocessLock);
}