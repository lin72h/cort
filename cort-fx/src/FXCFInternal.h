#ifndef FX_CF_INTERNAL_H
#define FX_CF_INTERNAL_H

#include <CoreFoundation/CFRuntime.h>

#include <limits.h>
#include <pthread.h>
#include <stdalign.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    _kCFRuntimeIDNotAType = 0,
    _kCFRuntimeIDCFType = 1,
    _kCFRuntimeIDCFAllocator = 2,
    _kCFRuntimeIDCFBoolean = 3,
    _kCFRuntimeIDCFNumber = 4,
    _kCFRuntimeIDCFData = 5,
    _kCFRuntimeIDCFDate = 6,
    _kCFRuntimeIDCFString = 7,
    _kCFRuntimeStartingClassID = 16
};

#define FX_CF_MAX_RUNTIME_TYPES 1024u
#define FX_CF_ALLOCATOR_PREFIX_SIZE 16u
#define FX_CF_ALLOCATOR_HINT_ZERO 0x10000000UL

#define FX_CF_INFO_TYPE_SHIFT 8u
#define FX_CF_INFO_TYPE_MASK (0xffffULL << FX_CF_INFO_TYPE_SHIFT)
#define FX_CF_INFO_USES_DEFAULT_ALLOCATOR (1ULL << 7)
#define FX_CF_INFO_IMMORTAL (1ULL << 24)
#define FX_CF_INFO_DEALLOCATING (1ULL << 25)
#define FX_CF_INFO_DEALLOCATED (1ULL << 26)
#define FX_CF_INFO_RETAIN_SHIFT 32u
#define FX_CF_INFO_RETAIN_INCREMENT (1ULL << FX_CF_INFO_RETAIN_SHIFT)

#define FX_CF_ALIGN_16(value) (((value) + 15u) & ~(size_t)15u)

static inline void _FXCFAbort(const char *message) __attribute__((noreturn));
static inline uint64_t _FXCFInfoMake(CFTypeID typeID, uint32_t retainCount, Boolean usesDefaultAllocator, Boolean immortal);
static inline CFTypeID _FXCFTypeIDFromInfo(uint64_t info);
static inline uint32_t _FXCFRetainCountFromInfo(uint64_t info);
static inline Boolean _FXCFInfoIsImmortal(uint64_t info);
static inline Boolean _FXCFAllocatorIsSystemDefault(CFAllocatorRef allocator);
static inline Boolean _FXCFAllocatorRespectsHintZero(CFAllocatorRef allocator);
static inline void _FXCFValidateType(CFTypeRef cf, CFTypeID expectedTypeID, const char *apiName);

const CFRuntimeClass *_FXCFAllocatorClass(void);
const CFRuntimeClass *_FXCFBooleanClass(void);
const CFRuntimeClass *_FXCFNumberClass(void);
const CFRuntimeClass *_FXCFDataClass(void);
const CFRuntimeClass *_FXCFDateClass(void);
const CFRuntimeClass *_FXCFStringClass(void);

static inline CFRuntimeBase *_FXCFBaseFromRef(CFTypeRef cf) {
    return (CFRuntimeBase *)(uintptr_t)cf;
}

static inline const CFRuntimeClass *_FXCFClassFromBase(const CFRuntimeBase *base) {
    return (const CFRuntimeClass *)(uintptr_t)base->_cfisa;
}

static inline void _FXCFAbort(const char *message) {
    if (message != NULL) {
        fprintf(stderr, "cort-fx: %s\n", message);
    }
    abort();
}

static inline uint64_t _FXCFInfoMake(CFTypeID typeID, uint32_t retainCount, Boolean usesDefaultAllocator, Boolean immortal) {
    if (typeID >= (CFTypeID)(FX_CF_INFO_TYPE_MASK >> FX_CF_INFO_TYPE_SHIFT)) {
        _FXCFAbort("CFTypeID exceeds Subset 0 _cfinfoa encoding");
    }

    uint64_t info = ((uint64_t)typeID << FX_CF_INFO_TYPE_SHIFT);
    if (usesDefaultAllocator) {
        info |= FX_CF_INFO_USES_DEFAULT_ALLOCATOR;
    }
    if (immortal) {
        info |= FX_CF_INFO_IMMORTAL;
    } else {
        info |= ((uint64_t)retainCount << FX_CF_INFO_RETAIN_SHIFT);
    }
    return info;
}

static inline CFTypeID _FXCFTypeIDFromInfo(uint64_t info) {
    return (CFTypeID)((info & FX_CF_INFO_TYPE_MASK) >> FX_CF_INFO_TYPE_SHIFT);
}

static inline uint32_t _FXCFRetainCountFromInfo(uint64_t info) {
    return (uint32_t)(info >> FX_CF_INFO_RETAIN_SHIFT);
}

static inline Boolean _FXCFInfoIsImmortal(uint64_t info) {
    return (info & FX_CF_INFO_IMMORTAL) != 0;
}

static inline Boolean _FXCFAllocatorIsSystemDefault(CFAllocatorRef allocator) {
    if (allocator == NULL || allocator == kCFAllocatorDefault) {
        return CFAllocatorGetDefault() == kCFAllocatorSystemDefault;
    }
    return allocator == kCFAllocatorSystemDefault;
}

static inline Boolean _FXCFAllocatorRespectsHintZero(CFAllocatorRef allocator) {
    return allocator == kCFAllocatorSystemDefault;
}

static inline void _FXCFValidateType(CFTypeRef cf, CFTypeID expectedTypeID, const char *apiName) {
    if (cf == NULL) {
        _FXCFAbort(apiName);
    }
    if (CFGetTypeID(cf) != expectedTypeID) {
        _FXCFAbort(apiName);
    }
}

#endif
