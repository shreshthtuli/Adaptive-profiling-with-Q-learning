#include "InterceptorInfo.h"
#include "ImageCollection.h"
#include "Utils.h"


INTERCEPTOR_INFO::~INTERCEPTOR_INFO()
{

}


VOID INTERCEPTOR_INFO::GenericToString(std::wostream& os, const WINDOW_CONFIG::INTERCEPT_TYPE_FIELD& elemenType, const UINT8 *addr, UINT32 level)
{
	UINT32 size = 0;
	BOOL isSigned;
	if (GetPrimitiveTypeSizeAndSign(elemenType.typeSpec, size, isSigned))
	{
		os << std::wstring(level * levelIndentation, ' ') << StringToWString(ShowN(size, addr)) << "\n";
	}
	else if (elemenType.typeSpec == WINDOW_CONFIG::INTERCEPT_TYPE_SPEC_CUSTOM)
	{
		CustomToString(os, WINDOW_CONFIG::GetCustomType(elemenType.typeIndex), addr, level);
	}
	else if (elemenType.typeSpec == WINDOW_CONFIG::INTERCEPT_TYPE_SPEC_POINTER)
	{
		const WINDOW_CONFIG::INTERCEPT_TYPE_POINTER& pointerType =
			WINDOW_CONFIG::GetPointerType(elemenType.typeIndex);

		const UINT8 *pointerValue = *reinterpret_cast<UINT8* const*>(addr);

		if (pointerValue == NULL)
		{
			os << std::wstring(level * levelIndentation, ' ') << "NULL (failed to record)\n";
		}
		else
		{
			const size_t *arraySize = reinterpret_cast<const size_t*>(pointerValue);
			const VOID* const* arrayActualAddr = reinterpret_cast<VOID* const*>(pointerValue + sizeof(size_t));
			const UINT8 *arrayPointer = pointerValue + sizeof(size_t) + sizeof(VOID*);

			size_t numOfElements = *arraySize / ComputeFieldSize(pointerType.elementType);
			os << std::wstring(level * levelIndentation, ' ') << "Number of elements = " << numOfElements << "\n";

			if (pointerType.type == WINDOW_CONFIG::INTERCEPT_TYPE_POINTER_TYPE_FIXED ||
				pointerType.type == WINDOW_CONFIG::INTERCEPT_TYPE_POINTER_TYPE_SENTINEL)
			{
				os << std::wstring(level * INTERCEPTOR_INFO::levelIndentation, ' ') << "Virtual address = " << *arrayActualAddr << "\n";
				ArrayToString(
					os,
					pointerType.elementType,
					numOfElements,
					arrayPointer,
					level + 1);
				delete[] pointerValue;
			}
			else
			{
				ASSERT(FALSE, "Unexpected innterceptor pointer type in generic.");
			}
		}
	}
	else
	{
		ASSERT(FALSE, "Unexpected type spec in generic.");
	}
}

UINT32 INTERCEPTOR_INFO::ComputeFieldSize(const WINDOW_CONFIG::INTERCEPT_TYPE_FIELD& fieldType)
{
	BOOL isSigned;
	UINT32 size;
	if (GetPrimitiveTypeSizeAndSign(fieldType.typeSpec, size, isSigned))
		return size;
	else if (fieldType.typeSpec == WINDOW_CONFIG::INTERCEPT_TYPE_SPEC_POINTER)
		return sizeof(VOID*);
	else if (fieldType.typeSpec == WINDOW_CONFIG::INTERCEPT_TYPE_SPEC_CUSTOM)
	{
		const WINDOW_CONFIG::INTERCEPT_CUSTOM_TYPE& customType = WINDOW_CONFIG::GetCustomType(fieldType.typeIndex);
		for (size_t i = 0; i < customType.fields.size(); ++i)
		{
			size += ComputeFieldSize(customType.fields[i]);
		}
		return size;
	}
	return 0;
}

