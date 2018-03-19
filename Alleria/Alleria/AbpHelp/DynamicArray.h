#ifndef DYNAMIC_ARRAY_H
#define DYNAMIC_ARRAY_H

#define DYNAMIC_ARRAY_INITIAL_CAPACITY 2
typedef void* DYNAMIC_ARRAY_ELEMENT_TYPE;
typedef DYNAMIC_ARRAY_ELEMENT_TYPE* PDYNAMIC_ARRAY_ELEMENT_TYPE;
typedef PDYNAMIC_ARRAY_ELEMENT_TYPE* PPDYNAMIC_ARRAY_ELEMENT_TYPE;

typedef struct DYNAMIC_ARRAY_TAG {
	size_t size;
	size_t capacity;
	DYNAMIC_ARRAY_ELEMENT_TYPE *data;
} DYNAMIC_ARRAY, *PDYNAMIC_ARRAY;

void DynamicArrayInit(PDYNAMIC_ARRAY da);

void DynamicArrayAppend(PDYNAMIC_ARRAY da, PDYNAMIC_ARRAY_ELEMENT_TYPE value);

unsigned int DynamicArrayGet(PDYNAMIC_ARRAY da, size_t index, PDYNAMIC_ARRAY_ELEMENT_TYPE value);

void DynamicArraySet(PDYNAMIC_ARRAY da, size_t index, PDYNAMIC_ARRAY_ELEMENT_TYPE value);

void DynamicArrayExpand(PDYNAMIC_ARRAY da);

void DynamicArrayShrink(PDYNAMIC_ARRAY da);

void DynamicArrayFree(PDYNAMIC_ARRAY da);

size_t DynamicArrayGetSize(PDYNAMIC_ARRAY da);

size_t DynamicArrayGetCapacity(PDYNAMIC_ARRAY da);

PDYNAMIC_ARRAY_ELEMENT_TYPE DynamicArrayGetRawArray(PDYNAMIC_ARRAY da);

#endif /* DYNAMIC_ARRAY_H */