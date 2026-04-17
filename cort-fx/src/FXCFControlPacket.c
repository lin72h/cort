#include "FXCFControlPacket.h"

#include <CoreFoundation/CoreFoundation.h>

#include <math.h>
#include <stdarg.h>
#include <time.h>

enum {
    FX_CF_CONTROL_PACKET_UNIX_EPOCH_DELTA = 978307200
};

static const UInt8 kFXCFBinaryPlistHeader[] = {
    'b', 'p', 'l', 'i', 's', 't', '0', '0'
};

struct __FXCFTextBuffer {
    char *bytes;
    size_t length;
    size_t capacity;
};

static void __FXCFControlPacketSetError(struct _FXCFControlPacketError *error, enum _FXCFControlPacketErrorKind kind, CFIndex version, const char *text) {
    if (error == NULL) {
        return;
    }
    error->kind = kind;
    error->version = version;
    if (text == NULL) {
        error->text[0] = '\0';
        return;
    }
    snprintf(error->text, sizeof(error->text), "%s", text);
}

static Boolean __FXCFAddSize(size_t left, size_t right, size_t *out) {
    if (SIZE_MAX - left < right) {
        return false;
    }
    *out = left + right;
    return true;
}

static Boolean __FXCFTextBufferReserve(struct __FXCFTextBuffer *buffer, size_t additional) {
    size_t required = 0u;
    char *newBytes = NULL;

    if (!__FXCFAddSize(buffer->length, additional, &required)) {
        return false;
    }
    if (required <= buffer->capacity) {
        return true;
    }

    size_t newCapacity = buffer->capacity > 0u ? buffer->capacity : 256u;
    while (newCapacity < required) {
        if (newCapacity > SIZE_MAX / 2u) {
            newCapacity = required;
            break;
        }
        newCapacity *= 2u;
    }

    newBytes = (char *)realloc(buffer->bytes, newCapacity);
    if (newBytes == NULL) {
        return false;
    }

    buffer->bytes = newBytes;
    buffer->capacity = newCapacity;
    return true;
}

static Boolean __FXCFTextBufferAppendBytes(struct __FXCFTextBuffer *buffer, const char *text, size_t length) {
    if (length == 0u) {
        return true;
    }
    if (text == NULL) {
        return false;
    }
    if (!__FXCFTextBufferReserve(buffer, length + 1u)) {
        return false;
    }
    memcpy(buffer->bytes + buffer->length, text, length);
    buffer->length += length;
    buffer->bytes[buffer->length] = '\0';
    return true;
}

static Boolean __FXCFTextBufferAppendCString(struct __FXCFTextBuffer *buffer, const char *text) {
    return __FXCFTextBufferAppendBytes(buffer, text, text != NULL ? strlen(text) : 0u);
}

static Boolean __FXCFTextBufferAppendFormat(struct __FXCFTextBuffer *buffer, const char *format, ...) {
    va_list args;
    va_list copy;
    int written = 0;

    va_start(args, format);
    va_copy(copy, args);
    written = vsnprintf(NULL, 0, format, copy);
    va_end(copy);
    if (written < 0) {
        va_end(args);
        return false;
    }
    if (!__FXCFTextBufferReserve(buffer, (size_t)written + 1u)) {
        va_end(args);
        return false;
    }
    (void)vsnprintf(buffer->bytes + buffer->length, buffer->capacity - buffer->length, format, args);
    va_end(args);
    buffer->length += (size_t)written;
    return true;
}

static Boolean __FXCFTextBufferAppendJSONString(struct __FXCFTextBuffer *buffer, const char *text) {
    const unsigned char *cursor = (const unsigned char *)text;

    if (!__FXCFTextBufferAppendCString(buffer, "\"")) {
        return false;
    }
    if (cursor != NULL) {
        while (*cursor != '\0') {
            switch (*cursor) {
                case '\\':
                    if (!__FXCFTextBufferAppendCString(buffer, "\\\\")) {
                        return false;
                    }
                    break;
                case '"':
                    if (!__FXCFTextBufferAppendCString(buffer, "\\\"")) {
                        return false;
                    }
                    break;
                case '\n':
                    if (!__FXCFTextBufferAppendCString(buffer, "\\n")) {
                        return false;
                    }
                    break;
                case '\r':
                    if (!__FXCFTextBufferAppendCString(buffer, "\\r")) {
                        return false;
                    }
                    break;
                case '\t':
                    if (!__FXCFTextBufferAppendCString(buffer, "\\t")) {
                        return false;
                    }
                    break;
                default:
                    if (*cursor < 0x20u) {
                        if (!__FXCFTextBufferAppendFormat(buffer, "\\u%04x", (unsigned int)*cursor)) {
                            return false;
                        }
                    } else {
                        if (!__FXCFTextBufferReserve(buffer, 2u)) {
                            return false;
                        }
                        buffer->bytes[buffer->length++] = (char)*cursor;
                        buffer->bytes[buffer->length] = '\0';
                    }
                    break;
            }
            ++cursor;
        }
    }
    return __FXCFTextBufferAppendCString(buffer, "\"");
}

static char *__FXCFTextBufferDetach(struct __FXCFTextBuffer *buffer) {
    if (buffer->bytes == NULL) {
        buffer->bytes = (char *)malloc(1u);
        if (buffer->bytes == NULL) {
            return NULL;
        }
        buffer->bytes[0] = '\0';
    }
    char *result = buffer->bytes;
    buffer->bytes = NULL;
    buffer->length = 0u;
    buffer->capacity = 0u;
    return result;
}

static void __FXCFTextBufferFree(struct __FXCFTextBuffer *buffer) {
    free(buffer->bytes);
    buffer->bytes = NULL;
    buffer->length = 0u;
    buffer->capacity = 0u;
}

static char *__FXCFControlPacketCopyCString(CFStringRef string) {
    CFIndex length = 0;
    CFIndex capacity = 0;
    char *buffer = NULL;

    if (string == NULL) {
        return NULL;
    }

    length = CFStringGetLength(string);
    if (length < 0) {
        return NULL;
    }
    capacity = (length * 4) + 1;
    if (capacity < 1) {
        capacity = 1;
    }

    buffer = (char *)calloc((size_t)capacity, sizeof(char));
    if (buffer == NULL) {
        return NULL;
    }
    if (!CFStringGetCString(string, buffer, capacity, kCFStringEncodingUTF8)) {
        free(buffer);
        return NULL;
    }
    return buffer;
}

static Boolean __FXCFControlPacketStringEqualsCString(CFStringRef string, const char *expected) {
    char *text = NULL;
    Boolean matches = false;

    if (string == NULL || expected == NULL) {
        return false;
    }

    text = __FXCFControlPacketCopyCString(string);
    if (text == NULL) {
        return false;
    }
    matches = strcmp(text, expected) == 0;
    free(text);
    return matches;
}

static Boolean __FXCFTextBufferAppendJSONFieldPrefix(
    struct __FXCFTextBuffer *buffer,
    const char *key,
    Boolean *needsComma
) {
    if (*needsComma && !__FXCFTextBufferAppendCString(buffer, ",")) {
        return false;
    }
    if (!__FXCFTextBufferAppendJSONString(buffer, key) ||
        !__FXCFTextBufferAppendCString(buffer, ":")) {
        return false;
    }
    *needsComma = true;
    return true;
}

static int __FXCFControlPacketCompareDictionaryEntries(const void *left, const void *right) {
    const struct __FXCFDictionarySortEntry {
        const char *keyText;
        CFTypeRef value;
    } *entryLeft = (const struct __FXCFDictionarySortEntry *)left;
    const struct __FXCFDictionarySortEntry *entryRight = (const struct __FXCFDictionarySortEntry *)right;
    return strcmp(entryLeft->keyText, entryRight->keyText);
}

static int __FXCFControlPacketCompareCStringPointers(const void *left, const void *right) {
    const char *const *leftText = (const char *const *)left;
    const char *const *rightText = (const char *const *)right;
    return strcmp(*leftText, *rightText);
}

static void __FXCFControlPacketFreeSortedKeys(char **keys, CFIndex count) {
    if (keys == NULL) {
        return;
    }
    for (CFIndex index = 0; index < count; ++index) {
        free(keys[index]);
    }
    free(keys);
}

static Boolean __FXCFControlPacketCopySortedKeys(CFDictionaryRef dictionary, char ***keysOut, CFIndex *countOut) {
    CFIndex count = CFDictionaryGetCount(dictionary);
    char **keys = NULL;

    if (keysOut == NULL || countOut == NULL) {
        return false;
    }

    *keysOut = NULL;
    *countOut = 0;

    if (count == 0) {
        return true;
    }

    keys = (char **)calloc((size_t)count, sizeof(char *));
    if (keys == NULL) {
        return false;
    }

    for (CFIndex index = 0; index < count; ++index) {
        const void *key = NULL;
        const void *value = NULL;

        if (!_FXCFDictionaryGetEntryAtIndex(dictionary, index, &key, &value)) {
            __FXCFControlPacketFreeSortedKeys(keys, count);
            return false;
        }
        (void)value;

        keys[index] = __FXCFControlPacketCopyCString((CFStringRef)key);
        if (keys[index] == NULL) {
            __FXCFControlPacketFreeSortedKeys(keys, count);
            return false;
        }
    }

    qsort(keys, (size_t)count, sizeof(char *), __FXCFControlPacketCompareCStringPointers);
    *keysOut = keys;
    *countOut = count;
    return true;
}

static Boolean __FXCFControlPacketAppendKeyArrayJSON(struct __FXCFTextBuffer *buffer, char *const *keys, CFIndex count) {
    if (!__FXCFTextBufferAppendCString(buffer, "[")) {
        return false;
    }
    for (CFIndex index = 0; index < count; ++index) {
        if (index > 0 && !__FXCFTextBufferAppendCString(buffer, ",")) {
            return false;
        }
        if (!__FXCFTextBufferAppendJSONString(buffer, keys[index])) {
            return false;
        }
    }
    return __FXCFTextBufferAppendCString(buffer, "]");
}

static Boolean __FXCFControlPacketDataIsUTF8(const UInt8 *bytes, CFIndex length) {
    CFIndex index = 0;

    while (index < length) {
        UInt8 byte = bytes[index];

        if (byte <= 0x7fu) {
            ++index;
            continue;
        }

        if ((byte & 0xe0u) == 0xc0u) {
            if (index + 1 >= length) {
                return false;
            }
            UInt8 next = bytes[index + 1];
            if (byte < 0xc2u || (next & 0xc0u) != 0x80u) {
                return false;
            }
            index += 2;
            continue;
        }

        if ((byte & 0xf0u) == 0xe0u) {
            if (index + 2 >= length) {
                return false;
            }
            UInt8 next1 = bytes[index + 1];
            UInt8 next2 = bytes[index + 2];
            if ((next1 & 0xc0u) != 0x80u || (next2 & 0xc0u) != 0x80u) {
                return false;
            }
            if ((byte == 0xe0u && next1 < 0xa0u) || (byte == 0xedu && next1 > 0x9fu)) {
                return false;
            }
            index += 3;
            continue;
        }

        if ((byte & 0xf8u) == 0xf0u) {
            if (index + 3 >= length) {
                return false;
            }
            UInt8 next1 = bytes[index + 1];
            UInt8 next2 = bytes[index + 2];
            UInt8 next3 = bytes[index + 3];
            if ((next1 & 0xc0u) != 0x80u || (next2 & 0xc0u) != 0x80u || (next3 & 0xc0u) != 0x80u) {
                return false;
            }
            if ((byte == 0xf0u && next1 < 0x90u) || (byte == 0xf4u && next1 > 0x8fu) || byte > 0xf4u) {
                return false;
            }
            index += 4;
            continue;
        }

        return false;
    }

    return true;
}

static Boolean __FXCFControlPacketAppendBase64(struct __FXCFTextBuffer *buffer, const UInt8 *bytes, CFIndex length) {
    static const char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    CFIndex index = 0;

    while (index < length) {
        uint32_t block = 0u;
        int pad = 0;

        block |= (uint32_t)bytes[index] << 16u;
        if (index + 1 < length) {
            block |= (uint32_t)bytes[index + 1] << 8u;
        } else {
            pad += 1;
        }
        if (index + 2 < length) {
            block |= (uint32_t)bytes[index + 2];
        } else {
            pad += 1;
        }

        if (!__FXCFTextBufferReserve(buffer, 5u)) {
            return false;
        }
        buffer->bytes[buffer->length++] = alphabet[(block >> 18u) & 0x3fu];
        buffer->bytes[buffer->length++] = alphabet[(block >> 12u) & 0x3fu];
        buffer->bytes[buffer->length++] = pad >= 2 ? '=' : alphabet[(block >> 6u) & 0x3fu];
        buffer->bytes[buffer->length++] = pad >= 1 ? '=' : alphabet[block & 0x3fu];
        buffer->bytes[buffer->length] = '\0';
        index += 3;
    }

    return true;
}

static Boolean __FXCFControlPacketAppendISODate(struct __FXCFTextBuffer *buffer, CFDateRef date) {
    double unixTime = CFDateGetAbsoluteTime(date) + (double)FX_CF_CONTROL_PACKET_UNIX_EPOCH_DELTA;
    double integral = 0.0;
    double fractional = modf(unixTime, &integral);
    time_t seconds = (time_t)integral;
    struct tm utc;
    char stamp[48];

    if (gmtime_r(&seconds, &utc) == NULL) {
        return false;
    }
    if (strftime(stamp, sizeof(stamp), "%Y-%m-%dT%H:%M:%S", &utc) == 0u) {
        return false;
    }
    if (fabs(fractional) < 1e-9) {
        return __FXCFTextBufferAppendFormat(buffer, "{\"$date\":\"%sZ\"}", stamp);
    }

    long millis = (long)((fractional >= 0.0 ? (fractional * 1000.0) + 0.5 : (-fractional * 1000.0) + 0.5));
    if (millis < 0) {
        millis = -millis;
    }
    return __FXCFTextBufferAppendFormat(buffer, "{\"$date\":\"%s.%03ldZ\"}", stamp, millis);
}

static Boolean __FXCFControlPacketAppendValueJSON(struct __FXCFTextBuffer *buffer, CFTypeRef value);

static Boolean __FXCFControlPacketAppendArrayJSON(struct __FXCFTextBuffer *buffer, CFArrayRef array) {
    CFIndex count = CFArrayGetCount(array);
    CFIndex index = 0;

    if (!__FXCFTextBufferAppendCString(buffer, "[")) {
        return false;
    }
    for (index = 0; index < count; ++index) {
        if (index > 0 && !__FXCFTextBufferAppendCString(buffer, ",")) {
            return false;
        }
        if (!__FXCFControlPacketAppendValueJSON(buffer, (CFTypeRef)CFArrayGetValueAtIndex(array, index))) {
            return false;
        }
    }
    return __FXCFTextBufferAppendCString(buffer, "]");
}

static Boolean __FXCFControlPacketAppendDictionaryJSON(struct __FXCFTextBuffer *buffer, CFDictionaryRef dictionary) {
    struct __FXCFDictionarySortEntry {
        const char *keyText;
        CFTypeRef value;
    };
    CFIndex count = CFDictionaryGetCount(dictionary);
    CFIndex index = 0;
    const struct __FXCFDictionarySortEntry *unused = NULL;
    struct __FXCFDictionarySortEntry *entries = NULL;

    (void)unused;

    if (!__FXCFTextBufferAppendCString(buffer, "{")) {
        return false;
    }

    if (count > 0) {
        entries = (struct __FXCFDictionarySortEntry *)calloc((size_t)count, sizeof(struct __FXCFDictionarySortEntry));
        if (entries == NULL) {
            return false;
        }

        for (index = 0; index < count; ++index) {
            const void *key = NULL;
            const void *value = NULL;

            if (!_FXCFDictionaryGetEntryAtIndex(dictionary, index, &key, &value)) {
                goto fail;
            }

            entries[index].keyText = __FXCFControlPacketCopyCString((CFStringRef)key);
            entries[index].value = (CFTypeRef)value;
            if (entries[index].keyText == NULL) {
                goto fail;
            }
        }

        qsort(entries, (size_t)count, sizeof(struct __FXCFDictionarySortEntry), __FXCFControlPacketCompareDictionaryEntries);

        for (index = 0; index < count; ++index) {
            if (index > 0 && !__FXCFTextBufferAppendCString(buffer, ",")) {
                goto fail;
            }
            if (!__FXCFTextBufferAppendJSONString(buffer, entries[index].keyText) ||
                !__FXCFTextBufferAppendCString(buffer, ":") ||
                !__FXCFControlPacketAppendValueJSON(buffer, entries[index].value)) {
                goto fail;
            }
        }
    }

    for (index = 0; index < count; ++index) {
        free((void *)entries[index].keyText);
    }
    free(entries);
    return __FXCFTextBufferAppendCString(buffer, "}");

fail:
    for (CFIndex freeIndex = 0; freeIndex < count; ++freeIndex) {
        free((void *)entries[freeIndex].keyText);
    }
    free(entries);
    return false;
}

static Boolean __FXCFControlPacketAppendNumberJSON(struct __FXCFTextBuffer *buffer, CFNumberRef number) {
    if (CFNumberGetType(number) == kCFNumberFloat64Type) {
        double value = 0.0;
        if (!CFNumberGetValue(number, kCFNumberFloat64Type, &value)) {
            return false;
        }
        return __FXCFTextBufferAppendFormat(buffer, "%.17g", value);
    }

    SInt64 value = 0;
    if (!CFNumberGetValue(number, kCFNumberSInt64Type, &value)) {
        return false;
    }
    return __FXCFTextBufferAppendFormat(buffer, "%lld", (long long)value);
}

static Boolean __FXCFControlPacketAppendDataJSON(struct __FXCFTextBuffer *buffer, CFDataRef data) {
    const UInt8 *bytes = CFDataGetBytePtr(data);
    CFIndex length = CFDataGetLength(data);

    if (bytes == NULL && length != 0) {
        return false;
    }

    if (__FXCFControlPacketDataIsUTF8(bytes, length)) {
        char *text = (char *)calloc((size_t)length + 1u, sizeof(char));
        if (text == NULL) {
            return false;
        }
        if (length > 0) {
            memcpy(text, bytes, (size_t)length);
        }
        Boolean ok = __FXCFTextBufferAppendCString(buffer, "{\"$data_utf8\":") &&
            __FXCFTextBufferAppendJSONString(buffer, text) &&
            __FXCFTextBufferAppendCString(buffer, "}");
        free(text);
        return ok;
    }

    if (!__FXCFTextBufferAppendCString(buffer, "{\"$data_base64\":\"")) {
        return false;
    }
    if (!__FXCFControlPacketAppendBase64(buffer, bytes, length)) {
        return false;
    }
    return __FXCFTextBufferAppendCString(buffer, "\"}");
}

