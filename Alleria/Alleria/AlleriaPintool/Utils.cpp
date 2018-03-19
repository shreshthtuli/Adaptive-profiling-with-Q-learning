#include "Utils.h"
#include <unordered_set>
#include <sstream>
#include <iomanip>

extern UINT8 g_virtualAddressSize; // In bytes.
extern UINT8 g_physicalAddressSize; // In bytes.

enum V2P_TRANS_MODE
{
	V2P_TRANS_MODE_INVALID,
	V2P_TRANS_MODE_OFF,
	V2P_TRANS_MODE_FAST,
	V2P_TRANS_MODE_ACCURATE
};

extern V2P_TRANS_MODE g_v2pTransMode;

// In bytes.
LOCALCONST UINT32 LegacyStateComponentSizes[] =
{
	24, // X87 status and control registers.
	8 * (10 + 6), // X87 registers. This holds regardless of the REX prefix. The reserved parts are zeroed out.
	8, // SSE status and control registers.
	16 * 8, // SSE on 32-bit (XMM0-7).
	16 * 16, // SSE on 64-bit (XMM0-15).
};

LOCALCONST UINT32 LegacyStateComponentOffsets[] =
{
	0, // X87 status and control registers.
	32, // X87 registers. This holds regardless of the REX prefix. The reserved parts are zeroed out.
	24, // SSE status and control registers.
	160, // SSE on 32-bit (XMM0-7).
	160, // SSE on 64-bit (XMM0-15).
};

// The following macro is used to make the implementation of GetXsaveAccessInfo and GetXrstorAccessInfo look better.
// If compSize == 0, this means that the state component is not supported by the processor.
#define SAVE_COMPONENT_STATE_EXTENDED(memop) \
if(compSize > 0) \
{ \
	if (recordVals) \
	{ \
		FillMemAccess0FromStateComps(acccessDesc_0, offset, compSize, xsaveAreaBaseInt, memop); \
		accesses_0->push_back(acccessDesc_0); \
	} \
	else \
	{ \
		FillMemAccess1FromStateComps(acccessDesc_1, offset, compSize, xsaveAreaBaseInt, memop); \
		accesses_1->accesses.push_back(acccessDesc_1); \
	}\
}

// The following three macros are used to make the implementation of GetXsavecAccessInfo and GetXrstorAccessInfo look better.
// If compSize == 0, this means that the state component is not supported by the processor.
#define SAVE_COMPONENT_STATE_EXTENDED_OFFSET(offset, memop) \
if(compSize > 0) \
{ \
	if (recordVals) \
	{ \
		FillMemAccess0FromStateComps(acccessDesc_0, offset, compSize, xsaveAreaBaseInt, memop); \
		accesses_0->push_back(acccessDesc_0); \
	} \
	else \
	{ \
		FillMemAccess1FromStateComps(acccessDesc_1, offset, compSize, xsaveAreaBaseInt, memop); \
		accesses_1->accesses.push_back(acccessDesc_1); \
	} \
}

#define ALIGN_XSAVEC_OFFSET \
if (align && ((lastOffset & 0x3F) != 0)) \
lastOffset = (lastOffset | 0x3F) + 1;

#define PROCESS_STATE_COMPONENT(component, memop) \
BOOL align = GetStateComponentOffset(component, offset, compSize); \
size += compSize; \
SAVE_COMPONENT_STATE_EXTENDED_OFFSET(lastOffset, memop); \
lastOffset += compSize; \
ALIGN_XSAVEC_OFFSET

const char* ContextChangeReasonString(CONTEXT_CHANGE_REASON reason)
{
	switch (reason)
	{
	case CONTEXT_CHANGE_REASON::CONTEXT_CHANGE_REASON_APC:
		return "APC";
	case CONTEXT_CHANGE_REASON::CONTEXT_CHANGE_REASON_CALLBACK:
		return "Callback";
	case CONTEXT_CHANGE_REASON::CONTEXT_CHANGE_REASON_EXCEPTION:
		return "Exception";
	case CONTEXT_CHANGE_REASON::CONTEXT_CHANGE_REASON_FATALSIGNAL:
		return "Fatal signal";
	case CONTEXT_CHANGE_REASON::CONTEXT_CHANGE_REASON_SIGNAL:
		return "Signal";
	case CONTEXT_CHANGE_REASON::CONTEXT_CHANGE_REASON_SIGRETURN:
		return "Return from signal handler";
	}
	return "Context Changed";
}

ALLERIA_MULTI_MEM_ACCESS_INFO *CopyMultiMemAccessInfo(PIN_MULTI_MEM_ACCESS_INFO *accesses, UINT32& size)
{
	ASSERTX(accesses != NULL);
	ALLERIA_MULTI_MEM_ACCESS_INFO *result = new ALLERIA_MULTI_MEM_ACCESS_INFO;
	result->numberOfMemops = accesses->numberOfMemops;
	UINT32 i = 0;
	size = 0;
	while (i < accesses->numberOfMemops)
	{
		result->memop[i] = accesses->memop[i];
		if (accesses->memop[i].maskOn)
		{
			UINT8 *value = new UINT8[accesses->memop[i].bytesAccessed];
			if (PIN_SafeCopy(value, reinterpret_cast<VOID*>(accesses->memop[i].memoryAddress), accesses->memop[i].bytesAccessed)
				== accesses->memop[i].bytesAccessed)
				result->values[i] = reinterpret_cast<ADDRINT>(value);
			else
			{
				delete[] value;
				result->values[i] = NULL;
			}
			size += accesses->memop[i].bytesAccessed;
		}
		else
		{
			result->values[i] = NULL;
		}
		++i;
	}
	return result;
}

ALLERIA_MULTI_MEM_ACCESS_ADDRS *CopyMultiMemAccessAddrs(PIN_MULTI_MEM_ACCESS_INFO *accesses, UINT32& size)
{
	ASSERTX(accesses != NULL);
	ALLERIA_MULTI_MEM_ACCESS_ADDRS *result = new ALLERIA_MULTI_MEM_ACCESS_ADDRS;
	result->numberOfMemops = accesses->numberOfMemops;
	UINT32 i = 0;
	size = 0;
	while (i < accesses->numberOfMemops)
	{
		result->addrs[i] = accesses->memop[i].memoryAddress;
		result->maskOn[i] = accesses->memop[i].maskOn;
		result->sizes[i] = accesses->memop[i].bytesAccessed;
		if (accesses->memop[i].maskOn)
		{
			size += accesses->memop[i].bytesAccessed;
		}
		++i;
	}
	// result->base is not set here. It's set by the caller.
	return result;
}

BOOL GetStateComponentOffset(int stateComponent, UINT32& offset, UINT32& size)
{
	int cpuInfo[4];
	IntrinsicCpuid(cpuInfo, 0x0D, stateComponent);
	offset = (UINT32)cpuInfo[1];
	size = (UINT32)cpuInfo[0];
	BOOL align = (cpuInfo[2] & 0x2) == 1;
	return align;
}

BOOL IsCompactedXrstorSupported()
{
	int cpuInfo[4];
	IntrinsicCpuid(cpuInfo, 0x0D, 1);
	return (cpuInfo[0] & 0x2) == 1;
}

VOID FillMemAccess0FromStateComps(MEM_ACCESS_GENERAL_0& access, UINT32 offset, UINT32 size, ADDRINT base, PIN_MEMOP_ENUM memop)
{
	access.bytesAccessed = size;
	access.memopType = memop;
	access.addr = base + offset;
	access.value = new UINT8[access.bytesAccessed];
	if (PIN_SafeCopy(access.value, reinterpret_cast<VOID*>(access.addr), access.bytesAccessed) != access.bytesAccessed)
	{
		delete[] access.value;
		access.value = NULL;
	}
}

VOID FillMemAccess1FromStateComps(MEM_ACCESS_GENERAL_1& access, UINT32 offset, UINT32 size, ADDRINT base, PIN_MEMOP_ENUM memop)
{
	access.bytesAccessed = size;
	access.memopType = memop;
	access.addr = base + offset;
}

