#include "FXCFInternal.h"

#include <CoreFoundation/CFString.h>

#include <limits.h>

enum {
    FX_CFSTRING_FLAG_ASCII = 1u
};

struct __CFString {
    CFRuntimeBase _base;
    CFIndex length;
    UInt32 flags;
    UniChar contents[];
};

static Boolean __CFStringRangeIsValid(const struct __CFString *string, CFRange range) {
    if (range.location < 0 || range.length < 0) {
        return false;
    }
    if (range.location > string->length) {
        return false;
    }
    return range.length <= (string->length - range.location);
}

static Boolean __CFStringIsASCII(const struct __CFString *string) {
    return (string->flags & FX_CFSTRING_FLAG_ASCII) != 0u;
}

static CFHashCode __CFHashUInt64(uint64_t value) {
    value ^= value >> 33u;
    value *= 0xff51afd7ed558ccdULL;
    value ^= value >> 33u;
    value *= 0xc4ceb9fe1a85ec53ULL;
    value ^= value >> 33u;
    return (CFHashCode)value;
}

static CFHashCode __CFStringHash(CFTypeRef cf) {
    const struct __CFString *string = (const struct __CFString *)cf;
    uint64_t hash = 1469598103934665603ULL;

    for (CFIndex index = 0; index < string->length; ++index) {
        hash ^= (UInt8)(string->contents[index] & 0xffu);
        hash *= 1099511628211ULL;
        hash ^= (UInt8)((string->contents[index] >> 8u) & 0xffu);
        hash *= 1099511628211ULL;
    }

    return __CFHashUInt64(hash ^ (uint64_t)string->length);
}

static Boolean __CFStringEqual(CFTypeRef cf1, CFTypeRef cf2) {
    const struct __CFString *string1 = (const struct __CFString *)cf1;
    const struct __CFString *string2 = (const struct __CFString *)cf2;

    if (string1->length != string2->length) {
        return false;
    }
    if (string1->length == 0) {
        return true;
    }
    return memcmp(string1->contents, string2->contents, (size_t)string1->length * sizeof(UniChar)) == 0;
}

static const CFRuntimeClass __CFStringClass = {
    0,
    "CFString",
    NULL,
    NULL,
    NULL,
    __CFStringEqual,
    __CFStringHash,
    NULL,
    NULL,
    NULL,
    NULL,
    0
};

const CFRuntimeClass *_FXCFStringClass(void) {
    return &__CFStringClass;
}

static Boolean __CFUTF8IsContinuation(UInt8 byte) {
    return (byte & 0xc0u) == 0x80u;
}

static Boolean __CFStringUTF8CountCodeUnits(const UInt8 *bytes, CFIndex byteCount, CFIndex *unitCount) {
    CFIndex totalUnits = 0;
    CFIndex index = 0;

    while (index < byteCount) {
        UInt8 byte = bytes[index];
        if (byte <= 0x7fu) {
            totalUnits += 1;
            index += 1;
            continue;
        }

        if (byte >= 0xc2u && byte <= 0xdfu) {
            if (index + 1 >= byteCount || !__CFUTF8IsContinuation(bytes[index + 1])) {
                return false;
            }
            totalUnits += 1;
            index += 2;
            continue;
        }

        if (byte == 0xe0u) {
            if (index + 2 >= byteCount) {
                return false;
            }
            if (bytes[index + 1] < 0xa0u || bytes[index + 1] > 0xbfu || !__CFUTF8IsContinuation(bytes[index + 2])) {
                return false;
            }
            totalUnits += 1;
            index += 3;
            continue;
        }

        if ((byte >= 0xe1u && byte <= 0xecu) || (byte >= 0xeeu && byte <= 0xefu)) {
            if (index + 2 >= byteCount || !__CFUTF8IsContinuation(bytes[index + 1]) || !__CFUTF8IsContinuation(bytes[index + 2])) {
                return false;
            }
            totalUnits += 1;
            index += 3;
            continue;
        }

        if (byte == 0xedu) {
            if (index + 2 >= byteCount) {
                return false;
            }
            if (bytes[index + 1] < 0x80u || bytes[index + 1] > 0x9fu || !__CFUTF8IsContinuation(bytes[index + 2])) {
                return false;
            }
            totalUnits += 1;
            index += 3;
            continue;
        }

        if (byte == 0xf0u) {
            if (index + 3 >= byteCount) {
                return false;
            }
            if (bytes[index + 1] < 0x90u || bytes[index + 1] > 0xbfu ||
                !__CFUTF8IsContinuation(bytes[index + 2]) ||
                !__CFUTF8IsContinuation(bytes[index + 3])) {
                return false;
            }
            totalUnits += 2;
            index += 4;
            continue;
        }

        if (byte >= 0xf1u && byte <= 0xf3u) {
            if (index + 3 >= byteCount ||
                !__CFUTF8IsContinuation(bytes[index + 1]) ||
                !__CFUTF8IsContinuation(bytes[index + 2]) ||
                !__CFUTF8IsContinuation(bytes[index + 3])) {
                return false;
            }
            totalUnits += 2;
            index += 4;
            continue;
        }

        if (byte == 0xf4u) {
            if (index + 3 >= byteCount) {
                return false;
            }
            if (bytes[index + 1] < 0x80u || bytes[index + 1] > 0x8fu ||
                !__CFUTF8IsContinuation(bytes[index + 2]) ||
                !__CFUTF8IsContinuation(bytes[index + 3])) {
                return false;
            }
            totalUnits += 2;
            index += 4;
            continue;
        }

        return false;
    }

    if (unitCount != NULL) {
        *unitCount = totalUnits;
    }
    return true;
}

