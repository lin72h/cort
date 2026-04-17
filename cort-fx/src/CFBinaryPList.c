#include "FXCFInternal.h"

#include <CoreFoundation/CFArray.h>
#include <CoreFoundation/CFData.h>
#include <CoreFoundation/CFDate.h>
#include <CoreFoundation/CFDictionary.h>
#include <CoreFoundation/CFError.h>
#include <CoreFoundation/CFNumber.h>
#include <CoreFoundation/CFPropertyList.h>
#include <CoreFoundation/CFString.h>

enum {
    FX_BPLIST_MAX_DEPTH = 64
};

struct FXBplistObject {
    CFTypeRef object;
    size_t offset;
};

struct FXBplistCollector {
    struct FXBplistObject *objects;
    size_t count;
    size_t capacity;
    CFTypeRef stack[FX_BPLIST_MAX_DEPTH];
    size_t stackDepth;
};

struct FXBplistBuffer {
    UInt8 *bytes;
    size_t length;
    size_t capacity;
};

struct FXBplistParser {
    const UInt8 *bytes;
    size_t length;
    CFAllocatorRef allocator;
    UInt8 offsetIntSize;
    UInt8 objectRefSize;
    uint64_t numObjects;
    uint64_t topObject;
    uint64_t offsetTableOffset;
    uint64_t *offsets;
    CFTypeRef *cache;
    Boolean *inProgress;
};

static void __FXBplistSetError(CFErrorRef *errorOut, CFAllocatorRef allocator, CFIndex code) {
    if (errorOut == NULL || *errorOut != NULL) {
        return;
    }
    *errorOut = _FXCFErrorCreateCode(allocator, code);
}

static Boolean __FXBplistBufferReserve(struct FXBplistBuffer *buffer, size_t additional) {
    if (additional > SIZE_MAX - buffer->length) {
        return false;
    }
    size_t required = buffer->length + additional;
    if (required <= buffer->capacity) {
        return true;
    }

    size_t newCapacity = buffer->capacity > 0 ? buffer->capacity : 64;
    while (newCapacity < required) {
        if (newCapacity > SIZE_MAX / 2) {
            newCapacity = required;
            break;
        }
        newCapacity *= 2;
    }

    UInt8 *newBytes = (UInt8 *)realloc(buffer->bytes, newCapacity);
    if (newBytes == NULL) {
        return false;
    }
    buffer->bytes = newBytes;
    buffer->capacity = newCapacity;
    return true;
}

static Boolean __FXBplistBufferAppendBytes(struct FXBplistBuffer *buffer, const UInt8 *bytes, size_t length) {
    if (length == 0) {
        return true;
    }
    if (bytes == NULL || !__FXBplistBufferReserve(buffer, length)) {
        return false;
    }
    memcpy(buffer->bytes + buffer->length, bytes, length);
    buffer->length += length;
    return true;
}

static Boolean __FXBplistBufferAppendByte(struct FXBplistBuffer *buffer, UInt8 byte) {
    return __FXBplistBufferAppendBytes(buffer, &byte, 1);
}

static Boolean __FXBplistBufferAppendBEUnsigned(struct FXBplistBuffer *buffer, uint64_t value, size_t byteCount) {
    UInt8 bytes[8];
    if (byteCount == 0 || byteCount > sizeof(bytes)) {
        return false;
    }
    for (size_t index = 0; index < byteCount; ++index) {
        size_t shift = (byteCount - index - 1) * 8u;
        bytes[index] = (UInt8)((value >> shift) & 0xffu);
    }
    return __FXBplistBufferAppendBytes(buffer, bytes, byteCount);
}

static Boolean __FXBplistBufferAppendBEDouble(struct FXBplistBuffer *buffer, double value) {
    union {
        double value;
        uint64_t bits;
    } storage;
    storage.value = value;
    return __FXBplistBufferAppendBEUnsigned(buffer, storage.bits, 8);
}

static size_t __FXBplistMinimalBytesForUnsigned(uint64_t value) {
    if (value <= UINT8_MAX) {
        return 1;
    }
    if (value <= UINT16_MAX) {
        return 2;
    }
    if (value <= UINT32_MAX) {
        return 4;
    }
    return 8;
}

static UInt8 __FXBplistIntegerExponentForBytes(size_t byteCount) {
    switch (byteCount) {
        case 1:
            return 0;
        case 2:
            return 1;
        case 4:
            return 2;
        case 8:
            return 3;
        default:
            _FXCFAbort("invalid binary-plist integer byte count");
    }
}

static Boolean __FXBplistAppendCountObject(struct FXBplistBuffer *buffer, uint64_t value) {
    size_t byteCount = __FXBplistMinimalBytesForUnsigned(value);
    return
        __FXBplistBufferAppendByte(buffer, (UInt8)(0x10u | __FXBplistIntegerExponentForBytes(byteCount))) &&
        __FXBplistBufferAppendBEUnsigned(buffer, value, byteCount);
}

