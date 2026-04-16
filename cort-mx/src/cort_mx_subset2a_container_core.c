#include <CoreFoundation/CoreFoundation.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    kMaxText = 512
};

struct CaseResult {
    const char *name;
    const char *classification;
    const char *fxMatchLevel;
    const char *ownershipNote;
    bool success;
    bool hasTypeID;
    long typeID;
    bool hasTypeDescription;
    char typeDescription[kMaxText];
    bool hasPointerIdentity;
    bool pointerIdentitySame;
    bool hasLengthValue;
    long lengthValue;
    bool hasPrimaryValueText;
    char primaryValueText[kMaxText];
    bool hasAlternateValueText;
    char alternateValueText[kMaxText];
};

static void json_print_string(FILE *stream, const char *value) {
    fputc('"', stream);
    if (value != NULL) {
        for (const unsigned char *cursor = (const unsigned char *)value; *cursor != '\0'; ++cursor) {
            switch (*cursor) {
                case '\\':
                    fputs("\\\\", stream);
                    break;
                case '"':
                    fputs("\\\"", stream);
                    break;
                case '\n':
                    fputs("\\n", stream);
                    break;
                case '\r':
                    fputs("\\r", stream);
                    break;
                case '\t':
                    fputs("\\t", stream);
                    break;
                default:
                    if (*cursor < 0x20u) {
                        fprintf(stream, "\\u%04x", (unsigned int)*cursor);
                    } else {
                        fputc((int)*cursor, stream);
                    }
                    break;
            }
        }
    }
    fputc('"', stream);
}

static void json_print_bool_or_null(FILE *stream, bool hasValue, bool value) {
    if (!hasValue) {
        fputs("null", stream);
        return;
    }
    fputs(value ? "true" : "false", stream);
}

static void json_print_long_or_null(FILE *stream, bool hasValue, long value) {
    if (!hasValue) {
        fputs("null", stream);
        return;
    }
    fprintf(stream, "%ld", value);
}

static void json_print_text_or_null(FILE *stream, bool hasValue, const char *value) {
    if (!hasValue) {
        fputs("null", stream);
        return;
    }
    json_print_string(stream, value);
}

static void json_print_result(FILE *stream, const struct CaseResult *result, bool trailingComma) {
    fputs("    {\n", stream);
    fputs("      \"name\": ", stream);
    json_print_string(stream, result->name);
    fputs(",\n      \"classification\": ", stream);
    json_print_string(stream, result->classification);
    fputs(",\n      \"fx_match_level\": ", stream);
    json_print_string(stream, result->fxMatchLevel);
    fputs(",\n      \"success\": ", stream);
    fputs(result->success ? "true" : "false", stream);
    fputs(",\n      \"type_id\": ", stream);
    json_print_long_or_null(stream, result->hasTypeID, result->typeID);
    fputs(",\n      \"type_description\": ", stream);
    json_print_text_or_null(stream, result->hasTypeDescription, result->typeDescription);
    fputs(",\n      \"pointer_identity_same\": ", stream);
    json_print_bool_or_null(stream, result->hasPointerIdentity, result->pointerIdentitySame);
    fputs(",\n      \"length_value\": ", stream);
    json_print_long_or_null(stream, result->hasLengthValue, result->lengthValue);
    fputs(",\n      \"primary_value_text\": ", stream);
    json_print_text_or_null(stream, result->hasPrimaryValueText, result->primaryValueText);
    fputs(",\n      \"alternate_value_text\": ", stream);
    json_print_text_or_null(stream, result->hasAlternateValueText, result->alternateValueText);
    fputs(",\n      \"ownership_note\": ", stream);
    json_print_string(stream, result->ownershipNote);
    fputs("\n    }", stream);
    if (trailingComma) {
        fputc(',', stream);
    }
    fputc('\n', stream);
}

static void set_hex_bytes(char *buffer, size_t bufferSize, const UInt8 *bytes, size_t byteCount) {
    size_t offset = 0;
    if (bufferSize == 0) {
        return;
    }
    buffer[0] = '\0';
    for (size_t index = 0; index < byteCount && offset + 3 <= bufferSize; ++index) {
        int written = snprintf(buffer + offset, bufferSize - offset, "%02x", (unsigned int)bytes[index]);
        if (written < 0 || (size_t)written >= bufferSize - offset) {
            break;
        }
        offset += (size_t)written;
    }
}