VOID INTERCEPTOR_INFO::ArrayToString(std::wostream& os, const WINDOW_CONFIG::INTERCEPT_TYPE_FIELD& elemenType,
	size_t count, const VOID *addr, UINT32 level)
{
	if (count == 0)
		return;

	if (elemenType.typeSpec == WINDOW_CONFIG::INTERCEPT_TYPE_SPEC_ASCII)
	{
		const CHAR *asciiString = reinterpret_cast<const CHAR*>(addr); 
		if (asciiString[count - 1] == 0)
		{
			// Already NUL-terminated.
			os << std::wstring(level * levelIndentation, ' ') << asciiString;
		}
		else
		{
			// Add terminating NUL.
			CHAR *sz = new CHAR[count + 1];
			memcpy(sz, asciiString, sizeof(CHAR) * count);
			sz[count] = '\0';
			os << std::wstring(level * levelIndentation, ' ') << sz;
			delete[] sz;
		}
		os << "\n";
	}
	else if (elemenType.typeSpec == WINDOW_CONFIG::INTERCEPT_TYPE_SPEC_UNICODE)
	{
		const wchar_t *unicodeString = reinterpret_cast<const wchar_t*>(addr);
		if (unicodeString[count - 1] == 0)
		{
			// Already NUL-terminated.
			os << std::wstring(level * levelIndentation, ' ') << unicodeString;
		}
		else
		{
			// Add terminating NUL.
			wchar_t *sz = new wchar_t[count + 1];
			memcpy(sz, unicodeString, sizeof(wchar_t) * count);
			sz[count] = L'\0';
			os << std::wstring(level * levelIndentation, ' ') << sz;
			delete[] sz;
		}
		os << "\n";
	}
	else
	{
		UINT32 elementSize = ComputeFieldSize(elemenType);
		const UINT8 *currentElement = reinterpret_cast<const UINT8*>(addr);
		for (UINT64 i = 0; i < count; ++i)
		{
			GenericToString(os, elemenType, currentElement, level);
			currentElement += elementSize;
		}
	}
}

