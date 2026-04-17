#include "FXCFInternal.h"

#include <CoreFoundation/CoreFoundation.h>

#include <float.h>
#include <limits.h>

enum {
    FX_CF_BPLIST_HEADER_SIZE = 8u,
    FX_CF_BPLIST_TRAILER_SIZE = 32u,
    FX_CF_BPLIST_MAX_DEPTH = 64u
};

struct __FXCFBPlistBuffer {
    UInt8 *bytes;
    size_t length;
    size_t capacity;
};

struct __FXCFBPlistWriteState {
    CFTypeRef *objects;
    UInt8 *visit;
    size_t count;
    size_t capacity;
};

struct __FXCFBPlistParseState {
    const UInt8 *bytes;
    size_t length;
    size_t offsetTableOffset;
    UInt8 offsetIntSize;
    UInt8 objectRefSize;
    size_t numObjects;
    size_t topObject;
    size_t *offsets;
    CFTypeRef *cache;
    UInt8 *state;
    size_t depth;
    CFAllocatorRef allocator;
};

static void __FXCFSetError(CFErrorRef *error, CFAllocatorRef allocator, CFIndex code) {
    if (error == NULL) {
        return;
    }
    *error = _FXCFErrorCreateCode(allocator, code);
}

static Boolean __FXCFAddSize(size_t a, size_t b, size_t *out) {
    if (SIZE_MAX - a < b) {
        return false;
    }
    *out = a + b;
    return true;
}

static Boolean __FXCFMultiplySize(size_t a, size_t b, size_t *out) {
    if (a != 0 && SIZE_MAX / a < b) {
        return false;
    }
    *out = a * b;
    return true;
}

static size_t __FXCFUnsignedByteCount(uint64_t value) {
    if (value <= UINT8_MAX) {
        return 1u;
    }
    if (value <= UINT16_MAX) {
        return 2u;
    }
    if (value <= UINT32_MAX) {
        return 4u;
    }
    return 8u;
}

static size_t __FXCFSignedByteCount(SInt64 value) {
    if (value >= INT8_MIN && value <= INT8_MAX) {
        return 1u;
    }
    if (value >= INT16_MIN && value <= INT16_MAX) {
        return 2u;
    }
    if (value >= INT32_MIN && value <= INT32_MAX) {
        return 4u;
    }
    return 8u;
}

static UInt8 __FXCFLog2ByteCount(size_t byteCount) {
    switch (byteCount) {
        case 1u:
            return 0u;
        case 2u:
            return 1u;
        case 4u:
            return 2u;
        case 8u:
            return 3u;
        default:
            _FXCFAbort("unsupported bplist integer byte count");
    }
}

static Boolean __FXCFBPlistBufferReserve(struct __FXCFBPlistBuffer *buffer, size_t additional) {
    size_t required = 0;
    UInt8 *newBytes = NULL;

    if (!__FXCFAddSize(buffer->length, additional, &required)) {
        return false;
    }
    if (required <= buffer->capacity) {
        return true;
    }

    size_t newCapacity = buffer->capacity > 0 ? buffer->capacity : 64u;
    while (newCapacity < required) {
        if (newCapacity > SIZE_MAX / 2u) {
            newCapacity = required;
            break;
        }
        newCapacity *= 2u;
    }

    newBytes = (UInt8 *)realloc(buffer->bytes, newCapacity);
    if (newBytes == NULL) {
        return false;
    }

    buffer->bytes = newBytes;
    buffer->capacity = newCapacity;
    return true;
}

static Boolean __FXCFBPlistBufferAppend(struct __FXCFBPlistBuffer *buffer, const UInt8 *bytes, size_t byteCount) {
    if (byteCount == 0u) {
        return true;
    }
    if (bytes == NULL) {
        return false;
    }
    if (!__FXCFBPlistBufferReserve(buffer, byteCount)) {
        return false;
    }
    memcpy(buffer->bytes + buffer->length, bytes, byteCount);
    buffer->length += byteCount;
    return true;
}

static Boolean __FXCFBPlistBufferAppendByte(struct __FXCFBPlistBuffer *buffer, UInt8 value) {
    return __FXCFBPlistBufferAppend(buffer, &value, 1u);
}

static Boolean __FXCFBPlistBufferAppendUnsigned(struct __FXCFBPlistBuffer *buffer, uint64_t value, size_t byteCount) {
    UInt8 encoded[8];
    size_t offset = 0u;

    if (byteCount == 0u || byteCount > sizeof(encoded)) {
        return false;
    }

    for (offset = 0u; offset < byteCount; ++offset) {
        encoded[byteCount - 1u - offset] = (UInt8)(value & 0xffu);
        value >>= 8u;
    }
    return __FXCFBPlistBufferAppend(buffer, encoded, byteCount);
}

static Boolean __FXCFBPlistBufferAppendSigned(struct __FXCFBPlistBuffer *buffer, SInt64 value, size_t byteCount) {
    return __FXCFBPlistBufferAppendUnsigned(buffer, (uint64_t)value, byteCount);
}

static Boolean __FXCFBPlistBufferAppendDouble(struct __FXCFBPlistBuffer *buffer, double value) {
    union {
        double f64;
        uint64_t bits;
    } storage;

    storage.f64 = value;
    return __FXCFBPlistBufferAppendUnsigned(buffer, storage.bits, 8u);
}

static Boolean __FXCFBPlistBufferAppendLength(struct __FXCFBPlistBuffer *buffer, UInt8 family, uint64_t count) {
    if (count < 15u) {
        return __FXCFBPlistBufferAppendByte(buffer, (UInt8)(family | (UInt8)count));
    }

    size_t countBytes = __FXCFUnsignedByteCount(count);
    if (!__FXCFBPlistBufferAppendByte(buffer, (UInt8)(family | 0x0fu))) {
        return false;
    }
    if (!__FXCFBPlistBufferAppendByte(buffer, (UInt8)(0x10u | __FXCFLog2ByteCount(countBytes)))) {
        return false;
    }
    return __FXCFBPlistBufferAppendUnsigned(buffer, count, countBytes);
}