static Boolean __FXBplistAppendLengthMarker(struct FXBplistBuffer *buffer, UInt8 family, uint64_t count) {
    if (count < 15u) {
        return __FXBplistBufferAppendByte(buffer, (UInt8)((family << 4u) | (UInt8)count));
    }
    return
        __FXBplistBufferAppendByte(buffer, (UInt8)((family << 4u) | 0x0fu)) &&
        __FXBplistAppendCountObject(buffer, count);
}

static size_t __FXBplistFindObjectIndex(const struct FXBplistCollector *collector, CFTypeRef object) {
    for (size_t index = 0; index < collector->count; ++index) {
        if (collector->objects[index].object == object) {
            return index;
        }
    }
    return SIZE_MAX;
}

static Boolean __FXBplistStackContains(const struct FXBplistCollector *collector, CFTypeRef object) {
    for (size_t index = 0; index < collector->stackDepth; ++index) {
        if (collector->stack[index] == object) {
            return true;
        }
    }
    return false;
}

static Boolean __FXBplistCollectorAppend(struct FXBplistCollector *collector, CFTypeRef object, size_t *indexOut) {
    if (collector->count == collector->capacity) {
        size_t newCapacity = collector->capacity > 0 ? collector->capacity * 2 : 16;
        struct FXBplistObject *newObjects =
            (struct FXBplistObject *)realloc(collector->objects, newCapacity * sizeof(struct FXBplistObject));
        if (newObjects == NULL) {
            return false;
        }
        collector->objects = newObjects;
        collector->capacity = newCapacity;
    }

    collector->objects[collector->count].object = object;
    collector->objects[collector->count].offset = 0;
    *indexOut = collector->count;
    collector->count += 1;
    return true;
}

static Boolean __FXBplistCollectObject(struct FXBplistCollector *collector, CFTypeRef object, CFAllocatorRef allocator, CFErrorRef *errorOut) {
    if (object == NULL) {
        __FXBplistSetError(errorOut, allocator, kCFPropertyListWriteStreamError);
        return false;
    }

    if (__FXBplistStackContains(collector, object)) {
        __FXBplistSetError(errorOut, allocator, kCFPropertyListWriteStreamError);
        return false;
    }

    if (__FXBplistFindObjectIndex(collector, object) != SIZE_MAX) {
        return true;
    }

    CFTypeID typeID = CFGetTypeID(object);
    size_t objectIndex = 0;
    if (!__FXBplistCollectorAppend(collector, object, &objectIndex)) {
        __FXBplistSetError(errorOut, allocator, kCFPropertyListWriteStreamError);
        return false;
    }

    if (typeID == CFArrayGetTypeID()) {
        if (collector->stackDepth >= FX_BPLIST_MAX_DEPTH) {
            __FXBplistSetError(errorOut, allocator, kCFPropertyListWriteStreamError);
            return false;
        }

        CFArrayRef array = (CFArrayRef)object;
        collector->stack[collector->stackDepth++] = object;
        CFIndex count = CFArrayGetCount(array);
        for (CFIndex index = 0; index < count; ++index) {
            if (!__FXBplistCollectObject(collector, (CFTypeRef)CFArrayGetValueAtIndex(array, index), allocator, errorOut)) {
                collector->stackDepth -= 1;
                return false;
            }
        }
        collector->stackDepth -= 1;
        return true;
    }

    if (typeID == CFDictionaryGetTypeID()) {
        if (collector->stackDepth >= FX_BPLIST_MAX_DEPTH) {
            __FXBplistSetError(errorOut, allocator, kCFPropertyListWriteStreamError);
            return false;
        }

        CFDictionaryRef dictionary = (CFDictionaryRef)object;
        collector->stack[collector->stackDepth++] = object;
        CFIndex count = CFDictionaryGetCount(dictionary);
        for (CFIndex index = 0; index < count; ++index) {
            const void *key = NULL;
            const void *value = NULL;
            if (!_FXCFDictionaryGetEntryAtIndex(dictionary, index, &key, &value) ||
                key == NULL ||
                value == NULL ||
                CFGetTypeID((CFTypeRef)key) != CFStringGetTypeID() ||
                !__FXBplistCollectObject(collector, (CFTypeRef)key, allocator, errorOut) ||
                !__FXBplistCollectObject(collector, (CFTypeRef)value, allocator, errorOut)) {
                collector->stackDepth -= 1;
                __FXBplistSetError(errorOut, allocator, kCFPropertyListWriteStreamError);
                return false;
            }
        }
        collector->stackDepth -= 1;
        return true;
    }

    if (typeID == CFBooleanGetTypeID() ||
        typeID == CFNumberGetTypeID() ||
        typeID == CFDateGetTypeID() ||
        typeID == CFDataGetTypeID() ||
        typeID == CFStringGetTypeID()) {
        (void)objectIndex;
        return true;
    }

    __FXBplistSetError(errorOut, allocator, kCFPropertyListWriteStreamError);
    return false;
}