// If recordVals is TRUE, returns vector<MEM_ACCESS_GENERAL_0>*. Otherwise, returns ALLERIA_XSAVE_MEM_ACCESS_ADDRS*.
// Must be called after executing the XSAVE instruction.
// Use this function to record accesses performed by the XSAVE and XSAVEOPT instructions only.
void* GetXsaveAccessInfo(UINT8 *xsaveAreaBase, BOOL is32bit, UINT32& size, BOOL recordVals)
{
	/* IMPORTANT NOTE

	From 13.13 of the intel software developer manual version June 2015.

	Section 13.5 provides details of the different XSAVE-managed state components. Some portions of some of these
	components are accessible only in 64-bit mode. Executions of XRSTOR and XRSTORS outside 64-bit mode will not
	update those portions; executions of XSAVE, XSAVEC, XSAVEOPT, and XSAVES will not modify the corresponding
	locations in memory.

	Despite this fact, any execution of these instructions outside 64-bit mode may access any byte in any state
	component on which that execution operates — even those at addresses corresponding to registers that are accessible
	only in 64-bit mode. As result, such an execution may incur a fault due to an attempt to access such an address.

	*/

	UINT64 xstate_bv = *reinterpret_cast<UINT64*>(xsaveAreaBase + 512);
	vector<MEM_ACCESS_GENERAL_0> *accesses_0;
	ALLERIA_XSAVE_MEM_ACCESS_ADDRS *accesses_1;
	ADDRINT xsaveAreaBaseInt = reinterpret_cast<ADDRINT>(xsaveAreaBase);

	/*
	
	The XSAVE instruction reads the XSTATE_BV bitmap at offset 512 from the XSAVE area. This is also
	at offset zero from the extended region of the XSAVE area. The analysis routine that is calling this
	function is responsible for registering this memory access.

	The XSAVE instruction writes the XSTATE_BV bitmap at the same offset as well. This bitmap specifies which
	state component has been written to the XSAVE area even when the processor init optimization and/or the processor
	modified optimization is on. We use these bits to determine the which memory accesses that have occurred. 
	That's why this function must be called after the execution of the XSAVE instruction.
	
	*/
	
	size = 8 + 16; // Writing XSTATE_BV. The processor also fills the next 16 bytes with zeroes.
	UINT32 offset, compSize;
	MEM_ACCESS_GENERAL_0 acccessDesc_0;
	MEM_ACCESS_GENERAL_1 acccessDesc_1;

	// Accounts for writing some of the bytes in the XSAVE header.
	if (recordVals)
	{
		accesses_0 = new vector<MEM_ACCESS_GENERAL_0>;

		acccessDesc_0.addr = xsaveAreaBaseInt + 512;
		acccessDesc_0.bytesAccessed = size;
		acccessDesc_0.memopType = PIN_MEMOP_STORE;
		acccessDesc_0.value = new UINT8[size];
		if (PIN_SafeCopy(acccessDesc_0.value, reinterpret_cast<UINT8*>(xsaveAreaBase + 512), size) != size)
		{
			delete[] acccessDesc_0.value;
			acccessDesc_0.value = NULL;
		}

		accesses_0->push_back(acccessDesc_0);
	}
	else
	{
		accesses_1 = new ALLERIA_XSAVE_MEM_ACCESS_ADDRS;
		accesses_1->base = xsaveAreaBaseInt;

		acccessDesc_1.addr = xsaveAreaBaseInt + 512;
		acccessDesc_1.memopType = PIN_MEMOP_STORE;
		acccessDesc_1.bytesAccessed = size;

		accesses_1->accesses.push_back(acccessDesc_1);
	}

	// Accounts for saving the x87 state component. The offset and size of this component is fixed.
	if (xstate_bv & XSAVE_STATE_COMPONENT_MASK_X87)
	{
		size += LegacyStateComponentSizes[0];
		size += LegacyStateComponentSizes[1];
		if (recordVals)
		{
			FillMemAccess0FromStateComps(acccessDesc_0, LegacyStateComponentOffsets[0], LegacyStateComponentSizes[0], xsaveAreaBaseInt, PIN_MEMOP_STORE);
			accesses_0->push_back(acccessDesc_0);

			FillMemAccess0FromStateComps(acccessDesc_0, LegacyStateComponentOffsets[1], LegacyStateComponentSizes[1], xsaveAreaBaseInt, PIN_MEMOP_STORE);
			accesses_0->push_back(acccessDesc_0);
		}
		else
		{
			FillMemAccess1FromStateComps(acccessDesc_1, LegacyStateComponentOffsets[0], LegacyStateComponentSizes[0], xsaveAreaBaseInt, PIN_MEMOP_STORE);
			accesses_1->accesses.push_back(acccessDesc_1);

			FillMemAccess1FromStateComps(acccessDesc_1, LegacyStateComponentOffsets[1], LegacyStateComponentSizes[1], xsaveAreaBaseInt, PIN_MEMOP_STORE);
			accesses_1->accesses.push_back(acccessDesc_1);
		}
	}

	// Accounts for saving the SSE state component except for the MXCSR and MXCSR_MASK registers.
	if (xstate_bv & XSAVE_STATE_COMPONENT_MASK_SSE)
	{
		size += (is32bit ? LegacyStateComponentSizes[3] : LegacyStateComponentSizes[4]);
		if (recordVals)
		{
			FillMemAccess0FromStateComps(acccessDesc_0, is32bit ? LegacyStateComponentOffsets[3] : LegacyStateComponentOffsets[4], 
				is32bit ? LegacyStateComponentSizes[3] : LegacyStateComponentSizes[4], xsaveAreaBaseInt, PIN_MEMOP_STORE);
			accesses_0->push_back(acccessDesc_0);
		}
		else
		{
			FillMemAccess1FromStateComps(acccessDesc_1, is32bit ? LegacyStateComponentOffsets[3] : LegacyStateComponentOffsets[4], 
				is32bit ? LegacyStateComponentSizes[3] : LegacyStateComponentSizes[4], xsaveAreaBaseInt, PIN_MEMOP_STORE);
			accesses_1->accesses.push_back(acccessDesc_1);
		}
	}

	// Accounts for saving the MXCSR and MXCSR_MASK registers.
	// The Intel doc section 13.7 and 13.9 states that these registers are saved when either XSTATE_BV[1] = 1 or XSTATE_BV[2] = 1.
	if (xstate_bv & XSAVE_STATE_COMPONENT_MASK_SSE || xstate_bv & XSAVE_STATE_COMPONENT_MASK_AVX)
	{
		size += LegacyStateComponentSizes[2];
		if (recordVals)
		{
			FillMemAccess0FromStateComps(acccessDesc_0, LegacyStateComponentOffsets[2], LegacyStateComponentSizes[2], xsaveAreaBaseInt, PIN_MEMOP_STORE);
			accesses_0->push_back(acccessDesc_0);
		}
		else
		{
			FillMemAccess1FromStateComps(acccessDesc_1, LegacyStateComponentOffsets[2], LegacyStateComponentSizes[2], xsaveAreaBaseInt, PIN_MEMOP_STORE);
			accesses_1->accesses.push_back(acccessDesc_1);
		}
	}

	/*

	From Intel manual volume 1 section 13.7:
	The XSAVE instruction always uses the standard format for the extended region of the XSAVE area.
	All of the following state components are stored at fixed offsets and fixed sizes.

	*/

	if (xstate_bv & XSAVE_STATE_COMPONENT_MASK_AVX)
	{
		GetStateComponentOffset(XSAVE_STATE_COMPONENT_AVX, offset, compSize);
		size += compSize;
		SAVE_COMPONENT_STATE_EXTENDED(PIN_MEMOP_STORE);
	}

	if (xstate_bv & XSAVE_STATE_COMPONENT_MASK_MPX_1)
	{
		GetStateComponentOffset(XSAVE_STATE_COMPONENT_MPX_1, offset, compSize);
		size += compSize;
		SAVE_COMPONENT_STATE_EXTENDED(PIN_MEMOP_STORE);
	}

	if (xstate_bv & XSAVE_STATE_COMPONENT_MASK_MPX_2)
	{
		GetStateComponentOffset(XSAVE_STATE_COMPONENT_MPX_2, offset, compSize);
		size += compSize;
		SAVE_COMPONENT_STATE_EXTENDED(PIN_MEMOP_STORE);
	}

	if (xstate_bv & XSAVE_STATE_COMPONENT_MASK_AVX512_1)
	{
		GetStateComponentOffset(XSAVE_STATE_COMPONENT_AVX512_1, offset, compSize);
		size += compSize;
		SAVE_COMPONENT_STATE_EXTENDED(PIN_MEMOP_STORE);
	}

	if (xstate_bv & XSAVE_STATE_COMPONENT_MASK_AVX512_2)
	{
		GetStateComponentOffset(XSAVE_STATE_COMPONENT_AVX512_2, offset, compSize);
		size += compSize;
		SAVE_COMPONENT_STATE_EXTENDED(PIN_MEMOP_STORE);
	}
	if (xstate_bv & XSAVE_STATE_COMPONENT_MASK_AVX512_3)
	{
		GetStateComponentOffset(XSAVE_STATE_COMPONENT_AVX512_3, offset, compSize);
		size += compSize;
		SAVE_COMPONENT_STATE_EXTENDED(PIN_MEMOP_STORE);
	}

	if (xstate_bv & XSAVE_STATE_COMPONENT_MASK_PKRU)
	{
		GetStateComponentOffset(XSAVE_STATE_COMPONENT_PKRU, offset, compSize);
		size += compSize;
		SAVE_COMPONENT_STATE_EXTENDED(PIN_MEMOP_STORE);
	}

	if (xstate_bv & !XSAVE_STATE_COMPONENT_MASK_ALL)
	{
		ALLERIA_WriteMessage(ALLERIA_REPORT_XSAVE_UNKOWN_STATE);
	}

	if (recordVals)
		return accesses_0;
	else
		return accesses_1;
}

