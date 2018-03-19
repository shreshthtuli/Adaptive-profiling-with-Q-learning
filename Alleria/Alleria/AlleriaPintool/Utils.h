#ifndef UTILS_H
#define UTILS_H

#include "pin.H"
#include "BufferTypes.h"
#include "ImageCollection.h"
#include "Error.h"
#include "PortableApi.h"
extern "C" {
#pragma DISABLE_COMPILER_WARNING_INT2INT_LOSS
#include "xed-interface.h"
#pragma RESTORE_COMPILER_WARNING_INT2INT_LOSS
}

std::string ShowN(UINT32 n, const VOID *ea);
xed_error_enum_t InsDecode(UINT8 *ip, xed_decoded_inst_t& xedd, size_t byteCount);
const std::wstring StringToWString(const std::string& s);

struct ALLERIA_MULTI_MEM_ACCESS_INFO
{
	UINT32 numberOfMemops;
	ADDRINT values[MAX_MULTI_MEMOPS];
	PIN_MEM_ACCESS_INFO memop[MAX_MULTI_MEMOPS];
};


struct ALLERIA_MULTI_MEM_ACCESS_ADDRS
{
	UINT32 numberOfMemops;
	ADDRINT base;
	ADDRINT addrs[MAX_MULTI_MEMOPS];
	UINT32 sizes[MAX_MULTI_MEMOPS];
	BOOL maskOn[MAX_MULTI_MEMOPS];
};

struct MEM_ACCESS_GENERAL_0
{
	ADDRINT         addr;
	UINT8           *value;
	PIN_MEMOP_ENUM  memopType;
	UINT32          bytesAccessed;
};

struct MEM_ACCESS_GENERAL_1
{
	ADDRINT         addr;
	PIN_MEMOP_ENUM  memopType;
	UINT32          bytesAccessed;
};

struct ALLERIA_XSAVE_MEM_ACCESS_ADDRS
{
	ADDRINT base;
	vector<MEM_ACCESS_GENERAL_1> accesses;
};

ALLERIA_MULTI_MEM_ACCESS_INFO *CopyMultiMemAccessInfo(PIN_MULTI_MEM_ACCESS_INFO *accesses, UINT32& size);
ALLERIA_MULTI_MEM_ACCESS_ADDRS *CopyMultiMemAccessAddrs(PIN_MULTI_MEM_ACCESS_INFO *accesses, UINT32& size);

std::string MultiMemAccessInfoStringAndDispose(ALLERIA_MULTI_MEM_ACCESS_INFO *accesses,
	UINT64 **physicalAddressesMemRef);
std::string MultiMemAccessAddrsStringAndDispose(ALLERIA_MULTI_MEM_ACCESS_ADDRS *addrs,
	UINT64 **physicalAddressesMemRef);

std::string MultiMemXsaveInfoStringAndDispose(std::vector<MEM_ACCESS_GENERAL_0> *accessInfo,
	UINT64 **physicalAddressesMemRef);
std::string MultiMemXsaveAddrsStringAndDispose(ALLERIA_XSAVE_MEM_ACCESS_ADDRS *accessInfo,
	UINT64 **physicalAddressesMemRef);

VOID WriteBinaryMultiMemAccessInfoAndDispose(std::ofstream& ofs, ALLERIA_MULTI_MEM_ACCESS_INFO *accesses,
	UINT64 **physicalAddressesMemRef);
VOID WriteBinaryMultiMemAccessAddrsAndDispose(std::ofstream& ofs, ALLERIA_MULTI_MEM_ACCESS_ADDRS *addrs,
	UINT64 **physicalAddressesMemRef);

VOID WriteBinaryXsaveInfoAndDispose(std::ofstream& ofs, std::vector<MEM_ACCESS_GENERAL_0> *accessInfo,
	UINT64 **physicalAddressesMemRef);
VOID WriteBinaryXsaveAddrsAndDispose(std::ofstream& ofs, ALLERIA_XSAVE_MEM_ACCESS_ADDRS *accessInfo,
	UINT64 **physicalAddressesMemRef);

struct MODULEENTRY32_SUMMARY
{
	unsigned char  *modBaseAddr;        // Base address of module in th32ProcessID's context
	char    szExePath[PORTABLE_MAX_PATH];
};

