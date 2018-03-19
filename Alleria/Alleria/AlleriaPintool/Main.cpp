/*
-tasks:
-Fix interceptors specified in mulitple threads in config.
-AMD cpuid.
-intercept the following functions:
Zw/NtAllocateUserPhysicalPages
Zw/NtFreeUserPhysicalPages
Zw/NtMapUserPhysicalPages
Zw/NtMapUserPhysicalPagesScatter

TODO:
-improve the LLW accuracy.
-extend the profile file format to support recording call stacks. This enables tools to construct call graphs and flame graphs. Use Pin 3.1 features.
-investigate the accuracy of thread scheduling information and physical addresses.
-profiling networked apps.
-profiling mpi.
-scalable profiling.
-supporting skylake MPX and xeon phi VSCATTER.
-debugging info variables - addresses.
-complete the portable api for linux
-.NET support.
*/

// TODO: replace all \n with NEW_LINE.

#include <iostream>
#include <fstream>
#include <map>
#include <set>
#include <list>
#include <iomanip>
#include <malloc.h>
#include <sstream>


#include "pin.H"
#include "PortableApi.h"

extern "C" {
#pragma DISABLE_COMPILER_WARNING_INT2INT_LOSS
#include "xed-interface.h"
#pragma RESTORE_COMPILER_WARNING_INT2INT_LOSS
}

#include "MemoryRegistry.h"
#include "GlobalState.h"
#include "LocalState.h"
#include "BufferTypes.h"
#include "AppThreadManager.h"
#include "AnalysisCallsTrace.h"
#include "WindowConfig.h"
#include "BinaryProfile.h"
#include "InterceptorMemoryManager.h"

#ifdef TARGET_WINDOWS
#include "WindowsLiveLandsWalker.h"
#endif /* TARGET_WINDOWS */

LOCAL_STATE lstate;

//extern void AlleriaRuntimeSwitchToAllocRegister();

EXCEPT_HANDLING_RESULT GlobalInternalHandler(
	THREADID threadIndex,
	EXCEPTION_INFO * pExceptInfo,
	PHYSICAL_CONTEXT * pPhysCtxt,
	VOID *v)
{
	std::string temp;
	std::string exceptStr = pExceptInfo->ToString();
#ifdef TARGET_WINDOWS
	if (pExceptInfo->IsWindowsSysException())
	{
		UINT32 winErr = pExceptInfo->GetWindowsSysExceptionCode();
		UINT32 argCount = pExceptInfo->CountWindowsSysArguments();
		for (UINT32 i = 0; i < argCount; ++i)
		{
			ADDRINT arg = pExceptInfo->GetWindowsSysArgument(i);
		}
		IntegerToString(winErr, temp);
		exceptStr = exceptStr + ". Error code: " + temp + ". " + GetLastErrorString(winErr);
	}
#endif /* TARGET_WINDOWS */
	IntegerToString(threadIndex, temp);
	exceptStr = "Internal Exception on thread " + temp + ": " + exceptStr;
	ALLERIA_WriteMessageEx(ALLERIA_REPORT_INTERNAL_EXCEPTION, exceptStr.c_str());
	return EHR_UNHANDLED;
}

VOID OnContextChanged(
	THREADID threadIndex,
	CONTEXT_CHANGE_REASON reason,
	const CONTEXT *from,
	CONTEXT *to,
	INT32 info,
	VOID *v)
{
	if (reason == CONTEXT_CHANGE_REASON_FATALSIGNAL ||
		reason == CONTEXT_CHANGE_REASON_SIGNAL ||
		reason == CONTEXT_CHANGE_REASON_EXCEPTION)
	{
		// Unhandled exception.
		std::string temp;
		IntegerToString(threadIndex, temp);
		std::string msg = ContextChangeReasonString(reason);
		msg += " on thread " + temp;
		IntegerToString(info, temp);
		msg += ". Error code: " + temp + GetLastErrorString(info);
		ALLERIA_WriteMessageEx(ALLERIA_REPORT_CONTEXT_CHANGED, msg.c_str());
	}

	// Check to see whether a system call got interrupted.
	// See also AnalyzeSysCallPre and AnalyzeSysCallExit.
	SYS_CALL_RECORD *sysRec = reinterpret_cast<SYS_CALL_RECORD*>(PIN_GetThreadData(lstate.sysCallBufferEntryAddress, threadIndex));
	if (sysRec != NULL)
	{
		sysRec->retVal = 0;
		sysRec->err = 0;
		sysRec->errRetRecorded = FALSE;

		// This is necessary so that AnalyzeSysCallExit understands the situation correctly.
		PIN_SetThreadData(lstate.sysCallBufferEntryAddress, NULL, threadIndex);
	}
}

UINT32 GetPhysicalAddresses(
	VOID *bufferStart,
	VOID *bufferEnd,
	UINT64 **physicalAddresses)
{
	VOID *buff = reinterpret_cast<VOID*>(bufferStart);
	std::vector<ADDRINT> virtualAddresses;

	while (buff < reinterpret_cast<VOID*>(bufferEnd))
	{
		BUFFER_ENTRY_TYPE *type = reinterpret_cast<BUFFER_ENTRY_TYPE*>(buff);
		while (*type == BUFFER_ENTRY_TYPE_INVALID && type < reinterpret_cast<BUFFER_ENTRY_TYPE*>(bufferEnd))
		{
			++type;
		}
		buff = type;
		if (buff >= reinterpret_cast<BUFFER_ENTRY_TYPE*>(bufferEnd))
			break;

		switch (*type)
		{
		case BUFFER_ENTRY_TYPE_IR0:
		{
			INS_RECORD_0 *insrec = reinterpret_cast<INS_RECORD_0*>(buff);
			virtualAddresses.push_back(insrec->insAddr);
			buff = reinterpret_cast<UINT8*>(insrec)+sizeof(INS_RECORD_0);
			break;
		}
		case BUFFER_ENTRY_TYPE_IR1:
		{
			INS_RECORD_1 *insrec = reinterpret_cast<INS_RECORD_1*>(buff);
			virtualAddresses.push_back(insrec->insAddr);
			buff = reinterpret_cast<UINT8*>(insrec)+sizeof(INS_RECORD_1);
			break;
		}
		case BUFFER_ENTRY_TYPE_IR2:
		{
			INS_RECORD_2 *insrec = reinterpret_cast<INS_RECORD_2*>(buff);
			virtualAddresses.push_back(insrec->insAddr);
			buff = reinterpret_cast<UINT8*>(insrec)+sizeof(INS_RECORD_2);
			break;
		}
		case BUFFER_ENTRY_TYPE_MR0:
		{
			MEM_REF_0 *memRefObj = reinterpret_cast<MEM_REF_0*>(buff);
			virtualAddresses.push_back(memRefObj->vea);
			buff = reinterpret_cast<UINT8*>(memRefObj)+sizeof(MEM_REF_0);
			break;
		}
		case BUFFER_ENTRY_TYPE_MR1:
		{
			MEM_REF_1 *memRefObj = reinterpret_cast<MEM_REF_1*>(buff);
			virtualAddresses.push_back(memRefObj->vea);
			buff = reinterpret_cast<UINT8*>(memRefObj)+sizeof(MEM_REF_1);
			break;
		}
		case BUFFER_ENTRY_TYPE_MR2:
		{
			MEM_REF_2 *memRefObj = reinterpret_cast<MEM_REF_2*>(buff);
			buff = reinterpret_cast<UINT8*>(memRefObj)+sizeof(MEM_REF_2);
			break;
		}
		case BUFFER_ENTRY_TYPE_VGATHER_0:
		{
			MEM_REF_0 *memRefObj = reinterpret_cast<MEM_REF_0*>(buff);
			virtualAddresses.push_back(memRefObj->vea);
			ALLERIA_MULTI_MEM_ACCESS_INFO *accesses = reinterpret_cast<ALLERIA_MULTI_MEM_ACCESS_INFO*>(memRefObj->value);
			for (UINT32 i = 0; i < accesses->numberOfMemops; ++i)
				if (accesses->memop[i].maskOn)
					virtualAddresses.push_back(accesses->memop[i].memoryAddress);
			buff = reinterpret_cast<UINT8*>(memRefObj)+sizeof(MEM_REF_0);
			break;
		}
		case BUFFER_ENTRY_TYPE_VGATHER_1:
		{
			MEM_REF_1 *memRefObj = reinterpret_cast<MEM_REF_1*>(buff);
			ALLERIA_MULTI_MEM_ACCESS_ADDRS *accesses = reinterpret_cast<ALLERIA_MULTI_MEM_ACCESS_ADDRS*>(memRefObj->vea);
			virtualAddresses.push_back(accesses->base);
			for (UINT32 i = 0; i < accesses->numberOfMemops; ++i)
				if (accesses->maskOn[i])
					virtualAddresses.push_back(accesses->addrs[i]);
			buff = reinterpret_cast<UINT8*>(memRefObj)+sizeof(MEM_REF_1);
			break;
		}
		case BUFFER_ENTRY_TYPE_XSAVE_0:
		{
			MEM_REF_0 *memRefObj = reinterpret_cast<MEM_REF_0*>(buff);
			virtualAddresses.push_back(memRefObj->vea);
			std::vector<MEM_ACCESS_GENERAL_0> *accesses = reinterpret_cast<std::vector<MEM_ACCESS_GENERAL_0>*>(memRefObj->value);
			for (UINT32 i = 0; i < accesses->size(); ++i)
				virtualAddresses.push_back((*accesses)[i].addr);
			buff = reinterpret_cast<UINT8*>(memRefObj)+sizeof(MEM_REF_0);
			break;
		}
		case BUFFER_ENTRY_TYPE_XSAVE_1:
		{
			MEM_REF_1 *memRefObj = reinterpret_cast<MEM_REF_1*>(buff);
			ALLERIA_XSAVE_MEM_ACCESS_ADDRS *accesses = reinterpret_cast<ALLERIA_XSAVE_MEM_ACCESS_ADDRS*>(memRefObj->vea);
			virtualAddresses.push_back(accesses->base);
			for (UINT32 i = 0; i < accesses->accesses.size(); ++i)
				virtualAddresses.push_back(accesses->accesses[i].addr);
			buff = reinterpret_cast<UINT8*>(memRefObj)+sizeof(MEM_REF_0);
			break;
		}
		case BUFFER_ENTRY_TYPE_BRANCH_RECORD:
		{
			BRANCH_RECORD *br = reinterpret_cast<BRANCH_RECORD*>(buff);
			virtualAddresses.push_back(br->target);
			buff = reinterpret_cast<UINT8*>(br)+sizeof(BRANCH_RECORD);
			break;
		}
		case BUFFER_ENTRY_TYPE_PROCESSOR_RECORD:
		{
			PROCESSOR_RECORD *pr = reinterpret_cast<PROCESSOR_RECORD*>(buff);
			buff = reinterpret_cast<UINT8*>(pr)+sizeof(PROCESSOR_RECORD);
			break;
		}
		case BUFFER_ENTRY_TYPE_SYS_CALL_RECORD:
		{
			SYS_CALL_RECORD *scr = reinterpret_cast<SYS_CALL_RECORD*>(buff);
			buff = reinterpret_cast<UINT8*>(scr)+sizeof(SYS_CALL_RECORD);
			break;
		}
		default:
			ASSERT(FALSE, "Buffer corrupted.");
		}
	}

	UINT32 addrsConverted;

	if (virtualAddresses.size() > 0)
	{
		*physicalAddresses = new UINT64[virtualAddresses.size()];
		addrsConverted = Virtual2Physical(
			reinterpret_cast<void**>(&virtualAddresses[0]), *physicalAddresses, (UINT32)virtualAddresses.size());

		if (addrsConverted != virtualAddresses.size())
		{
			ALLERIA_WriteMessage(ALLERIA_REPORT_V2P);
		}
	}

	return (UINT32)virtualAddresses.size();
}