UINT32 INTERCEPTOR_INFO::CustomToString(std::wostream& os, const WINDOW_CONFIG::INTERCEPT_CUSTOM_TYPE& customType, const VOID *object, UINT32 level)
{
	UINT32 size = 0;
	BOOL isSigned;
	const UINT8 *currentField = reinterpret_cast<const UINT8*>(object);

	//std::vector<const VOID*> fieldAddrs;
	//std::vector<UINT32> fieldSizes;
	//fieldSizes.push_back(0);
	//for (size_t i = 0; i < customType.fields.size(); ++i)
	//{
	//	fieldAddrs.push_back(currentField);
	//	fieldSizes.push_back(fieldSizes.back() + ComputeFieldSize(customType.fields[i]));
	//	currentField += fieldSizes.back();
	//}

	//currentField = reinterpret_cast<const UINT8*>(object);
	for (size_t i = 0; i < customType.fields.size(); ++i)
	{
		os << std::wstring(level * levelIndentation, ' ') << StringToWString(customType.fields[i].name) << " = ";
		if (GetPrimitiveTypeSizeAndSign(customType.fields[i].typeSpec, size, isSigned))
		{
			os << StringToWString(ShowN(size, currentField)) << "\n";
		}
		else if (customType.fields[i].typeSpec == WINDOW_CONFIG::INTERCEPT_TYPE_SPEC_POINTER)
		{
			const WINDOW_CONFIG::INTERCEPT_TYPE_POINTER& pointerType =
				WINDOW_CONFIG::GetPointerType(customType.fields[i].typeIndex);
			const UINT8 *pointerValue = currentField;

			if (pointerType.type == WINDOW_CONFIG::INTERCEPT_TYPE_POINTER_TYPE_START_POINTER)
			{
				os << "Stores the end address of an array whose start address is stored in the " <<
					StringToWString(customType.fields[customType.fields[i].fieldOrParamIndex].name) <<
					" field. Refer to that field for more information.\n";
			}
			else if (pointerValue == NULL)
			{
				os << "NULL (failed to record)\n";
			}
			else
			{
				const size_t *arraySize = reinterpret_cast<const size_t*>(pointerValue);
				const VOID* const* arrayStartAddr = reinterpret_cast<VOID* const*>(pointerValue + sizeof(size_t));
				const VOID* const* arrayEndAddr = reinterpret_cast<VOID* const*>(pointerValue + sizeof(size_t) + sizeof(VOID*));

				const UINT8 *arrayPointer = pointerValue + sizeof(size_t) + sizeof(VOID*);

				size_t numOfElements = *arraySize / ComputeFieldSize(pointerType.elementType);
				os << "\n" << std::wstring((level + 1) * INTERCEPTOR_INFO::levelIndentation, ' ') << "Number of elements = " << numOfElements << "\n";

				if (pointerType.type == WINDOW_CONFIG::INTERCEPT_TYPE_POINTER_TYPE_FIXED ||
					pointerType.type == WINDOW_CONFIG::INTERCEPT_TYPE_POINTER_TYPE_SENTINEL ||
					pointerType.type == WINDOW_CONFIG::INTERCEPT_TYPE_POINTER_TYPE_COUNT)
				{
					os << std::wstring((level + 1) * INTERCEPTOR_INFO::levelIndentation, ' ') << "Virtual address = " << *arrayStartAddr << "\n";
				}
				else
				{
					arrayPointer = arrayPointer + sizeof(VOID*);
					os << std::wstring((level + 1) * INTERCEPTOR_INFO::levelIndentation, ' ') << "Start virtual address = " << *arrayStartAddr << "\n";
					os << std::wstring((level + 1) * INTERCEPTOR_INFO::levelIndentation, ' ') << "End virtual address = " << *arrayEndAddr << "\n";
				}

				os << std::wstring((level + 1) * INTERCEPTOR_INFO::levelIndentation, ' ') << "Elements: \n";

				switch (pointerType.type)
				{
				case WINDOW_CONFIG::INTERCEPT_TYPE_POINTER_TYPE_FIXED:
				case WINDOW_CONFIG::INTERCEPT_TYPE_POINTER_TYPE_SENTINEL:
				case WINDOW_CONFIG::INTERCEPT_TYPE_POINTER_TYPE_COUNT:
				case WINDOW_CONFIG::INTERCEPT_TYPE_POINTER_TYPE_END_POINTER:
					ArrayToString(
						os,
						pointerType.elementType,
						numOfElements,
						arrayPointer,
						level + 2);
					break;
				default:
					ASSERT(FALSE, "Unexpected innterceptor pointer type.");
					break;
				}

				delete[] pointerValue;
				size = sizeof(VOID*);
			}
		}
		else if (customType.fields[i].typeSpec == WINDOW_CONFIG::INTERCEPT_TYPE_SPEC_CUSTOM)
		{
			size = CustomToString(os, WINDOW_CONFIG::GetCustomType(customType.fields[i].typeIndex), currentField, level + 1);
		}
		currentField += size;
	}

	if (size > 0)
		delete[] object;

	return size;
}

