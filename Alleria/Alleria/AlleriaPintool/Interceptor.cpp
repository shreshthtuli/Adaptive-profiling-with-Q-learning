#include "Interceptor.h"
#include "GlobalState.h"
#include "MemoryRegistry.h"
#include "LocalState.h"
#include "InterceptorInfo.h"

extern LOCAL_STATE lstate;

VOID PIN_FAST_ANALYSIS_CALL AnalyzeXxAllocateVirtualMemoryPre(ADDRINT processHandle, ADDRINT *address)
{
	if (processHandle != GLOBAL_STATE::currentProcessHandle)
		return;

	PIN_SetThreadData(lstate.memAllocInfo, address, PIN_ThreadId());
}

VOID PIN_FAST_ANALYSIS_CALL AnalyzeXxAllocateVirtualMemoryPost(UINT32 returnVal)
{
	ADDRINT *addr = reinterpret_cast<ADDRINT*>(PIN_GetThreadData(lstate.memAllocInfo, PIN_ThreadId()));
	if (returnVal == 0 /* STATUS_SUCCESS */
		&& addr != NULL)
	{
		MEMORY_REGISTRY::RegisterGeneral(*addr, REGION_USER::App);
	}
	PIN_SetThreadData(lstate.memAllocInfo, NULL, PIN_ThreadId());
}

VOID PIN_FAST_ANALYSIS_CALL AnalyzeXxFreeVirtualMemoryPre(ADDRINT processHandle, ADDRINT *address, UINT32 freeType)
{
	if (processHandle != GLOBAL_STATE::currentProcessHandle || freeType != 0x8000 /* MEM_RELEASE */)
		return;

	PIN_SetThreadData(lstate.memAllocInfo, address, PIN_ThreadId());
}

VOID PIN_FAST_ANALYSIS_CALL AnalyzeXxFreeVirtualMemoryPost(UINT32 returnVal)
{
	ADDRINT *addr = reinterpret_cast<ADDRINT*>(PIN_GetThreadData(lstate.memAllocInfo, PIN_ThreadId()));
	if (returnVal == 0 /* STATUS_SUCCESS */
		&& addr != NULL)
	{
		MEMORY_REGISTRY::Unregister(*addr);
	}
	PIN_SetThreadData(lstate.memAllocInfo, NULL, PIN_ThreadId());
}

VOID PIN_FAST_ANALYSIS_CALL AnalyzeInterestingFuncCall(ADDRINT funcAddr)
{
	PIN_LockClient();
	RTN rtn = RTN_FindByAddress(funcAddr);
	PIN_UnlockClient();
	WINDOW_CONFIG::ReportFunctionCall(rtn);
}

VOID PIN_FAST_ANALYSIS_CALL AnalyzeInterestingFuncRet(ADDRINT funcAddr)
{
	PIN_LockClient();
	RTN rtn = RTN_FindByAddress(funcAddr);
	PIN_UnlockClient();
	WINDOW_CONFIG::ReportFunctionReturn(rtn);
}

LOCALFUN VOID* CaptureBytes(VOID *source, UINT32 size)
{
	VOID *dest = new UINT8[size];
	if (PIN_SafeCopy(dest, source, size) != size)
	{
		delete[] dest;
		dest = NULL;
	}
	return dest;
}

LOCALFUN BOOL IsSignedIntegralType(WINDOW_CONFIG::INTERCEPT_TYPE_SPEC typeSpec)
{
	switch (typeSpec)
	{
	case WINDOW_CONFIG::INTERCEPT_TYPE_SPEC_SINT1:
	case WINDOW_CONFIG::INTERCEPT_TYPE_SPEC_SINT2:
	case WINDOW_CONFIG::INTERCEPT_TYPE_SPEC_SINT3:
	case WINDOW_CONFIG::INTERCEPT_TYPE_SPEC_SINT4:
	case WINDOW_CONFIG::INTERCEPT_TYPE_SPEC_SINT5:
	case WINDOW_CONFIG::INTERCEPT_TYPE_SPEC_SINT6:
	case WINDOW_CONFIG::INTERCEPT_TYPE_SPEC_SINT7:
	case WINDOW_CONFIG::INTERCEPT_TYPE_SPEC_SINT8:
		return TRUE;
	default:
		return FALSE;
	}
}

