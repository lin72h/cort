#ifndef FX_COREFOUNDATION_CFDATA_H
#define FX_COREFOUNDATION_CFDATA_H

#include "CFBase.h"

#if defined(__cplusplus)
extern "C" {
#endif

typedef const struct __CFData *CFDataRef;

CF_EXPORT CFTypeID CFDataGetTypeID(void);
CF_EXPORT CFDataRef CFDataCreate(CFAllocatorRef allocator, const UInt8 *bytes, CFIndex length);
CF_EXPORT CFDataRef CFDataCreateCopy(CFAllocatorRef allocator, CFDataRef theData);
CF_EXPORT CFIndex CFDataGetLength(CFDataRef theData);
CF_EXPORT const UInt8 *CFDataGetBytePtr(CFDataRef theData);

#if defined(__cplusplus)
}
#endif

#endif