BOOL INTERCEPTOR_INFO::GetPrimitiveTypeSizeAndSign(WINDOW_CONFIG::INTERCEPT_TYPE_SPEC typeSpec, UINT32& size, BOOL& isSigned)
{
	switch (typeSpec)
	{
	case WINDOW_CONFIG::INTERCEPT_TYPE_SPEC_SINT1:
		size = 1;
		isSigned = TRUE;
		break;
	case WINDOW_CONFIG::INTERCEPT_TYPE_SPEC_UINT1:
	case WINDOW_CONFIG::INTERCEPT_TYPE_SPEC_ASCII:
		size = 1;
		isSigned = FALSE;
		break;
	case WINDOW_CONFIG::INTERCEPT_TYPE_SPEC_SINT2:
		size = 2;
		isSigned = TRUE;
		break;
	case WINDOW_CONFIG::INTERCEPT_TYPE_SPEC_UINT2:
	case WINDOW_CONFIG::INTERCEPT_TYPE_SPEC_UNICODE:
		size = 2;
		isSigned = FALSE;
		break;
	case WINDOW_CONFIG::INTERCEPT_TYPE_SPEC_SINT3:
		size = 3;
		isSigned = TRUE;
		break;
	case WINDOW_CONFIG::INTERCEPT_TYPE_SPEC_UINT3:
		size = 3;
		isSigned = FALSE;
		break;
	case WINDOW_CONFIG::INTERCEPT_TYPE_SPEC_SINT4:
		size = 4;
		isSigned = TRUE;
		break;
	case WINDOW_CONFIG::INTERCEPT_TYPE_SPEC_UINT4:
		size = 4;
		isSigned = FALSE;
		break;
	case WINDOW_CONFIG::INTERCEPT_TYPE_SPEC_SINT5:
		size = 5;
		isSigned = TRUE;
		break;
	case WINDOW_CONFIG::INTERCEPT_TYPE_SPEC_UINT5:
		size = 5;
		isSigned = FALSE;
		break;
	case WINDOW_CONFIG::INTERCEPT_TYPE_SPEC_SINT6:
		size = 6;
		isSigned = TRUE;
		break;
	case WINDOW_CONFIG::INTERCEPT_TYPE_SPEC_UINT6:
		size = 6;
		isSigned = FALSE;
		break;
	case WINDOW_CONFIG::INTERCEPT_TYPE_SPEC_SINT7:
		size = 7;
		isSigned = TRUE;
		break;
	case WINDOW_CONFIG::INTERCEPT_TYPE_SPEC_UINT7:
		size = 7;
		isSigned = FALSE;
		break;
	case WINDOW_CONFIG::INTERCEPT_TYPE_SPEC_SINT8:
		size = 8;
		isSigned = TRUE;
		break;
	case WINDOW_CONFIG::INTERCEPT_TYPE_SPEC_UINT8:
		size = 8;
		isSigned = FALSE;
		break;
	default:
		return FALSE;// Not primitive.
	}
	return TRUE;
}

