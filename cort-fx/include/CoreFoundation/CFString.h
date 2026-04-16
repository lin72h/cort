#ifndef FX_COREFOUNDATION_CFSTRING_H
#define FX_COREFOUNDATION_CFSTRING_H

#include "CFBase.h"

#if defined(__cplusplus)
extern "C" {
#endif

typedef UInt32 CFStringEncoding;

enum {
    kCFStringEncodingMacRoman = 0,
    kCFStringEncodingASCII = 0x0600,
    kCFStringEncodingUnicode = 0x0100,
    kCFStringEncodingUTF8 = 0x08000100,
    kCFStringEncodingUTF16 = 0x0100
};

CF_EXPORT CFTypeID CFStringGetTypeID(void);
CF_EXPORT CFStringRef CFStringCreateWithCString(CFAllocatorRef allocator, const char *cStr, CFStringEncoding encoding);
CF_EXPORT CFStringRef CFStringCreateWithBytes(CFAllocatorRef allocator, const UInt8 *bytes, CFIndex numBytes, CFStringEncoding encoding, Boolean isExternalRepresentation);
CF_EXPORT CFStringRef CFStringCreateWithCharacters(CFAllocatorRef allocator, const UniChar *chars, CFIndex numChars);
CF_EXPORT CFStringRef CFStringCreateCopy(CFAllocatorRef allocator, CFStringRef theString);
CF_EXPORT CFIndex CFStringGetLength(CFStringRef theString);
CF_EXPORT void CFStringGetCharacters(CFStringRef theString, CFRange range, UniChar *buffer);
CF_EXPORT Boolean CFStringGetCString(CFStringRef theString, char *buffer, CFIndex bufferSize, CFStringEncoding encoding);
CF_EXPORT CFIndex CFStringGetBytes(CFStringRef theString, CFRange range, CFStringEncoding encoding, UInt8 lossByte, Boolean isExternalRepresentation, UInt8 *buffer, CFIndex maxBufLen, CFIndex *usedBufLen);

#if defined(__cplusplus)
}
#endif

#endif
