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

static void record_type_identity(struct CaseResult *result, CFTypeRef object, const char *typeDescription) {
    result->hasTypeID = true;
    result->typeID = (long)CFGetTypeID(object);
    result->hasTypeDescription = true;
    snprintf(result->typeDescription, sizeof(result->typeDescription), "%s", typeDescription);
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
    if (array != NULL) {
        record_type_identity(&result, (CFTypeRef)array, "CFArray");
        result.hasLengthValue = true;
        result.lengthValue = (long)CFArrayGetCount(array);
        result.hasPrimaryValueText = true;
        result.primaryValueText[0] = '\0';
        result.hasAlternateValueText = true;
        result.alternateValueText[0] = '\0';
        result.success = CFArrayGetCount(array) == 0;
        CFRelease((CFTypeRef)array);
    }
    return result;
}

static struct CaseResult observe_array_immutable_cftype_borrowed_get(void) {
    static const UInt8 firstBytes[] = {
        0x61u, 0x6cu, 0x70u, 0x68u, 0x61u, 0x2du, 0x6bu, 0x65u, 0x79u
    };
    static const UInt8 secondBytes[] = {0x00u, 0x41u, 0x7fu, 0xffu};

    struct CaseResult result;
    memset(&result, 0, sizeof(result));
    result.name = "cfarray_immutable_cftype_borrowed_get";
    result.classification = "required";
    result.fxMatchLevel = "semantic";
    result.ownershipNote = "Immutable array create must retain inserted CFType children; Get returns borrowed child pointers without an extra retain in the tested kCFType callback mode.";

    CFDataRef first = create_data(firstBytes, (CFIndex)sizeof(firstBytes));
    CFDataRef second = create_data(secondBytes, (CFIndex)sizeof(secondBytes));
    const void *values[] = {first, second};
    CFIndex firstBase = (first != NULL) ? CFGetRetainCount((CFTypeRef)first) : -1;
    CFIndex secondBase = (second != NULL) ? CFGetRetainCount((CFTypeRef)second) : -1;
    CFArrayRef array = (first != NULL && second != NULL)
        ? CFArrayCreate(kCFAllocatorSystemDefault, values, 2, &kCFTypeArrayCallBacks)
        : NULL;

    if (array != NULL) {
        const void *gotFirst = CFArrayGetValueAtIndex(array, 0);
        const void *gotSecond = CFArrayGetValueAtIndex(array, 1);
        record_type_identity(&result, (CFTypeRef)array, "CFArray");
        result.hasPointerIdentity = true;
        result.pointerIdentitySame = (gotFirst == (const void *)first) && (gotSecond == (const void *)second);
        result.hasLengthValue = true;
        result.lengthValue = (long)CFArrayGetCount(array);
        result.hasPrimaryValueText = true;
        set_hex_bytes(result.primaryValueText, sizeof(result.primaryValueText), firstBytes, sizeof(firstBytes));
        result.hasAlternateValueText = true;
        set_hex_bytes(result.alternateValueText, sizeof(result.alternateValueText), secondBytes, sizeof(secondBytes));
        result.success =
            result.pointerIdentitySame &&
            result.lengthValue == 2 &&
            CFGetRetainCount((CFTypeRef)first) == firstBase + 1 &&
            CFGetRetainCount((CFTypeRef)second) == secondBase + 1;
        CFRelease((CFTypeRef)array);
        result.success =
            result.success &&
            CFGetRetainCount((CFTypeRef)first) == firstBase &&
            CFGetRetainCount((CFTypeRef)second) == secondBase;
    }