static void cfstring_to_utf8(CFStringRef string, char *buffer, size_t bufferSize) {
    if (bufferSize == 0) {
        return;
    }
    buffer[0] = '\0';
    if (string == NULL) {
        return;
    }

    CFIndex length = CFStringGetLength(string);
    CFIndex maxBytes = CFStringGetMaximumSizeForEncoding(length, kCFStringEncodingUTF8);
    if (maxBytes < 0) {
        snprintf(buffer, bufferSize, "%s", "<conversion-failed>");
        return;
    }

    char *temporary = (char *)calloc((size_t)maxBytes + 1u, sizeof(char));
    if (temporary == NULL) {
        snprintf(buffer, bufferSize, "%s", "<conversion-failed>");
        return;
    }

    if (CFStringGetCString(string, temporary, maxBytes + 1, kCFStringEncodingUTF8)) {
        snprintf(buffer, bufferSize, "%s", temporary);
    } else {
        snprintf(buffer, bufferSize, "%s", "<conversion-failed>");
    }
    free(temporary);
}

static bool utf8_hex_for_string(CFStringRef string, char *buffer, size_t bufferSize) {
    if (string == NULL) {
        return false;
    }

    CFIndex length = CFStringGetLength(string);
    CFIndex maxBytes = CFStringGetMaximumSizeForEncoding(length, kCFStringEncodingUTF8);
    if (maxBytes < 0) {
        return false;
    }

    char *temporary = (char *)calloc((size_t)maxBytes + 1u, sizeof(char));
    if (temporary == NULL) {
        return false;
    }

    bool ok = CFStringGetCString(string, temporary, maxBytes + 1, kCFStringEncodingUTF8);
    if (ok) {
        set_hex_bytes(buffer, bufferSize, (const UInt8 *)temporary, strlen(temporary));
    }
    free(temporary);
    return ok;
}

static void record_type_identity(struct CaseResult *result, CFTypeRef object) {
    if (object == NULL) {
        return;
    }

    result->hasTypeID = true;
    result->typeID = (long)CFGetTypeID(object);

    CFStringRef description = CFCopyTypeIDDescription(CFGetTypeID(object));
    if (description != NULL) {
        result->hasTypeDescription = true;
        cfstring_to_utf8(description, result->typeDescription, sizeof(result->typeDescription));
        CFRelease(description);
    }
}

static CFStringRef create_ascii_string(const char *text) {
    return CFStringCreateWithCString(kCFAllocatorSystemDefault, text, kCFStringEncodingASCII);
}

static CFStringRef create_utf8_string(const char *text) {
    return CFStringCreateWithCString(kCFAllocatorSystemDefault, text, kCFStringEncodingUTF8);
}

static CFDataRef create_data(const UInt8 *bytes, CFIndex length) {
    return CFDataCreate(kCFAllocatorSystemDefault, bytes, length);
}

static struct CaseResult observe_array_empty_immutable(void) {
    struct CaseResult result;
    memset(&result, 0, sizeof(result));
    result.name = "cfarray_empty_immutable";
    result.classification = "required";
    result.fxMatchLevel = "semantic";
    result.ownershipNote = "Required empty immutable array semantics. Raw type numerics remain diagnostic across implementations.";

    CFArrayRef array = CFArrayCreate(kCFAllocatorSystemDefault, NULL, 0, &kCFTypeArrayCallBacks);
    record_type_identity(&result, (CFTypeRef)array);
    result.hasLengthValue = (array != NULL);
    result.lengthValue = (array != NULL) ? (long)CFArrayGetCount(array) : 0;
    result.hasPrimaryValueText = true;
    result.primaryValueText[0] = '\0';
    result.hasAlternateValueText = true;
    result.alternateValueText[0] = '\0';
    result.success = (array != NULL) && (CFArrayGetCount(array) == 0);

    if (array != NULL) {
        CFRelease(array);
    }
    return result;
}

static struct CaseResult observe_array_immutable_cftype_borrowed_get(void) {
    static const UInt8 bytes[] = {0x00u, 0x41u, 0x7fu, 0xffu};

    struct CaseResult result;
    memset(&result, 0, sizeof(result));
    result.name = "cfarray_immutable_cftype_borrowed_get";
    result.classification = "required";
    result.fxMatchLevel = "semantic";
    result.ownershipNote = "Immutable array create must retain inserted CFType children; Get returns borrowed child pointers without an extra retain in the tested kCFType callback mode.";