static CFIndex __FXCFBPlistFindObject(const struct __FXCFBPlistWriteState *state, CFTypeRef object) {
    size_t index = 0u;
    for (index = 0u; index < state->count; ++index) {
        if (state->objects[index] == object) {
            return (CFIndex)index;
        }
    }
    return kCFNotFound;
}

static Boolean __FXCFBPlistEnsureWriteCapacity(struct __FXCFBPlistWriteState *state, size_t required) {
    size_t newCapacity = 0u;
    CFTypeRef *newObjects = NULL;
    UInt8 *newVisit = NULL;

    if (required <= state->capacity) {
        return true;
    }

    newCapacity = state->capacity > 0u ? state->capacity : 16u;
    while (newCapacity < required) {
        if (newCapacity > SIZE_MAX / 2u) {
            newCapacity = required;
            break;
        }
        newCapacity *= 2u;
    }

    newObjects = (CFTypeRef *)calloc(newCapacity, sizeof(CFTypeRef));
    newVisit = (UInt8 *)calloc(newCapacity, sizeof(UInt8));
    if (newObjects == NULL || newVisit == NULL) {
        free(newObjects);
        free(newVisit);
        return false;
    }
    if (state->count > 0u) {
        memcpy(newObjects, state->objects, state->count * sizeof(CFTypeRef));
        memcpy(newVisit, state->visit, state->count * sizeof(UInt8));
    }

    free(state->objects);
    free(state->visit);
    state->objects = newObjects;
    state->visit = newVisit;
    state->capacity = newCapacity;
    return true;
}

static Boolean __FXCFBPlistAddObject(struct __FXCFBPlistWriteState *state, CFTypeRef object, CFAllocatorRef allocator, CFErrorRef *error);

static Boolean __FXCFBPlistAddArray(struct __FXCFBPlistWriteState *state, CFArrayRef array, CFAllocatorRef allocator, CFErrorRef *error) {
    CFIndex count = _FXCFArrayFastCount(array);
    CFIndex index = 0;

    for (index = 0; index < count; ++index) {
        if (!__FXCFBPlistAddObject(state, (CFTypeRef)_FXCFArrayFastValueAtIndex(array, index), allocator, error)) {
            return false;
        }
    }
    return true;
}

static Boolean __FXCFBPlistAddDictionary(struct __FXCFBPlistWriteState *state, CFDictionaryRef dictionary, CFAllocatorRef allocator, CFErrorRef *error) {
    CFIndex count = _FXCFDictionaryFastCount(dictionary);
    CFIndex index = 0;

    for (index = 0; index < count; ++index) {
        CFTypeRef key = (CFTypeRef)_FXCFDictionaryFastKeyAtIndex(dictionary, index);
        CFTypeRef value = (CFTypeRef)_FXCFDictionaryFastValueAtIndex(dictionary, index);

        if (CFGetTypeID(key) != CFStringGetTypeID()) {
            __FXCFSetError(error, allocator, kCFPropertyListWriteStreamError);
            return false;
        }
        if (!__FXCFBPlistAddObject(state, key, allocator, error)) {
            return false;
        }
        if (!__FXCFBPlistAddObject(state, value, allocator, error)) {
            return false;
        }
    }
    return true;
}

static Boolean __FXCFBPlistAddObject(struct __FXCFBPlistWriteState *state, CFTypeRef object, CFAllocatorRef allocator, CFErrorRef *error) {
    CFIndex existing = 0;
    CFTypeID typeID = 0;

    if (object == NULL) {
        __FXCFSetError(error, allocator, kCFPropertyListWriteStreamError);
        return false;
    }

    existing = __FXCFBPlistFindObject(state, object);
    if (existing != kCFNotFound) {
        if (state->visit[existing] == 1u) {
            __FXCFSetError(error, allocator, kCFPropertyListWriteStreamError);
            return false;
        }
        return true;
    }

    if (!__FXCFBPlistEnsureWriteCapacity(state, state->count + 1u)) {
        __FXCFSetError(error, allocator, kCFPropertyListWriteStreamError);
        return false;
    }

    state->objects[state->count] = object;
    state->visit[state->count] = 1u;
    existing = (CFIndex)state->count;
    state->count += 1u;

    typeID = CFGetTypeID(object);
    if (typeID == CFBooleanGetTypeID() ||
        typeID == CFNumberGetTypeID() ||
        typeID == CFDateGetTypeID() ||
        typeID == CFDataGetTypeID() ||
        typeID == CFStringGetTypeID()) {
        state->visit[existing] = 2u;
        return true;
    }
    if (typeID == CFArrayGetTypeID()) {
        if (!__FXCFBPlistAddArray(state, (CFArrayRef)object, allocator, error)) {
            return false;
        }
        state->visit[existing] = 2u;
        return true;
    }
    if (typeID == CFDictionaryGetTypeID()) {
        if (!__FXCFBPlistAddDictionary(state, (CFDictionaryRef)object, allocator, error)) {
            return false;
        }
        state->visit[existing] = 2u;
        return true;
    }

    __FXCFSetError(error, allocator, kCFPropertyListWriteStreamError);
    return false;
}

static Boolean __FXCFStringASCIIByteCount(CFStringRef string, size_t *byteCount) {
    CFIndex length = CFStringGetLength(string);
    CFIndex used = 0;
    CFIndex converted = 0;
    if (length < 0) {
        return false;
    }
    converted = CFStringGetBytes(
        string,
        CFRangeMake(0, length),
        kCFStringEncodingASCII,
        0,
        false,
        NULL,
        0,
        &used
    );
    if (converted != length || used != length) {
        return false;
    }
    *byteCount = (size_t)used;
    return true;
}

