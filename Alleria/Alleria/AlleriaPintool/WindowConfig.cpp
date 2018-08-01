#include "WindowConfig.h"

#define WINDOW_CONFIG_TAG_ALL "All"
#define WINDOW_CONFIG_TAG_ONE "One"
#define WINDOW_CONFIG_TAG_CREATION "Creation"
#define WINDOW_CONFIG_TAG_TERMINATION "Termination"
#define WINDOW_CONFIG_TAG_LOAD "Load"
#define WINDOW_CONFIG_TAG_UNLOAD "Unload"
#define WINDOW_CONFIG_TAG_CALL "Call"
#define WINDOW_CONFIG_TAG_RETURN "Return"
#define WINDOW_CONFIG_TAG_INSTRUCTIONS "Instructions"
#define WINDOW_CONFIG_TAG_TIME "Time"
#define WINDOW_CONFIG_TAG_THREADS "Threads"
#define WINDOW_CONFIG_TAG_ACCESSES "Accesses"
#define WINDOW_CONFIG_TAG_THREAD "Thread"
#define WINDOW_CONFIG_TAG_IMAGE "Image"
#define WINDOW_CONFIG_TAG_FUNCTION "Function"
#define WINDOW_CONFIG_TAG_INSTRUCTION "Instruction"
#define WINDOW_CONFIG_TAG_ALLERIACONFIG "AlleriaConfig"
#define WINDOW_CONFIG_TAG_WINDOW "Window"
#define WINDOW_CONFIG_TAG_ON "On"
#define WINDOW_CONFIG_TAG_TILL "Till"
#define WINDOW_CONFIG_TAG_PROFILE "Profile"
#define WINDOW_CONFIG_TAG_PROCESS "Process"
#define WINDOW_CONFIG_TAG_INTERCEPT "Intercept"
#define WINDOW_CONFIG_TAG_TYPES "Types"
#define WINDOW_CONFIG_TAG_TYPE "Type"
#define WINDOW_CONFIG_TAG_POINTER "Pointer"
#define WINDOW_CONFIG_TAG_FIELD "Field"
#define WINDOW_CONFIG_TAG_PARAMETER "Parameter"

#define WINDOW_CONFIG_ATTRIBUTE_VERSION "version"
#define WINDOW_CONFIG_ATTRIBUTE_REOPENABLE "reopenable"
#define WINDOW_CONFIG_ATTRIBUTE_PROCESS "process"
#define WINDOW_CONFIG_ATTRIBUTE_THREAD "thread"
#define WINDOW_CONFIG_ATTRIBUTE_PATH "path"
#define WINDOW_CONFIG_ATTRIBUTE_NAME "name"
#define WINDOW_CONFIG_ATTRIBUTE_IMAGEPATH "imagePath"
#define WINDOW_CONFIG_ATTRIBUTE_IMAGENAME "imageName"
#define WINDOW_CONFIG_ATTRIBUTE_THRESHOLD "threshold"
#define WINDOW_CONFIG_ATTRIBUTE_DECORATED "decorated"
#define WINDOW_CONFIG_ATTRIBUTE_ANYTHREAD "anyThread"
#define WINDOW_CONFIG_ATTRIBUTE_READS "reads"
#define WINDOW_CONFIG_ATTRIBUTE_WRITES "writes"
#define WINDOW_CONFIG_ATTRIBUTE_TOTAL "total"
#define WINDOW_CONFIG_ATTRIBUTE_SMALLEREQUAL "smallerEqual"
#define WINDOW_CONFIG_ATTRIBUTE_LARGEREQUAL "largerEqual"
#define WINDOW_CONFIG_ATTRIBUTE_N "n"
#define WINDOW_CONFIG_ATTRIBUTE_MAIN "main"
#define WINDOW_CONFIG_ATTRIBUTE_ANYBUTSYS "anyButSys"
#define WINDOW_CONFIG_ATTRIBUTE_ANY "any"
#define WINDOW_CONFIG_ATTRIBUTE_ANYMEM "anyMem"
#define WINDOW_CONFIG_ATTRIBUTE_TIMESTAMP "timeStamp"
#define WINDOW_CONFIG_ATTRIBUTE_ROUTINEID "routineId"
#define WINDOW_CONFIG_ATTRIBUTE_ACCESSADDRESSANDISZE "accessAddressAndSize"
#define WINDOW_CONFIG_ATTRIBUTE_ACCESSVALUE "accessValue"
#define WINDOW_CONFIG_ATTRIBUTE_TYPE "type"
#define WINDOW_CONFIG_ATTRIBUTE_ELEMENTTYPE "elementType"
#define WINDOW_CONFIG_ATTRIBUTE_DIRECTION "direction"
#define WINDOW_CONFIG_ATTRIBUTE_SENTINEL "sentinel"
#define WINDOW_CONFIG_ATTRIBUTE_COUNTVARIABLE "countVariable"
#define WINDOW_CONFIG_ATTRIBUTE_COUNT "count"
#define WINDOW_CONFIG_ATTRIBUTE_STARTVARIABLE "startVariable"
#define WINDOW_CONFIG_ATTRIBUTE_ENDVARIABLE "endVariable"
#define WINDOW_CONFIG_ATTRIBUTE_ENDVALID "endValid"
#define WINDOW_CONFIG_ATTRIBUTE_RETTYPE "retType"
#define WINDOW_CONFIG_ATTRIBUTE_INDEX "index"
#define WINDOW_CONFIG_ATTRIBUTE_IN "in"
#define WINDOW_CONFIG_ATTRIBUTE_OUT "out"


#define WINDOW_CONFIG_TYPE_U1 "u1"
#define WINDOW_CONFIG_TYPE_U2 "u2"
#define WINDOW_CONFIG_TYPE_U3 "u3"
#define WINDOW_CONFIG_TYPE_U4 "u4"
#define WINDOW_CONFIG_TYPE_U5 "u5"
#define WINDOW_CONFIG_TYPE_U6 "u6"
#define WINDOW_CONFIG_TYPE_U7 "u7"
#define WINDOW_CONFIG_TYPE_U8 "u8"
#define WINDOW_CONFIG_TYPE_S1 "s1"
#define WINDOW_CONFIG_TYPE_S2 "s2"
#define WINDOW_CONFIG_TYPE_S3 "s3"
#define WINDOW_CONFIG_TYPE_S4 "s4"
#define WINDOW_CONFIG_TYPE_S5 "s5"
#define WINDOW_CONFIG_TYPE_S6 "s6"
#define WINDOW_CONFIG_TYPE_S7 "s7"
#define WINDOW_CONFIG_TYPE_S8 "s8"
#define WINDOW_CONFIG_TYPE_ASCII "ascii"
#define WINDOW_CONFIG_TYPE_UNICODE "unicode"

UINT64 WINDOW_CONFIG::EVENT_COUNT_THREAD::currentCount = 0;
UINT64 WINDOW_CONFIG::EVENT_COUNT_ACCESS::currentReadCountAllThreads = 0;
UINT64 WINDOW_CONFIG::EVENT_COUNT_ACCESS::currentWriteCountAllThreads = 0;
UINT64 WINDOW_CONFIG::EVENT_INSTRUCTION::currentCountAllThreads = 0;
const CHAR *WINDOW_CONFIG::DefaultConfigFileName = "alleria.config";
WINDOW_CONFIG::EVENT *WINDOW_CONFIG::s_on = NULL;
WINDOW_CONFIG::EVENT *WINDOW_CONFIG::s_till = NULL;
std::map<UINT32, WINDOW_CONFIG::PROCESS_CONFIG> WINDOW_CONFIG::s_processMap;
WINDOW_CONFIG::PROCESS_CONFIG WINDOW_CONFIG::s_any;
BOOL WINDOW_CONFIG::s_reopenable = FALSE;
std::map<THREADID, PIN_THREAD_UID> WINDOW_CONFIG::s_tids;
PIN_LOCK WINDOW_CONFIG::s_reportingLock;
std::map<std::string, std::pair<UINT64, UINT64> > WINDOW_CONFIG::s_interestingFuncs;
UINT32 WINDOW_CONFIG::s_processNumber = 0;
std::string WINDOW_CONFIG::s_mainExePath;
std::vector<WINDOW_CONFIG::INTERCEPT_CUSTOM_TYPE> WINDOW_CONFIG::s_customTypes;
std::vector<WINDOW_CONFIG::INTERCEPT_TYPE_POINTER> WINDOW_CONFIG::s_pointerTypes;
std::map<PIN_THREAD_UID, UINT64> WINDOW_CONFIG::s_tuidMap;
PIN_LOCK WINDOW_CONFIG::s_tuidMapLock;

extern std::string g_outputDir;
extern void PathCombine(std::string& output, const std::string& path1, const std::string& path2);

const char *WINDOW_CONFIG::INS_CONFIG_STRINGS[] =
{
	"form", /* xed_iform_enum_t */
	"class", /* xed_iclass_enum_t */
	"isa", /* xed_isa_set_enum_t */
	"ext", /* xed_extension_enum_t */
	"cat" /* xed_category_enum_t */
};

LOCALFUN VOID StringToUpper(std::string& str)
{
	for (std::string::iterator p = str.begin(); p != str.end(); ++p)
		*p = (char)toupper(*p);
}

LOCALFUN UINT32 OnlyOneAttributeExists(tinyxml2::XMLElement &el, const char *names[], size_t size, const char **value)
{
	const char *insType = NULL;
	UINT32 index = 0;
	for (UINT32 i = 0; i < size; ++i)
	{
		if (!insType)
		{
			insType = el.Attribute(names[i]);
			index = i;
			*value = insType;
		}
		else if (el.Attribute(names[i]))
		{
			*value = NULL;
			return i;
		}
	}
	return index;
}

LOCALFUN UINT8 HexCharToNibble(char hex)
{
	if (hex >= '0' && hex <= '9')
		return hex - '0';
	else if (hex >= 'a' && hex <= 'f')
		return (hex - 'a') + 10;
	else if (hex >= 'A' && hex <= 'F')
		return (hex - 'A') + 10;
	else
		return 0xFF;
}

BOOL WINDOW_CONFIG::IsProcessInteresting(UINT32 processNumber)
{
	// Don't call IsWindowOpen(). As long as Pin is attached to the process,
	// check for other processes. This makes it convenient for the user.
	return s_any.valid || (s_processMap.find(processNumber) != s_processMap.end());
}

UINT64 WINDOW_CONFIG::GetAppSerialId(PIN_THREAD_UID tuid)
{
	UINT64 id;
	PIN_GetLock(&s_tuidMapLock, PIN_ThreadId());
	ASSERTX(s_tuidMap.find(tuid) != s_tuidMap.end());
	id = s_tuidMap[tuid];
	PIN_ReleaseLock(&s_tuidMapLock);
	return id;
}

UINT8* WINDOW_CONFIG::HexStrToBytes(size_t cbytes, const char *str)
{
	if (!str || cbytes == 0)
		return NULL;

	UINT8 *bytes = new UINT8[cbytes];

	size_t index = cbytes - 1;
	while (index >= 0)
	{
		UINT8 lowNibble = HexCharToNibble(str[index * 2]);
		UINT8 highNibble = HexCharToNibble(str[index * 2 + 1]) << 4;

		if (highNibble == 0xFF || lowNibble == 0xFF)
		{
			delete[] bytes;
			return NULL;
		}

		bytes[index] = highNibble | lowNibble;
		--index;
	}

	return bytes;
}

BOOL WINDOW_CONFIG::Init(std::string configFilePath, UINT32 processNumber, std::string mainExePath)
{
	s_processNumber = processNumber;
	s_mainExePath = mainExePath;
	tinyxml2::XMLDocument xmlDoc;
	std::string fullPath;
	PathCombine(fullPath, g_outputDir, WINDOW_CONFIG::DefaultConfigFileName);
	
	if (xmlDoc.LoadFile(configFilePath.c_str()) == tinyxml2::XMLError::XML_NO_ERROR)
	{
		ALLERIA_WriteMessageEx(ALLERIA_REPORT_INIT, ("Config file loaded from " + configFilePath).c_str());
		InitFromConfig(xmlDoc);
	}
	else if (xmlDoc.LoadFile(fullPath.c_str()) == tinyxml2::XMLError::XML_NO_ERROR)
	{
		std::string errorString = "Config file loaded from " + fullPath;
		ALLERIA_WriteMessageEx(ALLERIA_REPORT_INIT, errorString.c_str());
		InitFromConfig(xmlDoc);
	}
	else
	{
		ALLERIA_WriteMessageEx(ALLERIA_REPORT_INIT, "Config file was not found. Defaulted to profile all memory access instructions.");
		InitDefault();
	}

	PIN_InitLock(&s_reportingLock);

	return AlleriaGetLastError() == ALLERIA_ERROR_SUCCESS;
}