static Boolean __FXCFControlPacketAppendValueJSON(struct __FXCFTextBuffer *buffer, CFTypeRef value) {
    CFTypeID typeID = 0;

    if (value == NULL) {
        return __FXCFTextBufferAppendJSONString(buffer, "nil");
    }

    typeID = CFGetTypeID(value);
    if (typeID == CFStringGetTypeID()) {
        char *text = __FXCFControlPacketCopyCString((CFStringRef)value);
        Boolean ok = text != NULL && __FXCFTextBufferAppendJSONString(buffer, text);
        free(text);
        return ok;
    }
    if (typeID == CFBooleanGetTypeID()) {
        return __FXCFTextBufferAppendCString(buffer, CFBooleanGetValue((CFBooleanRef)value) ? "true" : "false");
    }
    if (typeID == CFNumberGetTypeID()) {
        return __FXCFControlPacketAppendNumberJSON(buffer, (CFNumberRef)value);
    }
    if (typeID == CFDateGetTypeID()) {
        return __FXCFControlPacketAppendISODate(buffer, (CFDateRef)value);
    }
    if (typeID == CFDataGetTypeID()) {
        return __FXCFControlPacketAppendDataJSON(buffer, (CFDataRef)value);
    }
    if (typeID == CFArrayGetTypeID()) {
        return __FXCFControlPacketAppendArrayJSON(buffer, (CFArrayRef)value);
    }
    if (typeID == CFDictionaryGetTypeID()) {
        return __FXCFControlPacketAppendDictionaryJSON(buffer, (CFDictionaryRef)value);
    }

    return false;
}

Boolean _FXCFControlPacketIsBinaryPlist(CFDataRef payload) {
    const UInt8 *bytes = NULL;

    if (payload == NULL) {
        return false;
    }
    if (CFDataGetLength(payload) < (CFIndex)sizeof(kFXCFBinaryPlistHeader)) {
        return false;
    }

    bytes = CFDataGetBytePtr(payload);
    if (bytes == NULL) {
        return false;
    }
    return memcmp(bytes, kFXCFBinaryPlistHeader, sizeof(kFXCFBinaryPlistHeader)) == 0;
}

enum _FXCFControlValueKind _FXCFControlPacketValueKind(CFTypeRef value) {
    CFTypeID typeID = 0;

    if (value == NULL) {
        return _FXCFControlValueKindInvalid;
    }

    typeID = CFGetTypeID(value);
    if (typeID == CFDictionaryGetTypeID()) {
        return _FXCFControlValueKindObject;
    }
    if (typeID == CFArrayGetTypeID()) {
        return _FXCFControlValueKindArray;
    }
    if (typeID == CFStringGetTypeID()) {
        return _FXCFControlValueKindString;
    }
    if (typeID == CFBooleanGetTypeID()) {
        return _FXCFControlValueKindBool;
    }
    if (typeID == CFNumberGetTypeID()) {
        return CFNumberGetType((CFNumberRef)value) == kCFNumberFloat64Type
            ? _FXCFControlValueKindDouble
            : _FXCFControlValueKindInteger;
    }
    if (typeID == CFDateGetTypeID()) {
        return _FXCFControlValueKindDate;
    }
    if (typeID == CFDataGetTypeID()) {
        return _FXCFControlValueKindData;
    }
    return _FXCFControlValueKindInvalid;
}

char *_FXCFControlPacketCopyCanonicalJSON(CFTypeRef value) {
    struct __FXCFTextBuffer buffer = {0};
    char *result = NULL;

    if (!__FXCFControlPacketAppendValueJSON(&buffer, value)) {
        __FXCFTextBufferFree(&buffer);
        return NULL;
    }
    result = __FXCFTextBufferDetach(&buffer);
    __FXCFTextBufferFree(&buffer);
    return result;
}

static Boolean __FXCFControlPacketRequireInteger(CFDictionaryRef dictionary, const char *keyText, CFIndex *valueOut, struct _FXCFControlPacketError *error) {
    CFStringRef key = NULL;
    const void *raw = NULL;
    SInt64 integerValue = 0;
    Boolean exact = false;
    Boolean ok = false;

    key = CFStringCreateWithCString(kCFAllocatorSystemDefault, keyText, kCFStringEncodingASCII);
    if (key == NULL) {
        __FXCFControlPacketSetError(error, _FXCFControlPacketErrorInvalidPacket, 0, "failed to create lookup key");
        return false;
    }

    ok = CFDictionaryGetValueIfPresent(dictionary, key, &raw);
    CFRelease((CFTypeRef)key);
    if (!ok || raw == NULL || CFGetTypeID((CFTypeRef)raw) != CFNumberGetTypeID() || CFGetTypeID((CFTypeRef)raw) == CFBooleanGetTypeID()) {
        __FXCFControlPacketSetError(error, _FXCFControlPacketErrorMissingKey, 0, keyText);
        return false;
    }

    if (CFNumberGetType((CFNumberRef)raw) == kCFNumberFloat64Type) {
        __FXCFControlPacketSetError(error, _FXCFControlPacketErrorMissingKey, 0, keyText);
        return false;
    }

    exact = CFNumberGetValue((CFNumberRef)raw, kCFNumberSInt64Type, &integerValue);
    if (!exact) {
        __FXCFControlPacketSetError(error, _FXCFControlPacketErrorMissingKey, 0, keyText);
        return false;
    }

    *valueOut = (CFIndex)integerValue;
    return true;
}

static Boolean __FXCFControlPacketRequireString(CFDictionaryRef dictionary, const char *keyText, CFStringRef *valueOut, struct _FXCFControlPacketError *error) {
    CFStringRef key = NULL;
    const void *raw = NULL;
    Boolean ok = false;

    key = CFStringCreateWithCString(kCFAllocatorSystemDefault, keyText, kCFStringEncodingASCII);
    if (key == NULL) {
        __FXCFControlPacketSetError(error, _FXCFControlPacketErrorInvalidPacket, 0, "failed to create lookup key");
        return false;
    }

    ok = CFDictionaryGetValueIfPresent(dictionary, key, &raw);
    CFRelease((CFTypeRef)key);
    if (!ok || raw == NULL || CFGetTypeID((CFTypeRef)raw) != CFStringGetTypeID()) {
        __FXCFControlPacketSetError(error, _FXCFControlPacketErrorMissingKey, 0, keyText);
        return false;
    }

    *valueOut = (CFStringRef)raw;
    return true;
}

static Boolean __FXCFControlPacketRequireDictionary(CFDictionaryRef dictionary, const char *keyText, CFDictionaryRef *valueOut, struct _FXCFControlPacketError *error) {
    CFStringRef key = NULL;
    const void *raw = NULL;
    Boolean ok = false;

    key = CFStringCreateWithCString(kCFAllocatorSystemDefault, keyText, kCFStringEncodingASCII);
    if (key == NULL) {
        __FXCFControlPacketSetError(error, _FXCFControlPacketErrorInvalidPacket, 0, "failed to create lookup key");
        return false;
    }

    ok = CFDictionaryGetValueIfPresent(dictionary, key, &raw);
    CFRelease((CFTypeRef)key);
    if (!ok || raw == NULL || CFGetTypeID((CFTypeRef)raw) != CFDictionaryGetTypeID()) {
        __FXCFControlPacketSetError(error, _FXCFControlPacketErrorMissingKey, 0, keyText);
        return false;
    }

    *valueOut = (CFDictionaryRef)raw;
    return true;
}

static Boolean __FXCFControlPacketRequireArray(CFDictionaryRef dictionary, const char *keyText, CFArrayRef *valueOut, struct _FXCFControlPacketError *error) {
    CFStringRef key = NULL;
    const void *raw = NULL;
    Boolean ok = false;

    key = CFStringCreateWithCString(kCFAllocatorSystemDefault, keyText, kCFStringEncodingASCII);
    if (key == NULL) {
        __FXCFControlPacketSetError(error, _FXCFControlPacketErrorInvalidPacket, 0, "failed to create lookup key");
        return false;
    }

    ok = CFDictionaryGetValueIfPresent(dictionary, key, &raw);
    CFRelease((CFTypeRef)key);
    if (!ok || raw == NULL || CFGetTypeID((CFTypeRef)raw) != CFArrayGetTypeID()) {
        __FXCFControlPacketSetError(error, _FXCFControlPacketErrorMissingKey, 0, keyText);
        return false;
    }

    *valueOut = (CFArrayRef)raw;
    return true;
}

static Boolean __FXCFControlPacketRequireValue(CFDictionaryRef dictionary, const char *keyText, CFTypeRef *valueOut, struct _FXCFControlPacketError *error) {
    CFStringRef key = NULL;
    const void *raw = NULL;
    Boolean ok = false;

    key = CFStringCreateWithCString(kCFAllocatorSystemDefault, keyText, kCFStringEncodingASCII);
    if (key == NULL) {
        __FXCFControlPacketSetError(error, _FXCFControlPacketErrorInvalidPacket, 0, "failed to create lookup key");
        return false;
    }

    ok = CFDictionaryGetValueIfPresent(dictionary, key, &raw);
    CFRelease((CFTypeRef)key);
    if (!ok || raw == NULL) {
        __FXCFControlPacketSetError(error, _FXCFControlPacketErrorMissingKey, 0, keyText);
        return false;
    }

    *valueOut = (CFTypeRef)raw;
    return true;
}

static Boolean __FXCFControlPacketLookupValue(
    CFDictionaryRef dictionary,
    const char *keyText,
    const void **valueOut,
    Boolean *presentOut,
    struct _FXCFControlPacketError *error
);

static Boolean __FXCFControlPacketRequireBoolean(CFDictionaryRef dictionary, const char *keyText, Boolean *valueOut, struct _FXCFControlPacketError *error) {
    const void *raw = NULL;
    Boolean present = false;

    if (valueOut != NULL) {
        *valueOut = false;
    }
    if (!__FXCFControlPacketLookupValue(dictionary, keyText, &raw, &present, error)) {
        return false;
    }
    if (!present || raw == NULL || CFGetTypeID((CFTypeRef)raw) != CFBooleanGetTypeID()) {
        __FXCFControlPacketSetError(error, _FXCFControlPacketErrorMissingKey, 0, keyText);
        return false;
    }
    if (valueOut != NULL) {
        *valueOut = CFBooleanGetValue((CFBooleanRef)raw);
    }
    return true;
}

static Boolean __FXCFControlPacketLookupValue(
    CFDictionaryRef dictionary,
    const char *keyText,
    const void **valueOut,
    Boolean *presentOut,
    struct _FXCFControlPacketError *error
) {
    CFStringRef key = NULL;
    const void *raw = NULL;
    Boolean present = false;

    if (valueOut != NULL) {
        *valueOut = NULL;
    }
    if (presentOut != NULL) {
        *presentOut = false;
    }

    key = CFStringCreateWithCString(kCFAllocatorSystemDefault, keyText, kCFStringEncodingASCII);
    if (key == NULL) {
        __FXCFControlPacketSetError(error, _FXCFControlPacketErrorInvalidPacket, 0, "failed to create lookup key");
        return false;
    }

    present = CFDictionaryGetValueIfPresent(dictionary, key, &raw);
    CFRelease((CFTypeRef)key);

    if (presentOut != NULL) {
        *presentOut = present;
    }
    if (present && valueOut != NULL) {
        *valueOut = raw;
    }
    return true;
}

static Boolean __FXCFControlPacketLookupOptionalString(
    CFDictionaryRef dictionary,
    const char *keyText,
    CFStringRef *valueOut,
    Boolean *presentOut,
    struct _FXCFControlPacketError *error
) {
    const void *raw = NULL;
    Boolean present = false;

    if (valueOut != NULL) {
        *valueOut = NULL;
    }
    if (presentOut != NULL) {
        *presentOut = false;
    }

    if (!__FXCFControlPacketLookupValue(dictionary, keyText, &raw, &present, error)) {
        return false;
    }
    if (!present || raw == NULL) {
        return true;
    }
    if (CFGetTypeID((CFTypeRef)raw) != CFStringGetTypeID()) {
        __FXCFControlPacketSetError(error, _FXCFControlPacketErrorInvalidPacket, 0, keyText);
        return false;
    }

    if (presentOut != NULL) {
        *presentOut = true;
    }
    if (valueOut != NULL) {
        *valueOut = (CFStringRef)raw;
    }
    return true;
}

static Boolean __FXCFControlPacketLookupOptionalDictionary(
    CFDictionaryRef dictionary,
    const char *keyText,
    CFDictionaryRef *valueOut,
    Boolean *presentOut,
    struct _FXCFControlPacketError *error
) {
    const void *raw = NULL;
    Boolean present = false;

    if (valueOut != NULL) {
        *valueOut = NULL;
    }
    if (presentOut != NULL) {
        *presentOut = false;
    }

    if (!__FXCFControlPacketLookupValue(dictionary, keyText, &raw, &present, error)) {
        return false;
    }
    if (!present || raw == NULL) {
        return true;
    }
    if (CFGetTypeID((CFTypeRef)raw) != CFDictionaryGetTypeID()) {
        __FXCFControlPacketSetError(error, _FXCFControlPacketErrorInvalidPacket, 0, keyText);
        return false;
    }

    if (presentOut != NULL) {
        *presentOut = true;
    }
    if (valueOut != NULL) {
        *valueOut = (CFDictionaryRef)raw;
    }
    return true;
}

static Boolean __FXCFControlPacketLookupOptionalDate(
    CFDictionaryRef dictionary,
    const char *keyText,
    CFDateRef *valueOut,
    Boolean *presentOut,
    struct _FXCFControlPacketError *error
) {
    const void *raw = NULL;
    Boolean present = false;

    if (valueOut != NULL) {
        *valueOut = NULL;
    }
    if (presentOut != NULL) {
        *presentOut = false;
    }

    if (!__FXCFControlPacketLookupValue(dictionary, keyText, &raw, &present, error)) {
        return false;
    }
    if (!present || raw == NULL) {
        return true;
    }
    if (CFGetTypeID((CFTypeRef)raw) != CFDateGetTypeID()) {
        __FXCFControlPacketSetError(error, _FXCFControlPacketErrorInvalidPacket, 0, keyText);
        return false;
    }

    if (presentOut != NULL) {
        *presentOut = true;
    }
    if (valueOut != NULL) {
        *valueOut = (CFDateRef)raw;
    }
    return true;
}

static Boolean __FXCFControlPacketLookupOptionalInteger(
    CFDictionaryRef dictionary,
    const char *keyText,
    CFIndex *valueOut,
    Boolean *presentOut,
    struct _FXCFControlPacketError *error
) {
    const void *raw = NULL;
    Boolean present = false;
    SInt64 integerValue = 0;

    if (valueOut != NULL) {
        *valueOut = 0;
    }
    if (presentOut != NULL) {
        *presentOut = false;
    }

    if (!__FXCFControlPacketLookupValue(dictionary, keyText, &raw, &present, error)) {
        return false;
    }
    if (!present || raw == NULL) {
        return true;
    }
    if (CFGetTypeID((CFTypeRef)raw) != CFNumberGetTypeID() ||
        CFGetTypeID((CFTypeRef)raw) == CFBooleanGetTypeID() ||
        CFNumberGetType((CFNumberRef)raw) == kCFNumberFloat64Type ||
        !CFNumberGetValue((CFNumberRef)raw, kCFNumberSInt64Type, &integerValue)) {
        __FXCFControlPacketSetError(error, _FXCFControlPacketErrorInvalidPacket, 0, keyText);
        return false;
    }

    if (presentOut != NULL) {
        *presentOut = true;
    }
    if (valueOut != NULL) {
        *valueOut = (CFIndex)integerValue;
    }
    return true;
}

static Boolean __FXCFControlPacketLookupOptionalBoolean(
    CFDictionaryRef dictionary,
    const char *keyText,
    Boolean *valueOut,
    Boolean *presentOut,
    struct _FXCFControlPacketError *error
) {
    const void *raw = NULL;
    Boolean present = false;

    if (valueOut != NULL) {
        *valueOut = false;
    }
    if (presentOut != NULL) {
        *presentOut = false;
    }

    if (!__FXCFControlPacketLookupValue(dictionary, keyText, &raw, &present, error)) {
        return false;
    }
    if (!present || raw == NULL) {
        return true;
    }
    if (CFGetTypeID((CFTypeRef)raw) != CFBooleanGetTypeID()) {
        __FXCFControlPacketSetError(error, _FXCFControlPacketErrorInvalidPacket, 0, keyText);
        return false;
    }

    if (presentOut != NULL) {
        *presentOut = true;
    }
    if (valueOut != NULL) {
        *valueOut = CFBooleanGetValue((CFBooleanRef)raw);
    }
    return true;
}

static Boolean __FXCFControlPacketValidateRequest(CFDictionaryRef packet, struct _FXCFControlPacketError *error) {
    CFIndex protocolVersion = 0;
    CFStringRef service = NULL;
    CFStringRef method = NULL;
    CFDictionaryRef params = NULL;

    if (!__FXCFControlPacketRequireInteger(packet, "protocol_version", &protocolVersion, error)) {
        return false;
    }
    if (protocolVersion != 1) {
        __FXCFControlPacketSetError(error, _FXCFControlPacketErrorUnsupportedVersion, protocolVersion, "");
        return false;
    }
    if (!__FXCFControlPacketRequireString(packet, "service", &service, error)) {
        return false;
    }
    if (!__FXCFControlPacketRequireString(packet, "method", &method, error)) {
        return false;
    }
    if (!__FXCFControlPacketRequireDictionary(packet, "params", &params, error)) {
        return false;
    }

    (void)service;
    (void)method;
    (void)params;
    return true;
}

static Boolean __FXCFControlPacketValidateResponse(CFDictionaryRef packet, struct _FXCFControlPacketError *error) {
    CFIndex protocolVersion = 0;
    CFStringRef status = NULL;
    char *statusText = NULL;
    Boolean ok = false;

    if (!__FXCFControlPacketRequireInteger(packet, "protocol_version", &protocolVersion, error)) {
        return false;
    }
    if (protocolVersion != 1) {
        __FXCFControlPacketSetError(error, _FXCFControlPacketErrorUnsupportedVersion, protocolVersion, "");
        return false;
    }
    if (!__FXCFControlPacketRequireString(packet, "status", &status, error)) {
        return false;
    }

    statusText = __FXCFControlPacketCopyCString(status);
    if (statusText == NULL) {
        __FXCFControlPacketSetError(error, _FXCFControlPacketErrorInvalidPacket, 0, "failed to decode response status");
        return false;
    }

    if (strcmp(statusText, "ok") == 0) {
        CFTypeRef result = NULL;
        ok = __FXCFControlPacketRequireValue(packet, "result", &result, error);
        (void)result;
    } else if (strcmp(statusText, "error") == 0) {
        CFDictionaryRef payload = NULL;
        CFStringRef code = NULL;
        CFStringRef message = NULL;
        ok =
            __FXCFControlPacketRequireDictionary(packet, "error", &payload, error) &&
            __FXCFControlPacketRequireString(payload, "code", &code, error);
        if (!ok && error != NULL && error->kind == _FXCFControlPacketErrorMissingKey && strcmp(error->text, "code") == 0) {
            snprintf(error->text, sizeof(error->text), "%s", "error.code");
        }
        if (ok) {
            ok = __FXCFControlPacketRequireString(payload, "message", &message, error);
            if (!ok && error != NULL && error->kind == _FXCFControlPacketErrorMissingKey && strcmp(error->text, "message") == 0) {
                snprintf(error->text, sizeof(error->text), "%s", "error.message");
            }
        }
        (void)code;
        (void)message;
    } else {
        __FXCFControlPacketSetError(error, _FXCFControlPacketErrorInvalidPacket, 0, statusText);
        if (error != NULL) {
            char text[sizeof(error->text)];
            snprintf(text, sizeof(text), "unsupported status %s", statusText);
            snprintf(error->text, sizeof(error->text), "%s", text);
        }
        ok = false;
    }

    free(statusText);
    return ok;
}

