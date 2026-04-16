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
    bool hasEqualSameValue;
    bool equalSameValue;
    bool hasEqualDifferentValue;
    bool equalDifferentValue;
    bool hasHashPrimary;
    long hashPrimary;
    bool hasHashSameValue;
    long hashSameValue;
    bool hasHashDifferentValue;
    long hashDifferentValue;
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
    fputs(",\n      \"equal_same_value\": ", stream);
    json_print_bool_or_null(stream, result->hasEqualSameValue, result->equalSameValue);
    fputs(",\n      \"equal_different_value\": ", stream);
    json_print_bool_or_null(stream, result->hasEqualDifferentValue, result->equalDifferentValue);
    fputs(",\n      \"hash_primary\": ", stream);
    json_print_long_or_null(stream, result->hasHashPrimary, result->hashPrimary);
    fputs(",\n      \"hash_same_value\": ", stream);
    json_print_long_or_null(stream, result->hasHashSameValue, result->hashSameValue);
    fputs(",\n      \"hash_different_value\": ", stream);
    json_print_long_or_null(stream, result->hasHashDifferentValue, result->hashDifferentValue);
    fputs(",\n      \"length_value\": ", stream);
    json_print_long_or_null(stream, result->hasLengthValue, result->lengthValue);
    fputs(",\n      \"primary_value_text\": ", stream);
    json_print_text_or_null(stream, result->hasPrimaryValueText, result->primaryValueText);
    fputs(",\n      \"alternate_value_text\": ", stream);
    json_print_text_or_null(stream, result->hasAlternateValueText, result->alternateValueText);
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

static void record_type_identity(struct CaseResult *result, CFTypeRef object) {
    result->hasTypeID = true;
    result->typeID = (long)CFGetTypeID(object);
}

static struct CaseResult observe_boolean_singleton(const char *name, CFBooleanRef object, bool expectedValue) {
    static const char *apiCalls[] = {
        "kCFBooleanTrue/kCFBooleanFalse",
        "CFRetain",
        "CFRelease",
        "CFBooleanGetValue",
        "CFEqual",
        "CFHash"
    };

    struct CaseResult result;
    memset(&result, 0, sizeof(result));
    result.name = name;
    result.classification = "required";
    result.fxMatchLevel = "semantic";
    result.ownershipNote = "Subset 1A requires public singleton identity and boolean value semantics. Retain-count numerics remain diagnostic only.";
    set_api_calls(&result, apiCalls, sizeof(apiCalls) / sizeof(apiCalls[0]));

    CFRetain((CFTypeRef)object);
    bool value = CFBooleanGetValue(object);
    CFRelease((CFTypeRef)object);

    record_type_identity(&result, (CFTypeRef)object);
    result.hasPointerIdentity = true;
    result.pointerIdentitySame = true;
    result.hasPrimaryValueText = true;
    snprintf(result.primaryValueText, sizeof(result.primaryValueText), "%s", value ? "true" : "false");
    result.hasEqualSameValue = true;
    result.equalSameValue = CFEqual((CFTypeRef)object, (CFTypeRef)object);
    result.hasHashPrimary = true;
    result.hashPrimary = (long)CFHash((CFTypeRef)object);
    result.hasHashSameValue = true;
    result.hashSameValue = (long)CFHash((CFTypeRef)object);
    result.success = (value == expectedValue) && result.equalSameValue;
    return result;
}

static struct CaseResult observe_boolean_equality_hash(void) {
    static const char *apiCalls[] = {
        "kCFBooleanTrue",
        "kCFBooleanFalse",
        "CFEqual",
        "CFHash"
    };

    struct CaseResult result;
    memset(&result, 0, sizeof(result));
    result.name = "cfboolean_equality_hash";
    result.classification = "required";
    result.fxMatchLevel = "semantic";
    result.ownershipNote = "Required boolean equality/hash coherence. Raw hash numerics are diagnostic across implementations.";
    set_api_calls(&result, apiCalls, sizeof(apiCalls) / sizeof(apiCalls[0]));

