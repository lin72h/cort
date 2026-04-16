#include "FXCFInternal.h"

#include <CoreFoundation/CFArray.h>

enum {
    FX_CFARRAY_FLAG_MUTABLE = 1u
};

struct __CFArray {
    CFRuntimeBase _base;
    CFIndex count;
    CFIndex capacity;
    UInt32 flags;
    const void **values;
};

static const void *__CFTypeArrayRetain(CFAllocatorRef allocator, const void *value) {
    (void)allocator;
    return CFRetain((CFTypeRef)value);
}

static void __CFTypeArrayRelease(CFAllocatorRef allocator, const void *value) {
    (void)allocator;
    CFRelease((CFTypeRef)value);
}

static Boolean __CFTypeArrayEqual(const void *value1, const void *value2) {
    return CFEqual((CFTypeRef)value1, (CFTypeRef)value2);
}

const CFArrayCallBacks kCFTypeArrayCallBacks = {
    0,
    __CFTypeArrayRetain,
    __CFTypeArrayRelease,
    NULL,
    __CFTypeArrayEqual
};

static Boolean __CFArrayCallbacksAreSupported(const CFArrayCallBacks *callBacks) {
    return
        callBacks != NULL &&
        callBacks->version == 0 &&
        callBacks->retain == kCFTypeArrayCallBacks.retain &&
        callBacks->release == kCFTypeArrayCallBacks.release &&
        callBacks->copyDescription == kCFTypeArrayCallBacks.copyDescription &&
        callBacks->equal == kCFTypeArrayCallBacks.equal;
}

static void __CFArrayValidate(CFArrayRef theArray, const char *apiName) {
    _FXCFValidateType((CFTypeRef)theArray, _kCFRuntimeIDCFArray, apiName);
}

static void __CFArrayValidateMutable(CFMutableArrayRef theArray, const char *apiName) {
    struct __CFArray *array = (struct __CFArray *)theArray;
    __CFArrayValidate((CFArrayRef)array, apiName);
    if ((array->flags & FX_CFARRAY_FLAG_MUTABLE) == 0u) {
        _FXCFAbort(apiName);
    }
}

static Boolean __CFArrayCheckedAllocationSize(CFIndex capacity, size_t *sizeOut) {
    if (capacity < 0) {
        return false;
    }
    if ((uint64_t)capacity > (uint64_t)(SIZE_MAX / sizeof(const void *))) {
        return false;
    }
    *sizeOut = (size_t)capacity * sizeof(const void *);
    return true;
}

static Boolean __CFArrayEnsureCapacity(struct __CFArray *array, CFIndex requiredCount) {
    if (requiredCount <= array->capacity) {
        return true;
    }

    CFIndex newCapacity = array->capacity > 0 ? array->capacity : 4;
    while (newCapacity < requiredCount) {
        if (newCapacity > LONG_MAX / 2) {
            newCapacity = requiredCount;
            break;
        }
        newCapacity *= 2;
    }

    size_t newSize = 0;
    if (!__CFArrayCheckedAllocationSize(newCapacity, &newSize)) {
        return false;
    }

    CFAllocatorRef allocator = CFGetAllocator((CFTypeRef)array);
    const void **newValues = NULL;
    if (newSize > 0) {
        newValues = (const void **)CFAllocatorAllocate(allocator, (CFIndex)newSize, 0);
        if (newValues == NULL) {
            return false;
        }
        if (array->count > 0) {
            memcpy(newValues, array->values, (size_t)array->count * sizeof(const void *));
        }
    }

    if (array->values != NULL) {
        CFAllocatorDeallocate(allocator, (void *)array->values);
    }
    array->values = newValues;
    array->capacity = newCapacity;
    return true;
}

static void __CFArrayFinalize(CFTypeRef cf) {
    struct __CFArray *array = (struct __CFArray *)cf;
    for (CFIndex index = 0; index < array->count; ++index) {
        CFRelease((CFTypeRef)array->values[index]);
    }
    if (array->values != NULL) {
        CFAllocatorDeallocate(CFGetAllocator(cf), (void *)array->values);
    }
}

static const CFRuntimeClass __CFArrayClass = {
    0,
    "CFArray",
    NULL,
    NULL,
    __CFArrayFinalize,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    0
};

const CFRuntimeClass *_FXCFArrayClass(void) {
    return &__CFArrayClass;
}

CFTypeID CFArrayGetTypeID(void) {
    return _kCFRuntimeIDCFArray;
}