static Boolean __FXBplistEncodeString(struct FXBplistBuffer *buffer, CFStringRef string) {
    CFIndex length = CFStringGetLength(string);
    if (length < 0) {
        return false;
    }

    UInt8 *asciiBytes = NULL;
    CFIndex converted = 0;
    CFIndex used = 0;
    if (length > 0) {
        asciiBytes = (UInt8 *)malloc((size_t)length);
        if (asciiBytes == NULL) {
            return false;
        }
    }

    converted = CFStringGetBytes(
        string,
        CFRangeMake(0, length),
        kCFStringEncodingASCII,
        0,
        false,
        asciiBytes,
        length,
        &used
    );

    if (converted == length && used == length) {
        Boolean ok =
            __FXBplistAppendLengthMarker(buffer, 0x5u, (uint64_t)length) &&
            __FXBplistBufferAppendBytes(buffer, asciiBytes, (size_t)used);
        free(asciiBytes);
        return ok;
    }

    free(asciiBytes);

    UniChar *characters = NULL;
    if (length > 0) {
        characters = (UniChar *)malloc((size_t)length * sizeof(UniChar));
        if (characters == NULL) {
            return false;
        }
        CFStringGetCharacters(string, CFRangeMake(0, length), characters);
    }

    if (!__FXBplistAppendLengthMarker(buffer, 0x6u, (uint64_t)length)) {
        free(characters);
        return false;
    }

    for (CFIndex index = 0; index < length; ++index) {
        if (!__FXBplistBufferAppendBEUnsigned(buffer, (uint64_t)characters[index], 2)) {
            free(characters);
            return false;
        }
    }

    free(characters);
    return true;
}

static Boolean __FXBplistEncodeArray(struct FXBplistBuffer *buffer, const struct FXBplistCollector *collector, CFArrayRef array, UInt8 objectRefSize) {
    CFIndex count = CFArrayGetCount(array);
    if (count < 0 || !__FXBplistAppendLengthMarker(buffer, 0xAu, (uint64_t)count)) {
        return false;
    }

    for (CFIndex index = 0; index < count; ++index) {
        size_t childIndex = __FXBplistFindObjectIndex(collector, (CFTypeRef)CFArrayGetValueAtIndex(array, index));
        if (childIndex == SIZE_MAX || !__FXBplistBufferAppendBEUnsigned(buffer, (uint64_t)childIndex, objectRefSize)) {
            return false;
        }
    }
    return true;
}

static Boolean __FXBplistEncodeDictionary(struct FXBplistBuffer *buffer, const struct FXBplistCollector *collector, CFDictionaryRef dictionary, UInt8 objectRefSize) {
    CFIndex count = CFDictionaryGetCount(dictionary);
    if (count < 0 || !__FXBplistAppendLengthMarker(buffer, 0xDu, (uint64_t)count)) {
        return false;
    }

    for (CFIndex index = 0; index < count; ++index) {
        const void *key = NULL;
        const void *value = NULL;
        if (!_FXCFDictionaryGetEntryAtIndex(dictionary, index, &key, &value)) {
            return false;
        }

        size_t keyIndex = __FXBplistFindObjectIndex(collector, (CFTypeRef)key);
        if (keyIndex == SIZE_MAX || !__FXBplistBufferAppendBEUnsigned(buffer, (uint64_t)keyIndex, objectRefSize)) {
            return false;
        }
    }

    for (CFIndex index = 0; index < count; ++index) {
        const void *key = NULL;
        const void *value = NULL;
        if (!_FXCFDictionaryGetEntryAtIndex(dictionary, index, &key, &value)) {
            return false;
        }

        size_t valueIndex = __FXBplistFindObjectIndex(collector, (CFTypeRef)value);
        if (valueIndex == SIZE_MAX || !__FXBplistBufferAppendBEUnsigned(buffer, (uint64_t)valueIndex, objectRefSize)) {
            return false;
        }
    }
    return true;
}

