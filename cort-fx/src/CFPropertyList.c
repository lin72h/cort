#include "FXCFInternal.h"

#include <CoreFoundation/CFData.h>
#include <CoreFoundation/CFPropertyList.h>

CFDataRef _FXCFBinaryPListCreateData(CFAllocatorRef allocator, CFPropertyListRef propertyList, CFErrorRef *errorOut);
CFPropertyListRef _FXCFBinaryPListCreateWithData(CFAllocatorRef allocator, CFDataRef data, CFPropertyListFormat *formatOut, CFErrorRef *errorOut);

static void __CFPropertyListClearError(CFErrorRef *error) {
    if (error != NULL) {
        *error = NULL;
    }
}

CFDataRef CFPropertyListCreateData(CFAllocatorRef allocator, CFPropertyListRef propertyList, CFPropertyListFormat format, CFOptionFlags options, CFErrorRef *error) {
    __CFPropertyListClearError(error);

    if (format != kCFPropertyListBinaryFormat_v1_0 || options != 0) {
        if (error != NULL) {
            *error = _FXCFErrorCreateCode(allocator, kCFPropertyListWriteStreamError);
        }
        return NULL;
    }

    return _FXCFBinaryPListCreateData(allocator, propertyList, error);
}

CFPropertyListRef CFPropertyListCreateWithData(CFAllocatorRef allocator, CFDataRef data, CFOptionFlags options, CFPropertyListFormat *format, CFErrorRef *error) {
    if (format != NULL) {
        *format = 0;
    }
    __CFPropertyListClearError(error);

    if (options != kCFPropertyListImmutable) {
        if (error != NULL) {
            *error = _FXCFErrorCreateCode(allocator, kCFPropertyListReadCorruptError);
        }
        return NULL;
    }

    return _FXCFBinaryPListCreateWithData(allocator, data, format, error);
}