    CFStringRef key = create_ascii_string("alpha-key");
    CFDataRef data = create_data(bytes, (CFIndex)sizeof(bytes));
    CFIndex keyBase = (key != NULL) ? CFGetRetainCount(key) : -1;
    CFIndex dataBase = (data != NULL) ? CFGetRetainCount(data) : -1;
    const void *values[] = {key, data};
    CFArrayRef array = (key != NULL && data != NULL)
        ? CFArrayCreate(kCFAllocatorSystemDefault, values, 2, &kCFTypeArrayCallBacks)
        : NULL;

    CFIndex keyAfterCreate = (key != NULL) ? CFGetRetainCount(key) : -1;
    CFIndex dataAfterCreate = (data != NULL) ? CFGetRetainCount(data) : -1;
    const void *gotKey = (array != NULL) ? CFArrayGetValueAtIndex(array, 0) : NULL;
    const void *gotData = (array != NULL) ? CFArrayGetValueAtIndex(array, 1) : NULL;
    CFIndex arrayCount = (array != NULL) ? CFArrayGetCount(array) : 0;
    CFIndex keyAfterGet = (key != NULL) ? CFGetRetainCount(key) : -1;
    CFIndex dataAfterGet = (data != NULL) ? CFGetRetainCount(data) : -1;

    record_type_identity(&result, (CFTypeRef)array);
    result.hasPointerIdentity = true;
    result.pointerIdentitySame = (gotKey == (const void *)key) && (gotData == (const void *)data);
    result.hasLengthValue = (array != NULL);
    result.lengthValue = (array != NULL) ? (long)CFArrayGetCount(array) : 0;
    result.hasPrimaryValueText = (key != NULL) && utf8_hex_for_string(key, result.primaryValueText, sizeof(result.primaryValueText));
    result.hasAlternateValueText = true;
    set_hex_bytes(result.alternateValueText, sizeof(result.alternateValueText), bytes, sizeof(bytes));

    if (array != NULL) {
        CFRelease(array);
    }
    CFIndex keyAfterRelease = (key != NULL) ? CFGetRetainCount(key) : -1;
    CFIndex dataAfterRelease = (data != NULL) ? CFGetRetainCount(data) : -1;

    result.success =
        (key != NULL) &&
        (data != NULL) &&
        (array != NULL) &&
        result.pointerIdentitySame &&
        (arrayCount == 2) &&
        (keyAfterCreate == keyBase + 1) &&
        (dataAfterCreate == dataBase + 1) &&
        (keyAfterGet == keyAfterCreate) &&
        (dataAfterGet == dataAfterCreate) &&
        (keyAfterRelease == keyBase) &&
        (dataAfterRelease == dataBase);

    if (data != NULL) {
        CFRelease(data);
    }
    if (key != NULL) {
        CFRelease(key);
    }
    return result;
}

static struct CaseResult observe_array_mutable_append_remove_retains(void) {
    static const UInt8 bytes[] = {0xaau, 0xbbu, 0xccu};

    struct CaseResult result;
    memset(&result, 0, sizeof(result));
    result.name = "cfarray_mutable_append_remove_retains";
    result.classification = "required";
    result.fxMatchLevel = "semantic";
    result.ownershipNote = "Mutable array append retains, remove releases, deallocation releases remaining children, and Get remains borrowed in the tested kCFType callback mode.";

    CFDataRef data = create_data(bytes, (CFIndex)sizeof(bytes));
    CFIndex base = (data != NULL) ? CFGetRetainCount(data) : -1;
    CFMutableArrayRef array = (data != NULL)
        ? CFArrayCreateMutable(kCFAllocatorSystemDefault, 0, &kCFTypeArrayCallBacks)
        : NULL;

    record_type_identity(&result, (CFTypeRef)array);

    if (array != NULL && data != NULL) {
        CFArrayAppendValue(array, data);
    }
    CFIndex afterAppend = (data != NULL) ? CFGetRetainCount(data) : -1;
    const void *got = (array != NULL && CFArrayGetCount(array) == 1) ? CFArrayGetValueAtIndex(array, 0) : NULL;
    CFIndex afterGet = (data != NULL) ? CFGetRetainCount(data) : -1;
    if (array != NULL && CFArrayGetCount(array) == 1) {
        CFArrayRemoveValueAtIndex(array, 0);
    }
    CFIndex afterRemove = (data != NULL) ? CFGetRetainCount(data) : -1;

