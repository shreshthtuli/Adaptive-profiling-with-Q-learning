#ifndef INTERCEPTORINFO_H
#define INTERCEPTORINFO_H

#include <sstream>
#include <iomanip>
#include <algorithm>
#include <vector>

#include "pin.H"
#include "WindowConfig.h"
#include "Utils.h"


class INTERCEPTOR_INFO
{
public:
	ADDRINT returnAddr;
	VOID *ret; // Return value.
	std::vector<VOID*> argsIn; // Arguments on entry.
	std::vector<VOID*> argsOut; // Arguments on exit.

	// Used by post-interceptor analysis routine.
	// Do not print.
	INT32 rtnId;

	// Used to store the address of arguments.
	// Do not print.
	std::vector<std::pair<VOID*, UINT32> > argAddresses;

	// The time at which the function was called.
	TIME_STAMP callTimestamp;

	// The time at which the function returned.
	TIME_STAMP returnTimestamp;

	// The interceptor configuration. This is required to access the types of the parameters and return values.
	WINDOW_CONFIG::INTERCEPT_CONFIG *config;

	~INTERCEPTOR_INFO();
	VOID WriteString(std::wostream& os);
	VOID WriteBin(std::ostream& os);
	LOCALFUN UINT32 ComputeFieldSize(const WINDOW_CONFIG::INTERCEPT_TYPE_FIELD& fieldType);
	LOCALFUN BOOL GetPrimitiveTypeSizeAndSign(WINDOW_CONFIG::INTERCEPT_TYPE_SPEC typeSpec, UINT32& size, BOOL& isSigned);

	LOCALFUN VOID EmitBinaryProfileTypesSection(std::ostream& os);
	LOCALFUN UINT32 TypeIndexToGlobalTypeIndex(UINT32 typeIndex, WINDOW_CONFIG::INTERCEPT_TYPE_SPEC typeSpec);

private:
	LOCALFUN UINT32 CustomToString(std::wostream& os, const WINDOW_CONFIG::INTERCEPT_CUSTOM_TYPE& customType, const VOID *object, UINT32 level);
	LOCALFUN VOID ArrayToString(std::wostream& os, const WINDOW_CONFIG::INTERCEPT_TYPE_FIELD& elemenType, size_t count, const VOID *addr, UINT32 level);
	LOCALFUN VOID GenericToString(std::wostream& os, const WINDOW_CONFIG::INTERCEPT_TYPE_FIELD& elemenType, const UINT8 *addr, UINT32 level);

	VOID WriteStringArgs(std::wostream& os, BOOL in);
	VOID WriteStringArgsBin(std::ostream& os, BOOL in);

	LOCALCONST UINT32 levelIndentation = 8;
};

#endif /* INTERCEPTORINFO_H */