static Boolean __FXBplistEncodeObject(struct FXBplistBuffer *buffer, const struct FXBplistCollector *collector, CFTypeRef object, UInt8 objectRefSize) {
    CFTypeID typeID = CFGetTypeID(object);

    if (typeID == CFBooleanGetTypeID()) {
        return __FXBplistBufferAppendByte(buffer, object == (CFTypeRef)kCFBooleanTrue ? 0x09u : 0x08u);
    }

    if (typeID == CFNumberGetTypeID()) {
        CFNumberType numberType = CFNumberGetType((CFNumberRef)object);
        if (numberType == kCFNumberFloat64Type) {
            double realValue = 0.0;
            return
                CFNumberGetValue((CFNumberRef)object, kCFNumberFloat64Type, &realValue) &&
                __FXBplistBufferAppendByte(buffer, 0x23u) &&
                __FXBplistBufferAppendBEDouble(buffer, realValue);
        }

        SInt64 integerValue = 0;
        return
            CFNumberGetValue((CFNumberRef)object, kCFNumberSInt64Type, &integerValue) &&
            __FXBplistBufferAppendByte(buffer, 0x13u) &&
            __FXBplistBufferAppendBEUnsigned(buffer, (uint64_t)integerValue, 8);
    }

    if (typeID == CFDateGetTypeID()) {
        return
            __FXBplistBufferAppendByte(buffer, 0x33u) &&
            __FXBplistBufferAppendBEDouble(buffer, CFDateGetAbsoluteTime((CFDateRef)object));
    }

    if (typeID == CFDataGetTypeID()) {
        CFDataRef data = (CFDataRef)object;
        CFIndex length = CFDataGetLength(data);
        return
            length >= 0 &&
            __FXBplistAppendLengthMarker(buffer, 0x4u, (uint64_t)length) &&
            __FXBplistBufferAppendBytes(buffer, CFDataGetBytePtr(data), (size_t)length);
    }

    if (typeID == CFStringGetTypeID()) {
        return __FXBplistEncodeString(buffer, (CFStringRef)object);
    }

    if (typeID == CFArrayGetTypeID()) {
        return __FXBplistEncodeArray(buffer, collector, (CFArrayRef)object, objectRefSize);
    }

    if (typeID == CFDictionaryGetTypeID()) {
        return __FXBplistEncodeDictionary(buffer, collector, (CFDictionaryRef)object, objectRefSize);
    }

    return false;
}

static Boolean __FXBplistReadBEUnsigned(const UInt8 *bytes, size_t byteCount, uint64_t *valueOut) {
    if (byteCount == 0 || byteCount > 8 || bytes == NULL || valueOut == NULL) {
        return false;
    }

    uint64_t value = 0;
    for (size_t index = 0; index < byteCount; ++index) {
        value = (value << 8u) | bytes[index];
    }
    *valueOut = value;
    return true;
}

static Boolean __FXBplistReadBESigned(const UInt8 *bytes, size_t byteCount, SInt64 *valueOut) {
    uint64_t value = 0;
    if (!__FXBplistReadBEUnsigned(bytes, byteCount, &value) || valueOut == NULL) {
        return false;
    }

    if (byteCount < 8 && (bytes[0] & 0x80u) != 0) {
        uint64_t mask = ~((1ULL << (byteCount * 8u)) - 1u);
        value |= mask;
    }

    *valueOut = (SInt64)value;
    return true;
}

static double __FXBplistReadBEDouble(const UInt8 *bytes) {
    union {
        uint64_t bits;
        double value;
    } storage;
    storage.bits = 0;
    (void)__FXBplistReadBEUnsigned(bytes, 8, &storage.bits);
    return storage.value;
}

static Boolean __FXBplistResolveLength(const struct FXBplistParser *parser, size_t offset, UInt8 lowNibble, uint64_t *countOut, size_t *headerSizeOut) {
    if (countOut == NULL || headerSizeOut == NULL || offset >= parser->length) {
        return false;
    }

    if (lowNibble < 0x0fu) {
        *countOut = lowNibble;
        *headerSizeOut = 1;
        return true;
    }

    if (offset + 2 > parser->length) {
        return false;
    }

    UInt8 intMarker = parser->bytes[offset + 1];
    if ((intMarker >> 4u) != 0x1u) {
        return false;
    }

    size_t byteCount = (size_t)1u << (intMarker & 0x0fu);
    if (byteCount == 0 || byteCount > 8 || offset + 2 + byteCount > parser->length) {
        return false;
    }

    if (!__FXBplistReadBEUnsigned(parser->bytes + offset + 2, byteCount, countOut)) {
        return false;
    }

    *headerSizeOut = 2 + byteCount;
    return true;
}

static CFTypeRef __FXBplistParseObjectAtIndex(struct FXBplistParser *parser, uint64_t objectIndex, CFErrorRef *errorOut);

