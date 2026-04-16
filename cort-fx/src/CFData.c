#include "FXCFInternal.h"

#include <CoreFoundation/CFData.h>

#include <limits.h>

struct __CFData {
    CFRuntimeBase _base;
    CFIndex length;
    UInt8 bytes[];
};

static Boolean __CFDataEqual(CFTypeRef cf1, CFTypeRef cf2) {
    const struct __CFData *data1 = (const struct __CFData *)cf1;
    const struct __CFData *data2 = (const struct __CFData *)cf2;

    if (data1->length != data2->length) {
        return false;
    }
    if (data1->length == 0) {
        return true;
    }
    return memcmp(data1->bytes, data2->bytes, (size_t)data1->length) == 0;
}

static CFHashCode __CFDataHash(CFTypeRef cf) {
    const struct __CFData *data = (const struct __CFData *)cf;
    uint64_t hash = 1469598103934665603ULL;

    for (CFIndex index = 0; index < data->length; ++index) {
        hash ^= data->bytes[index];
        hash *= 1099511628211ULL;
    }

    return (CFHashCode)hash;
}

static const CFRuntimeClass __CFDataClass = {
    0,
    "CFData",
    NULL,
    NULL,
    NULL,
    __CFDataEqual,
    __CFDataHash,
    NULL,
    NULL,
    NULL,
    NULL,
    0
};

const CFRuntimeClass *_FXCFDataClass(void) {
    return &__CFDataClass;
}

CFTypeID CFDataGetTypeID(void) {
    return _kCFRuntimeIDCFData;
}

CFDataRef CFDataCreate(CFAllocatorRef allocator, const UInt8 *bytes, CFIndex length) {
    if (length < 0) {
        return NULL;
    }
    if (length > 0 && bytes == NULL) {
        return NULL;
    }

    size_t headerSize = sizeof(struct __CFData) - sizeof(CFRuntimeBase);
    if ((uint64_t)length > (uint64_t)(LONG_MAX - (long)headerSize)) {
        return NULL;
    }

    CFIndex extraBytes = (CFIndex)(headerSize + (size_t)length);
    struct __CFData *data = (struct __CFData *)_CFRuntimeCreateInstance(allocator, CFDataGetTypeID(), extraBytes, NULL);
    if (data == NULL) {
        return NULL;
    }

    data->length = length;
    if (length > 0) {
        memcpy(data->bytes, bytes, (size_t)length);
    }
    return data;
}

CFDataRef CFDataCreateCopy(CFAllocatorRef allocator, CFDataRef theData) {
    _FXCFValidateType((CFTypeRef)theData, CFDataGetTypeID(), "CFDataCreateCopy called with non-data object");
    const struct __CFData *data = (const struct __CFData *)theData;
    return CFDataCreate(allocator, data->length > 0 ? data->bytes : NULL, data->length);
}

CFIndex CFDataGetLength(CFDataRef theData) {
    _FXCFValidateType((CFTypeRef)theData, CFDataGetTypeID(), "CFDataGetLength called with non-data object");
    const struct __CFData *data = (const struct __CFData *)theData;
    return data->length;
}

const UInt8 *CFDataGetBytePtr(CFDataRef theData) {
    _FXCFValidateType((CFTypeRef)theData, CFDataGetTypeID(), "CFDataGetBytePtr called with non-data object");
    const struct __CFData *data = (const struct __CFData *)theData;
    if (data->length == 0) {
        return NULL;
    }
    return data->bytes;
}