VOID WINDOW_CONFIG::Destroy()
{
	/* Nothing to do here. */

	/* Why?

	All fields that are pointers in the config and event types
	point to heap-allocated objects. However, since all of them
	are required till the process terminates, we don't have to
	explicitly free them.

	*/
}

BOOL WINDOW_CONFIG::EventFromConfig(tinyxml2::XMLElement *xmlEl, EVENT **e)
{
	tinyxml2::XMLElement *root;
	if ((root = xmlEl->FirstChildElement(WINDOW_CONFIG_TAG_ALL)) ||
		(root = xmlEl->FirstChildElement(WINDOW_CONFIG_TAG_ONE)))
	{
		if (root->NoChildren())
		{
			AlleriaSetLastError(ALLERIA_ERROR_CONFIG_UNKOWN_EVENT);
			return FALSE;
		}

		std::vector<EVENT*> *events;

		if (root = xmlEl->FirstChildElement(WINDOW_CONFIG_TAG_ALL))
		{
			EVENT_ALL *all = new EVENT_ALL;
			events = &all->events;
			*e = all;
		}
		else
		{
			// Must be EVENT_ONE.
			EVENT_ONE *one = new EVENT_ONE;
			events = &one->events;
			*e = one;
		}

		EVENT *subEvent;
		tinyxml2::XMLElement *child = root->FirstChild()->ToElement();

		if (!child)
		{
			AlleriaSetLastError(ALLERIA_ERROR_CONFIG_UNKOWN_EVENT);
			return FALSE;
		}

		EventFromConfig(child, &subEvent);

		while (AlleriaGetLastError() == ALLERIA_ERROR_SUCCESS)
		{
			events->push_back(subEvent);
			if (child->NextSibling() && (child = child->NextSibling()->ToElement()))
				EventFromConfig(child, &subEvent);
			else
				break;
		}
	}
	else if ((root = xmlEl->FirstChildElement(WINDOW_CONFIG_TAG_CREATION)) ||
		(root = xmlEl->FirstChildElement(WINDOW_CONFIG_TAG_TERMINATION)))
	{
		unsigned int processId;
		if (root->QueryUnsignedAttribute(WINDOW_CONFIG_ATTRIBUTE_PROCESS, &processId) != tinyxml2::XML_NO_ERROR || processId == 0)
		{
			AlleriaSetLastError(ALLERIA_ERROR_CONFIG_UNKOWN_EVENT);
			return FALSE;
		}

		unsigned int threadId = root->UnsignedAttribute(WINDOW_CONFIG_ATTRIBUTE_THREAD);

		EVENT_THREAD *eThread = new EVENT_THREAD;
		eThread->creation = xmlEl->FirstChildElement(WINDOW_CONFIG_TAG_CREATION) != NULL;
		eThread->processId = processId;
		eThread->threadId = threadId;
		*e = eThread;
	}
	else if ((root = xmlEl->FirstChildElement(WINDOW_CONFIG_TAG_LOAD)) ||
		(root = xmlEl->FirstChildElement(WINDOW_CONFIG_TAG_UNLOAD)))
	{
		unsigned int processId = root->UnsignedAttribute(WINDOW_CONFIG_ATTRIBUTE_PROCESS);

		const char *path = root->Attribute(WINDOW_CONFIG_ATTRIBUTE_PATH);
		const char *name = root->Attribute(WINDOW_CONFIG_ATTRIBUTE_NAME);

		if ((!path && !name) || (path && name))
		{
			AlleriaSetLastError(ALLERIA_ERROR_CONFIG_UNKOWN_EVENT);
			return FALSE;
		}

		EVENT_IMAGE *eImage = new EVENT_IMAGE;
		eImage->isPath = name == NULL;
		eImage->load = xmlEl->FirstChildElement(WINDOW_CONFIG_TAG_LOAD) != NULL;
		eImage->pathOrName = name == NULL ? path : name;
		StringToUpper(eImage->pathOrName);
		eImage->processId = processId;
		*e = eImage;
	}
	else if ((root = xmlEl->FirstChildElement(WINDOW_CONFIG_TAG_CALL)) ||
		(root = xmlEl->FirstChildElement(WINDOW_CONFIG_TAG_RETURN)))
	{
		unsigned int processId = root->UnsignedAttribute(WINDOW_CONFIG_ATTRIBUTE_PROCESS);
		unsigned int threadId = root->UnsignedAttribute(WINDOW_CONFIG_ATTRIBUTE_THREAD);

		const char *path = root->Attribute(WINDOW_CONFIG_ATTRIBUTE_IMAGEPATH);
		const char *name = root->Attribute(WINDOW_CONFIG_ATTRIBUTE_IMAGENAME);
		const char *fname = root->Attribute(WINDOW_CONFIG_ATTRIBUTE_NAME);

		if ((path && name) || !fname)
		{
			AlleriaSetLastError(ALLERIA_ERROR_CONFIG_UNKOWN_EVENT);
			return FALSE;
		}

		std::string nameString;
		if (!path && !name)
		{
			PathToName(s_mainExePath, nameString);
			name = nameString.c_str();
		}

		UINT64 threshold = root->Unsigned64Attribute(WINDOW_CONFIG_ATTRIBUTE_THRESHOLD);
		bool isDecorated = root->BoolAttribute(WINDOW_CONFIG_ATTRIBUTE_DECORATED);

		if (threshold > 0 && threadId != 0)
		{
			AlleriaSetLastError(ALLERIA_ERROR_CONFIG_UNKOWN_EVENT);
			return FALSE;
		}

		EVENT_FUNC *eFunc = new EVENT_FUNC;
		eFunc->imagePathOrName = name == NULL ? path : name;
		StringToUpper(eFunc->imagePathOrName);
		eFunc->isDecorated = isDecorated;
		eFunc->isPath = name == NULL;
		eFunc->processId = processId;
		eFunc->threadId = threadId;
		eFunc->threshold = threshold;
		eFunc->name = fname;
		eFunc->call = xmlEl->FirstChildElement(WINDOW_CONFIG_TAG_CALL) != NULL;
		*e = eFunc;

		if (isDecorated)
			s_interestingFuncs.insert(
			std::make_pair(PIN_UndecorateSymbolName(fname, UNDECORATION::UNDECORATION_NAME_ONLY),
			std::make_pair(0, 0)));
		else
			s_interestingFuncs.insert(
			std::make_pair(fname,
			std::make_pair(0, 0)));
	}
	else if (root = xmlEl->FirstChildElement(WINDOW_CONFIG_TAG_THREADS))
	{
		unsigned int processId = root->UnsignedAttribute(WINDOW_CONFIG_ATTRIBUTE_PROCESS);

		UINT64 smallerEqual = root->Unsigned64Attribute(WINDOW_CONFIG_ATTRIBUTE_SMALLEREQUAL);
		UINT64 largerEqual = root->Unsigned64Attribute(WINDOW_CONFIG_ATTRIBUTE_LARGEREQUAL);

		if (smallerEqual + largerEqual == 0)
		{
			AlleriaSetLastError(ALLERIA_ERROR_CONFIG_UNKOWN_EVENT);
			return FALSE;
		}

		EVENT_COUNT_THREAD *ecThread = new EVENT_COUNT_THREAD;
		ecThread->processId = processId;
		ecThread->smallerThanOrEqual = smallerEqual;
		ecThread->largerThanOrEqual = largerEqual;
	}
	else if (root = xmlEl->FirstChildElement(WINDOW_CONFIG_TAG_ACCESSES))
	{
		unsigned int processId = root->UnsignedAttribute(WINDOW_CONFIG_ATTRIBUTE_PROCESS);
		unsigned int threadId = root->UnsignedAttribute(WINDOW_CONFIG_ATTRIBUTE_THREAD);

		UINT64 reads = root->Unsigned64Attribute(WINDOW_CONFIG_ATTRIBUTE_READS);
		UINT64 writes = root->Unsigned64Attribute(WINDOW_CONFIG_ATTRIBUTE_WRITES);
		UINT64 total = root->Unsigned64Attribute(WINDOW_CONFIG_ATTRIBUTE_TOTAL);

		if (((reads || writes) && total) || (!reads && !writes && !total))
		{
			AlleriaSetLastError(ALLERIA_ERROR_CONFIG_UNKOWN_EVENT);
			return FALSE;
		}

		bool anyThread = root->BoolAttribute(WINDOW_CONFIG_ATTRIBUTE_ANYTHREAD);

		EVENT_COUNT_ACCESS *eAccess = new EVENT_COUNT_ACCESS;

		if (total)
		{
			eAccess->accessType = ACCESS_TYPE::Total;
			eAccess->reads = total;
		}
		else
		{
			eAccess->accessType = ACCESS_TYPE::Detail;
			eAccess->reads = reads;
			eAccess->writes = writes;
		}

		eAccess->any = anyThread;
		eAccess->processId = processId;
		eAccess->threadId = threadId;
		*e = eAccess;
	}
	else if (root = xmlEl->FirstChildElement(WINDOW_CONFIG_TAG_INSTRUCTIONS))
	{
		unsigned int processId = root->UnsignedAttribute(WINDOW_CONFIG_ATTRIBUTE_PROCESS);
		unsigned int threadId = root->UnsignedAttribute(WINDOW_CONFIG_ATTRIBUTE_THREAD);

		UINT64 threshold = root->Unsigned64Attribute(WINDOW_CONFIG_ATTRIBUTE_THRESHOLD);
		if (threshold == 0)
		{
			AlleriaSetLastError(ALLERIA_ERROR_CONFIG_UNKOWN_EVENT);
			return FALSE;
		}

		bool anyThread = root->BoolAttribute(WINDOW_CONFIG_ATTRIBUTE_ANYTHREAD);

		EVENT_INSTRUCTION *eIns = new EVENT_INSTRUCTION;
		eIns->any = anyThread;
		eIns->processId = processId;
		eIns->threadId = threadId;
		eIns->threshold = threshold;
		*e = eIns;
	}
	else if (root = xmlEl->FirstChildElement(WINDOW_CONFIG_TAG_TIME))
	{
		unsigned int processId = root->UnsignedAttribute(WINDOW_CONFIG_ATTRIBUTE_PROCESS);
		unsigned int threadId = root->UnsignedAttribute(WINDOW_CONFIG_ATTRIBUTE_THREAD);
		UINT64 threshold = root->Unsigned64Attribute(WINDOW_CONFIG_ATTRIBUTE_THRESHOLD);
		if (threshold == 0)
		{
			AlleriaSetLastError(ALLERIA_ERROR_CONFIG_UNKOWN_EVENT);
			return FALSE;
		}
		EVENT_TIME *eTime = new EVENT_TIME;
		eTime->processId = processId;
		eTime->threadId = threadId;
		eTime->thresholdMilliseconds = threshold;
		*e = eTime;
	}
	else
		AlleriaSetLastError(ALLERIA_ERROR_CONFIG_UNKOWN_EVENT);

	if(e) (*e)->isTriggered = FALSE;
	
	return AlleriaGetLastError() == ALLERIA_ERROR_SUCCESS;
}

