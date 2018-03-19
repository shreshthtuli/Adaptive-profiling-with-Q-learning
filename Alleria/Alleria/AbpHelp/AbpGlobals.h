#ifndef ABP_GLOBALS_H
#define ABP_GLOBALS_H

#include "AbpTypes.h"

#define INVALID_HANDLE_TABLE_INDEX -1

// The size of the profile handle array.
#define PROFILE_MAX            512

PPROFILE_INTERNALS g_profileHandles[PROFILE_MAX] = { (PPROFILE_INTERNALS)NULL };

// Holds the index of the next slot in g_profileHandles to examine for use.
PROFILE_HANDLE_TABLE_SIZE g_profileHandlesTop = 0;

#define ABP_VERSION_1_0 "1.0"
#define ABP_VERSION_1_0_SIZE 3
#define ABP_ID "HadiBrais"
#define ABP_ID_SIZE 9

#endif /* ABP_GLOBALS_H */