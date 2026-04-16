#ifndef FX_COREFOUNDATION_CFDATE_H
#define FX_COREFOUNDATION_CFDATE_H

#include "CFBase.h"

#if defined(__cplusplus)
extern "C" {
#endif

typedef double CFTimeInterval;
typedef CFTimeInterval CFAbsoluteTime;

typedef const struct __CFDate *CFDateRef;

CF_EXPORT CFTypeID CFDateGetTypeID(void);
CF_EXPORT CFDateRef CFDateCreate(CFAllocatorRef allocator, CFAbsoluteTime at);
CF_EXPORT CFAbsoluteTime CFDateGetAbsoluteTime(CFDateRef theDate);

#if defined(__cplusplus)
}
#endif

#endif
