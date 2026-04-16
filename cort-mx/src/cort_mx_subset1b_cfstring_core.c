#include <CoreFoundation/CoreFoundation.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    kMaxAPICalls = 10,
    kMaxText = 512
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

static void set_hex_bytes(char *buffer, size_t bufferSize, const UInt8 *bytes, size_t byteCount) {
    size_t offset = 0;
    if (bufferSize == 0) {
        return;
    }
    buffer[0] = '\0';
    for (size_t i = 0; i < byteCount && offset + 3 <= bufferSize; ++i) {
        int written = snprintf(buffer + offset, bufferSize - offset, "%02x", (unsigned int)bytes[i]);
        if (written < 0 || (size_t)written >= bufferSize - offset) {
            break;
        }
        offset += (size_t)written;
    }
}

static void set_hex_unichars(char *buffer, size_t bufferSize, const UniChar *chars, CFIndex charCount) {
    size_t offset = 0;
    if (bufferSize == 0) {
        return;
    }
    buffer[0] = '\0';
    for (CFIndex i = 0; i < charCount && offset + 5 <= bufferSize; ++i) {
        int written = snprintf(buffer + offset, bufferSize - offset, "%04x", (unsigned int)chars[i]);
        if (written < 0 || (size_t)written >= bufferSize - offset) {
            break;
        }
        offset += (size_t)written;
    }
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

static bool utf16_hex_for_string(CFStringRef string, char *buffer, size_t bufferSize) {
    if (string == NULL) {
        return false;
    }

    CFIndex length = CFStringGetLength(string);
    UniChar *characters = (UniChar *)calloc((size_t)length == 0 ? 1u : (size_t)length, sizeof(UniChar));
    if (characters == NULL) {
        return false;
    }

    if (length > 0) {
        CFStringGetCharacters(string, CFRangeMake(0, length), characters);
    }
    set_hex_unichars(buffer, bufferSize, characters, length);
    free(characters);
    return true;
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

    snprintf(buffer, bufferSize, "%s", "<conversion-failed>");
}

static void record_type_identity(struct CaseResult *result, CFTypeRef object) {
    result->hasTypeID = true;
    result->typeID = (long)CFGetTypeID(object);

    CFStringRef description = CFCopyTypeIDDescription(CFGetTypeID(object));
    if (description != NULL) {
        result->hasTypeDescription = true;
        cfstring_to_utf8(description, result->typeDescription, sizeof(result->typeDescription));
        CFRelease(description);
    }
}

static void init_required_result(
    struct CaseResult *result,
    const char *name,
    const char *ownershipNote,
    const char **apiCalls,
    size_t apiCallCount
) {
    memset(result, 0, sizeof(*result));
    result->name = name;
    result->classification = "required";
    result->fxMatchLevel = "semantic";
    result->ownershipNote = ownershipNote;
    set_api_calls(result, apiCalls, apiCallCount);
}

static struct CaseResult observe_ascii_cstring_roundtrip(void) {
    static const char *apiCalls[] = {
        "CFStringCreateWithCString",
        "CFRetain",
        "CFRelease",
        "CFStringGetLength",
        "CFStringGetCString",
        "CFStringGetCharacters",
        "CFEqual",
        "CFHash"
    };
    static const char sample[] = "launchd.packet-key";
    static const char differentSample[] = "launchd.packet-value";
    static const char expectedUTF8Hex[] = "6c61756e6368642e7061636b65742d6b6579";
    static const char expectedUTF16Hex[] = "006c00610075006e006300680064002e007000610063006b00650074002d006b00650079";

    struct CaseResult result;
    init_required_result(
        &result,
        "cfstring_ascii_cstring_roundtrip",
        "Subset 1B requires immutable ASCII create/get semantics for plist-valid key paths.",
        apiCalls,
        sizeof(apiCalls) / sizeof(apiCalls[0])
    );

    CFStringRef primary = CFStringCreateWithCString(kCFAllocatorSystemDefault, sample, kCFStringEncodingASCII);
    CFStringRef same = CFStringCreateWithCString(kCFAllocatorSystemDefault, sample, kCFStringEncodingASCII);
    CFStringRef different = CFStringCreateWithCString(kCFAllocatorSystemDefault, differentSample, kCFStringEncodingASCII);

    if (primary != NULL) {
        CFRetain(primary);
        CFRelease(primary);
        record_type_identity(&result, primary);
        result.hasLengthValue = true;
        result.lengthValue = (long)CFStringGetLength(primary);
        result.hasPrimaryValueText = utf8_hex_for_string(primary, result.primaryValueText, sizeof(result.primaryValueText));
        result.hasAlternateValueText = utf16_hex_for_string(primary, result.alternateValueText, sizeof(result.alternateValueText));
    }

    if (primary != NULL && same != NULL) {
        result.hasEqualSameValue = true;
        result.equalSameValue = CFEqual(primary, same);
        result.hasHashPrimary = true;
        result.hashPrimary = (long)CFHash(primary);
        result.hasHashSameValue = true;
        result.hashSameValue = (long)CFHash(same);
    }

    if (primary != NULL && different != NULL) {
        result.hasEqualDifferentValue = true;
        result.equalDifferentValue = CFEqual(primary, different);
        result.hasHashDifferentValue = true;
        result.hashDifferentValue = (long)CFHash(different);
    }

    result.success =
        primary != NULL &&
        same != NULL &&
        different != NULL &&
        result.lengthValue == 18 &&
        result.hasPrimaryValueText &&
        strcmp(result.primaryValueText, expectedUTF8Hex) == 0 &&
        result.hasAlternateValueText &&
        strcmp(result.alternateValueText, expectedUTF16Hex) == 0 &&
        result.equalSameValue &&
        !result.equalDifferentValue &&
        result.hashPrimary == result.hashSameValue;

    if (different != NULL) {
        CFRelease(different);
    }
    if (same != NULL) {
        CFRelease(same);
    }
    if (primary != NULL) {
        CFRelease(primary);
    }

    return result;
}

static struct CaseResult observe_utf8_cstring_roundtrip(void) {
    static const char *apiCalls[] = {
        "CFStringCreateWithCString",
        "CFStringGetLength",
        "CFStringGetCString",
        "CFStringGetCharacters",
        "CFEqual",
        "CFHash"
    };
    static const char sample[] = "caf\xc3\xa9-\xe2\x98\x83-\xf0\x9f\x93\xa6";
    static const char differentSample[] = "caf\xc3\xa9-\xe2\x98\x83-\xe2\x98\x85";
    static const char expectedUTF8Hex[] = "636166c3a92de298832df09f93a6";
    static const char expectedUTF16Hex[] = "00630061006600e9002d2603002dd83ddce6";

    struct CaseResult result;
    init_required_result(
        &result,
        "cfstring_utf8_cstring_roundtrip",
        "Subset 1B requires UTF-8 create/export semantics, including tested BMP and supplementary scalar paths.",
        apiCalls,
        sizeof(apiCalls) / sizeof(apiCalls[0])
    );

    CFStringRef primary = CFStringCreateWithCString(kCFAllocatorSystemDefault, sample, kCFStringEncodingUTF8);
    CFStringRef same = CFStringCreateWithCString(kCFAllocatorSystemDefault, sample, kCFStringEncodingUTF8);
    CFStringRef different = CFStringCreateWithCString(kCFAllocatorSystemDefault, differentSample, kCFStringEncodingUTF8);

    if (primary != NULL) {
        record_type_identity(&result, primary);
        result.hasLengthValue = true;
        result.lengthValue = (long)CFStringGetLength(primary);
        result.hasPrimaryValueText = utf8_hex_for_string(primary, result.primaryValueText, sizeof(result.primaryValueText));
        result.hasAlternateValueText = utf16_hex_for_string(primary, result.alternateValueText, sizeof(result.alternateValueText));
    }

    if (primary != NULL && same != NULL) {
        result.hasEqualSameValue = true;
        result.equalSameValue = CFEqual(primary, same);
        result.hasHashPrimary = true;
        result.hashPrimary = (long)CFHash(primary);
        result.hasHashSameValue = true;
        result.hashSameValue = (long)CFHash(same);
    }

    if (primary != NULL && different != NULL) {
        result.hasEqualDifferentValue = true;
        result.equalDifferentValue = CFEqual(primary, different);
        result.hasHashDifferentValue = true;
        result.hashDifferentValue = (long)CFHash(different);
    }

    result.success =
        primary != NULL &&
        same != NULL &&
        different != NULL &&
        result.lengthValue == 9 &&
        result.hasPrimaryValueText &&
        strcmp(result.primaryValueText, expectedUTF8Hex) == 0 &&
        result.hasAlternateValueText &&
        strcmp(result.alternateValueText, expectedUTF16Hex) == 0 &&
        result.equalSameValue &&
        !result.equalDifferentValue &&
        result.hashPrimary == result.hashSameValue;

    if (different != NULL) {
        CFRelease(different);
    }
    if (same != NULL) {
        CFRelease(same);
    }
    if (primary != NULL) {
        CFRelease(primary);
    }

    return result;
}

static struct CaseResult observe_bytes_ascii_roundtrip(void) {
    static const char *apiCalls[] = {
        "CFStringCreateWithBytes",
        "CFStringGetLength",
        "CFStringGetCString",
        "CFStringGetCharacters",
        "CFStringGetBytes",
        "CFEqual",
        "CFHash"
    };
    static const UInt8 sampleBytes[] = "launchd.packet-key";
    static const char expectedASCIIHex[] = "6c61756e6368642e7061636b65742d6b6579";
    static const char expectedUTF16Hex[] = "006c00610075006e006300680064002e007000610063006b00650074002d006b00650079";

    struct CaseResult result;
    init_required_result(
        &result,
        "cfstring_bytes_ascii_roundtrip",
        "Subset 1B requires ASCII byte creation and full-range ASCII extraction for later bplist write fast paths.",
        apiCalls,
        sizeof(apiCalls) / sizeof(apiCalls[0])
    );

    CFStringRef primary = CFStringCreateWithBytes(
        kCFAllocatorSystemDefault,
        sampleBytes,
        (CFIndex)(sizeof(sampleBytes) - 1u),
        kCFStringEncodingASCII,
        false
    );
    CFStringRef same = CFStringCreateWithCString(kCFAllocatorSystemDefault, (const char *)sampleBytes, kCFStringEncodingASCII);
    CFStringRef different = CFStringCreateWithCString(kCFAllocatorSystemDefault, "launchd.packet-value", kCFStringEncodingASCII);

    UInt8 extracted[64];
    CFIndex usedBytes = -1;
    CFIndex convertedCount = -1;
    memset(extracted, 0, sizeof(extracted));

    if (primary != NULL) {
        record_type_identity(&result, primary);
        convertedCount = CFStringGetBytes(
            primary,
            CFRangeMake(0, CFStringGetLength(primary)),
            kCFStringEncodingASCII,
            0,
            false,
            extracted,
            (CFIndex)sizeof(extracted),
            &usedBytes
        );
        result.hasLengthValue = true;
        result.lengthValue = (long)convertedCount;
        result.hasPrimaryValueText = true;
        set_hex_bytes(result.primaryValueText, sizeof(result.primaryValueText), extracted, (size_t)usedBytes);
        result.hasAlternateValueText = utf16_hex_for_string(primary, result.alternateValueText, sizeof(result.alternateValueText));
    }

    if (primary != NULL && same != NULL) {
        result.hasEqualSameValue = true;
        result.equalSameValue = CFEqual(primary, same);
        result.hasHashPrimary = true;
        result.hashPrimary = (long)CFHash(primary);
        result.hasHashSameValue = true;
        result.hashSameValue = (long)CFHash(same);
    }

    if (primary != NULL && different != NULL) {
        result.hasEqualDifferentValue = true;
        result.equalDifferentValue = CFEqual(primary, different);
        result.hasHashDifferentValue = true;
        result.hashDifferentValue = (long)CFHash(different);
    }

    result.success =
        primary != NULL &&
        same != NULL &&
        different != NULL &&
        convertedCount == 18 &&
        usedBytes == 18 &&
        result.hasPrimaryValueText &&
        strcmp(result.primaryValueText, expectedASCIIHex) == 0 &&
        result.hasAlternateValueText &&
        strcmp(result.alternateValueText, expectedUTF16Hex) == 0 &&
        result.equalSameValue &&
        !result.equalDifferentValue &&
        result.hashPrimary == result.hashSameValue;

    if (different != NULL) {
        CFRelease(different);
    }
    if (same != NULL) {
        CFRelease(same);
    }
    if (primary != NULL) {
        CFRelease(primary);
    }

    return result;
}

static struct CaseResult observe_utf16_characters_roundtrip(void) {
    static const char *apiCalls[] = {
        "CFStringCreateWithCharacters",
        "CFStringGetLength",
        "CFStringGetCString",
        "CFStringGetCharacters",
        "CFEqual",
        "CFHash"
    };
    static const UniChar sampleChars[] = {0x004fu, 0x03a9u, 0x002du, 0x65e5u, 0xd83du, 0xdce6u};
    static const char sampleUTF8[] = "O\xce\xa9-\xe6\x97\xa5\xf0\x9f\x93\xa6";
    static const char expectedUTF8Hex[] = "4fcea92de697a5f09f93a6";
    static const char expectedUTF16Hex[] = "004f03a9002d65e5d83ddce6";

    struct CaseResult result;
    init_required_result(
        &result,
        "cfstring_utf16_characters_roundtrip",
        "Subset 1B requires direct UTF-16 code-unit creation and exact character extraction for later Unicode bplist paths.",
        apiCalls,
        sizeof(apiCalls) / sizeof(apiCalls[0])
    );

    CFStringRef primary = CFStringCreateWithCharacters(
        kCFAllocatorSystemDefault,
        sampleChars,
        (CFIndex)(sizeof(sampleChars) / sizeof(sampleChars[0]))
    );
    CFStringRef same = CFStringCreateWithCString(kCFAllocatorSystemDefault, sampleUTF8, kCFStringEncodingUTF8);
    CFStringRef different = CFStringCreateWithCString(kCFAllocatorSystemDefault, "O\xce\xa9-\xe6\x97\xa5", kCFStringEncodingUTF8);

    if (primary != NULL) {
        record_type_identity(&result, primary);
        result.hasLengthValue = true;
        result.lengthValue = (long)CFStringGetLength(primary);
        result.hasPrimaryValueText = utf8_hex_for_string(primary, result.primaryValueText, sizeof(result.primaryValueText));
        result.hasAlternateValueText = utf16_hex_for_string(primary, result.alternateValueText, sizeof(result.alternateValueText));
    }

    if (primary != NULL && same != NULL) {
        result.hasEqualSameValue = true;
        result.equalSameValue = CFEqual(primary, same);
        result.hasHashPrimary = true;
        result.hashPrimary = (long)CFHash(primary);
        result.hasHashSameValue = true;
        result.hashSameValue = (long)CFHash(same);
    }

    if (primary != NULL && different != NULL) {
        result.hasEqualDifferentValue = true;
        result.equalDifferentValue = CFEqual(primary, different);
        result.hasHashDifferentValue = true;
        result.hashDifferentValue = (long)CFHash(different);
    }

    result.success =
        primary != NULL &&
        same != NULL &&
        different != NULL &&
        result.lengthValue == 6 &&
        result.hasPrimaryValueText &&
        strcmp(result.primaryValueText, expectedUTF8Hex) == 0 &&
        result.hasAlternateValueText &&
        strcmp(result.alternateValueText, expectedUTF16Hex) == 0 &&
        result.equalSameValue &&
        !result.equalDifferentValue &&
        result.hashPrimary == result.hashSameValue;

    if (different != NULL) {
        CFRelease(different);
    }
    if (same != NULL) {
        CFRelease(same);
    }
    if (primary != NULL) {
        CFRelease(primary);
    }

    return result;
}

static struct CaseResult observe_copy_equality_hash(void) {
    static const char *apiCalls[] = {
        "CFStringCreateWithCString",
        "CFStringCreateCopy",
        "CFStringGetLength",
        "CFEqual",
        "CFHash"
    };
    static const char sample[] = "caf\xc3\xa9-\xe2\x98\x83-\xf0\x9f\x93\xa6";
    static const char differentSample[] = "caf\xc3\xa9-\xe2\x98\x83-\xe2\x98\x85";
    static const char expectedUTF8Hex[] = "636166c3a92de298832df09f93a6";
    static const char expectedUTF16Hex[] = "00630061006600e9002d2603002dd83ddce6";

    struct CaseResult result;
    init_required_result(
        &result,
        "cfstring_copy_equality_hash",
        "Subset 1B requires Create/Copy ownership semantics and equal-string hash coherence; copy pointer identity remains diagnostic only.",
        apiCalls,
        sizeof(apiCalls) / sizeof(apiCalls[0])
    );

    CFStringRef primary = CFStringCreateWithCString(kCFAllocatorSystemDefault, sample, kCFStringEncodingUTF8);
    CFStringRef copy = primary == NULL ? NULL : CFStringCreateCopy(kCFAllocatorSystemDefault, primary);
    CFStringRef different = CFStringCreateWithCString(kCFAllocatorSystemDefault, differentSample, kCFStringEncodingUTF8);

    if (primary != NULL) {
        record_type_identity(&result, primary);
        result.hasLengthValue = true;
        result.lengthValue = (long)CFStringGetLength(primary);
        result.hasPrimaryValueText = utf8_hex_for_string(primary, result.primaryValueText, sizeof(result.primaryValueText));
        result.hasAlternateValueText = utf16_hex_for_string(primary, result.alternateValueText, sizeof(result.alternateValueText));
    }

    if (primary != NULL && copy != NULL) {
        result.hasEqualSameValue = true;
        result.equalSameValue = CFEqual(primary, copy);
        result.hasHashPrimary = true;
        result.hashPrimary = (long)CFHash(primary);
        result.hasHashSameValue = true;
        result.hashSameValue = (long)CFHash(copy);
    }

    if (primary != NULL && different != NULL) {
        result.hasEqualDifferentValue = true;
        result.equalDifferentValue = CFEqual(primary, different);
        result.hasHashDifferentValue = true;
        result.hashDifferentValue = (long)CFHash(different);
    }

    result.success =
        primary != NULL &&
        copy != NULL &&
        different != NULL &&
        result.lengthValue == 9 &&
        result.hasPrimaryValueText &&
        strcmp(result.primaryValueText, expectedUTF8Hex) == 0 &&
        result.hasAlternateValueText &&
        strcmp(result.alternateValueText, expectedUTF16Hex) == 0 &&
        result.equalSameValue &&
        !result.equalDifferentValue &&
        result.hashPrimary == result.hashSameValue;

    if (different != NULL) {
        CFRelease(different);
    }
    if (copy != NULL) {
        CFRelease(copy);
    }
    if (primary != NULL) {
        CFRelease(primary);
    }

    return result;
}

static struct CaseResult observe_getcstring_small_buffer(void) {
    static const char *apiCalls[] = {
        "CFStringCreateWithCString",
        "CFStringGetLength",
        "CFStringGetCString"
    };
    static const char sample[] = "caf\xc3\xa9-\xe2\x98\x83-\xf0\x9f\x93\xa6";

    struct CaseResult result;
    init_required_result(
        &result,
        "cfstring_getcstring_small_buffer",
        "Subset 1B requires CFStringGetCString to fail cleanly when the output buffer is too small.",
        apiCalls,
        sizeof(apiCalls) / sizeof(apiCalls[0])
    );

    CFStringRef primary = CFStringCreateWithCString(kCFAllocatorSystemDefault, sample, kCFStringEncodingUTF8);
    char smallBuffer[4];
    memset(smallBuffer, 0, sizeof(smallBuffer));

    if (primary != NULL) {
        record_type_identity(&result, primary);
        result.hasLengthValue = true;
        result.lengthValue = (long)CFStringGetLength(primary);
        result.hasPrimaryValueText = true;
        snprintf(result.primaryValueText, sizeof(result.primaryValueText), "%s", "buffer-too-small");
        result.success = !CFStringGetCString(primary, smallBuffer, (CFIndex)sizeof(smallBuffer), kCFStringEncodingUTF8);
        CFRelease(primary);
    } else {
        result.success = false;
    }

    return result;
}

static struct CaseResult observe_invalid_utf8_rejected(void) {
    static const char *apiCalls[] = {
        "CFStringCreateWithBytes"
    };
    static const UInt8 invalidUTF8[] = {0xc3u, 0x28u};

    struct CaseResult result;
    memset(&result, 0, sizeof(result));
    result.name = "cfstring_bytes_invalid_utf8_rejected";
    result.classification = "required-but-error-text-flexible";
    result.fxMatchLevel = "error-text-flexible";
    result.ownershipNote = "Malformed UTF-8 must be rejected safely. Error wording is not part of the contract.";
    set_api_calls(&result, apiCalls, sizeof(apiCalls) / sizeof(apiCalls[0]));

    CFStringRef string = CFStringCreateWithBytes(
        kCFAllocatorSystemDefault,
        invalidUTF8,
        (CFIndex)sizeof(invalidUTF8),
        kCFStringEncodingUTF8,
        false
    );
    result.success = (string == NULL);
    if (string != NULL) {
        CFRelease(string);
    }
    return result;
}

int main(void) {
    struct CaseResult results[] = {
        observe_ascii_cstring_roundtrip(),
        observe_utf8_cstring_roundtrip(),
        observe_bytes_ascii_roundtrip(),
        observe_utf16_characters_roundtrip(),
        observe_copy_equality_hash(),
        observe_getcstring_small_buffer(),
        observe_invalid_utf8_rejected()
    };
    size_t resultCount = sizeof(results) / sizeof(results[0]);

    fputs("{\n", stdout);
    fputs("  \"probe\": \"subset1b_cfstring_core\",\n", stdout);
    fputs("  \"results\": [\n", stdout);
    for (size_t i = 0; i < resultCount; ++i) {
        json_print_result(stdout, &results[i], i + 1u < resultCount);
    }
    fputs("  ]\n", stdout);
    fputs("}\n", stdout);
    return 0;
}