static Boolean __CFStringDecodeUTF8(const UInt8 *bytes, CFIndex byteCount, UniChar *buffer, CFIndex unitCount) {
    CFIndex index = 0;
    CFIndex out = 0;

    while (index < byteCount) {
        UInt32 codePoint = 0;
        UInt8 byte = bytes[index];

        if (byte <= 0x7fu) {
            codePoint = byte;
            index += 1;
        } else if (byte >= 0xc2u && byte <= 0xdfu) {
            codePoint = (UInt32)(byte & 0x1fu) << 6u;
            codePoint |= (UInt32)(bytes[index + 1] & 0x3fu);
            index += 2;
        } else if (byte <= 0xefu) {
            codePoint = (UInt32)(byte & 0x0fu) << 12u;
            codePoint |= (UInt32)(bytes[index + 1] & 0x3fu) << 6u;
            codePoint |= (UInt32)(bytes[index + 2] & 0x3fu);
            index += 3;
        } else {
            codePoint = (UInt32)(byte & 0x07u) << 18u;
            codePoint |= (UInt32)(bytes[index + 1] & 0x3fu) << 12u;
            codePoint |= (UInt32)(bytes[index + 2] & 0x3fu) << 6u;
            codePoint |= (UInt32)(bytes[index + 3] & 0x3fu);
            index += 4;
        }

        if (codePoint <= 0xffffu) {
            if (out >= unitCount) {
                return false;
            }
            buffer[out++] = (UniChar)codePoint;
        } else {
            UInt32 adjusted = codePoint - 0x10000u;
            if (out + 1 >= unitCount) {
                return false;
            }
            buffer[out++] = (UniChar)(0xd800u + (adjusted >> 10u));
            buffer[out++] = (UniChar)(0xdc00u + (adjusted & 0x3ffu));
        }
    }

    return out == unitCount;
}

static Boolean __CFStringUTF8EncodedLength(const struct __CFString *string, CFRange range, CFIndex *byteCount) {
    CFIndex totalBytes = 0;

    for (CFIndex index = 0; index < range.length; ++index) {
        UniChar unit = string->contents[range.location + index];
        if (unit <= 0x7fu) {
            totalBytes += 1;
            continue;
        }
        if (unit <= 0x7ffu) {
            totalBytes += 2;
            continue;
        }
        if (unit >= 0xd800u && unit <= 0xdbffu) {
            if (index + 1 >= range.length) {
                return false;
            }
            UniChar low = string->contents[range.location + index + 1];
            if (low < 0xdc00u || low > 0xdfffu) {
                return false;
            }
            totalBytes += 4;
            index += 1;
            continue;
        }
        if (unit >= 0xdc00u && unit <= 0xdfffu) {
            return false;
        }
        totalBytes += 3;
    }

    if (byteCount != NULL) {
        *byteCount = totalBytes;
    }
    return true;
}