LOCALFUN VOID* CapturePrimitive(const WINDOW_CONFIG::INTERCEPT_TYPE_FIELD& argTypeDesc, VOID *argument, UINT32 *size, BOOL& wasPrimitive);
LOCALFUN VOID* CapturePointer(const VOID *typeDesc, UINT32 fieldOrParamIndex, BOOL isIntercept, std::vector<VOID*>& args, BOOL& wasPointer);
LOCALFUN VOID* CaptureCustom(const WINDOW_CONFIG::INTERCEPT_TYPE_FIELD& argTypeDesc, VOID *argument, BOOL& wasCustom);

LOCALFUN VOID* CaptureGeneric(VOID *typeDesc, UINT32 fieldOrParamIndex, BOOL isIntercept, std::vector<VOID*>& args)
{
	BOOL success;
	VOID *result;

	WINDOW_CONFIG::INTERCEPT_CONFIG *interceptConfig;
	WINDOW_CONFIG::INTERCEPT_CUSTOM_TYPE *customType;
	WINDOW_CONFIG::INTERCEPT_TYPE_FIELD *interceptTypeField;
	if (isIntercept)
	{
		interceptConfig = reinterpret_cast<WINDOW_CONFIG::INTERCEPT_CONFIG*>(typeDesc);
		interceptTypeField = &interceptConfig->parameters[fieldOrParamIndex].parameter;
	}
	else
	{
		customType = reinterpret_cast<WINDOW_CONFIG::INTERCEPT_CUSTOM_TYPE*>(typeDesc);
		interceptTypeField = &customType->fields[fieldOrParamIndex];
	}

	result = CapturePrimitive(*interceptTypeField, args[fieldOrParamIndex], NULL, success);
	if (!success)
	{
		result = CapturePointer(typeDesc, fieldOrParamIndex, isIntercept, args, success);
		if (!success)
		{
			result = CaptureCustom(*interceptTypeField, args[fieldOrParamIndex], success);
			if (!success)
			{
				ASSERT(FALSE, "Unkown type.");
				return NULL;
			}
		}
	}

	return result;
}

LOCALFUN VOID* CapturePrimitive(const WINDOW_CONFIG::INTERCEPT_TYPE_FIELD& argTypeDesc, VOID *argument, UINT32 *size, BOOL& wasPrimitive)
{
	wasPrimitive = TRUE;
	BOOL isSigned;
	UINT32 byteCount;
	if (!INTERCEPTOR_INFO::GetPrimitiveTypeSizeAndSign(argTypeDesc.typeSpec, byteCount, isSigned))
	{
		wasPrimitive = FALSE;
		return NULL;
	}
	VOID *result = CaptureBytes(argument, byteCount);
	if (result && size)
		*size = byteCount;
	else if (size)
		*size = 0;
	return result;
}

LOCALFUN VOID CaptureAllPointers(const WINDOW_CONFIG::INTERCEPT_CUSTOM_TYPE& customType, VOID *source, VOID *dest)
{
	std::vector<VOID*> args;
	std::vector<UINT32> fieldSizes;
	UINT8 *currentField = reinterpret_cast<UINT8*>(source);
	for (size_t i = 0; i < customType.fields.size(); ++i)
	{
		args.push_back(currentField);
		fieldSizes.push_back(INTERCEPTOR_INFO::ComputeFieldSize(customType.fields[i]));
		currentField += fieldSizes.back();
	}

	// For every pointer field, capture it and replace the pointers.
	BOOL wasPointer;
	UINT8 *currentFieldDest = reinterpret_cast<UINT8*>(dest);
	currentField = reinterpret_cast<UINT8*>(source);
	for (size_t i = 0; i < customType.fields.size(); ++i)
	{
		if (customType.fields[i].typeSpec == WINDOW_CONFIG::INTERCEPT_TYPE_SPEC_POINTER)
		{
			VOID *capturedPointer = CapturePointer(&customType, (UINT32)i, FALSE, args, wasPointer);
			memcpy(currentFieldDest, &capturedPointer, sizeof(VOID*));
		}
		else if (customType.fields[i].typeSpec == WINDOW_CONFIG::INTERCEPT_TYPE_SPEC_CUSTOM)
		{
			CaptureAllPointers(WINDOW_CONFIG::GetCustomType(customType.fields[i].typeIndex), currentField, currentFieldDest);
		}

		currentField += fieldSizes[i];
		currentFieldDest += fieldSizes[i];
	}
}