static Boolean __FXCFBPlistLengthFieldExtraSize(uint64_t count, size_t *out) {
    size_t extra = 0u;
    if (count >= 15u) {
        if (!__FXCFAddSize(1u, __FXCFUnsignedByteCount(count), &extra)) {
            return false;
        }
    }
    *out = extra;
    return true;
}

static Boolean __FXCFBPlistObjectSize(CFTypeRef object, UInt8 objectRefSize, size_t *sizeOut) {
    size_t total = 0u;
    size_t extra = 0u;
    size_t payload = 0u;
    CFTypeID typeID = CFGetTypeID(object);

    if (typeID == CFBooleanGetTypeID()) {
        *sizeOut = 1u;
        return true;
    }
    if (typeID == CFNumberGetTypeID()) {
        if (CFNumberGetType((CFNumberRef)object) == kCFNumberFloat64Type) {
            *sizeOut = 9u;
            return true;
        }
        {
            SInt64 value = 0;
            if (!CFNumberGetValue((CFNumberRef)object, kCFNumberSInt64Type, &value)) {
                return false;
            }
            *sizeOut = 1u + __FXCFSignedByteCount(value);
        }
        return true;
    }
    if (typeID == CFDateGetTypeID()) {
        *sizeOut = 9u;
        return true;
    }
    if (typeID == CFDataGetTypeID()) {
        CFIndex length = CFDataGetLength((CFDataRef)object);
        if (length < 0) {
            return false;
        }
        if (!__FXCFBPlistLengthFieldExtraSize((uint64_t)length, &extra)) {
            return false;
        }
        total = 1u;
        if (!__FXCFAddSize(total, extra, &total) ||
            !__FXCFAddSize(total, (size_t)length, &total)) {
            return false;
        }
        *sizeOut = total;
        return true;
    }
    if (typeID == CFStringGetTypeID()) {
        CFIndex length = CFStringGetLength((CFStringRef)object);
        if (length < 0) {
            return false;
        }
        if (__FXCFStringASCIIByteCount((CFStringRef)object, &payload)) {
            if (!__FXCFBPlistLengthFieldExtraSize((uint64_t)payload, &extra)) {
                return false;
            }
            total = 1u;
            if (!__FXCFAddSize(total, extra, &total) ||
                !__FXCFAddSize(total, payload, &total)) {
                return false;
            }
            *sizeOut = total;
            return true;
        }
        if (!__FXCFBPlistLengthFieldExtraSize((uint64_t)length, &extra)) {
            return false;
        }
        if (!__FXCFMultiplySize((size_t)length, 2u, &payload)) {
            return false;
        }
        total = 1u;
        if (!__FXCFAddSize(total, extra, &total) ||
            !__FXCFAddSize(total, payload, &total)) {
            return false;
        }
        *sizeOut = total;
        return true;
    }
    if (typeID == CFArrayGetTypeID()) {
        CFIndex count = _FXCFArrayFastCount((CFArrayRef)object);
        if (count < 0) {
            return false;
        }
        if (!__FXCFBPlistLengthFieldExtraSize((uint64_t)count, &extra) ||
            !__FXCFMultiplySize((size_t)count, (size_t)objectRefSize, &payload)) {
            return false;
        }
        total = 1u;
        if (!__FXCFAddSize(total, extra, &total) ||
            !__FXCFAddSize(total, payload, &total)) {
            return false;
        }
        *sizeOut = total;
        return true;
    }
    if (typeID == CFDictionaryGetTypeID()) {
        CFIndex count = _FXCFDictionaryFastCount((CFDictionaryRef)object);
        if (count < 0) {
            return false;
        }
        if (!__FXCFBPlistLengthFieldExtraSize((uint64_t)count, &extra) ||
            !__FXCFMultiplySize((size_t)count, (size_t)objectRefSize * 2u, &payload)) {
            return false;
        }
        total = 1u;
        if (!__FXCFAddSize(total, extra, &total) ||
            !__FXCFAddSize(total, payload, &total)) {
            return false;
        }
        *sizeOut = total;
        return true;
    }
    return false;
}