VOID INTERCEPTOR_INFO::WriteStringArgs(std::wostream& os, BOOL in)
{
	std::vector<VOID*>& values = in ? argsIn : argsOut;
	WINDOW_CONFIG::INTERCEPT_PARAMETER_USAGE usageFlag = in ? WINDOW_CONFIG::INTERCEPT_PARAMETER_USAGE_INPUT : WINDOW_CONFIG::INTERCEPT_PARAMETER_USAGE_OUTPUT;

	if (values.size() > 0)
	{
		UINT32 size = 0;
		BOOL isSigned;
		size_t valueIndex = 0;
		for (size_t paramIndex = 0; paramIndex < config->parameters.size(); ++paramIndex)
		{
			WINDOW_CONFIG::INTERCEPT_PARAMETER& argDesc = config->parameters[paramIndex];
			if ((argDesc.usage & usageFlag) == usageFlag)
			{
				os << std::wstring(levelIndentation, ' ') << argDesc.index << "/" << StringToWString(argDesc.parameter.name) << " = ";
				if (values[valueIndex] == NULL)
				{
					os << "NULL (failed to record)\n";
				}
				else
				{
					if (GetPrimitiveTypeSizeAndSign(argDesc.parameter.typeSpec, size, isSigned))
					{
						os << StringToWString(ShowN(size, values[valueIndex])) << "\n";
						delete[]  values[valueIndex];
					}
					else if (argDesc.parameter.typeSpec == WINDOW_CONFIG::INTERCEPT_TYPE_SPEC_POINTER)
					{
						const WINDOW_CONFIG::INTERCEPT_TYPE_POINTER& pointerType =
							WINDOW_CONFIG::GetPointerType(argDesc.parameter.typeIndex);
						const UINT8 *pointerValue = reinterpret_cast<const UINT8*>(values[valueIndex]);

						if (pointerType.type == WINDOW_CONFIG::INTERCEPT_TYPE_POINTER_TYPE_START_POINTER)
						{
							os << "Stores the end address of an array whose start address is stored in the " <<
								StringToWString(config->parameters[argDesc.parameter.fieldOrParamIndex].parameter.name) <<
								" parameter. Refer to that parameter for more information.\n";
						}
						else
						{
							const size_t *arraySize = reinterpret_cast<const size_t*>(pointerValue);
							const VOID* const* arrayStartAddr = reinterpret_cast<VOID* const*>(pointerValue + sizeof(size_t));
							const VOID* const* arrayEndAddr = reinterpret_cast<VOID* const*>(pointerValue + sizeof(size_t) + sizeof(VOID*));

							const UINT8 *arrayPointer = pointerValue + sizeof(size_t) + sizeof(VOID*);

							size_t numOfElements = *arraySize / ComputeFieldSize(pointerType.elementType);
							os << "\n" << std::wstring(2 * INTERCEPTOR_INFO::levelIndentation, ' ') << "Number of elements = " << numOfElements << "\n";

							if (pointerType.type == WINDOW_CONFIG::INTERCEPT_TYPE_POINTER_TYPE_FIXED ||
								pointerType.type == WINDOW_CONFIG::INTERCEPT_TYPE_POINTER_TYPE_SENTINEL ||
								pointerType.type == WINDOW_CONFIG::INTERCEPT_TYPE_POINTER_TYPE_COUNT)
							{
								os << std::wstring(2 * INTERCEPTOR_INFO::levelIndentation, ' ') << "Virtual address = " << *arrayStartAddr << "\n";
							}
							else
							{
								arrayPointer = arrayPointer + sizeof(VOID*);
								os << std::wstring(2 * INTERCEPTOR_INFO::levelIndentation, ' ') << "Start virtual address = " << *arrayStartAddr << "\n";
								os << std::wstring(2 * INTERCEPTOR_INFO::levelIndentation, ' ') << "End virtual address = " << *arrayEndAddr << "\n";
							}

							os << std::wstring(2 * INTERCEPTOR_INFO::levelIndentation, ' ') << "Elements: \n";

							switch (pointerType.type)
							{
							case WINDOW_CONFIG::INTERCEPT_TYPE_POINTER_TYPE_FIXED:
							case WINDOW_CONFIG::INTERCEPT_TYPE_POINTER_TYPE_SENTINEL:
							case WINDOW_CONFIG::INTERCEPT_TYPE_POINTER_TYPE_COUNT:
							case WINDOW_CONFIG::INTERCEPT_TYPE_POINTER_TYPE_END_POINTER:
								ArrayToString(
									os,
									pointerType.elementType,
									numOfElements,
									arrayPointer,
									3);
								break;
							default:
								ASSERT(FALSE, "Unexpected innterceptor pointer type.");
								break;
							}

							delete[] pointerValue;
							size = sizeof(VOID*);
						}

					}
					else if (argDesc.parameter.typeSpec == WINDOW_CONFIG::INTERCEPT_TYPE_SPEC_CUSTOM)
					{
						size = CustomToString(os, WINDOW_CONFIG::GetCustomType(argDesc.parameter.typeIndex), values[valueIndex], 2);
					}
				}

				++valueIndex;
			}
		}
	}
}

