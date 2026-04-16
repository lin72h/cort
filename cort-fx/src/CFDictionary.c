#include "FXCFInternal.h"

#include <CoreFoundation/CFDictionary.h>

enum {
    FX_CFDICTIONARY_FLAG_MUTABLE = 1u
};

struct __CFDictionaryEntry {
    const void *key;
    const void *value;
};

struct __CFDictionary {
    CFRuntimeBase _base;
    CFIndex count;
    CFIndex capacity;
    UInt32 flags;
    struct __CFDictionaryEntry *entries;
};

static const void *__CFTypeDictionaryRetain(CFAllocatorRef allocator, const void *value) {
    (void)allocator;
    return CFRetain((CFTypeRef)value);
}

static void __CFTypeDictionaryRelease(CFAllocatorRef allocator, const void *value) {
    (void)allocator;
    CFRelease((CFTypeRef)value);
}

static Boolean __CFTypeDictionaryEqual(const void *value1, const void *value2) {
    return CFEqual((CFTypeRef)value1, (CFTypeRef)value2);
}

static CFHashCode __CFTypeDictionaryHash(const void *value) {
    return CFHash((CFTypeRef)value);
}

const CFDictionaryKeyCallBacks kCFTypeDictionaryKeyCallBacks = {
    0,
    __CFTypeDictionaryRetain,
    __CFTypeDictionaryRelease,
    NULL,
    __CFTypeDictionaryEqual,
    __CFTypeDictionaryHash
};

const CFDictionaryValueCallBacks kCFTypeDictionaryValueCallBacks = {
    0,
    __CFTypeDictionaryRetain,
    __CFTypeDictionaryRelease,
    NULL,
    __CFTypeDictionaryEqual
};

static void __CFDictionaryFinalize(CFTypeRef cf) {
    struct __CFDictionary *dictionary = (struct __CFDictionary *)cf;
    for (CFIndex index = 0; index < dictionary->count; ++index) {
        CFRelease((CFTypeRef)dictionary->entries[index].key);
        CFRelease((CFTypeRef)dictionary->entries[index].value);
    }
    if (dictionary->entries != NULL) {
        CFAllocatorDeallocate(CFGetAllocator(cf), dictionary->entries);
    }
}

static const CFRuntimeClass __CFDictionaryClass = {
    0,
    "CFDictionary",
    NULL,
    NULL,
    __CFDictionaryFinalize,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    0
};

const CFRuntimeClass *_FXCFDictionaryClass(void) {
    return &__CFDictionaryClass;
}

static Boolean __CFDictionaryKeyCallbacksAreSupported(const CFDictionaryKeyCallBacks *callbacks) {
    return
        callbacks != NULL &&
        callbacks->version == 0 &&
        callbacks->retain == kCFTypeDictionaryKeyCallBacks.retain &&
        callbacks->release == kCFTypeDictionaryKeyCallBacks.release &&
        callbacks->copyDescription == kCFTypeDictionaryKeyCallBacks.copyDescription &&
        callbacks->equal == kCFTypeDictionaryKeyCallBacks.equal &&
        callbacks->hash == kCFTypeDictionaryKeyCallBacks.hash;
}

static Boolean __CFDictionaryValueCallbacksAreSupported(const CFDictionaryValueCallBacks *callbacks) {
    return
        callbacks != NULL &&
        callbacks->version == 0 &&
        callbacks->retain == kCFTypeDictionaryValueCallBacks.retain &&
        callbacks->release == kCFTypeDictionaryValueCallBacks.release &&
        callbacks->copyDescription == kCFTypeDictionaryValueCallBacks.copyDescription &&
        callbacks->equal == kCFTypeDictionaryValueCallBacks.equal;
}

static void __CFDictionaryValidate(CFDictionaryRef theDictionary, const char *apiName) {
    _FXCFValidateType((CFTypeRef)theDictionary, _kCFRuntimeIDCFDictionary, apiName);
}

static void __CFDictionaryValidateMutable(CFMutableDictionaryRef theDictionary, const char *apiName) {
    struct __CFDictionary *dictionary = (struct __CFDictionary *)theDictionary;
    __CFDictionaryValidate((CFDictionaryRef)dictionary, apiName);
    if ((dictionary->flags & FX_CFDICTIONARY_FLAG_MUTABLE) == 0u) {
        _FXCFAbort(apiName);
    }
}

static Boolean __CFDictionaryCheckedAllocationSize(CFIndex capacity, size_t *sizeOut) {
    if (capacity < 0) {
        return false;
    }
    if ((uint64_t)capacity > (uint64_t)(SIZE_MAX / sizeof(struct __CFDictionaryEntry))) {
        return false;
    }
    *sizeOut = (size_t)capacity * sizeof(struct __CFDictionaryEntry);
    return true;
}

