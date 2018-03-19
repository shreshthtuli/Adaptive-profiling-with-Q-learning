/*

Declarations of Alleria Window Configuration files.

*/

/*

Events:
Process/Thread creation and destruction
Process creation and destruction
When the number of process/threads is smaller/larger/equal
Till/at N'th entry of image/function
Till/at N'th return from image/function
Till/after a specified number of (specific) (memory) instructions get executed
Till/after a specified number of reads/writes
At load/unload of images
Until/after a specified time

<AlleriaConfig version=1.0>
	<Types>
		<Type name=>
			<Field name= typeName= />
			<Field name= typeName= />
		</Type>
		<Pointer name= elementType= direction=inc/dec
			sentinel= countField= countParameter= count=
			startField= endField= 
			startParameter= endParameter= 
			endValid= />
	</Types>
	<Window reopenable=>
		<On>
			<All/One>
				<Creation process= thread= />
				<Termination process= thread= />
				<Load path or name with extension/>
				<Unload path or name with extension/>
				<Call process= thread= image function time />
				<Return process= thread= image function time />
				<Threads process= Smaller= Larger= />
				<Time threshold= />
				<Accesses Reads= Writes= or Total= />
				<Instructions threshold= />
			</All/One>
			...
		</On>
		<Till>
			<All/One>
				<Creation process= thread= />
				<Termination process= thread= />
				<Load path or name with extension/>
				<Unload path or name with extension/>
				<Call process= thread= image function threshold />
				<Return process= thread= image function threshold />
				<Threads process= Smaller= Larger= />
				<Time threshold= />
				<Accesses Reads= Writes= or Total= />
				<Instructions threshold= />
			</All/One>
			...
		</Till>
		<Profile>
			<Process n=>
				<Thread n=>
					<Image name or path>
						<Function name decorated>
							<Instruction cat/ext/class/form/isa timeStamp routineId accessAddress accessValueAndSize/>
							...
							<Intercept retType>
								<Parameter name index type in out>
								<Parameter name index type in out>
								<Parameter name index type in out>
							</Intercept>
						</Function>
						...
					</Image>
					...
				</Thread>
				...
			</Process>
			...
		</Profile>
	</Window>
</AlleriaConfig>

*/

#ifndef WINDOWCONFIG_H
#define WINDOWCONFIG_H

#include "pin.H"
#include <string>
#include <map> // Use unordered map in C++11
#include <vector>
#include "PortableApi.h"
extern "C" {
#pragma DISABLE_COMPILER_WARNING_INT2INT_LOSS
#include "xed-interface.h"
#pragma RESTORE_COMPILER_WARNING_INT2INT_LOSS
}
#pragma DISABLE_COMPILER_WARNING_DATA_LOSS
#include <atomic/ops.hpp>
#pragma RESTORE_COMPILER_WARNING_DATA_LOSS
#include "TinyXml2\tinyxml2.h"
#include "Error.h"
#include "ImageCollection.h"

class WINDOW_CONFIG
{
public:
	LOCALFUN BOOL Init(std::string configFilePath, UINT32 processNumber, std::string mainExePath);
	LOCALFUN VOID Destroy();
	LOCALFUN BOOL IsWindowOpen();
	LOCALFUN BOOL IsProcessInteresting(UINT32 processNumber);
	LOCALFUN UINT64 GetAppSerialId(PIN_THREAD_UID tuid);

	LOCALCONST CHAR *DefaultConfigFileName;
private:
	LOCALFUN VOID InitDefault();
	LOCALFUN BOOL InitFromConfig(tinyxml2::XMLDocument& xmlDoc);

	/* Types of the events */

	enum EVENT_TYPE
	{
		Invalid = 0x0,
		ThreadCreation = 0x1,
		ThreadTermination = 0x2,
		ImageLoad = 0x4,
		ImageUnload = 0x8,
		FunctionCall = 0x10,
		FunctionReturn = 0x20,
		CountAccess = 0x40,
		CountInstruction = 0x80,
		Time = 0x100
	};

	enum EVENT_CATEGORY
	{
		// For an occurring event to match an event description,
		// the fast check (EVENT_CATEGORY | EVENT_TYPE == EVENT_CATEGORY)
		// can be performed. CountThread, All, and One are special cases and can take any other values.