LOCALFUN VOID* CaptureCustom(const WINDOW_CONFIG::INTERCEPT_TYPE_FIELD& argTypeDesc, VOID *argument, BOOL& wasCustom)
{
	if (argTypeDesc.typeSpec != WINDOW_CONFIG::INTERCEPT_TYPE_SPEC_CUSTOM)
	{
		wasCustom = FALSE;
		return NULL;
	}

	wasCustom = TRUE;
	UINT32 size = INTERCEPTOR_INFO::ComputeFieldSize(argTypeDesc);
	UINT8 *object = new UINT8[size];
	if (PIN_SafeCopy(object, argument, size) != size)
	{
		delete[] object;
		return NULL;
	}
	CaptureAllPointers(WINDOW_CONFIG::GetCustomType(argTypeDesc.typeIndex), argument, object);
	return object;
}

LOCALFUN VOID* CaptureArrayCount(const WINDOW_CONFIG::INTERCEPT_TYPE_FIELD& elemenType, size_t count, BOOL inc, BOOL isCountSigned, VOID **pointerToAddr);
LOCALFUN VOID* CaptureArraySentinel(const WINDOW_CONFIG::INTERCEPT_TYPE_FIELD& elemenType, UINT64 sentinel, BOOL inc, VOID **pointerToAddr);

LOCALFUN VOID CaptureFieldRestricted(const WINDOW_CONFIG::INTERCEPT_TYPE_FIELD& elemenType, VOID *from, VOID *to)
{
	BOOL success;
	UINT32 size = INTERCEPTOR_INFO::ComputeFieldSize(elemenType);
	memset(to, 0, size);
	VOID *result = CapturePrimitive(elemenType, from, &size, success);
	if (!success)
	{
		if (elemenType.typeSpec == WINDOW_CONFIG::INTERCEPT_TYPE_SPEC_POINTER)
		{
			const WINDOW_CONFIG::INTERCEPT_TYPE_POINTER& pointerType =
				WINDOW_CONFIG::GetPointerType(elemenType.typeIndex);
			switch (pointerType.type)
			{
			case WINDOW_CONFIG::INTERCEPT_TYPE_POINTER_TYPE_FIXED:
				result = CaptureArrayCount(
					pointerType.elementType,
					pointerType.fixed.count,
					pointerType.fixed.direction == WINDOW_CONFIG::INTERCEPT_TYPE_POINTER_DIRECTION_INCREMENT ? TRUE : FALSE,
					FALSE,
					reinterpret_cast<VOID**>(from));
				break;
			case WINDOW_CONFIG::INTERCEPT_TYPE_POINTER_TYPE_SENTINEL:
				result = CaptureArraySentinel(
					pointerType.elementType,
					pointerType.sentinel.sentinelValue,
					pointerType.sentinel.direction == WINDOW_CONFIG::INTERCEPT_TYPE_POINTER_DIRECTION_INCREMENT ? TRUE : FALSE,
					reinterpret_cast<VOID**>(from));
				break;
			default:
				result = NULL;
				break;
			}

			if (result)
				memcpy(to, &result, sizeof(VOID*));
		}
		else if (elemenType.typeSpec == WINDOW_CONFIG::INTERCEPT_TYPE_SPEC_CUSTOM)
		{
			if (PIN_SafeCopy(to, from, size) != size)
			{
				memcpy(to, 0, size);
				return;
			}
			CaptureAllPointers(WINDOW_CONFIG::GetCustomType(elemenType.typeIndex), from, to);
		}
	}
	else
	{
		if (result)
		{
			memcpy(to, result, size);
			delete[] result;
		}
	}
}