    if (second != NULL) {
        CFRelease((CFTypeRef)second);
    }
    if (first != NULL) {
        CFRelease((CFTypeRef)first);
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
    CFMutableArrayRef array = (data != NULL)
        ? CFArrayCreateMutable(kCFAllocatorSystemDefault, 0, &kCFTypeArrayCallBacks)
        : NULL;
    CFIndex base = (data != NULL) ? CFGetRetainCount((CFTypeRef)data) : -1;

    if (array != NULL && data != NULL) {
        record_type_identity(&result, (CFTypeRef)array, "CFArray");
        CFArrayAppendValue(array, data);
        result.hasPointerIdentity = true;
        result.pointerIdentitySame = CFArrayGetValueAtIndex(array, 0) == (const void *)data;
        CFArrayRemoveValueAtIndex(array, 0);
        result.hasLengthValue = true;
        result.lengthValue = (long)CFArrayGetCount(array);
        result.hasPrimaryValueText = true;
        snprintf(result.primaryValueText, sizeof(result.primaryValueText), "%s", "append-retain remove-release dealloc-release");
        result.hasAlternateValueText = true;
        snprintf(result.alternateValueText, sizeof(result.alternateValueText), "%s", "borrowed-get");
        result.success =
            result.pointerIdentitySame &&
            CFGetRetainCount((CFTypeRef)data) == base &&
            result.lengthValue == 0;
        CFArrayAppendValue(array, data);
        result.success = result.success && CFGetRetainCount((CFTypeRef)data) == base + 1;
        CFRelease((CFTypeRef)array);
        result.success = result.success && CFGetRetainCount((CFTypeRef)data) == base;
    }

    if (data != NULL) {
        CFRelease((CFTypeRef)data);
    }
    return result;
}

static struct CaseResult observe_array_createcopy_borrowed_get(void) {
    struct CaseResult result;
    memset(&result, 0, sizeof(result));
    result.name = "cfarray_createcopy_borrowed_get";
    result.classification = "required";
    result.fxMatchLevel = "semantic";
    result.ownershipNote = "Array CreateCopy must return an owned container with the same borrowed child pointer identity for the tested CFType children. Copy container pointer identity itself remains diagnostic only.";

    static const UInt8 bytes[] = {0x11u, 0x22u};
    CFStringRef key = create_ascii_string("copy-array-key");
    CFDataRef data = create_data(bytes, (CFIndex)sizeof(bytes));
    const void *values[] = {key, data};
    CFArrayRef original = (key != NULL && data != NULL)
        ? CFArrayCreate(kCFAllocatorSystemDefault, values, 2, &kCFTypeArrayCallBacks)
        : NULL;
    CFArrayRef copy = (original != NULL) ? CFArrayCreateCopy(kCFAllocatorSystemDefault, original) : NULL;

    if (original != NULL) {
        CFRelease((CFTypeRef)original);
    }

    if (copy != NULL) {
        record_type_identity(&result, (CFTypeRef)copy, "CFArray");
        result.hasPointerIdentity = true;
        result.pointerIdentitySame =
            CFArrayGetValueAtIndex(copy, 0) == (const void *)key &&
            CFArrayGetValueAtIndex(copy, 1) == (const void *)data;
        result.hasLengthValue = true;
        result.lengthValue = (long)CFArrayGetCount(copy);
        result.hasPrimaryValueText = true;
        snprintf(result.primaryValueText, sizeof(result.primaryValueText), "%s", "636f70792d61727261792d6b6579");
        result.hasAlternateValueText = true;
        snprintf(result.alternateValueText, sizeof(result.alternateValueText), "%s", "copy-preserves-child-identity");
        result.success = result.pointerIdentitySame && result.lengthValue == 2;
        CFRelease((CFTypeRef)copy);
    }