static CFTypeRef __FXBplistParseArray(struct FXBplistParser *parser, uint64_t objectIndex, size_t offset, UInt8 lowNibble, CFErrorRef *errorOut) {
    uint64_t count = 0;
    size_t headerSize = 0;
    if (!__FXBplistResolveLength(parser, offset, lowNibble, &count, &headerSize)) {
        __FXBplistSetError(errorOut, parser->allocator, kCFPropertyListReadCorruptError);
        return NULL;
    }

    if (count > (uint64_t)LONG_MAX || count > (SIZE_MAX / sizeof(CFTypeRef))) {
        __FXBplistSetError(errorOut, parser->allocator, kCFPropertyListReadCorruptError);
        return NULL;
    }

    if (count > 0 && offset + headerSize > parser->length) {
        __FXBplistSetError(errorOut, parser->allocator, kCFPropertyListReadCorruptError);
        return NULL;
    }

    size_t refsOffset = offset + headerSize;
    if (count > 0 && ((uint64_t)refsOffset + count * parser->objectRefSize > parser->length)) {
        __FXBplistSetError(errorOut, parser->allocator, kCFPropertyListReadCorruptError);
        return NULL;
    }

    const void **values = NULL;
    if (count > 0) {
        values = (const void **)calloc((size_t)count, sizeof(const void *));
        if (values == NULL) {
            __FXBplistSetError(errorOut, parser->allocator, kCFPropertyListReadCorruptError);
            return NULL;
        }
    }

    for (uint64_t index = 0; index < count; ++index) {
        uint64_t childIndex = 0;
        if (!__FXBplistReadBEUnsigned(parser->bytes + refsOffset + index * parser->objectRefSize, parser->objectRefSize, &childIndex) ||
            childIndex >= parser->numObjects) {
            free(values);
            __FXBplistSetError(errorOut, parser->allocator, kCFPropertyListReadCorruptError);
            return NULL;
        }
        values[index] = __FXBplistParseObjectAtIndex(parser, childIndex, errorOut);
        if (values[index] == NULL) {
            free(values);
            return NULL;
        }
    }

    CFArrayRef array = CFArrayCreate(parser->allocator, values, (CFIndex)count, &kCFTypeArrayCallBacks);
    free(values);
    if (array == NULL) {
        __FXBplistSetError(errorOut, parser->allocator, kCFPropertyListReadCorruptError);
        return NULL;
    }
    parser->cache[objectIndex] = (CFTypeRef)array;
    return (CFTypeRef)array;
}

static CFTypeRef __FXBplistParseDictionary(struct FXBplistParser *parser, uint64_t objectIndex, size_t offset, UInt8 lowNibble, CFErrorRef *errorOut) {
    uint64_t count = 0;
    size_t headerSize = 0;
    if (!__FXBplistResolveLength(parser, offset, lowNibble, &count, &headerSize)) {
        __FXBplistSetError(errorOut, parser->allocator, kCFPropertyListReadCorruptError);
        return NULL;
    }

    if (count > (uint64_t)LONG_MAX || count > (SIZE_MAX / sizeof(const void *))) {
        __FXBplistSetError(errorOut, parser->allocator, kCFPropertyListReadCorruptError);
        return NULL;
    }

    size_t refsOffset = offset + headerSize;
    uint64_t refsLength = count * parser->objectRefSize;
    if ((uint64_t)refsOffset + refsLength * 2u > parser->length) {
        __FXBplistSetError(errorOut, parser->allocator, kCFPropertyListReadCorruptError);
        return NULL;
    }

    const void **keys = NULL;
    const void **values = NULL;
    if (count > 0) {
        keys = (const void **)calloc((size_t)count, sizeof(const void *));
        values = (const void **)calloc((size_t)count, sizeof(const void *));
        if (keys == NULL || values == NULL) {
            free(values);
            free(keys);
            __FXBplistSetError(errorOut, parser->allocator, kCFPropertyListReadCorruptError);
            return NULL;
        }
    }

    for (uint64_t index = 0; index < count; ++index) {
        uint64_t keyIndex = 0;
        uint64_t valueIndex = 0;
        if (!__FXBplistReadBEUnsigned(parser->bytes + refsOffset + index * parser->objectRefSize, parser->objectRefSize, &keyIndex) ||
            !__FXBplistReadBEUnsigned(parser->bytes + refsOffset + refsLength + index * parser->objectRefSize, parser->objectRefSize, &valueIndex) ||
            keyIndex >= parser->numObjects ||
            valueIndex >= parser->numObjects) {
            free(values);
            free(keys);
            __FXBplistSetError(errorOut, parser->allocator, kCFPropertyListReadCorruptError);
            return NULL;
        }

        keys[index] = __FXBplistParseObjectAtIndex(parser, keyIndex, errorOut);
        values[index] = __FXBplistParseObjectAtIndex(parser, valueIndex, errorOut);
        if (keys[index] == NULL || values[index] == NULL || CFGetTypeID((CFTypeRef)keys[index]) != CFStringGetTypeID()) {
            free(values);
            free(keys);
            __FXBplistSetError(errorOut, parser->allocator, kCFPropertyListReadCorruptError);
            return NULL;
        }
    }

    CFDictionaryRef dictionary = CFDictionaryCreate(
        parser->allocator,
        keys,
        values,
        (CFIndex)count,
        &kCFTypeDictionaryKeyCallBacks,
        &kCFTypeDictionaryValueCallBacks
    );
    free(values);
    free(keys);
    if (dictionary == NULL) {
        __FXBplistSetError(errorOut, parser->allocator, kCFPropertyListReadCorruptError);
        return NULL;
    }
    parser->cache[objectIndex] = (CFTypeRef)dictionary;
    return (CFTypeRef)dictionary;
}