// If recordVals is TRUE, returns vector<MEM_ACCESS_GENERAL_0>*. Otherwise, returns ALLERIA_XSAVE_MEM_ACCESS_ADDRS*.
// Must be called after executing the XSAVEC instruction.
// Use this function to record accesses performed by the XSAVEC instruction only.
void* GetXsavecAccessInfo(UINT8 *xsaveAreaBase, BOOL is32bit, UINT32& size, BOOL recordVals)
{
	/* IMPORTANT NOTE

	From 13.13 of the intel software developer manual version June 2015.

	Section 13.5 provides details of the different XSAVE-managed state components. Some portions of some of these
	components are accessible only in 64-bit mode. Executions of XRSTOR and XRSTORS outside 64-bit mode will not
	update those portions; executions of XSAVE, XSAVEC, XSAVEOPT, and XSAVES will not modify the corresponding
	locations in memory.

	Despite this fact, any execution of these instructions outside 64-bit mode may access any byte in any state
	component on which that execution operates — even those at addresses corresponding to registers that are accessible
	only in 64-bit mode. As result, such an execution may incur a fault due to an attempt to access such an address.

	*/

	/*
	
	From 13.10

	The operation of XSAVEC is similar to that of XSAVE. Two main differences are (1) XSAVEC uses the compacted
	format for the extended region of the XSAVE area; and (2) XSAVEC uses the init optimization (see Section 13.6).
	Unlike XSAVEOPT, XSAVEC does not use the modified optimization.

	However, the manual doen't say anything about reading from the XSAVE header. Therefore, I think that XSAVEC doesn't read
	XSTATE_BV which is different from XSAVE and XSAVEOPT. There is another different as mentioned at the end of 13.10.
	See the code that checks for the SSE state component.
	
	*/

	UINT64 xstate_bv = *reinterpret_cast<UINT64*>(xsaveAreaBase + 512);
	vector<MEM_ACCESS_GENERAL_0> *accesses_0;
	ALLERIA_XSAVE_MEM_ACCESS_ADDRS *accesses_1;
	ADDRINT xsaveAreaBaseInt = reinterpret_cast<ADDRINT>(xsaveAreaBase);

	size = 16; // Writing XSTATE_BV and XCOMP_BV.
	UINT32 offset, compSize;
	UINT32 lastOffset = 576; // The offset of the Extended Region of an XSAVE area.
	MEM_ACCESS_GENERAL_0 acccessDesc_0;
	MEM_ACCESS_GENERAL_1 acccessDesc_1;

	// Accounts for writing some of the bytes in the XSAVE header.
	if (recordVals)
	{
		accesses_0 = new vector<MEM_ACCESS_GENERAL_0>;

		acccessDesc_0.addr = xsaveAreaBaseInt + 512;
		acccessDesc_0.bytesAccessed = size;
		acccessDesc_0.memopType = PIN_MEMOP_STORE;
		acccessDesc_0.value = new UINT8[size];
		if (PIN_SafeCopy(acccessDesc_0.value, reinterpret_cast<UINT8*>(xsaveAreaBase + 512), size) != size)
		{
			delete[] acccessDesc_0.value;
			acccessDesc_0.value = NULL;
		}

		accesses_0->push_back(acccessDesc_0);
	}
	else
	{
		accesses_1 = new ALLERIA_XSAVE_MEM_ACCESS_ADDRS;
		accesses_1->base = xsaveAreaBaseInt;

		acccessDesc_1.addr = xsaveAreaBaseInt + 512;
		acccessDesc_1.memopType = PIN_MEMOP_STORE;
		acccessDesc_1.bytesAccessed = size;

		accesses_1->accesses.push_back(acccessDesc_1);
	}

	// Accounts for saving the x87 state component. The offset and size of this component is fixed.
	if (xstate_bv & XSAVE_STATE_COMPONENT_MASK_X87)
	{
		size += LegacyStateComponentSizes[0];
		size += LegacyStateComponentSizes[1];
		if (recordVals)
		{
			FillMemAccess0FromStateComps(acccessDesc_0, LegacyStateComponentOffsets[0], LegacyStateComponentSizes[0], xsaveAreaBaseInt, PIN_MEMOP_STORE);
			accesses_0->push_back(acccessDesc_0);

			FillMemAccess0FromStateComps(acccessDesc_0, LegacyStateComponentOffsets[1], LegacyStateComponentSizes[1], xsaveAreaBaseInt, PIN_MEMOP_STORE);
			accesses_0->push_back(acccessDesc_0);
		}
		else
		{
			FillMemAccess1FromStateComps(acccessDesc_1, LegacyStateComponentOffsets[0], LegacyStateComponentSizes[0], xsaveAreaBaseInt, PIN_MEMOP_STORE);
			accesses_1->accesses.push_back(acccessDesc_1);

			FillMemAccess1FromStateComps(acccessDesc_1, LegacyStateComponentOffsets[1], LegacyStateComponentSizes[1], xsaveAreaBaseInt, PIN_MEMOP_STORE);
			accesses_1->accesses.push_back(acccessDesc_1);
		}
	}

	// Accounts for saving the SSE state component including the MXCSR and MXCSR_MASK registers.
	if (xstate_bv & XSAVE_STATE_COMPONENT_MASK_SSE)
	{
		size += LegacyStateComponentSizes[2];
		size += (is32bit ? LegacyStateComponentSizes[3] : LegacyStateComponentSizes[4]);
		if (recordVals)
		{
			FillMemAccess0FromStateComps(acccessDesc_0, LegacyStateComponentOffsets[2], LegacyStateComponentSizes[2], xsaveAreaBaseInt, PIN_MEMOP_STORE);
			accesses_0->push_back(acccessDesc_0);

			FillMemAccess0FromStateComps(acccessDesc_0, is32bit ? LegacyStateComponentOffsets[3] : LegacyStateComponentOffsets[4],
				is32bit ? LegacyStateComponentSizes[3] : LegacyStateComponentSizes[4], xsaveAreaBaseInt, PIN_MEMOP_STORE);
			accesses_0->push_back(acccessDesc_0);
		}
		else
		{
			FillMemAccess1FromStateComps(acccessDesc_1, LegacyStateComponentOffsets[2], LegacyStateComponentSizes[2], xsaveAreaBaseInt, PIN_MEMOP_STORE);
			accesses_1->accesses.push_back(acccessDesc_1);

			FillMemAccess1FromStateComps(acccessDesc_1, is32bit ? LegacyStateComponentOffsets[3] : LegacyStateComponentOffsets[4], 
				is32bit ? LegacyStateComponentSizes[3] : LegacyStateComponentSizes[4], xsaveAreaBaseInt, PIN_MEMOP_STORE);
			accesses_1->accesses.push_back(acccessDesc_1);
		}
	}

	/*

	The XSAVEC instruction always uses the compacted format for the extended region of the XSAVE area.
	See section 13.4.3 for a description of this format.
	The following code takes care of this format as well as any alignment requirements.

	*/

	if (xstate_bv & XSAVE_STATE_COMPONENT_MASK_AVX)
	{
		BOOL align = GetStateComponentOffset(XSAVE_STATE_COMPONENT_AVX, offset, compSize);
		lastOffset = offset + compSize;
		// No need to check alignment here.
		ASSERTX(!align || ((lastOffset & 0x3F) == 0));
		size += compSize;
		SAVE_COMPONENT_STATE_EXTENDED(PIN_MEMOP_STORE);
	}

	if (xstate_bv & XSAVE_STATE_COMPONENT_MASK_MPX_1)
	{
		PROCESS_STATE_COMPONENT(XSAVE_STATE_COMPONENT_MPX_1, PIN_MEMOP_STORE);
	}

	if (xstate_bv & XSAVE_STATE_COMPONENT_MASK_MPX_2)
	{
		PROCESS_STATE_COMPONENT(XSAVE_STATE_COMPONENT_MPX_2, PIN_MEMOP_STORE);
	}

	if (xstate_bv & XSAVE_STATE_COMPONENT_MASK_AVX512_1)
	{
		PROCESS_STATE_COMPONENT(XSAVE_STATE_COMPONENT_AVX512_1, PIN_MEMOP_STORE);
	}

	if (xstate_bv & XSAVE_STATE_COMPONENT_MASK_AVX512_2)
	{
		PROCESS_STATE_COMPONENT(XSAVE_STATE_COMPONENT_AVX512_2, PIN_MEMOP_STORE);
	}

	if (xstate_bv & XSAVE_STATE_COMPONENT_MASK_AVX512_3)
	{
		PROCESS_STATE_COMPONENT(XSAVE_STATE_COMPONENT_AVX512_3, PIN_MEMOP_STORE);
	}

	if (xstate_bv & XSAVE_STATE_COMPONENT_MASK_PKRU)
	{
		BOOL align = GetStateComponentOffset(XSAVE_STATE_COMPONENT_PKRU, offset, compSize);
		size += compSize;
	}

	if (xstate_bv & !XSAVE_STATE_COMPONENT_MASK_ALL)
	{
		ALLERIA_WriteMessage(ALLERIA_REPORT_XSAVE_UNKOWN_STATE);
	}

	if (recordVals)
		return accesses_0;
	else
		return accesses_1;
}