BOOL WINDOW_CONFIG::ProfileFromConfig(tinyxml2::XMLElement *xmlEl)
{
	tinyxml2::XMLElement *process = xmlEl->FirstChildElement(WINDOW_CONFIG_TAG_PROCESS);
	if (!process)
	{
		AlleriaSetLastError(ALLERIA_ERROR_CONFIG_PROFILE_PROCESS_DOES_NOT_EXIST);
		return FALSE;
	}

	while (process)
	{
		unsigned int processId = process->UnsignedAttribute(WINDOW_CONFIG_ATTRIBUTE_N);

		PROCESS_CONFIG pConfig;
		pConfig.any.valid = FALSE;
		if (processId == 0)
		{
			if (s_any.valid)
			{
				AlleriaSetLastError(ALLERIA_ERROR_CONFIG_PROFILE_INVALID_PROFILE);
				return FALSE;
			}
		}
		else
		{
			if (s_processMap.find(processId) != s_processMap.end())
			{
				AlleriaSetLastError(ALLERIA_ERROR_CONFIG_PROFILE_INVALID_PROFILE);
				return FALSE;
			}
		}

		tinyxml2::XMLElement *thread = process->FirstChildElement(WINDOW_CONFIG_TAG_THREAD);
		if (!thread)
		{
			// Allow a empty process tag.
			// This enables the user to attach Alleria to every process in
			// the process tree, but only profile specific processes.
			// This means that process numbers are computed overall
			// rather than to only those injected child processes.

			pConfig.any.valid = FALSE;
		}

		while (thread)
		{
			unsigned int threadId = thread->UnsignedAttribute(WINDOW_CONFIG_ATTRIBUTE_N);

			THREAD_CONFIG tConfig;
			tConfig.any.valid = FALSE;
			tConfig.anyButSys.valid = FALSE;
			if (threadId == 0)
			{
				if (pConfig.any.valid)
				{
					AlleriaSetLastError(ALLERIA_ERROR_CONFIG_PROFILE_INVALID_PROFILE);
					return FALSE;
				}
			}
			else
			{
				if (pConfig.threadMap.find(threadId) != pConfig.threadMap.end())
				{
					AlleriaSetLastError(ALLERIA_ERROR_CONFIG_PROFILE_INVALID_PROFILE);
					return FALSE;
				}
			}

			tinyxml2::XMLElement *image = thread->FirstChildElement(WINDOW_CONFIG_TAG_IMAGE);
			if (!image)
			{
				AlleriaSetLastError(ALLERIA_ERROR_CONFIG_PROFILE_IMAGE_DOES_NOT_EXIST);
				return FALSE;
			}

			while (image)
			{
				std::string nameString;
				const char *path = image->Attribute(WINDOW_CONFIG_ATTRIBUTE_PATH);
				const char *name = image->Attribute(WINDOW_CONFIG_ATTRIBUTE_NAME);
				BOOL isMain = image->BoolAttribute(WINDOW_CONFIG_ATTRIBUTE_MAIN);
				BOOL noSys = image->BoolAttribute(WINDOW_CONFIG_ATTRIBUTE_ANYBUTSYS);

				if ((path && name) || (isMain && noSys) ||
					(path && isMain) || (name && isMain) ||
					(path && noSys) || (name && noSys))
				{
					AlleriaSetLastError(ALLERIA_ERROR_CONFIG_PROFILE_INVALID_PROFILE);
					return FALSE;
				}

				if (isMain)
				{
					PathToName(s_mainExePath, nameString);
					name = nameString.c_str();
				}
				else if (path)
				{
					PathToName(path, nameString);
					name = nameString.c_str();
				}

				IMAGE_CONFIG iConfig;
				iConfig.any.valid = FALSE;
				std::string pathStr, nameStr;

				if (!path && !name)
				{
					if ((!noSys && tConfig.any.valid) || (noSys && tConfig.anyButSys.valid))
					{
						AlleriaSetLastError(ALLERIA_ERROR_CONFIG_PROFILE_INVALID_PROFILE);
						return FALSE;
					}
				}
				else
				{
					if (path)
					{
						pathStr = path;
						StringToUpper(pathStr);
					}
					if (name)
					{
						nameStr = name;
						StringToUpper(nameStr);
					}

					if (tConfig.imageMap.find(nameStr) != tConfig.imageMap.end())
					{
						AlleriaSetLastError(ALLERIA_ERROR_CONFIG_PROFILE_INVALID_PROFILE);
						return FALSE;
					}
				}

				tinyxml2::XMLElement *func = image->FirstChildElement(WINDOW_CONFIG_TAG_FUNCTION);

				// Allow an image to have no functions under it so that it's excluded from profiling.
				/*if (!func)
				{
					AlleriaSetLastError(ALLERIA_ERROR_CONFIG_PROFILE_FUNCTION_DOES_NOT_EXIST);
					return FALSE;
				}*/

				while (func)
				{
					if (func->NoChildren())
					{
						AlleriaSetLastError(ALLERIA_ERROR_CONFIG_PROFILE_FUNCTION_DOES_NOT_EXIST);
						return FALSE;
					}

					const char *fname = func->Attribute("name");
					bool isDecorated = func->BoolAttribute(WINDOW_CONFIG_ATTRIBUTE_DECORATED);

					FUNC_CONFIG fConfig;
					fConfig.any.valid = FALSE;
					fConfig.anyMem.valid = FALSE;
					//fConfig.intercept.valid = FALSE;
					if (!fname)
					{
						if (iConfig.any.valid)
						{
							AlleriaSetLastError(ALLERIA_ERROR_CONFIG_PROFILE_INVALID_PROFILE);
							return FALSE;
						}
					}
					else
					{
						if ((isDecorated && iConfig.decoratedMap.find(fname) != iConfig.decoratedMap.end()) ||
							(!isDecorated && iConfig.undecoratedMap.find(fname) != iConfig.undecoratedMap.end()))
						{
							AlleriaSetLastError(ALLERIA_ERROR_CONFIG_PROFILE_INVALID_PROFILE);
							return FALSE;
						}
					}

					tinyxml2::XMLElement *core = func->FirstChild()->ToElement();
					if (!core)
					{
						AlleriaSetLastError(ALLERIA_ERROR_CONFIG_PROFILE_INVALID_PROFILE);
						return FALSE;
					}

					INTERCEPT_CONFIG interceptorConfig;
					interceptorConfig.valid = false;
					interceptorConfig.intrumented = false;
					while (core)
					{
						if (strcmp(core->Value(), WINDOW_CONFIG_TAG_INSTRUCTION) == 0)
						{
							INS_CONFIG insConfig;
							// cat or ext or class or form or isa
							const char *value = NULL;

							insConfig.id = static_cast<INS_ID>(1 +
								OnlyOneAttributeExists(
								*core,
								INS_CONFIG_STRINGS,
								sizeof(INS_CONFIG_STRINGS) / sizeof(char*), &value));

							if (!value)
							{
								AlleriaSetLastError(ALLERIA_ERROR_CONFIG_PROFILE_INVALID_PROFILE);
								return FALSE;
							}

							if (insConfig.id == INS_ID::Category)
							{
								if (strcmp(value, WINDOW_CONFIG_ATTRIBUTE_ANY) == 0)
								{
									insConfig.id = INS_ID::Any;
								}
								else if (strcmp(value, WINDOW_CONFIG_ATTRIBUTE_ANYMEM) == 0)
								{
									insConfig.id = INS_ID::AnyMem;
								}
							}

							switch (insConfig.id)
							{
							case INS_ID::Category:
								insConfig.value = str2xed_category_enum_t(value);
								break;
							case INS_ID::Class:
								insConfig.value = str2xed_iclass_enum_t(value);
								break;
							case INS_ID::Extension:
								insConfig.value = str2xed_extension_enum_t(value);
								break;
							case INS_ID::Form:
								insConfig.value = str2xed_iform_enum_t(value);
								break;
							case INS_ID::IsaSet:
								insConfig.value = str2xed_isa_set_enum_t(value);
								break;
							}

							insConfig.timeStamp = core->BoolAttribute(WINDOW_CONFIG_ATTRIBUTE_TIMESTAMP);
							insConfig.routineId = core->BoolAttribute(WINDOW_CONFIG_ATTRIBUTE_ROUTINEID);
							insConfig.accessAddressAndSize = core->BoolAttribute(WINDOW_CONFIG_ATTRIBUTE_ACCESSADDRESSANDISZE);
							insConfig.accessValue = core->BoolAttribute(WINDOW_CONFIG_ATTRIBUTE_ACCESSVALUE);

							insConfig.valid = TRUE;

							switch (insConfig.id)
							{
							case INS_ID::Any:
								if (fConfig.any.valid) goto err;
								fConfig.any = insConfig;
								break;
							case INS_ID::AnyMem:
								if (fConfig.anyMem.valid) goto err;
								fConfig.anyMem = insConfig;
								break;
							case INS_ID::Category:
								if (fConfig.catMap.find(insConfig.value) != fConfig.catMap.end() || 
									insConfig.value == xed_category_enum_t::XED_CATEGORY_INVALID) goto err;
								fConfig.catMap.insert(std::make_pair(insConfig.value, insConfig));
								break;
							case INS_ID::Class:
								if (fConfig.classMap.find(insConfig.value) != fConfig.classMap.end() ||
									insConfig.value == xed_iclass_enum_t::XED_ICLASS_INVALID) goto err;
								fConfig.classMap.insert(std::make_pair(insConfig.value, insConfig));
								break;
							case INS_ID::Extension:
								if (fConfig.extMap.find(insConfig.value) != fConfig.extMap.end() ||
									insConfig.value == xed_extension_enum_t::XED_EXTENSION_INVALID) goto err;
								fConfig.extMap.insert(std::make_pair(insConfig.value, insConfig));
								break;
							case INS_ID::Form:
								if (fConfig.formMap.find(insConfig.value) != fConfig.formMap.end() ||
									insConfig.value == xed_iform_enum_t::XED_IFORM_INVALID) goto err;
								fConfig.formMap.insert(std::make_pair(insConfig.value, insConfig));
								break;
							case INS_ID::IsaSet:
								if (fConfig.isaSetMap.find(insConfig.value) != fConfig.isaSetMap.end() ||
									insConfig.value == xed_isa_set_enum_t::XED_ISA_SET_INVALID) goto err;
								fConfig.isaSetMap.insert(std::make_pair(insConfig.value, insConfig));
								break;
							}
							goto success;
						err:
							AlleriaSetLastError(ALLERIA_ERROR_CONFIG_PROFILE_INVALID_PROFILE);
							return FALSE;
						success:;
						}
						else if (strcmp(core->Value(), WINDOW_CONFIG_TAG_INTERCEPT) == 0 && !interceptorConfig.valid)
						{
							const char *type = core->Attribute(WINDOW_CONFIG_ATTRIBUTE_RETTYPE);

							if (!type)
							{
								interceptorConfig.returnType.typeSpec = INTERCEPT_TYPE_SPEC_INVALID;
							}
							else
							{
								interceptorConfig.returnType.typeSpec = FindType(type, interceptorConfig.returnType.typeIndex);

								if (interceptorConfig.returnType.typeSpec == INTERCEPT_TYPE_SPEC_INVALID)
								{
									AlleriaSetLastError(ALLERIA_ERROR_CONFIG_PROFILE_INVALID_PROFILE);
									return FALSE;
								}
							}

							tinyxml2::XMLElement *parameter = core->FirstChildElement(WINDOW_CONFIG_TAG_PARAMETER);
							while (parameter)
							{
								INTERCEPT_PARAMETER paramDesc;

								const char *name = parameter->Attribute(WINDOW_CONFIG_ATTRIBUTE_NAME); // Optional.
								const char *type = parameter->Attribute(WINDOW_CONFIG_ATTRIBUTE_TYPE); // Mandatory.

								if (!type)
								{
									AlleriaSetLastError(ALLERIA_ERROR_CONFIG_PROFILE_INVALID_PROFILE);
									return FALSE;
								}

								paramDesc.parameter.typeSpec = FindType(type, paramDesc.parameter.typeIndex);

								if (paramDesc.parameter.typeSpec == INTERCEPT_TYPE_SPEC_INVALID ||
									parameter->QueryUnsignedAttribute(WINDOW_CONFIG_ATTRIBUTE_INDEX, &paramDesc.index) != tinyxml2::XML_NO_ERROR) // Mandatory.
								{
									AlleriaSetLastError(ALLERIA_ERROR_CONFIG_PROFILE_INVALID_PROFILE);
									return FALSE;
								}

								BOOL in, out;
								tinyxml2::XMLError xmlErrIn = parameter->QueryBoolAttribute(WINDOW_CONFIG_ATTRIBUTE_IN, &in);
								tinyxml2::XMLError xmlErrOut = parameter->QueryBoolAttribute(WINDOW_CONFIG_ATTRIBUTE_OUT, &out);
								
								if (xmlErrIn == tinyxml2::XML_NO_ERROR && xmlErrOut == tinyxml2::XML_NO_ATTRIBUTE && in)
								{
									paramDesc.usage = INTERCEPT_PARAMETER_USAGE_INPUT;
								}
								else if(xmlErrOut == tinyxml2::XML_NO_ERROR && xmlErrIn == tinyxml2::XML_NO_ATTRIBUTE && out)
								{
									paramDesc.usage = INTERCEPT_PARAMETER_USAGE_OUTPUT;
								}
								else if (xmlErrIn == tinyxml2::XML_NO_ERROR && xmlErrOut == tinyxml2::XML_NO_ERROR && in && out)
								{
									paramDesc.usage = static_cast<INTERCEPT_PARAMETER_USAGE>(INTERCEPT_PARAMETER_USAGE_INPUT | INTERCEPT_PARAMETER_USAGE_OUTPUT);
								}
								else
								{
									AlleriaSetLastError(ALLERIA_ERROR_CONFIG_PROFILE_INVALID_PROFILE);
									return FALSE;
								}

								if (name == NULL)
									paramDesc.parameter.name = "";
								else
								{
									// Make sure that the parameter name has not been used before.
									for (size_t i = 0; i < interceptorConfig.parameters.size(); ++i)
									{
										if (strcmp(name, interceptorConfig.parameters[i].parameter.name.c_str()) == 0)
										{
											AlleriaSetLastError(ALLERIA_ERROR_CONFIG_PROFILE_INVALID_PROFILE);
											return FALSE;
										}
									}
									paramDesc.parameter.name = name;
								}

								interceptorConfig.parameters.push_back(paramDesc);
								parameter = parameter->NextSiblingElement(WINDOW_CONFIG_TAG_PARAMETER);
							}

							// Make sure that parameters and the return value type of pointer types that depend on parameters are valid.
							size_t i = -1; // Start at -1 to account for the return type.
							INTERCEPT_TYPE_FIELD *typeField = &interceptorConfig.returnType;
							while (i < interceptorConfig.parameters.size())
							{
								if (i >= 0)
									typeField = &interceptorConfig.parameters[i].parameter;

								if (typeField->typeSpec == INTERCEPT_TYPE_SPEC_POINTER)
								{
									const char *paramName = NULL;
									INTERCEPT_TYPE_POINTER_TYPE ptype = s_pointerTypes[typeField->typeIndex].type;
									if (ptype == INTERCEPT_TYPE_POINTER_TYPE_COUNT)
										paramName = s_pointerTypes[typeField->typeIndex].count.name;
									else if (ptype == INTERCEPT_TYPE_POINTER_TYPE_END_POINTER)
										paramName = s_pointerTypes[typeField->typeIndex].startPointerEnd.endPointerName;
									else if (ptype == INTERCEPT_TYPE_POINTER_TYPE_START_POINTER)
										paramName = s_pointerTypes[typeField->typeIndex].endPointerStart.startPointerName;

									if (paramName)
									{
										// Find the parameter and make sure it has a primitive type.
										size_t j;
										for (j = 0; j < interceptorConfig.parameters.size(); ++j)
										{
											if (strcmp(interceptorConfig.parameters[j].parameter.name.c_str(), paramName) == 0)
											{
												// Note: any input/output paramter inconsistent dependencies are handled cleverly by the implemented interception logic.
												if (!IsTypeFieldValidForPointer(s_pointerTypes[typeField->typeIndex], interceptorConfig.parameters[j].parameter))
													j = interceptorConfig.parameters.size();
												else
													typeField->fieldOrParamIndex = static_cast<UINT32>(j);
												break;
											}
										}

										if (j == interceptorConfig.parameters.size())
											return FALSE; // Referenced field not found.
									}
								}

								++i;
							}

							interceptorConfig.valid = TRUE;
							fConfig.intercept = interceptorConfig;
						}
						else
						{
							AlleriaSetLastError(ALLERIA_ERROR_CONFIG_PROFILE_INVALID_PROFILE);
							return FALSE;
						}

						if (core->NextSibling())
							core = core->NextSibling()->ToElement();
						else
							core = NULL;
					}

					fConfig.valid = TRUE;

					if (!fname)
						iConfig.any = fConfig;
					else if (isDecorated)
						iConfig.decoratedMap.insert(std::make_pair(fname, fConfig));
					else
						iConfig.undecoratedMap.insert(std::make_pair(fname, fConfig));

					if (func->NextSibling())
						func = func->NextSibling()->ToElement();
					else
						func = NULL;
				}

				iConfig.pathValid = path != NULL;
				iConfig.path = path != NULL ? pathStr : "";
				iConfig.valid = TRUE;

				if (!path && !name)
				{
					if (noSys)
						tConfig.anyButSys = iConfig;
					else
						tConfig.any = iConfig;
				}
				else
					tConfig.imageMap.insert(std::make_pair(nameStr, iConfig));

				if (image->NextSibling())
					image = image->NextSibling()->ToElement();
				else
					image = NULL;
			}

			tConfig.valid = TRUE;

			if (threadId == 0)
				pConfig.any = tConfig;
			else
				pConfig.threadMap.insert(std::make_pair(threadId, tConfig));

			thread = thread->NextSiblingElement(WINDOW_CONFIG_TAG_THREAD);
		}

		pConfig.valid = TRUE;

		if (processId == 0)
			s_any = pConfig;
		else
			s_processMap.insert(std::make_pair(processId, pConfig));

		process = process->NextSiblingElement(WINDOW_CONFIG_TAG_PROCESS);
	}

	return AlleriaGetLastError() == ALLERIA_ERROR_SUCCESS;
}