static CFTypeRef __FXBplistParseObjectAtIndex(struct FXBplistParser *parser, uint64_t objectIndex, CFErrorRef *errorOut) {
    if (objectIndex >= parser->numObjects) {
        __FXBplistSetError(errorOut, parser->allocator, kCFPropertyListReadCorruptError);
        return NULL;
    }

    if (parser->cache[objectIndex] != NULL) {
        return parser->cache[objectIndex];
    }

    if (parser->inProgress[objectIndex]) {
        __FXBplistSetError(errorOut, parser->allocator, kCFPropertyListReadCorruptError);
        return NULL;
    }

    parser->inProgress[objectIndex] = true;

    size_t offset = (size_t)parser->offsets[objectIndex];
    if (offset >= parser->length || offset < 8u || offset >= parser->offsetTableOffset) {
        parser->inProgress[objectIndex] = false;
        __FXBplistSetError(errorOut, parser->allocator, kCFPropertyListReadCorruptError);
        return NULL;
    }

    UInt8 marker = parser->bytes[offset];
    UInt8 family = marker >> 4u;
    UInt8 lowNibble = marker & 0x0fu;
    CFTypeRef object = NULL;

    switch (family) {
        case 0x0u:
            if (marker == 0x08u) {
                object = CFRetain((CFTypeRef)kCFBooleanFalse);
            } else if (marker == 0x09u) {
                object = CFRetain((CFTypeRef)kCFBooleanTrue);
            }
            break;
        case 0x1u: {
            size_t byteCount = (size_t)1u << lowNibble;
            SInt64 value = 0;
            if ((byteCount == 1 || byteCount == 2 || byteCount == 4 || byteCount == 8) &&
                offset + 1 + byteCount <= parser->length &&
                __FXBplistReadBESigned(parser->bytes + offset + 1, byteCount, &value)) {
                object = (CFTypeRef)CFNumberCreate(parser->allocator, kCFNumberSInt64Type, &value);
            }
            break;
        }
        case 0x2u: {
            size_t byteCount = (size_t)1u << lowNibble;
            if (offset + 1 + byteCount <= parser->length && (byteCount == 4 || byteCount == 8)) {
                double value = 0.0;
                if (byteCount == 8) {
                    value = __FXBplistReadBEDouble(parser->bytes + offset + 1);
                } else {
                    union {
                        uint32_t bits;
                        float value;
                    } storage;
                    uint64_t bits = 0;
                    if (!__FXBplistReadBEUnsigned(parser->bytes + offset + 1, 4, &bits)) {
                        break;
                    }
                    storage.bits = (uint32_t)bits;
                    value = storage.value;
                }
                object = (CFTypeRef)CFNumberCreate(parser->allocator, kCFNumberFloat64Type, &value);
            }
            break;
        }
        case 0x3u:
            if (marker == 0x33u && offset + 9 <= parser->length) {
                double value = __FXBplistReadBEDouble(parser->bytes + offset + 1);
                object = (CFTypeRef)CFDateCreate(parser->allocator, value);
            }
            break;
        case 0x4u: {
            uint64_t count = 0;
            size_t headerSize = 0;
            if (__FXBplistResolveLength(parser, offset, lowNibble, &count, &headerSize) &&
                count <= (uint64_t)LONG_MAX &&
                (uint64_t)(offset + headerSize) + count <= parser->length) {
                object = (CFTypeRef)CFDataCreate(parser->allocator, parser->bytes + offset + headerSize, (CFIndex)count);
            }
            break;
        }
        case 0x5u: {
            uint64_t count = 0;
            size_t headerSize = 0;
            if (__FXBplistResolveLength(parser, offset, lowNibble, &count, &headerSize) &&
                count <= (uint64_t)LONG_MAX &&
                (uint64_t)(offset + headerSize) + count <= parser->length) {
                object = (CFTypeRef)CFStringCreateWithBytes(
                    parser->allocator,
                    parser->bytes + offset + headerSize,
                    (CFIndex)count,
                    kCFStringEncodingASCII,
                    false
                );
            }
            break;
        }
        case 0x6u: {
            uint64_t count = 0;
            size_t headerSize = 0;
            if (__FXBplistResolveLength(parser, offset, lowNibble, &count, &headerSize) &&
                count <= (uint64_t)LONG_MAX &&
                count <= (uint64_t)(SIZE_MAX / sizeof(UniChar)) &&
                (uint64_t)(offset + headerSize) + count * 2u <= parser->length) {
                UniChar *characters = NULL;
                if (count > 0) {
                    characters = (UniChar *)malloc((size_t)count * sizeof(UniChar));
                    if (characters == NULL) {
                        break;
                    }
                    for (uint64_t index = 0; index < count; ++index) {
                        uint64_t unit = 0;
                        if (!__FXBplistReadBEUnsigned(parser->bytes + offset + headerSize + index * 2u, 2, &unit)) {
                            free(characters);
                            characters = NULL;
                            count = 0;
                            break;
                        }
                        characters[index] = (UniChar)unit;
                    }
                }
                if (count == 0 || characters != NULL) {
                    object = (CFTypeRef)CFStringCreateWithCharacters(parser->allocator, characters, (CFIndex)count);
                }
                free(characters);
            }
            break;
        }
        case 0xAu:
            object = __FXBplistParseArray(parser, objectIndex, offset, lowNibble, errorOut);
            break;
        case 0xDu:
            object = __FXBplistParseDictionary(parser, objectIndex, offset, lowNibble, errorOut);
            break;
        default:
            break;
    }

    if (object == NULL && (errorOut == NULL || *errorOut == NULL)) {
        __FXBplistSetError(errorOut, parser->allocator, kCFPropertyListReadCorruptError);
    }

    if (object != NULL && parser->cache[objectIndex] == NULL) {
        parser->cache[objectIndex] = object;
    }

    parser->inProgress[objectIndex] = false;
    return object;
}

