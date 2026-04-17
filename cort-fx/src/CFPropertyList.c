#include "FXCFInternal.h"

#include <CoreFoundation/CFPropertyList.h>

CFDataRef CFPropertyListCreateData(
    CFAllocatorRef allocator,
    CFPropertyListRef propertyList,
    CFPropertyListFormat format,
    CFOptionFlags options,
    CFErrorRef *error
) {
    return _FXCFBinaryPListCreateData(allocator, propertyList, format, options, error);
}

CFPropertyListRef CFPropertyListCreateWithData(
    CFAllocatorRef allocator,
    CFDataRef data,
    CFPropertyListMutabilityOptions options,
    CFPropertyListFormat *format,
    CFErrorRef *error
) {
    return _FXCFBinaryPListCreateWithData(allocator, data, options, format, error);
}