static Boolean __FXCFBPlistEncodeObject(
    struct __FXCFBPlistBuffer *buffer,
    const struct __FXCFBPlistWriteState *state,
    CFTypeRef object,
    UInt8 objectRefSize
) {
    CFTypeID typeID = CFGetTypeID(object);

    if (typeID == CFBooleanGetTypeID()) {
        return __FXCFBPlistBufferAppendByte(
            buffer,
            object == (CFTypeRef)kCFBooleanTrue ? 0x09u : 0x08u
        );
    }

    if (typeID == CFNumberGetTypeID()) {
        if (CFNumberGetType((CFNumberRef)object) == kCFNumberFloat64Type) {
            double value = 0.0;
            if (!CFNumberGetValue((CFNumberRef)object, kCFNumberFloat64Type, &value)) {
                return false;
            }
            if (!__FXCFBPlistBufferAppendByte(buffer, 0x23u)) {
                return false;
            }
            return __FXCFBPlistBufferAppendDouble(buffer, value);
        }

        SInt64 value = 0;
        size_t byteCount = 0u;
        if (!CFNumberGetValue((CFNumberRef)object, kCFNumberSInt64Type, &value)) {
            return false;
        }
        byteCount = __FXCFSignedByteCount(value);
        if (!__FXCFBPlistBufferAppendByte(buffer, (UInt8)(0x10u | __FXCFLog2ByteCount(byteCount)))) {
            return false;
        }
        return __FXCFBPlistBufferAppendSigned(buffer, value, byteCount);
    }

    if (typeID == CFDateGetTypeID()) {
        if (!__FXCFBPlistBufferAppendByte(buffer, 0x33u)) {
            return false;
        }
        return __FXCFBPlistBufferAppendDouble(buffer, CFDateGetAbsoluteTime((CFDateRef)object));
    }

    if (typeID == CFDataGetTypeID()) {
        CFIndex length = CFDataGetLength((CFDataRef)object);
        const UInt8 *bytes = CFDataGetBytePtr((CFDataRef)object);
        if (length < 0) {
            return false;
        }
        if (!__FXCFBPlistBufferAppendLength(buffer, 0x40u, (uint64_t)length)) {
            return false;
        }
        return __FXCFBPlistBufferAppend(buffer, bytes, (size_t)length);
    }

    if (typeID == CFStringGetTypeID()) {
        CFStringRef string = (CFStringRef)object;
        CFIndex length = CFStringGetLength(string);
        size_t asciiLength = 0u;

        if (length < 0) {
            return false;
        }

        if (__FXCFStringASCIIByteCount(string, &asciiLength)) {
            UInt8 *bytes = NULL;
            CFIndex used = 0;
            if (!__FXCFBPlistBufferAppendLength(buffer, 0x50u, (uint64_t)asciiLength)) {
                return false;
            }
            if (asciiLength == 0u) {
                return true;
            }
            bytes = (UInt8 *)calloc(asciiLength, sizeof(UInt8));
            if (bytes == NULL) {
                return false;
            }
            if (CFStringGetBytes(
                    string,
                    CFRangeMake(0, length),
                    kCFStringEncodingASCII,
                    0,
                    false,
                    bytes,
                    (CFIndex)asciiLength,
                    &used
                ) != length ||
                used != (CFIndex)asciiLength) {
                free(bytes);
                return false;
            }
            {
                Boolean ok = __FXCFBPlistBufferAppend(buffer, bytes, asciiLength);
                free(bytes);
                return ok;
            }
        }

        if (!__FXCFBPlistBufferAppendLength(buffer, 0x60u, (uint64_t)length)) {
            return false;
        }
        if (length > 0) {
            UniChar *chars = (UniChar *)calloc((size_t)length, sizeof(UniChar));
            CFIndex index = 0;
            if (chars == NULL) {
                return false;
            }
            CFStringGetCharacters(string, CFRangeMake(0, length), chars);
            for (index = 0; index < length; ++index) {
                if (!__FXCFBPlistBufferAppendUnsigned(buffer, chars[index], 2u)) {
                    free(chars);
                    return false;
                }
            }
            free(chars);
        }
        return true;
    }

    if (typeID == CFArrayGetTypeID()) {
        CFArrayRef array = (CFArrayRef)object;
        CFIndex count = _FXCFArrayFastCount(array);
        CFIndex index = 0;

        if (!__FXCFBPlistBufferAppendLength(buffer, 0xa0u, (uint64_t)count)) {
            return false;
        }
        for (index = 0; index < count; ++index) {
            CFIndex objectIndex = __FXCFBPlistFindObject(state, (CFTypeRef)_FXCFArrayFastValueAtIndex(array, index));
            if (objectIndex == kCFNotFound) {
                return false;
            }
            if (!__FXCFBPlistBufferAppendUnsigned(buffer, (uint64_t)objectIndex, (size_t)objectRefSize)) {
                return false;
            }
        }
        return true;
    }

    if (typeID == CFDictionaryGetTypeID()) {
        CFDictionaryRef dictionary = (CFDictionaryRef)object;
        CFIndex count = _FXCFDictionaryFastCount(dictionary);
        CFIndex index = 0;

        if (!__FXCFBPlistBufferAppendLength(buffer, 0xd0u, (uint64_t)count)) {
            return false;
        }
        for (index = 0; index < count; ++index) {
            CFIndex objectIndex = __FXCFBPlistFindObject(state, (CFTypeRef)_FXCFDictionaryFastKeyAtIndex(dictionary, index));
            if (objectIndex == kCFNotFound) {
                return false;
            }
            if (!__FXCFBPlistBufferAppendUnsigned(buffer, (uint64_t)objectIndex, (size_t)objectRefSize)) {
                return false;
            }
        }
        for (index = 0; index < count; ++index) {
            CFIndex objectIndex = __FXCFBPlistFindObject(state, (CFTypeRef)_FXCFDictionaryFastValueAtIndex(dictionary, index));
            if (objectIndex == kCFNotFound) {
                return false;
            }
            if (!__FXCFBPlistBufferAppendUnsigned(buffer, (uint64_t)objectIndex, (size_t)objectRefSize)) {
                return false;
            }
        }
        return true;
    }

    return false;
}

static Boolean __FXCFBPlistReadUnsigned(const UInt8 *bytes, size_t byteCount, uint64_t *valueOut) {
    uint64_t value = 0u;
    size_t index = 0u;

    if (byteCount == 0u || byteCount > 8u) {
        return false;
    }
    for (index = 0u; index < byteCount; ++index) {
        value = (value << 8u) | bytes[index];
    }
    *valueOut = value;
    return true;
}

static Boolean __FXCFBPlistReadSigned(const UInt8 *bytes, size_t byteCount, SInt64 *valueOut) {
    uint64_t value = 0u;
    if (!__FXCFBPlistReadUnsigned(bytes, byteCount, &value)) {
        return false;
    }
    if (byteCount < 8u && (bytes[0] & 0x80u) != 0u) {
        value |= (~0ULL) << (byteCount * 8u);
    }
    *valueOut = (SInt64)value;
    return true;
}