BOOL WINDOW_CONFIG::IsPointerTypeValidForPointer(INTERCEPT_TYPE_POINTER& pointer, BOOL nestedPointer)
{
	if (nestedPointer)
	{
		if (pointer.type == INTERCEPT_TYPE_POINTER_TYPE_START_POINTER ||
			pointer.type == INTERCEPT_TYPE_POINTER_TYPE_END_POINTER ||
			pointer.type == INTERCEPT_TYPE_POINTER_TYPE_COUNT)
			return FALSE;
	}
	return TRUE;
}

BOOL WINDOW_CONFIG::IsElementTypeValidForPointer(INTERCEPT_TYPE_POINTER& pointer)
{
	// This function makes sure that if the pointer type points to an object
	// that contains other pointers, these pointers must not reference parameters.

	if (pointer.elementType.typeSpec == INTERCEPT_TYPE_SPEC_INVALID)
	{
		return TRUE;
	}
	else if (pointer.elementType.typeSpec == INTERCEPT_TYPE_SPEC_CUSTOM)
	{
		INTERCEPT_CUSTOM_TYPE& customType = s_customTypes[pointer.elementType.typeIndex];
		for (UINT32 i = 0; i < customType.fields.size(); ++i)
		{
			if (customType.fields[i].typeSpec == INTERCEPT_TYPE_SPEC_POINTER)
			{
				if (!IsPointerTypeValidForPointer(s_pointerTypes[customType.fields[i].typeIndex], FALSE))
					return FALSE;
			}
		}
	}
	else if (pointer.elementType.typeSpec == INTERCEPT_TYPE_SPEC_POINTER)
	{
		if (!IsPointerTypeValidForPointer(s_pointerTypes[pointer.elementType.typeIndex], TRUE))
			return FALSE;
	}
	return TRUE;
}

BOOL WINDOW_CONFIG::IsTypeFieldValidForPointer(INTERCEPT_TYPE_POINTER& pointer, INTERCEPT_TYPE_FIELD& typeField)
{
	// Note that if the field or parameter represents the count, we allow both signed and unsigned integral types.
	// That's because it's common that signed integers are used to represent sizes.

	if (typeField.typeSpec == INTERCEPT_TYPE_SPEC_CUSTOM || typeField.typeSpec == INTERCEPT_TYPE_SPEC_INVALID)
	{
		// A pointer type cannot refernce a field or parameter that has custom type. This is not supported.
		return FALSE;
	}
	else if (typeField.typeSpec == INTERCEPT_TYPE_SPEC_POINTER)
	{
		INTERCEPT_TYPE_POINTER& tptr = s_pointerTypes[typeField.typeIndex];

		if (pointer.type == INTERCEPT_TYPE_POINTER_TYPE_COUNT)
		{
			// The pointer requires a field that stores a counter. Consider the field valid only if it's a pointer to a single value of integral type.
			if (tptr.type != INTERCEPT_TYPE_POINTER_TYPE_FIXED || tptr.fixed.count != 1 || 
				tptr.elementType.typeSpec == INTERCEPT_TYPE_SPEC_CUSTOM || tptr.elementType.typeSpec == INTERCEPT_TYPE_SPEC_INVALID ||
				tptr.elementType.typeSpec == INTERCEPT_TYPE_SPEC_POINTER ||
				tptr.elementType.typeSpec == INTERCEPT_TYPE_SPEC_ASCII ||
				tptr.elementType.typeSpec == INTERCEPT_TYPE_SPEC_UNICODE)
			{
				return FALSE;
			}
		}
		else
		{
			// The pointer type is one of these:
			// INTERCEPT_TYPE_POINTER_TYPE_START_POINTER or INTERCEPT_TYPE_POINTER_TYPE_END_POINTER.
			// So make sure that the type field has the approperiate type.

			// Both pointers point to elements of the same type.
			if (tptr.elementType.typeSpec != pointer.elementType.typeSpec)
				return FALSE;
			else
			{
				if ((tptr.elementType.typeSpec == INTERCEPT_TYPE_SPEC_CUSTOM || tptr.elementType.typeSpec == INTERCEPT_TYPE_SPEC_POINTER)
					&& tptr.elementType.typeIndex != pointer.elementType.typeIndex)
					return FALSE;
			}

			// If the pointer stores the address of the first element, the field must store the address of the last or past the last element.
			if (tptr.type == INTERCEPT_TYPE_POINTER_TYPE_START_POINTER && (pointer.type != INTERCEPT_TYPE_POINTER_TYPE_END_POINTER ||
				tptr.endPointerStart.endValid != pointer.startPointerEnd.endValid))
				return FALSE;

			// If the pointer stores the address of the last or past the last element, the field must store the address of the first element.
			if (tptr.type == INTERCEPT_TYPE_POINTER_TYPE_END_POINTER && (pointer.type != INTERCEPT_TYPE_POINTER_TYPE_START_POINTER ||
				pointer.endPointerStart.endValid != tptr.startPointerEnd.endValid))
				return FALSE;
		}
	}
	else if (pointer.type != INTERCEPT_TYPE_POINTER_TYPE_COUNT)
	{
		// The referenced field or parameter has an integral type. Therefore, it must represent the count.
		return FALSE;
	}
	return TRUE;
}

WINDOW_CONFIG::INTERCEPT_TYPE_SPEC WINDOW_CONFIG::FindType(const char *typeName, UINT32& typeIndex)
{
	INTERCEPT_TYPE_SPEC typeSpec = INTERCEPT_TYPE_SPEC_INVALID;

	if (strcmp(typeName, WINDOW_CONFIG_TYPE_S1) == 0)
		typeSpec = INTERCEPT_TYPE_SPEC_SINT1;
	else if (strcmp(typeName, WINDOW_CONFIG_TYPE_S2) == 0)
		typeSpec = INTERCEPT_TYPE_SPEC_SINT2;
	else if (strcmp(typeName, WINDOW_CONFIG_TYPE_S3) == 0)
		typeSpec = INTERCEPT_TYPE_SPEC_SINT3;
	else if (strcmp(typeName, WINDOW_CONFIG_TYPE_S4) == 0)
		typeSpec = INTERCEPT_TYPE_SPEC_SINT4;
	else if (strcmp(typeName, WINDOW_CONFIG_TYPE_S5) == 0)
		typeSpec = INTERCEPT_TYPE_SPEC_SINT5;
	else if (strcmp(typeName, WINDOW_CONFIG_TYPE_S6) == 0)
		typeSpec = INTERCEPT_TYPE_SPEC_SINT6;
	else if (strcmp(typeName, WINDOW_CONFIG_TYPE_S7) == 0)
		typeSpec = INTERCEPT_TYPE_SPEC_SINT7;
	else if (strcmp(typeName, WINDOW_CONFIG_TYPE_S8) == 0)
		typeSpec = INTERCEPT_TYPE_SPEC_SINT8;
	else if (strcmp(typeName, WINDOW_CONFIG_TYPE_U1) == 0)
		typeSpec = INTERCEPT_TYPE_SPEC_UINT1;
	else if (strcmp(typeName, WINDOW_CONFIG_TYPE_U2) == 0)
		typeSpec = INTERCEPT_TYPE_SPEC_UINT2;
	else if (strcmp(typeName, WINDOW_CONFIG_TYPE_U3) == 0)
		typeSpec = INTERCEPT_TYPE_SPEC_UINT3;
	else if (strcmp(typeName, WINDOW_CONFIG_TYPE_U4) == 0)
		typeSpec = INTERCEPT_TYPE_SPEC_UINT4;
	else if (strcmp(typeName, WINDOW_CONFIG_TYPE_U5) == 0)
		typeSpec = INTERCEPT_TYPE_SPEC_UINT5;
	else if (strcmp(typeName, WINDOW_CONFIG_TYPE_U6) == 0)
		typeSpec = INTERCEPT_TYPE_SPEC_UINT6;
	else if (strcmp(typeName, WINDOW_CONFIG_TYPE_U7) == 0)
		typeSpec = INTERCEPT_TYPE_SPEC_UINT7;
	else if (strcmp(typeName, WINDOW_CONFIG_TYPE_U8) == 0)
		typeSpec = INTERCEPT_TYPE_SPEC_UINT8;
	else if (strcmp(typeName, WINDOW_CONFIG_TYPE_ASCII) == 0)
		typeSpec = INTERCEPT_TYPE_SPEC_ASCII;
	else if (strcmp(typeName, WINDOW_CONFIG_TYPE_UNICODE) == 0)
		typeSpec = INTERCEPT_TYPE_SPEC_UNICODE;
	else
	{
		// It's either a custom type, a pointer type or an undefined type.
		size_t i;
		for (i = 0; i < s_customTypes.size(); ++i)
		{
			if (strcmp(typeName, s_customTypes[i].name.c_str()) == 0)
			{
				typeSpec = INTERCEPT_TYPE_SPEC_CUSTOM;
				typeIndex = static_cast<UINT32>(i);
				break;
			}
		}

		if (i == s_customTypes.size())
		{
			for (i = 0; i < s_pointerTypes.size(); ++i)
			{
				if (strcmp(typeName, s_pointerTypes[i].name.c_str()) == 0)
				{
					typeSpec = INTERCEPT_TYPE_SPEC_POINTER;
					typeIndex = static_cast<UINT32>(i);
					break;
				}
			}
		}
	}

	return typeSpec;
}