// If recordVals is TRUE, returns vector<MEM_ACCESS_GENERAL_0>*. Otherwise, returns ALLERIA_XSAVE_MEM_ACCESS_ADDRS*.
// Can be called before or after executing the XRSTOR instruction.
// Use this function to record accesses performed by the XRSTOR instruction only.
void* GetXrstorAccessInfo(const CONTEXT *ctxt, UINT8 *xsaveAreaBase, BOOL is32bit, UINT32& size, BOOL recordVals)
{
	static BOOL isCompactedXrstorSupported = IsCompactedXrstorSupported();
	
	UINT64 xstate_bv = *reinterpret_cast<UINT64*>(xsaveAreaBase + 512);
	UINT64 xcomp_bv = *reinterpret_cast<UINT64*>(xsaveAreaBase + 520);

	UINT64 edxEax = 0; // Instruction mask.

	edxEax = ((UINT64)PIN_GetContextReg(ctxt, REG_EDX)) << 32;
	edxEax = edxEax | PIN_GetContextReg(ctxt, REG_EAX);

	// Unfortunately, XCR0 is not available from CONTEXT. So we have no option but
	// to get the value of that register using the XGETBV instruction. In theory, 
	// this may not work if XCR0 has been modified after the execution of the XSAVE
	// instruction but before getting the value of the register. In practice, this
	// doesn't happen.

	// Extended control register 0.
	UINT64 XCR0 = IntrinsicXgetbv(_XCR_XFEATURE_ENABLED_MASK);

	// Requested-feature bitmap.
	UINT64 rfbm = edxEax & XCR0;
	
	/*
	From section 13.8 of the Intel manual:
	If XCOMP_BV[63] = 0, the standard form of XRSTOR is executed (see Section
	13.8.1); otherwise, the compacted form of XRSTOR is executed (see Section 13.8.2).
	If the processor does not support the compacted form of XRSTOR, it may execute the standard form of XRSTOR without first reading the XCOMP_BV field.
	*/
	BOOL isCompacted = isCompactedXrstorSupported && ((xcomp_bv & 0x8000000000000000) == 1);

	vector<MEM_ACCESS_GENERAL_0> *accesses_0;
	ALLERIA_XSAVE_MEM_ACCESS_ADDRS *accesses_1;
	ADDRINT xsaveAreaBaseInt = reinterpret_cast<ADDRINT>(xsaveAreaBase);

	// 13.8: If the processor does not support the compacted form of XRSTOR, it may execute the standard form of XRSTOR without first reading the XCOMP_BV field.
	// Unfortunately, this statement is ambiguous because it uses the term "may." I'll just think of it as "will."
	size = isCompactedXrstorSupported ? 16 : 8; // XSTATE_BV and XCOMP_BV or just XSTATE_BV.
	UINT32 offset, compSize;
	UINT32 lastOffset = 576; // The offset of the Extended Region of an XSAVE area.
	MEM_ACCESS_GENERAL_0 acccessDesc_0;
	MEM_ACCESS_GENERAL_1 acccessDesc_1;

	// Accounts for reading some of the bytes from the XSAVE header.
	if (recordVals)
	{
		accesses_0 = new vector<MEM_ACCESS_GENERAL_0>;

		acccessDesc_0.addr = xsaveAreaBaseInt + 512;
		acccessDesc_0.bytesAccessed = size;
		acccessDesc_0.memopType = PIN_MEMOP_LOAD;
		acccessDesc_0.value = new UINT8[size];
		if (PIN_SafeCopy(acccessDesc_0.value, reinterpret_cast<UINT8*>(xsaveAreaBase + 512), size) != size)
		{
			delete[] acccessDesc_0.value;
			acccessDesc_0.value = NULL;
		}

		accesses_0->push_back(acccessDesc_0);
	}
	else
	{
		accesses_1 = new ALLERIA_XSAVE_MEM_ACCESS_ADDRS;
		accesses_1->base = xsaveAreaBaseInt;

		acccessDesc_1.addr = xsaveAreaBaseInt + 512;
		acccessDesc_1.memopType = PIN_MEMOP_LOAD;
		acccessDesc_1.bytesAccessed = size;

		accesses_1->accesses.push_back(acccessDesc_1);
	}

	if (isCompacted)
	{
		/*
		
		The code that follows handles the compacted foramt of the extended region of the XSAVE area.

		*/

		// According 13.8.2, the processor reads bytes 63:16 of the XSAVE header to make sure that all are 0.
		// This code accounts for this memory accesss.
		if (recordVals)
		{
			FillMemAccess0FromStateComps(acccessDesc_0, 512 + 16, 48, xsaveAreaBaseInt, PIN_MEMOP_LOAD);
			accesses_0->push_back(acccessDesc_0);
		}
		else
		{
			FillMemAccess1FromStateComps(acccessDesc_1, 512 + 16, 48, xsaveAreaBaseInt, PIN_MEMOP_LOAD);
			accesses_1->accesses.push_back(acccessDesc_1);
		}

		// Accounts for restoring the x87 state component. The offset and size of this component is fixed.
		if (((rfbm & XSAVE_STATE_COMPONENT_MASK_X87) == XSAVE_STATE_COMPONENT_MASK_X87) && 
			((xstate_bv & XSAVE_STATE_COMPONENT_MASK_X87) == XSAVE_STATE_COMPONENT_MASK_X87))
		{
			size += LegacyStateComponentSizes[0];
			size += LegacyStateComponentSizes[1];
			if (recordVals)
			{
				FillMemAccess0FromStateComps(acccessDesc_0, LegacyStateComponentOffsets[0], LegacyStateComponentSizes[0], xsaveAreaBaseInt, PIN_MEMOP_LOAD);
				accesses_0->push_back(acccessDesc_0);

				FillMemAccess0FromStateComps(acccessDesc_0, LegacyStateComponentOffsets[1], LegacyStateComponentSizes[1], xsaveAreaBaseInt, PIN_MEMOP_LOAD);
				accesses_0->push_back(acccessDesc_0);
			}
			else
			{
				FillMemAccess1FromStateComps(acccessDesc_1, LegacyStateComponentOffsets[0], LegacyStateComponentSizes[0], xsaveAreaBaseInt, PIN_MEMOP_LOAD);
				accesses_1->accesses.push_back(acccessDesc_1);

				FillMemAccess1FromStateComps(acccessDesc_1, LegacyStateComponentOffsets[1], LegacyStateComponentSizes[1], xsaveAreaBaseInt, PIN_MEMOP_LOAD);
				accesses_1->accesses.push_back(acccessDesc_1);
			}
		}

		
		/*
		From 13.8.2:

		If XSTATE_BV[1] = 0, the compacted form XRSTOR initializes MXCSR to 1F80H. (This differs from the standard
		from of XRSTOR, which loads MXCSR from the XSAVE area whenever either RFBM[1] or RFBM[2] is set.)

		*/
		// Accounts for restoring the SSE state component.
		if (((rfbm & XSAVE_STATE_COMPONENT_MASK_SSE) == XSAVE_STATE_COMPONENT_MASK_SSE) && 
			((xstate_bv & XSAVE_STATE_COMPONENT_MASK_SSE) == XSAVE_STATE_COMPONENT_MASK_SSE))
		{
			size += LegacyStateComponentSizes[2];
			size += (is32bit ? LegacyStateComponentSizes[3] : LegacyStateComponentSizes[4]);
			if (recordVals)
			{
				FillMemAccess0FromStateComps(acccessDesc_0, LegacyStateComponentOffsets[2], LegacyStateComponentSizes[2], xsaveAreaBaseInt, PIN_MEMOP_LOAD);
				accesses_0->push_back(acccessDesc_0);

				FillMemAccess0FromStateComps(acccessDesc_0, is32bit ? LegacyStateComponentOffsets[3] : LegacyStateComponentOffsets[4],
					is32bit ? LegacyStateComponentSizes[3] : LegacyStateComponentSizes[4], xsaveAreaBaseInt, PIN_MEMOP_LOAD);
				accesses_0->push_back(acccessDesc_0);
			}
			else
			{
				FillMemAccess1FromStateComps(acccessDesc_1, LegacyStateComponentOffsets[2], LegacyStateComponentSizes[2], xsaveAreaBaseInt, PIN_MEMOP_LOAD);
				accesses_1->accesses.push_back(acccessDesc_1);

				FillMemAccess1FromStateComps(acccessDesc_1, is32bit ? LegacyStateComponentOffsets[3] : LegacyStateComponentOffsets[4], 
					is32bit ? LegacyStateComponentSizes[3] : LegacyStateComponentSizes[4], xsaveAreaBaseInt, PIN_MEMOP_LOAD);
				accesses_1->accesses.push_back(acccessDesc_1);
			}
		}

		// Accounts for restoring the AVX state component.
		if (((rfbm & XSAVE_STATE_COMPONENT_MASK_AVX) == XSAVE_STATE_COMPONENT_MASK_AVX) && 
			((xstate_bv & XSAVE_STATE_COMPONENT_MASK_AVX) == XSAVE_STATE_COMPONENT_MASK_AVX))
		{
			BOOL align = GetStateComponentOffset(XSAVE_STATE_COMPONENT_AVX, offset, compSize);
			lastOffset = offset + compSize;
			// No need to check alignment here.
			ASSERTX(!align || ((lastOffset & 0x3F) == 0));
			size += compSize;
			SAVE_COMPONENT_STATE_EXTENDED(PIN_MEMOP_LOAD);
		}

		// Accounts for restoring the BNDREG state of the MPX state component.
		if (((rfbm & XSAVE_STATE_COMPONENT_MASK_MPX_1) == XSAVE_STATE_COMPONENT_MASK_MPX_1) && 
			((xstate_bv & XSAVE_STATE_COMPONENT_MASK_MPX_1) == XSAVE_STATE_COMPONENT_MASK_MPX_1))
		{
			PROCESS_STATE_COMPONENT(XSAVE_STATE_COMPONENT_MPX_1, PIN_MEMOP_LOAD);
		}

		// Accounts for restoring the BNDCSR state of the MPX state component.
		if (((rfbm & XSAVE_STATE_COMPONENT_MASK_MPX_2) == XSAVE_STATE_COMPONENT_MASK_MPX_2) && 
			((xstate_bv & XSAVE_STATE_COMPONENT_MASK_MPX_2) == XSAVE_STATE_COMPONENT_MASK_MPX_2))
		{
			PROCESS_STATE_COMPONENT(XSAVE_STATE_COMPONENT_MPX_2, PIN_MEMOP_LOAD);
		}

		// Accounts for restoring the Opmask state of the AVX512 state component.
		if (((rfbm & XSAVE_STATE_COMPONENT_MASK_AVX512_1) == XSAVE_STATE_COMPONENT_MASK_AVX512_1) && 
			((xstate_bv & XSAVE_STATE_COMPONENT_MASK_AVX512_1) == XSAVE_STATE_COMPONENT_MASK_AVX512_1))
		{
			PROCESS_STATE_COMPONENT(XSAVE_STATE_COMPONENT_AVX512_1, PIN_MEMOP_LOAD);
		}

		// Accounts for restoring the ZMM_Hi256 state of the AVX512 state component.
		if (((rfbm & XSAVE_STATE_COMPONENT_MASK_AVX512_2) == XSAVE_STATE_COMPONENT_MASK_AVX512_2) && 
			((xstate_bv & XSAVE_STATE_COMPONENT_MASK_AVX512_2) == XSAVE_STATE_COMPONENT_MASK_AVX512_2))
		{
			PROCESS_STATE_COMPONENT(XSAVE_STATE_COMPONENT_AVX512_2, PIN_MEMOP_LOAD);
		}

		// Accounts for restoring the Hi16_ZMM state of the AVX512 state component.
		if (((rfbm & XSAVE_STATE_COMPONENT_MASK_AVX512_3) == XSAVE_STATE_COMPONENT_MASK_AVX512_3) && 
			((xstate_bv & XSAVE_STATE_COMPONENT_MASK_AVX512_3) == XSAVE_STATE_COMPONENT_MASK_AVX512_3))
		{
			PROCESS_STATE_COMPONENT(XSAVE_STATE_COMPONENT_AVX512_3, PIN_MEMOP_LOAD);
		}

		// Accounts for restoring the PKRU state component.
		if (((rfbm & XSAVE_STATE_COMPONENT_MASK_PKRU) == XSAVE_STATE_COMPONENT_MASK_PKRU) && 
			((xstate_bv & XSAVE_STATE_COMPONENT_MASK_PKRU) == XSAVE_STATE_COMPONENT_MASK_PKRU))
		{
			PROCESS_STATE_COMPONENT(XSAVE_STATE_COMPONENT_PKRU, PIN_MEMOP_LOAD);
		}
	}
	else
	{
		/*

		The code that follows handles the standard format of the extended region of the XSAVE area.

		*/

		// According 13.8.1, the processor reads bytes 23:8 of the XSAVE header to make sure that all are 0.
		// This code accounts for this memory accesss.
		if (recordVals)
		{
			FillMemAccess0FromStateComps(acccessDesc_0, 512 + 16, 16, xsaveAreaBaseInt, PIN_MEMOP_LOAD);
			accesses_0->push_back(acccessDesc_0);
		}
		else
		{
			FillMemAccess1FromStateComps(acccessDesc_1, 512 + 16, 16, xsaveAreaBaseInt, PIN_MEMOP_LOAD);
			accesses_1->accesses.push_back(acccessDesc_1);
		}

		if (((rfbm & XSAVE_STATE_COMPONENT_MASK_X87) == XSAVE_STATE_COMPONENT_MASK_X87) && 
			((xstate_bv & XSAVE_STATE_COMPONENT_MASK_X87) == XSAVE_STATE_COMPONENT_MASK_X87))
		{
			size += LegacyStateComponentSizes[0];
			size += LegacyStateComponentSizes[1];
			if (recordVals)
			{
				FillMemAccess0FromStateComps(acccessDesc_0, LegacyStateComponentOffsets[0], LegacyStateComponentSizes[0], xsaveAreaBaseInt, PIN_MEMOP_LOAD);
				accesses_0->push_back(acccessDesc_0);

				FillMemAccess0FromStateComps(acccessDesc_0, LegacyStateComponentOffsets[1], LegacyStateComponentSizes[1], xsaveAreaBaseInt, PIN_MEMOP_LOAD);
				accesses_0->push_back(acccessDesc_0);
			}
			else
			{
				FillMemAccess1FromStateComps(acccessDesc_1, LegacyStateComponentOffsets[0], LegacyStateComponentSizes[0], xsaveAreaBaseInt, PIN_MEMOP_LOAD);
				accesses_1->accesses.push_back(acccessDesc_1);

				FillMemAccess1FromStateComps(acccessDesc_1, LegacyStateComponentOffsets[1], LegacyStateComponentSizes[1], xsaveAreaBaseInt, PIN_MEMOP_LOAD);
				accesses_1->accesses.push_back(acccessDesc_1);
			}
		}

		/*
		From 13.8.2:

		If XSTATE_BV[1] = 0, the compacted form XRSTOR initializes MXCSR to 1F80H. (This differs from the standard
		from of XRSTOR, which loads MXCSR from the XSAVE area whenever either RFBM[1] or RFBM[2] is set.)

		*/

		if (((rfbm & XSAVE_STATE_COMPONENT_MASK_SSE) == XSAVE_STATE_COMPONENT_MASK_SSE) && 
			((xstate_bv & XSAVE_STATE_COMPONENT_MASK_SSE) == XSAVE_STATE_COMPONENT_MASK_SSE))
		{
			size += 4; // Consider only MXCSR_MASK and not all of LegacyStateComponentSizes[2].
			size += (is32bit ? LegacyStateComponentSizes[3] : LegacyStateComponentSizes[4]);
			if (recordVals)
			{
				FillMemAccess0FromStateComps(acccessDesc_0, LegacyStateComponentOffsets[2] + 4, 4, xsaveAreaBaseInt, PIN_MEMOP_LOAD);
				accesses_0->push_back(acccessDesc_0);

				FillMemAccess0FromStateComps(acccessDesc_0, is32bit ? LegacyStateComponentOffsets[3] : LegacyStateComponentOffsets[4],
					is32bit ? LegacyStateComponentSizes[3] : LegacyStateComponentSizes[4], xsaveAreaBaseInt, PIN_MEMOP_LOAD);
				accesses_0->push_back(acccessDesc_0);
			}
			else
			{
				FillMemAccess1FromStateComps(acccessDesc_1, LegacyStateComponentOffsets[2] + 4, 4, xsaveAreaBaseInt, PIN_MEMOP_LOAD);
				accesses_1->accesses.push_back(acccessDesc_1);

				FillMemAccess1FromStateComps(acccessDesc_1, is32bit ? LegacyStateComponentOffsets[3] : LegacyStateComponentOffsets[4], 
					is32bit ? LegacyStateComponentSizes[3] : LegacyStateComponentSizes[4], xsaveAreaBaseInt, PIN_MEMOP_LOAD);
				accesses_1->accesses.push_back(acccessDesc_1);
			}
		}

		if (((rfbm & XSAVE_STATE_COMPONENT_MASK_AVX) == XSAVE_STATE_COMPONENT_MASK_AVX) && 
			((xstate_bv & XSAVE_STATE_COMPONENT_MASK_AVX) == XSAVE_STATE_COMPONENT_MASK_AVX))
		{
			GetStateComponentOffset(XSAVE_STATE_COMPONENT_AVX, offset, compSize);
			size += compSize;
			SAVE_COMPONENT_STATE_EXTENDED(PIN_MEMOP_LOAD);
		}

		/*
		
		13.8.1:
		The MXCSR register is part of state component 1, SSE state (see Section 13.5.2). However, the standard form of
		XRSTOR loads the MXCSR register from memory whenever the RFBM[1] (SSE) or RFBM[2] (AVX) is set, regardless
		of the values of XSTATE_BV[1] and XSTATE_BV[2].

		*/

		if (((rfbm & XSAVE_STATE_COMPONENT_MASK_SSE) == XSAVE_STATE_COMPONENT_MASK_SSE)  ||
			(((rfbm & XSAVE_STATE_COMPONENT_MASK_AVX) == XSAVE_STATE_COMPONENT_MASK_AVX)))
		{
			size += 4; // Consider only MXCSR and not all of LegacyStateComponentSizes[2].
			if (recordVals)
			{
				FillMemAccess0FromStateComps(acccessDesc_0, LegacyStateComponentOffsets[2], 4, xsaveAreaBaseInt, PIN_MEMOP_LOAD);
				accesses_0->push_back(acccessDesc_0);
			}
			else
			{
				FillMemAccess1FromStateComps(acccessDesc_1, LegacyStateComponentOffsets[2], 4, xsaveAreaBaseInt, PIN_MEMOP_LOAD);
				accesses_1->accesses.push_back(acccessDesc_1);
			}
		}

		if (((rfbm & XSAVE_STATE_COMPONENT_MASK_MPX_1) == XSAVE_STATE_COMPONENT_MASK_MPX_1) && 
			((xstate_bv & XSAVE_STATE_COMPONENT_MASK_MPX_1) == XSAVE_STATE_COMPONENT_MASK_MPX_1))
		{
			GetStateComponentOffset(XSAVE_STATE_COMPONENT_MPX_1, offset, compSize);
			size += compSize;
			SAVE_COMPONENT_STATE_EXTENDED(PIN_MEMOP_LOAD);
		}

		if (((rfbm & XSAVE_STATE_COMPONENT_MASK_MPX_2) == XSAVE_STATE_COMPONENT_MASK_MPX_2) && 
			((xstate_bv & XSAVE_STATE_COMPONENT_MASK_MPX_2) == XSAVE_STATE_COMPONENT_MASK_MPX_2))
		{
			GetStateComponentOffset(XSAVE_STATE_COMPONENT_MPX_2, offset, compSize);
			size += compSize;
			SAVE_COMPONENT_STATE_EXTENDED(PIN_MEMOP_LOAD);
		}

		if (((rfbm & XSAVE_STATE_COMPONENT_MASK_AVX512_1) == XSAVE_STATE_COMPONENT_MASK_AVX512_1) && 
			((xstate_bv & XSAVE_STATE_COMPONENT_MASK_AVX512_1) == XSAVE_STATE_COMPONENT_MASK_AVX512_1))
		{
			GetStateComponentOffset(XSAVE_STATE_COMPONENT_AVX512_1, offset, compSize);
			size += compSize;
			SAVE_COMPONENT_STATE_EXTENDED(PIN_MEMOP_LOAD);
		}

		if (((rfbm & XSAVE_STATE_COMPONENT_MASK_AVX512_2) == XSAVE_STATE_COMPONENT_MASK_AVX512_2) && 
			((xstate_bv & XSAVE_STATE_COMPONENT_MASK_AVX512_2) == XSAVE_STATE_COMPONENT_MASK_AVX512_2))
		{
			GetStateComponentOffset(XSAVE_STATE_COMPONENT_AVX512_2, offset, compSize);
			size += compSize;
			SAVE_COMPONENT_STATE_EXTENDED(PIN_MEMOP_LOAD);
		}
		if (((rfbm & XSAVE_STATE_COMPONENT_MASK_AVX512_3) == XSAVE_STATE_COMPONENT_MASK_AVX512_3) && 
			((xstate_bv & XSAVE_STATE_COMPONENT_MASK_AVX512_3) == XSAVE_STATE_COMPONENT_MASK_AVX512_3))
		{
			GetStateComponentOffset(XSAVE_STATE_COMPONENT_AVX512_3, offset, compSize);
			size += compSize;
			SAVE_COMPONENT_STATE_EXTENDED(PIN_MEMOP_LOAD);
		}

		if (((rfbm & XSAVE_STATE_COMPONENT_MASK_PKRU) == XSAVE_STATE_COMPONENT_MASK_PKRU) && 
			((xstate_bv & XSAVE_STATE_COMPONENT_MASK_PKRU) == XSAVE_STATE_COMPONENT_MASK_PKRU))
		{
			GetStateComponentOffset(XSAVE_STATE_COMPONENT_PKRU, offset, compSize);
			size += compSize;
			SAVE_COMPONENT_STATE_EXTENDED(PIN_MEMOP_LOAD);
		}
	}

	if ((rfbm & !XSAVE_STATE_COMPONENT_MASK_ALL) || (xstate_bv & !XSAVE_STATE_COMPONENT_MASK_ALL))
	{
		ALLERIA_WriteMessage(ALLERIA_REPORT_XSAVE_UNKOWN_STATE);
	}

	if (recordVals)
		return accesses_0;
	else
		return accesses_1;
}