    record_type_identity(&result, (CFTypeRef)kCFBooleanTrue);
    result.hasEqualSameValue = true;
    result.equalSameValue = CFEqual((CFTypeRef)kCFBooleanTrue, (CFTypeRef)kCFBooleanTrue);
    result.hasEqualDifferentValue = true;
    result.equalDifferentValue = CFEqual((CFTypeRef)kCFBooleanTrue, (CFTypeRef)kCFBooleanFalse);
    result.hasHashPrimary = true;
    result.hashPrimary = (long)CFHash((CFTypeRef)kCFBooleanTrue);
    result.hasHashSameValue = true;
    result.hashSameValue = (long)CFHash((CFTypeRef)kCFBooleanTrue);
    result.hasHashDifferentValue = true;
    result.hashDifferentValue = (long)CFHash((CFTypeRef)kCFBooleanFalse);
    result.success =
        result.equalSameValue &&
        !result.equalDifferentValue &&
        (result.hashPrimary == result.hashSameValue);
    return result;
}

static struct CaseResult observe_data_roundtrip(void) {
    static const char *apiCalls[] = {
        "CFDataCreate",
        "CFDataCreateCopy",
        "CFDataGetLength",
        "CFDataGetBytePtr",
        "CFEqual",
        "CFHash"
    };
    static const UInt8 bytes[] = {0x00u, 0x41u, 0x7fu, 0xffu};
    static const UInt8 otherBytes[] = {0x00u, 0x41u, 0x7fu, 0x00u};

    struct CaseResult result;
    memset(&result, 0, sizeof(result));
    result.name = "cfdata_value_roundtrip";
    result.classification = "required";
    result.fxMatchLevel = "semantic";
    result.ownershipNote = "Subset 1A requires immutable byte storage semantics. Raw hash numerics are diagnostic; equal data must hash coherently within one implementation.";
    set_api_calls(&result, apiCalls, sizeof(apiCalls) / sizeof(apiCalls[0]));

    CFDataRef data = CFDataCreate(kCFAllocatorSystemDefault, bytes, (CFIndex)sizeof(bytes));
    CFDataRef copy = CFDataCreateCopy(kCFAllocatorSystemDefault, data);
    CFDataRef different = CFDataCreate(kCFAllocatorSystemDefault, otherBytes, (CFIndex)sizeof(otherBytes));
    const UInt8 *dataBytes = data != NULL ? CFDataGetBytePtr(data) : NULL;

    record_type_identity(&result, (CFTypeRef)data);
    result.hasLengthValue = true;
    result.lengthValue = (long)CFDataGetLength(data);
    result.hasPrimaryValueText = true;
    snprintf(result.primaryValueText, sizeof(result.primaryValueText), "%02x%02x%02x%02x", bytes[0], bytes[1], bytes[2], bytes[3]);
    result.hasEqualSameValue = true;
    result.equalSameValue = CFEqual((CFTypeRef)data, (CFTypeRef)copy);
    result.hasEqualDifferentValue = true;
    result.equalDifferentValue = CFEqual((CFTypeRef)data, (CFTypeRef)different);
    result.hasHashPrimary = true;
    result.hashPrimary = (long)CFHash((CFTypeRef)data);
    result.hasHashSameValue = true;
    result.hashSameValue = (long)CFHash((CFTypeRef)copy);
    result.hasHashDifferentValue = true;
    result.hashDifferentValue = (long)CFHash((CFTypeRef)different);
    result.success =
        (data != NULL) &&
        (copy != NULL) &&
        (different != NULL) &&
        (dataBytes != NULL) &&
        (result.lengthValue == (long)sizeof(bytes)) &&
        (memcmp(dataBytes, bytes, sizeof(bytes)) == 0) &&
        result.equalSameValue &&
        !result.equalDifferentValue &&
        (result.hashPrimary == result.hashSameValue);

    CFRelease((CFTypeRef)different);
    CFRelease((CFTypeRef)copy);
    CFRelease((CFTypeRef)data);
    return result;
}