LOCALFUN VOID* CaptureArrayCount(const WINDOW_CONFIG::INTERCEPT_TYPE_FIELD& elemenType, size_t count, BOOL inc, BOOL isCountSigned, VOID **pointerToAddr)
{
	size_t elementSize = INTERCEPTOR_INFO::ComputeFieldSize(elemenType);
	size_t arraySize = 0;
	UINT8 *array;
	UINT8 *addr = *reinterpret_cast<UINT8**>(pointerToAddr);

	// Address and size in bytes of the array.
	size_t overheadSize = sizeof(VOID*) + sizeof(size_t);

	if (isCountSigned)
	{
		size_t actualCount = count;
		if (addr == NULL || elementSize == 0 || actualCount <= 0)
		{
			array = new UINT8[overheadSize];
			memcpy(array, &arraySize, sizeof(size_t));
			memcpy(array + sizeof(size_t), pointerToAddr, sizeof(VOID*));
			return array;
		}

		arraySize = actualCount * elementSize;
		array = new UINT8[arraySize + overheadSize];
		memcpy(array, &arraySize, sizeof(size_t));
		memcpy(array + sizeof(size_t), pointerToAddr, sizeof(VOID*));

		UINT8 *currentElement = addr;
		UINT8 *currentBuffer = (inc ? array : array + arraySize) + overheadSize;
		for (size_t i = 0; i < actualCount; ++i)
		{
			CaptureFieldRestricted(elemenType, currentElement, currentBuffer);
			if (inc)
			{
				currentElement += elementSize;
				currentBuffer += elementSize;
			}
			else
			{
				currentElement -= elementSize;
				currentBuffer -= elementSize;
			}
		}
	}
	else
	{
		if (addr == NULL || elementSize == 0 || count == 0)
		{
			array = new UINT8[overheadSize];
			memcpy(array, &arraySize, sizeof(size_t));
			memcpy(array + sizeof(size_t), pointerToAddr, sizeof(VOID*));
			return array;
		}

		arraySize = count * elementSize;
		array = new UINT8[arraySize + overheadSize];
		memcpy(array, &arraySize, sizeof(size_t));
		memcpy(array + sizeof(size_t), pointerToAddr, sizeof(VOID*));

		UINT8 *currentElement = addr;
		UINT8 *currentBuffer = (inc ? array : array + arraySize - elementSize) + overheadSize;
		for (size_t i = 0; i < count; ++i)
		{
			CaptureFieldRestricted(elemenType, currentElement, currentBuffer);
			if (inc)
			{
				currentElement += elementSize;
				currentBuffer += elementSize;
			}
			else
			{
				currentElement -= elementSize;
				currentBuffer -= elementSize;
			}
		}
	}

	return array;
}