static Boolean __CFStringEncodeUTF8(const struct __CFString *string, CFRange range, UInt8 *buffer, CFIndex bufferSize, CFIndex *usedBytes) {
    CFIndex out = 0;

    for (CFIndex index = 0; index < range.length; ++index) {
        UniChar unit = string->contents[range.location + index];
        UInt32 codePoint = unit;
        UInt8 encoded[4];
        CFIndex encodedCount = 0;

        if (unit <= 0x7fu) {
            encoded[0] = (UInt8)unit;
            encodedCount = 1;
        } else if (unit <= 0x7ffu) {
            encoded[0] = (UInt8)(0xc0u | (unit >> 6u));
            encoded[1] = (UInt8)(0x80u | (unit & 0x3fu));
            encodedCount = 2;
        } else if (unit >= 0xd800u && unit <= 0xdbffu) {
            if (index + 1 >= range.length) {
                return false;
            }
            UniChar low = string->contents[range.location + index + 1];
            if (low < 0xdc00u || low > 0xdfffu) {
                return false;
            }
            codePoint = 0x10000u + ((((UInt32)unit - 0xd800u) << 10u) | ((UInt32)low - 0xdc00u));
            encoded[0] = (UInt8)(0xf0u | (codePoint >> 18u));
            encoded[1] = (UInt8)(0x80u | ((codePoint >> 12u) & 0x3fu));
            encoded[2] = (UInt8)(0x80u | ((codePoint >> 6u) & 0x3fu));
            encoded[3] = (UInt8)(0x80u | (codePoint & 0x3fu));
            encodedCount = 4;
            index += 1;
        } else if (unit >= 0xdc00u && unit <= 0xdfffu) {
            return false;
        } else {
            encoded[0] = (UInt8)(0xe0u | (unit >> 12u));
            encoded[1] = (UInt8)(0x80u | ((unit >> 6u) & 0x3fu));
            encoded[2] = (UInt8)(0x80u | (unit & 0x3fu));
            encodedCount = 3;
        }

        if (buffer != NULL) {
            if (out + encodedCount > bufferSize) {
                return false;
            }
            memcpy(buffer + out, encoded, (size_t)encodedCount);
        }
        out += encodedCount;
    }

    if (usedBytes != NULL) {
        *usedBytes = out;
    }
    return true;
}

static CFStringRef __CFStringCreateWithCodeUnits(CFAllocatorRef allocator, const UniChar *chars, CFIndex numChars) {
    if (numChars < 0) {
        return NULL;
    }
    if (numChars > 0 && chars == NULL) {
        return NULL;
    }

    size_t headerSize = sizeof(struct __CFString) - sizeof(CFRuntimeBase);
    size_t unitBytes = (size_t)numChars * sizeof(UniChar);
    if (unitBytes > (size_t)LONG_MAX || headerSize > (size_t)LONG_MAX - unitBytes) {
        return NULL;
    }

    CFIndex extraBytes = (CFIndex)(headerSize + unitBytes);
    struct __CFString *string = (struct __CFString *)_CFRuntimeCreateInstance(allocator, CFStringGetTypeID(), extraBytes, NULL);
    if (string == NULL) {
        return NULL;
    }

    string->length = numChars;
    string->flags = FX_CFSTRING_FLAG_ASCII;
    for (CFIndex index = 0; index < numChars; ++index) {
        UniChar unit = chars[index];
        string->contents[index] = unit;
        if (unit > 0x7fu) {
            string->flags &= ~FX_CFSTRING_FLAG_ASCII;
        }
    }

    return string;
}

CFTypeID CFStringGetTypeID(void) {
    return _kCFRuntimeIDCFString;
}

CFStringRef CFStringCreateWithCString(CFAllocatorRef allocator, const char *cStr, CFStringEncoding encoding) {
    if (cStr == NULL) {
        return NULL;
    }
    return CFStringCreateWithBytes(allocator, (const UInt8 *)cStr, (CFIndex)strlen(cStr), encoding, false);
}

CFStringRef CFStringCreateWithBytes(CFAllocatorRef allocator, const UInt8 *bytes, CFIndex numBytes, CFStringEncoding encoding, Boolean isExternalRepresentation) {
    if (numBytes < 0) {
        return NULL;
    }
    if (numBytes > 0 && bytes == NULL) {
        return NULL;
    }
    if (isExternalRepresentation) {
        return NULL;
    }

    if (encoding == kCFStringEncodingASCII) {
        if (numBytes == 0) {
            return __CFStringCreateWithCodeUnits(allocator, NULL, 0);
        }

        UniChar *chars = (UniChar *)calloc((size_t)numBytes, sizeof(UniChar));
        if (chars == NULL) {
            return NULL;
        }
        for (CFIndex index = 0; index < numBytes; ++index) {
            chars[index] = bytes[index];
        }
        CFStringRef string = __CFStringCreateWithCodeUnits(allocator, chars, numBytes);
        free(chars);
        return string;
    }

    if (encoding == kCFStringEncodingUTF8) {
        CFIndex unitCount = 0;
        if (!__CFStringUTF8CountCodeUnits(bytes, numBytes, &unitCount)) {
            return NULL;
        }

        UniChar *chars = NULL;
        if (unitCount > 0) {
            chars = (UniChar *)calloc((size_t)unitCount, sizeof(UniChar));
            if (chars == NULL) {
                return NULL;
            }
            if (!__CFStringDecodeUTF8(bytes, numBytes, chars, unitCount)) {
                free(chars);
                return NULL;
            }
        }

        CFStringRef string = __CFStringCreateWithCodeUnits(allocator, chars, unitCount);
        free(chars);
        return string;
    }

    return NULL;
}