		InvalidCat = 0x0,
		ThreadCreationTermination = ThreadCreation + ThreadTermination,
		ImageLoadUnload = ImageLoad + ImageUnload,
		FunctionCallReturn = FunctionCall + FunctionReturn,
		CountAccessCat = 0x40,
		CountInstructionCat = 0x80,
		TimeCat = 0x100,
		CountThread,
		All,
		One
	};

	struct EVENT
	{
		BOOL isTriggered;
		EVENT_CATEGORY cat; // Pin CRT does not support RTTI.
		
		// Currently Pin does not support dynamic casts.
		//virtual VOID Dummy(){} // Just to make EVENT polymorphic so we can use dynamic_cast.
	};

	struct EVENT_ALL : EVENT
	{
		std::vector<EVENT*> events; // All must be valid.

		EVENT_ALL();
	};

	struct EVENT_ONE : EVENT
	{
		std::vector<EVENT*> events; // All must be valid.

		EVENT_ONE();
	};

	struct EVENT_THREAD : EVENT
	{
		PIN_THREAD_UID threadId; // Zero means any thread.
		UINT32 processId; // Cannot be zero.
		BOOL creation; // TRUE means that it's a creation event, FALSE means it's a termination event.

		EVENT_THREAD();
	};

	struct EVENT_IMAGE : EVENT
	{
		UINT32 processId;
		std::string pathOrName;
		BOOL isPath;
		BOOL load; // TRUE means that it's a load event, FALSE means it's an unload event.

		EVENT_IMAGE();
	};

	struct EVENT_FUNC : EVENT
	{
		std::string name;
		std::string imagePathOrName;
		UINT64 threshold; // The number of calls or returns required to trigger this event.
						  // Zero means invalid. If valid, threadId must be zero.
		UINT32 processId;
		PIN_THREAD_UID threadId; // Zero means any thread.
		BOOL isDecorated;
		BOOL call; // TRUE means that it's a call event, FALSE means it's a return event.
		BOOL isPath;

		EVENT_FUNC();
	};

	struct EVENT_COUNT_THREAD : EVENT
	{
		UINT32 processId; // Zero means any process.
		UINT64 smallerThanOrEqual; // Zero means invalid.
		UINT64 largerThanOrEqual; // Zero means invalid.
		
		// This field is used internally and not provided by the config file.
		// It's static because it must be shared between all threads and all instances.
		LOCALVAR UINT64 currentCount;

		EVENT_COUNT_THREAD();
	};

	enum class ACCESS_TYPE
	{
		Invalid,
		Detail,
		Total
	};

	struct EVENT_COUNT_ACCESS : EVENT
	{
		UINT32 processId; // Zero means any process.
		PIN_THREAD_UID threadId; // Zero means any/all thread.
		UINT64 reads; // if accessType is Total then this is the threshold
		UINT64 writes; // if accessType is Total then this is invalid
		ACCESS_TYPE accessType;
		BOOL any; // FALSE means all threads (use currentCountAllThreads).

		// This field is used internally and not provided by the config file.
		// It's static because it must be shared between all threads and all instances.
		LOCALVAR UINT64 currentReadCountAllThreads;

		// This field is used internally and not provided by the config file.
		// It's static because it must be shared between all threads and all instances.
		LOCALVAR UINT64 currentWriteCountAllThreads;

		EVENT_COUNT_ACCESS();
	};

	struct EVENT_INSTRUCTION : EVENT
	{
		UINT32 processId; // Zero means any process.
		PIN_THREAD_UID threadId; // Zero means any/all thread.
		UINT64 threshold; // Zero means invalid.
		BOOL any; // FALSE means all threads (use currentCountAllThreads).

		// This field is used internally and not provided by the config file.
		// It's static because it must be shared between all threads and all instances.
		LOCALVAR UINT64 currentCountAllThreads;

		EVENT_INSTRUCTION();
	};

	struct EVENT_TIME : EVENT
	{
		UINT32 processId; // Zero means any process.
		PIN_THREAD_UID threadId; // Zero means any thread.
		UINT64 thresholdMilliseconds; // Zero means invalid.

		EVENT_TIME();
	};

	/* End types of the events */