// This handy function was adapted from the debugtrace pintool.
std::string ShowN(UINT32 n, const VOID *ea)
{
	std::stringstream ss;
	// Print out the bytes in "big endian even though they are in memory little endian.
	// This is most natural for 8B and 16B quantities that show up most frequently.
	// The address pointed to 
	ss << std::hex << std::setfill('0');
	UINT8 b[64];
	UINT8* x;
	if (n > 64)
		x = new UINT8[n];
	else
		x = b;
	ASSERTX(PIN_SafeCopy(x, static_cast<const UINT8*>(ea), n) == n);
	for (UINT32 i = 0; i < n; i++)
	{
		ss << std::setw(2) << static_cast<UINT32>(x[n - i - 1]);
		if (((reinterpret_cast<ADDRINT>(ea)+n - i - 1) & 0x3) == 0 && i < n - 1)
			ss << "_";
	}
	if (n > 64)
		delete[] x;

	return ss.str();
}

const std::wstring StringToWString(const std::string& s)
{
	std::wstring ws(s.begin(), s.end());
	return ws;
}

VOID WriteBinaryFuncTableIndex(std::ofstream& ofs, UINT32 index)
{
	if ((index & 0x3F) == index) // 6 bits.
	{
		index = (index << 2) | 0x0;
		ofs.write(reinterpret_cast<char*>(&index), 1);
	}
	else if ((index & 0x3FFF) == index) // 14 bits.
	{
		index = (index << 2) | 0x1;
		ofs.write(reinterpret_cast<char*>(&index), 2);
	}
	else if ((index & 0x3FFFFF) == index) // 22 bits.
	{
		index = (index << 2) | 0x2;
		ofs.write(reinterpret_cast<char*>(&index), 3);
	}
	else if ((index & 0x3FFFFFFF) == index) // 30 bits.
	{
		index = (index << 2) | 0x3;
		ofs.write(reinterpret_cast<char*>(&index), 4);
	}
	else
	{
		// This case occurs when there are more than 2^30 functions, which
		// is extremely unlikely. Anyway, if it happens then make it point
		// to the unkown function slot.
		index = 0;
		ofs.write(reinterpret_cast<char*>(&index), 1);
	}
}