    if (data != NULL) {
        CFRelease((CFTypeRef)data);
    }
    if (key != NULL) {
        CFRelease((CFTypeRef)key);
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
    if (dictionary != NULL) {
        record_type_identity(&result, (CFTypeRef)dictionary, "CFDictionary");
        result.hasLengthValue = true;
        result.lengthValue = (long)CFDictionaryGetCount(dictionary);
        result.hasPrimaryValueText = true;
        result.primaryValueText[0] = '\0';
        result.hasAlternateValueText = true;
        result.alternateValueText[0] = '\0';
        result.success = CFDictionaryGetCount(dictionary) == 0;
        CFRelease((CFTypeRef)dictionary);
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
    const void *keys[] = {key};
    const void *values[] = {value};
    const void *presentValue = NULL;
    const void *missingValue = (const void *)0x1;
    CFIndex keyBase = (key != NULL) ? CFGetRetainCount((CFTypeRef)key) : -1;
    CFIndex valueBase = (value != NULL) ? CFGetRetainCount((CFTypeRef)value) : -1;
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

    if (dictionary != NULL) {
        record_type_identity(&result, (CFTypeRef)dictionary, "CFDictionary");
        result.hasPointerIdentity = true;
        result.pointerIdentitySame =
            CFDictionaryGetValue(dictionary, key) == (const void *)value &&
            CFDictionaryGetValueIfPresent(dictionary, key, &presentValue) &&
            presentValue == (const void *)value &&
            !CFDictionaryGetValueIfPresent(dictionary, missingKey, &missingValue) &&
            missingValue == NULL;
        result.hasLengthValue = true;
        result.lengthValue = (long)CFDictionaryGetCount(dictionary);
        result.hasPrimaryValueText = true;
        snprintf(result.primaryValueText, sizeof(result.primaryValueText), "%s", "636166c3a92de29883");
        result.hasAlternateValueText = true;
        set_hex_bytes(result.alternateValueText, sizeof(result.alternateValueText), bytes, sizeof(bytes));
        result.success =
            result.pointerIdentitySame &&
            result.lengthValue == 1 &&
            CFGetRetainCount((CFTypeRef)key) == keyBase + 1 &&
            CFGetRetainCount((CFTypeRef)value) == valueBase + 1;
        CFRelease((CFTypeRef)dictionary);
        result.success =
            result.success &&
            CFGetRetainCount((CFTypeRef)key) == keyBase &&
            CFGetRetainCount((CFTypeRef)value) == valueBase;
    }

    if (value != NULL) {
        CFRelease((CFTypeRef)value);
    }
    if (missingKey != NULL) {
        CFRelease((CFTypeRef)missingKey);
    }
    if (key != NULL) {
        CFRelease((CFTypeRef)key);
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
    CFIndex keyBase = (key != NULL) ? CFGetRetainCount((CFTypeRef)key) : -1;
    CFIndex value1Base = (value1 != NULL) ? CFGetRetainCount((CFTypeRef)value1) : -1;
    CFIndex value2Base = (value2 != NULL) ? CFGetRetainCount((CFTypeRef)value2) : -1;

    if (dictionary != NULL) {
        record_type_identity(&result, (CFTypeRef)dictionary, "CFDictionary");
        CFDictionarySetValue(dictionary, key, value1);
        result.hasPointerIdentity = true;
        result.pointerIdentitySame = CFDictionaryGetValue(dictionary, key) == (const void *)value1;
        CFDictionarySetValue(dictionary, key, value2);
        result.pointerIdentitySame = result.pointerIdentitySame && (CFDictionaryGetValue(dictionary, key) == (const void *)value2);
        CFDictionaryRemoveValue(dictionary, key);
        result.hasLengthValue = true;
        result.lengthValue = (long)CFDictionaryGetCount(dictionary);
        result.hasPrimaryValueText = true;
        snprintf(result.primaryValueText, sizeof(result.primaryValueText), "%s", "73756273657432612d6b6579");
        result.hasAlternateValueText = true;
        snprintf(result.alternateValueText, sizeof(result.alternateValueText), "%s", "set-retain replace-release remove-release dealloc-release");
        result.success =
            result.pointerIdentitySame &&
            CFGetRetainCount((CFTypeRef)key) == keyBase &&
            CFGetRetainCount((CFTypeRef)value1) == value1Base &&
            CFGetRetainCount((CFTypeRef)value2) == value2Base &&
            result.lengthValue == 0;
        CFDictionarySetValue(dictionary, key, value1);
        result.success =
            result.success &&
            CFGetRetainCount((CFTypeRef)key) == keyBase + 1 &&
            CFGetRetainCount((CFTypeRef)value1) == value1Base + 1;
        CFRelease((CFTypeRef)dictionary);
        result.success =
            result.success &&
            CFGetRetainCount((CFTypeRef)key) == keyBase &&
            CFGetRetainCount((CFTypeRef)value1) == value1Base;
    }

    if (value2 != NULL) {
        CFRelease((CFTypeRef)value2);
    }
    if (value1 != NULL) {
        CFRelease((CFTypeRef)value1);
    }
    if (key != NULL) {
        CFRelease((CFTypeRef)key);
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
        CFRelease((CFTypeRef)original);
    }

    if (copy != NULL) {
        record_type_identity(&result, (CFTypeRef)copy, "CFDictionary");
        result.hasPointerIdentity = true;
        result.pointerIdentitySame = CFDictionaryGetValue(copy, key) == (const void *)value;
        result.hasLengthValue = true;
        result.lengthValue = (long)CFDictionaryGetCount(copy);
        result.hasPrimaryValueText = true;
        snprintf(result.primaryValueText, sizeof(result.primaryValueText), "%s", "636f70792d646963742d6b6579");
        result.hasAlternateValueText = true;
        set_hex_bytes(result.alternateValueText, sizeof(result.alternateValueText), bytes, sizeof(bytes));
        result.success = result.pointerIdentitySame && result.lengthValue == 1;
        CFRelease((CFTypeRef)copy);
    }

    if (value != NULL) {
        CFRelease((CFTypeRef)value);
    }
    if (key != NULL) {
        CFRelease((CFTypeRef)key);
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