    result.hasPointerIdentity = true;
    result.pointerIdentitySame = (got == (const void *)data);
    result.hasLengthValue = (array != NULL);
    result.lengthValue = (array != NULL) ? (long)CFArrayGetCount(array) : 0;
    result.hasPrimaryValueText = true;
    snprintf(result.primaryValueText, sizeof(result.primaryValueText), "%s", "append-retain remove-release dealloc-release");
    result.hasAlternateValueText = true;
    snprintf(result.alternateValueText, sizeof(result.alternateValueText), "%s", "borrowed-get");

    if (array != NULL && data != NULL) {
        CFArrayAppendValue(array, data);
    }
    CFIndex afterReappend = (data != NULL) ? CFGetRetainCount(data) : -1;
    if (array != NULL) {
        CFRelease(array);
    }
    CFIndex afterArrayRelease = (data != NULL) ? CFGetRetainCount(data) : -1;

    result.success =
        (data != NULL) &&
        (array != NULL) &&
        result.pointerIdentitySame &&
        (afterAppend == base + 1) &&
        (afterGet == afterAppend) &&
        (afterRemove == base) &&
        (result.lengthValue == 0) &&
        (afterReappend == base + 1) &&
        (afterArrayRelease == base);

    if (data != NULL) {
        CFRelease(data);
    }
    return result;
}

static struct CaseResult observe_array_createcopy_borrowed_get(void) {
    static const UInt8 bytes[] = {0x11u, 0x22u};

    struct CaseResult result;
    memset(&result, 0, sizeof(result));
    result.name = "cfarray_createcopy_borrowed_get";
    result.classification = "required";
    result.fxMatchLevel = "semantic";
    result.ownershipNote = "Array CreateCopy must return an owned container with the same borrowed child pointer identity for the tested CFType children. Copy container pointer identity itself remains diagnostic only.";

    CFStringRef key = create_ascii_string("copy-array-key");
    CFDataRef data = create_data(bytes, (CFIndex)sizeof(bytes));
    const void *values[] = {key, data};
    CFArrayRef original = (key != NULL && data != NULL)
        ? CFArrayCreate(kCFAllocatorSystemDefault, values, 2, &kCFTypeArrayCallBacks)
        : NULL;
    CFArrayRef copy = (original != NULL) ? CFArrayCreateCopy(kCFAllocatorSystemDefault, original) : NULL;

    if (original != NULL) {
        CFRelease(original);
    }

    record_type_identity(&result, (CFTypeRef)copy);
    const void *gotKey = (copy != NULL && CFArrayGetCount(copy) == 2) ? CFArrayGetValueAtIndex(copy, 0) : NULL;
    result.hasPointerIdentity = true;
    result.pointerIdentitySame = (gotKey == (const void *)key);
    result.hasLengthValue = (copy != NULL);
    result.lengthValue = (copy != NULL) ? (long)CFArrayGetCount(copy) : 0;
    result.hasPrimaryValueText = (key != NULL) && utf8_hex_for_string(key, result.primaryValueText, sizeof(result.primaryValueText));
    result.hasAlternateValueText = true;
    snprintf(result.alternateValueText, sizeof(result.alternateValueText), "%s", "copy-preserves-child-identity");
    result.success =
        (key != NULL) &&
        (data != NULL) &&
        (copy != NULL) &&
        result.pointerIdentitySame &&
        (result.lengthValue == 2);

    if (copy != NULL) {
        CFRelease(copy);
    }
    if (data != NULL) {
        CFRelease(data);
    }
    if (key != NULL) {
        CFRelease(key);
    }
    return result;
}

static struct CaseResult observe_dictionary_empty_immutable(void) {
    struct CaseResult result;
    memset(&result, 0, sizeof(result));
    result.name = "cfdictionary_empty_immutable";
    result.classification = "required";
    result.fxMatchLevel = "semantic";
    result.ownershipNote = "Required empty immutable dictionary semantics. Raw type numerics remain diagnostic across implementations.";

    CFDictionaryRef dictionary = CFDictionaryCreate(
        kCFAllocatorSystemDefault,
        NULL,
        NULL,
        0,
        &kCFTypeDictionaryKeyCallBacks,
        &kCFTypeDictionaryValueCallBacks
    );
    record_type_identity(&result, (CFTypeRef)dictionary);
    result.hasLengthValue = (dictionary != NULL);
    result.lengthValue = (dictionary != NULL) ? (long)CFDictionaryGetCount(dictionary) : 0;
    result.hasPrimaryValueText = true;
    result.primaryValueText[0] = '\0';
    result.hasAlternateValueText = true;
    result.alternateValueText[0] = '\0';
    result.success = (dictionary != NULL) && (CFDictionaryGetCount(dictionary) == 0);

