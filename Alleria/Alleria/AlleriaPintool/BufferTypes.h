#ifndef BUFFERTYPES_H
#define BUFFERTYPES_H

#include "pin.H"
#include "PortableApi.h"

enum BUFFER_ENTRY_TYPE : UINT8
{
	BUFFER_ENTRY_TYPE_INVALID,
	BUFFER_ENTRY_TYPE_IR0,
	BUFFER_ENTRY_TYPE_IR1,
	BUFFER_ENTRY_TYPE_IR2,
	BUFFER_ENTRY_TYPE_MR0,
	BUFFER_ENTRY_TYPE_MR1,
	BUFFER_ENTRY_TYPE_MR2,
	BUFFER_ENTRY_TYPE_PROCESSOR_RECORD,
	BUFFER_ENTRY_TYPE_SYS_CALL_RECORD,
	BUFFER_ENTRY_TYPE_BRANCH_RECORD,
	BUFFER_ENTRY_TYPE_VGATHER_0,
	BUFFER_ENTRY_TYPE_VGATHER_1,
	BUFFER_ENTRY_TYPE_XSAVE_0,
	BUFFER_ENTRY_TYPE_XSAVE_1,
	BUFFER_ENTRY_TYPE_HEADER,
};

struct INS_RECORD_0
{
	BUFFER_ENTRY_TYPE type;

	// The ID of the routine that contains this instruction.
	UINT32 routineId;

	// The address of the instruction. This can be used to get all information about the instruction.
	ADDRINT insAddr;

	// The time at which the instruction was about to get executed.
	TIME_STAMP timestamp;
};

struct INS_RECORD_1
{
	BUFFER_ENTRY_TYPE type;
	UINT32 routineId;
	ADDRINT insAddr;
};

struct INS_RECORD_2
{
	BUFFER_ENTRY_TYPE type;
	ADDRINT insAddr;
};

struct PROCESSOR_RECORD
{
	BUFFER_ENTRY_TYPE type;
	UINT32 processorPackage;
	UINT32 processorNumber;
};

struct BRANCH_RECORD
{
	// The type of the structure.
	BUFFER_ENTRY_TYPE type;

	BOOL taken;
	ADDRINT target;
};

struct SYS_CALL_RECORD
{
	// The type of the structure.
	BUFFER_ENTRY_TYPE type;

	BOOL errRetRecorded; // TRUE if err and retVal were recorded.
	ADDRINT err; // System call error number.
	UINT64 num; // System call number.
	UINT64 retVal; // System call return value.

	// System call args.
	// The system call that requires the largest number of arguments as far as I know
	// considering both Windows and Linux is the NtCreateNamedPipeFile Windows system call
	// which has 14 arguments. On Linux, it's 6.
	UINT64 arg0;
	UINT64 arg1;
	UINT64 arg2;
	UINT64 arg3;
	UINT64 arg4;
	UINT64 arg5;
	UINT64 arg6;
	UINT64 arg7;
	UINT64 arg8;
	UINT64 arg9;
	UINT64 arg10;
	UINT64 arg11;
	UINT64 arg12;
	UINT64 arg13;
	UINT64 arg14;
	UINT64 arg15;
};

struct MEM_REF_0
{
	// The type of the structure.
	BUFFER_ENTRY_TYPE type;

	// TRUE if a value is being read, FALSE if a value is being written.
	BOOL isRead;

	// TRUE if the value has been successfully recorded. FALSE, otherwise.
	BOOL isValueValid;

	// The number of bytes being read or written.
	UINT32 size;

	// The value being read or written.
	UINT64 value;

	// The virtual effective address of the memory reference.
	ADDRINT vea;
};

struct MEM_REF_1
{
	BUFFER_ENTRY_TYPE type;
	BOOL isRead;
	ADDRINT vea;
	UINT32 size;
};

struct MEM_REF_2
{
	BUFFER_ENTRY_TYPE type;
	BOOL isRead;
};

#endif /* BUFFERTYPES_H */