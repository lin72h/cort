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