    if (dictionary != NULL) {
        CFRelease(dictionary);
    }
    return result;
}

static struct CaseResult observe_dictionary_immutable_string_key_lookup(void) {
    static const UInt8 bytes[] = {0x10u, 0x20u, 0x30u, 0x40u};

    struct CaseResult result;
    memset(&result, 0, sizeof(result));
    result.name = "cfdictionary_immutable_string_key_lookup";
    result.classification = "required";
    result.fxMatchLevel = "semantic";
    result.ownershipNote = "Immutable dictionary create must retain string keys and CFType values; Get and GetValueIfPresent return borrowed value pointers without an extra retain for the tested string-key path.";

    CFStringRef key = create_utf8_string("caf\xC3\xA9-\xE2\x98\x83");
    CFStringRef missingKey = create_ascii_string("missing-key");
    CFDataRef value = create_data(bytes, (CFIndex)sizeof(bytes));
    CFIndex keyBase = (key != NULL) ? CFGetRetainCount(key) : -1;
    CFIndex valueBase = (value != NULL) ? CFGetRetainCount(value) : -1;
    const void *keys[] = {key};
    const void *values[] = {value};
    CFDictionaryRef dictionary = (key != NULL && value != NULL)
        ? CFDictionaryCreate(
            kCFAllocatorSystemDefault,
            keys,
            values,
            1,
            &kCFTypeDictionaryKeyCallBacks,
            &kCFTypeDictionaryValueCallBacks
        )
        : NULL;

    CFIndex keyAfterCreate = (key != NULL) ? CFGetRetainCount(key) : -1;
    CFIndex valueAfterCreate = (value != NULL) ? CFGetRetainCount(value) : -1;
    const void *got = (dictionary != NULL) ? CFDictionaryGetValue(dictionary, key) : NULL;
    CFIndex valueAfterGet = (value != NULL) ? CFGetRetainCount(value) : -1;
    const void *presentValue = NULL;
    Boolean foundPresent = (dictionary != NULL) ? CFDictionaryGetValueIfPresent(dictionary, key, &presentValue) : false;
    CFIndex valueAfterPresent = (value != NULL) ? CFGetRetainCount(value) : -1;
    const void *missingValue = NULL;
    Boolean foundMissing = (dictionary != NULL && missingKey != NULL)
        ? CFDictionaryGetValueIfPresent(dictionary, missingKey, &missingValue)
        : false;

    record_type_identity(&result, (CFTypeRef)dictionary);
    result.hasPointerIdentity = true;
    result.pointerIdentitySame = (got == (const void *)value) && (presentValue == (const void *)value);
    result.hasLengthValue = (dictionary != NULL);
    result.lengthValue = (dictionary != NULL) ? (long)CFDictionaryGetCount(dictionary) : 0;
    result.hasPrimaryValueText = (key != NULL) && utf8_hex_for_string(key, result.primaryValueText, sizeof(result.primaryValueText));
    result.hasAlternateValueText = true;
    set_hex_bytes(result.alternateValueText, sizeof(result.alternateValueText), bytes, sizeof(bytes));

    if (dictionary != NULL) {
        CFRelease(dictionary);
    }
    CFIndex keyAfterRelease = (key != NULL) ? CFGetRetainCount(key) : -1;
    CFIndex valueAfterRelease = (value != NULL) ? CFGetRetainCount(value) : -1;

    result.success =
        (key != NULL) &&
        (missingKey != NULL) &&
        (value != NULL) &&
        (dictionary != NULL) &&
        result.pointerIdentitySame &&
        (result.lengthValue == 1) &&
        foundPresent &&
        !foundMissing &&
        (keyAfterCreate == keyBase + 1) &&
        (valueAfterCreate == valueBase + 1) &&
        (valueAfterGet == valueAfterCreate) &&
        (valueAfterPresent == valueAfterCreate) &&
        (keyAfterRelease == keyBase) &&
        (valueAfterRelease == valueBase);