static Boolean __FXCFBPlistDecodeInlineCount(
    const struct __FXCFBPlistParseState *state,
    size_t objectOffset,
    UInt8 marker,
    size_t *payloadOffset,
    size_t *countOut
) {
    size_t offset = objectOffset + 1u;
    uint64_t count = 0u;

    if ((marker & 0x0fu) != 0x0fu) {
        *payloadOffset = offset;
        *countOut = (size_t)(marker & 0x0fu);
        return true;
    }

    if (offset >= state->offsetTableOffset) {
        return false;
    }
    UInt8 lengthMarker = state->bytes[offset];
    if ((lengthMarker & 0xf0u) != 0x10u) {
        return false;
    }
    size_t byteCount = (size_t)1u << (lengthMarker & 0x0fu);
    if (byteCount == 0u || byteCount > 8u) {
        return false;
    }
    if (offset + 1u + byteCount > state->offsetTableOffset) {
        return false;
    }
    if (!__FXCFBPlistReadUnsigned(state->bytes + offset + 1u, byteCount, &count)) {
        return false;
    }
    if (count > (uint64_t)LONG_MAX) {
        return false;
    }
    *payloadOffset = offset + 1u + byteCount;
    *countOut = (size_t)count;
    return true;
}

static Boolean __FXCFBPlistReadObjectRef(const struct __FXCFBPlistParseState *state, const UInt8 *bytes, size_t *valueOut) {
    uint64_t value = 0u;
    if (!__FXCFBPlistReadUnsigned(bytes, state->objectRefSize, &value)) {
        return false;
    }
    if (value >= state->numObjects) {
        return false;
    }
    *valueOut = (size_t)value;
    return true;
}

static CFTypeRef __FXCFBPlistParseObject(struct __FXCFBPlistParseState *state, size_t index, CFErrorRef *error);

static CFTypeRef __FXCFBPlistParseArray(
    struct __FXCFBPlistParseState *state,
    size_t objectOffset,
    UInt8 marker,
    CFErrorRef *error
) {
    size_t payloadOffset = 0u;
    size_t count = 0u;
    const void **values = NULL;
    CFArrayRef array = NULL;
    size_t refBytes = 0u;
    size_t item = 0u;

    if (!__FXCFBPlistDecodeInlineCount(state, objectOffset, marker, &payloadOffset, &count) ||
        !__FXCFMultiplySize(count, (size_t)state->objectRefSize, &refBytes) ||
        payloadOffset + refBytes > state->offsetTableOffset) {
        __FXCFSetError(error, state->allocator, kCFPropertyListReadCorruptError);
        return NULL;
    }

    if (count > 0u) {
        values = (const void **)calloc(count, sizeof(const void *));
        if (values == NULL) {
            __FXCFSetError(error, state->allocator, kCFPropertyListReadCorruptError);
            return NULL;
        }
    }

    for (item = 0u; item < count; ++item) {
        size_t childIndex = 0u;
        if (!__FXCFBPlistReadObjectRef(state, state->bytes + payloadOffset + (item * state->objectRefSize), &childIndex)) {
            __FXCFSetError(error, state->allocator, kCFPropertyListReadCorruptError);
            goto fail;
        }
        values[item] = __FXCFBPlistParseObject(state, childIndex, error);
        if (values[item] == NULL) {
            goto fail;
        }
    }

    array = CFArrayCreate(state->allocator, values, (CFIndex)count, &kCFTypeArrayCallBacks);
    if (array == NULL) {
        __FXCFSetError(error, state->allocator, kCFPropertyListReadCorruptError);
        goto fail;
    }

    for (item = 0u; item < count; ++item) {
        if (values[item] != NULL) {
            CFRelease((CFTypeRef)values[item]);
        }
    }
    free(values);
    return array;

fail:
    for (item = 0u; item < count; ++item) {
        if (values != NULL && values[item] != NULL) {
            CFRelease((CFTypeRef)values[item]);
        }
    }
    free(values);
    return NULL;
}

static CFTypeRef __FXCFBPlistParseDictionary(
    struct __FXCFBPlistParseState *state,
    size_t objectOffset,
    UInt8 marker,
    CFErrorRef *error
) {
    size_t payloadOffset = 0u;
    size_t count = 0u;
    size_t refBytes = 0u;
    size_t item = 0u;
    const void **keys = NULL;
    const void **values = NULL;
    CFDictionaryRef dictionary = NULL;

    if (!__FXCFBPlistDecodeInlineCount(state, objectOffset, marker, &payloadOffset, &count) ||
        !__FXCFMultiplySize(count, (size_t)state->objectRefSize * 2u, &refBytes) ||
        payloadOffset + refBytes > state->offsetTableOffset) {
        __FXCFSetError(error, state->allocator, kCFPropertyListReadCorruptError);
        return NULL;
    }

    if (count > 0u) {
        keys = (const void **)calloc(count, sizeof(const void *));
        values = (const void **)calloc(count, sizeof(const void *));
        if (keys == NULL || values == NULL) {
            free(keys);
            free(values);
            __FXCFSetError(error, state->allocator, kCFPropertyListReadCorruptError);
            return NULL;
        }
    }

    for (item = 0u; item < count; ++item) {
        size_t childIndex = 0u;
        if (!__FXCFBPlistReadObjectRef(state, state->bytes + payloadOffset + (item * state->objectRefSize), &childIndex)) {
            __FXCFSetError(error, state->allocator, kCFPropertyListReadCorruptError);
            goto fail;
        }
        keys[item] = __FXCFBPlistParseObject(state, childIndex, error);
        if (keys[item] == NULL || CFGetTypeID((CFTypeRef)keys[item]) != CFStringGetTypeID()) {
            __FXCFSetError(error, state->allocator, kCFPropertyListReadCorruptError);
            goto fail;
        }
    }

    for (item = 0u; item < count; ++item) {
        size_t valueIndex = 0u;
        size_t refOffset = payloadOffset + ((count + item) * state->objectRefSize);
        if (!__FXCFBPlistReadObjectRef(state, state->bytes + refOffset, &valueIndex)) {
            __FXCFSetError(error, state->allocator, kCFPropertyListReadCorruptError);
            goto fail;
        }
        values[item] = __FXCFBPlistParseObject(state, valueIndex, error);
        if (values[item] == NULL) {
            goto fail;
        }
    }

    dictionary = CFDictionaryCreate(
        state->allocator,
        keys,
        values,
        (CFIndex)count,
        &kCFTypeDictionaryKeyCallBacks,
        &kCFTypeDictionaryValueCallBacks
    );
    if (dictionary == NULL) {
        __FXCFSetError(error, state->allocator, kCFPropertyListReadCorruptError);
        goto fail;
    }

    for (item = 0u; item < count; ++item) {
        if (keys[item] != NULL) {
            CFRelease((CFTypeRef)keys[item]);
        }
        if (values[item] != NULL) {
            CFRelease((CFTypeRef)values[item]);
        }
    }
    free(keys);
    free(values);
    return dictionary;

fail:
    for (item = 0u; item < count; ++item) {
        if (keys != NULL && keys[item] != NULL) {
            CFRelease((CFTypeRef)keys[item]);
        }
        if (values != NULL && values[item] != NULL) {
            CFRelease((CFTypeRef)values[item]);
        }
    }
    free(keys);
    free(values);
    return NULL;
}