static void __FXBplistParserCleanup(struct FXBplistParser *parser) {
    if (parser->cache != NULL) {
        for (uint64_t index = 0; index < parser->numObjects; ++index) {
            if (parser->cache[index] != NULL) {
                CFRelease(parser->cache[index]);
            }
        }
    }
    free(parser->inProgress);
    free(parser->cache);
    free(parser->offsets);
}

CFDataRef _FXCFBinaryPListCreateData(CFAllocatorRef allocator, CFPropertyListRef propertyList, CFErrorRef *errorOut) {
    struct FXBplistCollector collector;
    memset(&collector, 0, sizeof(collector));

    if (!__FXBplistCollectObject(&collector, propertyList, allocator, errorOut)) {
        free(collector.objects);
        return NULL;
    }

    UInt8 objectRefSize = (UInt8)__FXBplistMinimalBytesForUnsigned((uint64_t)(collector.count - 1u));
    struct FXBplistBuffer buffer;
    memset(&buffer, 0, sizeof(buffer));

    static const UInt8 header[] = {'b', 'p', 'l', 'i', 's', 't', '0', '0'};
    if (!__FXBplistBufferAppendBytes(&buffer, header, sizeof(header))) {
        free(buffer.bytes);
        free(collector.objects);
        __FXBplistSetError(errorOut, allocator, kCFPropertyListWriteStreamError);
        return NULL;
    }

    for (size_t index = 0; index < collector.count; ++index) {
        collector.objects[index].offset = buffer.length;
        if (!__FXBplistEncodeObject(&buffer, &collector, collector.objects[index].object, objectRefSize)) {
            free(buffer.bytes);
            free(collector.objects);
            __FXBplistSetError(errorOut, allocator, kCFPropertyListWriteStreamError);
            return NULL;
        }
    }

    uint64_t maxOffset = 0;
    for (size_t index = 0; index < collector.count; ++index) {
        if ((uint64_t)collector.objects[index].offset > maxOffset) {
            maxOffset = (uint64_t)collector.objects[index].offset;
        }
    }

    size_t offsetTableOffset = buffer.length;
    UInt8 offsetIntSize = (UInt8)__FXBplistMinimalBytesForUnsigned(maxOffset);
    for (size_t index = 0; index < collector.count; ++index) {
        if (!__FXBplistBufferAppendBEUnsigned(&buffer, (uint64_t)collector.objects[index].offset, offsetIntSize)) {
            free(buffer.bytes);
            free(collector.objects);
            __FXBplistSetError(errorOut, allocator, kCFPropertyListWriteStreamError);
            return NULL;
        }
    }

    static const UInt8 trailerPrefix[6] = {0, 0, 0, 0, 0, 0};
    if (!__FXBplistBufferAppendBytes(&buffer, trailerPrefix, sizeof(trailerPrefix)) ||
        !__FXBplistBufferAppendByte(&buffer, offsetIntSize) ||
        !__FXBplistBufferAppendByte(&buffer, objectRefSize) ||
        !__FXBplistBufferAppendBEUnsigned(&buffer, (uint64_t)collector.count, 8) ||
        !__FXBplistBufferAppendBEUnsigned(&buffer, 0, 8) ||
        !__FXBplistBufferAppendBEUnsigned(&buffer, (uint64_t)offsetTableOffset, 8)) {
        free(buffer.bytes);
        free(collector.objects);
        __FXBplistSetError(errorOut, allocator, kCFPropertyListWriteStreamError);
        return NULL;
    }

    CFDataRef result = CFDataCreate(allocator, buffer.bytes, (CFIndex)buffer.length);
    free(buffer.bytes);
    free(collector.objects);
    if (result == NULL) {
        __FXBplistSetError(errorOut, allocator, kCFPropertyListWriteStreamError);
    }
    return result;
}

