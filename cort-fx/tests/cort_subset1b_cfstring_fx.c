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
    bool success;
    bool hasTypeID;
    long typeID;
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

static void json_print_result(FILE *stream, const struct CaseResult *result, bool trailingComma) {
    fputs("    {\n", stream);
    fputs("      \"name\": ", stream);
    json_print_string(stream, result->name);
    fputs(",\n      \"classification\": ", stream);
    json_print_string(stream, result->classification);
    fputs(",\n      \"success\": ", stream);
    fputs(result->success ? "true" : "false", stream);
    fputs(",\n      \"type_id\": ", stream);
    json_print_long_or_null(stream, result->hasTypeID, result->typeID);
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

static void set_hex_unichars(char *buffer, size_t bufferSize, const UniChar *characters, CFIndex length) {
    size_t offset = 0;
    if (bufferSize == 0) {
        return;
    }
    buffer[0] = '\0';
    for (CFIndex index = 0; index < length && offset + 5 <= bufferSize; ++index) {
        int written = snprintf(buffer + offset, bufferSize - offset, "%04x", (unsigned int)characters[index]);
        if (written < 0 || (size_t)written >= bufferSize - offset) {
            break;
        }
        offset += (size_t)written;
    }
}

static bool utf8_hex_for_string(CFStringRef string, char *buffer, size_t bufferSize) {
    CFIndex length = CFStringGetLength(string);
    CFIndex bufferBytes = (length * 4) + 1;
    char *temporary = (char *)calloc((size_t)bufferBytes, sizeof(char));
    if (temporary == NULL) {
        return false;
    }

    bool ok = CFStringGetCString(string, temporary, bufferBytes, kCFStringEncodingUTF8);
    if (ok) {
        set_hex_bytes(buffer, bufferSize, (const UInt8 *)temporary, strlen(temporary));
    }
    free(temporary);
    return ok;
}

static void utf16_hex_for_string(CFStringRef string, char *buffer, size_t bufferSize) {
    CFIndex length = CFStringGetLength(string);
    UniChar *characters = (UniChar *)calloc(length == 0 ? 1u : (size_t)length, sizeof(UniChar));
    if (characters == NULL) {
        buffer[0] = '\0';
        return;
    }
    if (length > 0) {
        CFStringGetCharacters(string, CFRangeMake(0, length), characters);
    }
    set_hex_unichars(buffer, bufferSize, characters, length);
    free(characters);
}

static void record_type_identity(struct CaseResult *result, CFTypeRef object) {
    result->hasTypeID = true;
    result->typeID = (long)CFGetTypeID(object);
}

static struct CaseResult observe_ascii_cstring_roundtrip(void) {
    struct CaseResult result;
    memset(&result, 0, sizeof(result));
    result.name = "cfstring_ascii_cstring_roundtrip";
    result.classification = "required";

    CFStringRef primary = CFStringCreateWithCString(kCFAllocatorSystemDefault, "launchd.packet-key", kCFStringEncodingASCII);
    CFStringRef same = CFStringCreateWithCString(kCFAllocatorSystemDefault, "launchd.packet-key", kCFStringEncodingASCII);
    CFStringRef different = CFStringCreateWithCString(kCFAllocatorSystemDefault, "launchd.packet-value", kCFStringEncodingASCII);

    if (primary == NULL || same == NULL || different == NULL) {
        result.success = false;
        return result;
    }

    record_type_identity(&result, (CFTypeRef)primary);
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
    result.hasLengthValue = true;
    result.lengthValue = (long)CFStringGetLength(primary);
    result.hasPrimaryValueText = utf8_hex_for_string(primary, result.primaryValueText, sizeof(result.primaryValueText));
    result.hasAlternateValueText = true;
    utf16_hex_for_string(primary, result.alternateValueText, sizeof(result.alternateValueText));
    result.success =
        primary != NULL &&
        same != NULL &&
        different != NULL &&
        result.equalSameValue &&
        !result.equalDifferentValue &&
        (result.hashPrimary == result.hashSameValue) &&
        result.lengthValue == 18 &&
        result.hasPrimaryValueText;

    CFRelease((CFTypeRef)different);
    CFRelease((CFTypeRef)same);
    CFRelease((CFTypeRef)primary);
    return result;
}

static struct CaseResult observe_empty_roundtrip(void) {
    struct CaseResult result;
    memset(&result, 0, sizeof(result));
    result.name = "cfstring_empty_roundtrip";
    result.classification = "required";

    CFStringRef primary = CFStringCreateWithCString(kCFAllocatorSystemDefault, "", kCFStringEncodingASCII);
    CFStringRef same = CFStringCreateWithBytes(kCFAllocatorSystemDefault, NULL, 0, kCFStringEncodingASCII, false);
    CFStringRef sameChars = CFStringCreateWithCharacters(kCFAllocatorSystemDefault, NULL, 0);
    CFStringRef different = CFStringCreateWithCString(kCFAllocatorSystemDefault, "x", kCFStringEncodingASCII);
    char buffer[1];

    if (primary == NULL || same == NULL || sameChars == NULL || different == NULL) {
        result.success = false;
        return result;
    }

    record_type_identity(&result, (CFTypeRef)primary);
    result.hasEqualSameValue = true;
    result.equalSameValue = CFEqual((CFTypeRef)primary, (CFTypeRef)same) && CFEqual((CFTypeRef)primary, (CFTypeRef)sameChars);
    result.hasEqualDifferentValue = true;
    result.equalDifferentValue = CFEqual((CFTypeRef)primary, (CFTypeRef)different);
    result.hasHashPrimary = true;
    result.hashPrimary = (long)CFHash((CFTypeRef)primary);
    result.hasHashSameValue = true;
    result.hashSameValue = (long)CFHash((CFTypeRef)same);
    result.hasHashDifferentValue = true;
    result.hashDifferentValue = (long)CFHash((CFTypeRef)different);
    result.hasLengthValue = true;
    result.lengthValue = (long)CFStringGetLength(primary);
    result.hasPrimaryValueText = true;
    buffer[0] = '\0';
    result.hasAlternateValueText = true;
    if (!CFStringGetCString(primary, buffer, 1, kCFStringEncodingUTF8)) {
        result.primaryValueText[0] = '\0';
        result.success = false;
    } else {
        result.primaryValueText[0] = '\0';
        result.alternateValueText[0] = '\0';
        result.success = true;
    }
    result.success =
        result.success &&
        primary != NULL &&
        same != NULL &&
        sameChars != NULL &&
        different != NULL &&
        result.equalSameValue &&
        !result.equalDifferentValue &&
        (result.hashPrimary == result.hashSameValue) &&
        result.lengthValue == 0;

    CFRelease((CFTypeRef)different);
    CFRelease((CFTypeRef)sameChars);
    CFRelease((CFTypeRef)same);
    CFRelease((CFTypeRef)primary);
    return result;
}

static struct CaseResult observe_utf8_cstring_roundtrip(void) {
    static const char text[] = "caf\xC3\xA9-\xE2\x98\x83-\xF0\x9F\x93\xA6";

    struct CaseResult result;
    memset(&result, 0, sizeof(result));
    result.name = "cfstring_utf8_cstring_roundtrip";
    result.classification = "required";

    CFStringRef primary = CFStringCreateWithCString(kCFAllocatorSystemDefault, text, kCFStringEncodingUTF8);
    CFStringRef same = CFStringCreateWithCString(kCFAllocatorSystemDefault, text, kCFStringEncodingUTF8);
    CFStringRef different = CFStringCreateWithCString(kCFAllocatorSystemDefault, "caf\xC3\xA9-\xE2\x98\x83-\xF0\x9F\x93\xA7", kCFStringEncodingUTF8);

    if (primary == NULL || same == NULL || different == NULL) {
        result.success = false;
        return result;
    }

    record_type_identity(&result, (CFTypeRef)primary);
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
    result.hasLengthValue = true;
    result.lengthValue = (long)CFStringGetLength(primary);
    result.hasPrimaryValueText = utf8_hex_for_string(primary, result.primaryValueText, sizeof(result.primaryValueText));
    result.hasAlternateValueText = true;
    utf16_hex_for_string(primary, result.alternateValueText, sizeof(result.alternateValueText));
    result.success =
        primary != NULL &&
        same != NULL &&
        different != NULL &&
        result.equalSameValue &&
        !result.equalDifferentValue &&
        (result.hashPrimary == result.hashSameValue) &&
        result.lengthValue == 9 &&
        result.hasPrimaryValueText;

    CFRelease((CFTypeRef)different);
    CFRelease((CFTypeRef)same);
    CFRelease((CFTypeRef)primary);
    return result;
}

static struct CaseResult observe_bytes_ascii_roundtrip(void) {
    static const UInt8 bytes[] = "launchd.packet-key";

    struct CaseResult result;
    memset(&result, 0, sizeof(result));
    result.name = "cfstring_bytes_ascii_roundtrip";
    result.classification = "required";

    UInt8 extracted[32];
    CFIndex usedBytes = 0;
    CFStringRef primary = CFStringCreateWithBytes(kCFAllocatorSystemDefault, bytes, 18, kCFStringEncodingASCII, false);
    CFStringRef same = CFStringCreateWithCString(kCFAllocatorSystemDefault, "launchd.packet-key", kCFStringEncodingASCII);
    CFStringRef different = CFStringCreateWithBytes(kCFAllocatorSystemDefault, (const UInt8 *)"launchd.packet-value", 20, kCFStringEncodingASCII, false);

    if (primary == NULL || same == NULL || different == NULL) {
        result.success = false;
        return result;
    }

    record_type_identity(&result, (CFTypeRef)primary);
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
    result.hasLengthValue = true;
    result.lengthValue = (long)CFStringGetLength(primary);
    result.hasPrimaryValueText = true;
    result.hasAlternateValueText = true;
    if (CFStringGetBytes(primary, CFRangeMake(0, CFStringGetLength(primary)), kCFStringEncodingASCII, 0, false, extracted, (CFIndex)sizeof(extracted), &usedBytes) == 18 &&
        usedBytes == 18) {
        set_hex_bytes(result.primaryValueText, sizeof(result.primaryValueText), extracted, (size_t)usedBytes);
        utf16_hex_for_string(primary, result.alternateValueText, sizeof(result.alternateValueText));
    } else {
        result.primaryValueText[0] = '\0';
        result.alternateValueText[0] = '\0';
    }
    result.success =
        primary != NULL &&
        same != NULL &&
        different != NULL &&
        result.equalSameValue &&
        !result.equalDifferentValue &&
        (result.hashPrimary == result.hashSameValue) &&
        result.lengthValue == 18 &&
        strcmp(result.primaryValueText, "6c61756e6368642e7061636b65742d6b6579") == 0;

    CFRelease((CFTypeRef)different);
    CFRelease((CFTypeRef)same);
    CFRelease((CFTypeRef)primary);
    return result;
}

static struct CaseResult observe_utf16_characters_roundtrip(void) {
    static const UniChar characters[] = {0x004f, 0x03a9, 0x002d, 0x65e5, 0xd83d, 0xdce6};
    static const UniChar differentCharacters[] = {0x004f, 0x03a9, 0x002d, 0x65e5, 0xd83d, 0xdce7};

    struct CaseResult result;
    memset(&result, 0, sizeof(result));
    result.name = "cfstring_utf16_characters_roundtrip";
    result.classification = "required";

    CFStringRef primary = CFStringCreateWithCharacters(kCFAllocatorSystemDefault, characters, 6);
    CFStringRef same = CFStringCreateWithCharacters(kCFAllocatorSystemDefault, characters, 6);
    CFStringRef different = CFStringCreateWithCharacters(kCFAllocatorSystemDefault, differentCharacters, 6);

    if (primary == NULL || same == NULL || different == NULL) {
        result.success = false;
        return result;
    }

    record_type_identity(&result, (CFTypeRef)primary);
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
    result.hasLengthValue = true;
    result.lengthValue = (long)CFStringGetLength(primary);
    result.hasPrimaryValueText = utf8_hex_for_string(primary, result.primaryValueText, sizeof(result.primaryValueText));
    result.hasAlternateValueText = true;
    utf16_hex_for_string(primary, result.alternateValueText, sizeof(result.alternateValueText));
    result.success =
        primary != NULL &&
        same != NULL &&
        different != NULL &&
        result.equalSameValue &&
        !result.equalDifferentValue &&
        (result.hashPrimary == result.hashSameValue) &&
        result.lengthValue == 6 &&
        result.hasPrimaryValueText;

    CFRelease((CFTypeRef)different);
    CFRelease((CFTypeRef)same);
    CFRelease((CFTypeRef)primary);
    return result;
}

static struct CaseResult observe_copy_equality_hash(void) {
    static const char text[] = "caf\xC3\xA9-\xE2\x98\x83-\xF0\x9F\x93\xA6";

    struct CaseResult result;
    memset(&result, 0, sizeof(result));
    result.name = "cfstring_copy_equality_hash";
    result.classification = "required";

    CFStringRef primary = CFStringCreateWithCString(kCFAllocatorSystemDefault, text, kCFStringEncodingUTF8);
    CFStringRef same = CFStringCreateCopy(kCFAllocatorSystemDefault, primary);
    CFStringRef different = CFStringCreateWithCString(kCFAllocatorSystemDefault, "caf\xC3\xA9-\xE2\x98\x83-\xF0\x9F\x93\xA7", kCFStringEncodingUTF8);

    if (primary == NULL || same == NULL || different == NULL) {
        result.success = false;
        return result;
    }

    record_type_identity(&result, (CFTypeRef)primary);
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
    result.hasLengthValue = true;
    result.lengthValue = (long)CFStringGetLength(primary);
    result.hasPrimaryValueText = utf8_hex_for_string(primary, result.primaryValueText, sizeof(result.primaryValueText));
    result.hasAlternateValueText = true;
    utf16_hex_for_string(primary, result.alternateValueText, sizeof(result.alternateValueText));
    result.success =
        primary != NULL &&
        same != NULL &&
        different != NULL &&
        result.equalSameValue &&
        !result.equalDifferentValue &&
        (result.hashPrimary == result.hashSameValue) &&
        result.lengthValue == 9 &&
        result.hasPrimaryValueText;

    CFRelease((CFTypeRef)different);
    CFRelease((CFTypeRef)same);
    CFRelease((CFTypeRef)primary);
    return result;
}

static struct CaseResult observe_getcstring_small_buffer(void) {
    static const char text[] = "caf\xC3\xA9-\xE2\x98\x83-\xF0\x9F\x93\xA6";

    struct CaseResult result;
    memset(&result, 0, sizeof(result));
    result.name = "cfstring_getcstring_small_buffer";
    result.classification = "required";

    char exactFit[sizeof(text)];
    char smallBuffer[sizeof(text) - 1];
    CFStringRef primary = CFStringCreateWithCString(kCFAllocatorSystemDefault, text, kCFStringEncodingUTF8);

    if (primary == NULL) {
        result.success = false;
        return result;
    }

    record_type_identity(&result, (CFTypeRef)primary);
    result.hasLengthValue = true;
    result.lengthValue = (long)CFStringGetLength(primary);
    result.hasPrimaryValueText = true;
    result.hasAlternateValueText = true;
    if (CFStringGetCString(primary, exactFit, (CFIndex)sizeof(exactFit), kCFStringEncodingUTF8)) {
        set_hex_bytes(result.primaryValueText, sizeof(result.primaryValueText), (const UInt8 *)exactFit, strlen(exactFit));
    } else {
        result.primaryValueText[0] = '\0';
    }
    snprintf(result.alternateValueText, sizeof(result.alternateValueText), "%s", "small-buffer-fails");
    result.success =
        primary != NULL &&
        result.lengthValue == 9 &&
        strcmp(result.primaryValueText, "636166c3a92de298832df09f93a6") == 0 &&
        !CFStringGetCString(primary, smallBuffer, (CFIndex)sizeof(smallBuffer), kCFStringEncodingUTF8);

    CFRelease((CFTypeRef)primary);
    return result;
}

static struct CaseResult observe_bytes_invalid_utf8_rejected(void) {
    static const UInt8 invalidBytes[] = {0xc3u, 0x28u};

    struct CaseResult result;
    memset(&result, 0, sizeof(result));
    result.name = "cfstring_bytes_invalid_utf8_rejected";
    result.classification = "required-but-error-text-flexible";
    result.success = (CFStringCreateWithBytes(kCFAllocatorSystemDefault, invalidBytes, 2, kCFStringEncodingUTF8, false) == NULL);
    return result;
}

static struct CaseResult observe_cstring_invalid_utf8_rejected(void) {
    static const char invalidCString[] = "\xC3(";

    struct CaseResult result;
    memset(&result, 0, sizeof(result));
    result.name = "cfstring_cstring_invalid_utf8_rejected";
    result.classification = "required-but-error-text-flexible";
    result.success = (CFStringCreateWithCString(kCFAllocatorSystemDefault, invalidCString, kCFStringEncodingUTF8) == NULL);
    return result;
}

int main(void) {
    struct CaseResult results[] = {
        observe_ascii_cstring_roundtrip(),
        observe_empty_roundtrip(),
        observe_utf8_cstring_roundtrip(),
        observe_bytes_ascii_roundtrip(),
        observe_utf16_characters_roundtrip(),
        observe_copy_equality_hash(),
        observe_getcstring_small_buffer(),
        observe_bytes_invalid_utf8_rejected(),
        observe_cstring_invalid_utf8_rejected()
    };
    size_t caseCount = sizeof(results) / sizeof(results[0]);

    fputs("{\n", stdout);
    fputs("  \"probe\": \"subset1b_cfstring_core\",\n", stdout);
    fputs("  \"results\": [\n", stdout);
    for (size_t index = 0; index < caseCount; ++index) {
        json_print_result(stdout, &results[index], index + 1 < caseCount);
    }
    fputs("  ]\n", stdout);
    fputs("}\n", stdout);
    return 0;
}