LOCALFUN VOID* CaptureArraySentinel(const WINDOW_CONFIG::INTERCEPT_TYPE_FIELD& elemenType, UINT64 sentinel, BOOL inc, VOID **pointerToAddr)
{
	size_t elementSize = INTERCEPTOR_INFO::ComputeFieldSize(elemenType);
	size_t arraySize = 0;
	UINT8 *array;
	UINT8 *addr = *reinterpret_cast<UINT8**>(pointerToAddr);

	// Address and size in bytes of the array.
	size_t overheadSize = sizeof(VOID*) + sizeof(size_t);

	if (addr == NULL || elementSize == 0 || elementSize > sizeof(sentinel))
	{
		array = new UINT8[overheadSize];
		memcpy(array, &arraySize, sizeof(size_t));
		memcpy(array + sizeof(size_t), pointerToAddr, sizeof(VOID*));
		return array;
	}

	std::vector<UINT64> data;

	UINT8 *currentElement = addr;

	while (TRUE)
	{
		UINT64 currentElementValue = 0;
		if (PIN_SafeCopy(&currentElementValue, currentElement, elementSize) != elementSize)
		{
			return NULL;
		}

		if (currentElementValue == sentinel)
		{
			break;
		}

		data.push_back(currentElementValue);

		if (inc)
			currentElement += elementSize;
		else
			currentElement -= elementSize;
	}

	data.push_back(sentinel);

	arraySize = data.size() * elementSize;
	array = new UINT8[arraySize + overheadSize];
	memcpy(array, &arraySize, sizeof(size_t));
	memcpy(array + sizeof(size_t), pointerToAddr, sizeof(VOID*));
	currentElement = (inc ? array : array + arraySize - elementSize) + overheadSize;

	for (size_t i = 0; i < data.size(); ++i)
	{
		memcpy(currentElement, &data[i], elementSize);
		if (inc)
			currentElement += elementSize;
		else
			currentElement -= elementSize;
	}

	return array;
}

LOCALFUN VOID* CaptureArrayRange(const WINDOW_CONFIG::INTERCEPT_TYPE_FIELD& elemenType, VOID **pointerToStartAddr, VOID **pointerToendAddr, BOOL isEndValid)
{
	size_t elementSize = INTERCEPTOR_INFO::ComputeFieldSize(elemenType);
	size_t arraySize = 0;
	UINT8 *array;
	UINT8 *startAddr = *reinterpret_cast<UINT8**>(pointerToStartAddr);
	UINT8 *endAddr = *reinterpret_cast<UINT8**>(pointerToendAddr);

	// Start address, end address and size in bytes of the array.
	size_t overheadSize = sizeof(VOID*) + sizeof(VOID*) + sizeof(size_t);

	if (startAddr == NULL || endAddr == NULL || elementSize == 0 || startAddr > endAddr || (startAddr == endAddr && !isEndValid))
	{
		array = new UINT8[overheadSize];
		memcpy(array, &arraySize, sizeof(size_t));
		memcpy(array + sizeof(size_t), pointerToStartAddr, sizeof(VOID*));
		memcpy(array + sizeof(size_t) + sizeof(VOID*), pointerToendAddr, sizeof(VOID*));
		return array;
	}

	if (startAddr < endAddr)
	{
		arraySize = endAddr - startAddr;
	}

	if (startAddr <= endAddr && isEndValid)
	{
		arraySize += elementSize;
	}

	array = new UINT8[overheadSize + arraySize];
	memset(array, 0, arraySize);
	memcpy(array, &arraySize, sizeof(size_t));
	memcpy(array + sizeof(size_t), pointerToStartAddr, sizeof(VOID*));
	memcpy(array + sizeof(size_t) + sizeof(VOID*), pointerToendAddr, sizeof(VOID*));

	UINT8 *currentElement = reinterpret_cast<UINT8*>(startAddr);
	UINT8 *currentBuffer = array + overheadSize;

	while (currentElement < endAddr)
	{
		CaptureFieldRestricted(elemenType, currentElement, currentBuffer);
		currentElement += elementSize;
		currentBuffer += elementSize;
	}

	if (isEndValid)
	{
		CaptureFieldRestricted(elemenType, currentElement, currentBuffer);
	}

	return array;
}

LOCALFUN size_t GetValueFromBytes(const VOID *bytes, UINT32 count)
{
	size_t result = 0;
	if (count <= sizeof(result))
	{
		memcpy(&result, bytes, count);
		return result;
	}
	return 0;
}

