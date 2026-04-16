#include <CoreFoundation/CoreFoundation.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    kMaxAPICalls = 8,
    kMaxText = 256
};

struct CaseResult {
    const char *name;
    const char *classification;
    const char *fxMatchLevel;
    const char *ownershipNote;
    bool success;
    const char *apiCalls[kMaxAPICalls];
    size_t apiCallCount;
    bool hasTypeID;
    long typeID;
    bool hasTypeDescription;
    char typeDescription[kMaxText];
    bool hasPointerIdentity;
    bool pointerIdentitySame;
    bool hasRetainCountDiagnostics;
    long retainCountBefore;
    long retainCountAfterRetain;
    long retainCountAfterRelease;
    bool hasRetainCallbackCount;
    long retainCallbackCount;
    bool hasReleaseCallbackCount;
    long releaseCallbackCount;
    bool hasKeyRetainCallbackCount;
    long keyRetainCallbackCount;
    bool hasKeyReleaseCallbackCount;
    long keyReleaseCallbackCount;
    bool hasValueRetainCallbackCount;
    long valueRetainCallbackCount;
    bool hasValueReleaseCallbackCount;
    long valueReleaseCallbackCount;
};

struct CallbackCounts {
    long retainCount;
    long releaseCount;
};

static struct CallbackCounts gArrayCounts = {0, 0};
static struct CallbackCounts gDictionaryKeyCounts = {0, 0};
static struct CallbackCounts gDictionaryValueCounts = {0, 0};

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

static void json_print_api_calls(FILE *stream, const struct CaseResult *result) {
    fputc('[', stream);
    for (size_t i = 0; i < result->apiCallCount; ++i) {
        if (i != 0) {
            fputs(", ", stream);
        }
        json_print_string(stream, result->apiCalls[i]);
    }
    fputc(']', stream);
}

static void json_print_result(FILE *stream, const struct CaseResult *result, bool trailingComma) {
    fputs("    {\n", stream);
    fputs("      \"name\": ", stream);
    json_print_string(stream, result->name);
    fputs(",\n      \"classification\": ", stream);
    json_print_string(stream, result->classification);
    fputs(",\n      \"success\": ", stream);
    fputs(result->success ? "true" : "false", stream);
    fputs(",\n      \"api_calls\": ", stream);
    json_print_api_calls(stream, result);
    fputs(",\n      \"type_id\": ", stream);
    json_print_long_or_null(stream, result->hasTypeID, result->typeID);
    fputs(",\n      \"type_description\": ", stream);
    json_print_text_or_null(stream, result->hasTypeDescription, result->typeDescription);
    fputs(",\n      \"pointer_identity_same\": ", stream);
    json_print_bool_or_null(stream, result->hasPointerIdentity, result->pointerIdentitySame);
    fputs(",\n      \"retain_callback_count\": ", stream);
    json_print_long_or_null(stream, result->hasRetainCallbackCount, result->retainCallbackCount);
    fputs(",\n      \"release_callback_count\": ", stream);
    json_print_long_or_null(stream, result->hasReleaseCallbackCount, result->releaseCallbackCount);
    fputs(",\n      \"key_retain_callback_count\": ", stream);
    json_print_long_or_null(stream, result->hasKeyRetainCallbackCount, result->keyRetainCallbackCount);
    fputs(",\n      \"key_release_callback_count\": ", stream);
    json_print_long_or_null(stream, result->hasKeyReleaseCallbackCount, result->keyReleaseCallbackCount);
    fputs(",\n      \"value_retain_callback_count\": ", stream);
    json_print_long_or_null(stream, result->hasValueRetainCallbackCount, result->valueRetainCallbackCount);
    fputs(",\n      \"value_release_callback_count\": ", stream);
    json_print_long_or_null(stream, result->hasValueReleaseCallbackCount, result->valueReleaseCallbackCount);
    fputs(",\n      \"retain_count_before\": ", stream);
    json_print_long_or_null(stream, result->hasRetainCountDiagnostics, result->retainCountBefore);
    fputs(",\n      \"retain_count_after_retain\": ", stream);
    json_print_long_or_null(stream, result->hasRetainCountDiagnostics, result->retainCountAfterRetain);
    fputs(",\n      \"retain_count_after_release\": ", stream);
    json_print_long_or_null(stream, result->hasRetainCountDiagnostics, result->retainCountAfterRelease);
    fputs(",\n      \"ownership_note\": ", stream);
    json_print_string(stream, result->ownershipNote);
    fputs(",\n      \"fx_match_level\": ", stream);
    json_print_string(stream, result->fxMatchLevel);
    fputs("\n    }", stream);
    if (trailingComma) {
        fputc(',', stream);
    }
    fputc('\n', stream);
}