Boolean _FXCFControlPacketDecodeEnvelope(
    CFAllocatorRef allocator,
    CFDataRef payload,
    enum _FXCFControlPacketKind kind,
    CFDictionaryRef *packetOut,
    struct _FXCFControlPacketError *errorOut
) {
    CFPropertyListFormat format = 0;
    CFErrorRef plistError = NULL;
    CFPropertyListRef root = NULL;

    if (packetOut != NULL) {
        *packetOut = NULL;
    }
    if (errorOut != NULL) {
        memset(errorOut, 0, sizeof(*errorOut));
    }

    root = CFPropertyListCreateWithData(
        allocator,
        payload,
        kCFPropertyListImmutable,
        &format,
        &plistError
    );
    if (root == NULL) {
        CFIndex code = plistError != NULL ? CFErrorGetCode(plistError) : 0;
        char detail[64];
        snprintf(detail, sizeof(detail), "property list decode failed code=%ld", (long)code);
        __FXCFControlPacketSetError(errorOut, _FXCFControlPacketErrorInvalidPacket, 0, detail);
        if (plistError != NULL) {
            CFRelease((CFTypeRef)plistError);
        }
        return false;
    }
    if (plistError != NULL) {
        CFRelease((CFTypeRef)plistError);
    }

    if (CFGetTypeID(root) != CFDictionaryGetTypeID()) {
        __FXCFControlPacketSetError(errorOut, _FXCFControlPacketErrorInvalidPacket, 0, "root property list must be a dictionary");
        CFRelease(root);
        return false;
    }

    if ((kind == _FXCFControlPacketKindRequest && !__FXCFControlPacketValidateRequest((CFDictionaryRef)root, errorOut)) ||
        (kind == _FXCFControlPacketKindResponse && !__FXCFControlPacketValidateResponse((CFDictionaryRef)root, errorOut))) {
        CFRelease(root);
        return false;
    }

    if (packetOut != NULL) {
        *packetOut = (CFDictionaryRef)root;
    } else {
        CFRelease(root);
    }
    return true;
}

static const char *__FXCFControlValueKindText(enum _FXCFControlValueKind kind) {
    switch (kind) {
        case _FXCFControlValueKindObject:
            return "object";
        case _FXCFControlValueKindArray:
            return "array";
        case _FXCFControlValueKindString:
            return "string";
        case _FXCFControlValueKindInteger:
            return "integer";
        case _FXCFControlValueKindDouble:
            return "double";
        case _FXCFControlValueKindBool:
            return "bool";
        case _FXCFControlValueKindDate:
            return "date";
        case _FXCFControlValueKindData:
            return "data";
        default:
            return "invalid";
    }
}

static const char *__FXCFControlRequestRouteKindText(enum _FXCFControlRequestRouteKind kind) {
    switch (kind) {
        case _FXCFControlRequestRouteKindControlCapabilities:
            return "control.capabilities";
        case _FXCFControlRequestRouteKindControlHealth:
            return "control.health";
        case _FXCFControlRequestRouteKindDiagnosticsSnapshot:
            return "diagnostics.snapshot";
        case _FXCFControlRequestRouteKindNotifyCancel:
            return "notify.cancel";
        case _FXCFControlRequestRouteKindNotifyCheck:
            return "notify.check";
        case _FXCFControlRequestRouteKindNotifyGetState:
            return "notify.get_state";
        case _FXCFControlRequestRouteKindNotifyIsValidToken:
            return "notify.is_valid_token";
        case _FXCFControlRequestRouteKindNotifyListNames:
            return "notify.list_names";
        case _FXCFControlRequestRouteKindNotifyPost:
            return "notify.post";
        case _FXCFControlRequestRouteKindNotifyPrepareFileDescriptorDelivery:
            return "notify.prepare_file_descriptor_delivery";
        case _FXCFControlRequestRouteKindNotifyRegisterCheck:
            return "notify.register_check";
        case _FXCFControlRequestRouteKindNotifyRegisterDispatch:
            return "notify.register_dispatch";
        case _FXCFControlRequestRouteKindNotifyRegisterFileDescriptor:
            return "notify.register_file_descriptor";
        case _FXCFControlRequestRouteKindNotifyRegisterSignal:
            return "notify.register_signal";
        case _FXCFControlRequestRouteKindNotifyResume:
            return "notify.resume";
        case _FXCFControlRequestRouteKindNotifySetState:
            return "notify.set_state";
        case _FXCFControlRequestRouteKindNotifySuspend:
            return "notify.suspend";
        default:
            return "invalid";
    }
}

static enum _FXCFControlRequestRouteKind __FXCFControlRequestRouteKindForEnvelope(
    CFStringRef service,
    CFStringRef method
) {
    if (__FXCFControlPacketStringEqualsCString(service, "control")) {
        if (__FXCFControlPacketStringEqualsCString(method, "capabilities")) {
            return _FXCFControlRequestRouteKindControlCapabilities;
        }
        if (__FXCFControlPacketStringEqualsCString(method, "health")) {
            return _FXCFControlRequestRouteKindControlHealth;
        }
        return _FXCFControlRequestRouteKindInvalid;
    }

    if (__FXCFControlPacketStringEqualsCString(service, "diagnostics")) {
        if (__FXCFControlPacketStringEqualsCString(method, "snapshot")) {
            return _FXCFControlRequestRouteKindDiagnosticsSnapshot;
        }
        return _FXCFControlRequestRouteKindInvalid;
    }

    if (__FXCFControlPacketStringEqualsCString(service, "notify")) {
        if (__FXCFControlPacketStringEqualsCString(method, "cancel")) {
            return _FXCFControlRequestRouteKindNotifyCancel;
        }
        if (__FXCFControlPacketStringEqualsCString(method, "check")) {
            return _FXCFControlRequestRouteKindNotifyCheck;
        }
        if (__FXCFControlPacketStringEqualsCString(method, "get_state")) {
            return _FXCFControlRequestRouteKindNotifyGetState;
        }
        if (__FXCFControlPacketStringEqualsCString(method, "is_valid_token")) {
            return _FXCFControlRequestRouteKindNotifyIsValidToken;
        }
        if (__FXCFControlPacketStringEqualsCString(method, "list_names")) {
            return _FXCFControlRequestRouteKindNotifyListNames;
        }
        if (__FXCFControlPacketStringEqualsCString(method, "post")) {
            return _FXCFControlRequestRouteKindNotifyPost;
        }
        if (__FXCFControlPacketStringEqualsCString(method, "prepare_file_descriptor_delivery")) {
            return _FXCFControlRequestRouteKindNotifyPrepareFileDescriptorDelivery;
        }
        if (__FXCFControlPacketStringEqualsCString(method, "register_check")) {
            return _FXCFControlRequestRouteKindNotifyRegisterCheck;
        }
        if (__FXCFControlPacketStringEqualsCString(method, "register_dispatch")) {
            return _FXCFControlRequestRouteKindNotifyRegisterDispatch;
        }
        if (__FXCFControlPacketStringEqualsCString(method, "register_file_descriptor")) {
            return _FXCFControlRequestRouteKindNotifyRegisterFileDescriptor;
        }
        if (__FXCFControlPacketStringEqualsCString(method, "register_signal")) {
            return _FXCFControlRequestRouteKindNotifyRegisterSignal;
        }
        if (__FXCFControlPacketStringEqualsCString(method, "resume")) {
            return _FXCFControlRequestRouteKindNotifyResume;
        }
        if (__FXCFControlPacketStringEqualsCString(method, "set_state")) {
            return _FXCFControlRequestRouteKindNotifySetState;
        }
        if (__FXCFControlPacketStringEqualsCString(method, "suspend")) {
            return _FXCFControlRequestRouteKindNotifySuspend;
        }
    }

    return _FXCFControlRequestRouteKindInvalid;
}

void _FXCFControlRequestClear(struct _FXCFControlRequest *request) {
    if (request == NULL) {
        return;
    }
    if (request->packet != NULL) {
        CFRelease((CFTypeRef)request->packet);
    }
    memset(request, 0, sizeof(*request));
}

Boolean _FXCFControlRequestInit(
    CFAllocatorRef allocator,
    CFDataRef payload,
    struct _FXCFControlRequest *requestOut,
    struct _FXCFControlPacketError *errorOut
) {
    CFDictionaryRef packet = NULL;
    CFIndex protocolVersion = 0;
    CFStringRef service = NULL;
    CFStringRef method = NULL;
    CFDictionaryRef params = NULL;

    if (requestOut == NULL) {
        __FXCFControlPacketSetError(errorOut, _FXCFControlPacketErrorInvalidPacket, 0, "requestOut is NULL");
        return false;
    }

    memset(requestOut, 0, sizeof(*requestOut));

    if (!_FXCFControlPacketDecodeEnvelope(allocator, payload, _FXCFControlPacketKindRequest, &packet, errorOut)) {
        return false;
    }

    if (!__FXCFControlPacketRequireInteger(packet, "protocol_version", &protocolVersion, errorOut) ||
        !__FXCFControlPacketRequireString(packet, "service", &service, errorOut) ||
        !__FXCFControlPacketRequireString(packet, "method", &method, errorOut) ||
        !__FXCFControlPacketRequireDictionary(packet, "params", &params, errorOut)) {
        CFRelease((CFTypeRef)packet);
        return false;
    }

    requestOut->packet = packet;
    requestOut->protocolVersion = protocolVersion;
    requestOut->service = service;
    requestOut->method = method;
    requestOut->params = params;
    return true;
}

void _FXCFControlResponseClear(struct _FXCFControlResponse *response) {
    if (response == NULL) {
        return;
    }
    if (response->packet != NULL) {
        CFRelease((CFTypeRef)response->packet);
    }
    memset(response, 0, sizeof(*response));
}

Boolean _FXCFControlResponseInit(
    CFAllocatorRef allocator,
    CFDataRef payload,
    struct _FXCFControlResponse *responseOut,
    struct _FXCFControlPacketError *errorOut
) {
    CFDictionaryRef packet = NULL;
    CFIndex protocolVersion = 0;
    CFStringRef status = NULL;
    char *statusText = NULL;

    if (responseOut == NULL) {
        __FXCFControlPacketSetError(errorOut, _FXCFControlPacketErrorInvalidPacket, 0, "responseOut is NULL");
        return false;
    }

    memset(responseOut, 0, sizeof(*responseOut));

    if (!_FXCFControlPacketDecodeEnvelope(allocator, payload, _FXCFControlPacketKindResponse, &packet, errorOut)) {
        return false;
    }

    if (!__FXCFControlPacketRequireInteger(packet, "protocol_version", &protocolVersion, errorOut) ||
        !__FXCFControlPacketRequireString(packet, "status", &status, errorOut)) {
        CFRelease((CFTypeRef)packet);
        return false;
    }

    statusText = __FXCFControlPacketCopyCString(status);
    if (statusText == NULL) {
        __FXCFControlPacketSetError(errorOut, _FXCFControlPacketErrorInvalidPacket, 0, "failed to decode response status");
        CFRelease((CFTypeRef)packet);
        return false;
    }

    responseOut->packet = packet;
    responseOut->protocolVersion = protocolVersion;

    if (strcmp(statusText, "ok") == 0) {
        responseOut->status = _FXCFControlResponseStatusOK;
        if (!__FXCFControlPacketRequireValue(packet, "result", &responseOut->result, errorOut)) {
            free(statusText);
            _FXCFControlResponseClear(responseOut);
            return false;
        }
    } else if (strcmp(statusText, "error") == 0) {
        responseOut->status = _FXCFControlResponseStatusError;
        if (!__FXCFControlPacketRequireDictionary(packet, "error", &responseOut->errorPayload, errorOut) ||
            !__FXCFControlPacketRequireString(responseOut->errorPayload, "code", &responseOut->errorCode, errorOut) ||
            !__FXCFControlPacketRequireString(responseOut->errorPayload, "message", &responseOut->errorMessage, errorOut)) {
            if (errorOut != NULL && errorOut->kind == _FXCFControlPacketErrorMissingKey) {
                if (strcmp(errorOut->text, "code") == 0) {
                    snprintf(errorOut->text, sizeof(errorOut->text), "%s", "error.code");
                } else if (strcmp(errorOut->text, "message") == 0) {
                    snprintf(errorOut->text, sizeof(errorOut->text), "%s", "error.message");
                }
            }
            free(statusText);
            _FXCFControlResponseClear(responseOut);
            return false;
        }
    } else {
        __FXCFControlPacketSetError(errorOut, _FXCFControlPacketErrorInvalidPacket, 0, "unsupported response status");
        free(statusText);
        _FXCFControlResponseClear(responseOut);
        return false;
    }

    free(statusText);
    return true;
}

static char *__FXCFControlPacketCopyRequestServiceText(const struct _FXCFControlRequest *request) {
    return request != NULL ? __FXCFControlPacketCopyCString(request->service) : NULL;
}

static char *__FXCFControlPacketCopyRequestMethodText(const struct _FXCFControlRequest *request) {
    return request != NULL ? __FXCFControlPacketCopyCString(request->method) : NULL;
}

char *_FXCFControlRequestCopyEnvelopeJSON(const struct _FXCFControlRequest *request) {
    struct __FXCFTextBuffer buffer = {0};
    char **keys = NULL;
    CFIndex count = 0;
    char *serviceText = NULL;
    char *methodText = NULL;
    char *result = NULL;

    if (request == NULL || request->packet == NULL || request->service == NULL || request->method == NULL || request->params == NULL) {
        return NULL;
    }

    if (!__FXCFControlPacketCopySortedKeys(request->params, &keys, &count)) {
        return NULL;
    }

    serviceText = __FXCFControlPacketCopyRequestServiceText(request);
    methodText = __FXCFControlPacketCopyRequestMethodText(request);
    if (serviceText == NULL || methodText == NULL) {
        __FXCFControlPacketFreeSortedKeys(keys, count);
        free(serviceText);
        free(methodText);
        return NULL;
    }

    if (!__FXCFTextBufferAppendCString(&buffer, "{\"method\":") ||
        !__FXCFTextBufferAppendJSONString(&buffer, methodText) ||
        !__FXCFTextBufferAppendCString(&buffer, ",\"param_keys\":") ||
        !__FXCFControlPacketAppendKeyArrayJSON(&buffer, keys, count) ||
        !__FXCFTextBufferAppendFormat(&buffer, ",\"params_count\":%ld,\"protocol_version\":%ld,\"service\":", (long)count, (long)request->protocolVersion) ||
        !__FXCFTextBufferAppendJSONString(&buffer, serviceText) ||
        !__FXCFTextBufferAppendCString(&buffer, "}")) {
        __FXCFTextBufferFree(&buffer);
        __FXCFControlPacketFreeSortedKeys(keys, count);
        free(serviceText);
        free(methodText);
        return NULL;
    }

    result = __FXCFTextBufferDetach(&buffer);
    __FXCFTextBufferFree(&buffer);
    __FXCFControlPacketFreeSortedKeys(keys, count);
    free(serviceText);
    free(methodText);
    return result;
}

char *_FXCFControlRequestCopyEnvelopeSummary(const struct _FXCFControlRequest *request) {
    char *serviceText = NULL;
    char *methodText = NULL;
    char *result = NULL;
    size_t size = 0u;
    CFIndex paramsCount = 0;

    if (request == NULL || request->packet == NULL || request->service == NULL || request->method == NULL || request->params == NULL) {
        return NULL;
    }

    serviceText = __FXCFControlPacketCopyRequestServiceText(request);
    methodText = __FXCFControlPacketCopyRequestMethodText(request);
    if (serviceText == NULL || methodText == NULL) {
        free(serviceText);
        free(methodText);
        return NULL;
    }

    paramsCount = CFDictionaryGetCount(request->params);
    size = strlen(serviceText) + strlen(methodText) + 48u;
    result = (char *)calloc(size, sizeof(char));
    if (result != NULL) {
        snprintf(result, size, "request %s %s params=%ld", serviceText, methodText, (long)paramsCount);
    }

    free(serviceText);
    free(methodText);
    return result;
}

void _FXCFControlRequestRouteClear(struct _FXCFControlRequestRoute *route) {
    if (route == NULL) {
        return;
    }
    _FXCFControlRequestClear(&route->request);
    memset(route, 0, sizeof(*route));
}

Boolean _FXCFControlRequestRouteInit(
    CFAllocatorRef allocator,
    CFDataRef payload,
    struct _FXCFControlRequestRoute *routeOut,
    struct _FXCFControlPacketError *errorOut
) {
    CFDictionaryRef params = NULL;

    if (routeOut == NULL) {
        __FXCFControlPacketSetError(errorOut, _FXCFControlPacketErrorInvalidPacket, 0, "routeOut is NULL");
        return false;
    }

    memset(routeOut, 0, sizeof(*routeOut));

    if (!_FXCFControlRequestInit(allocator, payload, &routeOut->request, errorOut)) {
        return false;
    }

    routeOut->kind = __FXCFControlRequestRouteKindForEnvelope(routeOut->request.service, routeOut->request.method);
    if (routeOut->kind == _FXCFControlRequestRouteKindInvalid) {
        __FXCFControlPacketSetError(errorOut, _FXCFControlPacketErrorInvalidPacket, 0, "unsupported request route");
        _FXCFControlRequestRouteClear(routeOut);
        return false;
    }

    params = routeOut->request.params;
    if (!__FXCFControlPacketLookupOptionalString(params, "name", &routeOut->name, NULL, errorOut) ||
        !__FXCFControlPacketLookupOptionalString(params, "scope", &routeOut->scope, NULL, errorOut) ||
        !__FXCFControlPacketLookupOptionalString(params, "expected_session_id", &routeOut->expectedSessionID, NULL, errorOut) ||
        !__FXCFControlPacketLookupOptionalString(params, "client_registration_id", &routeOut->clientRegistrationID, NULL, errorOut) ||
        !__FXCFControlPacketLookupOptionalString(params, "queue_name", &routeOut->queueName, NULL, errorOut) ||
        !__FXCFControlPacketLookupOptionalInteger(params, "token", &routeOut->token, &routeOut->hasToken, errorOut) ||
        !__FXCFControlPacketLookupOptionalInteger(params, "signal", &routeOut->signal, &routeOut->hasSignal, errorOut) ||
        !__FXCFControlPacketLookupOptionalInteger(params, "target_pid", &routeOut->targetPID, &routeOut->hasTargetPID, errorOut) ||
        !__FXCFControlPacketLookupOptionalInteger(params, "state", &routeOut->state, &routeOut->hasState, errorOut) ||
        !__FXCFControlPacketLookupOptionalBoolean(params, "reuse_existing_binding", &routeOut->reuseExistingBinding, &routeOut->hasReuseExistingBinding, errorOut)) {
        _FXCFControlRequestRouteClear(routeOut);
        return false;
    }

    return true;
}