xed_error_enum_t InsDecode(UINT8 *ip, xed_decoded_inst_t& xedd, size_t byteCount) {
#if defined(TARGET_IA32E)
	static const xed_state_t dstate = { XED_MACHINE_MODE_LONG_64, XED_ADDRESS_WIDTH_64b };
#else
	static const xed_state_t dstate = { XED_MACHINE_MODE_LEGACY_32, XED_ADDRESS_WIDTH_32b };
#endif
	xed_decoded_inst_zero_set_mode(&xedd, &dstate);
	xed_error_enum_t xed_code = xed_decode(&xedd, ip, static_cast<UINT32>(byteCount));
	return xed_code;
}

VOID WriteBinaryXsaveInfoAndDispose(std::ofstream& ofs, std::vector<MEM_ACCESS_GENERAL_0> *accessInfo,
	UINT64 **physicalAddressesMemRef)
{
	// The base address of the XSAVE instruction should be written by the caller before
	// calling this function.
	UINT64 *physicalAddresses = *physicalAddressesMemRef;
	UINT32 numberOfMemops = (UINT32)accessInfo->size();
	UINT8 memOpType;
	WriteBinary(ofs, numberOfMemops);

	for (UINT32 i = 0; i < numberOfMemops; ++i)
	{
		ofs.write(reinterpret_cast<char*>(&(*accessInfo)[i].addr), g_virtualAddressSize);
		if (g_v2pTransMode != V2P_TRANS_MODE_OFF)
		{
			ofs.write(reinterpret_cast<char*>(physicalAddresses), g_physicalAddressSize);
			++physicalAddresses;
		}

		BOOL isValueValid = (*accessInfo)[i].value == NULL ? FALSE : TRUE;
		WriteBinary(ofs, (*accessInfo)[i].bytesAccessed);
		memOpType = (*accessInfo)[i].memopType;
		WriteBinary(ofs, memOpType);
		WriteBinary(ofs, isValueValid);
		if (isValueValid)
		{
			ofs.write(reinterpret_cast<char*>((*accessInfo)[i].value), (*accessInfo)[i].bytesAccessed);
			delete[] reinterpret_cast<VOID*>((*accessInfo)[i].value);
		}
	}

	*physicalAddressesMemRef = physicalAddresses;
	delete accessInfo;
}

