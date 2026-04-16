#include "FXCFInternal.h"

#include <CoreFoundation/CFDate.h>

struct __CFDate {
    CFRuntimeBase _base;
    CFAbsoluteTime at;
};

static CFHashCode __CFDateHashValue(CFAbsoluteTime value) {
    union {
        double f64;
        uint64_t bits;
    } storage;

    storage.f64 = value;
    storage.bits ^= storage.bits >> 33u;
    storage.bits *= 0xff51afd7ed558ccdULL;
    storage.bits ^= storage.bits >> 33u;
    storage.bits *= 0xc4ceb9fe1a85ec53ULL;
    storage.bits ^= storage.bits >> 33u;
    return (CFHashCode)storage.bits;
}

static Boolean __CFDateEqual(CFTypeRef cf1, CFTypeRef cf2) {
    const struct __CFDate *date1 = (const struct __CFDate *)cf1;
    const struct __CFDate *date2 = (const struct __CFDate *)cf2;
    return date1->at == date2->at;
}

static CFHashCode __CFDateHash(CFTypeRef cf) {
    const struct __CFDate *date = (const struct __CFDate *)cf;
    return __CFDateHashValue(date->at);
}

static const CFRuntimeClass __CFDateClass = {
    0,
    "CFDate",
    NULL,
    NULL,
    NULL,
    __CFDateEqual,
    __CFDateHash,
    NULL,
    NULL,
    NULL,
    NULL,
    0
};

const CFRuntimeClass *_FXCFDateClass(void) {
    return &__CFDateClass;
}

CFTypeID CFDateGetTypeID(void) {
    return _kCFRuntimeIDCFDate;
}

CFDateRef CFDateCreate(CFAllocatorRef allocator, CFAbsoluteTime at) {
    struct __CFDate *date = (struct __CFDate *)_CFRuntimeCreateInstance(
        allocator,
        CFDateGetTypeID(),
        (CFIndex)(sizeof(struct __CFDate) - sizeof(CFRuntimeBase)),
        NULL
    );
    if (date == NULL) {
        return NULL;
    }

    date->at = at;
    return date;
}

CFAbsoluteTime CFDateGetAbsoluteTime(CFDateRef theDate) {
    _FXCFValidateType((CFTypeRef)theDate, CFDateGetTypeID(), "CFDateGetAbsoluteTime called with non-date object");
    return ((const struct __CFDate *)theDate)->at;
}
