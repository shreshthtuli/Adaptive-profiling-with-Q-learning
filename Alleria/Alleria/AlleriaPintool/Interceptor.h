#ifndef INTERCEPTOR_H
#define INTERCEPTOR_H

#include <vector>

#include "pin.H"
#include "Utils.h"
#include "PortableApi.h"
#include "WindowConfig.h"
#include "InterceptorMemoryManager.h"

VOID PIN_FAST_ANALYSIS_CALL AnalyzeXxAllocateVirtualMemoryPre(ADDRINT processHandle, ADDRINT *address);
VOID PIN_FAST_ANALYSIS_CALL AnalyzeXxAllocateVirtualMemoryPost(UINT32 returnVal);
VOID PIN_FAST_ANALYSIS_CALL AnalyzeXxFreeVirtualMemoryPre(ADDRINT processHandle, ADDRINT *address, UINT32 freeType);
VOID PIN_FAST_ANALYSIS_CALL AnalyzeXxFreeVirtualMemoryPost(UINT32 returnVal);

VOID PIN_FAST_ANALYSIS_CALL AnalyzeInterestingFuncCall(ADDRINT funcAddr);
VOID PIN_FAST_ANALYSIS_CALL AnalyzeInterestingFuncRet(ADDRINT funcAddr);

VOID PIN_FAST_ANALYSIS_CALL AnalyzeInterceptPre(InterceptorMemoryManager *memMan, WINDOW_CONFIG::INTERCEPT_CONFIG *config, UINT8 *arg, UINT32 argIndex);
VOID PIN_FAST_ANALYSIS_CALL AnalyzeInterceptPre2(InterceptorMemoryManager *memMan, ADDRINT returnAddr, INT32 rtnId);
VOID PIN_FAST_ANALYSIS_CALL AnalyzeInterceptPost(InterceptorMemoryManager *memMan, INT32 rtnId, UINT8 *retVal);


#endif /* INTERCEPTOR_H */