VOID INTERCEPTOR_INFO::WriteStringArgsBin(std::ostream& os, BOOL in)
{
	std::vector<VOID*>& values = in ? argsIn : argsOut;
	WINDOW_CONFIG::INTERCEPT_PARAMETER_USAGE usageFlag = in ? WINDOW_CONFIG::INTERCEPT_PARAMETER_USAGE_INPUT : WINDOW_CONFIG::INTERCEPT_PARAMETER_USAGE_OUTPUT;

	if (values.size() > 0)
	{
		UINT32 size = 0;
		BOOL isSigned;
		size_t valueIndex = 0;
		for (size_t paramIndex = 0; paramIndex < config->parameters.size(); ++paramIndex)
		{
			WINDOW_CONFIG::INTERCEPT_PARAMETER& argDesc = config->parameters[paramIndex];
			if ((argDesc.usage & usageFlag) == usageFlag)
			{
				//os << std::wstring(levelIndentation, ' ') << argDesc.index << "/" << StringToWString(argDesc.parameter.name) << " = ";
				if (values[valueIndex] == NULL)
				{
					//os << "NULL (failed to record)\n";
				}
				else
				{
					if (GetPrimitiveTypeSizeAndSign(argDesc.parameter.typeSpec, size, isSigned))
					{
						//os << StringToWString(ShowN(size, values[valueIndex])) << "\n";
						delete[]  values[valueIndex];
					}
					else if (argDesc.parameter.typeSpec == WINDOW_CONFIG::INTERCEPT_TYPE_SPEC_POINTER)
					{
						const WINDOW_CONFIG::INTERCEPT_TYPE_POINTER& pointerType =
							WINDOW_CONFIG::GetPointerType(argDesc.parameter.typeIndex);
						const UINT8 *pointerValue = reinterpret_cast<const UINT8*>(values[valueIndex]);

						// If pointerType.type == WINDOW_CONFIG::INTERCEPT_TYPE_POINTER_TYPE_START_POINTER
						// Stores the end address of an array whose start address is stored in the
						// StringToWString(config->parameters[argDesc.parameter.fieldOrParamIndex].parameter.name)
						// parameter. Refer to that parameter for more information.

						if (pointerType.type != WINDOW_CONFIG::INTERCEPT_TYPE_POINTER_TYPE_START_POINTER)
						{
							const UINT64 *arraySize = reinterpret_cast<const UINT64*>(pointerValue);
							const VOID* const* arrayStartAddr = reinterpret_cast<VOID* const*>(pointerValue + sizeof(UINT64));
							const VOID* const* arrayEndAddr = reinterpret_cast<VOID* const*>(pointerValue + sizeof(UINT64) + sizeof(VOID*));

							const UINT8 *arrayPointer = pointerValue + sizeof(UINT64) + sizeof(VOID*);

							UINT64 numOfElements = *arraySize / ComputeFieldSize(pointerType.elementType);
							WriteBinary(os, numOfElements);

							if (pointerType.type == WINDOW_CONFIG::INTERCEPT_TYPE_POINTER_TYPE_FIXED ||
								pointerType.type == WINDOW_CONFIG::INTERCEPT_TYPE_POINTER_TYPE_SENTINEL ||
								pointerType.type == WINDOW_CONFIG::INTERCEPT_TYPE_POINTER_TYPE_COUNT)
							{
								WriteBinary(os, arrayStartAddr);
							}
							else
							{
								arrayPointer = arrayPointer + sizeof(VOID*);
								WriteBinary(os, arrayStartAddr);
								WriteBinary(os, arrayEndAddr);
							}

							switch (pointerType.type)
							{
							case WINDOW_CONFIG::INTERCEPT_TYPE_POINTER_TYPE_FIXED:
							case WINDOW_CONFIG::INTERCEPT_TYPE_POINTER_TYPE_SENTINEL:
							case WINDOW_CONFIG::INTERCEPT_TYPE_POINTER_TYPE_COUNT:
							case WINDOW_CONFIG::INTERCEPT_TYPE_POINTER_TYPE_END_POINTER:
								//ArrayToString(
								//	os,
								//	pointerType.elementType,
								//	numOfElements,
								//	arrayPointer,
								//	3);
								break;
							default:
								ASSERT(FALSE, "Unexpected innterceptor pointer type.");
								break;
							}

							delete[] pointerValue;
							size = sizeof(VOID*);
						}

					}
					else if (argDesc.parameter.typeSpec == WINDOW_CONFIG::INTERCEPT_TYPE_SPEC_CUSTOM)
					{
						//size = CustomToString(os, WINDOW_CONFIG::GetCustomType(argDesc.parameter.typeIndex), values[valueIndex], 2);
					}
				}

				++valueIndex;
			}
		}
	}
}