VOID ProcessBuffer(
	const BUFFER_STATE& bufferState,
	std::ofstream &fstream,
	UINT64 *readsCount,
	UINT64 *writesCount,
	UINT64 *insCount,
	UINT64 *memInsCount,
	UINT32& unusedBytes)
{
	VOID *buff = reinterpret_cast<VOID*>(bufferState.m_bufferStart);
	string prevImgName;
	string prevRtnName;
	UINT64 *physicalAddresses = NULL;
	UINT64 *physicalAddressesIterator = NULL;
	UINT64 paddr = PHYSICAL_ADDRESS_INVALID;
	ADDRINT baseAddr = NULL;
	UINT32 readCount = 0;
	UINT32 writeCount = 0;
	BOOL isMem = FALSE;
	BOOL isIns = FALSE;
	UINT8 insBytes[XED_MAX_INSTRUCTION_BYTES];
	xed_error_enum_t xedErr;
	std::stringstream ss;
	xed_decoded_inst_t xedd;
	char insStr[1024] = { '\0' };
	xed_bool_t xedSuccess = 0;
	const string insInfoSpace = "          ";
	UINT32 physicalAddressesCount;

	unusedBytes = 0;
	fstream << "------------------------------------------------\n";
	fstream << "OS Thread " << bufferState.m_appThreadManager->GetOSThreadId() << "\n";
	ss << std::hex;

	if (g_v2pTransMode != V2P_TRANS_MODE_OFF)
	{
		physicalAddressesCount = GetPhysicalAddresses(bufferState.m_bufferStart, bufferState.m_bufferEnd, &physicalAddresses);
		physicalAddressesIterator = physicalAddresses;
	}

	while (buff < reinterpret_cast<VOID*>(bufferState.m_bufferEnd))
	{
		BUFFER_ENTRY_TYPE *type = reinterpret_cast<BUFFER_ENTRY_TYPE*>(buff);
		while (*type == BUFFER_ENTRY_TYPE_INVALID && type < reinterpret_cast<BUFFER_ENTRY_TYPE*>(bufferState.m_bufferEnd))
		{
			++type;
		}
		unusedBytes += (UINT32)(reinterpret_cast<ADDRINT>(type)-reinterpret_cast<ADDRINT>(buff));
		buff = type;
		if (buff >= reinterpret_cast<BUFFER_ENTRY_TYPE*>(bufferState.m_bufferEnd))
			break;

		if (*type == BUFFER_ENTRY_TYPE_PROCESSOR_RECORD)
		{
			fstream << "Package " << reinterpret_cast<PROCESSOR_RECORD*>(type)->processorPackage << "\n";
			fstream << "HW Thread " << reinterpret_cast<PROCESSOR_RECORD*>(type)->processorNumber << "\n";
			buff = reinterpret_cast<UINT8*>(buff)+sizeof(PROCESSOR_RECORD);
			continue;
		}

		if (g_v2pTransMode != V2P_TRANS_MODE_OFF)
		{
			paddr = *physicalAddressesIterator;
			// Checks for entries that don't consume physical addresses.
			if (*type != BUFFER_ENTRY_TYPE_MR2 && *type != BUFFER_ENTRY_TYPE_SYS_CALL_RECORD)
				++physicalAddressesIterator;
		}

		baseAddr = NULL;
		readCount = 0;
		writeCount = 0;
		isMem = FALSE;
		isIns = FALSE;
		xedSuccess = 0;
		ss.str(std::string());

		switch (*type)
		{
		case BUFFER_ENTRY_TYPE_IR0:
		{
			INS_RECORD_0 *insRecordObj = reinterpret_cast<INS_RECORD_0*>(buff);
			size_t bytesCopied = PIN_SafeCopy(insBytes, reinterpret_cast<VOID*>(insRecordObj->insAddr), XED_MAX_INSTRUCTION_BYTES);
			xedErr = bytesCopied == 0 ? XED_ERROR_GENERAL_ERROR : InsDecode(insBytes, xedd, bytesCopied);
			if (xedErr == XED_ERROR_NONE)
			{
				xed_uint64_t runtime_address = reinterpret_cast<xed_uint64_t>(insBytes);
				xedSuccess = xed_format_context(GLOBAL_STATE::GetAsmSyntaxStyle(), &xedd,
					insStr, 2048, runtime_address, 0, 0);
			}

			string imgName;
			string rtnName;
			IMAGE_COLLECTION::GetImageAndRoutineNames(insRecordObj->routineId, imgName, rtnName);
			if (imgName != prevImgName || rtnName != prevRtnName)
			{
				ss << imgName << ":" << rtnName << "\n";
				prevImgName = imgName;
				prevRtnName = rtnName;
			}

			ss << insRecordObj->insAddr;
			if (g_v2pTransMode != V2P_TRANS_MODE_OFF)
				ss << "/" << paddr;
			ss << "   " << std::left << std::setw(75) << (xedSuccess ? insStr : "UNKOWN");
			TIME_STAMP ts;
			GetTimeSpan(insRecordObj->timestamp, TIME_UNIT::TenthMicroseconds, ts);
			ss << ts << "\n";
			fstream << ss.str();

			baseAddr = insRecordObj->insAddr;
			isIns = TRUE;
			buff = reinterpret_cast<UINT8*>(insRecordObj)+sizeof(INS_RECORD_0);
			break;
		}
		case BUFFER_ENTRY_TYPE_IR1:
		{
			INS_RECORD_1 *insRecordObj = reinterpret_cast<INS_RECORD_1*>(buff);
			size_t bytesCopied = PIN_SafeCopy(insBytes, reinterpret_cast<VOID*>(insRecordObj->insAddr), XED_MAX_INSTRUCTION_BYTES);
			xedErr = bytesCopied == 0 ? XED_ERROR_GENERAL_ERROR : InsDecode(insBytes, xedd, bytesCopied);
			if (xedErr == XED_ERROR_NONE)
			{
				xed_uint64_t runtime_address = reinterpret_cast<xed_uint64_t>(insBytes);
				xedSuccess = xed_format_context(GLOBAL_STATE::GetAsmSyntaxStyle(), &xedd,
					insStr, 2048, runtime_address, 0, 0);
			}

			string imgName;
			string rtnName;
			IMAGE_COLLECTION::GetImageAndRoutineNames(insRecordObj->routineId, imgName, rtnName);

			if (imgName != prevImgName || rtnName != prevRtnName)
			{
				ss << imgName << ":" << rtnName << "\n";
				prevImgName = imgName;
				prevRtnName = rtnName;
			}

			ss << insRecordObj->insAddr;
			if (g_v2pTransMode != V2P_TRANS_MODE_OFF)
				ss << "/" << paddr;
			ss << "   " << (xedSuccess ? insStr : "UNKOWN") << "\n";
			fstream << ss.str();

			baseAddr = insRecordObj->insAddr;
			isIns = TRUE;
			buff = reinterpret_cast<UINT8*>(insRecordObj)+sizeof(INS_RECORD_1);
			break;
		}
		case BUFFER_ENTRY_TYPE_IR2:
		{
			INS_RECORD_2 *insRecordObj = reinterpret_cast<INS_RECORD_2*>(buff);
			size_t bytesCopied = PIN_SafeCopy(insBytes, reinterpret_cast<VOID*>(insRecordObj->insAddr), XED_MAX_INSTRUCTION_BYTES);
			xedErr = bytesCopied == 0 ? XED_ERROR_GENERAL_ERROR : InsDecode(insBytes, xedd, bytesCopied);
			if (xedErr == XED_ERROR_NONE)
			{
				xed_uint64_t runtime_address = reinterpret_cast<xed_uint64_t>(insBytes);
				xedSuccess = xed_format_context(GLOBAL_STATE::GetAsmSyntaxStyle(), &xedd,
					insStr, 2048, runtime_address, 0, 0);
			}

			ss << insRecordObj->insAddr;
			if (g_v2pTransMode != V2P_TRANS_MODE_OFF)
				ss << "/" << paddr;
			ss << "   " << (xedSuccess ? insStr : "UNKOWN") << "\n";
			fstream << ss.str();

			baseAddr = insRecordObj->insAddr;
			isIns = TRUE;
			buff = reinterpret_cast<UINT8*>(insRecordObj)+sizeof(INS_RECORD_2);
			break;
		}
		case BUFFER_ENTRY_TYPE_MR0:
		{
			MEM_REF_0 *memRefObj = reinterpret_cast<MEM_REF_0*>(buff);
			ss << insInfoSpace;
			std::string value = "";
			if (memRefObj->isValueValid)
			{
				if (memRefObj->size > 8)
				{
					value = ShowN(memRefObj->size, reinterpret_cast<VOID*>(memRefObj->value));
					delete[] reinterpret_cast<VOID*>(memRefObj->value);
				}
				else
				{
					value = ShowN(memRefObj->size, reinterpret_cast<VOID*>(&(memRefObj->value)));
				}
			}

			if (memRefObj->isRead)
			{
				ss << value << " = *" << memRefObj->vea;
				if (g_v2pTransMode != V2P_TRANS_MODE_OFF)
					ss << "/" << paddr;
				ss << " R " << memRefObj->size << " bytes\n";
			}
			else
			{
				ss << "*" << memRefObj->vea;
				if (g_v2pTransMode != V2P_TRANS_MODE_OFF)
					ss << "/" << paddr;
				ss << " = " << value << " W " << memRefObj->size << " bytes\n";
			}

			fstream << ss.str();

			if (memRefObj->isRead)
				++readCount;
			else
				++writeCount;
			baseAddr = memRefObj->vea;
			isMem = TRUE;
			buff = reinterpret_cast<UINT8*>(memRefObj)+sizeof(MEM_REF_0);
			break;
		}
		case BUFFER_ENTRY_TYPE_MR1:
		{
			MEM_REF_1 *memRefObj = reinterpret_cast<MEM_REF_1*>(buff);
			ss << insInfoSpace;

			ss << memRefObj->vea;
			if (g_v2pTransMode != V2P_TRANS_MODE_OFF)
				ss << "/" << paddr;

			if (memRefObj->isRead)
			{
				ss << " R " << memRefObj->size << " bytes\n";
			}
			else
			{
				ss << " W " << memRefObj->size << " bytes\n";
			}

			fstream << ss.str();

			if (memRefObj->isRead)
				++readCount;
			else
				++writeCount;
			baseAddr = memRefObj->vea;
			isMem = TRUE;
			buff = reinterpret_cast<UINT8*>(memRefObj)+sizeof(MEM_REF_1);
			break;
		}
		case BUFFER_ENTRY_TYPE_MR2:
		{
			MEM_REF_2 *memRefObj = reinterpret_cast<MEM_REF_2*>(buff);
			ss << insInfoSpace;

			if (memRefObj->isRead)
			{
				ss << " R\n";
			}
			else
			{
				ss << " W\n";
			}

			fstream << ss.str();

			if (memRefObj->isRead)
				++readCount;
			else
				++writeCount;
			isMem = TRUE;
			buff = reinterpret_cast<UINT8*>(memRefObj)+sizeof(MEM_REF_2);
			break;
		}
		case BUFFER_ENTRY_TYPE_VGATHER_0:
		{
			MEM_REF_0 *memRefObj = reinterpret_cast<MEM_REF_0*>(buff);
			std::string value = MultiMemAccessInfoStringAndDispose(
				reinterpret_cast<ALLERIA_MULTI_MEM_ACCESS_INFO*>(memRefObj->value), &physicalAddressesIterator);

			ss << insInfoSpace << "Base: ";
			ss << memRefObj->vea;
			if (g_v2pTransMode != V2P_TRANS_MODE_OFF)
				ss << "/" << paddr;
			ss << "\n" << insInfoSpace << value << "\n";

			fstream << ss.str();

			if (memRefObj->isRead)
				++readCount;
			else
				++writeCount;
			baseAddr = memRefObj->vea;
			isMem = TRUE;
			buff = reinterpret_cast<UINT8*>(memRefObj)+sizeof(MEM_REF_0);
			break;
		}
		case BUFFER_ENTRY_TYPE_VGATHER_1:
		{
			MEM_REF_1 *memRefObj = reinterpret_cast<MEM_REF_1*>(buff);
			baseAddr = reinterpret_cast<ALLERIA_MULTI_MEM_ACCESS_ADDRS*>(memRefObj->vea)->base;
			std::string value = MultiMemAccessAddrsStringAndDispose(
				reinterpret_cast<ALLERIA_MULTI_MEM_ACCESS_ADDRS*>(memRefObj->vea), &physicalAddressesIterator);

			ss << insInfoSpace << "Base: ";
			ss << baseAddr;
			if (g_v2pTransMode != V2P_TRANS_MODE_OFF)
				ss << "/" << paddr;
			ss << "\n" << insInfoSpace << value << "\n";

			fstream << ss.str();

			if (memRefObj->isRead)
				++readCount;
			else
				++writeCount;
			isMem = TRUE;
			buff = reinterpret_cast<UINT8*>(memRefObj)+sizeof(MEM_REF_1);
			break;
		}
		case BUFFER_ENTRY_TYPE_XSAVE_0:
		{
			MEM_REF_0 *memRefObj = reinterpret_cast<MEM_REF_0*>(buff);
			std::string value = MultiMemXsaveInfoStringAndDispose(
				reinterpret_cast<std::vector<MEM_ACCESS_GENERAL_0>*>(memRefObj->value), &physicalAddressesIterator);

			ss << insInfoSpace << "Base: ";
			ss << memRefObj->vea;
			if (g_v2pTransMode != V2P_TRANS_MODE_OFF)
				ss << "/" << paddr;
			ss << "\n" << insInfoSpace << value << "\n";

			fstream << ss.str();

			if (memRefObj->isRead)
				++readCount;
			else
				++writeCount;
			baseAddr = memRefObj->vea;
			isMem = TRUE;
			buff = reinterpret_cast<UINT8*>(memRefObj)+sizeof(MEM_REF_0);
			break;
		}
		case BUFFER_ENTRY_TYPE_XSAVE_1:
		{
			MEM_REF_1 *memRefObj = reinterpret_cast<MEM_REF_1*>(buff);
			baseAddr = reinterpret_cast<ALLERIA_XSAVE_MEM_ACCESS_ADDRS*>(memRefObj->vea)->base;
			std::string value = MultiMemXsaveAddrsStringAndDispose(
				reinterpret_cast<ALLERIA_XSAVE_MEM_ACCESS_ADDRS*>(memRefObj->vea), &physicalAddressesIterator);

			ss << insInfoSpace << "Base: ";
			ss << baseAddr;
			if (g_v2pTransMode != V2P_TRANS_MODE_OFF)
				ss << "/" << paddr;
			ss << "\n" << insInfoSpace << value << "\n";

			fstream << ss.str();

			if (memRefObj->isRead)
				++readCount;
			else
				++writeCount;
			isMem = TRUE;
			buff = reinterpret_cast<UINT8*>(memRefObj)+sizeof(MEM_REF_1);
			break;
		}
		case BUFFER_ENTRY_TYPE_BRANCH_RECORD:
		{
			BRANCH_RECORD *br = reinterpret_cast<BRANCH_RECORD*>(buff);
			ss << insInfoSpace << "Target: ";
			ss << br->target;
			if (g_v2pTransMode != V2P_TRANS_MODE_OFF)
				ss << "/" << paddr;

			if (br->taken)
				ss << " taken\n";
			else
				ss << " not taken\n";

			fstream << ss.str();

			buff = reinterpret_cast<UINT8*>(buff)+sizeof(BRANCH_RECORD);
			break;
		}
		case BUFFER_ENTRY_TYPE_SYS_CALL_RECORD:
		{
			SYS_CALL_RECORD *sysRec = reinterpret_cast<SYS_CALL_RECORD*>(buff);
			ss << insInfoSpace << "Number       = " << sysRec->num << "\n";
			ss << insInfoSpace << "Arg 0        = " << sysRec->arg0 << "\n";
			ss << insInfoSpace << "Arg 1        = " << sysRec->arg1 << "\n";
			ss << insInfoSpace << "Arg 2        = " << sysRec->arg2 << "\n";
			ss << insInfoSpace << "Arg 3        = " << sysRec->arg3 << "\n";
			ss << insInfoSpace << "Arg 4        = " << sysRec->arg4 << "\n";
			ss << insInfoSpace << "Arg 5        = " << sysRec->arg5 << "\n";
			ss << insInfoSpace << "Arg 6        = " << sysRec->arg6 << "\n";
			ss << insInfoSpace << "Arg 7        = " << sysRec->arg7 << "\n";
			ss << insInfoSpace << "Arg 8        = " << sysRec->arg8 << "\n";
			ss << insInfoSpace << "Arg 9        = " << sysRec->arg9 << "\n";
			ss << insInfoSpace << "Arg 10       = " << sysRec->arg10 << "\n";
			ss << insInfoSpace << "Arg 11       = " << sysRec->arg11 << "\n";
			ss << insInfoSpace << "Arg 12       = " << sysRec->arg12 << "\n";
			ss << insInfoSpace << "Arg 13       = " << sysRec->arg13 << "\n";
			ss << insInfoSpace << "Arg 14       = " << sysRec->arg14 << "\n";
			ss << insInfoSpace << "Arg 15       = " << sysRec->arg15 << "\n";
			if (sysRec->errRetRecorded)
			{
				ss << insInfoSpace << "Return Value = " << sysRec->retVal << "\n";
				ss << insInfoSpace << "Error        = " << sysRec->err << "\n";
			}

			fstream << ss.str();

			buff = reinterpret_cast<UINT8*>(buff)+sizeof(SYS_CALL_RECORD);
			break;
		}
		default:
			ASSERT(FALSE, "Buffer corrupted.");
			break;
		}

		if (isMem)
		{
			*readsCount += readCount;
			*writesCount += writeCount;
			++(*memInsCount);
		}

		if (isIns)
		{
			++(*insCount);
		}

		if (g_v2pTransMode == V2P_TRANS_MODE_ACCURATE && baseAddr)
		{
			UnpinVirtualPage(reinterpret_cast<void*>(baseAddr));
		}
	}

	if (g_v2pTransMode != V2P_TRANS_MODE_OFF)
	{
		ASSERTX(physicalAddresses + physicalAddressesCount == physicalAddressesIterator);
		delete[] physicalAddresses;
	}

	fstream << "------------------------------------------------\n";

	// Flush the buffer here to make sure that the process doesn't terminate without flushing everything.
	fstream.flush();

	// Mark all slots vacant.
	size_t bufferSize = (ADDRINT)bufferState.m_bufferEnd - (ADDRINT)bufferState.m_bufferStart;
	std::memset(bufferState.m_bufferStart, 0, bufferSize);
}

xed_error_enum_t WriteBinaryInstruction(std::ofstream& ofs, ADDRINT insAddr, UINT64 pAddr)
{
	xed_decoded_inst_t xedd;
	UINT8 insBytes[XED_MAX_INSTRUCTION_BYTES];
	size_t bytesCopied = PIN_SafeCopy(insBytes, reinterpret_cast<VOID*>(insAddr), XED_MAX_INSTRUCTION_BYTES);
	xed_error_enum_t xedErr = bytesCopied == 0 ? XED_ERROR_GENERAL_ERROR : InsDecode(insBytes, xedd, bytesCopied);
	ofs.write(reinterpret_cast<CHAR*>(&insAddr), g_virtualAddressSize);
	ofs.write(reinterpret_cast<CHAR*>(&pAddr), g_physicalAddressSize);
	if (KnobOuputInstructionEnabled)
	{
		if (xedErr == XED_ERROR_NONE)
		{
			xed_uint_t length = xed_decoded_inst_get_length(&xedd);
			ofs.write(reinterpret_cast<CHAR*>(insBytes), length);
		}
		else
		{
			OPCODE opcode = LEVEL_BASE::OPCODE_INVALID();
			ofs.write(reinterpret_cast<CHAR*>(&opcode), sizeof(opcode));
		}
	}
	return xedErr;
}

//UINT64 PinTuidToId(PIN_THREAD_UID tuid)
//{
//	UINT64 temp = tuid >> 32;
//	if (temp == 1) return temp;
//	else return temp - GLOBAL_STATE::processingThreadsIds.size() - 1;
//}

