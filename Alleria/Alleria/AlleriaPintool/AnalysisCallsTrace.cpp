#include "AnalysisCallsTrace.h"
#include "GlobalState.h"

ANALYSIS_CALLS_TRACE::ANALYSIS_CALLS_TRACE()
{
	// The first entry is always of type BUFFER_ENTRY_TYPE_PROCESSOR_RECORD at offset zero.
	m_offset = KnobTrackProcessorsEnabled ? sizeof(PROCESSOR_RECORD) : 0;
}

UINT32 ANALYSIS_CALLS_TRACE::RecordCall(INS ins, BUFFER_ENTRY_TYPE iType, BUFFER_ENTRY_TYPE mType)
{
	return RecordCallInternal(ins, iType, mType, BufferTypeToSize(mType));
}

UINT32 ANALYSIS_CALLS_TRACE::RecordMemCall(INS ins, BUFFER_ENTRY_TYPE iType, BUFFER_ENTRY_TYPE mType, UINT32 opCount)
{
	return RecordCallInternal(ins, iType, mType, opCount * BufferTypeToSize(mType));
}

UINT32 ANALYSIS_CALLS_TRACE::RecordCallInternal(INS ins, BUFFER_ENTRY_TYPE iType, BUFFER_ENTRY_TYPE mType, UINT32 mSize)
{
	ANALYSIS_CALL ac;
	ac.iType = iType;
	ac.mType = mType;
	ac.ins = ins;
	ac.offset = m_offset;
	UINT32 size = mSize + BufferTypeToSize(iType);
	m_offset += size;
	m_calls.push_back(ac);
	return size;
}

ADDRINT ANALYSIS_CALLS_TRACE::GetTraceSize()
{
	return m_offset;
}