static CFTypeRef __FXCFBPlistParseObject(struct __FXCFBPlistParseState *state, size_t index, CFErrorRef *error) {
    size_t payloadOffset = 0u;
    size_t count = 0u;
    size_t payloadBytes = 0u;
    UInt8 marker = 0u;
    CFTypeRef created = NULL;

    if (index >= state->numObjects) {
        __FXCFSetError(error, state->allocator, kCFPropertyListReadCorruptError);
        return NULL;
    }

    if (state->state[index] == 2u) {
        return CFRetain(state->cache[index]);
    }
    if (state->state[index] == 1u || state->depth >= FX_CF_BPLIST_MAX_DEPTH) {
        __FXCFSetError(error, state->allocator, kCFPropertyListReadCorruptError);
        return NULL;
    }

    if (state->offsets[index] >= state->offsetTableOffset) {
        __FXCFSetError(error, state->allocator, kCFPropertyListReadCorruptError);
        return NULL;
    }

    state->state[index] = 1u;
    state->depth += 1u;
    marker = state->bytes[state->offsets[index]];

    switch (marker & 0xf0u) {
        case 0x00u:
            if (marker == 0x08u) {
                created = (CFTypeRef)kCFBooleanFalse;
            } else if (marker == 0x09u) {
                created = (CFTypeRef)kCFBooleanTrue;
            }
            break;
        case 0x10u: {
            size_t byteCount = (size_t)1u << (marker & 0x0fu);
            SInt64 value = 0;
            if (byteCount == 0u || byteCount > 8u ||
                state->offsets[index] + 1u + byteCount > state->offsetTableOffset ||
                !__FXCFBPlistReadSigned(state->bytes + state->offsets[index] + 1u, byteCount, &value)) {
                break;
            }
            created = (CFTypeRef)CFNumberCreate(state->allocator, kCFNumberSInt64Type, &value);
            break;
        }
        case 0x20u: {
            if (marker != 0x23u || state->offsets[index] + 9u > state->offsetTableOffset) {
                break;
            }
            uint64_t bits = 0u;
            union {
                uint64_t bits;
                double f64;
            } storage;
            if (!__FXCFBPlistReadUnsigned(state->bytes + state->offsets[index] + 1u, 8u, &bits)) {
                break;
            }
            storage.bits = bits;
            created = (CFTypeRef)CFNumberCreate(state->allocator, kCFNumberFloat64Type, &storage.f64);
            break;
        }
        case 0x30u: {
            if (marker != 0x33u || state->offsets[index] + 9u > state->offsetTableOffset) {
                break;
            }
            uint64_t bits = 0u;
            union {
                uint64_t bits;
                double f64;
            } storage;
            if (!__FXCFBPlistReadUnsigned(state->bytes + state->offsets[index] + 1u, 8u, &bits)) {
                break;
            }
            storage.bits = bits;
            created = (CFTypeRef)CFDateCreate(state->allocator, storage.f64);
            break;
        }
        case 0x40u:
            if (!__FXCFBPlistDecodeInlineCount(state, state->offsets[index], marker, &payloadOffset, &count) ||
                payloadOffset > state->offsetTableOffset ||
                !__FXCFAddSize(payloadOffset, count, &payloadBytes) ||
                payloadBytes > state->offsetTableOffset) {
                break;
            }
            created = (CFTypeRef)CFDataCreate(state->allocator, state->bytes + payloadOffset, (CFIndex)count);
            break;
        case 0x50u:
            if (!__FXCFBPlistDecodeInlineCount(state, state->offsets[index], marker, &payloadOffset, &count) ||
                payloadOffset > state->offsetTableOffset ||
                !__FXCFAddSize(payloadOffset, count, &payloadBytes) ||
                payloadBytes > state->offsetTableOffset) {
                break;
            }
            created = (CFTypeRef)CFStringCreateWithBytes(
                state->allocator,
                state->bytes + payloadOffset,
                (CFIndex)count,
                kCFStringEncodingASCII,
                false
            );
            break;
        case 0x60u:
            if (!__FXCFBPlistDecodeInlineCount(state, state->offsets[index], marker, &payloadOffset, &count) ||
                !__FXCFMultiplySize(count, 2u, &payloadBytes) ||
                !__FXCFAddSize(payloadOffset, payloadBytes, &payloadBytes) ||
                payloadBytes > state->offsetTableOffset) {
                break;
            }
            if (count == 0u) {
                created = (CFTypeRef)CFStringCreateWithCharacters(state->allocator, NULL, 0);
            } else {
                UniChar *chars = (UniChar *)calloc(count, sizeof(UniChar));
                size_t charIndex = 0u;
                if (chars == NULL) {
                    break;
                }
                for (charIndex = 0u; charIndex < count; ++charIndex) {
                    uint64_t unit = 0u;
                    if (!__FXCFBPlistReadUnsigned(state->bytes + payloadOffset + (charIndex * 2u), 2u, &unit)) {
                        free(chars);
                        chars = NULL;
                        break;
                    }
                    chars[charIndex] = (UniChar)unit;
                }
                if (chars != NULL) {
                    created = (CFTypeRef)CFStringCreateWithCharacters(state->allocator, chars, (CFIndex)count);
                    free(chars);
                }
            }
            break;
        case 0xa0u:
            created = __FXCFBPlistParseArray(state, state->offsets[index], marker, error);
            break;
        case 0xd0u:
            created = __FXCFBPlistParseDictionary(state, state->offsets[index], marker, error);
            break;
        default:
            break;
    }

    state->depth -= 1u;

    if (created == NULL) {
        state->state[index] = 0u;
        if (error != NULL && *error == NULL) {
            __FXCFSetError(error, state->allocator, kCFPropertyListReadCorruptError);
        }
        return NULL;
    }

    state->cache[index] = created;
    state->state[index] = 2u;
    return CFRetain(created);
}