VOID ProcessBufferBinary(
	const BUFFER_STATE& bufferState,
	BOOL isAppThread,
	PIN_THREAD_UID tuid,
	UINT64 *readsCount,
	UINT64 *writesCount,
	UINT64 *insCount,
	UINT64 *memInsCount,
	UINT32& unusedBytes)
{
	unusedBytes = 0;
	if (bufferState.m_bufferStart >= bufferState.m_bufferEnd)
		return;

	VOID *buff = reinterpret_cast<VOID*>(bufferState.m_bufferStart);
	string prevImgName;
	string prevRtnName;
	OS_THREAD_ID ostid = bufferState.m_appThreadManager->GetOSThreadId();
	UINT64 tn = WINDOW_CONFIG::GetAppSerialId(tuid);

	std::map<UINT32, UINT32> *routineMap;
	std::ofstream *ofs;
	if (isAppThread)
	{
		routineMap = &GLOBAL_STATE::gRoutineMap;
		ofs = &GLOBAL_STATE::gfstream;
	}
	else
	{
		routineMap = reinterpret_cast<std::map<UINT32, UINT32>*>
			(PIN_GetThreadData(lstate.routineMapKey, PIN_ThreadId()));
		ofs = reinterpret_cast<std::ofstream*>
			(PIN_GetThreadData(lstate.ofstreamKey, PIN_ThreadId()));
		if (ofs == NULL)
			return; // This processing thread didn't process any buffers.
	}

	BUFFER_ENTRY_TYPE bst = BUFFER_ENTRY_TYPE_HEADER;
	WriteBinary((*ofs), bst);
	WriteBinary((*ofs), ostid);
	WriteBinary((*ofs), tn); // App thread number.

	if (isAppThread) // Func section directory entry index.
	{
		WriteBinary((*ofs), g_binSecDirFuncIndexMain);
	}
	else
	{
		WriteBinary((*ofs), g_binSecDirFuncIndexSub);
	}

	UINT64 *physicalAddresses = NULL;
	UINT64 *physicalAddressesIterator = NULL;
	UINT64 paddr = PHYSICAL_ADDRESS_INVALID;
	ADDRINT baseAddr = NULL;
	UINT32 readCount = 0;
	UINT32 writeCount = 0;
	BOOL isMem = FALSE;
	BOOL isIns = FALSE;
	UINT32 physicalAddressesCount;

	if (g_v2pTransMode != V2P_TRANS_MODE_OFF)
	{
		physicalAddressesCount = GetPhysicalAddresses(bufferState.m_bufferStart, bufferState.m_bufferEnd, &physicalAddresses);
		physicalAddressesIterator = physicalAddresses;
	}

	while (buff < reinterpret_cast<VOID*>(bufferState.m_bufferEnd))
	{
		BUFFER_ENTRY_TYPE *type = reinterpret_cast<BUFFER_ENTRY_TYPE*>(buff);
		while (*type == BUFFER_ENTRY_TYPE_INVALID && type < reinterpret_cast<BUFFER_ENTRY_TYPE*>(bufferState.m_bufferEnd))
		{
			++type;
		}
		unusedBytes += (UINT32)(reinterpret_cast<ADDRINT>(type)-reinterpret_cast<ADDRINT>(buff));
		buff = type;
		if (buff >= reinterpret_cast<BUFFER_ENTRY_TYPE*>(bufferState.m_bufferEnd))
			break;

		if (*type == BUFFER_ENTRY_TYPE_PROCESSOR_RECORD)
		{
			WriteBinary((*ofs), reinterpret_cast<PROCESSOR_RECORD*>(type)->type);
			WriteBinary((*ofs), reinterpret_cast<PROCESSOR_RECORD*>(type)->processorPackage);
			WriteBinary((*ofs), reinterpret_cast<PROCESSOR_RECORD*>(type)->processorNumber);
			buff = reinterpret_cast<UINT8*>(buff)+sizeof(PROCESSOR_RECORD);
			continue;
		}

		if (g_v2pTransMode != V2P_TRANS_MODE_OFF && physicalAddresses)
		{
			paddr = *physicalAddressesIterator;
			// Checks for entries that don't consume physical addresses.
			if (*type != BUFFER_ENTRY_TYPE_MR2 && *type != BUFFER_ENTRY_TYPE_SYS_CALL_RECORD)
				++physicalAddressesIterator;
		}

		baseAddr = NULL;
		readCount = 0;
		writeCount = 0;
		isMem = FALSE;
		isIns = FALSE;

		switch (*type)
		{
		case BUFFER_ENTRY_TYPE_IR0:
		{
			INS_RECORD_0 *insRecordObj = reinterpret_cast<INS_RECORD_0*>(buff);
			WriteBinary((*ofs), insRecordObj->type);
			WriteBinaryInstruction(*ofs, insRecordObj->insAddr, paddr);

			// Emit routine index according to the function names table section.
			if (insRecordObj->routineId == RTN_Invalid().q())
				WriteBinaryFuncTableIndex((*ofs), 0); // Unknown routine.
			else
			{
				UINT32 funcTableIndex;
				std::map<UINT32, UINT32>::const_iterator it = routineMap->find(insRecordObj->routineId);
				if (it == routineMap->end())
				{
					funcTableIndex = (UINT32)routineMap->size() + 1;
					routineMap->insert(std::pair<UINT32, UINT32>(insRecordObj->routineId, funcTableIndex));
				}
				else
				{
					funcTableIndex = it->second;
				}
				WriteBinaryFuncTableIndex((*ofs), funcTableIndex);
			}

			WriteBinary((*ofs), insRecordObj->timestamp);
			buff = reinterpret_cast<UINT8*>(buff)+sizeof(INS_RECORD_0);
			baseAddr = insRecordObj->insAddr;
			isIns = TRUE;
			break;
		}
		case BUFFER_ENTRY_TYPE_IR1:
		{
			INS_RECORD_1 *insRecordObj = reinterpret_cast<INS_RECORD_1*>(buff);
			WriteBinary((*ofs), insRecordObj->type);
			WriteBinaryInstruction(*ofs, insRecordObj->insAddr, paddr);

			// Emit routine index according to the function names table section.
			if (insRecordObj->routineId == RTN_Invalid().q())
				WriteBinaryFuncTableIndex((*ofs), 0); // Unknown routine.
			else
			{
				UINT32 funcTableIndex;
				std::map<UINT32, UINT32>::const_iterator it = routineMap->find(insRecordObj->routineId);
				if (it == routineMap->end())
				{
					funcTableIndex = (UINT32)routineMap->size() + 1;
					routineMap->insert(std::pair<UINT32, UINT32>(insRecordObj->routineId, funcTableIndex));
				}
				else
				{
					funcTableIndex = it->second;
				}
				WriteBinaryFuncTableIndex((*ofs), funcTableIndex);
			}

			buff = reinterpret_cast<UINT8*>(buff)+sizeof(INS_RECORD_1);
			baseAddr = insRecordObj->insAddr;
			isIns = TRUE;
			break;
		}
		case BUFFER_ENTRY_TYPE_IR2:
		{
			INS_RECORD_2 *insRecordObj = reinterpret_cast<INS_RECORD_2*>(buff);
			WriteBinary((*ofs), insRecordObj->type);
			WriteBinaryInstruction(*ofs, insRecordObj->insAddr, paddr);
			buff = reinterpret_cast<UINT8*>(buff)+sizeof(INS_RECORD_2);
			baseAddr = insRecordObj->insAddr;
			isIns = TRUE;
			break;
		}
		case BUFFER_ENTRY_TYPE_MR0:
		{
			MEM_REF_0 *memRefObj = reinterpret_cast<MEM_REF_0*>(buff);
			WriteBinary((*ofs), memRefObj->type);
			WriteBinary((*ofs), memRefObj->isRead);
			(*ofs).write(reinterpret_cast<CHAR*>(&memRefObj->vea), g_virtualAddressSize);
			(*ofs).write(reinterpret_cast<CHAR*>(&paddr), g_physicalAddressSize);
			WriteBinary((*ofs), memRefObj->size);
			WriteBinary((*ofs), memRefObj->isValueValid);
			if (memRefObj->isValueValid)
			{
				if (memRefObj->size > 8)
				{
					(*ofs).write(reinterpret_cast<char*>(memRefObj->value), memRefObj->size);
					delete[] reinterpret_cast<VOID*>(memRefObj->value);
				}
				else
				{
					(*ofs).write(reinterpret_cast<char*>(&memRefObj->value), memRefObj->size);
				}
			}
			baseAddr = memRefObj->vea;
			if (memRefObj->isRead)
				++readCount;
			else
				++writeCount;
			buff = reinterpret_cast<UINT8*>(buff)+sizeof(MEM_REF_0);
			isMem = TRUE;
			break;
		}
		case BUFFER_ENTRY_TYPE_MR1:
		{
			MEM_REF_1 *memRefObj = reinterpret_cast<MEM_REF_1*>(buff);
			WriteBinary((*ofs), memRefObj->type);
			WriteBinary((*ofs), memRefObj->isRead);
			(*ofs).write(reinterpret_cast<CHAR*>(&memRefObj->vea), g_virtualAddressSize);
			(*ofs).write(reinterpret_cast<CHAR*>(&paddr), g_physicalAddressSize);
			WriteBinary((*ofs), memRefObj->size);
			baseAddr = memRefObj->vea;
			if (memRefObj->isRead)
				++readCount;
			else
				++writeCount;
			buff = reinterpret_cast<UINT8*>(buff)+sizeof(MEM_REF_1);
			isMem = TRUE;
			break;
		}
		case BUFFER_ENTRY_TYPE_MR2:
		{
			MEM_REF_2 *memRefObj = reinterpret_cast<MEM_REF_2*>(buff);
			WriteBinary((*ofs), memRefObj->type);
			WriteBinary((*ofs), memRefObj->isRead);
			if (memRefObj->isRead)
				++readCount;
			else
				++writeCount;
			buff = reinterpret_cast<UINT8*>(buff)+sizeof(MEM_REF_2);
			isMem = TRUE;
			break;
		}
		case BUFFER_ENTRY_TYPE_VGATHER_0:
		{
			MEM_REF_0 *memRefObj = reinterpret_cast<MEM_REF_0*>(buff);
			WriteBinary((*ofs), memRefObj->type);
			WriteBinary((*ofs), memRefObj->isRead);
			(*ofs).write(reinterpret_cast<CHAR*>(&memRefObj->vea), g_virtualAddressSize);
			(*ofs).write(reinterpret_cast<CHAR*>(&paddr), g_physicalAddressSize);
			WriteBinary((*ofs), memRefObj->size);
			WriteBinaryMultiMemAccessInfoAndDispose((*ofs),
				reinterpret_cast<ALLERIA_MULTI_MEM_ACCESS_INFO*>(memRefObj->value),
				&physicalAddressesIterator);
			baseAddr = memRefObj->vea;
			if (memRefObj->isRead)
				++readCount;
			else
				++writeCount;
			buff = reinterpret_cast<UINT8*>(buff)+sizeof(MEM_REF_0);
			isMem = TRUE;
			break;
		}
		case BUFFER_ENTRY_TYPE_VGATHER_1:
		{
			MEM_REF_1 *memRefObj = reinterpret_cast<MEM_REF_1*>(buff);
			WriteBinary((*ofs), memRefObj->type);
			WriteBinary((*ofs), memRefObj->isRead);
			baseAddr = reinterpret_cast<ALLERIA_MULTI_MEM_ACCESS_ADDRS*>(memRefObj->vea)->base;
			(*ofs).write(reinterpret_cast<CHAR*>(&baseAddr), g_virtualAddressSize);
			(*ofs).write(reinterpret_cast<CHAR*>(&paddr), g_physicalAddressSize);
			WriteBinary((*ofs), memRefObj->size);
			WriteBinaryMultiMemAccessAddrsAndDispose((*ofs),
				reinterpret_cast<ALLERIA_MULTI_MEM_ACCESS_ADDRS*>(memRefObj->vea),
				&physicalAddressesIterator);
			if (memRefObj->isRead)
				++readCount;
			else
				++writeCount;
			buff = reinterpret_cast<UINT8*>(buff)+sizeof(MEM_REF_1);
			isMem = TRUE;
			break;
		}
		case BUFFER_ENTRY_TYPE_XSAVE_0:
		{
			MEM_REF_0 *memRefObj = reinterpret_cast<MEM_REF_0*>(buff);
			WriteBinary((*ofs), memRefObj->type);
			WriteBinary((*ofs), memRefObj->isRead);
			(*ofs).write(reinterpret_cast<CHAR*>(&memRefObj->vea), g_virtualAddressSize);
			(*ofs).write(reinterpret_cast<CHAR*>(&paddr), g_physicalAddressSize);
			WriteBinary((*ofs), memRefObj->size);
			WriteBinaryXsaveInfoAndDispose((*ofs),
				reinterpret_cast<std::vector<MEM_ACCESS_GENERAL_0>*>(memRefObj->value),
				&physicalAddressesIterator);
			baseAddr = memRefObj->vea;
			if (memRefObj->isRead)
				++readCount;
			else
				++writeCount;
			buff = reinterpret_cast<UINT8*>(buff)+sizeof(MEM_REF_0);
			isMem = TRUE;
			break;
		}
		case BUFFER_ENTRY_TYPE_XSAVE_1:
		{
			MEM_REF_1 *memRefObj = reinterpret_cast<MEM_REF_1*>(buff);
			WriteBinary((*ofs), memRefObj->type);
			WriteBinary((*ofs), memRefObj->isRead);
			baseAddr = reinterpret_cast<ALLERIA_XSAVE_MEM_ACCESS_ADDRS*>(memRefObj->vea)->base;
			(*ofs).write(reinterpret_cast<CHAR*>(&baseAddr), g_virtualAddressSize);
			(*ofs).write(reinterpret_cast<CHAR*>(&paddr), g_physicalAddressSize);
			WriteBinary((*ofs), memRefObj->size);
			WriteBinaryXsaveAddrsAndDispose((*ofs),
				reinterpret_cast<ALLERIA_XSAVE_MEM_ACCESS_ADDRS*>(memRefObj->vea),
				&physicalAddressesIterator);
			if (memRefObj->isRead)
				++readCount;
			else
				++writeCount;
			buff = reinterpret_cast<UINT8*>(buff)+sizeof(MEM_REF_1);
			isMem = TRUE;
			break;
		}
		case BUFFER_ENTRY_TYPE_BRANCH_RECORD:
		{
			BRANCH_RECORD *br = reinterpret_cast<BRANCH_RECORD*>(buff);
			WriteBinary((*ofs), br->type);
			(*ofs).write(reinterpret_cast<CHAR*>(&br->target), g_virtualAddressSize);
			(*ofs).write(reinterpret_cast<CHAR*>(&paddr), g_physicalAddressSize);
			WriteBinary((*ofs), br->taken);
			buff = reinterpret_cast<UINT8*>(buff)+sizeof(BRANCH_RECORD);
			break;
		}
		case BUFFER_ENTRY_TYPE_SYS_CALL_RECORD:
		{
			SYS_CALL_RECORD *sysRec = reinterpret_cast<SYS_CALL_RECORD*>(buff);
			WriteBinary((*ofs), sysRec->type);
			WriteBinary((*ofs), sysRec->errRetRecorded);
			WriteBinary((*ofs), sysRec->num);
			WriteBinary((*ofs), sysRec->retVal);
			WriteBinary((*ofs), sysRec->err);
			WriteBinary((*ofs), sysRec->arg0);
			WriteBinary((*ofs), sysRec->arg1);
			WriteBinary((*ofs), sysRec->arg2);
			WriteBinary((*ofs), sysRec->arg3);
			WriteBinary((*ofs), sysRec->arg4);
			WriteBinary((*ofs), sysRec->arg5);
			WriteBinary((*ofs), sysRec->arg6);
			WriteBinary((*ofs), sysRec->arg7);
			WriteBinary((*ofs), sysRec->arg8);
			WriteBinary((*ofs), sysRec->arg9);
			WriteBinary((*ofs), sysRec->arg10);
			WriteBinary((*ofs), sysRec->arg11);
			WriteBinary((*ofs), sysRec->arg12);
			WriteBinary((*ofs), sysRec->arg13);
			WriteBinary((*ofs), sysRec->arg14);
			WriteBinary((*ofs), sysRec->arg15);
			buff = reinterpret_cast<UINT8*>(buff)+sizeof(SYS_CALL_RECORD);
			break;
		}
		default:
			ASSERT(FALSE, "Buffer corrupted.");
		}

		if (isMem)
		{
			*readsCount += readCount;
			*writesCount += writeCount;
			++(*memInsCount);
		}

		if (isIns)
		{
			++(*insCount);
		}

		if (g_v2pTransMode == V2P_TRANS_MODE_ACCURATE && baseAddr)
		{
			UnpinVirtualPage(reinterpret_cast<void*>(baseAddr));
		}
	}

	if (g_v2pTransMode != V2P_TRANS_MODE_OFF)
	{
		ASSERTX(physicalAddresses + physicalAddressesCount == physicalAddressesIterator);
		delete[] physicalAddresses;
	}

	// Flush the buffer here to make sure that the process doesn't terminate without flushing everything.
	(*ofs).flush();

	// Mark all slots vacant.
	size_t bufferSize = (ADDRINT)bufferState.m_bufferEnd - (ADDRINT)bufferState.m_bufferStart;
	std::memset(bufferState.m_bufferStart, 0, bufferSize);
}

BOOL BreakIfNotInBuffer(VOID *addr, size_t size)
{
	THREADID threadIndex = PIN_ThreadId();
	APP_THREAD_MANAGER *atm = reinterpret_cast<APP_THREAD_MANAGER*>(
		PIN_GetThreadData(lstate.appThreadManagerKey, threadIndex));
	if (addr < atm->GetCurrentBuffer() || ((reinterpret_cast<UINT8*>(addr)+size) >= atm->GetCurrentBufferLast()))
	{
		__debugbreak();
		return FALSE;
	}
	return TRUE;
}

BOOL BreakIfNotSafeToWrite(VOID *buffer, size_t size)
{
	UINT8 *temp = reinterpret_cast<UINT8*>(buffer);
	for (size_t i = 0; i < size; ++i)
	{
		if (temp[i] != 0)
		{
			__debugbreak();
			return FALSE;
		}
	}
	return BreakIfNotInBuffer(buffer, size);
}

//#define DONT_CORRUPT_THE_BUFFER(addr, type) if (!BreakIfNotSafeToWrite(addr, sizeof(type))) return
#define DONT_CORRUPT_THE_BUFFER(addr, type)

VOID PIN_FAST_ANALYSIS_CALL AnalyzeInsRecord0(
	ADDRINT buffer,
	ADDRINT bufferSlotAddress,
	ADDRINT insAddr,
	UINT32 routineId)
{
	// INS_RECORD_0 and INS_RECORD_1 have the same structure.
	INS_RECORD_0 *insRecord = reinterpret_cast<INS_RECORD_0*>(buffer + bufferSlotAddress);
	DONT_CORRUPT_THE_BUFFER(insRecord, INS_RECORD_0);
	insRecord->insAddr = insAddr;
	insRecord->routineId = routineId;
	insRecord->type = BUFFER_ENTRY_TYPE_IR0;
	GetCurrentTimestamp(insRecord->timestamp);
}

VOID PIN_FAST_ANALYSIS_CALL AnalyzeInsRecord1(
	ADDRINT buffer,
	ADDRINT bufferSlotAddress,
	ADDRINT insAddr,
	UINT32 routineId)
{
	// INS_RECORD_0 and INS_RECORD_1 have the same structure.
	INS_RECORD_1 *insRecord = reinterpret_cast<INS_RECORD_1*>(buffer + bufferSlotAddress);
	DONT_CORRUPT_THE_BUFFER(insRecord, INS_RECORD_1);
	insRecord->insAddr = insAddr;
	insRecord->routineId = routineId;
	insRecord->type = BUFFER_ENTRY_TYPE_IR1;
}

VOID PIN_FAST_ANALYSIS_CALL AnalyzeInsRecord2(
	ADDRINT buffer,
	ADDRINT bufferSlotAddress,
	ADDRINT insAddr)
{
	// INS_RECORD_0 and INS_RECORD_1 have the same structure.
	INS_RECORD_2 *insRecord = reinterpret_cast<INS_RECORD_2*>(buffer + bufferSlotAddress);
	DONT_CORRUPT_THE_BUFFER(insRecord, INS_RECORD_2);
	insRecord->insAddr = insAddr;
	insRecord->type = BUFFER_ENTRY_TYPE_IR2;
}

VOID PIN_FAST_ANALYSIS_CALL AnalyzeWriteEa0(
	ADDRINT buffer, ADDRINT bufferSlotAddress,
	ADDRINT ea, UINT32 size)
{
	MEM_REF_0 *memRef = reinterpret_cast<MEM_REF_0*>(buffer + bufferSlotAddress);
	DONT_CORRUPT_THE_BUFFER(memRef, MEM_REF_0);
	memRef->isRead = FALSE;
	memRef->vea = ea;
	memRef->type = BUFFER_ENTRY_TYPE_MR0;
	memRef->size = size;
}

VOID PIN_FAST_ANALYSIS_CALL AnalyzeWriteEa1(
	ADDRINT buffer, ADDRINT bufferSlotAddress,
	ADDRINT ea, UINT32 size)
{
	MEM_REF_1 *memRef = reinterpret_cast<MEM_REF_1*>(buffer + bufferSlotAddress);
	DONT_CORRUPT_THE_BUFFER(memRef, MEM_REF_1);
	memRef->isRead = FALSE;
	memRef->vea = ea;
	memRef->type = BUFFER_ENTRY_TYPE_MR1;
	memRef->size = size;
}

VOID PIN_FAST_ANALYSIS_CALL AnalyzeWriteEa2(
	ADDRINT buffer, ADDRINT bufferSlotAddress)
{
	MEM_REF_2 *memRef = reinterpret_cast<MEM_REF_2*>(buffer + bufferSlotAddress);
	DONT_CORRUPT_THE_BUFFER(memRef, MEM_REF_2);
	memRef->isRead = FALSE;
	memRef->type = BUFFER_ENTRY_TYPE_MR2;
}

BOOL RecordMemValue(ADDRINT vea, UINT64 *value, UINT32 size)
{
	switch (size)
	{
	case 0:
		ASSERT(FALSE, "memory value recording 0 repeat count");
		break;

	case 1:
	{
		UINT8 x;
		if (PIN_SafeCopy(&x, (UINT8*)(vea), 1) == 1)
			*value = static_cast<UINT64>(x);
		else
			return FALSE;
	}
	break;

	case 2:
	{
		UINT16 x;
		if (PIN_SafeCopy(&x, (UINT8*)(vea), 2) == 2)
			*value = static_cast<UINT64>(x);
		else
			return FALSE;
	}
	break;

	case 4:
	{
		UINT32 x;
		if (PIN_SafeCopy(&x, (UINT8*)(vea), 4) == 4)
			*value = static_cast<UINT64>(x);
		else
			return FALSE;
	}
	break;

	case 8:
	{
		UINT64 x;
		if (PIN_SafeCopy(&x, (UINT8*)(vea), 8) == 8)
			*value = x;
		else
			return FALSE;
	}
	break;

	default:
		// Size of memory access is larger than 8 bytes.
		// So the field is used to hold an address to a location that contains the full value.
		UINT8 *x = new UINT8[size];
		if (PIN_SafeCopy(x, (UINT8*)(vea), size) == size)
			*value = reinterpret_cast<UINT64>(x);
		else
		{
			delete[] x;
			return FALSE;
		}
		break;
	}
	return TRUE;
}

VOID PIN_FAST_ANALYSIS_CALL AnalyzeWrite0(
	ADDRINT buffer, ADDRINT bufferSlotAddress)
{
	MEM_REF_0 *memRef = reinterpret_cast<MEM_REF_0*>(buffer + bufferSlotAddress);
	memRef->isValueValid = RecordMemValue(memRef->vea, &(memRef->value), memRef->size);
	memRef->type = BUFFER_ENTRY_TYPE_MR0;
}

VOID PIN_FAST_ANALYSIS_CALL Analyze2Read0(
	ADDRINT buffer, ADDRINT bufferSlotAddress,
	ADDRINT ea1, ADDRINT ea2, UINT32 size)
{
	MEM_REF_0 *memRef = reinterpret_cast<MEM_REF_0*>(buffer + bufferSlotAddress);
	DONT_CORRUPT_THE_BUFFER(memRef, MEM_REF_0);
	memRef->isRead = TRUE;
	memRef->vea = ea1;
	memRef->size = size;
	memRef->isValueValid = RecordMemValue(memRef->vea, &(memRef->value), memRef->size);
	memRef->type = BUFFER_ENTRY_TYPE_MR0;

	memRef = reinterpret_cast<MEM_REF_0*>(buffer + bufferSlotAddress + sizeof(MEM_REF_0));
	DONT_CORRUPT_THE_BUFFER(memRef, MEM_REF_0);
	memRef->isRead = TRUE;
	memRef->vea = ea2;
	memRef->size = size;
	memRef->isValueValid = RecordMemValue(memRef->vea, &(memRef->value), memRef->size);
	memRef->type = BUFFER_ENTRY_TYPE_MR0;
}

VOID PIN_FAST_ANALYSIS_CALL Analyze2Read1(
	ADDRINT buffer, ADDRINT bufferSlotAddress,
	ADDRINT ea1, ADDRINT ea2, UINT32 size)
{
	MEM_REF_1 *memRef = reinterpret_cast<MEM_REF_1*>(buffer + bufferSlotAddress);
	DONT_CORRUPT_THE_BUFFER(memRef, MEM_REF_1);
	memRef->isRead = TRUE;
	memRef->vea = ea1;
	memRef->size = size;
	memRef->type = BUFFER_ENTRY_TYPE_MR1;

	memRef = reinterpret_cast<MEM_REF_1*>(buffer + bufferSlotAddress + sizeof(MEM_REF_1));
	DONT_CORRUPT_THE_BUFFER(memRef, MEM_REF_1);
	memRef->isRead = TRUE;
	memRef->vea = ea2;
	memRef->size = size;
	memRef->type = BUFFER_ENTRY_TYPE_MR1;
}