static struct CaseResult observe_number_sint32_roundtrip(void) {
    static const char *apiCalls[] = {
        "CFNumberCreate",
        "CFNumberGetValue",
        "CFNumberGetType",
        "CFEqual",
        "CFHash"
    };
    SInt32 value = 42;
    SInt32 sameValue = 42;
    SInt32 differentValue = 7;

    struct CaseResult result;
    memset(&result, 0, sizeof(result));
    result.name = "cfnumber_sint32_roundtrip";
    result.classification = "required";
    result.fxMatchLevel = "semantic";
    result.ownershipNote = "Subset 1A requires numeric value roundtrip for tested scalar types. Exact hash numerics remain diagnostic across implementations.";
    set_api_calls(&result, apiCalls, sizeof(apiCalls) / sizeof(apiCalls[0]));

    CFNumberRef primary = CFNumberCreate(kCFAllocatorSystemDefault, kCFNumberSInt32Type, &value);
    CFNumberRef same = CFNumberCreate(kCFAllocatorSystemDefault, kCFNumberSInt32Type, &sameValue);
    CFNumberRef different = CFNumberCreate(kCFAllocatorSystemDefault, kCFNumberSInt32Type, &differentValue);
    SInt32 roundtrip = 0;

    record_type_identity(&result, (CFTypeRef)primary);
    CFNumberGetValue(primary, kCFNumberSInt32Type, &roundtrip);
    result.hasPrimaryValueText = true;
    snprintf(result.primaryValueText, sizeof(result.primaryValueText), "%d", (int)roundtrip);
    result.hasAlternateValueText = true;
    snprintf(result.alternateValueText, sizeof(result.alternateValueText), "%ld", (long)CFNumberGetType(primary));
    result.hasEqualSameValue = true;
    result.equalSameValue = CFEqual((CFTypeRef)primary, (CFTypeRef)same);
    result.hasEqualDifferentValue = true;
    result.equalDifferentValue = CFEqual((CFTypeRef)primary, (CFTypeRef)different);
    result.hasHashPrimary = true;
    result.hashPrimary = (long)CFHash((CFTypeRef)primary);
    result.hasHashSameValue = true;
    result.hashSameValue = (long)CFHash((CFTypeRef)same);
    result.hasHashDifferentValue = true;
    result.hashDifferentValue = (long)CFHash((CFTypeRef)different);
    result.success =
        (primary != NULL) &&
        (same != NULL) &&
        (different != NULL) &&
        (roundtrip == value) &&
        result.equalSameValue &&
        !result.equalDifferentValue &&
        (result.hashPrimary == result.hashSameValue);

    CFRelease((CFTypeRef)different);
    CFRelease((CFTypeRef)same);
    CFRelease((CFTypeRef)primary);
    return result;
}

static struct CaseResult observe_number_float64_roundtrip(void) {
    static const char *apiCalls[] = {
        "CFNumberCreate",
        "CFNumberGetValue",
        "CFNumberGetType",
        "CFEqual",
        "CFHash"
    };
    double value = 1234.5;
    double sameValue = 1234.5;
    double differentValue = 1235.5;

    struct CaseResult result;
    memset(&result, 0, sizeof(result));
    result.name = "cfnumber_float64_roundtrip";
    result.classification = "required";
    result.fxMatchLevel = "semantic";
    result.ownershipNote = "Subset 1A requires floating-point value roundtrip for tested scalar types. Exact hash numerics remain diagnostic across implementations.";
    set_api_calls(&result, apiCalls, sizeof(apiCalls) / sizeof(apiCalls[0]));

    CFNumberRef primary = CFNumberCreate(kCFAllocatorSystemDefault, kCFNumberFloat64Type, &value);
    CFNumberRef same = CFNumberCreate(kCFAllocatorSystemDefault, kCFNumberFloat64Type, &sameValue);
    CFNumberRef different = CFNumberCreate(kCFAllocatorSystemDefault, kCFNumberFloat64Type, &differentValue);
    double roundtrip = 0.0;