VOID INTERCEPTOR_INFO::WriteString(std::wostream& os)
{
	std::string imgName, funcName;
	IMAGE_COLLECTION::GetImageAndRoutineNames(rtnId, imgName, funcName);

	os << callTimestamp << " " << StringToWString(imgName) << ":" << StringToWString(funcName) << "\n";
	
	if (config->returnType.typeSpec != WINDOW_CONFIG::INTERCEPT_TYPE_SPEC_INVALID)
	{
		if (ret != NULL)
		{
			argsOut.push_back(ret);
			WINDOW_CONFIG::INTERCEPT_PARAMETER retParam;
			retParam.parameter = config->returnType;
			retParam.usage = WINDOW_CONFIG::INTERCEPT_PARAMETER_USAGE_OUTPUT;
			retParam.parameter.name = "Return value";
			retParam.index = (UINT32)-1;
			config->parameters.push_back(retParam);
		}
	}

	WriteStringArgs(os, TRUE);
	WriteStringArgs(os, FALSE);

	if (config->returnType.typeSpec != WINDOW_CONFIG::INTERCEPT_TYPE_SPEC_INVALID)
	{
		if (ret != NULL)
		{
			argsOut.pop_back();
			config->parameters.pop_back();
		}
		else
		{
			UINT32 retIndex = (UINT32)-1;
			os << std::wstring(levelIndentation, ' ') << retIndex << "/Return value = NULL (failed to record)\n";
		}
	}

	os << std::wstring(INTERCEPTOR_INFO::levelIndentation, ' ') << returnAddr << " " << returnTimestamp << "\n";
}

VOID INTERCEPTOR_INFO::WriteBin(std::ostream& os)
{
	// TODO: Implement interceptors profile writer.

	std::string imgName, funcName;
	IMAGE_COLLECTION::GetImageAndRoutineNames(rtnId, imgName, funcName);

	// os << callTimestamp << " " << StringToWString(imgName) << ":" << StringToWString(funcName) << "\n";

	if (config->returnType.typeSpec != WINDOW_CONFIG::INTERCEPT_TYPE_SPEC_INVALID)
	{
		if (ret != NULL)
		{
			argsOut.push_back(ret);
			WINDOW_CONFIG::INTERCEPT_PARAMETER retParam;
			retParam.parameter = config->returnType;
			retParam.usage = WINDOW_CONFIG::INTERCEPT_PARAMETER_USAGE_OUTPUT;
			retParam.parameter.name = "Return value";
			retParam.index = (UINT32)-1;
			config->parameters.push_back(retParam);
		}
	}

	// WriteStringArgs(os, TRUE);
	// WriteStringArgs(os, FALSE);

	if (config->returnType.typeSpec != WINDOW_CONFIG::INTERCEPT_TYPE_SPEC_INVALID)
	{
		if (ret != NULL)
		{
			argsOut.pop_back();
			config->parameters.pop_back();
		}
		else
		{
			UINT32 retIndex = (UINT32)-1;
			// os << std::wstring(levelIndentation, ' ') << retIndex << "/Return value = NULL (failed to record)\n";
		}
	}

	// os << std::wstring(INTERCEPTOR_INFO::levelIndentation, ' ') << returnAddr << " " << returnTimestamp << "\n";
}