VOID PIN_FAST_ANALYSIS_CALL AnalyzeRead0(
	ADDRINT buffer, ADDRINT bufferSlotAddress,
	ADDRINT ea, UINT32 size)
{
	MEM_REF_0 *memRef = reinterpret_cast<MEM_REF_0*>(buffer + bufferSlotAddress);
	DONT_CORRUPT_THE_BUFFER(memRef, MEM_REF_0);
	memRef->isRead = TRUE;
	memRef->vea = ea;
	memRef->size = size;
	memRef->isValueValid = RecordMemValue(memRef->vea, &(memRef->value), memRef->size);
	memRef->type = BUFFER_ENTRY_TYPE_MR0;
}

VOID PIN_FAST_ANALYSIS_CALL AnalyzeRead1(
	ADDRINT buffer, ADDRINT bufferSlotAddress,
	ADDRINT ea, UINT32 size)
{
	MEM_REF_1 *memRef = reinterpret_cast<MEM_REF_1*>(buffer + bufferSlotAddress);
	DONT_CORRUPT_THE_BUFFER(memRef, MEM_REF_1);
	memRef->isRead = TRUE;
	memRef->vea = ea;
	memRef->size = size;
	memRef->type = BUFFER_ENTRY_TYPE_MR1;
}

VOID PIN_FAST_ANALYSIS_CALL AnalyzeRead2(
	ADDRINT buffer, ADDRINT bufferSlotAddress)
{
	MEM_REF_2 *memRef = reinterpret_cast<MEM_REF_2*>(buffer + bufferSlotAddress);
	DONT_CORRUPT_THE_BUFFER(memRef, MEM_REF_2);
	memRef->isRead = TRUE;
	memRef->type = BUFFER_ENTRY_TYPE_MR2;
}

//VOID PIN_FAST_ANALYSIS_CALL AnalyzeInsCount(UINT32 count, UINT32 memCount)
//{
//	APP_THREAD_MANAGER *atm = static_cast<APP_THREAD_MANAGER*>(
//		PIN_GetThreadData(lstate.appThreadManagerKey, PIN_ThreadId()));
//	atm->AddInsCount(count);
//	atm->AddMemInsCount(memCount);
//}

VOID PIN_FAST_ANALYSIS_CALL AnalyzeBranch(ADDRINT buffer, ADDRINT bufferSlotAddress,
	ADDRINT branchTarget, BOOL taken)
{
	BRANCH_RECORD *br = reinterpret_cast<BRANCH_RECORD*>(buffer + bufferSlotAddress);
	DONT_CORRUPT_THE_BUFFER(br, BRANCH_RECORD);
	br->type = BUFFER_ENTRY_TYPE_BRANCH_RECORD;
	br->taken = taken;
	br->target = branchTarget;
}

VOID PIN_FAST_ANALYSIS_CALL AnalyzeSysCallPre(ADDRINT buffer, ADDRINT bufferSlotAddress,
	ADDRINT num,
	ADDRINT arg0, ADDRINT arg1, ADDRINT arg2, ADDRINT arg3,
	ADDRINT arg4, ADDRINT arg5, ADDRINT arg6, ADDRINT arg7,
	ADDRINT arg8, ADDRINT arg9, ADDRINT arg10, ADDRINT arg11,
	ADDRINT arg12, ADDRINT arg13, ADDRINT arg14, ADDRINT arg15)
{
	SYS_CALL_RECORD *sysRec = reinterpret_cast<SYS_CALL_RECORD*>(buffer + bufferSlotAddress);
	DONT_CORRUPT_THE_BUFFER(memRef, SYS_CALL_RECORD);
	sysRec->type = BUFFER_ENTRY_TYPE_SYS_CALL_RECORD;
	sysRec->num = num;
	sysRec->arg0 = arg0;
	sysRec->arg1 = arg1;
	sysRec->arg2 = arg2;
	sysRec->arg3 = arg3;
	sysRec->arg4 = arg4;
	sysRec->arg5 = arg5;
	sysRec->arg6 = arg6;
	sysRec->arg7 = arg7;
	sysRec->arg8 = arg8;
	sysRec->arg9 = arg9;
	sysRec->arg10 = arg10;
	sysRec->arg11 = arg11;
	sysRec->arg12 = arg12;
	sysRec->arg13 = arg13;
	sysRec->arg14 = arg14;
	sysRec->arg15 = arg15;

	// Set the address for AnalyzeSysCallExit or OnContextChanged.
	/* From Pin's doc:
	When a system call returns to the application, the tool receives the SYSCALL_EXIT_CALLBACK notification.
	Usually, this callback immediately follows the corresponding SYSCALL_ENTRY_CALLBACK notification and there is no application code executed between these two events.
	However, some (Windows) system calls can be interrupted by a system event (APC, Windows callback, exception) before they return to the application.
	If this happens, the tool receives the corresponding CONTEXT_CHANGE_CALLBACK notification just before the (user mode) handler of the system event gets executed.
	Eventually, when the event handler and the interrupted system call are completed, the SYSCALL_EXIT_CALLBACK notification is delivered to the tool.
	*/
	PIN_SetThreadData(lstate.sysCallBufferEntryAddress, sysRec, PIN_ThreadId());
}

// XSAVE and XSAVEOPT.
VOID PIN_FAST_ANALYSIS_CALL AnalyzeXsavePre0(ADDRINT buffer, ADDRINT bufferSlotAddress,
	ADDRINT ea)
{
	MEM_REF_0 *memRef = reinterpret_cast<MEM_REF_0*>(buffer + bufferSlotAddress);
	DONT_CORRUPT_THE_BUFFER(memRef, MEM_REF_0);
	memRef->isRead = FALSE;
	memRef->vea = ea;
	memRef->type = BUFFER_ENTRY_TYPE_XSAVE_0;

	memRef = reinterpret_cast<MEM_REF_0*>(buffer + bufferSlotAddress + sizeof(MEM_REF_0));
	DONT_CORRUPT_THE_BUFFER(memRef, MEM_REF_0);
	memRef->isRead = TRUE;
	memRef->type = BUFFER_ENTRY_TYPE_MR0;
	memRef->vea = ea + 512;
}

// XSAVE and XSAVEOPT.
VOID PIN_FAST_ANALYSIS_CALL AnalyzeXsavePre1(ADDRINT buffer, ADDRINT bufferSlotAddress,
	ADDRINT ea)
{
	MEM_REF_1 *memRef = reinterpret_cast<MEM_REF_1*>(buffer + bufferSlotAddress);
	DONT_CORRUPT_THE_BUFFER(memRef, MEM_REF_1);
	memRef->isRead = FALSE;
	memRef->vea = ea;
	memRef->type = BUFFER_ENTRY_TYPE_XSAVE_1;

	memRef = reinterpret_cast<MEM_REF_1*>(buffer + bufferSlotAddress + sizeof(MEM_REF_1));
	DONT_CORRUPT_THE_BUFFER(memRef, MEM_REF_1);
	memRef->isRead = TRUE;
	memRef->type = BUFFER_ENTRY_TYPE_MR1;
	memRef->vea = ea + 512;
}

// XSAVE and XSAVEOPT.
VOID PIN_FAST_ANALYSIS_CALL AnalyzeXsavePre2(ADDRINT buffer, ADDRINT bufferSlotAddress)
{
	MEM_REF_2 *memRef = reinterpret_cast<MEM_REF_2*>(buffer + bufferSlotAddress);
	DONT_CORRUPT_THE_BUFFER(memRef, MEM_REF_2);
	memRef->isRead = FALSE;
	memRef->type = BUFFER_ENTRY_TYPE_MR2;

	memRef = reinterpret_cast<MEM_REF_2*>(buffer + bufferSlotAddress + sizeof(MEM_REF_2));
	DONT_CORRUPT_THE_BUFFER(memRef, MEM_REF_2);
	memRef->isRead = TRUE;
	memRef->type = BUFFER_ENTRY_TYPE_MR2;
}

// XSAVE and XSAVEOPT.
VOID PIN_FAST_ANALYSIS_CALL AnalyzeXsavePost0(ADDRINT buffer, ADDRINT bufferSlotAddress)
{
	/*

	if (isOpt) // XSAVEOPT.
	{
	// Check whether XCR0 with ECX = 1 is supported.
	int cpuInfo[4];
	__cpuidex(cpuInfo, 0x0D, 1);
	if (cpuInfo[0] & 0x4)
	{
	// XCR0 & XINUSE.
	UINT32 XCR0_AND_XINUSE_Low;
	UINT32 XCR0_AND_XINUSE_High;
	_asm
	{
	mov ecx, 1;
	xgetbv;
	mov XCR0_AND_XINUSE_Low, eax;
	mov XCR0_AND_XINUSE_High, edx;
	}
	UINT64 XCR0_AND_XINUSE = XCR0_AND_XINUSE_Low | (((UINT64)XCR0_AND_XINUSE_High) << 32);

	// Apply init optimization.
	XCR0 = XCR0_AND_XINUSE;
	}
	}

	*/

	MEM_REF_0 *memRef = reinterpret_cast<MEM_REF_0*>(buffer + bufferSlotAddress);
	memRef->value = reinterpret_cast<UINT64>(GetXsaveAccessInfo(reinterpret_cast<UINT8*>(memRef->vea), Is32BitPlat(), memRef->size, TRUE));

	memRef = reinterpret_cast<MEM_REF_0*>(buffer + bufferSlotAddress + sizeof(MEM_REF_0));
	memRef->size = 8; // XSTATE_BV.
	RecordMemValue(memRef->vea, &(memRef->value), memRef->size);
}

// XSAVE and XSAVEOPT.
VOID PIN_FAST_ANALYSIS_CALL AnalyzeXsavePost1(ADDRINT buffer, ADDRINT bufferSlotAddress)
{
	MEM_REF_1 *memRef = reinterpret_cast<MEM_REF_1*>(buffer + bufferSlotAddress);
	memRef->vea = reinterpret_cast<UINT64>(GetXsaveAccessInfo(reinterpret_cast<UINT8*>(memRef->vea), Is32BitPlat(), memRef->size, FALSE));

	memRef = reinterpret_cast<MEM_REF_1*>(buffer + bufferSlotAddress + sizeof(MEM_REF_1));
	memRef->size = 8; // XSTATE_BV.
}

// XSAVEC.
VOID PIN_FAST_ANALYSIS_CALL AnalyzeXsavecPost0(ADDRINT buffer, ADDRINT bufferSlotAddress)
{
	MEM_REF_0 *memRef = reinterpret_cast<MEM_REF_0*>(buffer + bufferSlotAddress);
	memRef->value = reinterpret_cast<UINT64>(GetXsavecAccessInfo(reinterpret_cast<UINT8*>(memRef->vea), Is32BitPlat(), memRef->size, TRUE));
}

// XSAVEC.
VOID PIN_FAST_ANALYSIS_CALL AnalyzeXsavecPost1(ADDRINT buffer, ADDRINT bufferSlotAddress)
{
	MEM_REF_1 *memRef = reinterpret_cast<MEM_REF_1*>(buffer + bufferSlotAddress);
	memRef->vea = reinterpret_cast<UINT64>(GetXsavecAccessInfo(reinterpret_cast<UINT8*>(memRef->vea), Is32BitPlat(), memRef->size, FALSE));
}

VOID PIN_FAST_ANALYSIS_CALL AnalyzeXrstor0(ADDRINT buffer, ADDRINT bufferSlotAddress,
	const CONTEXT *ctxt,
	ADDRINT ea)
{
	MEM_REF_0 *memRef = reinterpret_cast<MEM_REF_0*>(buffer + bufferSlotAddress);
	DONT_CORRUPT_THE_BUFFER(memRef, MEM_REF_0);
	memRef->isRead = TRUE;
	memRef->vea = ea;
	memRef->type = BUFFER_ENTRY_TYPE_XSAVE_0;
	memRef->value = reinterpret_cast<UINT64>(GetXrstorAccessInfo(ctxt, reinterpret_cast<UINT8*>(memRef->vea), Is32BitPlat(), memRef->size, TRUE));
}

VOID PIN_FAST_ANALYSIS_CALL AnalyzeXrstor1(ADDRINT buffer, ADDRINT bufferSlotAddress,
	const CONTEXT *ctxt,
	ADDRINT ea)
{
	MEM_REF_1 *memRef = reinterpret_cast<MEM_REF_1*>(buffer + bufferSlotAddress);
	DONT_CORRUPT_THE_BUFFER(memRef, MEM_REF_1);
	memRef->isRead = TRUE;
	memRef->vea = reinterpret_cast<UINT64>(GetXrstorAccessInfo(ctxt, reinterpret_cast<UINT8*>(ea), Is32BitPlat(), memRef->size, FALSE));
	memRef->type = BUFFER_ENTRY_TYPE_XSAVE_1;
}

VOID PIN_FAST_ANALYSIS_CALL AnalyzeXrstor2(ADDRINT buffer, ADDRINT bufferSlotAddress)
{
	MEM_REF_2 *memRef = reinterpret_cast<MEM_REF_2*>(buffer + bufferSlotAddress);
	DONT_CORRUPT_THE_BUFFER(memRef, MEM_REF_2);
	memRef->isRead = TRUE;
	memRef->type = BUFFER_ENTRY_TYPE_MR2;
}

VOID PIN_FAST_ANALYSIS_CALL AnalyzeVgather0(ADDRINT buffer, ADDRINT bufferSlotAddress,
	PIN_MULTI_MEM_ACCESS_INFO *accesses, ADDRINT base)
{
	MEM_REF_0 *memRef = reinterpret_cast<MEM_REF_0*>(buffer + bufferSlotAddress);
	DONT_CORRUPT_THE_BUFFER(memRef, MEM_REF_0);
	memRef->isRead = TRUE;
	memRef->type = BUFFER_ENTRY_TYPE_VGATHER_0;
	memRef->value = reinterpret_cast<ADDRINT>(CopyMultiMemAccessInfo(accesses, memRef->size));
	memRef->vea = base;
	memRef->isValueValid = TRUE; // Refer to CopyMultiMemAccessInfo.
}

VOID PIN_FAST_ANALYSIS_CALL AnalyzeVgather1(ADDRINT buffer, ADDRINT bufferSlotAddress,
	PIN_MULTI_MEM_ACCESS_INFO *accesses, ADDRINT base)
{
	MEM_REF_1 *memRef = reinterpret_cast<MEM_REF_1*>(buffer + bufferSlotAddress);
	DONT_CORRUPT_THE_BUFFER(memRef, MEM_REF_1);
	memRef->isRead = TRUE;
	memRef->type = BUFFER_ENTRY_TYPE_VGATHER_1;
	ALLERIA_MULTI_MEM_ACCESS_ADDRS *addrs = CopyMultiMemAccessAddrs(accesses, memRef->size);
	addrs->base = base;
	memRef->vea = reinterpret_cast<ADDRINT>(addrs);
}

VOID PIN_FAST_ANALYSIS_CALL AnalyzeVgather2(ADDRINT buffer, ADDRINT bufferSlotAddress)
{
	MEM_REF_2 *memRef = reinterpret_cast<MEM_REF_2*>(buffer + bufferSlotAddress);
	DONT_CORRUPT_THE_BUFFER(memRef, MEM_REF_2);
	memRef->isRead = TRUE;
	memRef->type = BUFFER_ENTRY_TYPE_MR2;
}

VOID PIN_FAST_ANALYSIS_CALL AnalyzePinPageOfAddress1(ADDRINT addr)
{
	PinVirtualPage(reinterpret_cast<void*>(addr));
}

VOID PIN_FAST_ANALYSIS_CALL AnalyzePinPageOfAddress2(ADDRINT addr1, ADDRINT addr2)
{
	PinVirtualPage(reinterpret_cast<void*>(addr1));
	PinVirtualPage(reinterpret_cast<void*>(addr2));
}

VOID PIN_FAST_ANALYSIS_CALL AnalyzeProcessorNumbers(ADDRINT buffer, ADDRINT bufferSlotAddress)
{
	PROCESSOR_RECORD *pr = reinterpret_cast<PROCESSOR_RECORD*>(buffer + bufferSlotAddress);
	DONT_CORRUPT_THE_BUFFER(pr, PROCESSOR_RECORD);
	pr->type = BUFFER_ENTRY_TYPE_PROCESSOR_RECORD;
	GetMyProcessorNumber(pr->processorPackage, pr->processorNumber);
}

VOID AnalyzeSysCallExit(THREADID threadIndex, CONTEXT *ctxt, SYSCALL_STANDARD std, VOID *v)
{
	SYS_CALL_RECORD *sysRec = reinterpret_cast<SYS_CALL_RECORD*>(PIN_GetThreadData(lstate.sysCallBufferEntryAddress, threadIndex));

	/*
	If any user-mode code got executed before the system call completes (due to Windows APC, Windows callback or Windows exception),
	OnContextChanged is called which will set the address to NULL to prevent buffer corruption. The reason that the buffer
	might get corrupted is that the code the executes before the system call completes is being profiled and might get the buffer
	to become full. This makes the address stored by AnalyzeSysCallPre unreliable. If this happened, we will just not record
	the system call return value and the errro number to simplify the implementation. Fortunately, this rarely happens in most apps.
	*/
	if (sysRec != NULL)
	{
		sysRec->retVal = PIN_GetSyscallReturn(ctxt, std);
		sysRec->err = PIN_GetSyscallErrno(ctxt, std);
		sysRec->errRetRecorded = TRUE;

		// This is necessary so that OnContextChanged handles the situation correctly.
		PIN_SetThreadData(lstate.sysCallBufferEntryAddress, NULL, threadIndex);
	}
}