static Boolean __CFDictionaryEnsureCapacity(struct __CFDictionary *dictionary, CFIndex requiredCount) {
    if (requiredCount <= dictionary->capacity) {
        return true;
    }

    CFIndex newCapacity = dictionary->capacity > 0 ? dictionary->capacity : 4;
    while (newCapacity < requiredCount) {
        if (newCapacity > LONG_MAX / 2) {
            newCapacity = requiredCount;
            break;
        }
        newCapacity *= 2;
    }

    size_t newSize = 0;
    if (!__CFDictionaryCheckedAllocationSize(newCapacity, &newSize)) {
        return false;
    }

    CFAllocatorRef allocator = CFGetAllocator((CFTypeRef)dictionary);
    struct __CFDictionaryEntry *newEntries = NULL;
    if (newSize > 0) {
        newEntries = (struct __CFDictionaryEntry *)CFAllocatorAllocate(allocator, (CFIndex)newSize, 0);
        if (newEntries == NULL) {
            return false;
        }
        if (dictionary->count > 0) {
            memcpy(newEntries, dictionary->entries, (size_t)dictionary->count * sizeof(struct __CFDictionaryEntry));
        }
    }

    if (dictionary->entries != NULL) {
        CFAllocatorDeallocate(allocator, dictionary->entries);
    }
    dictionary->entries = newEntries;
    dictionary->capacity = newCapacity;
    return true;
}

static CFIndex __CFDictionaryFindIndex(const struct __CFDictionary *dictionary, const void *key) {
    for (CFIndex index = 0; index < dictionary->count; ++index) {
        const void *entryKey = dictionary->entries[index].key;
        if (entryKey == key) {
            return index;
        }
        if (CFEqual((CFTypeRef)entryKey, (CFTypeRef)key)) {
            return index;
        }
    }
    return kCFNotFound;
}

static Boolean __CFDictionaryInsertOrReplace(struct __CFDictionary *dictionary, const void *key, const void *value) {
    CFIndex index = __CFDictionaryFindIndex(dictionary, key);
    if (index != kCFNotFound) {
        const void *retainedValue = CFRetain((CFTypeRef)value);
        CFRelease((CFTypeRef)dictionary->entries[index].value);
        dictionary->entries[index].value = retainedValue;
        return true;
    }

    if (!__CFDictionaryEnsureCapacity(dictionary, dictionary->count + 1)) {
        return false;
    }

    dictionary->entries[dictionary->count].key = CFRetain((CFTypeRef)key);
    dictionary->entries[dictionary->count].value = CFRetain((CFTypeRef)value);
    dictionary->count += 1;
    return true;
}

CFTypeID CFDictionaryGetTypeID(void) {
    return _kCFRuntimeIDCFDictionary;
}

CFDictionaryRef CFDictionaryCreate(CFAllocatorRef allocator, const void **keys, const void **values, CFIndex numValues, const CFDictionaryKeyCallBacks *keyCallBacks, const CFDictionaryValueCallBacks *valueCallBacks) {
    if (numValues < 0) {
        return NULL;
    }
    if (numValues > 0 && (keys == NULL || values == NULL)) {
        return NULL;
    }
    if (!__CFDictionaryKeyCallbacksAreSupported(keyCallBacks) || !__CFDictionaryValueCallbacksAreSupported(valueCallBacks)) {
        return NULL;
    }

    struct __CFDictionary *dictionary = (struct __CFDictionary *)_CFRuntimeCreateInstance(
        allocator,
        CFDictionaryGetTypeID(),
        (CFIndex)(sizeof(struct __CFDictionary) - sizeof(CFRuntimeBase)),
        NULL
    );
    if (dictionary == NULL) {
        return NULL;
    }

    if (numValues > 0 && !__CFDictionaryEnsureCapacity(dictionary, numValues)) {
        CFRelease((CFTypeRef)dictionary);
        return NULL;
    }

    for (CFIndex index = 0; index < numValues; ++index) {
        if (keys[index] == NULL || values[index] == NULL) {
            CFRelease((CFTypeRef)dictionary);
            return NULL;
        }
        if (!__CFDictionaryInsertOrReplace(dictionary, keys[index], values[index])) {
            CFRelease((CFTypeRef)dictionary);
            return NULL;
        }
    }

    return dictionary;
}

CFDictionaryRef CFDictionaryCreateCopy(CFAllocatorRef allocator, CFDictionaryRef theDictionary) {
    __CFDictionaryValidate(theDictionary, "CFDictionaryCreateCopy called with non-dictionary object");
    const struct __CFDictionary *dictionary = (const struct __CFDictionary *)theDictionary;
    struct __CFDictionary *copy = (struct __CFDictionary *)_CFRuntimeCreateInstance(
        allocator,
        CFDictionaryGetTypeID(),
        (CFIndex)(sizeof(struct __CFDictionary) - sizeof(CFRuntimeBase)),
        NULL
    );
    if (copy == NULL) {
        return NULL;
    }

    if (dictionary->count > 0 && !__CFDictionaryEnsureCapacity(copy, dictionary->count)) {
        CFRelease((CFTypeRef)copy);
        return NULL;
    }

    for (CFIndex index = 0; index < dictionary->count; ++index) {
        if (!__CFDictionaryInsertOrReplace(copy, dictionary->entries[index].key, dictionary->entries[index].value)) {
            CFRelease((CFTypeRef)copy);
            return NULL;
        }
    }
    return copy;
}

