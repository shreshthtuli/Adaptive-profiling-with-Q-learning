#include <stdlib.h>
#include "DynamicArray.h"

void DynamicArrayInit(PDYNAMIC_ARRAY da)
{
	da->size = 0;
	da->capacity = DYNAMIC_ARRAY_INITIAL_CAPACITY;
	da->data = malloc(sizeof(DYNAMIC_ARRAY_ELEMENT_TYPE) * da->capacity);
}

void DynamicArrayAppend(PDYNAMIC_ARRAY da, PDYNAMIC_ARRAY_ELEMENT_TYPE value) {
	DynamicArrayExpand(da);
	da->data[da->size++] = *value;
}

unsigned int DynamicArrayGet(PDYNAMIC_ARRAY da, size_t index, PDYNAMIC_ARRAY_ELEMENT_TYPE value)
{
	if (index >= da->size || index < 0) {
		return 0;
	}
	*value = da->data[index]; 
	return 1;
}

void DynamicArraySet(PDYNAMIC_ARRAY da, size_t index, PDYNAMIC_ARRAY_ELEMENT_TYPE value)
{
	// Zero fill the vector up to the desired index.
	while (index >= da->size) {
		DynamicArrayAppend(da, 0);
	}

	// Set the value at the desired index.
	da->data[index] = *value;
}

void DynamicArrayExpand(PDYNAMIC_ARRAY da) 
{
	if (da->size >= da->capacity) {
		da->capacity *= 2;
		da->data = realloc(da->data, sizeof(DYNAMIC_ARRAY_ELEMENT_TYPE) * da->capacity);
	}
}

void DynamicArrayShrink(PDYNAMIC_ARRAY da)
{
	if (da->size < da->capacity) {
		da->capacity = da->size;
		da->data = da->capacity == 0 ? NULL : realloc(da->data, sizeof(DYNAMIC_ARRAY_ELEMENT_TYPE) * da->capacity);
	}
}

void DynamicArrayFree(PDYNAMIC_ARRAY da)
{
	if (da != NULL && da->data != NULL)
		free(da->data);
}

size_t DynamicArrayGetSize(PDYNAMIC_ARRAY da)
{
	return da->size;
}

size_t DynamicArrayGetCapacity(PDYNAMIC_ARRAY da)
{
	return da->capacity;
}

PDYNAMIC_ARRAY_ELEMENT_TYPE DynamicArrayGetRawArray(PDYNAMIC_ARRAY da)
{
	return da->data;
}