VOID InstrumentMemIns(ANALYSIS_CALLS_TRACE& act)
{
	if (KnobTrackProcessorsEnabled)
	{
		INS_InsertCall(
			act.m_calls[0].ins,
			IPOINT_BEFORE,
			AFUNPTR(AnalyzeProcessorNumbers),
			IARG_FAST_ANALYSIS_CALL,
			IARG_REG_VALUE, lstate.bufferEndReg,
			IARG_ADDRINT, 0 - act.GetTraceSize(),
			IARG_END);
	}

	for (UINT32 i = 0; i < act.m_calls.size(); ++i)
	{
		ANALYSIS_CALL ac = act.m_calls[i];
		INS ins = ac.ins;
		ADDRINT addr = INS_Address(ins);
		RTN routine = INS_Rtn(ins);
		UINT32 routineId = 0;
		if (RTN_Valid(routine))
			routineId = RTN_Id(routine);

		AFUNPTR aFUNPTRir = NULL;
		switch (ac.iType)
		{
		case BUFFER_ENTRY_TYPE_IR0:
			aFUNPTRir = AFUNPTR(AnalyzeInsRecord0);
			break;
		case BUFFER_ENTRY_TYPE_IR1:
			aFUNPTRir = AFUNPTR(AnalyzeInsRecord1);
			break;
		case BUFFER_ENTRY_TYPE_IR2:
			aFUNPTRir = AFUNPTR(AnalyzeInsRecord2);
			break;
		case BUFFER_ENTRY_TYPE_INVALID:
			// Allow invalid so that we can record entries of different types for the same instruction.
			break;
		default:
			ASSERT(FALSE, "Unkown iType");
		}

		AFUNPTR aFUNPTRmrW = NULL;
		switch (ac.mType)
		{
		case BUFFER_ENTRY_TYPE_MR0:
			aFUNPTRmrW = AFUNPTR(AnalyzeWriteEa0);
			break;
		case BUFFER_ENTRY_TYPE_MR1:
			aFUNPTRmrW = AFUNPTR(AnalyzeWriteEa1);
			break;
		case BUFFER_ENTRY_TYPE_MR2:
			aFUNPTRmrW = AFUNPTR(AnalyzeWriteEa2);
			break;
		case BUFFER_ENTRY_TYPE_SYS_CALL_RECORD:
		case BUFFER_ENTRY_TYPE_BRANCH_RECORD:
			break;
		case BUFFER_ENTRY_TYPE_INVALID:
			// Allow invalid so that we can profile nstructions that don't access memory.
			break;
		default:
			ASSERT(FALSE, "Unkown mType");
		}

		if (ac.iType == BUFFER_ENTRY_TYPE_IR2)
		{
			INS_InsertPredicatedCall(ins, IPOINT_BEFORE, aFUNPTRir,
				IARG_FAST_ANALYSIS_CALL,
				IARG_REG_VALUE, lstate.bufferEndReg,
				IARG_ADDRINT, ac.offset - act.GetTraceSize(),
				IARG_ADDRINT, addr,
				IARG_END);
		}
		else if (ac.iType != BUFFER_ENTRY_TYPE_INVALID)
		{
			INS_InsertPredicatedCall(ins, IPOINT_BEFORE, aFUNPTRir,
				IARG_FAST_ANALYSIS_CALL,
				IARG_REG_VALUE, lstate.bufferEndReg,
				IARG_ADDRINT, ac.offset - act.GetTraceSize(),
				IARG_ADDRINT, addr,
				IARG_UINT32, routineId,
				IARG_END);
		}

		xed_decoded_inst_t *xedInst = INS_XedDec(ins);
		xed_category_enum_t xedInstcat = xed_decoded_inst_get_category(xedInst);
		ADDRINT instOffset = ac.offset;
		ac.offset += BufferTypeToSize(ac.iType);

		if (ac.mType == BUFFER_ENTRY_TYPE_BRANCH_RECORD)
		{
			INS_InsertPredicatedCall(ins, IPOINT_BEFORE, AFUNPTR(AnalyzeBranch),
				IARG_FAST_ANALYSIS_CALL,
				IARG_REG_VALUE, lstate.bufferEndReg,
				IARG_ADDRINT, ac.offset - act.GetTraceSize(),
				IARG_BRANCH_TARGET_ADDR,
				IARG_BRANCH_TAKEN,
				IARG_END);

			if (g_v2pTransMode == V2P_TRANS_MODE_ACCURATE)
			{
				INS_InsertPredicatedCall(ins, IPOINT_BEFORE, AFUNPTR(AnalyzePinPageOfAddress1),
					IARG_FAST_ANALYSIS_CALL,
					IARG_BRANCH_TARGET_ADDR,
					IARG_END);
			}
		}
		else if (ac.mType == BUFFER_ENTRY_TYPE_SYS_CALL_RECORD)
		{
			INS_InsertPredicatedCall(ins, IPOINT_BEFORE, AFUNPTR(AnalyzeSysCallPre),
				IARG_FAST_ANALYSIS_CALL,
				IARG_REG_VALUE, lstate.bufferEndReg,
				IARG_ADDRINT, ac.offset - act.GetTraceSize(),
				IARG_SYSCALL_NUMBER,
				IARG_SYSARG_VALUE, 0, IARG_SYSARG_VALUE, 1,
				IARG_SYSARG_VALUE, 2, IARG_SYSARG_VALUE, 3,
				IARG_SYSARG_VALUE, 4, IARG_SYSARG_VALUE, 5,
				IARG_SYSARG_VALUE, 6, IARG_SYSARG_VALUE, 7,
				IARG_SYSARG_VALUE, 8, IARG_SYSARG_VALUE, 9,
				IARG_SYSARG_VALUE, 10, IARG_SYSARG_VALUE, 11,
				IARG_SYSARG_VALUE, 12, IARG_SYSARG_VALUE, 13,
				IARG_SYSARG_VALUE, 14, IARG_SYSARG_VALUE, 15,
				IARG_END);

			// Note that IARG_SYSRET_VALUE and IARG_SYSRET_ERRNO are not used
			// because they are not supported at IPOINT_AFTER with sysenter/syscall
			// instructions because they are not fall-through instructions.
			// PIN_AddSyscallExitFunction is used to capture these values.
		}
		else if (xedInstcat == XED_CATEGORY_XSAVE || xedInstcat == XED_CATEGORY_XSAVEOPT)
		{
			/* TODO: Pin 3.0+ supports IARG_MEMEORYWRITE_SIZE and IARG_MEMORYREAD_SIZE for all these instructions.
			Use them to verify tha the sizes are correct.
			*/

			xed_iclass_enum_t xedInstclass = xed_decoded_inst_get_iclass(xedInst);

			if (xedInstclass == XED_ICLASS_XSAVE || xedInstclass == XED_ICLASS_XSAVE64 ||
				xedInstclass == XED_ICLASS_XSAVEOPT || xedInstclass == XED_ICLASS_XSAVEOPT64)
			{
				switch (ac.mType)
				{
				case BUFFER_ENTRY_TYPE_MR0:
					aFUNPTRmrW = AFUNPTR(AnalyzeXsavePre0);
					break;
				case BUFFER_ENTRY_TYPE_MR1:
					aFUNPTRmrW = AFUNPTR(AnalyzeXsavePre1);
					break;
				case BUFFER_ENTRY_TYPE_MR2:
					aFUNPTRmrW = AFUNPTR(AnalyzeXsavePre2);
					break;
				}
				if (ac.mType == BUFFER_ENTRY_TYPE_MR2)
				{
					INS_InsertPredicatedCall(ins, IPOINT_BEFORE, AFUNPTR(aFUNPTRmrW),
						IARG_FAST_ANALYSIS_CALL,
						IARG_REG_VALUE, lstate.bufferEndReg,
						IARG_ADDRINT, ac.offset - act.GetTraceSize(),
						IARG_END);
				}
				else
				{
					// AnalyzeXsavePre0 and AnalyzeXsavePre1 have the same signature.
					INS_InsertPredicatedCall(ins, IPOINT_BEFORE, AFUNPTR(aFUNPTRmrW),
						IARG_FAST_ANALYSIS_CALL,
						IARG_REG_VALUE, lstate.bufferEndReg,
						IARG_ADDRINT, ac.offset - act.GetTraceSize(),
						IARG_MEMORYWRITE_EA,
						IARG_END);

					if (g_v2pTransMode == V2P_TRANS_MODE_ACCURATE)
					{
						INS_InsertPredicatedCall(ins, IPOINT_BEFORE, AFUNPTR(AnalyzePinPageOfAddress1),
							IARG_FAST_ANALYSIS_CALL,
							IARG_MEMORYWRITE_EA,
							IARG_END);
					}

					if (ac.mType == BUFFER_ENTRY_TYPE_MR0)
					{
						INS_InsertPredicatedCall(ins, IPOINT_AFTER, AFUNPTR(AnalyzeXsavePost0),
							IARG_FAST_ANALYSIS_CALL,
							IARG_REG_VALUE, lstate.bufferEndReg,
							IARG_ADDRINT, ac.offset - act.GetTraceSize(),
							IARG_END);
					}
					else // BUFFER_ENTRY_TYPE_MR1
					{
						INS_InsertPredicatedCall(ins, IPOINT_AFTER, AFUNPTR(AnalyzeXsavePost1),
							IARG_FAST_ANALYSIS_CALL,
							IARG_REG_VALUE, lstate.bufferEndReg,
							IARG_ADDRINT, ac.offset - act.GetTraceSize(),
							IARG_END);
					}
				}
			}
			else if (xedInstclass == XED_ICLASS_XSAVEC || xedInstclass == XED_ICLASS_XSAVEC64)
			{
				if (ac.mType == BUFFER_ENTRY_TYPE_MR2)
				{
					INS_InsertPredicatedCall(ins, IPOINT_BEFORE, AFUNPTR(aFUNPTRmrW),
						IARG_FAST_ANALYSIS_CALL,
						IARG_REG_VALUE, lstate.bufferEndReg,
						IARG_ADDRINT, ac.offset - act.GetTraceSize(),
						IARG_END);
				}
				else
				{
					// AnalyzeWriteEa0 and AnalyzeWriteEa1 have the same signature.
					INS_InsertPredicatedCall(ins, IPOINT_BEFORE, AFUNPTR(aFUNPTRmrW),
						IARG_FAST_ANALYSIS_CALL,
						IARG_REG_VALUE, lstate.bufferEndReg,
						IARG_ADDRINT, ac.offset - act.GetTraceSize(),
						IARG_MEMORYWRITE_EA,
						IARG_END);

					if (g_v2pTransMode == V2P_TRANS_MODE_ACCURATE)
					{
						INS_InsertPredicatedCall(ins, IPOINT_BEFORE, AFUNPTR(AnalyzePinPageOfAddress1),
							IARG_FAST_ANALYSIS_CALL,
							IARG_MEMORYWRITE_EA,
							IARG_END);
					}

					if (ac.mType == BUFFER_ENTRY_TYPE_MR0)
					{
						INS_InsertPredicatedCall(ins, IPOINT_AFTER, AFUNPTR(AnalyzeXsavecPost0),
							IARG_FAST_ANALYSIS_CALL,
							IARG_REG_VALUE, lstate.bufferEndReg,
							IARG_ADDRINT, ac.offset - act.GetTraceSize(),
							IARG_END);
					}
					else // BUFFER_ENTRY_TYPE_MR1			
					{
						INS_InsertPredicatedCall(ins, IPOINT_AFTER, AFUNPTR(AnalyzeXsavecPost1),
							IARG_FAST_ANALYSIS_CALL,
							IARG_REG_VALUE, lstate.bufferEndReg,
							IARG_ADDRINT, ac.offset - act.GetTraceSize(),
							IARG_END);
					}
				}
			}
			else if (xedInstclass == XED_ICLASS_XRSTOR || xedInstclass == XED_ICLASS_XRSTOR64)
			{
				switch (ac.mType)
				{
				case BUFFER_ENTRY_TYPE_MR0:
					INS_InsertPredicatedCall(ins, IPOINT_BEFORE, AFUNPTR(AnalyzeXrstor0),
						IARG_FAST_ANALYSIS_CALL,
						IARG_REG_VALUE, lstate.bufferEndReg,
						IARG_ADDRINT, ac.offset - act.GetTraceSize(),
						IARG_CONST_CONTEXT,
						IARG_MEMORYREAD_EA,
						IARG_END);
					break;
				case BUFFER_ENTRY_TYPE_MR1:
					INS_InsertPredicatedCall(ins, IPOINT_BEFORE, AFUNPTR(AnalyzeXrstor1),
						IARG_FAST_ANALYSIS_CALL,
						IARG_REG_VALUE, lstate.bufferEndReg,
						IARG_ADDRINT, ac.offset - act.GetTraceSize(),
						IARG_CONST_CONTEXT,
						IARG_MEMORYREAD_EA,
						IARG_END);
					break;
				case BUFFER_ENTRY_TYPE_MR2:
					INS_InsertPredicatedCall(ins, IPOINT_BEFORE, AFUNPTR(AnalyzeXrstor2),
						IARG_FAST_ANALYSIS_CALL,
						IARG_REG_VALUE, lstate.bufferEndReg,
						IARG_ADDRINT, ac.offset - act.GetTraceSize(),
						IARG_END);
					break;
				}

				if (g_v2pTransMode == V2P_TRANS_MODE_ACCURATE && ac.mType != BUFFER_ENTRY_TYPE_MR2)
				{
					INS_InsertPredicatedCall(ins, IPOINT_BEFORE, AFUNPTR(AnalyzePinPageOfAddress1),
						IARG_FAST_ANALYSIS_CALL,
						IARG_MEMORYREAD_EA,
						IARG_END);
				}
			}
		}
		else if (INS_IsVgather(ins))
		{
			aFUNPTRmrW = AFUNPTR(AnalyzeVgather1);
			switch (ac.mType)
			{
			case BUFFER_ENTRY_TYPE_MR0:
				aFUNPTRmrW = AFUNPTR(AnalyzeVgather0);
			case BUFFER_ENTRY_TYPE_MR1:
				INS_InsertPredicatedCall(ins, IPOINT_BEFORE, AFUNPTR(aFUNPTRmrW),
					IARG_FAST_ANALYSIS_CALL,
					IARG_REG_VALUE, lstate.bufferEndReg,
					IARG_ADDRINT, ac.offset - act.GetTraceSize(),
					IARG_MULTI_MEMORYACCESS_EA,
					IARG_MEMORYOP_EA, 1,
					IARG_END);
				break;
			case BUFFER_ENTRY_TYPE_MR2:
				INS_InsertPredicatedCall(ins, IPOINT_BEFORE, AFUNPTR(AnalyzeVgather2),
					IARG_FAST_ANALYSIS_CALL,
					IARG_REG_VALUE, lstate.bufferEndReg,
					IARG_ADDRINT, ac.offset - act.GetTraceSize(),
					IARG_END);
				break;
			}

			if (g_v2pTransMode == V2P_TRANS_MODE_ACCURATE && ac.mType != BUFFER_ENTRY_TYPE_MR2)
			{
				INS_InsertPredicatedCall(ins, IPOINT_BEFORE, AFUNPTR(AnalyzePinPageOfAddress1),
					IARG_FAST_ANALYSIS_CALL,
					IARG_MEMORYOP_EA, 1,
					IARG_END);
			}
		}
		else
		{
			// All instructions that are handled here must support IARG_MEMORYWRITE_SIZE and IARG_MEMORYREAD_SIZE.
			ASSERTX(INS_hasKnownMemorySize(ins));

			if (INS_IsMemoryWrite(ins))
			{
				if (ac.mType == BUFFER_ENTRY_TYPE_MR2)
				{
					INS_InsertPredicatedCall(ins, IPOINT_BEFORE, AFUNPTR(aFUNPTRmrW),
						IARG_FAST_ANALYSIS_CALL,
						IARG_REG_VALUE, lstate.bufferEndReg,
						IARG_ADDRINT, ac.offset - act.GetTraceSize(),
						IARG_END);
				}
				else
				{
					// AnalyzeWriteEa0 and AnalyzeWriteEa1 have the same signature.
					INS_InsertPredicatedCall(ins, IPOINT_BEFORE, AFUNPTR(aFUNPTRmrW),
						IARG_FAST_ANALYSIS_CALL,
						IARG_REG_VALUE, lstate.bufferEndReg,
						IARG_ADDRINT, ac.offset - act.GetTraceSize(),
						IARG_MEMORYWRITE_EA,
						IARG_MEMORYWRITE_SIZE,
						IARG_END);
				}

				if (ac.mType == BUFFER_ENTRY_TYPE_MR0)
				{
					if (INS_HasFallThrough(ins))
					{
						INS_InsertPredicatedCall(ins, IPOINT_AFTER, AFUNPTR(AnalyzeWrite0),
							IARG_FAST_ANALYSIS_CALL,
							IARG_REG_VALUE, lstate.bufferEndReg,
							IARG_ADDRINT, ac.offset - act.GetTraceSize(),
							IARG_END);
					}

					if (INS_IsBranchOrCall(ins))
					{
						INS_InsertPredicatedCall(ins, IPOINT_TAKEN_BRANCH, AFUNPTR(AnalyzeWrite0),
							IARG_FAST_ANALYSIS_CALL,
							IARG_REG_VALUE, lstate.bufferEndReg,
							IARG_ADDRINT, ac.offset - act.GetTraceSize(),
							IARG_END);
					}
				}

				if (g_v2pTransMode == V2P_TRANS_MODE_ACCURATE && ac.mType != BUFFER_ENTRY_TYPE_MR2)
				{
					INS_InsertPredicatedCall(ins, IPOINT_BEFORE, AFUNPTR(AnalyzePinPageOfAddress1),
						IARG_FAST_ANALYSIS_CALL,
						IARG_MEMORYWRITE_EA,
						IARG_END);
				}

				ac.offset += BufferTypeToSize(ac.mType);
			}

			if (INS_HasMemoryRead2(ins))
			{
				switch (ac.mType)
				{
				case BUFFER_ENTRY_TYPE_MR0:
					INS_InsertPredicatedCall(ins, IPOINT_BEFORE, AFUNPTR(Analyze2Read0),
						IARG_FAST_ANALYSIS_CALL,
						IARG_REG_VALUE, lstate.bufferEndReg,
						IARG_ADDRINT, ac.offset - act.GetTraceSize(),
						IARG_MEMORYREAD_EA,
						IARG_MEMORYREAD2_EA,
						IARG_MEMORYREAD_SIZE,
						IARG_END);
					break;
				case BUFFER_ENTRY_TYPE_MR1:
					INS_InsertPredicatedCall(ins, IPOINT_BEFORE, AFUNPTR(Analyze2Read1),
						IARG_FAST_ANALYSIS_CALL,
						IARG_REG_VALUE, lstate.bufferEndReg,
						IARG_ADDRINT, ac.offset - act.GetTraceSize(),
						IARG_MEMORYREAD_EA,
						IARG_MEMORYREAD2_EA,
						IARG_MEMORYREAD_SIZE,
						IARG_END);
					break;
				case BUFFER_ENTRY_TYPE_MR2:
					INS_InsertPredicatedCall(ins, IPOINT_BEFORE, AFUNPTR(AnalyzeRead2),
						IARG_FAST_ANALYSIS_CALL,
						IARG_REG_VALUE, lstate.bufferEndReg,
						IARG_ADDRINT, ac.offset - act.GetTraceSize(),
						IARG_END);
					break;
				}

				if (g_v2pTransMode == V2P_TRANS_MODE_ACCURATE && ac.mType != BUFFER_ENTRY_TYPE_MR2)
				{
					INS_InsertPredicatedCall(ins, IPOINT_BEFORE, AFUNPTR(AnalyzePinPageOfAddress2),
						IARG_FAST_ANALYSIS_CALL,
						IARG_MEMORYREAD_EA,
						IARG_MEMORYREAD2_EA,
						IARG_END);
				}
			}
			else if (INS_IsMemoryRead(ins))
			{
				switch (ac.mType)
				{
				case BUFFER_ENTRY_TYPE_MR0:
					INS_InsertPredicatedCall(ins, IPOINT_BEFORE, AFUNPTR(AnalyzeRead0),
						IARG_FAST_ANALYSIS_CALL,
						IARG_REG_VALUE, lstate.bufferEndReg,
						IARG_ADDRINT, ac.offset - act.GetTraceSize(),
						IARG_MEMORYREAD_EA,
						IARG_MEMORYREAD_SIZE,
						IARG_END);
					break;
				case BUFFER_ENTRY_TYPE_MR1:
					INS_InsertPredicatedCall(ins, IPOINT_BEFORE, AFUNPTR(AnalyzeRead1),
						IARG_FAST_ANALYSIS_CALL,
						IARG_REG_VALUE, lstate.bufferEndReg,
						IARG_ADDRINT, ac.offset - act.GetTraceSize(),
						IARG_MEMORYREAD_EA,
						IARG_MEMORYREAD_SIZE,
						IARG_END);
					break;
				case BUFFER_ENTRY_TYPE_MR2:
					INS_InsertPredicatedCall(ins, IPOINT_BEFORE, AFUNPTR(AnalyzeRead2),
						IARG_FAST_ANALYSIS_CALL,
						IARG_REG_VALUE, lstate.bufferEndReg,
						IARG_ADDRINT, ac.offset - act.GetTraceSize(),
						IARG_END);
					break;
				}

				if (g_v2pTransMode == V2P_TRANS_MODE_ACCURATE && ac.mType != BUFFER_ENTRY_TYPE_MR2)
				{
					INS_InsertPredicatedCall(ins, IPOINT_BEFORE, AFUNPTR(AnalyzePinPageOfAddress1),
						IARG_FAST_ANALYSIS_CALL,
						IARG_MEMORYREAD_EA,
						IARG_END);
				}
			}
		}

		if (g_v2pTransMode == V2P_TRANS_MODE_ACCURATE)
		{
			INS_InsertPredicatedCall(ins, IPOINT_BEFORE, AFUNPTR(AnalyzePinPageOfAddress1),
				IARG_FAST_ANALYSIS_CALL,
				IARG_INST_PTR,
				IARG_END);
		}
	}
}