CFPropertyListRef _FXCFBinaryPListCreateWithData(CFAllocatorRef allocator, CFDataRef data, CFPropertyListFormat *formatOut, CFErrorRef *errorOut) {
    if (data == NULL) {
        __FXBplistSetError(errorOut, allocator, kCFPropertyListReadCorruptError);
        return NULL;
    }
    if (CFGetTypeID((CFTypeRef)data) != CFDataGetTypeID()) {
        __FXBplistSetError(errorOut, allocator, kCFPropertyListReadCorruptError);
        return NULL;
    }

    CFIndex lengthValue = CFDataGetLength(data);
    const UInt8 *bytes = CFDataGetBytePtr(data);
    if (lengthValue < 40 || bytes == NULL) {
        __FXBplistSetError(errorOut, allocator, kCFPropertyListReadCorruptError);
        return NULL;
    }

    size_t length = (size_t)lengthValue;
    if (memcmp(bytes, "bplist00", 8) != 0) {
        __FXBplistSetError(errorOut, allocator, kCFPropertyListReadCorruptError);
        return NULL;
    }

    const UInt8 *trailer = bytes + length - 32;
    UInt8 offsetIntSize = trailer[6];
    UInt8 objectRefSize = trailer[7];
    uint64_t numObjects = 0;
    uint64_t topObject = 0;
    uint64_t offsetTableOffset = 0;

    if ((offsetIntSize != 1 && offsetIntSize != 2 && offsetIntSize != 4 && offsetIntSize != 8) ||
        (objectRefSize != 1 && objectRefSize != 2 && objectRefSize != 4 && objectRefSize != 8) ||
        !__FXBplistReadBEUnsigned(trailer + 8, 8, &numObjects) ||
        !__FXBplistReadBEUnsigned(trailer + 16, 8, &topObject) ||
        !__FXBplistReadBEUnsigned(trailer + 24, 8, &offsetTableOffset) ||
        numObjects == 0 ||
        numObjects > (uint64_t)(SIZE_MAX / sizeof(uint64_t)) ||
        topObject >= numObjects) {
        __FXBplistSetError(errorOut, allocator, kCFPropertyListReadCorruptError);
        return NULL;
    }

    uint64_t offsetTableBytes = numObjects * offsetIntSize;
    if (offsetTableOffset < 8u ||
        offsetTableOffset > (uint64_t)(length - 32) ||
        offsetTableOffset + offsetTableBytes > (uint64_t)(length - 32)) {
        __FXBplistSetError(errorOut, allocator, kCFPropertyListReadCorruptError);
        return NULL;
    }

    struct FXBplistParser parser;
    memset(&parser, 0, sizeof(parser));
    parser.bytes = bytes;
    parser.length = length;
    parser.allocator = allocator;
    parser.offsetIntSize = offsetIntSize;
    parser.objectRefSize = objectRefSize;
    parser.numObjects = numObjects;
    parser.topObject = topObject;
    parser.offsetTableOffset = offsetTableOffset;
    parser.offsets = (uint64_t *)calloc((size_t)numObjects, sizeof(uint64_t));
    parser.cache = (CFTypeRef *)calloc((size_t)numObjects, sizeof(CFTypeRef));
    parser.inProgress = (Boolean *)calloc((size_t)numObjects, sizeof(Boolean));

    if (parser.offsets == NULL || parser.cache == NULL || parser.inProgress == NULL) {
        __FXBplistParserCleanup(&parser);
        __FXBplistSetError(errorOut, allocator, kCFPropertyListReadCorruptError);
        return NULL;
    }

    for (uint64_t index = 0; index < numObjects; ++index) {
        uint64_t offset = 0;
        if (!__FXBplistReadBEUnsigned(bytes + offsetTableOffset + index * offsetIntSize, offsetIntSize, &offset) ||
            offset < 8u ||
            offset >= offsetTableOffset) {
            __FXBplistParserCleanup(&parser);
            __FXBplistSetError(errorOut, allocator, kCFPropertyListReadCorruptError);
            return NULL;
        }
        parser.offsets[index] = offset;
    }

    CFTypeRef topObjectRef = __FXBplistParseObjectAtIndex(&parser, topObject, errorOut);
    if (topObjectRef == NULL) {
        __FXBplistParserCleanup(&parser);
        return NULL;
    }

    if (formatOut != NULL) {
        *formatOut = kCFPropertyListBinaryFormat_v1_0;
    }

    CFRetain(topObjectRef);
    __FXBplistParserCleanup(&parser);
    return topObjectRef;
}