    if (value != NULL) {
        CFRelease(value);
    }
    if (missingKey != NULL) {
        CFRelease(missingKey);
    }
    if (key != NULL) {
        CFRelease(key);
    }
    return result;
}

static struct CaseResult observe_dictionary_mutable_set_replace_remove_retains(void) {
    static const UInt8 value1Bytes[] = {0x01u, 0x02u};
    static const UInt8 value2Bytes[] = {0x03u, 0x04u};

    struct CaseResult result;
    memset(&result, 0, sizeof(result));
    result.name = "cfdictionary_mutable_set_replace_remove_retains";
    result.classification = "required";
    result.fxMatchLevel = "semantic";
    result.ownershipNote = "Mutable dictionary set retains key/value, replacement releases the old value, removal releases key/value, and deallocation releases remaining children for the tested kCFType callback mode.";

    CFStringRef key = create_ascii_string("subset2a-key");
    CFDataRef value1 = create_data(value1Bytes, (CFIndex)sizeof(value1Bytes));
    CFDataRef value2 = create_data(value2Bytes, (CFIndex)sizeof(value2Bytes));
    CFMutableDictionaryRef dictionary =
        (key != NULL && value1 != NULL && value2 != NULL)
            ? CFDictionaryCreateMutable(
                kCFAllocatorSystemDefault,
                0,
                &kCFTypeDictionaryKeyCallBacks,
                &kCFTypeDictionaryValueCallBacks
            )
            : NULL;

    CFIndex keyBase = (key != NULL) ? CFGetRetainCount(key) : -1;
    CFIndex value1Base = (value1 != NULL) ? CFGetRetainCount(value1) : -1;
    CFIndex value2Base = (value2 != NULL) ? CFGetRetainCount(value2) : -1;

    record_type_identity(&result, (CFTypeRef)dictionary);

    if (dictionary != NULL) {
        CFDictionarySetValue(dictionary, key, value1);
    }
    CFIndex keyAfterSet = (key != NULL) ? CFGetRetainCount(key) : -1;
    CFIndex value1AfterSet = (value1 != NULL) ? CFGetRetainCount(value1) : -1;
    const void *got1 = (dictionary != NULL) ? CFDictionaryGetValue(dictionary, key) : NULL;
    CFIndex value1AfterGet = (value1 != NULL) ? CFGetRetainCount(value1) : -1;

    if (dictionary != NULL) {
        CFDictionarySetValue(dictionary, key, value2);
    }
    CFIndex keyAfterReplace = (key != NULL) ? CFGetRetainCount(key) : -1;
    CFIndex value1AfterReplace = (value1 != NULL) ? CFGetRetainCount(value1) : -1;
    CFIndex value2AfterReplace = (value2 != NULL) ? CFGetRetainCount(value2) : -1;
    const void *got2 = (dictionary != NULL) ? CFDictionaryGetValue(dictionary, key) : NULL;

    if (dictionary != NULL) {
        CFDictionaryRemoveValue(dictionary, key);
    }
    CFIndex keyAfterRemove = (key != NULL) ? CFGetRetainCount(key) : -1;
    CFIndex value2AfterRemove = (value2 != NULL) ? CFGetRetainCount(value2) : -1;

    result.hasPointerIdentity = true;
    result.pointerIdentitySame = (got1 == (const void *)value1) && (got2 == (const void *)value2);
    result.hasLengthValue = (dictionary != NULL);
    result.lengthValue = (dictionary != NULL) ? (long)CFDictionaryGetCount(dictionary) : 0;
    result.hasPrimaryValueText = (key != NULL) && utf8_hex_for_string(key, result.primaryValueText, sizeof(result.primaryValueText));
    result.hasAlternateValueText = true;
    snprintf(result.alternateValueText, sizeof(result.alternateValueText), "%s", "set-retain replace-release remove-release dealloc-release");

    if (dictionary != NULL) {
        CFDictionarySetValue(dictionary, key, value1);
    }
    CFIndex keyAfterReseed = (key != NULL) ? CFGetRetainCount(key) : -1;
    CFIndex value1AfterReseed = (value1 != NULL) ? CFGetRetainCount(value1) : -1;
    if (dictionary != NULL) {
        CFRelease(dictionary);
    }
    CFIndex keyAfterDealloc = (key != NULL) ? CFGetRetainCount(key) : -1;
    CFIndex value1AfterDealloc = (value1 != NULL) ? CFGetRetainCount(value1) : -1;