LOCALFUN VOID* CapturePointer(const VOID *typeDesc, UINT32 fieldOrParamIndex, BOOL isIntercept, std::vector<VOID*>& args, BOOL& wasPointer)
{
	const WINDOW_CONFIG::INTERCEPT_CONFIG *interceptConfig;
	const WINDOW_CONFIG::INTERCEPT_CUSTOM_TYPE *customType;
	const WINDOW_CONFIG::INTERCEPT_TYPE_FIELD *interceptTypeField;
	if (isIntercept)
	{
		interceptConfig = reinterpret_cast<const WINDOW_CONFIG::INTERCEPT_CONFIG*>(typeDesc);
		if (interceptConfig->parameters[fieldOrParamIndex].parameter.typeSpec != WINDOW_CONFIG::INTERCEPT_TYPE_SPEC_POINTER)
		{
			wasPointer = FALSE;
			return NULL;
		}
		interceptTypeField = &interceptConfig->parameters[fieldOrParamIndex].parameter;
	}
	else
	{
		customType = reinterpret_cast<const WINDOW_CONFIG::INTERCEPT_CUSTOM_TYPE*>(typeDesc);
		if (customType->fields[fieldOrParamIndex].typeSpec != WINDOW_CONFIG::INTERCEPT_TYPE_SPEC_POINTER)
		{
			wasPointer = FALSE;
			return NULL;
		}
		interceptTypeField = &customType->fields[fieldOrParamIndex];
	}

	wasPointer = TRUE;
	const WINDOW_CONFIG::INTERCEPT_TYPE_POINTER& pointerType =
		WINDOW_CONFIG::GetPointerType(interceptTypeField->typeIndex);
	VOID *argument = args[fieldOrParamIndex];

	if (isIntercept)
	{
		if (pointerType.type != WINDOW_CONFIG::INTERCEPT_TYPE_POINTER_TYPE_FIXED &&
			pointerType.type != WINDOW_CONFIG::INTERCEPT_TYPE_POINTER_TYPE_SENTINEL)
		{
			// The pointer depends on another parameter.

			WINDOW_CONFIG::INTERCEPT_PARAMETER_USAGE myUsage = interceptConfig->parameters[fieldOrParamIndex].usage;
			WINDOW_CONFIG::INTERCEPT_PARAMETER_USAGE otherUsage = interceptConfig->parameters[interceptTypeField->fieldOrParamIndex].usage;

			if (myUsage == WINDOW_CONFIG::INTERCEPT_PARAMETER_USAGE_INPUT &&
				otherUsage == WINDOW_CONFIG::INTERCEPT_PARAMETER_USAGE_OUTPUT)
			{
				// The parameter that this pointer depends on is not available yet. Capture only the value of the pointer.
				// Use a structure that works with all types of pointers to make it easier later to interpret it.
				UINT8 *val = new UINT8[sizeof(VOID*) + sizeof(VOID*) + sizeof(UINT64)];
				memset(val, 0, sizeof(UINT64));

				if (pointerType.type == WINDOW_CONFIG::INTERCEPT_TYPE_POINTER_TYPE_COUNT ||
					pointerType.type == WINDOW_CONFIG::INTERCEPT_TYPE_POINTER_TYPE_END_POINTER)
				{
					memcpy(val + sizeof(UINT64), reinterpret_cast<UINT8*>(argument), sizeof(VOID*));
					memset(val + sizeof(UINT64) + sizeof(VOID*), 0, sizeof(VOID*));
				}
				else
				{
					memset(val + sizeof(UINT64), 0, sizeof(VOID*));
					memcpy(val + sizeof(UINT64) + sizeof(VOID*), reinterpret_cast<UINT8*>(argument), sizeof(VOID*));
				}

				return val;
			}
		}
	}

	switch (pointerType.type)
	{
	case WINDOW_CONFIG::INTERCEPT_TYPE_POINTER_TYPE_FIXED:
		return CaptureArrayCount(
			pointerType.elementType,
			pointerType.fixed.count,
			pointerType.fixed.direction == WINDOW_CONFIG::INTERCEPT_TYPE_POINTER_DIRECTION_INCREMENT ? TRUE : FALSE,
			FALSE,
			reinterpret_cast<VOID**>(argument));
	case WINDOW_CONFIG::INTERCEPT_TYPE_POINTER_TYPE_SENTINEL:
		return CaptureArraySentinel(
			pointerType.elementType,
			pointerType.sentinel.sentinelValue,
			pointerType.sentinel.direction == WINDOW_CONFIG::INTERCEPT_TYPE_POINTER_DIRECTION_INCREMENT ? TRUE : FALSE,
			reinterpret_cast<VOID**>(argument));
	case WINDOW_CONFIG::INTERCEPT_TYPE_POINTER_TYPE_COUNT:
	{
		UINT32 countArgIndex = interceptTypeField->fieldOrParamIndex;
		const WINDOW_CONFIG::INTERCEPT_TYPE_FIELD& countType =
			isIntercept ? interceptConfig->parameters[countArgIndex].parameter : customType->fields[countArgIndex];
		UINT32 countSize;
		BOOL wasPrimitive, isCountSigned;
		VOID *countArgument = args[countArgIndex];
		VOID *count = CapturePrimitive(countType, countArgument, &countSize, wasPrimitive);
		isCountSigned = IsSignedIntegralType(countType.typeSpec);
		if (!wasPrimitive)
		{
			// The error checking code in the configuration manager makes sure that the count parameter either has an integral type or points to an object
			// of an integral type.
			const WINDOW_CONFIG::INTERCEPT_TYPE_POINTER *countPointerType = &WINDOW_CONFIG::GetPointerType(countType.typeIndex);
			count = CapturePrimitive(countPointerType->elementType, countArgument, &countSize, wasPrimitive);
			isCountSigned = IsSignedIntegralType(countPointerType->elementType.typeSpec);
		}

		if (count)
		{
			VOID *result = CaptureArrayCount(
				pointerType.elementType,
				GetValueFromBytes(count, countSize),
				pointerType.count.direction == WINDOW_CONFIG::INTERCEPT_TYPE_POINTER_DIRECTION_INCREMENT ? TRUE : FALSE,
				isCountSigned,
				reinterpret_cast<VOID**>(argument));
			delete[] count;
			return result;
		}
		else
		{
			ASSERT(FALSE, "Unexpected innterceptor pointer type.");
			return NULL;
		}
	}
	case WINDOW_CONFIG::INTERCEPT_TYPE_POINTER_TYPE_START_POINTER:
	{
		return NULL; // Capture once, when the paramter points to the first element of the array.
	}
	case WINDOW_CONFIG::INTERCEPT_TYPE_POINTER_TYPE_END_POINTER:
	{
		UINT32 endArgIndex = interceptTypeField->fieldOrParamIndex;
		VOID *endArgument = args[endArgIndex];
		return CaptureArrayRange(pointerType.elementType, 
			reinterpret_cast<VOID**>(argument),
			reinterpret_cast<VOID**>(endArgument),
			pointerType.startPointerEnd.endValid);
	}
	default:
		ASSERT(FALSE, "Unexpected innterceptor pointer type.");
		return NULL;
	}
}

