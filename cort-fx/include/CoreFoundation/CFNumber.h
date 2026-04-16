#ifndef FX_COREFOUNDATION_CFNUMBER_H
#define FX_COREFOUNDATION_CFNUMBER_H

#include "CFBase.h"

#if defined(__cplusplus)
extern "C" {
#endif

typedef const struct __CFBoolean *CFBooleanRef;

CF_EXPORT const CFBooleanRef kCFBooleanTrue;
CF_EXPORT const CFBooleanRef kCFBooleanFalse;

CF_EXPORT CFTypeID CFBooleanGetTypeID(void);
CF_EXPORT Boolean CFBooleanGetValue(CFBooleanRef boolean);

typedef enum {
    kCFNumberSInt8Type = 1,
    kCFNumberSInt16Type = 2,
    kCFNumberSInt32Type = 3,
    kCFNumberSInt64Type = 4,
    kCFNumberFloat32Type = 5,
    kCFNumberFloat64Type = 6,
    kCFNumberCharType = 7,
    kCFNumberShortType = 8,
    kCFNumberIntType = 9,
    kCFNumberLongType = 10,
    kCFNumberLongLongType = 11,
    kCFNumberFloatType = 12,
    kCFNumberDoubleType = 13,
    kCFNumberCFIndexType = 14,
    kCFNumberNSIntegerType = 15,
    kCFNumberCGFloatType = 16,
    kCFNumberMaxType = 16
} CFNumberType;

typedef const struct __CFNumber *CFNumberRef;

CF_EXPORT CFTypeID CFNumberGetTypeID(void);
CF_EXPORT CFNumberRef CFNumberCreate(CFAllocatorRef allocator, CFNumberType theType, const void *valuePtr);
CF_EXPORT CFNumberType CFNumberGetType(CFNumberRef number);
CF_EXPORT Boolean CFNumberGetValue(CFNumberRef number, CFNumberType theType, void *valuePtr);

#if defined(__cplusplus)
}
#endif

#endif