static void __FXCFBPlistReleaseCache(CFTypeRef *cache, size_t count) {
    size_t index = 0u;
    if (cache == NULL) {
        return;
    }
    for (index = 0u; index < count; ++index) {
        if (cache[index] != NULL) {
            CFRelease(cache[index]);
        }
    }
}

CFDataRef _FXCFBinaryPListCreateData(
    CFAllocatorRef allocator,
    CFPropertyListRef propertyList,
    CFPropertyListFormat format,
    CFOptionFlags options,
    CFErrorRef *error
) {
    struct __FXCFBPlistWriteState state;
    UInt8 header[FX_CF_BPLIST_HEADER_SIZE] = {'b', 'p', 'l', 'i', 's', 't', '0', '0'};
    struct __FXCFBPlistBuffer buffer;
    size_t *sizes = NULL;
    size_t *offsets = NULL;
    size_t maxOffset = 0u;
    size_t offsetTableOffset = 0u;
    size_t index = 0u;
    UInt8 objectRefSize = 1u;
    UInt8 offsetIntSize = 1u;
    CFAllocatorRef realAllocator = allocator == NULL ? CFAllocatorGetDefault() : allocator;
    CFDataRef result = NULL;

    if (error != NULL) {
        *error = NULL;
    }
    if (propertyList == NULL || format != kCFPropertyListBinaryFormat_v1_0 || options != 0u) {
        __FXCFSetError(error, realAllocator, kCFPropertyListWriteStreamError);
        return NULL;
    }

    memset(&state, 0, sizeof(state));
    memset(&buffer, 0, sizeof(buffer));

    if (!__FXCFBPlistAddObject(&state, (CFTypeRef)propertyList, realAllocator, error) || state.count == 0u) {
        free(state.objects);
        free(state.visit);
        return NULL;
    }

    objectRefSize = (UInt8)__FXCFUnsignedByteCount((uint64_t)(state.count - 1u));
    sizes = (size_t *)calloc(state.count, sizeof(size_t));
    offsets = (size_t *)calloc(state.count, sizeof(size_t));
    if (sizes == NULL || offsets == NULL) {
        free(sizes);
        free(offsets);
        free(state.objects);
        free(state.visit);
        __FXCFSetError(error, realAllocator, kCFPropertyListWriteStreamError);
        return NULL;
    }

    for (index = 0u; index < state.count; ++index) {
        if (!__FXCFBPlistObjectSize(state.objects[index], objectRefSize, &sizes[index])) {
            __FXCFSetError(error, realAllocator, kCFPropertyListWriteStreamError);
            goto cleanup;
        }
    }

    offsetTableOffset = FX_CF_BPLIST_HEADER_SIZE;
    for (index = 0u; index < state.count; ++index) {
        offsets[index] = offsetTableOffset;
        if (!__FXCFAddSize(offsetTableOffset, sizes[index], &offsetTableOffset)) {
            __FXCFSetError(error, realAllocator, kCFPropertyListWriteStreamError);
            goto cleanup;
        }
    }
    maxOffset = state.count > 0u ? offsets[state.count - 1u] : 0u;
    offsetIntSize = (UInt8)__FXCFUnsignedByteCount((uint64_t)maxOffset);

    if (!__FXCFBPlistBufferAppend(&buffer, header, sizeof(header))) {
        __FXCFSetError(error, realAllocator, kCFPropertyListWriteStreamError);
        goto cleanup;
    }

    for (index = 0u; index < state.count; ++index) {
        if (buffer.length != offsets[index] || !__FXCFBPlistEncodeObject(&buffer, &state, state.objects[index], objectRefSize)) {
            __FXCFSetError(error, realAllocator, kCFPropertyListWriteStreamError);
            goto cleanup;
        }
    }

    offsetTableOffset = buffer.length;
    for (index = 0u; index < state.count; ++index) {
        if (!__FXCFBPlistBufferAppendUnsigned(&buffer, (uint64_t)offsets[index], (size_t)offsetIntSize)) {
            __FXCFSetError(error, realAllocator, kCFPropertyListWriteStreamError);
            goto cleanup;
        }
    }

    {
        UInt8 zeros[6] = {0u, 0u, 0u, 0u, 0u, 0u};
        if (!__FXCFBPlistBufferAppend(&buffer, zeros, sizeof(zeros)) ||
            !__FXCFBPlistBufferAppendByte(&buffer, offsetIntSize) ||
            !__FXCFBPlistBufferAppendByte(&buffer, objectRefSize) ||
            !__FXCFBPlistBufferAppendUnsigned(&buffer, (uint64_t)state.count, 8u) ||
            !__FXCFBPlistBufferAppendUnsigned(&buffer, 0u, 8u) ||
            !__FXCFBPlistBufferAppendUnsigned(&buffer, (uint64_t)offsetTableOffset, 8u)) {
            __FXCFSetError(error, realAllocator, kCFPropertyListWriteStreamError);
            goto cleanup;
        }
    }

    result = CFDataCreate(realAllocator, buffer.bytes, (CFIndex)buffer.length);
    if (result == NULL) {
        __FXCFSetError(error, realAllocator, kCFPropertyListWriteStreamError);
    }

cleanup:
    free(buffer.bytes);
    free(offsets);
    free(sizes);
    free(state.objects);
    free(state.visit);
    return result;
}

