#ifndef FX_COREFOUNDATION_CFPROPERTYLIST_H
#define FX_COREFOUNDATION_CFPROPERTYLIST_H

#include "CFBase.h"
#include "CFData.h"
#include "CFError.h"

#if defined(__cplusplus)
extern "C" {
#endif

typedef CFIndex CFPropertyListFormat;
typedef CFOptionFlags CFPropertyListMutabilityOptions;

enum {
    kCFPropertyListOpenStepFormat = 1,
    kCFPropertyListXMLFormat_v1_0 = 100,
    kCFPropertyListBinaryFormat_v1_0 = 200
};

enum {
    kCFPropertyListImmutable = 0,
    kCFPropertyListMutableContainers = 1,
    kCFPropertyListMutableContainersAndLeaves = 2
};

enum {
    kCFPropertyListReadCorruptError = 3840,
    kCFPropertyListWriteStreamError = 3851
};

CF_EXPORT CFDataRef CFPropertyListCreateData(
    CFAllocatorRef allocator,
    CFPropertyListRef propertyList,
    CFPropertyListFormat format,
    CFOptionFlags options,
    CFErrorRef *error
);

CF_EXPORT CFPropertyListRef CFPropertyListCreateWithData(
    CFAllocatorRef allocator,
    CFDataRef data,
    CFPropertyListMutabilityOptions options,
    CFPropertyListFormat *format,
    CFErrorRef *error
);

#if defined(__cplusplus)
}
#endif

#endif
