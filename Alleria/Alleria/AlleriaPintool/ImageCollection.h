#ifndef IMAGECOLLECTION_H
#define IMAGECOLLECTION_H

#include "pin.H"
#include <vector>
#include <string>
#include "PortableApi.h"

struct RTN_DESC
{
	ADDRINT address;
	UINT32 numIns;
	UINT32 imgId;
	UINT32 rtnId;
	UINT32 secIndex;
	USIZE size;
	USIZE range;
	const CHAR *name;
	BOOL isArtificial;
	BOOL isDynamic;
};

struct SEC_DESC
{
	ADDRINT address;
	USIZE size;
	string name;
	SEC_TYPE type;
	BOOL isMapped;
	BOOL isExecutable;
	BOOL isReadable;
	BOOL isWriteable;
};

struct REGION_DESC
{
	ADDRINT lowAddress;
	ADDRINT highAddress;
};

struct IMG_DESC
{
	ADDRINT lowAddress;
	ADDRINT highAddress;
	ADDRINT entryPointAddress;
	UINT32 numOfRegions;
	UINT32 id;
	USIZE sizeMapped; // The total size of the image, including sections that may not be mapped (loaded).
	string name;
	IMG_TYPE type;
	std::vector<SEC_DESC> sections;
	std::vector<REGION_DESC> regions;
	TIME_STAMP loadTime;
	TIME_STAMP unloadTime;
	BOOL isMainExecutable;
	BOOL isUnloaded;
};

class IMAGE_COLLECTION
{
public:
	LOCALFUN void Init();
	LOCALFUN void Dispose();
	LOCALFUN VOID AddImage(IMG img);
	LOCALFUN VOID UnloadImage(IMG img);
	LOCALFUN std::vector<IMG_DESC>::const_iterator CBegin();
	LOCALFUN std::vector<IMG_DESC>::const_iterator CEnd();
	LOCALFUN VOID GetImageAndRoutineNames(UINT32 routineId, string &imgName, string &rtnName);
	LOCALFUN UINT32 GetRoutineInfo(UINT32 routineId, string &rtnName);
	LOCALFUN VOID GetImageInfo(UINT32 imageId, string &imgName);
	LOCALFUN VOID ProcessRoutine(RTN routine, BOOL isReprocess);
	LOCALFUN VOID ReprocessRoutines();
private:

	LOCALVAR std::vector<IMG_DESC> s_images;
	LOCALVAR std::vector<RTN_DESC> s_routines;
	LOCALVAR PIN_LOCK s_routinesLock;
	LOCALVAR PIN_LOCK s_imagesLock;

	// Reprocessing data structures.
	LOCALVAR std::vector<RTN> s_reprocessRoutines;
	LOCALVAR PIN_LOCK s_reprocessLock;

	LOCALFUN VOID InsertReprocessRoutine(RTN rtn);
};

#endif /* IMAGECOLLECTION_H */