BOOL WINDOW_CONFIG::IsTypeNameUnique(const char *typeName)
{
	if (strcmp(typeName, WINDOW_CONFIG_TYPE_S1) == 0 ||
		strcmp(typeName, WINDOW_CONFIG_TYPE_S2) == 0 ||
		strcmp(typeName, WINDOW_CONFIG_TYPE_S3) == 0 ||
		strcmp(typeName, WINDOW_CONFIG_TYPE_S4) == 0 ||
		strcmp(typeName, WINDOW_CONFIG_TYPE_S5) == 0 ||
		strcmp(typeName, WINDOW_CONFIG_TYPE_S6) == 0 ||
		strcmp(typeName, WINDOW_CONFIG_TYPE_S7) == 0 ||
		strcmp(typeName, WINDOW_CONFIG_TYPE_S8) == 0 ||
		strcmp(typeName, WINDOW_CONFIG_TYPE_U1) == 0 ||
		strcmp(typeName, WINDOW_CONFIG_TYPE_U2) == 0 ||
		strcmp(typeName, WINDOW_CONFIG_TYPE_U3) == 0 ||
		strcmp(typeName, WINDOW_CONFIG_TYPE_U4) == 0 ||
		strcmp(typeName, WINDOW_CONFIG_TYPE_U5) == 0 ||
		strcmp(typeName, WINDOW_CONFIG_TYPE_U6) == 0 ||
		strcmp(typeName, WINDOW_CONFIG_TYPE_U7) == 0 ||
		strcmp(typeName, WINDOW_CONFIG_TYPE_U8) == 0 ||
		strcmp(typeName, WINDOW_CONFIG_TYPE_ASCII) == 0 ||
		strcmp(typeName, WINDOW_CONFIG_TYPE_UNICODE) == 0)
		return FALSE;
	else
	{
		for (size_t i = 0; i < s_customTypes.size(); ++i)
		{
			if (strcmp(typeName, s_customTypes[i].name.c_str()) == 0)
				return FALSE;
		}
		for (size_t i = 0; i < s_pointerTypes.size(); ++i)
		{
			if (strcmp(typeName, s_pointerTypes[i].name.c_str()) == 0)
				return FALSE;
		}
	}
	return TRUE;
}

BOOL WINDOW_CONFIG::TypesFromConfig(tinyxml2::XMLElement *xmlEl)
{
	tinyxml2::XMLElement *child = xmlEl->FirstChildElement();
	INTERCEPT_CUSTOM_TYPE customType;
	INTERCEPT_TYPE_POINTER pointerType;
	while (child)
	{
		if (strcmp(child->Name(), WINDOW_CONFIG_TAG_TYPE) == 0)
		{
			const char *typeName = child->Attribute(WINDOW_CONFIG_ATTRIBUTE_NAME);
			tinyxml2::XMLElement *field = child->FirstChildElement(WINDOW_CONFIG_TAG_FIELD);
			if (!field || !typeName || !IsTypeNameUnique(typeName))
				return FALSE;

			while (field)
			{
				if (customType.fields.size() == (UINT32)-1)
					return FALSE; // Maximum number of fields exceeded.

				INTERCEPT_TYPE_FIELD typeField;
				const char *fieldName = field->Attribute(WINDOW_CONFIG_ATTRIBUTE_NAME);
				const char *fieldTypeName = field->Attribute(WINDOW_CONFIG_ATTRIBUTE_TYPE);
				if (!fieldName || !fieldTypeName)
					return FALSE;

				// Make sure that the field name has not been used before.
				for (size_t i = 0; i < customType.fields.size(); ++i)
				{
					if (strcmp(fieldTypeName, customType.fields[i].name.c_str()) == 0)
						return FALSE;
				}

				typeField.name = fieldName;
				typeField.typeIndex = 0;

				typeField.typeSpec = FindType(fieldTypeName, typeField.typeIndex);
				if (typeField.typeSpec == INTERCEPT_TYPE_SPEC_INVALID)
					return FALSE;

				customType.fields.push_back(typeField);
				field = child->NextSiblingElement(WINDOW_CONFIG_TAG_FIELD);
			}

			// Make sure that fields of pointer types that depend on fields are valid.
			for (size_t i = 0; i < customType.fields.size(); ++i)
			{
				if (customType.fields[i].typeSpec == INTERCEPT_TYPE_SPEC_POINTER)
				{
					const char *fieldName = NULL;
					INTERCEPT_TYPE_POINTER_TYPE ptype = s_pointerTypes[customType.fields[i].typeIndex].type;
					if (ptype == INTERCEPT_TYPE_POINTER_TYPE_COUNT)
						fieldName = s_pointerTypes[customType.fields[i].typeIndex].count.name;
					else if (ptype == INTERCEPT_TYPE_POINTER_TYPE_END_POINTER)
						fieldName = s_pointerTypes[customType.fields[i].typeIndex].startPointerEnd.endPointerName;
					else if (ptype == INTERCEPT_TYPE_POINTER_TYPE_START_POINTER)
						fieldName = s_pointerTypes[customType.fields[i].typeIndex].endPointerStart.startPointerName;

					if (fieldName)
					{
						// Find the field and make sure it has a primitive type.
						size_t j;
						for (j = 0; j < customType.fields.size(); ++j)
						{
							if (strcmp(customType.fields[j].name.c_str(), fieldName) == 0)
							{
								if (!IsTypeFieldValidForPointer(s_pointerTypes[customType.fields[i].typeIndex], customType.fields[j]))
									j = customType.fields.size();
								else
									customType.fields[i].fieldOrParamIndex = static_cast<UINT32>(j);
								break;
							}
						}

						if (j == customType.fields.size())
							return FALSE; // Referenced field not found.
					}
				}
			}

			customType.name = typeName;
			s_customTypes.push_back(customType);
		}
		else if (strcmp(child->Name(), WINDOW_CONFIG_TAG_POINTER) == 0)
		{
			const char *typeName = child->Attribute(WINDOW_CONFIG_ATTRIBUTE_NAME);
			const char *elemenTypeName = child->Attribute(WINDOW_CONFIG_ATTRIBUTE_ELEMENTTYPE);
			if (!typeName || !elemenTypeName)
				return FALSE;

			pointerType.elementType.typeSpec = FindType(elemenTypeName, pointerType.elementType.typeIndex);
			if (pointerType.elementType.typeSpec == INTERCEPT_TYPE_SPEC_INVALID ||
				(pointerType.elementType.typeSpec == INTERCEPT_TYPE_SPEC_POINTER && 
				!IsPointerTypeValidForPointer(s_pointerTypes[pointerType.elementType.typeIndex], TRUE)))
				return FALSE;

			pointerType.elementType.name = "";

			const char *endValid = child->Attribute(WINDOW_CONFIG_ATTRIBUTE_ENDVALID);
			if (!endValid)
			{
				const char *direction = child->Attribute(WINDOW_CONFIG_ATTRIBUTE_DIRECTION);
				INTERCEPT_TYPE_POINTER_DIRECTION dir = INTERCEPT_TYPE_POINTER_DIRECTION_INCREMENT;
				if (direction != NULL)
				{
					if (strcmp(direction, "inc") == 0)
					{
						dir = INTERCEPT_TYPE_POINTER_DIRECTION_INCREMENT;
					}
					else if (strcmp(direction, "dec") == 0)
					{
						dir = INTERCEPT_TYPE_POINTER_DIRECTION_DECREMENT;
					}
					else
					{
						return FALSE;
					}
				}

				const char *attr = child->Attribute(WINDOW_CONFIG_ATTRIBUTE_COUNT);
				if (attr)
				{
#ifdef TARGET_IA32
					if (child->QueryUnsignedAttribute(WINDOW_CONFIG_ATTRIBUTE_COUNT, &pointerType.fixed.count) != tinyxml2::XML_NO_ERROR)
						return FALSE;
#else
					if (child->QueryUnsigned64Attribute(WINDOW_CONFIG_ATTRIBUTE_COUNT, &pointerType.fixed.count) != tinyxml2::XML_NO_ERROR)
						return FALSE;
#endif /* TARGET_IA32 */
					if (pointerType.fixed.count > 1 && direction == NULL)
						return FALSE;

					pointerType.fixed.direction = dir;
					pointerType.type = INTERCEPT_TYPE_POINTER_TYPE_FIXED;
				}
				else
				{
					if (direction == NULL)
						return FALSE;

					attr = child->Attribute(WINDOW_CONFIG_ATTRIBUTE_SENTINEL);
					if (attr)
					{
						if (child->QueryUnsigned64Attribute(WINDOW_CONFIG_ATTRIBUTE_SENTINEL, &pointerType.sentinel.sentinelValue) != tinyxml2::XML_NO_ERROR)
							return FALSE;

						pointerType.sentinel.direction = dir;
						pointerType.type = INTERCEPT_TYPE_POINTER_TYPE_SENTINEL;
					}
					else
					{
						attr = child->Attribute(WINDOW_CONFIG_ATTRIBUTE_COUNTVARIABLE);
						if (attr)
						{
							pointerType.count.name = new char[strlen(attr) + 1];
							strcpy(pointerType.count.name, attr);
							pointerType.count.direction = dir;
							pointerType.type = INTERCEPT_TYPE_POINTER_TYPE_COUNT;
						}
						else
							return FALSE;
					}
				}
			}
			else
			{
				BOOL endValidBool;
				if (child->QueryBoolAttribute(WINDOW_CONFIG_ATTRIBUTE_ENDVALID, &endValidBool) != tinyxml2::XML_NO_ERROR)
					return FALSE;

				const char *attr = child->Attribute(WINDOW_CONFIG_ATTRIBUTE_ENDVARIABLE);
				if (attr)
				{
					pointerType.startPointerEnd.endPointerName = new char[strlen(attr) + 1];
					strcpy(pointerType.startPointerEnd.endPointerName, attr);
					pointerType.startPointerEnd.endValid = endValidBool;
					pointerType.type = INTERCEPT_TYPE_POINTER_TYPE_END_POINTER;
				}
				else
				{
					attr = child->Attribute(WINDOW_CONFIG_ATTRIBUTE_STARTVARIABLE);
					if (attr)
					{
						pointerType.endPointerStart.startPointerName = new char[strlen(attr) + 1];
						strcpy(pointerType.endPointerStart.startPointerName, attr);
						pointerType.endPointerStart.endValid = endValidBool;
						pointerType.type = INTERCEPT_TYPE_POINTER_TYPE_START_POINTER;
					}
					else
						return FALSE;
				}
			}

			pointerType.name = typeName;
			s_pointerTypes.push_back(pointerType);
		}
		else
		{
			return FALSE;
		}

		child = child->NextSiblingElement();

		if ((static_cast<UINT64>(s_customTypes.size()) + 
			static_cast<UINT64>(s_pointerTypes.size()) +
			INTERCEPT_TYPE_PRIMITIVE_COUNT == (UINT32)-1) &&
			!child)
			return FALSE; // Maximum number of types exceeded.
	}
	return TRUE;
}

