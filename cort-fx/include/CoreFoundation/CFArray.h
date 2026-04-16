#ifndef FX_COREFOUNDATION_CFARRAY_H
#define FX_COREFOUNDATION_CFARRAY_H

#include "CFBase.h"

#if defined(__cplusplus)
extern "C" {
#endif

typedef const void *(*CFArrayRetainCallBack)(CFAllocatorRef allocator, const void *value);
typedef void (*CFArrayReleaseCallBack)(CFAllocatorRef allocator, const void *value);
typedef CFStringRef (*CFArrayCopyDescriptionCallBack)(const void *value);
typedef Boolean (*CFArrayEqualCallBack)(const void *value1, const void *value2);

typedef struct {
    CFIndex version;
    CFArrayRetainCallBack retain;
    CFArrayReleaseCallBack release;
    CFArrayCopyDescriptionCallBack copyDescription;
    CFArrayEqualCallBack equal;
} CFArrayCallBacks;

CF_EXPORT const CFArrayCallBacks kCFTypeArrayCallBacks;

CF_EXPORT CFTypeID CFArrayGetTypeID(void);
CF_EXPORT CFArrayRef CFArrayCreate(CFAllocatorRef allocator, const void **values, CFIndex numValues, const CFArrayCallBacks *callBacks);
CF_EXPORT CFArrayRef CFArrayCreateCopy(CFAllocatorRef allocator, CFArrayRef theArray);
CF_EXPORT CFMutableArrayRef CFArrayCreateMutable(CFAllocatorRef allocator, CFIndex capacity, const CFArrayCallBacks *callBacks);
CF_EXPORT CFIndex CFArrayGetCount(CFArrayRef theArray);
CF_EXPORT const void *CFArrayGetValueAtIndex(CFArrayRef theArray, CFIndex idx);
CF_EXPORT void CFArrayAppendValue(CFMutableArrayRef theArray, const void *value);
CF_EXPORT void CFArrayRemoveValueAtIndex(CFMutableArrayRef theArray, CFIndex idx);

#if defined(__cplusplus)
}
#endif

#endif