VOID INTERCEPTOR_INFO::EmitBinaryProfileTypesSection(std::ostream& os)
{
	// Emit header.

	UINT32 typeCount = WINDOW_CONFIG::INTERCEPT_TYPE_PRIMITIVE_COUNT;
	WriteBinary(os, typeCount);
	typeCount = WINDOW_CONFIG::GetCustomTypeCount();
	WriteBinary(os, typeCount);
	typeCount = WINDOW_CONFIG::GetPointerTypeCount();
	WriteBinary(os, typeCount);

	char nul = 0;

	// Emit Custom Types.

	for (UINT32 typeIndex = 0; typeIndex < WINDOW_CONFIG::GetCustomTypeCount(); ++typeIndex)
	{
		const WINDOW_CONFIG::INTERCEPT_CUSTOM_TYPE& type = WINDOW_CONFIG::GetCustomType(typeIndex);
		os << type.name << nul << (UINT32)type.fields.size();

		for (UINT32 fieldIndex = 0; fieldIndex < type.fields.size(); ++fieldIndex)
		{
			os << type.fields[fieldIndex].name << nul;
			UINT32 globalTypeIndex = TypeIndexToGlobalTypeIndex(type.fields[fieldIndex].typeIndex, type.fields[fieldIndex].typeSpec);
			WriteBinary(os, globalTypeIndex);
		}
	}

	// Emit pointer types.

	for (UINT32 typeIndex = 0; typeIndex < WINDOW_CONFIG::GetPointerTypeCount(); ++typeIndex)
	{
		const WINDOW_CONFIG::INTERCEPT_TYPE_POINTER& type = WINDOW_CONFIG::GetPointerType(typeIndex);
		os << type.name << nul;
		UINT32 globalTypeIndex = TypeIndexToGlobalTypeIndex(type.elementType.typeIndex, type.elementType.typeSpec);
		WriteBinary(os, globalTypeIndex);
		WriteBinary(os, type.type);
		switch (type.type)
		{
		case WINDOW_CONFIG::INTERCEPT_TYPE_POINTER_TYPE::INTERCEPT_TYPE_POINTER_TYPE_FIXED:
			WriteBinary(os, type.fixed.direction);
			WriteBinary(os, type.fixed.count);
			break;
		case WINDOW_CONFIG::INTERCEPT_TYPE_POINTER_TYPE::INTERCEPT_TYPE_POINTER_TYPE_SENTINEL:
			WriteBinary(os, type.sentinel.direction);
			WriteBinary(os, type.sentinel.sentinelValue);
			break;
		case WINDOW_CONFIG::INTERCEPT_TYPE_POINTER_TYPE::INTERCEPT_TYPE_POINTER_TYPE_COUNT:
			WriteBinary(os, type.count.direction);
			os << type.count.name << nul;
			break;
		case WINDOW_CONFIG::INTERCEPT_TYPE_POINTER_TYPE::INTERCEPT_TYPE_POINTER_TYPE_START_POINTER:
			WriteBinary(os, type.endPointerStart.endValid);
			os << type.endPointerStart.startPointerName << nul;
			break;
		case WINDOW_CONFIG::INTERCEPT_TYPE_POINTER_TYPE::INTERCEPT_TYPE_POINTER_TYPE_END_POINTER:
			WriteBinary(os, type.startPointerEnd.endValid);
			os << type.startPointerEnd.endPointerName << nul;
			break;
		default:
			ASSERTQ("Invalid pointer type encountered while emitting types.");
		}
	}
}

UINT32 INTERCEPTOR_INFO::TypeIndexToGlobalTypeIndex(UINT32 typeIndex, WINDOW_CONFIG::INTERCEPT_TYPE_SPEC typeSpec)
{
	ASSERTX(typeSpec != WINDOW_CONFIG::INTERCEPT_TYPE_SPEC::INTERCEPT_TYPE_SPEC_INVALID);

	switch (typeSpec)
	{
	case WINDOW_CONFIG::INTERCEPT_TYPE_SPEC::INTERCEPT_TYPE_SPEC_CUSTOM:
		return typeIndex + WINDOW_CONFIG::INTERCEPT_TYPE_PRIMITIVE_COUNT;
	case WINDOW_CONFIG::INTERCEPT_TYPE_SPEC::INTERCEPT_TYPE_SPEC_POINTER:
		return typeIndex + WINDOW_CONFIG::INTERCEPT_TYPE_PRIMITIVE_COUNT + WINDOW_CONFIG::GetCustomTypeCount();
	default:
		return typeSpec - 1; // Primitive type.
	}
}