BOOL WINDOW_CONFIG::InitFromConfig(tinyxml2::XMLDocument& xmlDoc)
{
	tinyxml2::XMLElement *root = xmlDoc.FirstChildElement(WINDOW_CONFIG_TAG_ALLERIACONFIG);

	if (root)
	{
		if (root->Attribute(WINDOW_CONFIG_ATTRIBUTE_VERSION, "1.0"))
		{
			tinyxml2::XMLElement *types = root->FirstChildElement(WINDOW_CONFIG_TAG_TYPES);
			if (types)
				TypesFromConfig(types);

			tinyxml2::XMLElement *window = root->FirstChildElement(WINDOW_CONFIG_TAG_WINDOW);
			if (window)
			{
				s_reopenable = window->BoolAttribute(WINDOW_CONFIG_ATTRIBUTE_REOPENABLE);
				tinyxml2::XMLElement *on;
				tinyxml2::XMLElement *till;
				tinyxml2::XMLElement *profile;

				if (on = window->FirstChildElement(WINDOW_CONFIG_TAG_ON))
				{
					if (!EventFromConfig(on, &s_on))
					{
						return FALSE;
					}
					till = on->NextSiblingElement(WINDOW_CONFIG_TAG_TILL);
				}
				else
				{
					s_on = NULL;
					ALLERIA_WriteMessage(ALLERIA_REPORT_ON_EVENT_TRIGGERED);
					till = window->FirstChildElement(WINDOW_CONFIG_TAG_TILL);
				}

				if (till)
				{
					if (!EventFromConfig(till, &s_till))
					{
						return FALSE;
					}
					profile = till->NextSiblingElement(WINDOW_CONFIG_TAG_PROFILE);
				}
				else
				{
					s_till = NULL;
					if (s_on == NULL)
					{
						profile = window->FirstChildElement(WINDOW_CONFIG_TAG_PROFILE);
					}
					else
					{
						profile = on->NextSiblingElement(WINDOW_CONFIG_TAG_PROFILE);
					}
				}

				if (!profile)
				{
					AlleriaSetLastError(ALLERIA_ERROR_CONFIG_PROFILE_DOES_NOT_EXIST);
				}
				else
				{
					ProfileFromConfig(profile);
				}

				if (s_reopenable && s_on == NULL)
				{
					AlleriaSetLastError(ALLERIA_ERROR_CONFIG_ON_REQUIRED_WITH_REOPENABLE);
				}
			}
			else
			{
				s_on = NULL;
				s_till = NULL;
				s_any.valid = FALSE;
				s_reopenable = FALSE;
			}
		}
		else
		{
			AlleriaSetLastError(ALLERIA_ERROR_CONFIG_UNSUPPORTED_VERSION);
		}
	}
	else
	{
		AlleriaSetLastError(ALLERIA_ERROR_CONFIG_INVALID_FILE);
	}

	return AlleriaGetLastError() == ALLERIA_ERROR_SUCCESS;
}

VOID WINDOW_CONFIG::InitDefault()
{
	// Enable all features all the time.
	s_any.valid = TRUE;
	s_any.any.valid = TRUE;
	s_any.any.any.valid = TRUE;
	s_any.any.any.any.valid = TRUE;

	//s_any.any.any.any.intercept.valid = FALSE;
	/*s_any.any.any.any.intercept.returnSize = 4;
	s_any.any.any.any.intercept.subReturn = NULL;
	ARG_DESC adesc;
	adesc.size = 4;
	adesc.subIn = NULL;
	adesc.ptrType = PTR_TYPE::Direct;
	adesc.subOut = NULL;
	s_any.any.any.any.intercept.args.push_back(adesc);
	adesc.size = 4;
	adesc.subIn = NULL;
	adesc.ptrType = PTR_TYPE::DoubleIndirect;
	adesc.subOut = NULL;
	s_any.any.any.any.intercept.args.push_back(adesc);*/

	s_any.any.any.any.anyMem.valid = TRUE;
	s_any.any.any.any.anyMem.id = INS_ID::AnyMem;
	s_any.any.any.any.anyMem.timeStamp = TRUE;
	s_any.any.any.any.anyMem.routineId = TRUE;
	s_any.any.any.any.anyMem.accessAddressAndSize = TRUE;
	s_any.any.any.any.anyMem.accessValue = TRUE;

	s_any.any.any.any.any.valid = FALSE;
}

BOOL WINDOW_CONFIG::IsFuncInteresting(std::string undecoratedName)
{
	std::map<std::string, std::pair<UINT64, UINT64> >::iterator it =
		s_interestingFuncs.find(undecoratedName);
	if (it != s_interestingFuncs.end())
	{
		// TODO: check image too.
		return TRUE;
	}

	return FALSE;
}

VOID WINDOW_CONFIG::ResetInterestingFuncs()
{
	for (std::map<std::string, std::pair<UINT64, UINT64> >::iterator it = s_interestingFuncs.begin();
		it != s_interestingFuncs.end(); ++it)
	{
		it->second.first = -2;
		it->second.second = 0;
	}
}

BOOL WINDOW_CONFIG::IsWindowOpen()
{
	return (s_on == NULL || (s_on != NULL && s_on->isTriggered)) && (s_till == NULL || !s_till->isTriggered);
}

WINDOW_CONFIG::FUNC_CONFIG* WINDOW_CONFIG::MatchInternalForIntercept2(THREAD_CONFIG *threadConfig, 
	RTN rtn, ADDRINT addr)
{
	ASSERTX(threadConfig->valid);

	FUNC_CONFIG *funcConfig = NULL;
	PIN_LockClient();
	IMG img = IMG_FindByAddress(addr);
	PIN_UnlockClient();

	IMAGE_CONFIG *imageConfig;
	if (IMG_Valid(img))
	{
		std::string imgPath = IMG_Name(img);
		StringToUpper(imgPath);
		std::string imgName;
		PathToName(imgPath, imgName);
		std::map<std::string, IMAGE_CONFIG>::iterator imageConfigIt =
			threadConfig->imageMap.find(imgName);
		imageConfig =
			(imageConfigIt != threadConfig->imageMap.end() && // If the image were found in the config
			(!imageConfigIt->second.pathValid || imageConfigIt->second.path == imgPath)) ? // and the matches the specified path
			&imageConfigIt->second : // then choose that image config.
			((threadConfig->anyButSys.valid && !IsSystemFile(imgPath.c_str())) ? // Otherwise, if it's not a system image and config is valid
				&threadConfig->anyButSys : // then choose that config.
				&threadConfig->any); // Otherwise, choose the any image config.
	}
	else
	{
		imageConfig = &threadConfig->any;
	}

	if (imageConfig->valid)
	{
		if (RTN_Valid(rtn))
		{
			const std::string rtnDecoratedName = RTN_Name(rtn);
			std::map<std::string, FUNC_CONFIG>::iterator rtnIt =
				imageConfig->decoratedMap.find(rtnDecoratedName);

			if (rtnIt != imageConfig->decoratedMap.end())
			{
				funcConfig = &rtnIt->second;
			}
			else
			{
				const std::string rtnUndecoratedName =
					PIN_UndecorateSymbolName(rtnDecoratedName, UNDECORATION::UNDECORATION_NAME_ONLY);
				rtnIt = imageConfig->undecoratedMap.find(rtnDecoratedName);
				if (rtnIt != imageConfig->undecoratedMap.end())
				{
					funcConfig = &rtnIt->second;
				}
			}
		}

		if (funcConfig == NULL || !funcConfig->valid)
		{
			funcConfig = &imageConfig->any;
		}
	}
	return funcConfig;
}

WINDOW_CONFIG::FUNC_CONFIG* WINDOW_CONFIG::MatchInternalForIntercept(RTN rtn, ADDRINT addr)
{
	// First, we should check in s_processMap
	// If not found check s_any.
	FUNC_CONFIG *funcConfig = NULL;
	PROCESS_CONFIG *processConfig = NULL;
	std::map<UINT32, PROCESS_CONFIG>::iterator it = s_processMap.find(s_processNumber);
	if (it != s_processMap.end())
		processConfig = &it->second;
	else
		processConfig = &s_any;
	if (processConfig != NULL && processConfig->valid)
	{
		for (std::map<PIN_THREAD_UID, THREAD_CONFIG>::iterator it = processConfig->threadMap.begin();
			it != processConfig->threadMap.end(); ++it)
		{
			THREAD_CONFIG *threadConfig = &it->second;

			if (threadConfig->valid)
			{
				funcConfig = MatchInternalForIntercept2(threadConfig, rtn, addr);
				if (funcConfig != NULL && funcConfig->valid)
					break;
			}
		}

		if (funcConfig == NULL || !funcConfig->valid)
		{
			THREAD_CONFIG *threadConfig = &processConfig->any;
			if (threadConfig->valid)
			{
				funcConfig = MatchInternalForIntercept2(threadConfig, rtn, addr);
			}
		}
	}
	return funcConfig;
}

WINDOW_CONFIG::FUNC_CONFIG* WINDOW_CONFIG::MatchInternal(RTN rtn, ADDRINT addr)
{
	// First, we should check in s_processMap
	// If not found check s_any.
	FUNC_CONFIG *funcConfig = NULL;
	PROCESS_CONFIG *processConfig = NULL;
	std::map<UINT32, PROCESS_CONFIG>::iterator it = s_processMap.find(s_processNumber);
	if (it != s_processMap.end())
		processConfig = &it->second;
	else
		processConfig = &s_any;
	if (processConfig != NULL && processConfig->valid)
	{
		PIN_THREAD_UID tuid = WINDOW_CONFIG::GetAppSerialId(PIN_ThreadUid());
		std::map<PIN_THREAD_UID, THREAD_CONFIG>::iterator it = processConfig->threadMap.find(tuid);
		THREAD_CONFIG *threadConfig = it != processConfig->threadMap.end() ?
			&it->second : &processConfig->any;
		if (threadConfig->valid)
		{
			PIN_LockClient();
			IMG img = IMG_FindByAddress(addr);
			PIN_UnlockClient();

			IMAGE_CONFIG *imageConfig;
			if (IMG_Valid(img))
			{
				std::string imgPath = IMG_Name(img);
				StringToUpper(imgPath);
				std::string imgName;
				PathToName(imgPath, imgName);
				std::map<std::string, IMAGE_CONFIG>::iterator imageConfigIt =
					threadConfig->imageMap.find(imgName);
				imageConfig =
					(imageConfigIt != threadConfig->imageMap.end() && // If the image were found in the config
					(!imageConfigIt->second.pathValid || imageConfigIt->second.path == imgPath)) ? // and the matches the specified path
					&imageConfigIt->second : // then choose that image config.
					((threadConfig->anyButSys.valid && !IsSystemFile(imgPath.c_str())) ? // Otherwise, if it's not a system image and config is valid
					&threadConfig->anyButSys : // then choose that config.
					&threadConfig->any); // Otherwise, choose the any image config.
			}
			else
			{
				imageConfig = &threadConfig->any;
			}

			if (imageConfig->valid)
			{
				if (RTN_Valid(rtn))
				{
					const std::string rtnDecoratedName = RTN_Name(rtn);
					std::map<std::string, FUNC_CONFIG>::iterator rtnIt =
						imageConfig->decoratedMap.find(rtnDecoratedName);

					if (rtnIt != imageConfig->decoratedMap.end())
					{
						funcConfig = &rtnIt->second;
					}
					else
					{
						const std::string rtnUndecoratedName =
							PIN_UndecorateSymbolName(rtnDecoratedName, UNDECORATION::UNDECORATION_NAME_ONLY);
						rtnIt = imageConfig->undecoratedMap.find(rtnDecoratedName);
						if (rtnIt != imageConfig->undecoratedMap.end())
						{
							funcConfig = &rtnIt->second;
						}
					}
				}

				if (funcConfig == NULL || !funcConfig->valid)
				{
					funcConfig = &imageConfig->any;
				}
			}
		}
	}
	return funcConfig;
}

xed_error_enum_t InsDecode(UINT8 *ip, xed_decoded_inst_t& xedd, size_t byteCount);

