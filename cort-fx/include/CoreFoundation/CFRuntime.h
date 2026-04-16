#ifndef FX_COREFOUNDATION_CFRUNTIME_H
#define FX_COREFOUNDATION_CFRUNTIME_H

#include "CFBase.h"

#include <stdatomic.h>

#if defined(__cplusplus)
extern "C" {
#endif

enum {
    _kCFRuntimeNotATypeID = 0
};

enum {
    _kCFRuntimeScannedObject = (1UL << 0),
    _kCFRuntimeResourcefulObject = (1UL << 2),
    _kCFRuntimeCustomRefCount = (1UL << 3),
    _kCFRuntimeRequiresAlignment = (1UL << 4)
};

typedef struct __CFRuntimeClass {
    CFIndex version;
    const char *className;
    void (*init)(CFTypeRef cf);
    CFTypeRef (*copy)(CFAllocatorRef allocator, CFTypeRef cf);
    void (*finalize)(CFTypeRef cf);
    Boolean (*equal)(CFTypeRef cf1, CFTypeRef cf2);
    CFHashCode (*hash)(CFTypeRef cf);
    CFStringRef (*copyFormattingDesc)(CFTypeRef cf, CFDictionaryRef formatOptions);
    CFStringRef (*copyDebugDesc)(CFTypeRef cf);
    void (*reclaim)(CFTypeRef cf);
    uint32_t (*refcount)(intptr_t op, CFTypeRef cf);
    uintptr_t requiredAlignment;
} CFRuntimeClass;

typedef struct __CFRuntimeBase {
    uintptr_t _cfisa;
    _Atomic(uint64_t) _cfinfoa;
} CFRuntimeBase;

#define INIT_CFRUNTIME_BASE(...) {0, 0x0000000000000080ULL}

CF_EXPORT CFTypeID _CFRuntimeRegisterClass(const CFRuntimeClass *cls);
CF_EXPORT const CFRuntimeClass *_CFRuntimeGetClassWithTypeID(CFTypeID typeID);
CF_EXPORT void _CFRuntimeUnregisterClassWithTypeID(CFTypeID typeID);
CF_EXPORT CFTypeRef _CFRuntimeCreateInstance(CFAllocatorRef allocator, CFTypeID typeID, CFIndex extraBytes, unsigned char *category);
CF_EXPORT void _CFRuntimeInitStaticInstance(void *memory, CFTypeID typeID);

#if defined(__cplusplus)
}
#endif

#endif
