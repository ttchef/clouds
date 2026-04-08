
#ifndef DARRAY_H
#define DARRAY_H

#include <stdint.h>

// Strongly inspired by kohi game engine implementation

/*
Mem Layout
u64 capacity = number of elements that can be held
u64 length = number of elements currently contained
u64 stride = size of each element in bytes
void* elements
*/

enum {
    DARRAY_CAPACITY,
    DARRAY_LENGTH,
    DARRAY_STRIDE,
    DARRAY_FIELD_LENGTH,
};

void *_darray_create(uint64_t length, uint64_t stride);
void _darray_destroy(void *array);

uint64_t _darray_field_get(void *array, uint64_t field);
void _darray_field_set(void *array, uint64_t field, uint64_t val);

void *_darray_resize(void *array);

void *_darray_push(void *array, const void *valuePtr);
void _darray_pop(void *array, void *dest);

void *_darray_insert_at(void *array, uint64_t index, const void *valuePtr);
void *_darray_pop_at(void *array, uint64_t index, void *dest);

#define DARRAY_DEFAULT_CAPACITY 1
#define DARRAY_RESIZE_FACTOR 2

#define darrayCreate(type) _darray_create(DARRAY_DEFAULT_CAPACITY, sizeof(type))

#define darrayReserve(type, capacity) _darray_create(capacity, sizeof(type))

#define darrayDestroy(array) _darray_destroy(array);

#define darrayPush(array, value)                                               \
    {                                                                          \
        typeof(value) temp = value;                                            \
        array = _darray_push(array, &temp);                                    \
    }

#define darrayPop(array, valuePtr) _darray_pop(array, valuePtr);

#define darrayInsertAt(array, index, value)                                    \
    {                                                                          \
        typeof(value) temp = value;                                            \
        array = _darray_insert_at(array, index, &temp);                        \
    }

#define darrayPopAt(array, index, valuePtr)                                    \
    _darray_pop_at(array, index, valuePtr)

#define darrayClear(array) _darray_field_set(array, DARRAY_LENGTH, 0)

#define darrayCapacity(array) _darray_field_get(array, DARRAY_CAPACITY)

#define darrayLength(array) _darray_field_get(array, DARRAY_LENGTH)

#define darrayStride(array) _darray_field_get(array, DARRAY_STRIDE)

#define darrayLengthSet(array, value)                                          \
    _darray_field_set(array, DARRAY_LENGTH, value)

#endif