CFMutableDictionaryRef CFDictionaryCreateMutable(CFAllocatorRef allocator, CFIndex capacity, const CFDictionaryKeyCallBacks *keyCallBacks, const CFDictionaryValueCallBacks *valueCallBacks) {
    if (capacity < 0) {
        return NULL;
    }
    if (!__CFDictionaryKeyCallbacksAreSupported(keyCallBacks) || !__CFDictionaryValueCallbacksAreSupported(valueCallBacks)) {
        return NULL;
    }

    struct __CFDictionary *dictionary = (struct __CFDictionary *)_CFRuntimeCreateInstance(
        allocator,
        CFDictionaryGetTypeID(),
        (CFIndex)(sizeof(struct __CFDictionary) - sizeof(CFRuntimeBase)),
        NULL
    );
    if (dictionary == NULL) {
        return NULL;
    }

    dictionary->flags = FX_CFDICTIONARY_FLAG_MUTABLE;
    if (capacity > 0 && !__CFDictionaryEnsureCapacity(dictionary, capacity)) {
        CFRelease((CFTypeRef)dictionary);
        return NULL;
    }
    return dictionary;
}

CFIndex CFDictionaryGetCount(CFDictionaryRef theDictionary) {
    __CFDictionaryValidate(theDictionary, "CFDictionaryGetCount called with non-dictionary object");
    return ((const struct __CFDictionary *)theDictionary)->count;
}

const void *CFDictionaryGetValue(CFDictionaryRef theDictionary, const void *key) {
    __CFDictionaryValidate(theDictionary, "CFDictionaryGetValue called with non-dictionary object");
    if (key == NULL) {
        _FXCFAbort("CFDictionaryGetValue called with NULL key");
    }

    const struct __CFDictionary *dictionary = (const struct __CFDictionary *)theDictionary;
    CFIndex index = __CFDictionaryFindIndex(dictionary, key);
    if (index == kCFNotFound) {
        return NULL;
    }
    return dictionary->entries[index].value;
}

Boolean CFDictionaryGetValueIfPresent(CFDictionaryRef theDictionary, const void *key, const void **value) {
    __CFDictionaryValidate(theDictionary, "CFDictionaryGetValueIfPresent called with non-dictionary object");
    if (key == NULL) {
        _FXCFAbort("CFDictionaryGetValueIfPresent called with NULL key");
    }

    const struct __CFDictionary *dictionary = (const struct __CFDictionary *)theDictionary;
    CFIndex index = __CFDictionaryFindIndex(dictionary, key);
    if (index == kCFNotFound) {
        if (value != NULL) {
            *value = NULL;
        }
        return false;
    }

    if (value != NULL) {
        *value = dictionary->entries[index].value;
    }
    return true;
}

void CFDictionarySetValue(CFMutableDictionaryRef theDictionary, const void *key, const void *value) {
    struct __CFDictionary *dictionary = (struct __CFDictionary *)theDictionary;
    __CFDictionaryValidateMutable(theDictionary, "CFDictionarySetValue called on immutable or non-dictionary object");
    if (key == NULL || value == NULL) {
        _FXCFAbort("CFDictionarySetValue called with NULL key or value");
    }
    if (!__CFDictionaryInsertOrReplace(dictionary, key, value)) {
        _FXCFAbort("CFDictionarySetValue allocation failed");
    }
}

void CFDictionaryRemoveValue(CFMutableDictionaryRef theDictionary, const void *key) {
    struct __CFDictionary *dictionary = (struct __CFDictionary *)theDictionary;
    __CFDictionaryValidateMutable(theDictionary, "CFDictionaryRemoveValue called on immutable or non-dictionary object");
    if (key == NULL) {
        _FXCFAbort("CFDictionaryRemoveValue called with NULL key");
    }

    CFIndex index = __CFDictionaryFindIndex(dictionary, key);
    if (index == kCFNotFound) {
        return;
    }

    CFRelease((CFTypeRef)dictionary->entries[index].key);
    CFRelease((CFTypeRef)dictionary->entries[index].value);
    if (index + 1 < dictionary->count) {
        memmove(
            &dictionary->entries[index],
            &dictionary->entries[index + 1],
            (size_t)(dictionary->count - index - 1) * sizeof(struct __CFDictionaryEntry)
        );
    }
    dictionary->count -= 1;
    if (dictionary->entries != NULL) {
        dictionary->entries[dictionary->count].key = NULL;
        dictionary->entries[dictionary->count].value = NULL;
    }
}