VOID WriteBinaryXsaveAddrsAndDispose(std::ofstream& ofs, ALLERIA_XSAVE_MEM_ACCESS_ADDRS *accessInfo,
	UINT64 **physicalAddressesMemRef)
{
	UINT64 *physicalAddresses = *physicalAddressesMemRef;
	UINT32 numberOfMemops = (UINT32)accessInfo->accesses.size();
	UINT8 memOpType;
	WriteBinary(ofs, numberOfMemops);

	for (UINT32 i = 0; i < numberOfMemops; ++i)
	{
		ofs.write(reinterpret_cast<char*>(&accessInfo->accesses[i].addr), g_virtualAddressSize);
		if (g_v2pTransMode != V2P_TRANS_MODE_OFF)
		{
			ofs.write(reinterpret_cast<char*>(physicalAddresses), g_physicalAddressSize);
			++physicalAddresses;
		}

		WriteBinary(ofs, accessInfo->accesses[i].bytesAccessed);
		memOpType = accessInfo->accesses[i].memopType;
		WriteBinary(ofs, memOpType);
	}

	*physicalAddressesMemRef = physicalAddresses;
	delete accessInfo;
}

std::string MultiMemXsaveInfoStringAndDispose(std::vector<MEM_ACCESS_GENERAL_0> *accessInfo,
	UINT64 **physicalAddressesMemRef)
{
	UINT64 *physicalAddresses = *physicalAddressesMemRef;
	UINT32 numberOfMemops = (UINT32)accessInfo->size();
	std::stringstream ss;
	ss << std::hex;

	for (UINT32 i = 0; i < numberOfMemops; ++i)
	{
		std::string value = "";
		if ((*accessInfo)[i].value != NULL)
		{
			value = ShowN((*accessInfo)[i].bytesAccessed, reinterpret_cast<VOID*>((*accessInfo)[i].value));
			delete[] reinterpret_cast<VOID*>((*accessInfo)[i].value);
		}

		if ((*accessInfo)[i].memopType == PIN_MEMOP_LOAD)
		{
			ss << value << " = *" << (*accessInfo)[i].addr;
			if (g_v2pTransMode != V2P_TRANS_MODE_OFF)
			{
				ss << "/" << *physicalAddresses;
				++physicalAddresses;
			}
			ss << " R " << (*accessInfo)[i].bytesAccessed << " bytes";
		}
		else
		{
			ss << "*" << (*accessInfo)[i].addr;
			if (g_v2pTransMode != V2P_TRANS_MODE_OFF)
			{
				ss << "/" << *physicalAddresses;
				++physicalAddresses;
			}
			ss << " = " << value << " W " << (*accessInfo)[i].bytesAccessed << " bytes";
		}

		if (i < numberOfMemops - 1)
			ss << ", ";
	}

	*physicalAddressesMemRef = physicalAddresses;
	delete accessInfo;
	return ss.str();
}