LOCALFUN VOID CaptureParameters(INTERCEPTOR_INFO *info, BOOL in)
{
	std::vector<VOID*>& values = in ? info->argsIn : info->argsOut;
	WINDOW_CONFIG::INTERCEPT_PARAMETER_USAGE usageFlag = in ? WINDOW_CONFIG::INTERCEPT_PARAMETER_USAGE_INPUT : WINDOW_CONFIG::INTERCEPT_PARAMETER_USAGE_OUTPUT;
	std::vector<VOID*> argsAddresses;

	for (UINT32 argIndex = 0; argIndex < info->argAddresses.size(); ++argIndex)
	{
		argsAddresses.push_back(info->argAddresses[argIndex].first);
	}

	for (UINT32 argIndex = 0; argIndex < info->argAddresses.size(); ++argIndex)
	{
		WINDOW_CONFIG::INTERCEPT_PARAMETER& argDesc = info->config->parameters[info->argAddresses[argIndex].second];
		if ((argDesc.usage & usageFlag) == usageFlag)
		{
			values.push_back(CaptureGeneric(info->config, argIndex, TRUE, argsAddresses));
		}
	}
}

LOCALFUN VOID CaptureReturnValue(INTERCEPTOR_INFO *info, VOID *retAddr)
{
	std::vector<VOID*> argsAddresses;

	for (UINT32 argIndex = 0; argIndex < info->argAddresses.size(); ++argIndex)
	{
		argsAddresses.push_back(info->argAddresses[argIndex].first);
	}

	argsAddresses.push_back(retAddr);
	WINDOW_CONFIG::INTERCEPT_PARAMETER retParam;
	retParam.parameter = info->config->returnType;
	retParam.usage = WINDOW_CONFIG::INTERCEPT_PARAMETER_USAGE_OUTPUT;
	info->config->parameters.push_back(retParam);
	info->ret = CaptureGeneric(info->config, (UINT32)info->config->parameters.size() - 1, TRUE, argsAddresses);
	info->config->parameters.pop_back();
}

