#ifndef FX_COREFOUNDATION_CFDICTIONARY_H
#define FX_COREFOUNDATION_CFDICTIONARY_H

#include "CFBase.h"

#if defined(__cplusplus)
extern "C" {
#endif

typedef const void *(*CFDictionaryRetainCallBack)(CFAllocatorRef allocator, const void *value);
typedef void (*CFDictionaryReleaseCallBack)(CFAllocatorRef allocator, const void *value);
typedef CFStringRef (*CFDictionaryCopyDescriptionCallBack)(const void *value);
typedef Boolean (*CFDictionaryEqualCallBack)(const void *value1, const void *value2);
typedef CFHashCode (*CFDictionaryHashCallBack)(const void *value);

typedef struct {
    CFIndex version;
    CFDictionaryRetainCallBack retain;
    CFDictionaryReleaseCallBack release;
    CFDictionaryCopyDescriptionCallBack copyDescription;
    CFDictionaryEqualCallBack equal;
    CFDictionaryHashCallBack hash;
} CFDictionaryKeyCallBacks;

typedef struct {
    CFIndex version;
    CFDictionaryRetainCallBack retain;
    CFDictionaryReleaseCallBack release;
    CFDictionaryCopyDescriptionCallBack copyDescription;
    CFDictionaryEqualCallBack equal;
} CFDictionaryValueCallBacks;

CF_EXPORT const CFDictionaryKeyCallBacks kCFTypeDictionaryKeyCallBacks;
CF_EXPORT const CFDictionaryValueCallBacks kCFTypeDictionaryValueCallBacks;

CF_EXPORT CFTypeID CFDictionaryGetTypeID(void);
CF_EXPORT CFDictionaryRef CFDictionaryCreate(CFAllocatorRef allocator, const void **keys, const void **values, CFIndex numValues, const CFDictionaryKeyCallBacks *keyCallBacks, const CFDictionaryValueCallBacks *valueCallBacks);
CF_EXPORT CFDictionaryRef CFDictionaryCreateCopy(CFAllocatorRef allocator, CFDictionaryRef theDictionary);
CF_EXPORT CFMutableDictionaryRef CFDictionaryCreateMutable(CFAllocatorRef allocator, CFIndex capacity, const CFDictionaryKeyCallBacks *keyCallBacks, const CFDictionaryValueCallBacks *valueCallBacks);
CF_EXPORT CFIndex CFDictionaryGetCount(CFDictionaryRef theDictionary);
CF_EXPORT const void *CFDictionaryGetValue(CFDictionaryRef theDictionary, const void *key);
CF_EXPORT Boolean CFDictionaryGetValueIfPresent(CFDictionaryRef theDictionary, const void *key, const void **value);
CF_EXPORT void CFDictionarySetValue(CFMutableDictionaryRef theDictionary, const void *key, const void *value);
CF_EXPORT void CFDictionaryRemoveValue(CFMutableDictionaryRef theDictionary, const void *key);

#if defined(__cplusplus)
}
#endif

#endif