BOOL PIN_FAST_ANALYSIS_CALL CanBufferNotAccommodateTrace(
	ADDRINT memRefBufferEnd, ADDRINT memRefBufferLast,
	ADDRINT insRecordBufferEnd, ADDRINT insRecordBufferLast,
	UINT32 memRefRequired, UINT32 insRecorRequired)
{
	return FALSE;
	//return ((memRefBufferEnd + memRefRequired) >= memRefBufferLast) ||
	//	((insRecordBufferEnd + insRecorRequired) >= insRecordBufferLast);
}

VOID PIN_FAST_ANALYSIS_CALL EnqueFullAndGetFree(
	VOID **bufferEnd,
	VOID **bufferLast,
	THREADID tid)
{
	APP_THREAD_MANAGER *atm = static_cast<APP_THREAD_MANAGER*>(PIN_GetThreadData(lstate.appThreadManagerKey, tid));
	atm->EnqueFullAndGetFree(bufferEnd, bufferLast);
}

VOID PIN_FAST_ANALYSIS_CALL EnqueIfFullAndGetFree(
	VOID **bufferEnd,
	VOID **bufferLast,
	THREADID tid,
	UINT32 requiredSize)
{
	if (((ADDRINT)*bufferEnd + requiredSize) > (ADDRINT)*bufferLast)
	{
		APP_THREAD_MANAGER *atm = static_cast<APP_THREAD_MANAGER*>(PIN_GetThreadData(lstate.appThreadManagerKey, tid));
		if (atm->m_bufferSize < requiredSize)
		{
			ALLERIA_WriteError(ALLERIA_ERROR_KNOB_BufferSizeTooSmall);
		}
		atm->EnqueFullAndGetFree(bufferEnd, bufferLast);
	}
}

VOID PIN_FAST_ANALYSIS_CALL  AllocateSpaceForTraceInBuffer(
	ADDRINT *bufferEnd,
	UINT32 size)
{
	*bufferEnd = *bufferEnd + size;
}

VOID InstrumentBufferAllocationForTrace(TRACE trace,
	UINT32 requiredBufferSize)
{
	//TRACE_InsertIfCall(trace, IPOINT_BEFORE, (AFUNPTR)CanBufferNotAccommodateTrace,
	//	IARG_FAST_ANALYSIS_CALL,
	//	IARG_REG_VALUE, lstate.memRefBufferEndReg,
	//	IARG_REG_VALUE, lstate.memRefBufferLastReg,
	//	IARG_REG_VALUE, lstate.insRecordBufferEndReg,
	//	IARG_REG_VALUE, lstate.insRecordBufferLastReg,
	//	IARG_UINT32, requiredMemRefBufferSize,
	//	IARG_UINT32, requiredInsRecordBufferSize,
	//	IARG_END);

	//TRACE_InsertThenCall(trace, IPOINT_BEFORE, (AFUNPTR)EnqueFullAndGetFree,
	//	IARG_FAST_ANALYSIS_CALL,
	//	IARG_REG_REFERENCE, lstate.memRefBufferEndReg,
	//	IARG_REG_REFERENCE, lstate.memRefBufferLastReg,
	//	IARG_REG_REFERENCE, lstate.insRecordBufferEndReg,
	//	IARG_REG_REFERENCE, lstate.insRecordBufferLastReg,
	//	IARG_THREAD_ID,
	//	IARG_END);

	// Ensure that these come before any other analysis routine.

	TRACE_InsertCall(trace, IPOINT_BEFORE, (AFUNPTR)EnqueIfFullAndGetFree,
		IARG_FAST_ANALYSIS_CALL,
		IARG_REG_REFERENCE, lstate.bufferEndReg,
		IARG_REG_REFERENCE, lstate.bufferLastReg,
		IARG_THREAD_ID,
		IARG_UINT32, requiredBufferSize,
		IARG_END);

	TRACE_InsertCall(trace, IPOINT_BEFORE, (AFUNPTR)AllocateSpaceForTraceInBuffer,
		IARG_FAST_ANALYSIS_CALL,
		IARG_REG_REFERENCE, lstate.bufferEndReg,
		IARG_UINT32, requiredBufferSize,
		IARG_END);
}

VOID IntrumentRoutineOfTrace(TRACE routineFirstTrace)
{
	if (RTN_Valid(TRACE_Rtn(routineFirstTrace)) &&
		WINDOW_CONFIG::IsFuncInteresting(
		PIN_UndecorateSymbolName(RTN_Name(TRACE_Rtn(routineFirstTrace)), UNDECORATION::UNDECORATION_NAME_ONLY)))
	{
		WINDOW_CONFIG::ReportFunctionCall(TRACE_Rtn(routineFirstTrace));
		/*TRACE_InsertCall(routineFirstTrace, IPOINT_BEFORE, (AFUNPTR)ANALYSIS_ROUTINES::AnalyzeInterestingFuncCall,
			IARG_FAST_ANALYSIS_CALL,
			IARG_ADDRINT, TRACE_Address(routineFirstTrace),
			IARG_END);*/
	}
}

VOID InstrumentTrace(TRACE trace, VOID *arg)
{
	UINT32 requiredBufferSize = 0;
	ANALYSIS_CALLS_TRACE act;

	IntrumentRoutineOfTrace(trace);

	//RTN rtn = TRACE_Rtn(trace);
	//if (RTN_Valid(rtn))
	//{
	//	std::string name = RTN_Name(rtn);
	//	if (name == "_memset")
	//	{
	//		ADDRINT addr = RTN_Address(rtn);
	//		//"RtlCreateHeap\n";
	//	}
	//}

	for (BBL bbl = TRACE_BblHead(trace); BBL_Valid(bbl); bbl = BBL_Next(bbl))
	{
		for (INS ins = BBL_InsHead(bbl); INS_Valid(ins); ins = INS_Next(ins))
		{
			//if (INS_IsPrefetch(ins))
			//{
			//	string name = RTN_Name(RTN_FindByAddress(INS_Address(ins)));
			//}
			//std::string instr = INS_Disassemble(ins);

			WINDOW_CONFIG::INS_CONFIG insConfig;
			WINDOW_CONFIG::Match(ins, insConfig);
			BUFFER_ENTRY_TYPE iType;
			BUFFER_ENTRY_TYPE mType;
			if (insConfig.valid)
			{
				if (insConfig.timeStamp)
					iType = BUFFER_ENTRY_TYPE_IR0;
				else if (insConfig.routineId)
					iType = BUFFER_ENTRY_TYPE_IR1;
				else
					iType = BUFFER_ENTRY_TYPE_IR2;

				if (insConfig.accessValue)
					mType = BUFFER_ENTRY_TYPE_MR0;
				else if (insConfig.accessAddressAndSize)
					mType = BUFFER_ENTRY_TYPE_MR1;
				else
					mType = BUFFER_ENTRY_TYPE_MR2;

				if (INS_IsVgather(ins))
				{
					requiredBufferSize += act.RecordMemCall(ins, iType, mType, 1);
				}
				else
				{
					UINT32 count = INS_HasMemoryRead2(ins) ? 2 : (INS_IsMemoryRead(ins) ? 1 : 0);
					count += (INS_IsMemoryWrite(ins) ? 1 : 0);
					xed_decoded_inst_t *xedInst = INS_XedDec(ins);
					xed_iclass_enum_t xedInstclass = xed_decoded_inst_get_iclass(xedInst);

					if (count > 0 && INS_IsStandardMemop(ins))
					{
						requiredBufferSize += act.RecordMemCall(ins, iType, mType, count);

						iType = BUFFER_ENTRY_TYPE_INVALID;
						if (INS_IsBranch(ins))
						{
							mType = BUFFER_ENTRY_TYPE_BRANCH_RECORD;
							requiredBufferSize += act.RecordCall(ins, iType, mType);
						}
						else if (INS_IsSyscall(ins))
						{
							mType = BUFFER_ENTRY_TYPE_SYS_CALL_RECORD;
							requiredBufferSize += act.RecordCall(ins, iType, mType);
						}
					}
					else if (xedInstclass == XED_ICLASS_XSAVE || xedInstclass == XED_ICLASS_XSAVE64 ||
						xedInstclass == XED_ICLASS_XSAVEOPT || xedInstclass == XED_ICLASS_XSAVEOPT64)
					{
						// 1- Read XSTATE_BV field of the XSAVE area header.
						// 2- Write the specified state components.
						count = 2;
						requiredBufferSize += act.RecordMemCall(ins, iType, mType, count);
					}
					else if (xedInstclass == XED_ICLASS_XSAVEC || xedInstclass == XED_ICLASS_XSAVEC64 ||
						xedInstclass == XED_ICLASS_XRSTOR || xedInstclass == XED_ICLASS_XRSTOR64)
					{
						// Read or write the specified state components.
						count = 1;
						requiredBufferSize += act.RecordMemCall(ins, iType, mType, count);
					}
					else if (INS_IsBranch(ins))
					{
						mType = BUFFER_ENTRY_TYPE_BRANCH_RECORD;
						requiredBufferSize += act.RecordCall(ins, iType, mType);
					}
					else if (INS_IsSyscall(ins))
					{
						mType = BUFFER_ENTRY_TYPE_SYS_CALL_RECORD;
						requiredBufferSize += act.RecordCall(ins, iType, mType);
					}
					else
					{
						mType = BUFFER_ENTRY_TYPE_INVALID;
						requiredBufferSize += act.RecordCall(ins, iType, mType);
					}
				}
			}
		}

		//BBL_InsertCall(bbl, IPOINT_BEFORE, (AFUNPTR)AnalyzeInsCount,
		//	IARG_FAST_ANALYSIS_CALL,
		//	IARG_UINT32, BBL_NumIns(bbl), 
		//	IARG_UINT32, memInsCount,
		//	IARG_END);
	}

	if (act.m_calls.size() > 0)
	{
		InstrumentBufferAllocationForTrace(trace, requiredBufferSize + 
			(KnobTrackProcessorsEnabled ? sizeof(PROCESSOR_RECORD) : 0));
		InstrumentMemIns(act);
	}
}

VOID InstrumentImage(IMG img, VOID *arg)
{
	WINDOW_CONFIG::ReportImageLoad(img);
	if (!IMG_IsStaticExecutable(img)) // What is a static executable?
		IMAGE_COLLECTION::AddImage(img);
}

VOID InstrumentImageUnload(IMG img, VOID *arg)
{
	IMAGE_COLLECTION::UnloadImage(img);
	WINDOW_CONFIG::ReportImageUnload(img);
}

void RegisterTEB()
{
	void *addr = GetThreadSystemInfoAddr();
	if (addr != NULL)
		MEMORY_REGISTRY::RegisterGeneral(
		reinterpret_cast<ADDRINT>(addr), REGION_USER::Platform);
}


void RegisterProcessSystemBlocks()
{
	void *addr = GetProcessSystemInfoAddr();
	if (addr != NULL)
	{
		MEMORY_REGISTRY::RegisterGeneral(reinterpret_cast<ADDRINT>(addr), REGION_USER::Platform);
	}

	void *heapId;
	GetProcessDefaultHeapAddr(&addr, &heapId);

	if (addr != NULL)
	{
		MEMORY_REGISTRY::RegisterHeap(
			reinterpret_cast<ADDRINT>(addr),
			reinterpret_cast<ADDRINT>(heapId),
			REGION_USER::Platform);
	}
}

VOID ProcessingThreadRegister()
{
	THREADID tid = PIN_ThreadId();
	UINT32 dummy;
	MEMORY_REGISTRY::RegisterStack(reinterpret_cast<ADDRINT>(&dummy), PIN_GetTid(), REGION_USER::Alleria);

	RegisterTEB();
}

VOID ProcessingThreadUnregister()
{
	THREADID tid = PIN_ThreadId();
	UINT32 dummy;
	MEMORY_REGISTRY::Unregister(reinterpret_cast<ADDRINT>(&dummy));
}

VOID WriteBinaryFuncImgSections(BOOL isAppThread)
{
	std::map<UINT32, UINT32> *routineMap;
	std::ofstream *ofs;
	if (isAppThread)
	{
		routineMap = &GLOBAL_STATE::gRoutineMap;
		ofs = &GLOBAL_STATE::gfstream;
	}
	else
	{
		routineMap = reinterpret_cast<std::map<UINT32, UINT32>*>
			(PIN_GetThreadData(lstate.routineMapKey, PIN_ThreadId()));
		ofs = reinterpret_cast<std::ofstream*>
			(PIN_GetThreadData(lstate.ofstreamKey, PIN_ThreadId()));
		if (ofs == NULL)
			return; // This processing thread didn't process any buffers.
	}

	// Terminate the profile section.
	BUFFER_ENTRY_TYPE bet = BUFFER_ENTRY_TYPE_INVALID;
	WriteBinary((*ofs), bet);

	UINT64 funcSecOffset = 0, imgSecOffset = 0;
	std::map<UINT32, UINT32> imageMap;
	std::string name;
	std::vector<pair<UINT32, UINT32>> routineInfo;
	std::vector<pair<UINT32, UINT32>> imageInfo;
	UINT32 imgIndex;
	char nul = 0;
	UINT32 zero = 0;

	routineInfo.push_back(std::make_pair(0, 0)); // An unkown routine should be in the first entry.

	for (auto itr = routineMap->begin(); itr != routineMap->end(); ++itr)
	{
		routineInfo.push_back(*itr);
		routineMap->erase(itr); // Save memory.
	}

	sort(routineInfo.begin(), routineInfo.end(), [=](const pair<UINT32, UINT32>& a, const pair<UINT32, UINT32>& b)
	{
		return a.second < b.second;
	}
	);

	funcSecOffset = ofs->tellp();

	// Write functions table section header.

	UINT32 size = (UINT32)routineInfo.size();
	WriteBinary((*ofs), size);
	if (isAppThread) // Img section directory entry index.
	{
		WriteBinary((*ofs), g_binSecDirImgIndexMain);
	}
	else
	{
		WriteBinary((*ofs), g_binSecDirImgIndexSub);
	}

	imageInfo.push_back(std::make_pair(0, 0)); // An unkown image should be in the first entry.

	// Write functions table section entries.

	for (std::vector<pair<UINT32, UINT32>>::const_iterator it = routineInfo.begin(); it != routineInfo.end(); ++it)
	{
		UINT32 imgId = IMAGE_COLLECTION::GetRoutineInfo(it->first, name);
		if (imgId != 0)
		{
			std::map<UINT32, UINT32>::const_iterator it = imageMap.find(imgId);
			if (it == imageMap.end())
			{
				imgIndex = (UINT32)imageMap.size() + 1;
				imageMap.insert(std::make_pair(imgId, imgIndex));
			}
			else
			{
				imgIndex = it->second;
			}
		}
		else
		{
			imgIndex = 0;
		}

		*ofs << name;
		WriteBinary((*ofs), nul);
		WriteBinary((*ofs), imgIndex);
	}

	for (auto itr = imageMap.begin(); itr != imageMap.end(); ++itr)
	{
		imageInfo.push_back(*itr);
		imageMap.erase(itr); // Save memory.
	}

	sort(imageInfo.begin(), imageInfo.end(), [=](const pair<UINT32, UINT32>& a, const pair<UINT32, UINT32>& b)
	{
		return a.second < b.second;
	}
	);

	imgSecOffset = ofs->tellp();

	// Write images table section header.

	size = (UINT32)imageInfo.size();
	WriteBinary((*ofs), size);

	// Write images table section entries.

	for (UINT32 i = 0; i < imageInfo.size(); ++i)
	{
		IMAGE_COLLECTION::GetImageInfo(imageInfo[i].first, name);
		*ofs << name;
		WriteBinary((*ofs), nul);
	}

	// Patch functions and images table section offsets in the section directory.

	GLOBAL_STATE::PatchFuncImgSecOffsets(*ofs, isAppThread, funcSecOffset, imgSecOffset);
}

VOID ThreadStart(THREADID tid, CONTEXT *ctxt, INT32 flags, VOID *arg)
{
	WINDOW_CONFIG::ReportThreadCeation();

	UINT8 *buff = new UINT8[REG_Size(REG_STACK_PTR)];
	PIN_GetContextRegval(ctxt, REG_STACK_PTR, reinterpret_cast<UINT8*>(buff));
	MEMORY_REGISTRY::RegisterStack(reinterpret_cast<ADDRINT>(*reinterpret_cast<VOID**>(buff)),
		PIN_GetTid(), REGION_USER::App);
	MEMORY_REGISTRY::RegisterGeneral(reinterpret_cast<ADDRINT>(ctxt), REGION_USER::Pin);

	RegisterTEB();

	TIME_STAMP timestamp;
	GetCurrentTimestamp(timestamp);
	PIN_SetThreadData(lstate.startingTimeHighKey, reinterpret_cast<VOID*>(timestamp >> 32), tid);
	PIN_SetThreadData(lstate.startingTimeLowKey, reinterpret_cast<VOID*>(timestamp & 0xFFFFFFFF), tid);

	if (KnobTunerEnabled)
	{
		// Create a buffer before creating APP_THREAD_MANAGER.
		// Helps preventing a deadlock.
		GLOBAL_STATE::freeBuffersListManager.AddNewFreeBuffer(tid);
	}

#ifdef TARGET_WINDOWS
	// Get rid of Warning C4316 'APP_THREAD_MANAGER': object allocated on the heap may not be aligned 64
	APP_THREAD_MANAGER *atmtemp = (APP_THREAD_MANAGER*)memalign(64, sizeof(APP_THREAD_MANAGER));
	APP_THREAD_MANAGER *atm = new(atmtemp) APP_THREAD_MANAGER(tid, KnobNumKBBuffer * 1024);
#else
	APP_THREAD_MANAGER *atm = new APP_THREAD_MANAGER(tid, KnobNumKBBuffer * 1024);
#endif /* TARGET_WINDOWS */
	PIN_SetThreadData(lstate.appThreadManagerKey, atm, tid);

	PIN_SetContextReg(ctxt, lstate.bufferEndReg,
		reinterpret_cast<ADDRINT>(atm->GetCurrentBuffer()));
	PIN_SetContextReg(ctxt, lstate.bufferLastReg,
		reinterpret_cast<ADDRINT>(atm->GetCurrentBufferLast()));

	InterceptorMemoryManager *memMan = new InterceptorMemoryManager;
	PIN_SetContextReg(ctxt, lstate.interceptorMemManReg,
		reinterpret_cast<ADDRINT>(memMan));
}