    record_type_identity(&result, (CFTypeRef)primary);
    CFNumberGetValue(primary, kCFNumberFloat64Type, &roundtrip);
    result.hasPrimaryValueText = true;
    snprintf(result.primaryValueText, sizeof(result.primaryValueText), "%.17g", roundtrip);
    result.hasAlternateValueText = true;
    snprintf(result.alternateValueText, sizeof(result.alternateValueText), "%ld", (long)CFNumberGetType(primary));
    result.hasEqualSameValue = true;
    result.equalSameValue = CFEqual((CFTypeRef)primary, (CFTypeRef)same);
    result.hasEqualDifferentValue = true;
    result.equalDifferentValue = CFEqual((CFTypeRef)primary, (CFTypeRef)different);
    result.hasHashPrimary = true;
    result.hashPrimary = (long)CFHash((CFTypeRef)primary);
    result.hasHashSameValue = true;
    result.hashSameValue = (long)CFHash((CFTypeRef)same);
    result.hasHashDifferentValue = true;
    result.hashDifferentValue = (long)CFHash((CFTypeRef)different);
    result.success =
        (primary != NULL) &&
        (same != NULL) &&
        (different != NULL) &&
        (roundtrip == value) &&
        result.equalSameValue &&
        !result.equalDifferentValue &&
        (result.hashPrimary == result.hashSameValue);

    CFRelease((CFTypeRef)different);
    CFRelease((CFTypeRef)same);
    CFRelease((CFTypeRef)primary);
    return result;
}

static struct CaseResult observe_number_cross_type_equality(void) {
    static const char *apiCalls[] = {
        "CFNumberCreate",
        "CFEqual",
        "CFHash",
        "CFNumberGetValue"
    };
    SInt32 int32Value = 42;
    SInt64 int64Value = 42;
    double floatValue = 42.0;
    SInt64 roundtripInt64 = 0;
    double roundtripFloat = 0.0;

    struct CaseResult result;
    memset(&result, 0, sizeof(result));
    result.name = "cfnumber_cross_type_equality";
    result.classification = "semantic-match-only";
    result.fxMatchLevel = "semantic";
    result.ownershipNote = "Observation only for now. This case records cross-type numeric equality behavior before FX classifies it as required or variance.";
    set_api_calls(&result, apiCalls, sizeof(apiCalls) / sizeof(apiCalls[0]));

    CFNumberRef int32Number = CFNumberCreate(kCFAllocatorSystemDefault, kCFNumberSInt32Type, &int32Value);
    CFNumberRef int64Number = CFNumberCreate(kCFAllocatorSystemDefault, kCFNumberSInt64Type, &int64Value);
    CFNumberRef floatNumber = CFNumberCreate(kCFAllocatorSystemDefault, kCFNumberFloat64Type, &floatValue);

    record_type_identity(&result, (CFTypeRef)int32Number);
    CFNumberGetValue(int64Number, kCFNumberSInt64Type, &roundtripInt64);
    CFNumberGetValue(floatNumber, kCFNumberFloat64Type, &roundtripFloat);
    result.hasPrimaryValueText = true;
    snprintf(result.primaryValueText, sizeof(result.primaryValueText), "%lld", (long long)roundtripInt64);
    result.hasAlternateValueText = true;
    snprintf(result.alternateValueText, sizeof(result.alternateValueText), "%.17g", roundtripFloat);
    result.hasEqualSameValue = true;
    result.equalSameValue = CFEqual((CFTypeRef)int32Number, (CFTypeRef)int64Number);
    result.hasEqualDifferentValue = true;
    result.equalDifferentValue = CFEqual((CFTypeRef)int32Number, (CFTypeRef)floatNumber);
    result.hasHashPrimary = true;
    result.hashPrimary = (long)CFHash((CFTypeRef)int32Number);
    result.hasHashSameValue = true;
    result.hashSameValue = (long)CFHash((CFTypeRef)int64Number);
    result.hasHashDifferentValue = true;
    result.hashDifferentValue = (long)CFHash((CFTypeRef)floatNumber);
    result.success =
        (int32Number != NULL) &&
        (int64Number != NULL) &&
        (floatNumber != NULL);