CFStringRef CFStringCreateWithCharacters(CFAllocatorRef allocator, const UniChar *chars, CFIndex numChars) {
    return __CFStringCreateWithCodeUnits(allocator, chars, numChars);
}

CFStringRef CFStringCreateCopy(CFAllocatorRef allocator, CFStringRef theString) {
    _FXCFValidateType((CFTypeRef)theString, CFStringGetTypeID(), "CFStringCreateCopy called with non-string object");
    const struct __CFString *string = (const struct __CFString *)theString;
    return __CFStringCreateWithCodeUnits(allocator, string->length > 0 ? string->contents : NULL, string->length);
}

CFIndex CFStringGetLength(CFStringRef theString) {
    _FXCFValidateType((CFTypeRef)theString, CFStringGetTypeID(), "CFStringGetLength called with non-string object");
    return ((const struct __CFString *)theString)->length;
}

void CFStringGetCharacters(CFStringRef theString, CFRange range, UniChar *buffer) {
    _FXCFValidateType((CFTypeRef)theString, CFStringGetTypeID(), "CFStringGetCharacters called with non-string object");
    const struct __CFString *string = (const struct __CFString *)theString;
    if (!__CFStringRangeIsValid(string, range)) {
        _FXCFAbort("CFStringGetCharacters called with invalid range");
    }
    if (range.length > 0 && buffer == NULL) {
        _FXCFAbort("CFStringGetCharacters called with NULL buffer");
    }
    if (range.length > 0) {
        memcpy(buffer, string->contents + range.location, (size_t)range.length * sizeof(UniChar));
    }
}

Boolean CFStringGetCString(CFStringRef theString, char *buffer, CFIndex bufferSize, CFStringEncoding encoding) {
    _FXCFValidateType((CFTypeRef)theString, CFStringGetTypeID(), "CFStringGetCString called with non-string object");
    const struct __CFString *string = (const struct __CFString *)theString;
    if (buffer == NULL || bufferSize <= 0) {
        return false;
    }

    CFRange range = CFRangeMake(0, string->length);
    if (encoding == kCFStringEncodingASCII) {
        if (!__CFStringIsASCII(string)) {
            return false;
        }
        if (range.length + 1 > bufferSize) {
            return false;
        }
        for (CFIndex index = 0; index < range.length; ++index) {
            buffer[index] = (char)string->contents[index];
        }
        buffer[range.length] = '\0';
        return true;
    }

    if (encoding != kCFStringEncodingUTF8) {
        return false;
    }

    if (__CFStringIsASCII(string)) {
        if (range.length + 1 > bufferSize) {
            return false;
        }
        for (CFIndex index = 0; index < range.length; ++index) {
            buffer[index] = (char)string->contents[index];
        }
        buffer[range.length] = '\0';
        return true;
    }

    CFIndex requiredBytes = 0;
    if (!__CFStringUTF8EncodedLength(string, range, &requiredBytes)) {
        return false;
    }
    if (requiredBytes + 1 > bufferSize) {
        return false;
    }
    if (!__CFStringEncodeUTF8(string, range, (UInt8 *)buffer, bufferSize - 1, NULL)) {
        return false;
    }
    buffer[requiredBytes] = '\0';
    return true;
}

CFIndex CFStringGetBytes(CFStringRef theString, CFRange range, CFStringEncoding encoding, UInt8 lossByte, Boolean isExternalRepresentation, UInt8 *buffer, CFIndex maxBufLen, CFIndex *usedBufLen) {
    _FXCFValidateType((CFTypeRef)theString, CFStringGetTypeID(), "CFStringGetBytes called with non-string object");
    const struct __CFString *string = (const struct __CFString *)theString;
    (void)lossByte;
    if (usedBufLen != NULL) {
        *usedBufLen = 0;
    }
    if (isExternalRepresentation) {
        return 0;
    }
    if (!__CFStringRangeIsValid(string, range)) {
        _FXCFAbort("CFStringGetBytes called with invalid range");
    }
    if (encoding != kCFStringEncodingASCII) {
        return 0;
    }

    CFIndex converted = 0;
    CFIndex used = 0;
    for (CFIndex index = 0; index < range.length; ++index) {
        UniChar unit = string->contents[range.location + index];
        if (unit > 0x7fu) {
            break;
        }
        if (buffer != NULL) {
            if (used >= maxBufLen) {
                break;
            }
            buffer[used] = (UInt8)unit;
        }
        used += 1;
        converted += 1;
    }

    if (usedBufLen != NULL) {
        *usedBufLen = used;
    }
    return converted;
}