VOID ThreadFini(THREADID threadIndex, const CONTEXT *ctxt, INT32 code, VOID *arg)
{
	UINT8 *buff = new UINT8[REG_Size(lstate.interceptorMemManReg)];
	PIN_GetContextRegval(ctxt, lstate.interceptorMemManReg, buff);
	InterceptorMemoryManager *memMan = *reinterpret_cast<InterceptorMemoryManager**>(buff);
	if (g_outputFormat == OUTPUT_FORMAT_MODE_TEXTUAL)
		memMan->DumpToFile(GLOBAL_STATE::interceptorFileStreamText);
	else if (g_outputFormat == OUTPUT_FORMAT_MODE_BINARY)
		memMan->DumpToFileBin(GLOBAL_STATE::interceptorFileStreamBinary);
	delete memMan;

	APP_THREAD_MANAGER *atm = reinterpret_cast<APP_THREAD_MANAGER*>(
		PIN_GetThreadData(lstate.appThreadManagerKey, threadIndex));

	while (!atm->AreAllBuffersProcessed())
	{
		PIN_Sleep(1);
	}

	// Process the buffer that was last used, but didn't get full.
	UINT64 readsCount = 0;
	UINT64 writesCount = 0;
	UINT64 insCount = 0;
	UINT64 memInsCount = 0;
	UINT32 unusedBytes;
	VOID *bufferEnd = (VOID*)PIN_GetContextReg(ctxt, lstate.bufferEndReg);

	BUFFER_STATE bufferState;
	bufferState.m_appThreadManager = atm;
	bufferState.m_bufferStart = atm->GetCurrentBuffer();
	bufferState.m_bufferEnd = bufferEnd;

	PIN_GetLock(&GLOBAL_STATE::gAppThreadLock, threadIndex + 1);
	if (g_outputFormat == OUTPUT_FORMAT_MODE_TEXTUAL)
	{
		ProcessBuffer(bufferState,
			GLOBAL_STATE::gfstream, &readsCount, &writesCount, &insCount, &memInsCount,
			unusedBytes);
	}
	else if (g_outputFormat == OUTPUT_FORMAT_MODE_BINARY)
	{
		ProcessBufferBinary(
			bufferState, true,
			atm->GetThreadUid(),
			&readsCount, &writesCount, &insCount, &memInsCount,
			unusedBytes);
	}
	PIN_ReleaseLock(&GLOBAL_STATE::gAppThreadLock);

	// This is an app thread. Ignore unusedBytesInsRecord and unusedBytesMemRef.

	atm->AddInsCount(insCount);
	atm->AddMemInsCount(memInsCount);
	atm->AddReads(readsCount);
	atm->AddWrites(writesCount);

	GLOBAL_STATE::insCount += atm->GetInsCount();
	GLOBAL_STATE::insMemCount += atm->GetMemInsCount();

	if (atm->GetReadsCount() + atm->GetWritesCount() != 0)
		WINDOW_CONFIG::ReportAccesses(atm->GetThreadUid(), atm->GetReadsCount(), atm->GetWritesCount());
	if (atm->GetInsCount() != 0)
		WINDOW_CONFIG::ReportInstructions(atm->GetThreadUid(), atm->GetInsLastCount());

	// Recycle the buffer to be used by other threads.
	GLOBAL_STATE::freeBuffersListManager.AddFreeBuffer(atm->GetCurrentBuffer(), threadIndex);

	TIME_STAMP wt = atm->GetWaitingTime();
	++GLOBAL_STATE::totalAppThreadCount;
	GLOBAL_STATE::totalAppThreadWaitingTime += wt;

	PIN_SetThreadData(lstate.appThreadManagerKey, NULL, threadIndex);
	delete atm;

	buff = new UINT8[REG_Size(REG_STACK_PTR)];
	PIN_GetContextRegval(ctxt, REG_STACK_PTR, reinterpret_cast<UINT8*>(buff));
	MEMORY_REGISTRY::Unregister(reinterpret_cast<ADDRINT>(*reinterpret_cast<VOID**>(buff)));

	WINDOW_CONFIG::ReportThreadTermination(threadIndex);
}

VOID ProcessTunerReact()
{
	if (KnobTunerEnabled)
	{
		PIN_THREAD_UID tuidtc = ATOMIC::OPS::Load(&GLOBAL_STATE::tuidTunerControl);
		if (PIN_ThreadUid() == tuidtc)
		{
			ATOMIC::OPS::Store(&GLOBAL_STATE::tuidTunerControl, (PIN_THREAD_UID)0);

			// Notify the tuner that a proc thread has suspended.
			PIN_SemaphoreSet(&GLOBAL_STATE::tunerSemaphore);

			while (TRUE)
			{
				BOOL action = ATOMIC::OPS::Load(&GLOBAL_STATE::tuidTunerControlResume);
				tuidtc = ATOMIC::OPS::Load(&GLOBAL_STATE::tuidTunerControl);
				if (action && (PIN_ThreadUid() == tuidtc))
				{
					break; // Tuner said resume processing.
				}
				else
				{
					PIN_Sleep(KnobTimerFreq); // Tuner said suspend processing.
				}
			}

			ATOMIC::OPS::Store(&GLOBAL_STATE::tuidTunerControl, (PIN_THREAD_UID)0);

			// Notify the tuner that a proc thread has resumed.
			PIN_SemaphoreSet(&GLOBAL_STATE::tunerSemaphore);
		}
	}
}

VOID Process(VOID *arg)
{
	GLOBAL_STATE::processingThreadRunning = TRUE;

	ProcessingThreadRegister();
	THREADID tid = PIN_ThreadId();
	PIN_THREAD_UID tuid = PIN_ThreadUid();

	TIME_STAMP timestamp;
	GetCurrentTimestamp(timestamp);
	PIN_SetThreadData(lstate.startingTimeHighKey, reinterpret_cast<VOID*>(timestamp >> 32), tid);
	PIN_SetThreadData(lstate.startingTimeLowKey, reinterpret_cast<VOID*>(timestamp & 0xFFFFFFFF), tid);

	std::map<UINT32, UINT32> *routineMap = new std::map<UINT32, UINT32>;
	PIN_SetThreadData(lstate.routineMapKey, reinterpret_cast<VOID*>(routineMap), tid);

	TIME_STAMP idleTime = 0, idleTimePart;
	TIME_STAMP idleTimeStarting = 0;
	TIME_STAMP idleTimeEnding = 0;
	UINT64 unusedBytesAcc = 0;
	UINT64 bufferCount = 0;

	std::ofstream *fstream = reinterpret_cast<std::ofstream*>(PIN_GetThreadData(lstate.ofstreamKey, tid));
	ASSERTX(fstream == NULL);
	ASSERTX(g_outputFormat != OUTPUT_FORMAT_MODE_INVALID);

	std::string fullPathStr;
	std::string temp;
	IntegerToString(tid, temp);
	PathCombine(fullPathStr, GLOBAL_STATE::s_myDumpDirectory, temp + ".ap");
	if (g_outputFormat == OUTPUT_FORMAT_MODE_TEXTUAL)
	{
		fstream = new std::ofstream(fullPathStr.c_str(), GLOBAL_STATE::fomodetxt);

		if (!(*fstream).good())
		{
			ALLERIA_WriteError(ALLERIA_ERROR_CANNOT_CREATE_PROFILE);
		}
		*fstream << std::hex;
	}
	else // g_outputFormat == OUTPUT_FORMAT_MODE_BINARY
	{
		fstream = new std::ofstream(fullPathStr.c_str(), GLOBAL_STATE::fomodebin);

		if (!(*fstream).good())
		{
			ALLERIA_WriteError(ALLERIA_ERROR_CANNOT_CREATE_PROFILE);
		}
		GLOBAL_STATE::WriteBinaryHeader(*fstream, BINARY_HEADER_TYPE_SUB_PROFILE);
		GLOBAL_STATE::WriteBinaryProfile(*fstream, BINARY_HEADER_TYPE_SUB_PROFILE);
	}

	PIN_GetLock(&GLOBAL_STATE::gProcThreadLock, tid + 1);
	GLOBAL_STATE::localStreams.push_back(fstream);
	PIN_ReleaseLock(&GLOBAL_STATE::gProcThreadLock);

	PIN_SetThreadData(lstate.ofstreamKey, fstream, tid);

	// Notify the tuner that a proc thread has started.
	PIN_SemaphoreSet(&GLOBAL_STATE::tunerSemaphore);

	while (!GLOBAL_STATE::isAppExisting)
	{
		// Check whether the tuner has beef with me.
		ProcessTunerReact();

		BUFFER_STATE bufferState;
		GetCurrentTimestamp(idleTimeStarting);
		BOOL success = GLOBAL_STATE::fullBuffersListManager.AcquireFullBuffer(bufferState, tid);
		GetCurrentTimestamp(idleTimeEnding);
		GetTimeSpan(idleTimeStarting, idleTimeEnding, TIME_UNIT::Microseconds, idleTimePart);
		idleTime += idleTimePart;
		ATOMIC::OPS::Increment(&GLOBAL_STATE::totalProcessingThreadRunningWaitingTime, idleTimePart);
		if (!success)
		{
			ASSERTX(GLOBAL_STATE::isAppExisting);
			break;
		}

		UINT64 readsCount = 0;
		UINT64 writesCount = 0;
		UINT64 insCount = 0;
		UINT64 memInsCount = 0;
		UINT32 unusedBytes;
		APP_THREAD_MANAGER *atm = bufferState.m_appThreadManager;
		if (g_outputFormat == OUTPUT_FORMAT_MODE_TEXTUAL)
		{
			ProcessBuffer(bufferState,
				*fstream, &readsCount, &writesCount, &insCount, &memInsCount,
				unusedBytes);
		}
		else if (g_outputFormat == OUTPUT_FORMAT_MODE_BINARY)
		{
			ProcessBufferBinary(
				bufferState, false,
				atm->GetThreadUid(),
				&readsCount, &writesCount, &insCount, &memInsCount,
				unusedBytes);
		}

		unusedBytesAcc += unusedBytes;
		bufferCount += (insCount > 0 ? 1 : 0);

		atm->AddInsCount(insCount);
		atm->AddMemInsCount(memInsCount);
		atm->AddReads(readsCount);
		atm->AddWrites(writesCount);
		WINDOW_CONFIG::ReportAccesses(atm->GetThreadUid(), atm->GetReadsCount(), atm->GetWritesCount());
		WINDOW_CONFIG::ReportInstructions(atm->GetThreadUid(), atm->GetInsLastCount());

		GLOBAL_STATE::freeBuffersListManager.AddFreeBuffer(bufferState.m_bufferStart, tid);
		atm->DecrementPendingFullBufferCounter();
	}

	// Check whether the tuner has beef with me.
	ProcessTunerReact();

	TIME_STAMP endingTs, totalTime;
	GetCurrentTimestamp(endingTs);
	GetTimeSpan(timestamp, endingTs, TIME_UNIT::Microseconds, totalTime);
	PROCESSING_THREAD_STATS *stats = new PROCESSING_THREAD_STATS;
	stats->bufferPairCount = bufferCount;
	stats->idleTimeSpan = idleTime;
	stats->processingTimeSpan = totalTime - idleTime;
	stats->unusedBytes = unusedBytesAcc;
	PIN_SetThreadData(lstate.processingThreadStatsKey, reinterpret_cast<VOID*>(stats), tid);

	if (g_outputFormat == OUTPUT_FORMAT_MODE_BINARY)
		WriteBinaryFuncImgSections(FALSE);

	delete routineMap;

	ProcessingThreadUnregister();
}

//VOID Count(VOID *arg)
//{
//	ProcessingThreadRegister();
//
//	while (!PIN_IsProcessExiting())
//	{
//		PIN_StopApplicationThreads(PIN_ThreadId());
//		UINT32 count = PIN_GetStoppedThreadCount();
//		UINT64 insCount = 0;
//		UINT64 memInsCount = 0;
//		for (UINT32 i = 0; i < count; ++i)
//		{
//			THREADID tid = PIN_GetStoppedThreadId(i);
//			APP_THREAD_MANAGER *atm = reinterpret_cast<APP_THREAD_MANAGER*>(
//				PIN_GetThreadData(lstate.appThreadManagerKey, tid));
//			insCount += atm->GetInsCount();
//			memInsCount += atm->GetMemInsCount();
//		}
//
//		PIN_ResumeApplicationThreads(PIN_ThreadId());
//		PIN_Sleep(1000);
//	}
//
//	ProcessingThreadUnregister();
//}

#ifdef TARGET_WINDOWS
VOID LiveLandsWalker(VOID *arg)
{
	ProcessingThreadRegister();
	std::vector<USIZE> totalMemoryPin;
	USIZE memPinSample;
	size_t regionCount;
	while (!GLOBAL_STATE::isAppExisting)
	{
		std::vector<REGION> regions;
		TIME_STAMP timestamp;
		GetCurrentTimestamp(timestamp);
		WINDOWS_LIVE_LANDS_WALKER::TakeAWalk(regions);

		totalMemoryPin.push_back(PIN_MemoryAllocatedForPin());

		TIME_STAMP ts;
		GetTimeSpan(timestamp, TIME_UNIT::TenthMicroseconds, ts);

		if (g_outputFormat == OUTPUT_FORMAT_MODE_TEXTUAL)
		{
			GLOBAL_STATE::walkerFileStream << "------------------------------------------------\n";
			GLOBAL_STATE::walkerFileStream << "Walker snapshot at " << ts << "\n";
			GLOBAL_STATE::walkerFileStream << "Profiler consumed " << totalMemoryPin.back() << " bytes\n";
		}

		else if (g_outputFormat == OUTPUT_FORMAT_MODE_BINARY)
		{
			// Write LLW section entry header. 
			memPinSample = totalMemoryPin.back();
			regionCount = regions.size();
			WriteBinary(GLOBAL_STATE::walkerFileStream, ts);
			WriteBinary(GLOBAL_STATE::walkerFileStream, memPinSample);
			WriteBinary(GLOBAL_STATE::walkerFileStream, regionCount);
		}

		for (UINT32 i = 0; i < regions.size(); ++i)
		{
			MEMORY_REGISTRY_ENTRY entry;
			entry.baseAddr = reinterpret_cast<ADDRINT>(regions[i].pvBaseAddress);
			ASSERTX(regions[i].dwStorage != MEMORY_REGION_FREE_FLAG);

			if (MEMORY_REGISTRY::Find(reinterpret_cast<ADDRINT>(regions[i].pvBaseAddress), entry))
			{
				std::string temp;
				IntegerToString(entry.id, temp);
				regions[i].usageType = entry.type;
				regions[i].user = entry.user;
				if (regions[i].userId == "")
					regions[i].userId = temp;
			}
			/*else if (regions[i].dwProtection == PAGE_EXECUTE_READWRITE_FLAG)
			{
			// This is just a guess. Pin extensively uses PAGE_EXECUTE_READWRITE_FLAG pages.
				regions[i].usageType = REGION_USAGE_TYPE::General;
				regions[i].user = REGION_USER::Pin;
				regions[i].userId = "0";
			}*/
			else
			{
				regions[i].usageType = REGION_USAGE_TYPE::Unknown;
				regions[i].user = REGION_USER::UnKnown;
				regions[i].userId = "0";
			}

			if (g_outputFormat == OUTPUT_FORMAT_MODE_TEXTUAL)
				GLOBAL_STATE::walkerFileStream << i + 1 << " - " << regions[i].ToString() << "\n";
			else if (g_outputFormat == OUTPUT_FORMAT_MODE_BINARY)
			{
				regions[i].DumpToBinaryStream(GLOBAL_STATE::walkerFileStream);
			}
		}

		if (g_outputFormat == OUTPUT_FORMAT_MODE_TEXTUAL)
			GLOBAL_STATE::walkerFileStream << "------------------------------------------------\n";

		PIN_Sleep(1000);
	}

	GLOBAL_STATE::walkerFileStream.flush();
	ProcessingThreadUnregister();
	WINDOWS_LIVE_LANDS_WALKER::Destroy();
}
#endif /* TARGET_WINDOWS */

#ifdef TARGET_LINUX
VOID LiveLandsWalker(VOID *arg)
{
	ProcessingThreadRegister();

	std::vector<USIZE> totalMemoryPin;
	USIZE memPinSample;
	UINT32 regionCount;
	while (!PIN_IsProcessExiting())
	{
		GLOBAL_STATE::walkerFileStream.flush();
		PIN_Sleep(1000);
	}

	ProcessingThreadUnregister();
}
#endif /* TARGET_LINUX */

THREADID CreatePinThread(ROOT_THREAD_FUNC * pThreadFunc, PIN_THREAD_UID *ptid)
{
	THREADID tid = PIN_SpawnInternalThread(pThreadFunc, NULL, 0, ptid);
	if (tid == INVALID_THREADID)
	{
		AlleriaSetLastError(ALLERIA_ERROR_INIT_FAILED);
	}
	return tid;
}

VOID Timer(VOID *arg)
{
	ProcessingThreadRegister(); 

	Phase lastPhase = Phase::InvalidPhase;
	TunerAction action = TunerAction::None;
	size_t tuidControl = 0;
	TIME_STAMP ptitDebug;
	TIME_STAMP atwtDebug;
	//std::ofstream outdebug("D:\\PinRuns\\mydebug.txt");

	while (!GLOBAL_STATE::isAppExisting)
	{
		TIME_STAMP timestamp;
		GetCurrentTimestamp(timestamp);
		WINDOW_CONFIG::ReportTimestamp(lstate.startingTimeHighKey, lstate.startingTimeLowKey, timestamp);

		if (KnobTunerEnabled &&
			/* Wait until the most recent processing thread, if any, starts running */
			(action == TunerAction::None || PIN_SemaphoreIsSet(&GLOBAL_STATE::tunerSemaphore)) &&
			(GLOBAL_STATE::processingThreadsIds.size() < Tuner::MaxThreadCount))
		{
			PIN_SemaphoreClear(&GLOBAL_STATE::tunerSemaphore);
			action = Tuner::OnTimerInterrupt(
				&GLOBAL_STATE::totalProcessingThreadRunningWaitingTime,
				&GLOBAL_STATE::totalAppThreadRunningWaitingTime,
				/* Make sure to leave at least one processing thread running to prevent deadlock */
				tuidControl + 1 < GLOBAL_STATE::processingThreadsIds.size()
			,
				ptitDebug,
				atwtDebug
			);
			//outdebug << "ptit " << ptitDebug << ", atwt " << atwtDebug << "\n";
			//outdebug.flush();
			Phase phase = Tuner::GetPhase();

			if (action == TunerAction::CreateOrResume)
			{
				PIN_THREAD_UID tuid;
				if (tuidControl == 0)
				{
					THREADID tid = CreatePinThread(Process, &tuid);
					GLOBAL_STATE::processingThreadsIds.push_back(tuid);
					GLOBAL_STATE::freeBuffersListManager.AddNewFreeBuffer(tid);
					ALLERIA_WriteMessage(ALLERIA_REPORT_TUNER_CREATE);
				}
				else
				{
					--tuidControl;
					PIN_THREAD_UID tuid = GLOBAL_STATE::processingThreadsIds[tuidControl];
					ATOMIC::OPS::Store(&GLOBAL_STATE::tuidTunerControl, tuid);
					ATOMIC::OPS::Store(&GLOBAL_STATE::tuidTunerControlResume, (BOOL)TRUE);
					ALLERIA_WriteMessage(ALLERIA_REPORT_TUNER_RESUME);
				}
			}
			else if (action == TunerAction::Suspend)
			{
				ASSERTX(tuidControl < GLOBAL_STATE::processingThreadsIds.size());
				PIN_THREAD_UID tuid = GLOBAL_STATE::processingThreadsIds[tuidControl];
				ATOMIC::OPS::Store(&GLOBAL_STATE::tuidTunerControl, tuid);
				ATOMIC::OPS::Store(&GLOBAL_STATE::tuidTunerControlResume, (BOOL)FALSE);
				++tuidControl;
				ALLERIA_WriteMessage(ALLERIA_REPORT_TUNER_SUSPEND);
			}

			if (lastPhase != phase)
			{
				ALLERIA_WriteMessage(ALLERIA_REPORT_TUNER_PHASE_CHANGE);
				lastPhase = phase;
			}
		}

		PIN_Sleep(KnobTimerFreq);
	}

	// Resume all threads paused by the tuner
	if (KnobTunerEnabled)
	{
		if (action != TunerAction::None)
		{
			while (!PIN_SemaphoreIsSet(&GLOBAL_STATE::tunerSemaphore))
			{
				PIN_Sleep(KnobTimerFreq);
			}
		}

		while (tuidControl > 0)
		{
			PIN_SemaphoreClear(&GLOBAL_STATE::tunerSemaphore);
			--tuidControl;
			PIN_THREAD_UID tuid = GLOBAL_STATE::processingThreadsIds[tuidControl];
			ATOMIC::OPS::Store(&GLOBAL_STATE::tuidTunerControl, tuid);
			ATOMIC::OPS::Store(&GLOBAL_STATE::tuidTunerControlResume, (BOOL)TRUE);
			ALLERIA_WriteMessage(ALLERIA_REPORT_TUNER_RESUME);

			while (!PIN_SemaphoreIsSet(&GLOBAL_STATE::tunerSemaphore))
			{
				PIN_Sleep(KnobTimerFreq);
			}
		}
	}

	ProcessingThreadUnregister();
}