    CFRelease((CFTypeRef)floatNumber);
    CFRelease((CFTypeRef)int64Number);
    CFRelease((CFTypeRef)int32Number);
    return result;
}

static struct CaseResult observe_date_roundtrip(void) {
    static const char *apiCalls[] = {
        "CFDateCreate",
        "CFDateGetAbsoluteTime",
        "CFEqual",
        "CFHash"
    };
    CFAbsoluteTime value = 1234.5;
    CFAbsoluteTime sameValue = 1234.5;
    CFAbsoluteTime differentValue = 1235.5;

    struct CaseResult result;
    memset(&result, 0, sizeof(result));
    result.name = "cfdate_absolute_time_roundtrip";
    result.classification = "required";
    result.fxMatchLevel = "semantic";
    result.ownershipNote = "Subset 1A requires immutable date absolute-time semantics. Raw hash numerics are diagnostic across implementations.";
    set_api_calls(&result, apiCalls, sizeof(apiCalls) / sizeof(apiCalls[0]));

    CFDateRef primary = CFDateCreate(kCFAllocatorSystemDefault, value);
    CFDateRef same = CFDateCreate(kCFAllocatorSystemDefault, sameValue);
    CFDateRef different = CFDateCreate(kCFAllocatorSystemDefault, differentValue);
    CFAbsoluteTime roundtrip = 0.0;

    record_type_identity(&result, (CFTypeRef)primary);
    roundtrip = CFDateGetAbsoluteTime(primary);
    result.hasPrimaryValueText = true;
    snprintf(result.primaryValueText, sizeof(result.primaryValueText), "%.17g", roundtrip);
    result.hasEqualSameValue = true;
    result.equalSameValue = CFEqual((CFTypeRef)primary, (CFTypeRef)same);
    result.hasEqualDifferentValue = true;
    result.equalDifferentValue = CFEqual((CFTypeRef)primary, (CFTypeRef)different);
    result.hasHashPrimary = true;
    result.hashPrimary = (long)CFHash((CFTypeRef)primary);
    result.hasHashSameValue = true;
    result.hashSameValue = (long)CFHash((CFTypeRef)same);
    result.hasHashDifferentValue = true;
    result.hashDifferentValue = (long)CFHash((CFTypeRef)different);
    result.success =
        (primary != NULL) &&
        (same != NULL) &&
        (different != NULL) &&
        (roundtrip == value) &&
        result.equalSameValue &&
        !result.equalDifferentValue &&
        (result.hashPrimary == result.hashSameValue);

    CFRelease((CFTypeRef)different);
    CFRelease((CFTypeRef)same);
    CFRelease((CFTypeRef)primary);
    return result;
}

static void print_results(FILE *stream, const struct CaseResult *results, size_t count) {
    fputs("{\n", stream);
    fputs("  \"probe\": \"subset1_scalar_core\",\n", stream);
    fputs("  \"results\": [\n", stream);
    for (size_t index = 0; index < count; ++index) {
        json_print_result(stream, &results[index], index + 1 < count);
    }
    fputs("  ]\n", stream);
    fputs("}\n", stream);
}

int main(void) {
    struct CaseResult results[] = {
        observe_boolean_singleton("cfboolean_true_singleton", kCFBooleanTrue, true),
        observe_boolean_singleton("cfboolean_false_singleton", kCFBooleanFalse, false),
        observe_boolean_equality_hash(),
        observe_data_roundtrip(),
        observe_number_sint32_roundtrip(),
        observe_number_float64_roundtrip(),
        observe_number_cross_type_equality(),
        observe_date_roundtrip()
    };

    print_results(stdout, results, sizeof(results) / sizeof(results[0]));
    return 0;
}
