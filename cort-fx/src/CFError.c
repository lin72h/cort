#include "FXCFInternal.h"

#include <CoreFoundation/CFError.h>

struct __CFError {
    CFRuntimeBase _base;
    CFIndex code;
};

static Boolean __CFErrorEqual(CFTypeRef cf1, CFTypeRef cf2) {
    const struct __CFError *error1 = (const struct __CFError *)cf1;
    const struct __CFError *error2 = (const struct __CFError *)cf2;
    return error1->code == error2->code;
}

static CFHashCode __CFErrorHash(CFTypeRef cf) {
    const struct __CFError *error = (const struct __CFError *)cf;
    uint64_t value = (uint64_t)error->code;
    value ^= value >> 33u;
    value *= 0xff51afd7ed558ccdULL;
    value ^= value >> 33u;
    value *= 0xc4ceb9fe1a85ec53ULL;
    value ^= value >> 33u;
    return (CFHashCode)value;
}

static const CFRuntimeClass __CFErrorClass = {
    0,
    "CFError",
    NULL,
    NULL,
    NULL,
    __CFErrorEqual,
    __CFErrorHash,
    NULL,
    NULL,
    NULL,
    NULL,
    0
};

const CFRuntimeClass *_FXCFErrorClass(void) {
    return &__CFErrorClass;
}

CFErrorRef _FXCFErrorCreateCode(CFAllocatorRef allocator, CFIndex code) {
    struct __CFError *error = (struct __CFError *)_CFRuntimeCreateInstance(
        allocator,
        CFErrorGetTypeID(),
        (CFIndex)(sizeof(struct __CFError) - sizeof(CFRuntimeBase)),
        NULL
    );
    if (error == NULL) {
        return NULL;
    }

    error->code = code;
    return error;
}

CFTypeID CFErrorGetTypeID(void) {
    return _kCFRuntimeIDCFError;
}

CFIndex CFErrorGetCode(CFErrorRef err) {
    _FXCFValidateType((CFTypeRef)err, CFErrorGetTypeID(), "CFErrorGetCode called with non-error object");
    return ((const struct __CFError *)err)->code;
}