VOID PrepareForFini(VOID *v)
{
	// Gracefully terminate all processing threads.
	GLOBAL_STATE::isAppExisting = TRUE;
	GLOBAL_STATE::processingThreadRunning = FALSE; // Prevent a deadlock from happening in EnqueFullAndGetFree.
	PIN_WaitForThreadTermination(GLOBAL_STATE::walkerthreadUID, PIN_INFINITE_TIMEOUT, NULL);
	PIN_WaitForThreadTermination(GLOBAL_STATE::timerthreadUID, PIN_INFINITE_TIMEOUT, NULL);

	if (g_outputFormat == OUTPUT_FORMAT_MODE_BINARY)
		GLOBAL_STATE::PatchBinaryHeader(); // Emit ending time.

	ALLERIA_WriteMessage(ALLERIA_REPORT_FINALIZING);

	// It is safe to access processingThreadsIds after the timer thread has terminated.
	for (UINT32 i = 0; i < GLOBAL_STATE::processingThreadsIds.size(); i++)
	{
		GLOBAL_STATE::fullBuffersListManager.SignalTermination();
	}

	INT32 threadExitCode;
	BOOL waitStatus;
	for (UINT32 i = 0; i < GLOBAL_STATE::processingThreadsIds.size(); i++)
	{
		waitStatus = PIN_WaitForThreadTermination(GLOBAL_STATE::processingThreadsIds[i], PIN_INFINITE_TIMEOUT, &threadExitCode);
		if (!waitStatus)
		{
			ALLERIA_WriteError(ALLERIA_ERROR_THREAD_TERMINATION_FAILED);
		}
	}

	// ThreadFini will now be called for each app thread.
}

VOID CreateWalkerThread()
{
	CreatePinThread(LiveLandsWalker, &GLOBAL_STATE::walkerthreadUID);
}

VOID CreateTimerThread()
{
	CreatePinThread(Timer, &GLOBAL_STATE::timerthreadUID);
}

//VOID CreateCountingThread()
//{
//	PIN_THREAD_UID tuid;
//	THREADID tid = PIN_SpawnInternalThread(Count, NULL, 0, &tuid);
//	if (tid == INVALID_THREADID)
//	{
//		AlleriaSetLastError(ALLERIA_ERROR_INIT_FAILED);
//	}
//}

VOID CreateProcessingThreads(UINT32 n)
{
	for (UINT32 i = 0; i < n; ++i)
	{
		PIN_THREAD_UID tuid;
		THREADID tid = CreatePinThread(Process, &tuid);
		GLOBAL_STATE::processingThreadsIds.push_back(tuid);
	}
}

VOID CreateAlleriaThreads()
{
	CreateProcessingThreads(KnobTunerEnabled ? Tuner::InitialThreadCount : KnobNumProcessingThreads);

	if (KnobLLWEnabled)
		CreateWalkerThread();

	if (KnobTimerEnabled)
		CreateTimerThread();
}

BOOL FollowChild(CHILD_PROCESS childProcess, VOID *val)
{
	UINT32 pn = GLOBAL_STATE::NewChild(CHILD_PROCESS_GetId(childProcess));
	BOOL profile = WINDOW_CONFIG::IsProcessInteresting(pn);
	return profile;
}

VOID ProcessForkedProcessInParent(THREADID threadid, const CONTEXT *ctxt, VOID *v)
{
	// When a process is forked, it will always be profiled whether it's interesting or not.
}

VOID ProcessForkedProcessInChild(THREADID threadid, const CONTEXT *ctxt, VOID *v)
{
	// Properly initialize the global state here.
}

VOID ApplicationStart(VOID *arg)
{
	// When pin attaches to a running process, this notification function is called on a dedicated thread which is not part of the application's threads.
	// Otherwise, this is running in the app's main thread and has already been registered.
	if (!PIN_IsApplicationThread())
	{
		UINT32 dummy;
		MEMORY_REGISTRY::RegisterStack(
			reinterpret_cast<ADDRINT>(&dummy),
			PIN_GetTid(),
			REGION_USER::Pin);
	}

	ALLERIA_WriteMessage(ALLERIA_REPORT_APP_START);
	GLOBAL_STATE::AboutToStart();
	SetStartingTimestamp();
}

VOID EmitStats(PROCESSING_THREAD_STATS& total, PROCESSING_THREAD_STATS& avg)
{
#pragma DISABLE_COMPILER_WARNING_FLOAT2INT_LOSS
	double totalWaitingTime = GLOBAL_STATE::totalAppThreadWaitingTime;
	double avgWaitingTime = totalWaitingTime / GLOBAL_STATE::totalAppThreadCount;

	// Convert microseconds to seconds.
	const UINT32 sec = 1000000;
	totalWaitingTime = totalWaitingTime / sec;
	avgWaitingTime = avgWaitingTime / sec;
	double idleTimeSpan = (double)total.idleTimeSpan / sec;
	double processingTimeSpan = (double)total.processingTimeSpan / sec;
	double avgIdleTimeSpan = (double)avg.idleTimeSpan / sec;
	double avgProcessingTimeSpan = (double)avg.processingTimeSpan / sec;

	// Emit statistics.
	std::stringstream msg;
	msg << NEW_LINE << "     --- STATS TOTAL ---" << NEW_LINE;
	msg << "     Processing threads idle time       = " << idleTimeSpan << " seconds" << NEW_LINE;
	msg << "     Processing threads processing time = " << processingTimeSpan << " seconds" << NEW_LINE;
	msg << "     App threads waiting time           = " << totalWaitingTime << " seconds" << NEW_LINE;
	msg << "     Number of processed buffers        = " << total.bufferPairCount << NEW_LINE;
	msg << "     Unused Bytes in buffers            = " << total.unusedBytes / 1024 << " KiB" << NEW_LINE;
	msg << "     ---  STATS AVG  ---" << NEW_LINE;
	msg << "     Processing thread idle time        = " << avgIdleTimeSpan << " seconds" << NEW_LINE;
	msg << "     Processing thread processing time  = " << avgProcessingTimeSpan << " seconds" << NEW_LINE;
	msg << "     App thread waiting time            = " << avgWaitingTime << " seconds" << NEW_LINE;
	msg << "     Number of processed buffers        = " << avg.bufferPairCount << NEW_LINE;
	msg << "     Unused Bytes in buffers            = " << avg.unusedBytes / 1024 << " KiB" << NEW_LINE;
	ALLERIA_WriteMessageEx(ALLERIA_REPORT_STATS, msg.str().c_str());
#pragma RESTORE_COMPILER_WARNING_FLOAT2INT_LOSS
}

VOID Fini(INT32 code, VOID *arg)
{
	if (g_isFaulty)
		return;

	PROCESSING_THREAD_STATS total = { 0 };
	PROCESSING_THREAD_STATS avg = { 0 };
	UINT32 count = 0;
	for (std::vector<PIN_THREAD_UID>::iterator pthread = GLOBAL_STATE::processingThreadsIds.begin();
		pthread != GLOBAL_STATE::processingThreadsIds.end();
		++pthread)
	{
		PROCESSING_THREAD_STATS *stats =
			reinterpret_cast<PROCESSING_THREAD_STATS*>(PIN_GetThreadData(lstate.processingThreadStatsKey,
			/* IMPORTANT NOTE
			Casting from PIN_THREAD_UID to THREADID only works when
			no processing thread is created after one has terminated.
			*/
			(THREADID)*pthread));
		total.bufferPairCount += stats->bufferPairCount;
		total.idleTimeSpan += stats->idleTimeSpan;
		total.processingTimeSpan += stats->processingTimeSpan;
		total.unusedBytes += stats->unusedBytes;
		delete stats;
		++count;
	}

	if (count != 0)
	{
		avg.bufferPairCount = total.bufferPairCount / count;
		avg.idleTimeSpan = total.idleTimeSpan / count;
		avg.processingTimeSpan = total.processingTimeSpan / count;
		avg.unusedBytes = total.unusedBytes == 0 ? 0 : total.unusedBytes / total.bufferPairCount;

		EmitStats(total, avg);
	}

	if (g_outputFormat == OUTPUT_FORMAT_MODE_BINARY)
		PIN_DeleteThreadDataKey(lstate.routineMapKey);

	if (g_outputFormat == OUTPUT_FORMAT_MODE_TEXTUAL)
	{
		GLOBAL_STATE::gfstream << std::dec;
		GLOBAL_STATE::gfstream << "Total instruction count = " << GLOBAL_STATE::insCount
			<< "\nTotal memory access instruction count = " << GLOBAL_STATE::insMemCount << "\n";
	}
	else // OUTPUT_FORMAT_MODE_BINARY
	{
		GLOBAL_STATE::PatchInsCounts();
		WriteBinaryFuncImgSections(TRUE);
		GLOBAL_STATE::PatchProcFamily(); // Last section in main profile.
	}

	PortableDispose();
	GLOBAL_STATE::AboutToTerminate();

	ALLERIA_WriteMessage(ALLERIA_REPORT_EXITING);

	GLOBAL_STATE::Dispose();

	PIN_DeleteThreadDataKey(lstate.appThreadManagerKey);
	PIN_DeleteThreadDataKey(lstate.ofstreamKey);
	PIN_DeleteThreadDataKey(lstate.processingThreadStatsKey);
	PIN_DeleteThreadDataKey(lstate.startingTimeHighKey);
	PIN_DeleteThreadDataKey(lstate.startingTimeLowKey);
	PIN_DeleteThreadDataKey(lstate.sysCallBufferEntryAddress);
	PIN_DeleteThreadDataKey(lstate.memAllocInfo);
}

VOID TraceInsertedInCodeCache(TRACE trace, VOID *arg)
{
	ADDRINT addr = TRACE_CodeCacheAddress(trace);
	MEMORY_REGISTRY::RegisterGeneral(addr, REGION_USER::Pin);
}

VOID RegsiterCallbacks()
{
	TRACE_AddInstrumentFunction(InstrumentTrace, NULL);
	IMG_AddInstrumentFunction(InstrumentImage, NULL);
	IMG_AddUnloadFunction(InstrumentImageUnload, NULL);
	PIN_AddThreadStartFunction(ThreadStart, NULL);
	PIN_AddThreadFiniFunction(ThreadFini, NULL);
	PIN_AddFiniFunction(Fini, NULL);
	PIN_AddFollowChildProcessFunction(FollowChild, NULL);
	PIN_AddApplicationStartFunction(ApplicationStart, NULL);

	//PIN_AddInternalExceptionHandler(GlobalInternalHandler, NULL);

	PIN_AddContextChangeFunction(OnContextChanged, NULL);
	CODECACHE_AddTraceInsertedFunction(TraceInsertedInCodeCache, NULL);
	PIN_AddSyscallExitFunction(AnalyzeSysCallExit, NULL);
	PIN_AddPrepareForFiniFunction(PrepareForFini, NULL);
#if defined(TARGET_LINUX)
	PIN_AddForkFunction(FPOINT_BEFORE, ProcessForkedProcessInParent, NULL);
	PIN_AddForkFunction(FPOINT_AFTER_IN_CHILD, ProcessForkedProcessInChild, NULL);
#endif
}

INT32 Usage()
{
	cerr << KNOB_BASE::StringKnobSummary();
	return -1;
}

VOID ClaimRegisters()
{
	lstate.bufferEndReg = PIN_ClaimToolRegister();
	lstate.bufferLastReg = PIN_ClaimToolRegister();
	lstate.interceptorMemManReg = PIN_ClaimToolRegister();

	if (!(REG_valid(lstate.bufferEndReg) &&
		REG_valid(lstate.bufferLastReg) &&
		REG_valid(lstate.interceptorMemManReg)))
	{
		AlleriaSetLastError(ALLERIA_ERROR_INIT_FAILED);
	}
}

VOID InitMemoryRegistry()
{
	UINT32 dummy;
	MEMORY_REGISTRY::Init();
	//AlleriaRuntimeSwitchToAllocRegister();

	MEMORY_REGISTRY::RegisterStack(
		reinterpret_cast<ADDRINT>(&dummy),
		PIN_GetTid(),
		REGION_USER::Pin);

	RegisterTEB();

#ifdef TARGET_WINDOWS
	// Do we have to register the default heap? It can only be used by the system or the app.
#endif

	void *dummy2 = malloc(4);
	MEMORY_REGISTRY::RegisterGeneral(
		reinterpret_cast<ADDRINT>(dummy2),
		REGION_USER::Alleria | REGION_USER::Pin);
	free(dummy2);

	REGION pin;
	REGION alleria;
	std::vector<REGION> others;
	WINDOWS_LIVE_LANDS_WALKER::Init();
	WINDOWS_LIVE_LANDS_WALKER::FindSpecialRegions(pin, alleria, others);
	MEMORY_REGISTRY::RegisterImage(reinterpret_cast<ADDRINT>(pin.pvBaseAddress), REGION_USER::Pin);
	MEMORY_REGISTRY::RegisterImage(reinterpret_cast<ADDRINT>(alleria.pvBaseAddress), REGION_USER::Alleria);
	for (UINT32 i = 0; i < others.size(); ++i)
	{
		if (others[i].userId.find(".exe") == EOF && others[i].userId.find("dll") == EOF)
			MEMORY_REGISTRY::RegisterMappedMemory(
			reinterpret_cast<ADDRINT>(others[i].pvBaseAddress), REGION_USER::Platform);
	}

	RegisterProcessSystemBlocks();

	MEMORY_REGISTRY::RegisterGeneral(reinterpret_cast<ADDRINT>(GLOBAL_STATE::myPpndbEntry), REGION_USER::Alleria);

	//MEMORY_REGISTRY::RegisterGeneral(gstate.memInfo.MaximumApplicationAddress + 1, REGION_USER::Platform);
}

int main(int argc, char * argv[])
{
	PIN_InitSymbols();
	if (PIN_Init(argc, argv))
	{
		return Usage();
	}

	// Order of initialization is important:
	// 1- PortableInitBasic.
	// 2- GLOBAL_STATE::Init
	// 3- PortableInit
	// 4- The rest in any order.

	PortableInitBasic();

	// This must be called before any call to ALLERIA_WriteMessage and ALLERIA_WriteError.
	// It initializes global vars used by them.
	GLOBAL_STATE::Init(argc, argv);

	if (!IsPlatSupported())
	{
		AlleriaSetLastError(ALLERIA_ERROR_INIT_FAILED);
		ALLERIA_WriteError(AlleriaGetLastError());
	}

	INIT_FLAGS initFlags = (g_v2pTransMode == V2P_TRANS_MODE::V2P_TRANS_MODE_OFF) ? INIT_FLAGS_NOTHING : INIT_FLAGS_LOAD_DRIVER;
	initFlags = (INIT_FLAGS)(initFlags | ((g_odsMode == ODS_MODE::ODS_MODE_SUPPRESS) ? INIT_FLAGS_ODS_SUPPRESS : INIT_FLAGS_NOTHING));
	if (!PortableInit(initFlags))
		AlleriaSetLastError(ALLERIA_ERROR_INIT_FAILED);

	if (AlleriaGetLastError() != ALLERIA_ERROR::ALLERIA_ERROR_SUCCESS)
	{
		ALLERIA_WriteError(AlleriaGetLastError());
	}

	ALLERIA_WriteMessage(ALLERIA_REPORT_INIT);

	if (g_v2pTransMode != V2P_TRANS_MODE::V2P_TRANS_MODE_OFF)
	{
		if (IsElevated())
		{
			if (!LoadDriver() || (g_v2pTransMode == V2P_TRANS_MODE_ACCURATE && !PrepareForPinningVirtualPages()))
				AlleriaSetLastError(ALLERIA_ERROR_KNOB_DRIVER_FAILED);
		}
		else
		{
			AlleriaSetLastError(ALLERIA_ERROR_KNOB_ElevationRequired);
		}
	}

	if (AlleriaGetLastError() != ALLERIA_ERROR::ALLERIA_ERROR_SUCCESS)
	{
		ALLERIA_WriteError(AlleriaGetLastError());
	}

	InitMemoryRegistry();

	CHAR mainExe[PORTABLE_MAX_PATH];
	GetMainModuleFilePathA(mainExe, PORTABLE_MAX_PATH);

	WINDOW_CONFIG::Init(KnobConfigDir.ValueString(), g_processNumber, mainExe);

	lstate.appThreadManagerKey = PIN_CreateThreadDataKey(0);
	lstate.ofstreamKey = PIN_CreateThreadDataKey(0);
	lstate.startingTimeHighKey = PIN_CreateThreadDataKey(0);
	lstate.startingTimeLowKey = PIN_CreateThreadDataKey(0);
	lstate.processingThreadStatsKey = PIN_CreateThreadDataKey(0);
	lstate.sysCallBufferEntryAddress = PIN_CreateThreadDataKey(0);
	lstate.memAllocInfo = PIN_CreateThreadDataKey(0);
	if (g_outputFormat == OUTPUT_FORMAT_MODE_BINARY)
		lstate.routineMapKey = PIN_CreateThreadDataKey(0);

	RegsiterCallbacks();
	CreateAlleriaThreads();
	ClaimRegisters();

	if (AlleriaGetLastError() != ALLERIA_ERROR::ALLERIA_ERROR_SUCCESS)
		ALLERIA_WriteError(AlleriaGetLastError());

	GLOBAL_STATE::CreateOutputFiles();

	if (AlleriaGetLastError() != ALLERIA_ERROR::ALLERIA_ERROR_SUCCESS)
		ALLERIA_WriteError(AlleriaGetLastError());

	// Initialize Intel XED.
	xed_tables_init();

	PIN_StartProgram();

	return 0;
}