UINT32 BufferTypeToSize(BUFFER_ENTRY_TYPE type);

/*

Intel volume 1 section 13.1
All the state components corresponding to bits in the range 9:0 are user state components, except PT
state (corresponding to bit 8), which is a supervisor state component.

*/

// TODO: Check if any changes introduced by Skylake.
enum XSAVE_STATE_COMPONENT_MASK
{
	XSAVE_STATE_COMPONENT_MASK_X87 = 0x001,
	XSAVE_STATE_COMPONENT_MASK_SSE = 0x002,
	XSAVE_STATE_COMPONENT_MASK_AVX = 0x004,
	XSAVE_STATE_COMPONENT_MASK_MPX_1 = 0x008,
	XSAVE_STATE_COMPONENT_MASK_MPX_2 = 0x010,
	XSAVE_STATE_COMPONENT_MASK_AVX512_1 = 0x020,
	XSAVE_STATE_COMPONENT_MASK_AVX512_2 = 0x040,
	XSAVE_STATE_COMPONENT_MASK_AVX512_3 = 0x080,
	XSAVE_STATE_COMPONENT_MASK_PT = 0x100, // Ring 0 only.
	XSAVE_STATE_COMPONENT_MASK_PKRU = 0x200,

	XSAVE_STATE_COMPONENT_MASK_ALL =
	XSAVE_STATE_COMPONENT_MASK_X87 |
	XSAVE_STATE_COMPONENT_MASK_SSE |
	XSAVE_STATE_COMPONENT_MASK_AVX |
	XSAVE_STATE_COMPONENT_MASK_MPX_1 |
	XSAVE_STATE_COMPONENT_MASK_MPX_2 |
	XSAVE_STATE_COMPONENT_MASK_AVX512_1 |
	XSAVE_STATE_COMPONENT_MASK_AVX512_2 |
	XSAVE_STATE_COMPONENT_MASK_AVX512_3 |
	XSAVE_STATE_COMPONENT_MASK_PT |
	XSAVE_STATE_COMPONENT_MASK_PKRU
	// All other bits are invalid.
};

// TODO: Check if any changes introduced by Skylake.
enum XSAVE_STATE_SUBCOMPONENT
{
	XSAVE_STATE_COMPONENT_X87 = 0,
	XSAVE_STATE_COMPONENT_SSE,
	XSAVE_STATE_COMPONENT_AVX,
	XSAVE_STATE_COMPONENT_MPX_1,
	XSAVE_STATE_COMPONENT_MPX_2,
	XSAVE_STATE_COMPONENT_AVX512_1,
	XSAVE_STATE_COMPONENT_AVX512_2,
	XSAVE_STATE_COMPONENT_AVX512_3,
	XSAVE_STATE_COMPONENT_PT, // Ring 0 only.
	XSAVE_STATE_COMPONENT_PKRU,
};

// If recordVals is TRUE, returns vector<MEM_ACCESS_GENERAL_0>*. Otherwise, returns ALLERIA_XSAVE_MEM_ACCESS_ADDRS*.
void* GetXsaveAccessInfo(UINT8 *xsaveAreaBase, BOOL is32bit, UINT32& size, BOOL recordVals);

// If recordVals is TRUE, returns vector<MEM_ACCESS_GENERAL_0>*. Otherwise, returns ALLERIA_XSAVE_MEM_ACCESS_ADDRS*.
void* GetXsavecAccessInfo(UINT8 *xsaveAreaBase, BOOL is32bit, UINT32& size, BOOL recordVals);

// If recordVals is TRUE, returns vector<MEM_ACCESS_GENERAL_0>*. Otherwise, returns ALLERIA_XSAVE_MEM_ACCESS_ADDRS*.
void* GetXrstorAccessInfo(const CONTEXT *ctxt, UINT8 *xsaveAreaBase, BOOL is32bit, UINT32& size, BOOL recordVals);

const char* ContextChangeReasonString(CONTEXT_CHANGE_REASON reason);

#define PHYSICAL_ADDRESS_INVALID ((UINT64)-1)

#define WriteBinary(ofs, obj) \
ofs.write(reinterpret_cast<const char*>(&obj), sizeof(obj))

VOID WriteBinaryFuncTableIndex(std::ofstream& ofs, UINT32 index);

#endif /* UTILS_H */