static Boolean __FXCFControlRequestRouteAppendOptionalStringJSON(
    struct __FXCFTextBuffer *buffer,
    const char *key,
    CFStringRef value,
    Boolean *needsComma
) {
    char *text = NULL;
    Boolean ok = false;

    if (value == NULL) {
        return true;
    }

    text = __FXCFControlPacketCopyCString(value);
    if (text == NULL) {
        return false;
    }
    ok =
        __FXCFTextBufferAppendJSONFieldPrefix(buffer, key, needsComma) &&
        __FXCFTextBufferAppendJSONString(buffer, text);
    free(text);
    return ok;
}

static Boolean __FXCFControlRequestRouteAppendOptionalIntegerJSON(
    struct __FXCFTextBuffer *buffer,
    const char *key,
    Boolean present,
    CFIndex value,
    Boolean *needsComma
) {
    if (!present) {
        return true;
    }
    return __FXCFTextBufferAppendJSONFieldPrefix(buffer, key, needsComma) &&
        __FXCFTextBufferAppendFormat(buffer, "%ld", (long)value);
}

static Boolean __FXCFControlRequestRouteAppendOptionalBooleanJSON(
    struct __FXCFTextBuffer *buffer,
    const char *key,
    Boolean present,
    Boolean value,
    Boolean *needsComma
) {
    if (!present) {
        return true;
    }
    return __FXCFTextBufferAppendJSONFieldPrefix(buffer, key, needsComma) &&
        __FXCFTextBufferAppendCString(buffer, value ? "true" : "false");
}

char *_FXCFControlRequestRouteCopyCanonicalJSON(const struct _FXCFControlRequestRoute *route) {
    struct __FXCFTextBuffer buffer = {0};
    char **keys = NULL;
    CFIndex count = 0;
    char *serviceText = NULL;
    char *methodText = NULL;
    char *result = NULL;
    const char *kindText = NULL;
    Boolean needsComma = false;

    if (route == NULL || route->request.packet == NULL) {
        return NULL;
    }

    if (!__FXCFControlPacketCopySortedKeys(route->request.params, &keys, &count)) {
        return NULL;
    }

    serviceText = __FXCFControlPacketCopyRequestServiceText(&route->request);
    methodText = __FXCFControlPacketCopyRequestMethodText(&route->request);
    kindText = __FXCFControlRequestRouteKindText(route->kind);
    if (serviceText == NULL || methodText == NULL || kindText == NULL) {
        __FXCFControlPacketFreeSortedKeys(keys, count);
        free(serviceText);
        free(methodText);
        return NULL;
    }

    if (!__FXCFTextBufferAppendCString(&buffer, "{") ||
        !__FXCFControlRequestRouteAppendOptionalStringJSON(&buffer, "client_registration_id", route->clientRegistrationID, &needsComma) ||
        !__FXCFControlRequestRouteAppendOptionalStringJSON(&buffer, "expected_session_id", route->expectedSessionID, &needsComma) ||
        !__FXCFTextBufferAppendJSONFieldPrefix(&buffer, "kind", &needsComma) ||
        !__FXCFTextBufferAppendJSONString(&buffer, kindText) ||
        !__FXCFTextBufferAppendJSONFieldPrefix(&buffer, "method", &needsComma) ||
        !__FXCFTextBufferAppendJSONString(&buffer, methodText) ||
        !__FXCFControlRequestRouteAppendOptionalStringJSON(&buffer, "name", route->name, &needsComma) ||
        !__FXCFTextBufferAppendJSONFieldPrefix(&buffer, "param_keys", &needsComma) ||
        !__FXCFControlPacketAppendKeyArrayJSON(&buffer, keys, count) ||
        !__FXCFTextBufferAppendJSONFieldPrefix(&buffer, "protocol_version", &needsComma) ||
        !__FXCFTextBufferAppendFormat(&buffer, "%ld", (long)route->request.protocolVersion) ||
        !__FXCFControlRequestRouteAppendOptionalStringJSON(&buffer, "queue_name", route->queueName, &needsComma) ||
        !__FXCFControlRequestRouteAppendOptionalBooleanJSON(&buffer, "reuse_existing_binding", route->hasReuseExistingBinding, route->reuseExistingBinding, &needsComma) ||
        !__FXCFControlRequestRouteAppendOptionalStringJSON(&buffer, "scope", route->scope, &needsComma) ||
        !__FXCFTextBufferAppendJSONFieldPrefix(&buffer, "service", &needsComma) ||
        !__FXCFTextBufferAppendJSONString(&buffer, serviceText) ||
        !__FXCFControlRequestRouteAppendOptionalIntegerJSON(&buffer, "signal", route->hasSignal, route->signal, &needsComma) ||
        !__FXCFControlRequestRouteAppendOptionalIntegerJSON(&buffer, "state", route->hasState, route->state, &needsComma) ||
        !__FXCFControlRequestRouteAppendOptionalIntegerJSON(&buffer, "target_pid", route->hasTargetPID, route->targetPID, &needsComma) ||
        !__FXCFControlRequestRouteAppendOptionalIntegerJSON(&buffer, "token", route->hasToken, route->token, &needsComma) ||
        !__FXCFTextBufferAppendCString(&buffer, "}")) {
        __FXCFTextBufferFree(&buffer);
        __FXCFControlPacketFreeSortedKeys(keys, count);
        free(serviceText);
        free(methodText);
        return NULL;
    }

    result = __FXCFTextBufferDetach(&buffer);
    __FXCFTextBufferFree(&buffer);
    __FXCFControlPacketFreeSortedKeys(keys, count);
    free(serviceText);
    free(methodText);
    return result;
}

char *_FXCFControlRequestRouteCopySummary(const struct _FXCFControlRequestRoute *route) {
    struct __FXCFTextBuffer buffer = {0};
    char **keys = NULL;
    CFIndex count = 0;
    char *result = NULL;
    const char *kindText = NULL;

    if (route == NULL || route->request.packet == NULL) {
        return NULL;
    }

    if (!__FXCFControlPacketCopySortedKeys(route->request.params, &keys, &count)) {
        return NULL;
    }

    kindText = __FXCFControlRequestRouteKindText(route->kind);
    if (!__FXCFTextBufferAppendFormat(&buffer, "route %s params=", kindText)) {
        __FXCFTextBufferFree(&buffer);
        __FXCFControlPacketFreeSortedKeys(keys, count);
        return NULL;
    }

    if (count == 0) {
        if (!__FXCFTextBufferAppendCString(&buffer, "none")) {
            __FXCFTextBufferFree(&buffer);
            __FXCFControlPacketFreeSortedKeys(keys, count);
            return NULL;
        }
    } else {
        for (CFIndex index = 0; index < count; ++index) {
            if (index > 0 && !__FXCFTextBufferAppendCString(&buffer, ",")) {
                __FXCFTextBufferFree(&buffer);
                __FXCFControlPacketFreeSortedKeys(keys, count);
                return NULL;
            }
            if (!__FXCFTextBufferAppendCString(&buffer, keys[index])) {
                __FXCFTextBufferFree(&buffer);
                __FXCFControlPacketFreeSortedKeys(keys, count);
                return NULL;
            }
        }
    }

    result = __FXCFTextBufferDetach(&buffer);
    __FXCFTextBufferFree(&buffer);
    __FXCFControlPacketFreeSortedKeys(keys, count);
    return result;
}

struct __FXCFControlRegistrationFields {
    CFStringRef deliveryMethod;
    CFStringRef scope;
    Boolean hasToken;
    CFIndex token;
    Boolean hasValid;
    Boolean valid;
    Boolean hasPendingGeneration;
    CFIndex pendingGeneration;
};

static const char *__FXCFControlResponseProfileKindText(enum _FXCFControlResponseProfileKind kind) {
    switch (kind) {
        case _FXCFControlResponseProfileKindError:
            return "error";
        case _FXCFControlResponseProfileKindNotifyPost:
            return "notify.post";
        case _FXCFControlResponseProfileKindNotifyRegistrationWrapper:
            return "notify.registration_wrapper";
        case _FXCFControlResponseProfileKindNotifyCancel:
            return "notify.cancel";
        case _FXCFControlResponseProfileKindNotifyRegistration:
            return "notify.registration";
        case _FXCFControlResponseProfileKindNotifyNameState:
            return "notify.name_state";
        case _FXCFControlResponseProfileKindNotifyValidity:
            return "notify.validity";
        case _FXCFControlResponseProfileKindNotifyCheck:
            return "notify.check";
        case _FXCFControlResponseProfileKindNotifyNameList:
            return "notify.name_list";
        case _FXCFControlResponseProfileKindControlCapabilities:
            return "control.capabilities";
        case _FXCFControlResponseProfileKindControlHealth:
            return "control.health";
        case _FXCFControlResponseProfileKindDiagnosticsSnapshot:
            return "diagnostics.snapshot";
        case _FXCFControlResponseProfileKindGenericObject:
            return "generic.object";
        default:
            return "invalid";
    }
}

static void __FXCFControlPacketFreeCStringArray(char **items, CFIndex count) {
    if (items == NULL) {
        return;
    }
    for (CFIndex index = 0; index < count; ++index) {
        free(items[index]);
    }
    free(items);
}

static Boolean __FXCFControlPacketCopySortedStringArray(CFArrayRef array, char ***itemsOut, CFIndex *countOut) {
    CFIndex count = 0;
    char **items = NULL;

    if (itemsOut == NULL || countOut == NULL) {
        return false;
    }

    *itemsOut = NULL;
    *countOut = 0;
    if (array == NULL) {
        return false;
    }

    count = CFArrayGetCount(array);
    if (count == 0) {
        return true;
    }

    items = (char **)calloc((size_t)count, sizeof(char *));
    if (items == NULL) {
        return false;
    }

    for (CFIndex index = 0; index < count; ++index) {
        CFTypeRef value = (CFTypeRef)CFArrayGetValueAtIndex(array, index);
        if (value == NULL || CFGetTypeID(value) != CFStringGetTypeID()) {
            __FXCFControlPacketFreeCStringArray(items, count);
            return false;
        }
        items[index] = __FXCFControlPacketCopyCString((CFStringRef)value);
        if (items[index] == NULL) {
            __FXCFControlPacketFreeCStringArray(items, count);
            return false;
        }
    }

    qsort(items, (size_t)count, sizeof(char *), __FXCFControlPacketCompareCStringPointers);
    *itemsOut = items;
    *countOut = count;
    return true;
}

static Boolean __FXCFControlPacketAppendCStringArrayJSON(struct __FXCFTextBuffer *buffer, char *const *items, CFIndex count) {
    if (!__FXCFTextBufferAppendCString(buffer, "[")) {
        return false;
    }
    for (CFIndex index = 0; index < count; ++index) {
        if (index > 0 && !__FXCFTextBufferAppendCString(buffer, ",")) {
            return false;
        }
        if (!__FXCFTextBufferAppendJSONString(buffer, items[index])) {
            return false;
        }
    }
    return __FXCFTextBufferAppendCString(buffer, "]");
}

static Boolean __FXCFControlPacketValidateNameObject(CFDictionaryRef dictionary) {
    struct _FXCFControlPacketError ignored = {0};
    CFStringRef scope = NULL;
    CFStringRef name = NULL;
    CFIndex generation = 0;
    CFIndex state = 0;
    CFDateRef updatedAt = NULL;
    CFDateRef lastPostedAt = NULL;
    Boolean present = false;

    return
        __FXCFControlPacketRequireString(dictionary, "scope", &scope, &ignored) &&
        __FXCFControlPacketRequireString(dictionary, "name", &name, &ignored) &&
        __FXCFControlPacketRequireInteger(dictionary, "generation", &generation, &ignored) &&
        __FXCFControlPacketRequireInteger(dictionary, "state", &state, &ignored) &&
        __FXCFControlPacketLookupOptionalDate(dictionary, "updated_at", &updatedAt, &present, &ignored) && present &&
        __FXCFControlPacketLookupOptionalDate(dictionary, "last_posted_at", &lastPostedAt, &present, &ignored) && present;
}

static char *__FXCFControlPacketCopyNameSummaryText(CFDictionaryRef dictionary) {
    struct _FXCFControlPacketError ignored = {0};
    CFStringRef scope = NULL;
    CFStringRef name = NULL;
    CFIndex generation = 0;
    CFIndex state = 0;
    char *scopeText = NULL;
    char *nameText = NULL;
    char *result = NULL;
    size_t size = 0u;

    if (!__FXCFControlPacketRequireString(dictionary, "scope", &scope, &ignored) ||
        !__FXCFControlPacketRequireString(dictionary, "name", &name, &ignored) ||
        !__FXCFControlPacketRequireInteger(dictionary, "generation", &generation, &ignored) ||
        !__FXCFControlPacketRequireInteger(dictionary, "state", &state, &ignored)) {
        return NULL;
    }

    scopeText = __FXCFControlPacketCopyCString(scope);
    nameText = __FXCFControlPacketCopyCString(name);
    if (scopeText == NULL || nameText == NULL) {
        free(scopeText);
        free(nameText);
        return NULL;
    }

    size = strlen(scopeText) + strlen(nameText) + 48u;
    result = (char *)calloc(size, sizeof(char));
    if (result != NULL) {
        snprintf(result, size, "%s:%s:%ld:%ld", scopeText, nameText, (long)generation, (long)state);
    }

    free(scopeText);
    free(nameText);
    return result;
}

static Boolean __FXCFControlPacketCopySortedNameSummaries(CFArrayRef array, char ***itemsOut, CFIndex *countOut) {
    CFIndex count = 0;
    char **items = NULL;

    if (itemsOut == NULL || countOut == NULL || array == NULL) {
        return false;
    }

    *itemsOut = NULL;
    *countOut = 0;
    count = CFArrayGetCount(array);
    if (count == 0) {
        return true;
    }

    items = (char **)calloc((size_t)count, sizeof(char *));
    if (items == NULL) {
        return false;
    }

    for (CFIndex index = 0; index < count; ++index) {
        CFTypeRef value = (CFTypeRef)CFArrayGetValueAtIndex(array, index);
        if (value == NULL || CFGetTypeID(value) != CFDictionaryGetTypeID() ||
            !__FXCFControlPacketValidateNameObject((CFDictionaryRef)value)) {
            __FXCFControlPacketFreeCStringArray(items, count);
            return false;
        }
        items[index] = __FXCFControlPacketCopyNameSummaryText((CFDictionaryRef)value);
        if (items[index] == NULL) {
            __FXCFControlPacketFreeCStringArray(items, count);
            return false;
        }
    }

    qsort(items, (size_t)count, sizeof(char *), __FXCFControlPacketCompareCStringPointers);
    *itemsOut = items;
    *countOut = count;
    return true;
}

static Boolean __FXCFControlPacketValidateRegistrationObject(CFDictionaryRef dictionary, struct __FXCFControlRegistrationFields *fieldsOut) {
    struct _FXCFControlPacketError ignored = {0};
    CFStringRef deliveryMethod = NULL;
    CFStringRef scope = NULL;
    CFStringRef deliveryState = NULL;
    CFStringRef name = NULL;
    CFStringRef sessionID = NULL;
    CFDateRef createdAt = NULL;
    CFIndex token = 0;
    CFIndex suspendDepth = 0;
    CFIndex lastSeenGeneration = 0;
    Boolean valid = false;
    Boolean firstCheckPending = false;
    Boolean present = false;
    CFIndex pendingGeneration = 0;

    if (fieldsOut != NULL) {
        memset(fieldsOut, 0, sizeof(*fieldsOut));
    }

    if (!__FXCFControlPacketRequireString(dictionary, "delivery_method", &deliveryMethod, &ignored) ||
        !__FXCFControlPacketRequireString(dictionary, "scope", &scope, &ignored) ||
        !__FXCFControlPacketRequireString(dictionary, "delivery_state", &deliveryState, &ignored) ||
        !__FXCFControlPacketRequireString(dictionary, "name", &name, &ignored) ||
        !__FXCFControlPacketRequireString(dictionary, "session_id", &sessionID, &ignored) ||
        !__FXCFControlPacketRequireInteger(dictionary, "token", &token, &ignored) ||
        !__FXCFControlPacketRequireInteger(dictionary, "suspend_depth", &suspendDepth, &ignored) ||
        !__FXCFControlPacketRequireInteger(dictionary, "last_seen_generation", &lastSeenGeneration, &ignored) ||
        !__FXCFControlPacketLookupOptionalDate(dictionary, "created_at", &createdAt, &present, &ignored) || !present ||
        !__FXCFControlPacketLookupOptionalBoolean(dictionary, "valid", &valid, &present, &ignored) || !present ||
        !__FXCFControlPacketLookupOptionalBoolean(dictionary, "first_check_pending", &firstCheckPending, &present, &ignored) || !present ||
        !__FXCFControlPacketLookupOptionalInteger(dictionary, "pending_generation", &pendingGeneration, &present, &ignored)) {
        return false;
    }

    if (fieldsOut != NULL) {
        fieldsOut->deliveryMethod = deliveryMethod;
        fieldsOut->scope = scope;
        fieldsOut->hasToken = true;
        fieldsOut->token = token;
        fieldsOut->hasValid = true;
        fieldsOut->valid = valid;
        fieldsOut->hasPendingGeneration = present;
        fieldsOut->pendingGeneration = pendingGeneration;
    }
    return true;
}

static Boolean __FXCFControlPacketValidateOutputStatusObject(CFDictionaryRef dictionary) {
    struct _FXCFControlPacketError ignored = {0};
    CFStringRef state = NULL;
    Boolean dirty = false;
    Boolean retryScheduled = false;
    Boolean present = false;

    return
        __FXCFControlPacketRequireString(dictionary, "state", &state, &ignored) &&
        __FXCFControlPacketLookupOptionalBoolean(dictionary, "dirty", &dirty, &present, &ignored) && present &&
        __FXCFControlPacketLookupOptionalBoolean(dictionary, "retry_scheduled", &retryScheduled, &present, &ignored) && present;
}

