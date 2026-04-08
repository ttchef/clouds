
#include "darray.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void *_darray_create(uint64_t length, uint64_t stride) {
    uint64_t headerSize = DARRAY_FIELD_LENGTH * sizeof(uint64_t);
    uint64_t elemetsSize = length * stride;
    uint64_t *array = malloc(headerSize + elemetsSize);
    memset(array, 0, headerSize + elemetsSize);
    array[DARRAY_CAPACITY] = length;
    array[DARRAY_LENGTH] = 0;
    array[DARRAY_STRIDE] = stride;
    return (void *)(array + DARRAY_FIELD_LENGTH);
}

void _darray_destroy(void *array) {
    uint64_t *header = (uint64_t *)array - DARRAY_FIELD_LENGTH;
    free(header);
}

uint64_t _darray_field_get(void *array, uint64_t field) {
    uint64_t *header = (uint64_t *)array - DARRAY_FIELD_LENGTH;
    return header[field];
}

void _darray_field_set(void *array, uint64_t field, uint64_t val) {
    uint64_t *header = (uint64_t *)array - DARRAY_FIELD_LENGTH;
    header[field] = val;
}

void *_darray_resize(void *array) {
    uint64_t length = darrayLength(array);
    uint64_t stride = darrayStride(array);
    void *temp =
        _darray_create(DARRAY_RESIZE_FACTOR * darrayCapacity(array), stride);
    memcpy(temp, array, length * stride);

    _darray_field_set(temp, DARRAY_LENGTH, length);
    _darray_destroy(array);
    return temp;
}

void *_darray_push(void *array, const void *valuePtr) {
    uint64_t length = darrayLength(array);
    uint64_t stride = darrayStride(array);

    if (length >= darrayCapacity(array)) {
        array = _darray_resize(array);
    }

    uint64_t addr = (uint64_t)array;
    addr += length * stride;
    memcpy((void *)addr, valuePtr, stride);
    _darray_field_set(array, DARRAY_LENGTH, length + 1);
    return array;
}

void _darray_pop(void *array, void *dest) {
    uint64_t length = darrayLength(array);
    uint64_t stride = darrayStride(array);

    uint64_t addr = (uint64_t)array;
    addr += (length - 1) * stride;
    if (dest)
        memcpy(dest, (void *)addr, stride);

    _darray_field_set(array, DARRAY_LENGTH, length - 1);
}

void *_darray_insert_at(void *array, uint64_t index, const void *valuePtr) {
    uint64_t length = darrayLength(array);
    uint64_t stride = darrayStride(array);

    if (index == length)
        return _darray_push(array, valuePtr);

    if (index >= length) {
        fprintf(stderr,
                "Index out of bounds for this array!\n Length: %lu, Index: %lu",
                length, index);
        return array;
    }

    if (length >= darrayCapacity(array)) {
        array = _darray_resize(array);
    }

    uint64_t addr = (uint64_t)array;

    if (index != length - 1) {
        memmove((void *)(addr + ((index + 1) * stride)),
                (void *)(addr + (index * stride)),
                stride * (length - index - 1));
    }
    memcpy((void *)(addr + (index * stride)), valuePtr, stride);

    _darray_field_set(array, DARRAY_LENGTH, length + 1);
    return array;
}

void *_darray_pop_at(void *array, uint64_t index, void *dest) {
    uint64_t length = darrayLength(array);
    uint64_t stride = darrayStride(array);

    if (index == length) {
        _darray_pop(array, dest);
        return array;
    }

    if (index >= length) {
        fprintf(stderr,
                "Index out of bounds for this array!\n Length: %lu, Index: %lu",
                length, index);
        return array;
    }

    uint64_t addr = (uint64_t)array;

    if (dest)
        memcpy(dest, (void *)(addr + (index * stride)), stride);

    if (index != length - 1) {
        memmove((void *)(addr + (index * stride)),
                (void *)(addr + ((index + 1) * stride)),
                stride * (length - index - 1));
    }

    _darray_field_set(array, DARRAY_LENGTH, length - 1);
    return array;
}