CFArrayRef CFArrayCreate(CFAllocatorRef allocator, const void **values, CFIndex numValues, const CFArrayCallBacks *callBacks) {
    if (numValues < 0) {
        return NULL;
    }
    if (numValues > 0 && values == NULL) {
        return NULL;
    }
    if (!__CFArrayCallbacksAreSupported(callBacks)) {
        return NULL;
    }

    struct __CFArray *array = (struct __CFArray *)_CFRuntimeCreateInstance(
        allocator,
        CFArrayGetTypeID(),
        (CFIndex)(sizeof(struct __CFArray) - sizeof(CFRuntimeBase)),
        NULL
    );
    if (array == NULL) {
        return NULL;
    }

    if (numValues > 0 && !__CFArrayEnsureCapacity(array, numValues)) {
        CFRelease((CFTypeRef)array);
        return NULL;
    }

    for (CFIndex index = 0; index < numValues; ++index) {
        if (values[index] == NULL) {
            CFRelease((CFTypeRef)array);
            return NULL;
        }
        array->values[index] = CFRetain((CFTypeRef)values[index]);
        array->count = index + 1;
    }

    return array;
}

CFArrayRef CFArrayCreateCopy(CFAllocatorRef allocator, CFArrayRef theArray) {
    __CFArrayValidate(theArray, "CFArrayCreateCopy called with non-array object");
    const struct __CFArray *array = (const struct __CFArray *)theArray;
    return CFArrayCreate(allocator, array->values, array->count, &kCFTypeArrayCallBacks);
}

CFMutableArrayRef CFArrayCreateMutable(CFAllocatorRef allocator, CFIndex capacity, const CFArrayCallBacks *callBacks) {
    if (capacity < 0) {
        return NULL;
    }
    if (!__CFArrayCallbacksAreSupported(callBacks)) {
        return NULL;
    }

    struct __CFArray *array = (struct __CFArray *)_CFRuntimeCreateInstance(
        allocator,
        CFArrayGetTypeID(),
        (CFIndex)(sizeof(struct __CFArray) - sizeof(CFRuntimeBase)),
        NULL
    );
    if (array == NULL) {
        return NULL;
    }

    array->flags = FX_CFARRAY_FLAG_MUTABLE;
    if (capacity > 0 && !__CFArrayEnsureCapacity(array, capacity)) {
        CFRelease((CFTypeRef)array);
        return NULL;
    }
    return array;
}

CFIndex CFArrayGetCount(CFArrayRef theArray) {
    __CFArrayValidate(theArray, "CFArrayGetCount called with non-array object");
    return ((const struct __CFArray *)theArray)->count;
}

const void *CFArrayGetValueAtIndex(CFArrayRef theArray, CFIndex idx) {
    __CFArrayValidate(theArray, "CFArrayGetValueAtIndex called with non-array object");
    const struct __CFArray *array = (const struct __CFArray *)theArray;
    if (idx < 0 || idx >= array->count) {
        _FXCFAbort("CFArrayGetValueAtIndex index out of range");
    }
    return array->values[idx];
}

void CFArrayAppendValue(CFMutableArrayRef theArray, const void *value) {
    struct __CFArray *array = (struct __CFArray *)theArray;
    __CFArrayValidateMutable(theArray, "CFArrayAppendValue called on immutable or non-array object");
    if (value == NULL) {
        _FXCFAbort("CFArrayAppendValue called with NULL value");
    }
    if (!__CFArrayEnsureCapacity(array, array->count + 1)) {
        _FXCFAbort("CFArrayAppendValue allocation failed");
    }
    array->values[array->count] = CFRetain((CFTypeRef)value);
    array->count += 1;
}

void CFArrayRemoveValueAtIndex(CFMutableArrayRef theArray, CFIndex idx) {
    struct __CFArray *array = (struct __CFArray *)theArray;
    __CFArrayValidateMutable(theArray, "CFArrayRemoveValueAtIndex called on immutable or non-array object");
    if (idx < 0 || idx >= array->count) {
        _FXCFAbort("CFArrayRemoveValueAtIndex index out of range");
    }

    CFRelease((CFTypeRef)array->values[idx]);
    if (idx + 1 < array->count) {
        memmove(
            (void *)&array->values[idx],
            (const void *)&array->values[idx + 1],
            (size_t)(array->count - idx - 1) * sizeof(const void *)
        );
    }
    array->count -= 1;
    if (array->values != NULL) {
        array->values[array->count] = NULL;
    }
}