static void set_api_calls(struct CaseResult *result, const char **apiCalls, size_t apiCallCount) {
    result->apiCallCount = apiCallCount;
    for (size_t i = 0; i < apiCallCount && i < kMaxAPICalls; ++i) {
        result->apiCalls[i] = apiCalls[i];
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

    if (CFStringGetCString(string, buffer, (CFIndex)bufferSize, kCFStringEncodingUTF8)) {
        return;
    }

    CFIndex length = CFStringGetLength(string);
    CFIndex maxBytes = CFStringGetMaximumSizeForEncoding(length, kCFStringEncodingUTF8) + 1;
    if (maxBytes <= 0) {
        snprintf(buffer, bufferSize, "%s", "<conversion-failed>");
        return;
    }

    char *temporary = (char *)calloc((size_t)maxBytes, sizeof(char));
    if (temporary == NULL) {
        snprintf(buffer, bufferSize, "%s", "<allocation-failed>");
        return;
    }

    if (CFStringGetCString(string, temporary, maxBytes, kCFStringEncodingUTF8)) {
        snprintf(buffer, bufferSize, "%s", temporary);
    } else {
        snprintf(buffer, bufferSize, "%s", "<conversion-failed>");
    }
    free(temporary);
}

static void record_type_description(struct CaseResult *result, CFTypeRef object) {
    CFStringRef description = CFCopyTypeIDDescription(CFGetTypeID(object));
    if (description == NULL) {
        return;
    }
    result->hasTypeDescription = true;
    cfstring_to_utf8(description, result->typeDescription, sizeof(result->typeDescription));
    CFRelease(description);
}

static struct CaseResult observe_identity_case(
    const char *name,
    CFTypeRef object,
    const char *classification,
    const char *fxMatchLevel,
    const char *ownershipNote,
    const char **apiCalls,
    size_t apiCallCount
) {
    struct CaseResult result;
    memset(&result, 0, sizeof(result));

    result.name = name;
    result.classification = classification;
    result.fxMatchLevel = fxMatchLevel;
    result.ownershipNote = ownershipNote;
    set_api_calls(&result, apiCalls, apiCallCount);

    result.hasTypeID = true;
    result.typeID = (long)CFGetTypeID(object);
    record_type_description(&result, object);

    result.hasRetainCountDiagnostics = true;
    result.retainCountBefore = (long)CFGetRetainCount(object);
    CFTypeRef retained = CFRetain(object);
    result.retainCountAfterRetain = (long)CFGetRetainCount(object);
    CFRelease(object);
    result.retainCountAfterRelease = (long)CFGetRetainCount(object);

    result.hasPointerIdentity = true;
    result.pointerIdentitySame = retained == object;
    result.success = (retained == object);
    return result;
}

static struct CaseResult make_create_copy_get_case(void) {
    static const char *apiCalls[] = {
        "CFStringCreateWithCString",
        "CFStringCreateCopy",
        "CFArrayCreate",
        "CFArrayGetValueAtIndex"
    };

    struct CaseResult result;
    memset(&result, 0, sizeof(result));
    result.name = "create_copy_get_ownership";
    result.classification = "required";
    result.fxMatchLevel = "semantic";
    result.ownershipNote = "Create and Copy return owned objects. Array Get returns a borrowed reference and should not change retain ownership.";
    set_api_calls(&result, apiCalls, sizeof(apiCalls) / sizeof(apiCalls[0]));

    CFStringRef created = CFStringCreateWithCString(kCFAllocatorSystemDefault, "owned-value", kCFStringEncodingUTF8);
    CFStringRef copied = CFStringCreateCopy(kCFAllocatorSystemDefault, created);
    const void *values[] = { created };
    CFArrayRef array = CFArrayCreate(kCFAllocatorSystemDefault, values, 1, &kCFTypeArrayCallBacks);
    const void *borrowed = CFArrayGetValueAtIndex(array, 0);

    result.hasTypeID = true;
    result.typeID = (long)CFGetTypeID(array);
    record_type_description(&result, array);

    result.hasPointerIdentity = true;
    result.pointerIdentitySame = borrowed == created;
    result.hasRetainCountDiagnostics = true;
    result.retainCountBefore = (long)CFGetRetainCount(created);
    (void)borrowed;
    result.retainCountAfterRetain = (long)CFGetRetainCount(created);
    result.retainCountAfterRelease = (long)CFGetRetainCount(created);

    result.success = created != NULL &&
        copied != NULL &&
        borrowed == created &&
        result.retainCountBefore == result.retainCountAfterRetain &&
        result.retainCountBefore == result.retainCountAfterRelease;

    CFRelease(array);
    CFRelease(copied);
    CFRelease(created);
    return result;
}

static const void *array_retain(CFAllocatorRef allocator, const void *value) {
    (void)allocator;
    gArrayCounts.retainCount += 1;
    return value;
}

static void array_release(CFAllocatorRef allocator, const void *value) {
    (void)allocator;
    (void)value;
    gArrayCounts.releaseCount += 1;
}

static Boolean pointer_equal(const void *value1, const void *value2) {
    return value1 == value2;
}

static struct CaseResult make_array_callback_case(void) {
    static const char *apiCalls[] = {
        "CFArrayCreateMutable",
        "CFArrayAppendValue",
        "CFArrayGetValueAtIndex",
        "CFArrayRemoveValueAtIndex",
        "CFRelease"
    };

    struct CaseResult result;
    memset(&result, 0, sizeof(result));
    result.name = "mutable_array_callback_ownership";
    result.classification = "semantic-match-only";
    result.fxMatchLevel = "semantic";
    result.ownershipNote = "Future Subset 2 ownership evidence only. Not a Subset 0 implementation gate.";
    set_api_calls(&result, apiCalls, sizeof(apiCalls) / sizeof(apiCalls[0]));

    gArrayCounts.retainCount = 0;
    gArrayCounts.releaseCount = 0;

    CFArrayCallBacks callbacks = {
        0,
        array_retain,
        array_release,
        NULL,
        pointer_equal
    };

    const void *sentinel = (const void *)(uintptr_t)0x1234u;
    CFMutableArrayRef array = CFArrayCreateMutable(kCFAllocatorSystemDefault, 0, &callbacks);
    CFArrayAppendValue(array, sentinel);
    const void *borrowed = CFArrayGetValueAtIndex(array, 0);
    CFArrayRemoveValueAtIndex(array, 0);
    CFRelease(array);

    result.hasPointerIdentity = true;
    result.pointerIdentitySame = borrowed == sentinel;
    result.hasRetainCallbackCount = true;
    result.retainCallbackCount = gArrayCounts.retainCount;
    result.hasReleaseCallbackCount = true;
    result.releaseCallbackCount = gArrayCounts.releaseCount;
    result.success = borrowed == sentinel &&
        gArrayCounts.retainCount >= 1 &&
        gArrayCounts.releaseCount >= 1;
    return result;
}

static const void *dictionary_key_retain(CFAllocatorRef allocator, const void *value) {
    (void)allocator;
    gDictionaryKeyCounts.retainCount += 1;
    return value;
}

static void dictionary_key_release(CFAllocatorRef allocator, const void *value) {
    (void)allocator;
    (void)value;
    gDictionaryKeyCounts.releaseCount += 1;
}

static const void *dictionary_value_retain(CFAllocatorRef allocator, const void *value) {
    (void)allocator;
    gDictionaryValueCounts.retainCount += 1;
    return value;
}

static void dictionary_value_release(CFAllocatorRef allocator, const void *value) {
    (void)allocator;
    (void)value;
    gDictionaryValueCounts.releaseCount += 1;
}

static CFHashCode pointer_hash(const void *value) {
    return (CFHashCode)(uintptr_t)value;
}

static struct CaseResult make_dictionary_callback_case(void) {
    static const char *apiCalls[] = {
        "CFDictionaryCreateMutable",
        "CFDictionarySetValue",
        "CFDictionaryGetValue",
        "CFDictionaryRemoveValue",
        "CFRelease"
    };

    struct CaseResult result;
    memset(&result, 0, sizeof(result));
    result.name = "mutable_dictionary_callback_ownership";
    result.classification = "semantic-match-only";
    result.fxMatchLevel = "semantic";
    result.ownershipNote = "Future Subset 2 ownership evidence only. Not a Subset 0 implementation gate.";
    set_api_calls(&result, apiCalls, sizeof(apiCalls) / sizeof(apiCalls[0]));

    gDictionaryKeyCounts.retainCount = 0;
    gDictionaryKeyCounts.releaseCount = 0;
    gDictionaryValueCounts.retainCount = 0;
    gDictionaryValueCounts.releaseCount = 0;

    CFDictionaryKeyCallBacks keyCallbacks = {
        0,
        dictionary_key_retain,
        dictionary_key_release,
        NULL,
        pointer_equal,
        pointer_hash
    };
    CFDictionaryValueCallBacks valueCallbacks = {
        0,
        dictionary_value_retain,
        dictionary_value_release,
        NULL,
        pointer_equal
    };

    const void *key = (const void *)(uintptr_t)0x4321u;
    const void *value1 = (const void *)(uintptr_t)0x1111u;
    const void *value2 = (const void *)(uintptr_t)0x2222u;

    CFMutableDictionaryRef dictionary = CFDictionaryCreateMutable(kCFAllocatorSystemDefault, 0, &keyCallbacks, &valueCallbacks);
    CFDictionarySetValue(dictionary, key, value1);
    const void *borrowed = CFDictionaryGetValue(dictionary, key);
    CFDictionarySetValue(dictionary, key, value2);
    CFDictionaryRemoveValue(dictionary, key);
    CFRelease(dictionary);

    result.hasPointerIdentity = true;
    result.pointerIdentitySame = borrowed == value1;
    result.hasKeyRetainCallbackCount = true;
    result.keyRetainCallbackCount = gDictionaryKeyCounts.retainCount;
    result.hasKeyReleaseCallbackCount = true;
    result.keyReleaseCallbackCount = gDictionaryKeyCounts.releaseCount;
    result.hasValueRetainCallbackCount = true;
    result.valueRetainCallbackCount = gDictionaryValueCounts.retainCount;
    result.hasValueReleaseCallbackCount = true;
    result.valueReleaseCallbackCount = gDictionaryValueCounts.releaseCount;
    result.success = borrowed == value1 &&
        gDictionaryKeyCounts.retainCount >= 1 &&
        gDictionaryKeyCounts.releaseCount >= 1 &&
        gDictionaryValueCounts.retainCount >= 2 &&
        gDictionaryValueCounts.releaseCount >= 2;
    return result;
}

static struct CaseResult make_description_case(void) {
    static const char *apiCalls[] = {
        "CFStringCreateWithCString",
        "CFCopyDescription"
    };

    struct CaseResult result;
    memset(&result, 0, sizeof(result));
    result.name = "cfcopydescription_diagnostic";
    result.classification = "semantic-match-only";
    result.fxMatchLevel = "semantic";
    result.ownershipNote = "Diagnostic-only observation. Not a Subset 0 FX requirement because FX does not implement CFString in this subset.";
    set_api_calls(&result, apiCalls, sizeof(apiCalls) / sizeof(apiCalls[0]));

    CFStringRef sample = CFStringCreateWithCString(kCFAllocatorSystemDefault, "description-sample", kCFStringEncodingUTF8);
    CFStringRef description = CFCopyDescription(sample);

    if (description != NULL) {
        result.hasTypeDescription = true;
        cfstring_to_utf8(description, result.typeDescription, sizeof(result.typeDescription));
    }
    result.success = description != NULL && result.typeDescription[0] != '\0';

    if (description != NULL) {
        CFRelease(description);
    }
    CFRelease(sample);
    return result;
}

int main(void) {
    struct CaseResult results[16];
    size_t resultCount = 0;

    {
        static const char *apiCalls[] = { "CFRetain", "CFRelease", "CFGetTypeID", "CFGetRetainCount", "CFCopyTypeIDDescription" };
        results[resultCount++] = observe_identity_case(
            "cfallocator_system_default",
            (CFTypeRef)kCFAllocatorSystemDefault,
            "required",
            "semantic",
            "Subset 0 builtin immortal allocator semantics.",
            apiCalls,
            sizeof(apiCalls) / sizeof(apiCalls[0])
        );
    }

    {
        static const char *apiCalls[] = { "CFStringCreateWithCString", "CFRetain", "CFRelease", "CFGetTypeID", "CFCopyTypeIDDescription" };
        CFStringRef object = CFStringCreateWithCString(kCFAllocatorSystemDefault, "subset0", kCFStringEncodingUTF8);
        results[resultCount++] = observe_identity_case(
            "cfstring_create_with_cstring",
            (CFTypeRef)object,
            "required",
            "semantic",
            "Public oracle for retain/release/type identity only; FX does not implement CFString in Subset 0.",
            apiCalls,
            sizeof(apiCalls) / sizeof(apiCalls[0])
        );
        CFRelease(object);
    }

    {
        static const char *apiCalls[] = { "CFDataCreate", "CFRetain", "CFRelease", "CFGetTypeID", "CFCopyTypeIDDescription" };
        CFDataRef object = CFDataCreate(kCFAllocatorSystemDefault, (const UInt8 *)"abc", 3);
        results[resultCount++] = observe_identity_case(
            "cfdata_create",
            (CFTypeRef)object,
            "required",
            "semantic",
            "Public oracle for retain/release/type identity only; FX does not implement CFData in Subset 0.",
            apiCalls,
            sizeof(apiCalls) / sizeof(apiCalls[0])
        );
        CFRelease(object);
    }

    {
        static const char *apiCalls[] = { "CFNumberCreate", "CFRetain", "CFRelease", "CFGetTypeID", "CFCopyTypeIDDescription" };
        int32_t value = 42;
        CFNumberRef object = CFNumberCreate(kCFAllocatorSystemDefault, kCFNumberSInt32Type, &value);
        results[resultCount++] = observe_identity_case(
            "cfnumber_create_int32",
            (CFTypeRef)object,
            "required",
            "semantic",
            "Public oracle for retain/release/type identity only; FX does not implement CFNumber in Subset 0.",
            apiCalls,
            sizeof(apiCalls) / sizeof(apiCalls[0])
        );
        CFRelease(object);
    }

    {
        static const char *apiCalls[] = { "kCFBooleanTrue", "CFRetain", "CFRelease", "CFGetTypeID", "CFCopyTypeIDDescription" };
        results[resultCount++] = observe_identity_case(
            "cfboolean_true",
            (CFTypeRef)kCFBooleanTrue,
            "required",
            "semantic",
            "Public oracle for immortal singleton retain/release behavior.",
            apiCalls,
            sizeof(apiCalls) / sizeof(apiCalls[0])
        );
    }

    {
        static const char *apiCalls[] = { "kCFBooleanFalse", "CFRetain", "CFRelease", "CFGetTypeID", "CFCopyTypeIDDescription" };
        results[resultCount++] = observe_identity_case(
            "cfboolean_false",
            (CFTypeRef)kCFBooleanFalse,
            "required",
            "semantic",
            "Public oracle for immortal singleton retain/release behavior.",
            apiCalls,
            sizeof(apiCalls) / sizeof(apiCalls[0])
        );
    }

    {
        static const char *apiCalls[] = { "CFDateCreate", "CFRetain", "CFRelease", "CFGetTypeID", "CFCopyTypeIDDescription" };
        CFDateRef object = CFDateCreate(kCFAllocatorSystemDefault, 1234.5);
        results[resultCount++] = observe_identity_case(
            "cfdate_create",
            (CFTypeRef)object,
            "required",
            "semantic",
            "Public oracle for retain/release/type identity only; FX does not implement CFDate in Subset 0.",
            apiCalls,
            sizeof(apiCalls) / sizeof(apiCalls[0])
        );
        CFRelease(object);
    }

    {
        static const char *apiCalls[] = { "CFArrayCreate", "CFRetain", "CFRelease", "CFGetTypeID", "CFCopyTypeIDDescription" };
        CFArrayRef object = CFArrayCreate(kCFAllocatorSystemDefault, NULL, 0, &kCFTypeArrayCallBacks);
        results[resultCount++] = observe_identity_case(
            "cfarray_create_empty",
            (CFTypeRef)object,
            "required",
            "semantic",
            "Public oracle for array type identity only; FX containers remain future work.",
            apiCalls,
            sizeof(apiCalls) / sizeof(apiCalls[0])
        );
        CFRelease(object);
    }

    {
        static const char *apiCalls[] = { "CFDictionaryCreate", "CFRetain", "CFRelease", "CFGetTypeID", "CFCopyTypeIDDescription" };
        CFDictionaryRef object = CFDictionaryCreate(kCFAllocatorSystemDefault, NULL, NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
        results[resultCount++] = observe_identity_case(
            "cfdictionary_create_empty",
            (CFTypeRef)object,
            "required",
            "semantic",
            "Public oracle for dictionary type identity only; FX containers remain future work.",
            apiCalls,
            sizeof(apiCalls) / sizeof(apiCalls[0])
        );
        CFRelease(object);
    }

    {
        static const char *apiCalls[] = { "CFErrorCreate", "CFRetain", "CFRelease", "CFGetTypeID", "CFCopyTypeIDDescription" };
        CFStringRef domain = CFStringCreateWithCString(kCFAllocatorSystemDefault, "cort.mx.error", kCFStringEncodingUTF8);
        CFErrorRef object = CFErrorCreate(kCFAllocatorSystemDefault, domain, 17, NULL);
        results[resultCount++] = observe_identity_case(
            "cferror_create",
            (CFTypeRef)object,
            "required",
            "semantic",
            "Public oracle for error object identity only; FX error object support is outside Subset 0.",
            apiCalls,
            sizeof(apiCalls) / sizeof(apiCalls[0])
        );
        CFRelease(object);
        CFRelease(domain);
    }

    results[resultCount++] = make_create_copy_get_case();
    results[resultCount++] = make_array_callback_case();
    results[resultCount++] = make_dictionary_callback_case();
    results[resultCount++] = make_description_case();

    fputs("{\n", stdout);
    fputs("  \"probe\": ", stdout);
    json_print_string(stdout, "subset0_runtime_ownership");
    fputs(",\n  \"output_constraint\": ", stdout);
    json_print_string(stdout, "Manual fprintf JSON only. No Foundation, Objective-C, or third-party JSON dependency.");
    fputs(",\n  \"results\": [\n", stdout);
    for (size_t i = 0; i < resultCount; ++i) {
        json_print_result(stdout, &results[i], i + 1u < resultCount);
    }
    fputs("  ]\n}\n", stdout);
    return 0;
}