	/* Types of the Profile section */

public:
	enum class INS_ID
	{
		Invalid,
		Form, /* xed_iform_enum_t */
		Class, /* xed_iclass_enum_t */
		IsaSet, /* xed_isa_set_enum_t */
		Extension, /* xed_extension_enum_t */
		Category, /* xed_category_enum_t */
		AnyMem,
		Any
	};

	struct INS_CONFIG
	{
		BOOL valid;
		INS_ID id; // Cannot take the Invalid value.
		UINT32 value; // The value of the enumeration specified by id.
		BOOL timeStamp;
		BOOL routineId;
		BOOL accessAddressAndSize;
		BOOL accessValue;
	};

	//enum class PTR_TYPE
	//{
	//	Invalid,
	//	Direct,
	//	Indirect,  // Means that the arg is a pointer to an object whose size is size.
	//	DoubleIndirect, // Double pointer.
	//};

	enum INTERCEPT_TYPE_SPEC
	{
		INTERCEPT_TYPE_SPEC_INVALID,
		INTERCEPT_TYPE_SPEC_UINT1,
		INTERCEPT_TYPE_SPEC_UINT2,
		INTERCEPT_TYPE_SPEC_UINT3,
		INTERCEPT_TYPE_SPEC_UINT4,
		INTERCEPT_TYPE_SPEC_UINT5,
		INTERCEPT_TYPE_SPEC_UINT6,
		INTERCEPT_TYPE_SPEC_UINT7,
		INTERCEPT_TYPE_SPEC_UINT8,
		INTERCEPT_TYPE_SPEC_SINT1,
		INTERCEPT_TYPE_SPEC_SINT2,
		INTERCEPT_TYPE_SPEC_SINT3,
		INTERCEPT_TYPE_SPEC_SINT4,
		INTERCEPT_TYPE_SPEC_SINT5,
		INTERCEPT_TYPE_SPEC_SINT6,
		INTERCEPT_TYPE_SPEC_SINT7,
		INTERCEPT_TYPE_SPEC_SINT8,
		INTERCEPT_TYPE_SPEC_ASCII,
		INTERCEPT_TYPE_SPEC_UNICODE,
		INTERCEPT_TYPE_SPEC_CUSTOM,
		INTERCEPT_TYPE_SPEC_POINTER
	};

	LOCALCONST UINT32 INTERCEPT_TYPE_PRIMITIVE_COUNT = 18;

	enum INTERCEPT_PARAMETER_USAGE
	{
		INTERCEPT_PARAMETER_USAGE_INVALID,
		INTERCEPT_PARAMETER_USAGE_INPUT = 0x01,
		INTERCEPT_PARAMETER_USAGE_OUTPUT = 0x02
	};

	enum INTERCEPT_TYPE_POINTER_TYPE
	{
		INTERCEPT_TYPE_POINTER_TYPE_FIXED,
		INTERCEPT_TYPE_POINTER_TYPE_SENTINEL,
		INTERCEPT_TYPE_POINTER_TYPE_COUNT,

		// The pointer points to the end of the array.
		// Use endPointerStart to get the name of the variable that points to the start of the array.
		INTERCEPT_TYPE_POINTER_TYPE_START_POINTER,

		// The pointer points to the start of the array.
		// Use startPointerEnd to get the name of the variable that points to the end of the array.
		INTERCEPT_TYPE_POINTER_TYPE_END_POINTER,
	};

	enum INTERCEPT_TYPE_POINTER_DIRECTION
	{
		INTERCEPT_TYPE_POINTER_DIRECTION_INCREMENT,
		INTERCEPT_TYPE_POINTER_DIRECTION_DECREMENT
	};

	struct INTERCEPT_TYPE_FIELD
	{
		INTERCEPT_TYPE_SPEC typeSpec;
		std::string name;
		UINT32 typeIndex; // Valid only when typeSpec is INTERCEPT_CUSTOM_TYPE or INTERCEPT_TYPE_POINTER.

		// If typeSpec is INTERCEPT_TYPE_POINTER and the pointer type references a field. This stores the field index
		// in the field vector of the custom type. In this way, we don't have to compare strings to find the field,
		// reducing analysis overhead. Similarly when the pointer type references a parameter, it stores the parameter index
		// in the parameters vector (don't confuse it with the parameter index of the function being intercepted).
		UINT32 fieldOrParamIndex;
	};