std::string MultiMemXsaveAddrsStringAndDispose(ALLERIA_XSAVE_MEM_ACCESS_ADDRS *accessInfo,
	UINT64 **physicalAddressesMemRef)
{
	UINT64 *physicalAddresses = *physicalAddressesMemRef;
	UINT32 numberOfMemops = (UINT32)accessInfo->accesses.size();
	std::stringstream ss;
	ss << std::hex;

	for (UINT32 i = 0; i < numberOfMemops; ++i)
	{
		ss << accessInfo->accesses[i].addr;
		if (g_v2pTransMode != V2P_TRANS_MODE_OFF)
		{
			ss << "/" << *physicalAddresses;
			++physicalAddresses;
		}

		if (accessInfo->accesses[i].memopType == PIN_MEMOP_LOAD)
		{
			ss << " R " << accessInfo->accesses[i].bytesAccessed << " bytes";
		}
		else
		{
			ss << " W " << accessInfo->accesses[i].bytesAccessed << " bytes";
		}

		if (i < numberOfMemops - 1)
			ss << ", ";
	}

	*physicalAddressesMemRef = physicalAddresses;
	delete accessInfo;
	return ss.str();
}

VOID WriteBinaryMultiMemAccessInfoAndDispose(std::ofstream& ofs, ALLERIA_MULTI_MEM_ACCESS_INFO *accesses,
	UINT64 **physicalAddressesMemRef)
{
	// The base address of the vgather instruction should be written by the caller before
	// calling this function.
	UINT64 *physicalAddresses = *physicalAddressesMemRef;
	UINT64 invalidAddr = PHYSICAL_ADDRESS_INVALID;
	WriteBinary(ofs, accesses->numberOfMemops);
	for (UINT32 i = 0; i < accesses->numberOfMemops; ++i)
	{
		WriteBinary(ofs, accesses->memop[i].maskOn);
		ofs.write(reinterpret_cast<char*>(&accesses->memop[i].memoryAddress), g_virtualAddressSize);
		if (g_v2pTransMode != V2P_TRANS_MODE_OFF)
		{
			if (accesses->memop[i].maskOn)
			{
				ofs.write(reinterpret_cast<char*>(physicalAddresses), g_physicalAddressSize);
				++physicalAddresses;
			}
			else
			{
				ofs.write(reinterpret_cast<char*>(&invalidAddr), g_physicalAddressSize);
			}
		}
		if (accesses->memop[i].maskOn)
		{
			BOOL isValueValid = accesses->values[i] == NULL ? FALSE : TRUE;
			WriteBinary(ofs, accesses->memop[i].bytesAccessed);
			WriteBinary(ofs, isValueValid);
			if (isValueValid)
			{
				ofs.write(reinterpret_cast<char*>(accesses->values[i]), accesses->memop[i].bytesAccessed);
				delete[] reinterpret_cast<VOID*>(accesses->values[i]);
			}
		}
	}
	*physicalAddressesMemRef = physicalAddresses;
	delete accesses;
}

VOID WriteBinaryMultiMemAccessAddrsAndDispose(std::ofstream& ofs, ALLERIA_MULTI_MEM_ACCESS_ADDRS *addrs,
	UINT64 **physicalAddressesMemRef)
{
	UINT64 *physicalAddresses = *physicalAddressesMemRef;
	UINT64 invalidAddr = PHYSICAL_ADDRESS_INVALID;
	WriteBinary(ofs, addrs->numberOfMemops);
	for (UINT32 i = 0; i < addrs->numberOfMemops; ++i)
	{
		WriteBinary(ofs, addrs->maskOn[i]);
		ofs.write(reinterpret_cast<char*>(&addrs->addrs[i]), g_virtualAddressSize);
		if (g_v2pTransMode != V2P_TRANS_MODE_OFF)
		{
			if (addrs->maskOn[i])
			{
				ofs.write(reinterpret_cast<char*>(physicalAddresses), g_physicalAddressSize);
				++physicalAddresses;
			}
			else
			{
				ofs.write(reinterpret_cast<char*>(&invalidAddr), g_physicalAddressSize);
			}
		}
		if (addrs->maskOn[i])
		{
			WriteBinary(ofs, addrs->sizes[i]);
		}
	}
	*physicalAddressesMemRef = physicalAddresses;
	delete addrs;
}

std::string MultiMemAccessInfoStringAndDispose(ALLERIA_MULTI_MEM_ACCESS_INFO *accesses,
	UINT64 **physicalAddressesMemRef)
{
	std::stringstream ss;
	ss << std::hex;
	UINT64 *physicalAddresses = *physicalAddressesMemRef;

	for (UINT32 i = 0; i < accesses->numberOfMemops; ++i)
	{
		if (accesses->memop[i].maskOn)
		{
			std::string value = "";
			if (accesses->values[i] != NULL)
			{
				value = ShowN(accesses->memop[i].bytesAccessed, reinterpret_cast<VOID*>(accesses->values[i]));
				delete[] reinterpret_cast<VOID*>(accesses->values[i]);
			}

			if (accesses->memop[i].memopType == PIN_MEMOP_LOAD)
			{
				ss << value << " = *" << accesses->memop[i].memoryAddress;
				if (g_v2pTransMode != V2P_TRANS_MODE_OFF)
				{
					ss << "/" << *physicalAddresses;
					++physicalAddresses;
				}
				ss << " R " << accesses->memop[i].bytesAccessed << " bytes";
			}
			else
			{
				ss << "*" << accesses->memop[i].memoryAddress;
				if (g_v2pTransMode != V2P_TRANS_MODE_OFF)
				{
					ss << "/" << *physicalAddresses;
					++physicalAddresses;
				}
				ss << " = " << value << " W " << accesses->memop[i].bytesAccessed << " bytes";
			}
			
			if (i < accesses->numberOfMemops - 1)
				ss << ", ";
		}
	}

	*physicalAddressesMemRef = physicalAddresses;
	delete accesses;
	return ss.str();
}

std::string MultiMemAccessAddrsStringAndDispose(ALLERIA_MULTI_MEM_ACCESS_ADDRS *addrs,
	UINT64 **physicalAddressesMemRef)
{
	std::stringstream ss;
	ss << std::hex;
	UINT64 *physicalAddresses = *physicalAddressesMemRef;

	for (UINT32 i = 0; i < addrs->numberOfMemops; ++i)
	{
		if (addrs->maskOn[i])
		{
			ss << addrs->addrs[i];
			if (g_v2pTransMode != V2P_TRANS_MODE_OFF)
			{
				ss << "/" << *physicalAddresses;
				++physicalAddresses;
			}

			
			// I'll just simplify the code here.
			// VGATHER always reads from memory.
			/*if (addrs->memop[i].memopType == PIN_MEMOP_LOAD)
			{*/
				
			ss << " R " << addrs->sizes[i] << " bytes";
			/*}
			else
			{
				ss << " W";
			}*/

			if (i < addrs->numberOfMemops - 1)
				ss << ", ";
		}
	}

	*physicalAddressesMemRef = physicalAddresses;
	delete addrs;
	return ss.str();
}

UINT32 BufferTypeToSize(BUFFER_ENTRY_TYPE type)
{
	switch (type)
	{
	case BUFFER_ENTRY_TYPE_INVALID:
		return 0;
	case BUFFER_ENTRY_TYPE_IR0:
		return sizeof(INS_RECORD_0);
		break;
	case BUFFER_ENTRY_TYPE_IR1:
		return sizeof(INS_RECORD_1);
	case BUFFER_ENTRY_TYPE_IR2:
		return sizeof(INS_RECORD_2);
	case BUFFER_ENTRY_TYPE_MR0:
		return sizeof(MEM_REF_0);
	case BUFFER_ENTRY_TYPE_MR1:
		return sizeof(MEM_REF_1);
	case BUFFER_ENTRY_TYPE_MR2:
		return sizeof(MEM_REF_2);
	case BUFFER_ENTRY_TYPE_PROCESSOR_RECORD:
		return sizeof(PROCESSOR_RECORD);
	case BUFFER_ENTRY_TYPE_SYS_CALL_RECORD:
		return sizeof(SYS_CALL_RECORD);
	case BUFFER_ENTRY_TYPE_BRANCH_RECORD:
		return sizeof(BRANCH_RECORD);
	default:
		return -1;
	}
}

xed_syntax_enum_t Str2AsmSyntax(const CHAR *str)
{
	if (strcmp(str, "intel") == 0)
		return xed_syntax_enum_t::XED_SYNTAX_INTEL;
	else if (strcmp(str, "att") == 0)
		return xed_syntax_enum_t::XED_SYNTAX_ATT;
	else if (strcmp(str, "xed") == 0)
		return xed_syntax_enum_t::XED_SYNTAX_XED;
	else
		return xed_syntax_enum_t::XED_SYNTAX_INVALID;
}