BOOL WINDOW_CONFIG::Match(INS ins, INS_CONFIG& insConfig)
{
	ASSERTX(INS_Valid(ins));
	insConfig.valid = FALSE;

	if (IsWindowOpen())
	{
		const FUNC_CONFIG *funcConfig = MatchInternal(INS_Rtn(ins), INS_Address(ins));
		if (funcConfig != NULL && funcConfig->valid)
		{
			xed_decoded_inst_t xedInst;
			if (InsDecode(reinterpret_cast<UINT8*>(INS_Address(ins)), xedInst, XED_MAX_INSTRUCTION_BYTES) ==
				xed_error_enum_t::XED_ERROR_NONE)
			{
				std::map<UINT32, INS_CONFIG>::const_iterator it;

				xed_iform_enum_t iform = xed_decoded_inst_get_iform_enum(&xedInst);
				it = funcConfig->formMap.find(iform);
				if (it != funcConfig->formMap.end())
				{
					insConfig = it->second;
				}
				else
				{
					xed_iclass_enum_t iclass = xed_decoded_inst_get_iclass(&xedInst);
					it = funcConfig->classMap.find(iclass);
					if (it != funcConfig->classMap.end())
					{
						insConfig = it->second;
					}
					else
					{
						xed_isa_set_enum_t isa_set = xed_decoded_inst_get_isa_set(&xedInst);
						it = funcConfig->isaSetMap.find(isa_set);
						if (it != funcConfig->isaSetMap.end())
						{
							insConfig = it->second;
						}
						else
						{
							xed_extension_enum_t extension = xed_decoded_inst_get_extension(&xedInst);
							it = funcConfig->extMap.find(extension);
							if (it != funcConfig->extMap.end())
							{
								insConfig = it->second;
							}
							else
							{
								xed_category_enum_t category = xed_decoded_inst_get_category(&xedInst);
								it = funcConfig->catMap.find(category);
								if (it != funcConfig->catMap.end())
								{
									insConfig = it->second;
								}
								else if (
									funcConfig->anyMem.valid && 
									(INS_IsMemoryRead(ins) || INS_IsMemoryWrite(ins))
									&& (
									INS_IsStandardMemop(ins) || 
									INS_Category(ins) == XED_CATEGORY_XSAVE || 
									INS_IsVgather(ins)))
									insConfig = funcConfig->anyMem;
								else
									insConfig = funcConfig->any;
							}
						}
					}
				}

				return insConfig.valid;
			}
		}
	}
	return FALSE;
}

WINDOW_CONFIG::INTERCEPT_CONFIG* WINDOW_CONFIG::Match(RTN rtn)
{
	// IsWindowOpen is called by the interceptor analysis routines.
	if (/*IsWindowOpen() &&*/ RTN_Valid(rtn))
	{
		FUNC_CONFIG *funcConfig = MatchInternalForIntercept(rtn, RTN_Address(rtn));
		if (funcConfig != NULL && funcConfig->valid)
		{
			return &funcConfig->intercept;
		}
	}
	return NULL;
}

VOID WINDOW_CONFIG::ResestEvent(EVENT *e)
{
	// Do not reset the counters.

	if (EVENT_ALL *eAll = static_cast<EVENT_ALL*>(e))
	{
		for (size_t i = 0; i < eAll->events.size(); ++i)
		{
			ResestEvent(eAll->events[i]);
		}
		eAll->isTriggered = FALSE;
	}
	else if (EVENT_ONE *eOne = static_cast<EVENT_ONE*>(e))
	{
		for (size_t i = 0; i < eOne->events.size(); ++i)
		{
			ResestEvent(eOne->events[i]);
		}
		eOne->isTriggered = FALSE;
	}
	else
		e->isTriggered = FALSE;
}

VOID WINDOW_CONFIG::ResestEvents()
{
	ResestEvent(s_on);
	ResestEvent(s_till);
}

BOOL WINDOW_CONFIG::IsItTimeToDetach()
{
	return s_till != NULL && s_till->isTriggered && !s_reopenable;
}

WINDOW_CONFIG::EVENT* WINDOW_CONFIG::GetEventToReportTo()
{
	if (s_till != NULL && s_till->isTriggered && s_reopenable)
	{
		// If s_reopenable is TRUE, s_on must be valid.
		ASSERTX(s_on != NULL);
		ResestEvents();
		return s_on;
	}

	else if (s_on != NULL && !s_on->isTriggered && (s_till == NULL || !s_till->isTriggered))
	{
		return s_on;
	}

	else if ((s_on == NULL || s_on->isTriggered) && s_till != NULL && !s_till->isTriggered)
	{
		return s_till;
	}

	return NULL;
}

// The Reporting lock must be acquired before calling this function.
BOOL WINDOW_CONFIG::ReportInternal(UINT64 tuid, EVENT *e, WINDOW_CONFIG::EVENT_TYPE eType, 
	INT32 pinObj, UINT64 arg1, UINT64 arg2)
{
	if (e->cat == WINDOW_CONFIG::EVENT_CATEGORY::InvalidCat)
		ASSERT(FALSE, "Window event invalid");
	if (eType == WINDOW_CONFIG::EVENT_TYPE::Invalid)
		ASSERT(FALSE, "Event invalid");

	// if eType requires pinObj, then pinObj must be valid.
	// if eType requires arg1, then arg1 must be valid.
	// if eType requires arg2, then arg2 must be valid.

	BOOL isTriggered = FALSE;

	if (e->cat == WINDOW_CONFIG::EVENT_CATEGORY::All)
	{
		EVENT_ALL *eAll = static_cast<EVENT_ALL*>(e);
		ASSERTX(!eAll->isTriggered);
		BOOL statisfied = TRUE;
		BOOL triggered = FALSE;
		for (size_t i = 0; i < eAll->events.size(); ++i)
		{
			if (!triggered)
				triggered = ReportInternal(tuid, eAll->events[i], eType, pinObj, arg1, arg2);
			statisfied = statisfied && eAll->events[i]->isTriggered;
		}
		if (statisfied)
		{
			eAll->isTriggered = TRUE;
			isTriggered = TRUE;
		}
	}
	else if (e->cat == WINDOW_CONFIG::EVENT_CATEGORY::One)
	{
		EVENT_ONE *eOne = static_cast<EVENT_ONE*>(e);
		ASSERTX(!eOne->isTriggered);
		for (size_t i = 0; i < eOne->events.size(); ++i)
		{
			ASSERTX(!eOne->events[i]->isTriggered);
			if (ReportInternal(tuid, eOne->events[i], eType, pinObj, arg1, arg2))
			{
				eOne->isTriggered = TRUE;
				isTriggered = TRUE;
			}
		}
	}
	else
	{
		ASSERTX(e->cat != WINDOW_CONFIG::EVENT_CATEGORY::InvalidCat);
		ASSERTX(e->cat != WINDOW_CONFIG::EVENT_CATEGORY::All);
		ASSERTX(e->cat != WINDOW_CONFIG::EVENT_CATEGORY::One);

		EVENT_CATEGORY tempCat = e->cat == EVENT_CATEGORY::CountThread ? 
			EVENT_CATEGORY::ThreadCreationTermination : e->cat;
		if ((tempCat | eType) != tempCat)
			return isTriggered;

		switch (e->cat)
		{
		case EVENT_CATEGORY::ThreadCreationTermination:
		{
			ASSERTX(eType == EVENT_TYPE::ThreadCreation || eType == EVENT_TYPE::ThreadTermination);
			EVENT_THREAD *eThread = static_cast<EVENT_THREAD*>(e);
			if (eThread->processId == 0 || eThread->processId == s_processNumber)
			{
				if ((eThread->threadId == 0 || eThread->threadId == tuid) &&
					((eThread->creation && eType == EVENT_TYPE::ThreadCreation) ||
					(!eThread->creation && eType == EVENT_TYPE::ThreadTermination)))
				{
					eThread->isTriggered = TRUE;
					isTriggered = TRUE;
				}
			}
			break;
		}
		case EVENT_CATEGORY::CountThread:
		{
			ASSERTX(eType == EVENT_TYPE::ThreadCreation || eType == EVENT_TYPE::ThreadTermination);
			EVENT_COUNT_THREAD *eCountThread = static_cast<EVENT_COUNT_THREAD*>(e);
			if (eCountThread->processId == 0 || eCountThread->processId == s_processNumber)
			{
				ASSERTX(eCountThread->smallerThanOrEqual || eCountThread->largerThanOrEqual);
				BOOL statisfied = TRUE;
				UINT64 count = ATOMIC::OPS::Load(&WINDOW_CONFIG::EVENT_COUNT_THREAD::currentCount);
				if (eCountThread->smallerThanOrEqual)
				{
					statisfied = statisfied && (count <= eCountThread->smallerThanOrEqual);
				}
				if (eCountThread->largerThanOrEqual)
				{
					statisfied = statisfied && (count >= eCountThread->largerThanOrEqual);
				}
				if (statisfied)
				{
					eCountThread->isTriggered = TRUE;
					isTriggered = TRUE;
				}
			}
			break;
		}
		case EVENT_CATEGORY::ImageLoadUnload:
		{
			ASSERTX(eType == EVENT_TYPE::ImageLoad || eType == EVENT_TYPE::ImageUnload);
			EVENT_IMAGE *eImage = static_cast<EVENT_IMAGE*>(e);
			if (eImage->processId == 0 || eImage->processId == s_processNumber)
			{
				if ((eImage->load && eType == EVENT_TYPE::ImageLoad) ||
					(!eImage->load && eType == EVENT_TYPE::ImageUnload))
				{
					IMG img;
					img.q_set(pinObj);
					std::string match;
					std::string imgPath = IMG_Name(img);
					StringToUpper(imgPath);
					if (eImage->isPath)
					{
						match = imgPath;
					}
					else
						PathToName(imgPath, match);
					if (eImage->pathOrName == match)
					{
						eImage->isTriggered = TRUE;
						isTriggered = TRUE;
					}
				}
			}
			break;
		}
		case EVENT_CATEGORY::FunctionCallReturn:
		{
			ASSERTX(eType == EVENT_TYPE::FunctionCall || eType == EVENT_TYPE::FunctionReturn);
			EVENT_FUNC *eFunc = static_cast<EVENT_FUNC*>(e);

			if (eFunc->processId == 0 || eFunc->processId == s_processNumber)
			{
				ASSERTX(eFunc->threadId == 0 || (eFunc->threadId != 0 && eFunc->threshold == 0));

				if ((eFunc->call && eType == EVENT_TYPE::FunctionCall) ||
					(!eFunc->call && eType == EVENT_TYPE::FunctionReturn))
				{
					RTN rtn;
					rtn.q_set(pinObj);

					PIN_LockClient();
					IMG img = IMG_FindByAddress(RTN_Address(rtn));
					PIN_UnlockClient();
					if (IMG_Valid(img))
					{
						std::string match;
						std::string imgPath = IMG_Name(img);
						StringToUpper(imgPath);
						if (eFunc->isPath)
						{
							match = imgPath;
						}
						else
							PathToName(imgPath, match);

						if (eFunc->imagePathOrName == match)
						{
							if ((eFunc->threadId == 0 || eFunc->threadId == tuid) && eFunc->threshold == 0)
							{
								std::string match = RTN_Name(rtn);
								if (!eFunc->isDecorated)
									match = PIN_UndecorateSymbolName(match, UNDECORATION::UNDECORATION_NAME_ONLY);

								if (eFunc->name == match)
								{
									eFunc->isTriggered = TRUE;
									isTriggered = TRUE;
								}
							}
							else if (eFunc->threadId == 0 && eFunc->threshold != 0)
							{
								std::map<std::string, std::pair<UINT64, UINT64> >::iterator it =
									s_interestingFuncs.find(
										PIN_UndecorateSymbolName(RTN_Name(rtn), UNDECORATION::UNDECORATION_NAME_ONLY));

								UINT64 count = eType == EVENT_TYPE::FunctionCall ?
									ATOMIC::OPS::Load(&it->second.first) : ATOMIC::OPS::Load(&it->second.second);

								if (eFunc->threshold <= count)
								{
									eFunc->isTriggered = TRUE;
									isTriggered = TRUE;
								}
							}
						}
					}
				}
			}
			break;
		}
		case EVENT_CATEGORY::CountAccessCat:
		{
			ASSERTX(eType == EVENT_TYPE::CountAccess);
			EVENT_COUNT_ACCESS *eAccess = static_cast<EVENT_COUNT_ACCESS*>(e);

			if (eAccess->processId == 0 || eAccess->processId == s_processNumber)
			{
				UINT64 reads;
				UINT64 writes;
				BOOL check = FALSE;
				if ((eAccess->threadId == 0 && eAccess->any) || eAccess->threadId == tuid)
				{
					reads = arg1;
					writes = arg2;
					check = TRUE;
				}
				else if (eAccess->threadId == 0 && !eAccess->any)
				{
					reads = ATOMIC::OPS::Load(&WINDOW_CONFIG::EVENT_COUNT_ACCESS::currentReadCountAllThreads);
					writes = ATOMIC::OPS::Load(&WINDOW_CONFIG::EVENT_COUNT_ACCESS::currentWriteCountAllThreads);
					check = TRUE;
				}

				if (check)
				{
					ASSERTX(eAccess->accessType != ACCESS_TYPE::Invalid);
					BOOL statisfied = FALSE;
					switch (eAccess->accessType)
					{
					case ACCESS_TYPE::Detail:
						statisfied = reads >= eAccess->reads && writes >= eAccess->writes;
						break;
					case ACCESS_TYPE::Total:
						statisfied = (reads + writes) >= eAccess->reads;
						break;
					default:
						ASSERT(FALSE, "Unkown ACCESS_TYPE");
						break;
					}
					if (statisfied)
					{
						eAccess->isTriggered = TRUE;
						isTriggered = TRUE;
					}
				}
			}
			break;
		}
		case EVENT_CATEGORY::CountInstructionCat:
		{
			ASSERTX(eType == EVENT_TYPE::CountInstruction);
			EVENT_INSTRUCTION *eIns = static_cast<EVENT_INSTRUCTION*>(e);

			if (eIns->processId == 0 || eIns->processId == s_processNumber)
			{
				UINT64 count;
				BOOL check = FALSE;
				if ((eIns->threadId == 0 && eIns->any) || eIns->threadId == tuid)
				{
					count = arg1;
					check = TRUE;
				}
				else if (eIns->threadId == 0 && !eIns->any)
				{
					count = ATOMIC::OPS::Load(&WINDOW_CONFIG::EVENT_INSTRUCTION::currentCountAllThreads);
					check = TRUE;
				}

				if (check && count >= eIns->threshold)
				{
					eIns->isTriggered = TRUE;
					isTriggered = TRUE;
				}
			}
			break;
		}
		case EVENT_CATEGORY::TimeCat:
		{
			ASSERTX(eType == EVENT_TYPE::Time);
			EVENT_TIME *eTime = static_cast<EVENT_TIME*>(e);

			if (eTime->processId == 0 || eTime->processId == s_processNumber)
			{
				if ((eTime->threadId == tuid || eTime->threadId == 0) && arg1 >= eTime->thresholdMilliseconds)
				{
					eTime->isTriggered = TRUE;
					isTriggered = TRUE;
				}
			}
			break;
		}
		default:
			ASSERT(FALSE, "Unkown event category.");
			break;
		}
	}

	if (isTriggered)
	{
		if (e == s_on)
		{
			ALLERIA_WriteMessage(ALLERIA_REPORT_ON_EVENT_TRIGGERED);
		}
		else
		{
			// e == s_till.
			ALLERIA_WriteMessage(ALLERIA_REPORT_TILL_EVENT_TRIGGERED);

			//if (GetEventToReportTo())
			//{
				// Order matters.
				PIN_RemoveInstrumentation();
				ResetInterestingFuncs();
				IMAGE_COLLECTION::ReprocessRoutines();
			//}
			//else
			//{
				// Pin will not call threadFini and Fini callbacks when detaching.
				// It's not necessary anyway.
			    // PIN_Detach();
			//}
		}
	}

	return isTriggered;
}

