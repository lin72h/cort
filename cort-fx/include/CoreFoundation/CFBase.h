#ifndef FX_COREFOUNDATION_CFBASE_H
#define FX_COREFOUNDATION_CFBASE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#if defined(__cplusplus)
extern "C" {
#endif

#ifndef CF_EXPORT
#if defined(__clang__) || defined(__GNUC__)
#define CF_EXPORT extern __attribute__((visibility("default")))
#else
#define CF_EXPORT extern
#endif
#endif

#ifndef NULL
#if defined(__cplusplus)
#define NULL 0
#else
#define NULL ((void *)0)
#endif
#endif

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

typedef bool Boolean;
typedef uint8_t UInt8;
typedef int8_t SInt8;
typedef uint16_t UInt16;
typedef int16_t SInt16;
typedef uint32_t UInt32;
typedef int32_t SInt32;
typedef uint64_t UInt64;
typedef int64_t SInt64;
typedef UInt16 UniChar;

typedef unsigned long CFTypeID;
typedef unsigned long CFOptionFlags;
typedef unsigned long CFHashCode;
typedef signed long CFIndex;

typedef const void *CFTypeRef;

typedef const struct __CFAllocator *CFAllocatorRef;
typedef const struct __CFString *CFStringRef;
typedef struct __CFString *CFMutableStringRef;
typedef const struct __CFArray *CFArrayRef;
typedef struct __CFArray *CFMutableArrayRef;
typedef const struct __CFDictionary *CFDictionaryRef;
typedef struct __CFDictionary *CFMutableDictionaryRef;

typedef CFTypeRef CFPropertyListRef;

typedef enum {
    kCFCompareLessThan = -1L,
    kCFCompareEqualTo = 0,
    kCFCompareGreaterThan = 1
} CFComparisonResult;

typedef CFComparisonResult (*CFComparatorFunction)(const void *val1, const void *val2, void *context);

static const CFIndex kCFNotFound = -1;

typedef struct {
    CFIndex location;
    CFIndex length;
} CFRange;

static inline CFRange CFRangeMake(CFIndex loc, CFIndex len) {
    CFRange range;
    range.location = loc;
    range.length = len;
    return range;
}

CF_EXPORT CFRange __CFRangeMake(CFIndex loc, CFIndex len);

typedef const void *(*CFAllocatorRetainCallBack)(const void *info);
typedef void (*CFAllocatorReleaseCallBack)(const void *info);
typedef CFStringRef (*CFAllocatorCopyDescriptionCallBack)(const void *info);
typedef void *(*CFAllocatorAllocateCallBack)(CFIndex allocSize, CFOptionFlags hint, void *info);
typedef void *(*CFAllocatorReallocateCallBack)(void *ptr, CFIndex newsize, CFOptionFlags hint, void *info);
typedef void (*CFAllocatorDeallocateCallBack)(void *ptr, void *info);
typedef CFIndex (*CFAllocatorPreferredSizeCallBack)(CFIndex size, CFOptionFlags hint, void *info);

typedef struct {
    CFIndex version;
    void *info;
    CFAllocatorRetainCallBack retain;
    CFAllocatorReleaseCallBack release;
    CFAllocatorCopyDescriptionCallBack copyDescription;
    CFAllocatorAllocateCallBack allocate;
    CFAllocatorReallocateCallBack reallocate;
    CFAllocatorDeallocateCallBack deallocate;
    CFAllocatorPreferredSizeCallBack preferredSize;
} CFAllocatorContext;

CF_EXPORT const CFAllocatorRef kCFAllocatorDefault;
CF_EXPORT const CFAllocatorRef kCFAllocatorSystemDefault;
CF_EXPORT const CFAllocatorRef kCFAllocatorMalloc;
CF_EXPORT const CFAllocatorRef kCFAllocatorNull;
CF_EXPORT const CFAllocatorRef kCFAllocatorUseContext;

CF_EXPORT CFTypeID CFAllocatorGetTypeID(void);
CF_EXPORT void CFAllocatorSetDefault(CFAllocatorRef allocator);
CF_EXPORT CFAllocatorRef CFAllocatorGetDefault(void);
CF_EXPORT CFAllocatorRef CFAllocatorCreate(CFAllocatorRef allocator, CFAllocatorContext *context);
CF_EXPORT void *CFAllocatorAllocate(CFAllocatorRef allocator, CFIndex size, CFOptionFlags hint);
CF_EXPORT void *CFAllocatorReallocate(CFAllocatorRef allocator, void *ptr, CFIndex newsize, CFOptionFlags hint);
CF_EXPORT void CFAllocatorDeallocate(CFAllocatorRef allocator, void *ptr);
CF_EXPORT CFIndex CFAllocatorGetPreferredSizeForSize(CFAllocatorRef allocator, CFIndex size, CFOptionFlags hint);
CF_EXPORT void CFAllocatorGetContext(CFAllocatorRef allocator, CFAllocatorContext *context);

CF_EXPORT CFTypeID CFGetTypeID(CFTypeRef cf);
CF_EXPORT Boolean CFEqual(CFTypeRef cf1, CFTypeRef cf2);
CF_EXPORT CFHashCode CFHash(CFTypeRef cf);
CF_EXPORT CFTypeRef CFRetain(CFTypeRef cf);
CF_EXPORT void CFRelease(CFTypeRef cf);
CF_EXPORT CFIndex CFGetRetainCount(CFTypeRef cf);
CF_EXPORT CFAllocatorRef CFGetAllocator(CFTypeRef cf);

#if defined(__cplusplus)
}
#endif

#endif