static Boolean __FXCFControlPacketValidateCapabilitiesObject(CFDictionaryRef dictionary) {
    struct _FXCFControlPacketError ignored = {0};
    CFArrayRef featureFlags = NULL;
    Boolean present = false;
    Boolean value = false;

    if (!__FXCFControlPacketRequireArray(dictionary, "feature_flags", &featureFlags, &ignored)) {
        return false;
    }
    for (CFIndex index = 0; index < CFArrayGetCount(featureFlags); ++index) {
        CFTypeRef item = (CFTypeRef)CFArrayGetValueAtIndex(featureFlags, index);
        if (item == NULL || CFGetTypeID(item) != CFStringGetTypeID()) {
            return false;
        }
    }

    return
        __FXCFControlPacketLookupOptionalBoolean(dictionary, "supports_native_macos_passthrough", &value, &present, &ignored) && present &&
        __FXCFControlPacketLookupOptionalBoolean(dictionary, "supports_userspace_jobs", &value, &present, &ignored) && present &&
        __FXCFControlPacketLookupOptionalBoolean(dictionary, "supports_userspace_notify", &value, &present, &ignored) && present &&
        __FXCFControlPacketLookupOptionalBoolean(dictionary, "supports_cluster_overlay", &value, &present, &ignored) && present &&
        __FXCFControlPacketLookupOptionalBoolean(dictionary, "supports_notify_fast_path", &value, &present, &ignored) && present &&
        __FXCFControlPacketLookupOptionalBoolean(dictionary, "supports_launchctl_wrapper", &value, &present, &ignored) && present;
}

static Boolean __FXCFControlPacketValidateReasonObject(CFDictionaryRef dictionary) {
    struct _FXCFControlPacketError ignored = {0};
    CFStringRef component = NULL;
    CFStringRef issue = NULL;
    return
        __FXCFControlPacketRequireString(dictionary, "component", &component, &ignored) &&
        __FXCFControlPacketRequireString(dictionary, "issue", &issue, &ignored);
}

static Boolean __FXCFControlPacketCopySortedReasonSummaries(CFArrayRef array, char ***itemsOut, CFIndex *countOut) {
    CFIndex count = 0;
    char **items = NULL;

    if (itemsOut == NULL || countOut == NULL || array == NULL) {
        return false;
    }

    *itemsOut = NULL;
    *countOut = 0;
    count = CFArrayGetCount(array);
    if (count == 0) {
        return true;
    }

    items = (char **)calloc((size_t)count, sizeof(char *));
    if (items == NULL) {
        return false;
    }

    for (CFIndex index = 0; index < count; ++index) {
        struct _FXCFControlPacketError ignored = {0};
        CFDictionaryRef item = NULL;
        CFStringRef component = NULL;
        CFStringRef issue = NULL;
        char *componentText = NULL;
        char *issueText = NULL;
        size_t size = 0u;

        item = (CFDictionaryRef)CFArrayGetValueAtIndex(array, index);
        if (item == NULL || CFGetTypeID((CFTypeRef)item) != CFDictionaryGetTypeID() || !__FXCFControlPacketValidateReasonObject(item) ||
            !__FXCFControlPacketRequireString(item, "component", &component, &ignored) ||
            !__FXCFControlPacketRequireString(item, "issue", &issue, &ignored)) {
            __FXCFControlPacketFreeCStringArray(items, count);
            return false;
        }
        componentText = __FXCFControlPacketCopyCString(component);
        issueText = __FXCFControlPacketCopyCString(issue);
        if (componentText == NULL || issueText == NULL) {
            free(componentText);
            free(issueText);
            __FXCFControlPacketFreeCStringArray(items, count);
            return false;
        }
        size = strlen(componentText) + strlen(issueText) + 2u;
        items[index] = (char *)calloc(size, sizeof(char));
        if (items[index] == NULL) {
            free(componentText);
            free(issueText);
            __FXCFControlPacketFreeCStringArray(items, count);
            return false;
        }
        snprintf(items[index], size, "%s:%s", componentText, issueText);
        free(componentText);
        free(issueText);
    }

    qsort(items, (size_t)count, sizeof(char *), __FXCFControlPacketCompareCStringPointers);
    *itemsOut = items;
    *countOut = count;
    return true;
}

static Boolean __FXCFControlPacketValidateHealthObject(CFDictionaryRef dictionary) {
    struct _FXCFControlPacketError ignored = {0};
    CFStringRef state = NULL;
    CFArrayRef reasons = NULL;
    CFDateRef observedAt = NULL;
    CFDictionaryRef jobPersistence = NULL;
    CFDictionaryRef notifyPersistence = NULL;
    CFDictionaryRef notifyFastPath = NULL;
    Boolean present = false;

    if (!__FXCFControlPacketRequireString(dictionary, "state", &state, &ignored) ||
        !__FXCFControlPacketRequireArray(dictionary, "reasons", &reasons, &ignored) ||
        !__FXCFControlPacketLookupOptionalDate(dictionary, "observed_at", &observedAt, &present, &ignored) || !present ||
        !__FXCFControlPacketRequireDictionary(dictionary, "job_persistence", &jobPersistence, &ignored) ||
        !__FXCFControlPacketRequireDictionary(dictionary, "notify_persistence", &notifyPersistence, &ignored) ||
        !__FXCFControlPacketRequireDictionary(dictionary, "notify_fast_path", &notifyFastPath, &ignored) ||
        !__FXCFControlPacketValidateOutputStatusObject(jobPersistence) ||
        !__FXCFControlPacketValidateOutputStatusObject(notifyPersistence) ||
        !__FXCFControlPacketValidateOutputStatusObject(notifyFastPath)) {
        return false;
    }

    for (CFIndex index = 0; index < CFArrayGetCount(reasons); ++index) {
        CFTypeRef item = (CFTypeRef)CFArrayGetValueAtIndex(reasons, index);
        if (item == NULL || CFGetTypeID(item) != CFDictionaryGetTypeID() ||
            !__FXCFControlPacketValidateReasonObject((CFDictionaryRef)item)) {
            return false;
        }
    }
    return true;
}

static Boolean __FXCFControlPacketValidateDaemonObject(CFDictionaryRef dictionary) {
    struct _FXCFControlPacketError ignored = {0};
    CFStringRef daemonID = NULL;
    CFStringRef nodeID = NULL;
    CFStringRef hostname = NULL;
    CFStringRef runtimeMode = NULL;
    CFStringRef version = NULL;
    CFStringRef socketPath = NULL;
    CFDateRef startedAt = NULL;
    Boolean clusterEnabled = false;
    Boolean notifyFastPathEnabled = false;
    Boolean present = false;

    return
        __FXCFControlPacketRequireString(dictionary, "daemon_id", &daemonID, &ignored) &&
        __FXCFControlPacketRequireString(dictionary, "node_id", &nodeID, &ignored) &&
        __FXCFControlPacketRequireString(dictionary, "hostname", &hostname, &ignored) &&
        __FXCFControlPacketRequireString(dictionary, "runtime_mode", &runtimeMode, &ignored) &&
        __FXCFControlPacketRequireString(dictionary, "version", &version, &ignored) &&
        __FXCFControlPacketRequireString(dictionary, "control_socket_path", &socketPath, &ignored) &&
        __FXCFControlPacketLookupOptionalDate(dictionary, "started_at", &startedAt, &present, &ignored) && present &&
        __FXCFControlPacketLookupOptionalBoolean(dictionary, "cluster_enabled", &clusterEnabled, &present, &ignored) && present &&
        __FXCFControlPacketLookupOptionalBoolean(dictionary, "notify_fast_path_enabled", &notifyFastPathEnabled, &present, &ignored) && present;
}

static Boolean __FXCFControlPacketValidateDiagnosticsSnapshot(CFDictionaryRef dictionary) {
    struct _FXCFControlPacketError ignored = {0};
    CFDictionaryRef daemon = NULL;
    CFDictionaryRef health = NULL;
    CFDictionaryRef capabilities = NULL;
    CFArrayRef jobs = NULL;
    CFArrayRef notifyNames = NULL;

    if (!__FXCFControlPacketRequireDictionary(dictionary, "daemon", &daemon, &ignored) ||
        !__FXCFControlPacketRequireDictionary(dictionary, "health", &health, &ignored) ||
        !__FXCFControlPacketRequireDictionary(dictionary, "capabilities", &capabilities, &ignored) ||
        !__FXCFControlPacketRequireArray(dictionary, "jobs", &jobs, &ignored) ||
        !__FXCFControlPacketRequireArray(dictionary, "notify_names", &notifyNames, &ignored) ||
        !__FXCFControlPacketValidateDaemonObject(daemon) ||
        !__FXCFControlPacketValidateHealthObject(health) ||
        !__FXCFControlPacketValidateCapabilitiesObject(capabilities)) {
        return false;
    }

    for (CFIndex index = 0; index < CFArrayGetCount(notifyNames); ++index) {
        CFTypeRef item = (CFTypeRef)CFArrayGetValueAtIndex(notifyNames, index);
        if (item == NULL || CFGetTypeID(item) != CFDictionaryGetTypeID() ||
            !__FXCFControlPacketValidateNameObject((CFDictionaryRef)item)) {
            return false;
        }
    }
    return true;
}

void _FXCFControlResponseProfileClear(struct _FXCFControlResponseProfile *profile) {
    if (profile == NULL) {
        return;
    }
    _FXCFControlResponseClear(&profile->response);
    memset(profile, 0, sizeof(*profile));
}

Boolean _FXCFControlResponseProfileInit(
    CFAllocatorRef allocator,
    CFDataRef payload,
    struct _FXCFControlResponseProfile *profileOut,
    struct _FXCFControlPacketError *errorOut
) {
    struct __FXCFControlRegistrationFields registrationFields = {0};

    if (profileOut == NULL) {
        __FXCFControlPacketSetError(errorOut, _FXCFControlPacketErrorInvalidPacket, 0, "profileOut is NULL");
        return false;
    }

    memset(profileOut, 0, sizeof(*profileOut));

    if (!_FXCFControlResponseInit(allocator, payload, &profileOut->response, errorOut)) {
        return false;
    }

    if (profileOut->response.status == _FXCFControlResponseStatusError) {
        profileOut->kind = _FXCFControlResponseProfileKindError;
        return true;
    }

    if (profileOut->response.result == NULL) {
        __FXCFControlPacketSetError(errorOut, _FXCFControlPacketErrorInvalidPacket, 0, "response result is NULL");
        _FXCFControlResponseProfileClear(profileOut);
        return false;
    }

    if (_FXCFControlPacketValueKind(profileOut->response.result) == _FXCFControlValueKindArray) {
        CFArrayRef array = (CFArrayRef)profileOut->response.result;
        char **items = NULL;
        CFIndex count = 0;

        if (!__FXCFControlPacketCopySortedNameSummaries(array, &items, &count)) {
            __FXCFControlPacketSetError(errorOut, _FXCFControlPacketErrorInvalidPacket, 0, "unsupported response profile");
            _FXCFControlResponseProfileClear(profileOut);
            return false;
        }
        __FXCFControlPacketFreeCStringArray(items, count);
        profileOut->kind = _FXCFControlResponseProfileKindNotifyNameList;
        profileOut->resultArray = array;
        profileOut->notifyNames = array;
        return true;
    }

    if (_FXCFControlPacketValueKind(profileOut->response.result) != _FXCFControlValueKindObject) {
        __FXCFControlPacketSetError(errorOut, _FXCFControlPacketErrorInvalidPacket, 0, "unsupported response profile");
        _FXCFControlResponseProfileClear(profileOut);
        return false;
    }

    profileOut->resultObject = (CFDictionaryRef)profileOut->response.result;

    if (__FXCFControlPacketLookupOptionalDictionary(profileOut->resultObject, "registration", &profileOut->registration, NULL, NULL) &&
        profileOut->registration != NULL &&
        __FXCFControlPacketLookupOptionalBoolean(profileOut->resultObject, "canceled", &profileOut->canceled, &profileOut->hasCanceled, NULL) &&
        profileOut->hasCanceled &&
        __FXCFControlPacketValidateRegistrationObject(profileOut->registration, &registrationFields)) {
        profileOut->kind = _FXCFControlResponseProfileKindNotifyCancel;
        profileOut->hasToken = registrationFields.hasToken;
        profileOut->token = registrationFields.token;
        profileOut->hasValid = registrationFields.hasValid;
        profileOut->valid = registrationFields.valid;
        return true;
    }

    if (__FXCFControlPacketRequireDictionary(profileOut->resultObject, "registration", &profileOut->registration, NULL) &&
        __FXCFControlPacketLookupOptionalInteger(profileOut->resultObject, "generation", &profileOut->generation, &profileOut->hasGeneration, NULL) &&
        __FXCFControlPacketLookupOptionalInteger(profileOut->resultObject, "current_state", &profileOut->currentState, &profileOut->hasCurrentState, NULL) &&
        __FXCFControlPacketLookupOptionalBoolean(profileOut->resultObject, "changed", &profileOut->changed, &profileOut->hasChanged, NULL) &&
        profileOut->hasGeneration && profileOut->hasCurrentState && profileOut->hasChanged &&
        __FXCFControlPacketValidateRegistrationObject(profileOut->registration, &registrationFields)) {
        profileOut->kind = _FXCFControlResponseProfileKindNotifyCheck;
        profileOut->hasToken = registrationFields.hasToken;
        profileOut->token = registrationFields.token;
        profileOut->hasValid = registrationFields.hasValid;
        profileOut->valid = registrationFields.valid;
        return true;
    }

    {
        Boolean postedAtPresent = false;
        if (__FXCFControlPacketRequireDictionary(profileOut->resultObject, "name", &profileOut->nameObject, NULL) &&
            __FXCFControlPacketRequireArray(profileOut->resultObject, "delivered_tokens", &profileOut->deliveredTokens, NULL) &&
            __FXCFControlPacketRequireInteger(profileOut->resultObject, "generation", &profileOut->generation, NULL) &&
            __FXCFControlPacketLookupOptionalDate(profileOut->resultObject, "posted_at", NULL, &postedAtPresent, NULL) &&
            postedAtPresent &&
            __FXCFControlPacketValidateNameObject(profileOut->nameObject)) {
            profileOut->kind = _FXCFControlResponseProfileKindNotifyPost;
            profileOut->hasGeneration = true;
            {
                struct _FXCFControlPacketError ignored = {0};
                if (!__FXCFControlPacketRequireInteger(profileOut->nameObject, "state", &profileOut->state, &ignored)) {
                    __FXCFControlPacketSetError(errorOut, _FXCFControlPacketErrorInvalidPacket, 0, "unsupported response profile");
                    _FXCFControlResponseProfileClear(profileOut);
                    return false;
                }
                profileOut->hasState = true;
            }
            return true;
        }
    }

    if (__FXCFControlPacketRequireDictionary(profileOut->resultObject, "name", &profileOut->nameObject, NULL) &&
        __FXCFControlPacketRequireInteger(profileOut->resultObject, "state", &profileOut->state, NULL) &&
        __FXCFControlPacketValidateNameObject(profileOut->nameObject)) {
        profileOut->kind = _FXCFControlResponseProfileKindNotifyNameState;
        profileOut->hasState = true;
        {
            struct _FXCFControlPacketError ignored = {0};
            if (!__FXCFControlPacketRequireInteger(profileOut->nameObject, "generation", &profileOut->generation, &ignored)) {
                __FXCFControlPacketSetError(errorOut, _FXCFControlPacketErrorInvalidPacket, 0, "unsupported response profile");
                _FXCFControlResponseProfileClear(profileOut);
                return false;
            }
            profileOut->hasGeneration = true;
        }
        return true;
    }

    if (__FXCFControlPacketRequireDictionary(profileOut->resultObject, "registration", &profileOut->registration, NULL) &&
        __FXCFControlPacketValidateRegistrationObject(profileOut->registration, &registrationFields)) {
        profileOut->kind = _FXCFControlResponseProfileKindNotifyRegistrationWrapper;
        profileOut->hasToken = registrationFields.hasToken;
        profileOut->token = registrationFields.token;
        profileOut->hasValid = registrationFields.hasValid;
        profileOut->valid = registrationFields.valid;
        return true;
    }

    if (__FXCFControlPacketValidateRegistrationObject(profileOut->resultObject, &registrationFields)) {
        profileOut->kind = _FXCFControlResponseProfileKindNotifyRegistration;
        profileOut->registration = profileOut->resultObject;
        profileOut->hasToken = registrationFields.hasToken;
        profileOut->token = registrationFields.token;
        profileOut->hasValid = registrationFields.hasValid;
        profileOut->valid = registrationFields.valid;
        profileOut->hasPendingGeneration = registrationFields.hasPendingGeneration;
        profileOut->pendingGeneration = registrationFields.pendingGeneration;
        return true;
    }

    if (CFDictionaryGetCount(profileOut->resultObject) == 1 &&
        __FXCFControlPacketLookupOptionalBoolean(profileOut->resultObject, "valid", &profileOut->valid, &profileOut->hasValid, NULL) &&
        profileOut->hasValid) {
        profileOut->kind = _FXCFControlResponseProfileKindNotifyValidity;
        return true;
    }

    if (__FXCFControlPacketValidateCapabilitiesObject(profileOut->resultObject) &&
        __FXCFControlPacketRequireArray(profileOut->resultObject, "feature_flags", &profileOut->featureFlags, NULL)) {
        profileOut->kind = _FXCFControlResponseProfileKindControlCapabilities;
        profileOut->capabilities = profileOut->resultObject;
        return true;
    }

    if (__FXCFControlPacketValidateHealthObject(profileOut->resultObject) &&
        __FXCFControlPacketRequireArray(profileOut->resultObject, "reasons", &profileOut->reasons, NULL) &&
        __FXCFControlPacketRequireDictionary(profileOut->resultObject, "job_persistence", &profileOut->health, NULL)) {
        profileOut->kind = _FXCFControlResponseProfileKindControlHealth;
        profileOut->health = profileOut->resultObject;
        return true;
    }

    if (__FXCFControlPacketValidateDiagnosticsSnapshot(profileOut->resultObject) &&
        __FXCFControlPacketRequireDictionary(profileOut->resultObject, "daemon", &profileOut->daemon, NULL) &&
        __FXCFControlPacketRequireDictionary(profileOut->resultObject, "capabilities", &profileOut->capabilities, NULL) &&
        __FXCFControlPacketRequireDictionary(profileOut->resultObject, "health", &profileOut->health, NULL) &&
        __FXCFControlPacketRequireArray(profileOut->resultObject, "notify_names", &profileOut->notifyNames, NULL)) {
        profileOut->kind = _FXCFControlResponseProfileKindDiagnosticsSnapshot;
        return true;
    }

    profileOut->kind = _FXCFControlResponseProfileKindGenericObject;
    return true;
}