CFPropertyListRef _FXCFBinaryPListCreateWithData(
    CFAllocatorRef allocator,
    CFDataRef data,
    CFPropertyListMutabilityOptions options,
    CFPropertyListFormat *format,
    CFErrorRef *error
) {
    static const UInt8 header[FX_CF_BPLIST_HEADER_SIZE] = {'b', 'p', 'l', 'i', 's', 't', '0', '0'};
    struct __FXCFBPlistParseState state;
    CFAllocatorRef realAllocator = allocator == NULL ? CFAllocatorGetDefault() : allocator;
    CFPropertyListRef result = NULL;
    size_t offsetTableBytes = 0u;
    size_t index = 0u;

    if (format != NULL) {
        *format = 0;
    }
    if (error != NULL) {
        *error = NULL;
    }
    if (data == NULL) {
        return NULL;
    }
    _FXCFValidateType((CFTypeRef)data, CFDataGetTypeID(), "CFPropertyListCreateWithData called with non-data object");
    if (options != kCFPropertyListImmutable) {
        __FXCFSetError(error, realAllocator, kCFPropertyListReadCorruptError);
        return NULL;
    }

    memset(&state, 0, sizeof(state));
    state.allocator = realAllocator;
    state.length = (size_t)CFDataGetLength(data);
    state.bytes = CFDataGetBytePtr(data);
    if (state.length < FX_CF_BPLIST_HEADER_SIZE + FX_CF_BPLIST_TRAILER_SIZE ||
        state.bytes == NULL ||
        memcmp(state.bytes, header, sizeof(header)) != 0) {
        __FXCFSetError(error, realAllocator, kCFPropertyListReadCorruptError);
        return NULL;
    }

    {
        const UInt8 *trailer = state.bytes + state.length - FX_CF_BPLIST_TRAILER_SIZE;
        uint64_t numObjects = 0u;
        uint64_t topObject = 0u;
        uint64_t offsetTableOffset = 0u;
        state.offsetIntSize = trailer[6];
        state.objectRefSize = trailer[7];
        if ((state.offsetIntSize != 1u && state.offsetIntSize != 2u && state.offsetIntSize != 4u && state.offsetIntSize != 8u) ||
            (state.objectRefSize != 1u && state.objectRefSize != 2u && state.objectRefSize != 4u && state.objectRefSize != 8u) ||
            !__FXCFBPlistReadUnsigned(trailer + 8u, 8u, &numObjects) ||
            !__FXCFBPlistReadUnsigned(trailer + 16u, 8u, &topObject) ||
            !__FXCFBPlistReadUnsigned(trailer + 24u, 8u, &offsetTableOffset) ||
            numObjects == 0u ||
            numObjects > (uint64_t)LONG_MAX ||
            topObject >= numObjects ||
            offsetTableOffset > (uint64_t)(state.length - FX_CF_BPLIST_TRAILER_SIZE)) {
            __FXCFSetError(error, realAllocator, kCFPropertyListReadCorruptError);
            return NULL;
        }
        state.numObjects = (size_t)numObjects;
        state.topObject = (size_t)topObject;
        state.offsetTableOffset = (size_t)offsetTableOffset;
    }

    if (!__FXCFMultiplySize(state.numObjects, (size_t)state.offsetIntSize, &offsetTableBytes) ||
        state.offsetTableOffset + offsetTableBytes > state.length - FX_CF_BPLIST_TRAILER_SIZE) {
        __FXCFSetError(error, realAllocator, kCFPropertyListReadCorruptError);
        return NULL;
    }

    state.offsets = (size_t *)calloc(state.numObjects, sizeof(size_t));
    state.cache = (CFTypeRef *)calloc(state.numObjects, sizeof(CFTypeRef));
    state.state = (UInt8 *)calloc(state.numObjects, sizeof(UInt8));
    if (state.offsets == NULL || state.cache == NULL || state.state == NULL) {
        free(state.offsets);
        free(state.cache);
        free(state.state);
        __FXCFSetError(error, realAllocator, kCFPropertyListReadCorruptError);
        return NULL;
    }

    for (index = 0u; index < state.numObjects; ++index) {
        uint64_t offset = 0u;
        if (!__FXCFBPlistReadUnsigned(
                state.bytes + state.offsetTableOffset + (index * state.offsetIntSize),
                state.offsetIntSize,
                &offset
            ) ||
            offset >= (uint64_t)state.offsetTableOffset) {
            __FXCFSetError(error, realAllocator, kCFPropertyListReadCorruptError);
            goto cleanup;
        }
        state.offsets[index] = (size_t)offset;
    }

    result = __FXCFBPlistParseObject(&state, state.topObject, error);
    if (result == NULL) {
        goto cleanup;
    }

    if (format != NULL) {
        *format = kCFPropertyListBinaryFormat_v1_0;
    }

cleanup:
    __FXCFBPlistReleaseCache(state.cache, state.numObjects);
    free(state.state);
    free(state.cache);
    free(state.offsets);
    return result;
}