	struct INTERCEPT_TYPE_POINTER
	{
		std::string name; // The name of the type as specified in the config.
		INTERCEPT_TYPE_POINTER_TYPE type;
		INTERCEPT_TYPE_FIELD elementType;
		union
		{
			struct
			{
				INTERCEPT_TYPE_POINTER_DIRECTION direction;
				size_t count;
			} fixed;
			struct
			{
				INTERCEPT_TYPE_POINTER_DIRECTION direction;
				UINT64 sentinelValue; // Support only sentinels of built-in types.
			} sentinel;
			struct
			{
				INTERCEPT_TYPE_POINTER_DIRECTION direction;
				char *name;
			} count;
			struct
			{
				char *endPointerName;
				BOOL endValid;
			} startPointerEnd;
			struct
			{
				char *startPointerName;
				BOOL endValid;
			} endPointerStart;
		};
	};

	struct INTERCEPT_CUSTOM_TYPE
	{
		std::string name; // Type name.
		std::vector<INTERCEPT_TYPE_FIELD> fields;
	};

	struct INTERCEPT_PARAMETER
	{
		UINT32 index;
		INTERCEPT_TYPE_FIELD parameter;
		INTERCEPT_PARAMETER_USAGE usage;
	};

	struct INTERCEPT_CONFIG
	{
		BOOL valid;
		BOOL intrumented;
		INTERCEPT_TYPE_FIELD returnType;
		std::vector<INTERCEPT_PARAMETER> parameters;
	};

private:
	LOCALCONST char *INS_CONFIG_STRINGS[];

	struct FUNC_CONFIG
	{
		BOOL valid;

		// Declared in the order of search.
		// Betterto use unordered map in C++11
		std::map<UINT32, INS_CONFIG> formMap;
		std::map<UINT32, INS_CONFIG> classMap;
		std::map<UINT32, INS_CONFIG> isaSetMap;
		std::map<UINT32, INS_CONFIG> extMap;
		std::map<UINT32, INS_CONFIG> catMap;
		INS_CONFIG anyMem;
		INS_CONFIG any;
		INTERCEPT_CONFIG intercept;
	};

	struct IMAGE_CONFIG
	{
		BOOL valid;
		BOOL pathValid; // TRUE if specified in the config file, otherwise FALSE.
		std::string path; // Valid only if specified in the config file.

		// Declared in the order of search.
		std::map<std::string, FUNC_CONFIG> decoratedMap;
		std::map<std::string, FUNC_CONFIG> undecoratedMap;
		
		FUNC_CONFIG any;
	};

	struct THREAD_CONFIG
	{
		BOOL valid;

		// The key represents the name of the image, including the extenion but excluding the directory.
		std::map<std::string, IMAGE_CONFIG> imageMap;
		
		IMAGE_CONFIG anyButSys;
		IMAGE_CONFIG any;
	};

	struct PROCESS_CONFIG
	{
		BOOL valid;
		// The key represents the order of creation of the thread.
		std::map<PIN_THREAD_UID, THREAD_CONFIG> threadMap;
		THREAD_CONFIG any;
	};

	/* End Profile types */
	
	/* Fields of events */

	LOCALVAR EVENT *s_on;
	LOCALVAR EVENT *s_till;

	/* End fields of events */

	/* Fields of the Profile section. */

	// The key represents the order of creation of the process.
	LOCALVAR std::map<UINT32, PROCESS_CONFIG> s_processMap;
	LOCALVAR PROCESS_CONFIG s_any;

	/* End fields of the Profile section. */

	/* Other fields */

	LOCALVAR BOOL s_reopenable;

	LOCALVAR std::map<THREADID, PIN_THREAD_UID> s_tids;
	//LOCALVAR PIN_LOCK s_tidsLock;

	// Maps PIN_THREAD_UID to serial identifiers for application threads only.
	LOCALVAR std::map<PIN_THREAD_UID, UINT64> s_tuidMap;
	LOCALVAR PIN_LOCK s_tuidMapLock;

	// All event reporting functions must be thread-safe.
	LOCALVAR PIN_LOCK s_reportingLock;