char *_FXCFControlResponseProfileCopyCanonicalJSON(const struct _FXCFControlResponseProfile *profile) {
    struct __FXCFTextBuffer buffer = {0};
    const char *kindText = NULL;
    char *result = NULL;
    char *scopeText = NULL;
    char *deliveryText = NULL;
    char *nameText = NULL;
    char **items = NULL;
    CFIndex itemCount = 0;
    Boolean ok = false;

    if (profile == NULL || profile->response.packet == NULL) {
        return NULL;
    }

    kindText = __FXCFControlResponseProfileKindText(profile->kind);
    if (!__FXCFTextBufferAppendFormat(&buffer, "{\"kind\":\"%s\",\"protocol_version\":%ld,\"status\":",
            kindText,
            (long)profile->response.protocolVersion) ||
        !__FXCFTextBufferAppendJSONString(&buffer,
            profile->response.status == _FXCFControlResponseStatusError ? "error" : "ok")) {
        __FXCFTextBufferFree(&buffer);
        return NULL;
    }

    if (profile->kind == _FXCFControlResponseProfileKindError) {
        char *codeText = __FXCFControlPacketCopyCString(profile->response.errorCode);
        char *messageText = __FXCFControlPacketCopyCString(profile->response.errorMessage);
        if (codeText == NULL || messageText == NULL ||
            !__FXCFTextBufferAppendCString(&buffer, ",\"code\":") ||
            !__FXCFTextBufferAppendJSONString(&buffer, codeText) ||
            !__FXCFTextBufferAppendCString(&buffer, ",\"message\":") ||
            !__FXCFTextBufferAppendJSONString(&buffer, messageText) ||
            !__FXCFTextBufferAppendCString(&buffer, "}")) {
            __FXCFTextBufferFree(&buffer);
            free(codeText);
            free(messageText);
            return NULL;
        }
        free(codeText);
        free(messageText);
        result = __FXCFTextBufferDetach(&buffer);
        __FXCFTextBufferFree(&buffer);
        return result;
    }

    switch (profile->kind) {
        case _FXCFControlResponseProfileKindNotifyRegistrationWrapper:
        case _FXCFControlResponseProfileKindNotifyRegistration:
        case _FXCFControlResponseProfileKindNotifyCancel:
        case _FXCFControlResponseProfileKindNotifyCheck: {
            struct _FXCFControlPacketError ignored = {0};
            CFStringRef delivery = NULL;
            CFStringRef scope = NULL;
            if (!__FXCFControlPacketRequireString(profile->registration, "delivery_method", &delivery, &ignored) ||
                !__FXCFControlPacketRequireString(profile->registration, "scope", &scope, &ignored)) {
                __FXCFTextBufferFree(&buffer);
                return NULL;
            }
            deliveryText = __FXCFControlPacketCopyCString(delivery);
            scopeText = __FXCFControlPacketCopyCString(scope);
            if (deliveryText == NULL || scopeText == NULL) {
                __FXCFTextBufferFree(&buffer);
                free(deliveryText);
                free(scopeText);
                return NULL;
            }
            ok =
                __FXCFTextBufferAppendCString(&buffer, ",\"delivery_method\":") &&
                __FXCFTextBufferAppendJSONString(&buffer, deliveryText) &&
                __FXCFTextBufferAppendCString(&buffer, ",\"scope\":") &&
                __FXCFTextBufferAppendJSONString(&buffer, scopeText) &&
                __FXCFTextBufferAppendFormat(&buffer, ",\"token\":%ld,\"valid\":%s",
                    (long)profile->token,
                    profile->valid ? "true" : "false");
            if (ok && profile->kind == _FXCFControlResponseProfileKindNotifyCancel) {
                ok = __FXCFTextBufferAppendCString(&buffer, ",\"canceled\":") &&
                    __FXCFTextBufferAppendCString(&buffer, profile->canceled ? "true" : "false");
            }
            if (ok && profile->kind == _FXCFControlResponseProfileKindNotifyCheck) {
                ok = __FXCFTextBufferAppendFormat(&buffer,
                    ",\"changed\":%s,\"current_state\":%ld,\"generation\":%ld",
                    profile->changed ? "true" : "false",
                    (long)profile->currentState,
                    (long)profile->generation);
            }
            if (ok && profile->kind == _FXCFControlResponseProfileKindNotifyRegistration && profile->hasPendingGeneration) {
                ok = __FXCFTextBufferAppendFormat(&buffer, ",\"pending_generation\":%ld", (long)profile->pendingGeneration);
            }
            break;
        }
        case _FXCFControlResponseProfileKindNotifyPost: {
            struct _FXCFControlPacketError ignored = {0};
            CFStringRef name = NULL;
            CFStringRef scope = NULL;
            if (!__FXCFControlPacketRequireString(profile->nameObject, "name", &name, &ignored) ||
                !__FXCFControlPacketRequireString(profile->nameObject, "scope", &scope, &ignored)) {
                __FXCFTextBufferFree(&buffer);
                return NULL;
            }
            nameText = __FXCFControlPacketCopyCString(name);
            scopeText = __FXCFControlPacketCopyCString(scope);
            if (nameText == NULL || scopeText == NULL) {
                __FXCFTextBufferFree(&buffer);
                free(nameText);
                free(scopeText);
                return NULL;
            }
            ok =
                __FXCFTextBufferAppendCString(&buffer, ",\"delivered_tokens_count\":") &&
                __FXCFTextBufferAppendFormat(&buffer, "%ld", (long)CFArrayGetCount(profile->deliveredTokens)) &&
                __FXCFTextBufferAppendFormat(&buffer, ",\"generation\":%ld", (long)profile->generation) &&
                __FXCFTextBufferAppendCString(&buffer, ",\"name\":") &&
                __FXCFTextBufferAppendJSONString(&buffer, nameText) &&
                __FXCFTextBufferAppendCString(&buffer, ",\"scope\":") &&
                __FXCFTextBufferAppendJSONString(&buffer, scopeText) &&
                __FXCFTextBufferAppendFormat(&buffer, ",\"state\":%ld", (long)profile->state);
            break;
        }
        case _FXCFControlResponseProfileKindNotifyNameState: {
            struct _FXCFControlPacketError ignored = {0};
            CFStringRef name = NULL;
            CFStringRef scope = NULL;
            if (!__FXCFControlPacketRequireString(profile->nameObject, "name", &name, &ignored) ||
                !__FXCFControlPacketRequireString(profile->nameObject, "scope", &scope, &ignored)) {
                __FXCFTextBufferFree(&buffer);
                return NULL;
            }
            nameText = __FXCFControlPacketCopyCString(name);
            scopeText = __FXCFControlPacketCopyCString(scope);
            if (nameText == NULL || scopeText == NULL) {
                __FXCFTextBufferFree(&buffer);
                free(nameText);
                free(scopeText);
                return NULL;
            }
            ok =
                __FXCFTextBufferAppendFormat(&buffer, ",\"generation\":%ld", (long)profile->generation) &&
                __FXCFTextBufferAppendCString(&buffer, ",\"name\":") &&
                __FXCFTextBufferAppendJSONString(&buffer, nameText) &&
                __FXCFTextBufferAppendCString(&buffer, ",\"scope\":") &&
                __FXCFTextBufferAppendJSONString(&buffer, scopeText) &&
                __FXCFTextBufferAppendFormat(&buffer, ",\"state\":%ld", (long)profile->state);
            break;
        }
        case _FXCFControlResponseProfileKindNotifyValidity:
            ok = __FXCFTextBufferAppendCString(&buffer, ",\"valid\":") &&
                __FXCFTextBufferAppendCString(&buffer, profile->valid ? "true" : "false");
            break;
        case _FXCFControlResponseProfileKindNotifyNameList:
            ok = __FXCFControlPacketCopySortedNameSummaries(profile->notifyNames, &items, &itemCount) &&
                __FXCFTextBufferAppendFormat(&buffer, ",\"count\":%ld,\"items\":", (long)itemCount) &&
                __FXCFControlPacketAppendCStringArrayJSON(&buffer, items, itemCount);
            break;
        case _FXCFControlResponseProfileKindControlCapabilities:
        {
            struct _FXCFControlPacketError ignored = {0};
            Boolean supportsClusterOverlay = false;
            Boolean supportsLaunchctlWrapper = false;
            Boolean supportsNativePassthrough = false;
            Boolean supportsNotifyFastPath = false;
            Boolean supportsUserspaceJobs = false;
            Boolean supportsUserspaceNotify = false;

            ok =
                __FXCFControlPacketRequireBoolean(profile->capabilities, "supports_cluster_overlay", &supportsClusterOverlay, &ignored) &&
                __FXCFControlPacketRequireBoolean(profile->capabilities, "supports_launchctl_wrapper", &supportsLaunchctlWrapper, &ignored) &&
                __FXCFControlPacketRequireBoolean(profile->capabilities, "supports_native_macos_passthrough", &supportsNativePassthrough, &ignored) &&
                __FXCFControlPacketRequireBoolean(profile->capabilities, "supports_notify_fast_path", &supportsNotifyFastPath, &ignored) &&
                __FXCFControlPacketRequireBoolean(profile->capabilities, "supports_userspace_jobs", &supportsUserspaceJobs, &ignored) &&
                __FXCFControlPacketRequireBoolean(profile->capabilities, "supports_userspace_notify", &supportsUserspaceNotify, &ignored) &&
                __FXCFControlPacketCopySortedStringArray(profile->featureFlags, &items, &itemCount) &&
                __FXCFTextBufferAppendCString(&buffer, ",\"feature_flags\":") &&
                __FXCFControlPacketAppendCStringArrayJSON(&buffer, items, itemCount) &&
                __FXCFTextBufferAppendCString(&buffer, ",\"supports_cluster_overlay\":") &&
                __FXCFTextBufferAppendCString(&buffer, supportsClusterOverlay ? "true" : "false") &&
                __FXCFTextBufferAppendCString(&buffer, ",\"supports_launchctl_wrapper\":") &&
                __FXCFTextBufferAppendCString(&buffer, supportsLaunchctlWrapper ? "true" : "false") &&
                __FXCFTextBufferAppendCString(&buffer, ",\"supports_native_macos_passthrough\":") &&
                __FXCFTextBufferAppendCString(&buffer, supportsNativePassthrough ? "true" : "false") &&
                __FXCFTextBufferAppendCString(&buffer, ",\"supports_notify_fast_path\":") &&
                __FXCFTextBufferAppendCString(&buffer, supportsNotifyFastPath ? "true" : "false") &&
                __FXCFTextBufferAppendCString(&buffer, ",\"supports_userspace_jobs\":") &&
                __FXCFTextBufferAppendCString(&buffer, supportsUserspaceJobs ? "true" : "false") &&
                __FXCFTextBufferAppendCString(&buffer, ",\"supports_userspace_notify\":") &&
                __FXCFTextBufferAppendCString(&buffer, supportsUserspaceNotify ? "true" : "false");
            break;
        }
        case _FXCFControlResponseProfileKindControlHealth: {
            struct _FXCFControlPacketError ignored = {0};
            CFStringRef state = NULL;
            CFArrayRef reasons = NULL;
            CFDictionaryRef job = NULL;
            CFDictionaryRef notifyPersistence = NULL;
            CFDictionaryRef notifyFastPath = NULL;
            CFStringRef jobState = NULL;
            CFStringRef notifyState = NULL;
            CFStringRef fastState = NULL;
            char *stateText = NULL;
            char *jobText = NULL;
            char *notifyText = NULL;
            char *fastText = NULL;
            if (!__FXCFControlPacketRequireString(profile->health, "state", &state, &ignored) ||
                !__FXCFControlPacketRequireArray(profile->health, "reasons", &reasons, &ignored) ||
                !__FXCFControlPacketRequireDictionary(profile->health, "job_persistence", &job, &ignored) ||
                !__FXCFControlPacketRequireDictionary(profile->health, "notify_persistence", &notifyPersistence, &ignored) ||
                !__FXCFControlPacketRequireDictionary(profile->health, "notify_fast_path", &notifyFastPath, &ignored) ||
                !__FXCFControlPacketRequireString(job, "state", &jobState, &ignored) ||
                !__FXCFControlPacketRequireString(notifyPersistence, "state", &notifyState, &ignored) ||
                !__FXCFControlPacketRequireString(notifyFastPath, "state", &fastState, &ignored)) {
                __FXCFTextBufferFree(&buffer);
                return NULL;
            }
            stateText = __FXCFControlPacketCopyCString(state);
            jobText = __FXCFControlPacketCopyCString(jobState);
            notifyText = __FXCFControlPacketCopyCString(notifyState);
            fastText = __FXCFControlPacketCopyCString(fastState);
            ok = stateText != NULL && jobText != NULL && notifyText != NULL && fastText != NULL &&
                __FXCFControlPacketCopySortedReasonSummaries(reasons, &items, &itemCount) &&
                __FXCFTextBufferAppendCString(&buffer, ",\"job_persistence_state\":") &&
                __FXCFTextBufferAppendJSONString(&buffer, jobText) &&
                __FXCFTextBufferAppendCString(&buffer, ",\"notify_fast_path_state\":") &&
                __FXCFTextBufferAppendJSONString(&buffer, fastText) &&
                __FXCFTextBufferAppendCString(&buffer, ",\"notify_persistence_state\":") &&
                __FXCFTextBufferAppendJSONString(&buffer, notifyText) &&
                __FXCFTextBufferAppendFormat(&buffer, ",\"reason_count\":%ld,\"reason_keys\":", (long)itemCount) &&
                __FXCFControlPacketAppendCStringArrayJSON(&buffer, items, itemCount) &&
                __FXCFTextBufferAppendCString(&buffer, ",\"state\":") &&
                __FXCFTextBufferAppendJSONString(&buffer, stateText);
            free(stateText);
            free(jobText);
            free(notifyText);
            free(fastText);
            break;
        }
        case _FXCFControlResponseProfileKindDiagnosticsSnapshot: {
            struct _FXCFControlPacketError ignored = {0};
            CFStringRef daemonID = NULL;
            CFStringRef healthState = NULL;
            CFArrayRef jobs = NULL;
            CFDictionaryRef capabilities = NULL;
            CFDictionaryRef health = NULL;
            CFArrayRef featureFlags = NULL;
            char *daemonText = NULL;
            char *healthText = NULL;
            if (!__FXCFControlPacketRequireString(profile->daemon, "daemon_id", &daemonID, &ignored) ||
                !__FXCFControlPacketRequireDictionary(profile->resultObject, "capabilities", &capabilities, &ignored) ||
                !__FXCFControlPacketRequireDictionary(profile->resultObject, "health", &health, &ignored) ||
                !__FXCFControlPacketRequireString(health, "state", &healthState, &ignored) ||
                !__FXCFControlPacketRequireArray(capabilities, "feature_flags", &featureFlags, &ignored) ||
                !__FXCFControlPacketRequireArray(profile->resultObject, "jobs", &jobs, &ignored)) {
                __FXCFTextBufferFree(&buffer);
                return NULL;
            }
            daemonText = __FXCFControlPacketCopyCString(daemonID);
            healthText = __FXCFControlPacketCopyCString(healthState);
            ok = daemonText != NULL && healthText != NULL &&
                __FXCFControlPacketCopySortedStringArray(featureFlags, &items, &itemCount) &&
                __FXCFTextBufferAppendCString(&buffer, ",\"daemon_id\":") &&
                __FXCFTextBufferAppendJSONString(&buffer, daemonText) &&
                __FXCFTextBufferAppendCString(&buffer, ",\"feature_flags\":") &&
                __FXCFControlPacketAppendCStringArrayJSON(&buffer, items, itemCount) &&
                __FXCFTextBufferAppendCString(&buffer, ",\"health_state\":") &&
                __FXCFTextBufferAppendJSONString(&buffer, healthText) &&
                __FXCFTextBufferAppendFormat(&buffer, ",\"jobs_count\":%ld,\"notify_names_count\":%ld",
                    (long)CFArrayGetCount(jobs),
                    (long)CFArrayGetCount(profile->notifyNames));
            free(daemonText);
            free(healthText);
            break;
        }
        case _FXCFControlResponseProfileKindGenericObject: {
            char **keys = NULL;
            CFIndex count = 0;
            ok = __FXCFControlPacketCopySortedKeys(profile->resultObject, &keys, &count) &&
                __FXCFTextBufferAppendFormat(&buffer, ",\"result_keys\":") &&
                __FXCFControlPacketAppendCStringArrayJSON(&buffer, keys, count);
            __FXCFControlPacketFreeSortedKeys(keys, count);
            break;
        }
        default:
            ok = false;
            break;
    }

    free(scopeText);
    free(deliveryText);
    free(nameText);
    __FXCFControlPacketFreeCStringArray(items, itemCount);

    if (!ok || !__FXCFTextBufferAppendCString(&buffer, "}")) {
        __FXCFTextBufferFree(&buffer);
        return NULL;
    }

    result = __FXCFTextBufferDetach(&buffer);
    __FXCFTextBufferFree(&buffer);
    return result;
}