    result.success =
        (key != NULL) &&
        (value1 != NULL) &&
        (value2 != NULL) &&
        (dictionary != NULL) &&
        result.pointerIdentitySame &&
        (keyAfterSet == keyBase + 1) &&
        (value1AfterSet == value1Base + 1) &&
        (value1AfterGet == value1AfterSet) &&
        (keyAfterReplace == keyBase + 1) &&
        (value1AfterReplace == value1Base) &&
        (value2AfterReplace == value2Base + 1) &&
        (keyAfterRemove == keyBase) &&
        (value2AfterRemove == value2Base) &&
        (result.lengthValue == 0) &&
        (keyAfterReseed == keyBase + 1) &&
        (value1AfterReseed == value1Base + 1) &&
        (keyAfterDealloc == keyBase) &&
        (value1AfterDealloc == value1Base);

    if (value2 != NULL) {
        CFRelease(value2);
    }
    if (value1 != NULL) {
        CFRelease(value1);
    }
    if (key != NULL) {
        CFRelease(key);
    }
    return result;
}

static struct CaseResult observe_dictionary_createcopy_borrowed_lookup(void) {
    static const UInt8 bytes[] = {0x55u, 0x66u, 0x77u, 0x88u};

    struct CaseResult result;
    memset(&result, 0, sizeof(result));
    result.name = "cfdictionary_createcopy_borrowed_lookup";
    result.classification = "required";
    result.fxMatchLevel = "semantic";
    result.ownershipNote = "Dictionary CreateCopy must return an owned container with the same borrowed value pointer identity for the tested string-key entry. Copy container pointer identity itself remains diagnostic only.";

    CFStringRef key = create_ascii_string("copy-dict-key");
    CFDataRef value = create_data(bytes, (CFIndex)sizeof(bytes));
    const void *keys[] = {key};
    const void *values[] = {value};
    CFDictionaryRef original = (key != NULL && value != NULL)
        ? CFDictionaryCreate(
            kCFAllocatorSystemDefault,
            keys,
            values,
            1,
            &kCFTypeDictionaryKeyCallBacks,
            &kCFTypeDictionaryValueCallBacks
        )
        : NULL;
    CFDictionaryRef copy = (original != NULL) ? CFDictionaryCreateCopy(kCFAllocatorSystemDefault, original) : NULL;

    if (original != NULL) {
        CFRelease(original);
    }

    record_type_identity(&result, (CFTypeRef)copy);
    const void *got = (copy != NULL) ? CFDictionaryGetValue(copy, key) : NULL;
    result.hasPointerIdentity = true;
    result.pointerIdentitySame = (got == (const void *)value);
    result.hasLengthValue = (copy != NULL);
    result.lengthValue = (copy != NULL) ? (long)CFDictionaryGetCount(copy) : 0;
    result.hasPrimaryValueText = (key != NULL) && utf8_hex_for_string(key, result.primaryValueText, sizeof(result.primaryValueText));
    result.hasAlternateValueText = true;
    set_hex_bytes(result.alternateValueText, sizeof(result.alternateValueText), bytes, sizeof(bytes));
    result.success =
        (key != NULL) &&
        (value != NULL) &&
        (copy != NULL) &&
        result.pointerIdentitySame &&
        (result.lengthValue == 1);

    if (copy != NULL) {
        CFRelease(copy);
    }
    if (value != NULL) {
        CFRelease(value);
    }
    if (key != NULL) {
        CFRelease(key);
    }
    return result;
}

int main(void) {
    struct CaseResult results[] = {
        observe_array_empty_immutable(),
        observe_array_immutable_cftype_borrowed_get(),
        observe_array_mutable_append_remove_retains(),
        observe_array_createcopy_borrowed_get(),
        observe_dictionary_empty_immutable(),
        observe_dictionary_immutable_string_key_lookup(),
        observe_dictionary_mutable_set_replace_remove_retains(),
        observe_dictionary_createcopy_borrowed_lookup()
    };
    size_t caseCount = sizeof(results) / sizeof(results[0]);

    fputs("{\n", stdout);
    fputs("  \"probe\": \"subset2a_container_core\",\n", stdout);
    fputs("  \"results\": [\n", stdout);
    for (size_t index = 0; index < caseCount; ++index) {
        json_print_result(stdout, &results[index], index + 1 < caseCount);
    }
    fputs("  ]\n", stdout);
    fputs("}\n", stdout);
    return 0;
}
