#ifndef ANALYSISCALLSTRACE_H
#define ANALYSISCALLSTRACE_H

#include "pin.H"
#include <vector>
#include "BufferTypes.h"
#include "Utils.h"

struct ANALYSIS_CALL
{
	INS ins;
	ADDRINT offset;
	BUFFER_ENTRY_TYPE iType;
	BUFFER_ENTRY_TYPE mType;
};

class ANALYSIS_CALLS_TRACE
{
public:
	ANALYSIS_CALLS_TRACE();
	UINT32 RecordCall(INS ins, BUFFER_ENTRY_TYPE iType, BUFFER_ENTRY_TYPE mType);
	UINT32 RecordMemCall(INS ins, BUFFER_ENTRY_TYPE iType, BUFFER_ENTRY_TYPE mType, UINT32 opCount);
	ADDRINT GetTraceSize();

	std::vector<ANALYSIS_CALL> m_calls;

private:
	UINT32 RecordCallInternal(INS ins, BUFFER_ENTRY_TYPE iType, BUFFER_ENTRY_TYPE mType, UINT32 mSize);

	ADDRINT m_offset;
};

#endif /* ANALYSISCALLSTRACE_H */