char *_FXCFControlResponseProfileCopySummary(const struct _FXCFControlResponseProfile *profile) {
    char *result = NULL;
    size_t size = 0u;
    const char *kindText = NULL;

    if (profile == NULL || profile->response.packet == NULL) {
        return NULL;
    }

    kindText = __FXCFControlResponseProfileKindText(profile->kind);
    switch (profile->kind) {
        case _FXCFControlResponseProfileKindError: {
            char *codeText = __FXCFControlPacketCopyCString(profile->response.errorCode);
            if (codeText == NULL) {
                return NULL;
            }
            size = strlen(codeText) + 24u;
            result = (char *)calloc(size, sizeof(char));
            if (result != NULL) {
                snprintf(result, size, "profile error code=%s", codeText);
            }
            free(codeText);
            return result;
        }
        case _FXCFControlResponseProfileKindNotifyPost:
            size = 64u;
            result = (char *)calloc(size, sizeof(char));
            if (result != NULL) {
                snprintf(result, size, "profile %s delivered=%ld generation=%ld", kindText,
                    (long)CFArrayGetCount(profile->deliveredTokens), (long)profile->generation);
            }
            return result;
        case _FXCFControlResponseProfileKindNotifyRegistrationWrapper:
        case _FXCFControlResponseProfileKindNotifyRegistration:
            size = 64u;
            result = (char *)calloc(size, sizeof(char));
            if (result != NULL) {
                snprintf(result, size, "profile %s token=%ld", kindText, (long)profile->token);
            }
            return result;
        case _FXCFControlResponseProfileKindNotifyCancel:
            size = 64u;
            result = (char *)calloc(size, sizeof(char));
            if (result != NULL) {
                snprintf(result, size, "profile %s canceled=%s token=%ld", kindText, profile->canceled ? "true" : "false", (long)profile->token);
            }
            return result;
        case _FXCFControlResponseProfileKindNotifyNameState:
            size = 64u;
            result = (char *)calloc(size, sizeof(char));
            if (result != NULL) {
                snprintf(result, size, "profile %s generation=%ld state=%ld", kindText, (long)profile->generation, (long)profile->state);
            }
            return result;
        case _FXCFControlResponseProfileKindNotifyValidity:
            size = 48u;
            result = (char *)calloc(size, sizeof(char));
            if (result != NULL) {
                snprintf(result, size, "profile %s valid=%s", kindText, profile->valid ? "true" : "false");
            }
            return result;
        case _FXCFControlResponseProfileKindNotifyCheck:
            size = 64u;
            result = (char *)calloc(size, sizeof(char));
            if (result != NULL) {
                snprintf(result, size, "profile %s changed=%s generation=%ld", kindText, profile->changed ? "true" : "false", (long)profile->generation);
            }
            return result;
        case _FXCFControlResponseProfileKindNotifyNameList:
            size = 48u;
            result = (char *)calloc(size, sizeof(char));
            if (result != NULL) {
                snprintf(result, size, "profile %s count=%ld", kindText, (long)CFArrayGetCount(profile->notifyNames));
            }
            return result;
        case _FXCFControlResponseProfileKindControlCapabilities:
            size = 56u;
            result = (char *)calloc(size, sizeof(char));
            if (result != NULL) {
                snprintf(result, size, "profile %s flags=%ld", kindText, (long)CFArrayGetCount(profile->featureFlags));
            }
            return result;
        case _FXCFControlResponseProfileKindControlHealth: {
            struct _FXCFControlPacketError ignored = {0};
            CFStringRef state = NULL;
            char *stateText = NULL;
            if (!__FXCFControlPacketRequireString(profile->health, "state", &state, &ignored)) {
                return NULL;
            }
            stateText = __FXCFControlPacketCopyCString(state);
            if (stateText == NULL) {
                return NULL;
            }
            size = strlen(stateText) + 48u;
            result = (char *)calloc(size, sizeof(char));
            if (result != NULL) {
                snprintf(result, size, "profile %s state=%s reasons=%ld", kindText, stateText, (long)CFArrayGetCount(profile->reasons));
            }
            free(stateText);
            return result;
        }
        case _FXCFControlResponseProfileKindDiagnosticsSnapshot:
        {
            struct _FXCFControlPacketError ignored = {0};
            CFArrayRef jobs = NULL;
            if (!__FXCFControlPacketRequireArray(profile->resultObject, "jobs", &jobs, &ignored)) {
                return NULL;
            }
            size = 64u;
            result = (char *)calloc(size, sizeof(char));
            if (result != NULL) {
                snprintf(result, size, "profile %s jobs=%ld notify_names=%ld", kindText, (long)CFArrayGetCount(jobs), (long)CFArrayGetCount(profile->notifyNames));
            }
            return result;
        }
        case _FXCFControlResponseProfileKindGenericObject:
            size = 56u;
            result = (char *)calloc(size, sizeof(char));
            if (result != NULL) {
                snprintf(result, size, "profile %s keys=%ld", kindText, (long)CFDictionaryGetCount(profile->resultObject));
            }
            return result;
        default:
            return NULL;
    }
}

static const char *__FXCFControlNotifyOutcomeKindText(enum _FXCFControlNotifyOutcomeKind kind) {
    switch (kind) {
        case _FXCFControlNotifyOutcomeKindError:
            return "error";
        case _FXCFControlNotifyOutcomeKindPost:
            return "notify.post";
        case _FXCFControlNotifyOutcomeKindRegistration:
            return "notify.registration";
        case _FXCFControlNotifyOutcomeKindCheck:
            return "notify.check";
        case _FXCFControlNotifyOutcomeKindCancel:
            return "notify.cancel";
        case _FXCFControlNotifyOutcomeKindNameState:
            return "notify.name_state";
        case _FXCFControlNotifyOutcomeKindValidity:
            return "notify.validity";
        default:
            return "invalid";
    }
}

static const char *__FXCFControlNotifyStatusFamilyText(enum _FXCFControlNotifyStatusFamily family) {
    switch (family) {
        case _FXCFControlNotifyStatusFamilyOK:
            return "ok";
        case _FXCFControlNotifyStatusFamilyInvalidRequest:
            return "invalid_request";
        case _FXCFControlNotifyStatusFamilyInvalidName:
            return "invalid_name";
        case _FXCFControlNotifyStatusFamilyInvalidToken:
            return "invalid_token";
        case _FXCFControlNotifyStatusFamilyInvalidSignal:
            return "invalid_signal";
        case _FXCFControlNotifyStatusFamilyInvalidFile:
            return "invalid_file";
        case _FXCFControlNotifyStatusFamilyFailed:
            return "failed";
        default:
            return "invalid";
    }
}

static enum _FXCFControlNotifyStatusFamily __FXCFControlNotifyStatusFamilyForError(CFStringRef code, CFStringRef message) {
    char *codeText = NULL;
    char *messageText = NULL;
    enum _FXCFControlNotifyStatusFamily family = _FXCFControlNotifyStatusFamilyFailed;

    codeText = __FXCFControlPacketCopyCString(code);
    messageText = __FXCFControlPacketCopyCString(message);
    if (codeText == NULL || messageText == NULL) {
        free(codeText);
        free(messageText);
        return family;
    }

    if (strcmp(codeText, "notify_failed") == 0) {
        if (strstr(messageText, "invalid_name(") != NULL) {
            family = _FXCFControlNotifyStatusFamilyInvalidName;
        } else if (strstr(messageText, "invalid_token(") != NULL) {
            family = _FXCFControlNotifyStatusFamilyInvalidToken;
        } else if (strstr(messageText, "invalid_signal(") != NULL) {
            family = _FXCFControlNotifyStatusFamilyInvalidSignal;
        } else if (strstr(messageText, "invalid_client_registration_id(") != NULL) {
            family = _FXCFControlNotifyStatusFamilyInvalidRequest;
        } else if (strstr(messageText, "transport_binding_in_use(") != NULL ||
                strstr(messageText, "unknown_transport_binding(") != NULL ||
                strstr(messageText, "transport_failed(") != NULL) {
            family = _FXCFControlNotifyStatusFamilyInvalidFile;
        } else {
            family = _FXCFControlNotifyStatusFamilyFailed;
        }
    } else if (strcmp(codeText, "invalid_request") == 0 || strcmp(codeText, "unsupported_method") == 0) {
        family = _FXCFControlNotifyStatusFamilyInvalidRequest;
    } else {
        family = _FXCFControlNotifyStatusFamilyFailed;
    }

    free(codeText);
    free(messageText);
    return family;
}

static Boolean __FXCFControlNotifyRegistrationViewInit(
    CFDictionaryRef dictionary,
    struct _FXCFControlNotifyRegistrationView *viewOut,
    struct _FXCFControlPacketError *errorOut
) {
    if (dictionary == NULL || viewOut == NULL) {
        __FXCFControlPacketSetError(errorOut, _FXCFControlPacketErrorInvalidPacket, 0, "registration view input is NULL");
        return false;
    }

    memset(viewOut, 0, sizeof(*viewOut));
    return
        __FXCFControlPacketRequireInteger(dictionary, "token", &viewOut->token, errorOut) &&
        __FXCFControlPacketRequireString(dictionary, "session_id", &viewOut->sessionID, errorOut) &&
        __FXCFControlPacketRequireString(dictionary, "scope", &viewOut->scope, errorOut) &&
        __FXCFControlPacketRequireString(dictionary, "name", &viewOut->name, errorOut) &&
        __FXCFControlPacketLookupOptionalString(dictionary, "local_transport_binding_id", &viewOut->bindingID, NULL, errorOut) &&
        __FXCFControlPacketLookupOptionalString(dictionary, "local_transport_handle", &viewOut->bindingHandle, NULL, errorOut) &&
        __FXCFControlPacketRequireInteger(dictionary, "last_seen_generation", &viewOut->lastSeenGeneration, errorOut) &&
        __FXCFControlPacketRequireBoolean(dictionary, "first_check_pending", &viewOut->firstCheckPending, errorOut);
}

void _FXCFControlNotifyOutcomeClear(struct _FXCFControlNotifyOutcome *outcome) {
    if (outcome == NULL) {
        return;
    }
    _FXCFControlResponseProfileClear(&outcome->profile);
    memset(outcome, 0, sizeof(*outcome));
}

Boolean _FXCFControlNotifyOutcomeInit(
    CFAllocatorRef allocator,
    CFDataRef payload,
    struct _FXCFControlNotifyOutcome *outcomeOut,
    struct _FXCFControlPacketError *errorOut
) {
    if (outcomeOut == NULL) {
        __FXCFControlPacketSetError(errorOut, _FXCFControlPacketErrorInvalidPacket, 0, "outcomeOut is NULL");
        return false;
    }

    memset(outcomeOut, 0, sizeof(*outcomeOut));
    if (!_FXCFControlResponseProfileInit(allocator, payload, &outcomeOut->profile, errorOut)) {
        return false;
    }

    if (outcomeOut->profile.kind == _FXCFControlResponseProfileKindError) {
        outcomeOut->kind = _FXCFControlNotifyOutcomeKindError;
        outcomeOut->statusFamily = __FXCFControlNotifyStatusFamilyForError(
            outcomeOut->profile.response.errorCode,
            outcomeOut->profile.response.errorMessage
        );
        return true;
    }

    outcomeOut->statusFamily = _FXCFControlNotifyStatusFamilyOK;

    switch (outcomeOut->profile.kind) {
        case _FXCFControlResponseProfileKindNotifyPost:
            outcomeOut->kind = _FXCFControlNotifyOutcomeKindPost;
            if (!outcomeOut->profile.hasGeneration || !outcomeOut->profile.hasState ||
                outcomeOut->profile.deliveredTokens == NULL || outcomeOut->profile.nameObject == NULL ||
                !__FXCFControlPacketRequireString(outcomeOut->profile.nameObject, "name", &outcomeOut->name, errorOut) ||
                !__FXCFControlPacketRequireString(outcomeOut->profile.nameObject, "scope", &outcomeOut->scope, errorOut)) {
                __FXCFControlPacketSetError(errorOut, _FXCFControlPacketErrorInvalidPacket, 0, "invalid notify post outcome");
                _FXCFControlNotifyOutcomeClear(outcomeOut);
                return false;
            }
            outcomeOut->hasGeneration = true;
            outcomeOut->generation = outcomeOut->profile.generation;
            outcomeOut->hasState = true;
            outcomeOut->state = outcomeOut->profile.state;
            outcomeOut->hasDeliveredTokensCount = true;
            outcomeOut->deliveredTokensCount = CFArrayGetCount(outcomeOut->profile.deliveredTokens);
            return true;

        case _FXCFControlResponseProfileKindNotifyRegistrationWrapper:
        case _FXCFControlResponseProfileKindNotifyRegistration:
            outcomeOut->kind = _FXCFControlNotifyOutcomeKindRegistration;
            outcomeOut->hasRegistration = true;
            if (!__FXCFControlNotifyRegistrationViewInit(outcomeOut->profile.registration, &outcomeOut->registration, errorOut)) {
                _FXCFControlNotifyOutcomeClear(outcomeOut);
                return false;
            }
            return true;

        case _FXCFControlResponseProfileKindNotifyCheck:
            outcomeOut->kind = _FXCFControlNotifyOutcomeKindCheck;
            if (!outcomeOut->profile.hasChanged || !outcomeOut->profile.hasGeneration || !outcomeOut->profile.hasCurrentState) {
                __FXCFControlPacketSetError(errorOut, _FXCFControlPacketErrorInvalidPacket, 0, "invalid notify check outcome");
                _FXCFControlNotifyOutcomeClear(outcomeOut);
                return false;
            }
            outcomeOut->hasRegistration = true;
            if (!__FXCFControlNotifyRegistrationViewInit(outcomeOut->profile.registration, &outcomeOut->registration, errorOut)) {
                _FXCFControlNotifyOutcomeClear(outcomeOut);
                return false;
            }
            outcomeOut->hasChanged = true;
            outcomeOut->changed = outcomeOut->profile.changed;
            outcomeOut->hasGeneration = true;
            outcomeOut->generation = outcomeOut->profile.generation;
            outcomeOut->hasCurrentState = true;
            outcomeOut->currentState = outcomeOut->profile.currentState;
            return true;

        case _FXCFControlResponseProfileKindNotifyCancel:
            if (!outcomeOut->profile.hasCanceled) {
                __FXCFControlPacketSetError(errorOut, _FXCFControlPacketErrorInvalidPacket, 0, "invalid notify cancel outcome");
                _FXCFControlNotifyOutcomeClear(outcomeOut);
                return false;
            }
            outcomeOut->kind = _FXCFControlNotifyOutcomeKindCancel;
            outcomeOut->hasCanceled = true;
            outcomeOut->canceled = outcomeOut->profile.canceled;
            return true;

        case _FXCFControlResponseProfileKindNotifyNameState:
            outcomeOut->kind = _FXCFControlNotifyOutcomeKindNameState;
            if (!outcomeOut->profile.hasGeneration || !outcomeOut->profile.hasState || outcomeOut->profile.nameObject == NULL ||
                !__FXCFControlPacketRequireString(outcomeOut->profile.nameObject, "name", &outcomeOut->name, errorOut) ||
                !__FXCFControlPacketRequireString(outcomeOut->profile.nameObject, "scope", &outcomeOut->scope, errorOut)) {
                __FXCFControlPacketSetError(errorOut, _FXCFControlPacketErrorInvalidPacket, 0, "invalid notify name_state outcome");
                _FXCFControlNotifyOutcomeClear(outcomeOut);
                return false;
            }
            outcomeOut->hasGeneration = true;
            outcomeOut->generation = outcomeOut->profile.generation;
            outcomeOut->hasState = true;
            outcomeOut->state = outcomeOut->profile.state;
            return true;

        case _FXCFControlResponseProfileKindNotifyValidity:
            if (!outcomeOut->profile.hasValid) {
                __FXCFControlPacketSetError(errorOut, _FXCFControlPacketErrorInvalidPacket, 0, "invalid notify validity outcome");
                _FXCFControlNotifyOutcomeClear(outcomeOut);
                return false;
            }
            outcomeOut->kind = _FXCFControlNotifyOutcomeKindValidity;
            outcomeOut->hasValid = true;
            outcomeOut->valid = outcomeOut->profile.valid;
            return true;

        default:
            __FXCFControlPacketSetError(errorOut, _FXCFControlPacketErrorInvalidPacket, 0, "unsupported notify outcome profile");
            _FXCFControlNotifyOutcomeClear(outcomeOut);
            return false;
    }
}

static Boolean __FXCFControlNotifyOutcomeAppendRequiredStringJSON(
    struct __FXCFTextBuffer *buffer,
    const char *key,
    CFStringRef value,
    Boolean *needsComma
) {
    char *text = NULL;
    Boolean ok = false;

    if (value == NULL) {
        return false;
    }

    text = __FXCFControlPacketCopyCString(value);
    if (text == NULL) {
        return false;
    }

    ok =
        __FXCFTextBufferAppendJSONFieldPrefix(buffer, key, needsComma) &&
        __FXCFTextBufferAppendJSONString(buffer, text);
    free(text);
    return ok;
}

static Boolean __FXCFControlNotifyOutcomeAppendRegistrationJSON(
    struct __FXCFTextBuffer *buffer,
    const struct _FXCFControlNotifyRegistrationView *registration
) {
    Boolean needsComma = false;

    if (buffer == NULL || registration == NULL) {
        return false;
    }

    return
        __FXCFTextBufferAppendCString(buffer, "{") &&
        __FXCFControlRequestRouteAppendOptionalStringJSON(buffer, "binding_handle", registration->bindingHandle, &needsComma) &&
        __FXCFControlRequestRouteAppendOptionalStringJSON(buffer, "binding_id", registration->bindingID, &needsComma) &&
        __FXCFTextBufferAppendJSONFieldPrefix(buffer, "first_check_pending", &needsComma) &&
        __FXCFTextBufferAppendCString(buffer, registration->firstCheckPending ? "true" : "false") &&
        __FXCFTextBufferAppendJSONFieldPrefix(buffer, "last_seen_generation", &needsComma) &&
        __FXCFTextBufferAppendFormat(buffer, "%ld", (long)registration->lastSeenGeneration) &&
        __FXCFControlNotifyOutcomeAppendRequiredStringJSON(buffer, "name", registration->name, &needsComma) &&
        __FXCFControlNotifyOutcomeAppendRequiredStringJSON(buffer, "scope", registration->scope, &needsComma) &&
        __FXCFControlNotifyOutcomeAppendRequiredStringJSON(buffer, "session_id", registration->sessionID, &needsComma) &&
        __FXCFTextBufferAppendJSONFieldPrefix(buffer, "token", &needsComma) &&
        __FXCFTextBufferAppendFormat(buffer, "%ld", (long)registration->token) &&
        __FXCFTextBufferAppendCString(buffer, "}");
}

