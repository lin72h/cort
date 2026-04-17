#ifndef FX_COREFOUNDATION_CFPROPERTYLIST_H
#define FX_COREFOUNDATION_CFPROPERTYLIST_H

#include "CFBase.h"
#include "CFData.h"
#include "CFError.h"

#if defined(__cplusplus)
extern "C" {
#endif

typedef CFOptionFlags CFPropertyListMutabilityOptions;
typedef CFIndex CFPropertyListFormat;

enum {
    kCFPropertyListImmutable = 0,
    kCFPropertyListMutableContainers = 1 << 0,
    kCFPropertyListMutableContainersAndLeaves = 1 << 1
};

enum {
    kCFPropertyListOpenStepFormat = 1,
    kCFPropertyListXMLFormat_v1_0 = 100,
    kCFPropertyListBinaryFormat_v1_0 = 200
};

enum {
    kCFPropertyListReadCorruptError = 3840,
    kCFPropertyListReadUnknownVersionError = 3841,
    kCFPropertyListReadStreamError = 3842,
    kCFPropertyListWriteStreamError = 3851
};

CF_EXPORT CFPropertyListRef CFPropertyListCreateWithData(
    CFAllocatorRef allocator,
    CFDataRef data,
    CFOptionFlags options,
    CFPropertyListFormat *format,
    CFErrorRef *error
);

CF_EXPORT CFDataRef CFPropertyListCreateData(
    CFAllocatorRef allocator,
    CFPropertyListRef propertyList,
    CFPropertyListFormat format,
    CFOptionFlags options,
    CFErrorRef *error
);

#if defined(__cplusplus)
}
#endif

#endif