VOID WINDOW_CONFIG::ReportThreadCeation()
{
	PIN_GetLock(&s_reportingLock, PIN_ThreadId());

	//PIN_GetLock(&s_tidsLock, PIN_ThreadId());
	s_tids.insert(std::make_pair(PIN_ThreadId(), PIN_ThreadUid()));
	//PIN_ReleaseLock(&s_tidsLock);

	PIN_GetLock(&s_tuidMapLock, PIN_ThreadId());
	s_tuidMap.insert(std::make_pair(PIN_ThreadUid(), s_tuidMap.size() + 1));
	PIN_ReleaseLock(&s_tuidMapLock);

	ATOMIC::OPS::Increment(&WINDOW_CONFIG::EVENT_COUNT_THREAD::currentCount, (UINT64)1);
	
	EVENT *e = GetEventToReportTo();
	if (e != NULL)
		ReportInternal(GetAppSerialId(PIN_ThreadUid()), e, WINDOW_CONFIG::EVENT_TYPE::ThreadCreation, 0, 0, 0);

	PIN_ReleaseLock(&s_reportingLock);
}

VOID WINDOW_CONFIG::ReportThreadTermination(THREADID tid)
{
	ATOMIC::OPS::Increment(&WINDOW_CONFIG::EVENT_COUNT_THREAD::currentCount, (UINT64)-1);

	PIN_GetLock(&s_reportingLock, PIN_ThreadId());

	EVENT *e = GetEventToReportTo();
	if (e != NULL)
	{
		PIN_THREAD_UID tuid;
		//PIN_GetLock(&s_tidsLock, PIN_ThreadId());
		tuid = s_tids[tid];
		//PIN_ReleaseLock(&s_tidsLock);

		ReportInternal(GetAppSerialId(tuid), e, WINDOW_CONFIG::EVENT_TYPE::ThreadTermination, 0, 0, 0);
	}

	//PIN_GetLock(&s_tidsLock, PIN_ThreadId());
	s_tids.erase(tid);
	//PIN_ReleaseLock(&s_tidsLock);

	PIN_ReleaseLock(&s_reportingLock);
}

VOID WINDOW_CONFIG::ReportImageLoad(IMG image)
{
	if (IMG_Valid(image))
	{
		PIN_GetLock(&s_reportingLock, PIN_ThreadId());

		EVENT *e = GetEventToReportTo();
		if (e != NULL)
			ReportInternal(0 /* Not Used */, e, WINDOW_CONFIG::EVENT_TYPE::ImageLoad, image.q(), 0, 0);

		PIN_ReleaseLock(&s_reportingLock);
	}
}

VOID WINDOW_CONFIG::ReportImageUnload(IMG image)
{
	if (IMG_Valid(image))
	{
		PIN_GetLock(&s_reportingLock, PIN_ThreadId());

		EVENT *e = GetEventToReportTo();
		if (e != NULL)
			ReportInternal(0 /* Not Used */, e, WINDOW_CONFIG::EVENT_TYPE::ImageUnload, image.q(), 0, 0);

		PIN_ReleaseLock(&s_reportingLock);
	}
}

VOID WINDOW_CONFIG::ReportFunctionCall(RTN routine)
{
	if (RTN_Valid(routine))
	{
		std::map<std::string, std::pair<UINT64, UINT64> >::iterator it = 
			s_interestingFuncs.find(PIN_UndecorateSymbolName(RTN_Name(routine), UNDECORATION::UNDECORATION_NAME_ONLY));
		if (it != s_interestingFuncs.end())
		{
			ATOMIC::OPS::Increment(&(it->second.first), (UINT64)1);
		}

		PIN_GetLock(&s_reportingLock, PIN_ThreadId());

		EVENT *e = GetEventToReportTo();
		if (e != NULL)
			ReportInternal(GetAppSerialId(PIN_ThreadUid()), e, WINDOW_CONFIG::EVENT_TYPE::FunctionCall, routine.q(), 0, 0);

		PIN_ReleaseLock(&s_reportingLock);
	}
}

VOID WINDOW_CONFIG::ReportFunctionReturn(RTN routine)
{
	if (RTN_Valid(routine))
	{
		std::map<std::string, std::pair<UINT64, UINT64> >::iterator it =
			s_interestingFuncs.find(PIN_UndecorateSymbolName(RTN_Name(routine), UNDECORATION::UNDECORATION_NAME_ONLY));
		if (it != s_interestingFuncs.end())
		{
			ATOMIC::OPS::Increment(&(it->second.second), (UINT64)1);
		}

		PIN_GetLock(&s_reportingLock, PIN_ThreadId());

		EVENT *e = GetEventToReportTo();
		if (e != NULL)
			ReportInternal(GetAppSerialId(PIN_ThreadUid()), e, WINDOW_CONFIG::EVENT_TYPE::FunctionReturn, routine.q(), 0, 0);

		PIN_ReleaseLock(&s_reportingLock);
	}
}

VOID WINDOW_CONFIG::ReportAccesses(PIN_THREAD_UID tuid, UINT64 reads, UINT64 writes)
{
	ATOMIC::OPS::Increment(&EVENT_COUNT_ACCESS::currentReadCountAllThreads, reads);
	ATOMIC::OPS::Increment(&EVENT_COUNT_ACCESS::currentWriteCountAllThreads, writes);

	PIN_GetLock(&s_reportingLock, PIN_ThreadId());

	EVENT *e = GetEventToReportTo();
	if (e != NULL)
		ReportInternal(GetAppSerialId(tuid), e, WINDOW_CONFIG::EVENT_TYPE::CountAccess, 0, reads, writes);

	PIN_ReleaseLock(&s_reportingLock);
}

VOID WINDOW_CONFIG::ReportInstructions(PIN_THREAD_UID tuid, UINT64 count)
{
	ATOMIC::OPS::Increment(&EVENT_INSTRUCTION::currentCountAllThreads, count);

	PIN_GetLock(&s_reportingLock, PIN_ThreadId());

	EVENT *e = GetEventToReportTo();
	if (e != NULL)
		ReportInternal(GetAppSerialId(tuid), e, WINDOW_CONFIG::EVENT_TYPE::CountInstruction, 0, count, 0);

	PIN_ReleaseLock(&s_reportingLock);
}

VOID WINDOW_CONFIG::ReportTimestamp(TLS_KEY startingTimeHigh, TLS_KEY startingTimeLow, TIME_STAMP timestamp)
{
	PIN_GetLock(&s_reportingLock, PIN_ThreadId());

	EVENT *e = GetEventToReportTo();
	if (e != NULL)
	{
		//PIN_GetLock(&s_tidsLock, PIN_ThreadId());

		TIME_STAMP startingTime;
		for (std::map<THREADID, PIN_THREAD_UID>::iterator it = s_tids.begin(); it != s_tids.end(); ++it)
		{
#pragma DISABLE_COMPILER_WARNING_PTRTRUNC_LOSS
			startingTime = reinterpret_cast<UINT32>(PIN_GetThreadData(startingTimeLow, it->first));
#pragma RESTORE_COMPILER_WARNING_PTRTRUNC_LOSS
			startingTime = startingTime | (reinterpret_cast<UINT64>(PIN_GetThreadData(startingTimeHigh, it->first)) << 32);
			TIME_STAMP elapsedTime;
			GetTimeSpan(startingTime, timestamp, TIME_UNIT::Milliseconds, elapsedTime);
			ReportInternal(GetAppSerialId(it->second), e, WINDOW_CONFIG::EVENT_TYPE::Time, 0, elapsedTime, 0);
		}

		//PIN_ReleaseLock(&s_tidsLock);
	}

	PIN_ReleaseLock(&s_reportingLock);
}

const WINDOW_CONFIG::INTERCEPT_CUSTOM_TYPE& WINDOW_CONFIG::GetCustomType(UINT32 typeIndex)
{
	return s_customTypes[typeIndex];
}
const WINDOW_CONFIG::INTERCEPT_TYPE_POINTER& WINDOW_CONFIG::GetPointerType(UINT32 typeIndex)
{
	return s_pointerTypes[typeIndex];
}

UINT32 WINDOW_CONFIG::GetCustomTypeCount()
{
	return static_cast<UINT32>(s_customTypes.size());
}

UINT32 WINDOW_CONFIG::GetPointerTypeCount()
{
	return static_cast<UINT32>(s_pointerTypes.size());
}

WINDOW_CONFIG::EVENT_ALL::EVENT_ALL()
{
	cat = EVENT_CATEGORY::All;
}

WINDOW_CONFIG::EVENT_ONE::EVENT_ONE()
{
	cat = EVENT_CATEGORY::One;
}

WINDOW_CONFIG::EVENT_TIME::EVENT_TIME()
{
	cat = EVENT_CATEGORY::TimeCat;
}

WINDOW_CONFIG::EVENT_INSTRUCTION::EVENT_INSTRUCTION()
{
	cat = EVENT_CATEGORY::CountInstructionCat;
}

WINDOW_CONFIG::EVENT_COUNT_ACCESS::EVENT_COUNT_ACCESS()
{
	cat = EVENT_CATEGORY::CountAccessCat;
}

WINDOW_CONFIG::EVENT_COUNT_THREAD::EVENT_COUNT_THREAD()
{
	cat = EVENT_CATEGORY::CountThread;
}

WINDOW_CONFIG::EVENT_FUNC::EVENT_FUNC()
{
	cat = EVENT_CATEGORY::FunctionCallReturn;
}

WINDOW_CONFIG::EVENT_IMAGE::EVENT_IMAGE()
{
	cat = EVENT_CATEGORY::ImageLoadUnload;
}

WINDOW_CONFIG::EVENT_THREAD::EVENT_THREAD()
{
	cat = EVENT_CATEGORY::ThreadCreationTermination;
}