char *_FXCFControlNotifyOutcomeCopyCanonicalJSON(const struct _FXCFControlNotifyOutcome *outcome) {
    struct __FXCFTextBuffer buffer = {0};
    const char *kindText = NULL;
    const char *statusFamilyText = NULL;
    char *result = NULL;
    Boolean needsComma = false;
    Boolean ok = false;

    if (outcome == NULL || outcome->profile.response.packet == NULL) {
        return NULL;
    }

    kindText = __FXCFControlNotifyOutcomeKindText(outcome->kind);
    statusFamilyText = __FXCFControlNotifyStatusFamilyText(outcome->statusFamily);
    if (!__FXCFTextBufferAppendCString(&buffer, "{") ||
        !__FXCFTextBufferAppendJSONFieldPrefix(&buffer, "kind", &needsComma) ||
        !__FXCFTextBufferAppendJSONString(&buffer, kindText) ||
        !__FXCFTextBufferAppendJSONFieldPrefix(&buffer, "status_family", &needsComma) ||
        !__FXCFTextBufferAppendJSONString(&buffer, statusFamilyText)) {
        __FXCFTextBufferFree(&buffer);
        return NULL;
    }

    switch (outcome->kind) {
        case _FXCFControlNotifyOutcomeKindError: {
            ok =
                __FXCFControlNotifyOutcomeAppendRequiredStringJSON(&buffer, "code", outcome->profile.response.errorCode, &needsComma) &&
                __FXCFControlNotifyOutcomeAppendRequiredStringJSON(&buffer, "message", outcome->profile.response.errorMessage, &needsComma);
            break;
        }
        case _FXCFControlNotifyOutcomeKindPost:
            ok =
                __FXCFTextBufferAppendJSONFieldPrefix(&buffer, "delivered_tokens_count", &needsComma) &&
                __FXCFTextBufferAppendFormat(&buffer, "%ld", (long)outcome->deliveredTokensCount) &&
                __FXCFTextBufferAppendJSONFieldPrefix(&buffer, "generation", &needsComma) &&
                __FXCFTextBufferAppendFormat(&buffer, "%ld", (long)outcome->generation) &&
                __FXCFControlNotifyOutcomeAppendRequiredStringJSON(&buffer, "name", outcome->name, &needsComma) &&
                __FXCFControlNotifyOutcomeAppendRequiredStringJSON(&buffer, "scope", outcome->scope, &needsComma) &&
                __FXCFTextBufferAppendJSONFieldPrefix(&buffer, "state", &needsComma) &&
                __FXCFTextBufferAppendFormat(&buffer, "%ld", (long)outcome->state);
            break;
        case _FXCFControlNotifyOutcomeKindRegistration:
            ok =
                __FXCFTextBufferAppendJSONFieldPrefix(&buffer, "registration", &needsComma) &&
                __FXCFControlNotifyOutcomeAppendRegistrationJSON(&buffer, &outcome->registration);
            break;
        case _FXCFControlNotifyOutcomeKindCheck:
            ok =
                __FXCFTextBufferAppendJSONFieldPrefix(&buffer, "changed", &needsComma) &&
                __FXCFTextBufferAppendCString(&buffer, outcome->changed ? "true" : "false") &&
                __FXCFTextBufferAppendJSONFieldPrefix(&buffer, "current_state", &needsComma) &&
                __FXCFTextBufferAppendFormat(&buffer, "%ld", (long)outcome->currentState) &&
                __FXCFTextBufferAppendJSONFieldPrefix(&buffer, "generation", &needsComma) &&
                __FXCFTextBufferAppendFormat(&buffer, "%ld", (long)outcome->generation) &&
                __FXCFTextBufferAppendJSONFieldPrefix(&buffer, "registration", &needsComma) &&
                __FXCFControlNotifyOutcomeAppendRegistrationJSON(&buffer, &outcome->registration);
            break;
        case _FXCFControlNotifyOutcomeKindCancel:
            ok =
                __FXCFTextBufferAppendJSONFieldPrefix(&buffer, "canceled", &needsComma) &&
                __FXCFTextBufferAppendCString(&buffer, outcome->canceled ? "true" : "false");
            break;
        case _FXCFControlNotifyOutcomeKindNameState:
            ok =
                __FXCFTextBufferAppendJSONFieldPrefix(&buffer, "generation", &needsComma) &&
                __FXCFTextBufferAppendFormat(&buffer, "%ld", (long)outcome->generation) &&
                __FXCFControlNotifyOutcomeAppendRequiredStringJSON(&buffer, "name", outcome->name, &needsComma) &&
                __FXCFControlNotifyOutcomeAppendRequiredStringJSON(&buffer, "scope", outcome->scope, &needsComma) &&
                __FXCFTextBufferAppendJSONFieldPrefix(&buffer, "state", &needsComma) &&
                __FXCFTextBufferAppendFormat(&buffer, "%ld", (long)outcome->state);
            break;
        case _FXCFControlNotifyOutcomeKindValidity:
            ok =
                __FXCFTextBufferAppendJSONFieldPrefix(&buffer, "valid", &needsComma) &&
                __FXCFTextBufferAppendCString(&buffer, outcome->valid ? "true" : "false");
            break;
        default:
            ok = false;
            break;
    }

    if (!ok || !__FXCFTextBufferAppendCString(&buffer, "}")) {
        __FXCFTextBufferFree(&buffer);
        return NULL;
    }

    result = __FXCFTextBufferDetach(&buffer);
    __FXCFTextBufferFree(&buffer);
    return result;
}

char *_FXCFControlNotifyOutcomeCopySummary(const struct _FXCFControlNotifyOutcome *outcome) {
    char *result = NULL;
    char *sessionText = NULL;
    char *codeText = NULL;
    size_t size = 0u;
    const char *statusFamilyText = NULL;

    if (outcome == NULL || outcome->profile.response.packet == NULL) {
        return NULL;
    }

    statusFamilyText = __FXCFControlNotifyStatusFamilyText(outcome->statusFamily);
    switch (outcome->kind) {
        case _FXCFControlNotifyOutcomeKindError:
            codeText = __FXCFControlPacketCopyCString(outcome->profile.response.errorCode);
            if (codeText == NULL) {
                return NULL;
            }
            size = strlen(statusFamilyText) + strlen(codeText) + 24u;
            result = (char *)calloc(size, sizeof(char));
            if (result != NULL) {
                snprintf(result, size, "notify error %s code=%s", statusFamilyText, codeText);
            }
            free(codeText);
            return result;

        case _FXCFControlNotifyOutcomeKindPost:
            size = 72u;
            result = (char *)calloc(size, sizeof(char));
            if (result != NULL) {
                snprintf(result, size, "notify %s post generation=%ld delivered=%ld",
                    statusFamilyText,
                    (long)outcome->generation,
                    (long)outcome->deliveredTokensCount);
            }
            return result;

        case _FXCFControlNotifyOutcomeKindRegistration:
            sessionText = __FXCFControlPacketCopyCString(outcome->registration.sessionID);
            if (sessionText == NULL) {
                return NULL;
            }
            size = strlen(sessionText) + 56u;
            result = (char *)calloc(size, sizeof(char));
            if (result != NULL) {
                snprintf(result, size, "notify %s registration token=%ld session=%s",
                    statusFamilyText,
                    (long)outcome->registration.token,
                    sessionText);
            }
            free(sessionText);
            return result;

        case _FXCFControlNotifyOutcomeKindCheck:
            size = 80u;
            result = (char *)calloc(size, sizeof(char));
            if (result != NULL) {
                snprintf(result, size, "notify %s check token=%ld changed=%s generation=%ld",
                    statusFamilyText,
                    (long)outcome->registration.token,
                    outcome->changed ? "true" : "false",
                    (long)outcome->generation);
            }
            return result;

        case _FXCFControlNotifyOutcomeKindCancel:
            size = 48u;
            result = (char *)calloc(size, sizeof(char));
            if (result != NULL) {
                snprintf(result, size, "notify %s cancel canceled=%s",
                    statusFamilyText,
                    outcome->canceled ? "true" : "false");
            }
            return result;

        case _FXCFControlNotifyOutcomeKindNameState:
            size = 72u;
            result = (char *)calloc(size, sizeof(char));
            if (result != NULL) {
                snprintf(result, size, "notify %s name_state state=%ld generation=%ld",
                    statusFamilyText,
                    (long)outcome->state,
                    (long)outcome->generation);
            }
            return result;

        case _FXCFControlNotifyOutcomeKindValidity:
            size = 48u;
            result = (char *)calloc(size, sizeof(char));
            if (result != NULL) {
                snprintf(result, size, "notify %s validity valid=%s",
                    statusFamilyText,
                    outcome->valid ? "true" : "false");
            }
            return result;

        default:
            return NULL;
    }
}

static Boolean __FXCFControlResponseAppendSuccessEnvelopeJSON(struct __FXCFTextBuffer *buffer, const struct _FXCFControlResponse *response) {
    enum _FXCFControlValueKind kind = _FXCFControlPacketValueKind(response->result);
    const char *kindText = __FXCFControlValueKindText(kind);

    if (!__FXCFTextBufferAppendFormat(buffer, "{\"protocol_version\":%ld", (long)response->protocolVersion)) {
        return false;
    }

    switch (kind) {
        case _FXCFControlValueKindObject: {
            char **keys = NULL;
            CFIndex count = 0;
            if (!__FXCFControlPacketCopySortedKeys((CFDictionaryRef)response->result, &keys, &count)) {
                return false;
            }
            Boolean ok =
                __FXCFTextBufferAppendFormat(buffer, ",\"result_count\":%ld,\"result_keys\":", (long)count) &&
                __FXCFControlPacketAppendKeyArrayJSON(buffer, keys, count) &&
                __FXCFTextBufferAppendFormat(buffer, ",\"result_kind\":\"%s\",\"status\":\"ok\"}", kindText);
            __FXCFControlPacketFreeSortedKeys(keys, count);
            return ok;
        }
        case _FXCFControlValueKindArray:
            return __FXCFTextBufferAppendFormat(
                buffer,
                ",\"result_count\":%ld,\"result_kind\":\"%s\",\"status\":\"ok\"}",
                (long)CFArrayGetCount((CFArrayRef)response->result),
                kindText
            );
        case _FXCFControlValueKindString:
            return __FXCFTextBufferAppendFormat(
                buffer,
                ",\"result_kind\":\"%s\",\"result_length\":%ld,\"status\":\"ok\"}",
                kindText,
                (long)CFStringGetLength((CFStringRef)response->result)
            );
        case _FXCFControlValueKindData:
            return __FXCFTextBufferAppendFormat(
                buffer,
                ",\"result_kind\":\"%s\",\"result_size\":%ld,\"status\":\"ok\"}",
                kindText,
                (long)CFDataGetLength((CFDataRef)response->result)
            );
        case _FXCFControlValueKindInteger:
        case _FXCFControlValueKindDouble:
        case _FXCFControlValueKindBool:
        case _FXCFControlValueKindDate:
            return __FXCFTextBufferAppendFormat(buffer, ",\"result_kind\":\"%s\",\"status\":\"ok\"}", kindText);
        default:
            return false;
    }
}

char *_FXCFControlResponseCopyEnvelopeJSON(const struct _FXCFControlResponse *response) {
    struct __FXCFTextBuffer buffer = {0};
    char *codeText = NULL;
    char *messageText = NULL;
    char *result = NULL;

    if (response == NULL || response->packet == NULL) {
        return NULL;
    }

    if (response->status == _FXCFControlResponseStatusOK) {
        if (response->result == NULL || !__FXCFControlResponseAppendSuccessEnvelopeJSON(&buffer, response)) {
            __FXCFTextBufferFree(&buffer);
            return NULL;
        }
    } else if (response->status == _FXCFControlResponseStatusError) {
        if (response->errorCode == NULL || response->errorMessage == NULL) {
            return NULL;
        }
        codeText = __FXCFControlPacketCopyCString(response->errorCode);
        messageText = __FXCFControlPacketCopyCString(response->errorMessage);
        if (codeText == NULL || messageText == NULL) {
            __FXCFTextBufferFree(&buffer);
            free(codeText);
            free(messageText);
            return NULL;
        }
        if (!__FXCFTextBufferAppendCString(&buffer, "{\"code\":") ||
            !__FXCFTextBufferAppendJSONString(&buffer, codeText) ||
            !__FXCFTextBufferAppendCString(&buffer, ",\"message\":") ||
            !__FXCFTextBufferAppendJSONString(&buffer, messageText) ||
            !__FXCFTextBufferAppendFormat(&buffer, ",\"protocol_version\":%ld,\"status\":\"error\"}", (long)response->protocolVersion)) {
            __FXCFTextBufferFree(&buffer);
            free(codeText);
            free(messageText);
            return NULL;
        }
        free(codeText);
        free(messageText);
    } else {
        return NULL;
    }

    result = __FXCFTextBufferDetach(&buffer);
    __FXCFTextBufferFree(&buffer);
    return result;
}

char *_FXCFControlResponseCopyEnvelopeSummary(const struct _FXCFControlResponse *response) {
    enum _FXCFControlValueKind kind = _FXCFControlValueKindInvalid;
    char *codeText = NULL;
    char *result = NULL;
    size_t size = 0u;

    if (response == NULL || response->packet == NULL) {
        return NULL;
    }

    if (response->status == _FXCFControlResponseStatusError) {
        if (response->errorCode == NULL) {
            return NULL;
        }
        codeText = __FXCFControlPacketCopyCString(response->errorCode);
        if (codeText == NULL) {
            return NULL;
        }
        size = strlen(codeText) + 24u;
        result = (char *)calloc(size, sizeof(char));
        if (result != NULL) {
            snprintf(result, size, "response error code=%s", codeText);
        }
        free(codeText);
        return result;
    }

    if (response->status != _FXCFControlResponseStatusOK || response->result == NULL) {
        return NULL;
    }

    kind = _FXCFControlPacketValueKind(response->result);
    switch (kind) {
        case _FXCFControlValueKindObject: {
            CFIndex count = CFDictionaryGetCount((CFDictionaryRef)response->result);
            size = 64u;
            result = (char *)calloc(size, sizeof(char));
            if (result != NULL) {
                snprintf(result, size, "response ok result=object count=%ld", (long)count);
            }
            return result;
        }
        case _FXCFControlValueKindArray: {
            CFIndex count = CFArrayGetCount((CFArrayRef)response->result);
            size = 64u;
            result = (char *)calloc(size, sizeof(char));
            if (result != NULL) {
                snprintf(result, size, "response ok result=array count=%ld", (long)count);
            }
            return result;
        }
        case _FXCFControlValueKindString: {
            CFIndex length = CFStringGetLength((CFStringRef)response->result);
            size = 64u;
            result = (char *)calloc(size, sizeof(char));
            if (result != NULL) {
                snprintf(result, size, "response ok result=string length=%ld", (long)length);
            }
            return result;
        }
        case _FXCFControlValueKindData: {
            CFIndex sizeValue = CFDataGetLength((CFDataRef)response->result);
            size = 64u;
            result = (char *)calloc(size, sizeof(char));
            if (result != NULL) {
                snprintf(result, size, "response ok result=data size=%ld", (long)sizeValue);
            }
            return result;
        }
        default: {
            const char *kindText = __FXCFControlValueKindText(kind);
            size = strlen(kindText) + 24u;
            result = (char *)calloc(size, sizeof(char));
            if (result != NULL) {
                snprintf(result, size, "response ok result=%s", kindText);
            }
            return result;
        }
    }
}

char *_FXCFControlPacketCopyAcceptedSummary(CFDictionaryRef packet, enum _FXCFControlPacketKind kind) {
    char *serviceText = NULL;
    char *methodText = NULL;
    char *statusText = NULL;
    char *codeText = NULL;
    char *result = NULL;
    CFStringRef value = NULL;
    struct _FXCFControlPacketError ignored = {0};

    if (kind == _FXCFControlPacketKindRequest) {
        if (!__FXCFControlPacketRequireString(packet, "service", &value, &ignored)) {
            return NULL;
        }
        serviceText = __FXCFControlPacketCopyCString(value);
        if (!__FXCFControlPacketRequireString(packet, "method", &value, &ignored)) {
            free(serviceText);
            return NULL;
        }
        methodText = __FXCFControlPacketCopyCString(value);
        if (serviceText == NULL || methodText == NULL) {
            free(serviceText);
            free(methodText);
            return NULL;
        }
        size_t size = strlen(serviceText) + strlen(methodText) + 32u;
        result = (char *)calloc(size, sizeof(char));
        if (result != NULL) {
            snprintf(result, size, "accept request service=%s method=%s", serviceText, methodText);
        }
        free(serviceText);
        free(methodText);
        return result;
    }

    if (!__FXCFControlPacketRequireString(packet, "status", &value, &ignored)) {
        return NULL;
    }
    statusText = __FXCFControlPacketCopyCString(value);
    if (statusText == NULL) {
        return NULL;
    }
    if (strcmp(statusText, "ok") == 0) {
        result = (char *)calloc(26u, sizeof(char));
        if (result != NULL) {
            snprintf(result, 26u, "%s", "accept response status=ok");
        }
        free(statusText);
        return result;
    }

    if (strcmp(statusText, "error") == 0) {
        CFDictionaryRef error = NULL;
        if (!__FXCFControlPacketRequireDictionary(packet, "error", &error, &ignored) ||
            !__FXCFControlPacketRequireString(error, "code", &value, &ignored)) {
            free(statusText);
            return NULL;
        }
        codeText = __FXCFControlPacketCopyCString(value);
        if (codeText == NULL) {
            free(statusText);
            return NULL;
        }
        size_t size = strlen(codeText) + 34u;
        result = (char *)calloc(size, sizeof(char));
        if (result != NULL) {
            snprintf(result, size, "accept response status=error code=%s", codeText);
        }
        free(codeText);
        free(statusText);
        return result;
    }

    free(statusText);
    return NULL;
}

char *_FXCFControlPacketCopyErrorJSON(const struct _FXCFControlPacketError *error) {
    struct __FXCFTextBuffer buffer = {0};

    if (error == NULL) {
        return NULL;
    }

    switch (error->kind) {
        case _FXCFControlPacketErrorMissingKey:
            if (!__FXCFTextBufferAppendCString(&buffer, "{\"key\":") ||
                !__FXCFTextBufferAppendJSONString(&buffer, error->text) ||
                !__FXCFTextBufferAppendCString(&buffer, ",\"type\":\"missing_key\"}")) {
                __FXCFTextBufferFree(&buffer);
                return NULL;
            }
            break;
        case _FXCFControlPacketErrorUnsupportedVersion:
            if (!__FXCFTextBufferAppendFormat(&buffer, "{\"type\":\"unsupported_version\",\"version\":%ld}", (long)error->version)) {
                __FXCFTextBufferFree(&buffer);
                return NULL;
            }
            break;
        case _FXCFControlPacketErrorInvalidPacket:
            if (!__FXCFTextBufferAppendCString(&buffer, "{\"detail\":") ||
                !__FXCFTextBufferAppendJSONString(&buffer, error->text) ||
                !__FXCFTextBufferAppendCString(&buffer, ",\"type\":\"invalid_packet\"}")) {
                __FXCFTextBufferFree(&buffer);
                return NULL;
            }
            break;
        default:
            __FXCFTextBufferFree(&buffer);
            return NULL;
    }

    return __FXCFTextBufferDetach(&buffer);
}

char *_FXCFControlPacketCopyErrorSummary(enum _FXCFControlPacketKind kind, const struct _FXCFControlPacketError *error) {
    const char *kindText = kind == _FXCFControlPacketKindRequest ? "request" : "response";
    char *result = NULL;
    size_t size = 0u;

    if (error == NULL) {
        return NULL;
    }

    switch (error->kind) {
        case _FXCFControlPacketErrorMissingKey:
            size = strlen(kindText) + strlen(error->text) + 22u;
            result = (char *)calloc(size, sizeof(char));
            if (result != NULL) {
                snprintf(result, size, "reject %s missing_key %s", kindText, error->text);
            }
            return result;
        case _FXCFControlPacketErrorUnsupportedVersion:
            size = strlen(kindText) + 40u;
            result = (char *)calloc(size, sizeof(char));
            if (result != NULL) {
                snprintf(result, size, "reject %s unsupported_version %ld", kindText, (long)error->version);
            }
            return result;
        case _FXCFControlPacketErrorInvalidPacket:
            size = strlen(kindText) + strlen(error->text) + 26u;
            result = (char *)calloc(size, sizeof(char));
            if (result != NULL) {
                snprintf(result, size, "reject %s invalid_packet %s", kindText, error->text);
            }
            return result;
        default:
            return NULL;
    }
}