	// The keys are undecorated function names.
	// The values are pairs of (calls, returns) counters that occurred in all threads.
	// This is populated in the Init function and no changes are made after that.
	// Therefore, we don't need a lock.
	// Changes to the pairs are performed using atomic operations.
	LOCALVAR std::map<std::string, std::pair<UINT64, UINT64> > s_interestingFuncs;

	LOCALVAR UINT32 s_processNumber;
	LOCALVAR std::string s_mainExePath;

	// Types defined in the config used with interceptors.
	LOCALVAR std::vector<INTERCEPT_CUSTOM_TYPE> s_customTypes;
	LOCALVAR std::vector<INTERCEPT_TYPE_POINTER> s_pointerTypes;

	/* End */
public:
	// This function is not thread-safe.
	// Supposed to be called from instrumentation routines only.
	LOCALFUN BOOL IsFuncInteresting(std::string undecoratedName);

	LOCALFUN BOOL WINDOW_CONFIG::IsItTimeToDetach();

	LOCALFUN BOOL Match(INS ins, INS_CONFIG& insConfig);
	LOCALFUN INTERCEPT_CONFIG* Match(RTN rtn);

	LOCALFUN VOID ReportThreadCeation();
	LOCALFUN VOID ReportThreadTermination(THREADID tid);
	LOCALFUN VOID ReportImageLoad(IMG image);
	LOCALFUN VOID ReportImageUnload(IMG image);
	LOCALFUN VOID ReportFunctionCall(RTN routine);
	LOCALFUN VOID ReportFunctionReturn(RTN routine);
	LOCALFUN VOID ReportAccesses(PIN_THREAD_UID tuid, UINT64 reads, UINT64 writes);
	LOCALFUN VOID ReportInstructions(PIN_THREAD_UID tuid, UINT64 count);
	LOCALFUN VOID ReportTimestamp(TLS_KEY startingTimeHigh, TLS_KEY startingTimeLow, TIME_STAMP timestamp);

	LOCALFUN const INTERCEPT_CUSTOM_TYPE& GetCustomType(UINT32 typeIndex);
	LOCALFUN const INTERCEPT_TYPE_POINTER& GetPointerType(UINT32 typeIndex);
	LOCALFUN UINT32 GetCustomTypeCount();
	LOCALFUN UINT32 GetPointerTypeCount();

private:
	LOCALFUN BOOL EventFromConfig(tinyxml2::XMLElement *xmlEl, EVENT **e);
	LOCALFUN BOOL ProfileFromConfig(tinyxml2::XMLElement *xmlEl);
	LOCALFUN BOOL TypesFromConfig(tinyxml2::XMLElement *xmlEl);
	LOCALFUN FUNC_CONFIG*  MatchInternal(RTN rtn, ADDRINT addr);
	LOCALFUN FUNC_CONFIG* MatchInternalForIntercept(RTN rtn, ADDRINT addr);
	LOCALFUN FUNC_CONFIG* MatchInternalForIntercept2(THREAD_CONFIG *threadConfig,
		RTN rtn, ADDRINT addr);

	// The Reporting lock must be acquired before calling this function.
	LOCALFUN EVENT* GetEventToReportTo();
	
	// The Reporting lock must be acquired before calling this function.
	LOCALFUN BOOL ReportInternal(PIN_THREAD_UID tuid, EVENT *e, EVENT_TYPE eType, INT32 pinObj, UINT64 arg1, UINT64 arg2);
	
	LOCALFUN VOID ResestEvent(EVENT *e);
	LOCALFUN VOID ResestEvents();
	LOCALFUN UINT8* HexStrToBytes(size_t cbytes, const char *str);
	LOCALFUN INTERCEPT_TYPE_SPEC FindType(const char *typeName, UINT32& typeIndex);
	LOCALFUN BOOL IsTypeFieldValidForPointer(INTERCEPT_TYPE_POINTER& pointer, INTERCEPT_TYPE_FIELD& typeField);
	LOCALFUN BOOL IsElementTypeValidForPointer(INTERCEPT_TYPE_POINTER& pointer);
	LOCALFUN BOOL IsPointerTypeValidForPointer(INTERCEPT_TYPE_POINTER& pointer, BOOL nestedPointer);
	LOCALFUN BOOL IsTypeNameUnique(const char *typeName);

	LOCALFUN VOID ResetInterestingFuncs();
};

#endif /* WINDOWCONFIG_H */