VOID PIN_FAST_ANALYSIS_CALL AnalyzeInterceptPre(InterceptorMemoryManager *memMan, WINDOW_CONFIG::INTERCEPT_CONFIG *config, UINT8 *arg, UINT32 argIndex)
{
	if (!WINDOW_CONFIG::IsWindowOpen())
		return;

	INTERCEPTOR_INFO **interceptors = reinterpret_cast<INTERCEPTOR_INFO**>(memMan->GetCurrentChunk());
	INTERCEPTOR_INFO *info;

	if (argIndex == 0)
	{
		info = new INTERCEPTOR_INFO;
		info->config = config;
		*interceptors = info;
	}
	else
	{
		info = *interceptors;
	}

	UINT8 *argCopy = new UINT8[PORTABLE_MAX_PROCESSOR_REGISTER_SIZE];
	PIN_SafeCopy(argCopy, arg, PORTABLE_MAX_PROCESSOR_REGISTER_SIZE);
	info->argAddresses.push_back(std::make_pair(argCopy, argIndex));
}

VOID PIN_FAST_ANALYSIS_CALL AnalyzeInterceptPre2(InterceptorMemoryManager *memMan, ADDRINT returnAddr, INT32 rtnId)
{
	if (!WINDOW_CONFIG::IsWindowOpen())
		return;

	INTERCEPTOR_INFO **interceptors = reinterpret_cast<INTERCEPTOR_INFO**>(memMan->GetCurrentChunk());
	INTERCEPTOR_INFO *info = *interceptors;
	info->rtnId = rtnId;
	info->returnAddr = returnAddr;
	info->ret = NULL;
	CaptureParameters(info, TRUE);

	memMan->Push(info);
	memMan->GetCurrentChunkAndIncrementBy(sizeof(info));

	GetCurrentTimestamp(info->callTimestamp);
}

VOID PIN_FAST_ANALYSIS_CALL AnalyzeInterceptPost(InterceptorMemoryManager *memMan, INT32 rtnId, UINT8 *retVal)
{
	VOID *addr = memMan->FindRoutineAndPop(rtnId);
	if (addr == NULL)
		return;

	INTERCEPTOR_INFO *info = reinterpret_cast<INTERCEPTOR_INFO*>(addr);

	GetCurrentTimestamp(info->returnTimestamp);
	CaptureParameters(info, FALSE);

	if (info->config->returnType.typeSpec != WINDOW_CONFIG::INTERCEPT_TYPE_SPEC_INVALID)
	{
		CaptureReturnValue(info, retVal);
	}
	else
	{
		info->ret = NULL;
	}

	for (UINT32 argIndex = 0; argIndex < info->argAddresses.size(); ++argIndex)
	{
		delete[] info->argAddresses[argIndex].first;
	}
	info->argAddresses.clear(); // This step is not necessary for correctness.
}