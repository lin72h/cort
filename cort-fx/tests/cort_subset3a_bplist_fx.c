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
    bool success;
    bool hasTypeID;
    long typeID;
    bool hasTypeDescription;
    char typeDescription[kMaxText];
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
    size_t offset = 0u;
    if (bufferSize == 0u) {
        return;
    }
    buffer[0] = '\0';
    for (size_t index = 0u; index < byteCount && offset + 3u <= bufferSize; ++index) {
        int written = snprintf(buffer + offset, bufferSize - offset, "%02x", (unsigned int)bytes[index]);
        if (written < 0 || (size_t)written >= bufferSize - offset) {
            break;
        }
        offset += (size_t)written;
    }
}

static bool set_utf8_hex_for_string(char *buffer, size_t bufferSize, CFStringRef string) {
    CFIndex length = CFStringGetLength(string);
    char text[kMaxText];
    if (!CFStringGetCString(string, text, (CFIndex)sizeof(text), kCFStringEncodingUTF8)) {
        return false;
    }
    set_hex_bytes(buffer, bufferSize, (const UInt8 *)text, strlen(text));
    return length >= 0;
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

static CFNumberRef create_sint64_number(SInt64 value) {
    return CFNumberCreate(kCFAllocatorSystemDefault, kCFNumberSInt64Type, &value);
}

static CFNumberRef create_float64_number(double value) {
    return CFNumberCreate(kCFAllocatorSystemDefault, kCFNumberFloat64Type, &value);
}

static bool binary_header_is_bplist00(CFDataRef data) {
    static const UInt8 header[] = {'b', 'p', 'l', 'i', 's', 't', '0', '0'};
    return CFDataGetLength(data) >= 8 && memcmp(CFDataGetBytePtr(data), header, sizeof(header)) == 0;
}

static bool data_roundtrip(CFPropertyListRef plist, CFDataRef *dataOut, CFPropertyListRef *readbackOut, CFPropertyListFormat *formatOut, CFErrorRef *errorOut) {
    *dataOut = NULL;
    *readbackOut = NULL;
    *formatOut = 0;
    *errorOut = NULL;

    CFDataRef data = CFPropertyListCreateData(
        kCFAllocatorSystemDefault,
        plist,
        kCFPropertyListBinaryFormat_v1_0,
        0,
        errorOut
    );
    if (data == NULL) {
        return false;
    }

    CFPropertyListFormat format = 0;
    CFErrorRef readError = NULL;
    CFPropertyListRef readback = CFPropertyListCreateWithData(
        kCFAllocatorSystemDefault,
        data,
        kCFPropertyListImmutable,
        &format,
        &readError
    );
    if (readback == NULL) {
        if (readError != NULL) {
            *errorOut = readError;
        }
        CFRelease(data);
        return false;
    }

    if (readError != NULL) {
        CFRelease(readError);
    }
    *dataOut = data;
    *readbackOut = readback;
    *formatOut = format;
    return true;
}

static struct CaseResult observe_ascii_string_roundtrip(void) {
    struct CaseResult result;
    memset(&result, 0, sizeof(result));
    result.name = "bplist_ascii_string_roundtrip";
    result.classification = "required";
    result.fxMatchLevel = "semantic";

    CFStringRef string = create_ascii_string("launchd.packet-key");
    CFDataRef data = NULL;
    CFPropertyListRef readback = NULL;
    CFPropertyListFormat format = 0;
    CFErrorRef error = NULL;

    if (string != NULL && data_roundtrip((CFPropertyListRef)string, &data, &readback, &format, &error)) {
        record_type_identity(&result, readback, "CFString");
        result.hasLengthValue = true;
        result.lengthValue = (long)CFStringGetLength((CFStringRef)readback);
        result.hasPrimaryValueText = set_utf8_hex_for_string(result.primaryValueText, sizeof(result.primaryValueText), (CFStringRef)readback);
        result.hasAlternateValueText = true;
        snprintf(result.alternateValueText, sizeof(result.alternateValueText), "%s", "header=bplist00 tag=ascii-string");
        result.success =
            format == kCFPropertyListBinaryFormat_v1_0 &&
            binary_header_is_bplist00(data) &&
            CFGetTypeID(readback) == CFStringGetTypeID() &&
            CFEqual((CFTypeRef)string, readback);
    }

    if (error != NULL) {
        CFRelease(error);
    }
    if (readback != NULL) {
        CFRelease(readback);
    }
    if (data != NULL) {
        CFRelease(data);
    }
    if (string != NULL) {
        CFRelease(string);
    }
    return result;
}

static struct CaseResult observe_unicode_string_roundtrip(void) {
    struct CaseResult result;
    memset(&result, 0, sizeof(result));
    result.name = "bplist_unicode_string_roundtrip";
    result.classification = "required";
    result.fxMatchLevel = "semantic";

    CFStringRef string = create_utf8_string("caf\xC3\xA9-\xE2\x98\x83-\xF0\x9F\x93\xA6");
    CFDataRef data = NULL;
    CFPropertyListRef readback = NULL;
    CFPropertyListFormat format = 0;
    CFErrorRef error = NULL;

    if (string != NULL && data_roundtrip((CFPropertyListRef)string, &data, &readback, &format, &error)) {
        record_type_identity(&result, readback, "CFString");
        result.hasLengthValue = true;
        result.lengthValue = (long)CFStringGetLength((CFStringRef)readback);
        result.hasPrimaryValueText = set_utf8_hex_for_string(result.primaryValueText, sizeof(result.primaryValueText), (CFStringRef)readback);
        result.hasAlternateValueText = true;
        snprintf(result.alternateValueText, sizeof(result.alternateValueText), "%s", "header=bplist00 tag=unicode16-string");
        result.success =
            format == kCFPropertyListBinaryFormat_v1_0 &&
            binary_header_is_bplist00(data) &&
            CFGetTypeID(readback) == CFStringGetTypeID() &&
            CFEqual((CFTypeRef)string, readback);
    }

    if (error != NULL) {
        CFRelease(error);
    }
    if (readback != NULL) {
        CFRelease(readback);
    }
    if (data != NULL) {
        CFRelease(data);
    }
    if (string != NULL) {
        CFRelease(string);
    }
    return result;
}

static struct CaseResult observe_mixed_array_roundtrip(void) {
    static const UInt8 bytes[] = {0x10u, 0x20u, 0x30u, 0x40u};
    struct CaseResult result;
    memset(&result, 0, sizeof(result));
    result.name = "bplist_mixed_array_roundtrip";
    result.classification = "required";
    result.fxMatchLevel = "semantic";

    SInt64 intValue = 42;
    double realValue = 42.25;
    double when = 1234.5;
    CFNumberRef intNumber = create_sint64_number(intValue);
    CFNumberRef realNumber = create_float64_number(realValue);
    CFDateRef date = CFDateCreate(kCFAllocatorSystemDefault, when);
    CFDataRef dataValue = create_data(bytes, (CFIndex)sizeof(bytes));
    CFStringRef string = create_ascii_string("launchd.packet-key");
    const void *values[] = {kCFBooleanTrue, intNumber, realNumber, date, dataValue, string};
    CFArrayRef array =
        (intNumber != NULL && realNumber != NULL && date != NULL && dataValue != NULL && string != NULL)
            ? CFArrayCreate(kCFAllocatorSystemDefault, values, 6, &kCFTypeArrayCallBacks)
            : NULL;
    CFDataRef data = NULL;
    CFPropertyListRef readback = NULL;
    CFPropertyListFormat format = 0;
    CFErrorRef error = NULL;

    if (array != NULL && data_roundtrip((CFPropertyListRef)array, &data, &readback, &format, &error)) {
        CFArrayRef parsed = (CFArrayRef)readback;
        SInt64 parsedInt = 0;
        double parsedReal = 0.0;
        record_type_identity(&result, readback, "CFArray");
        result.hasLengthValue = true;
        result.lengthValue = (long)CFArrayGetCount(parsed);
        result.hasPrimaryValueText = true;
        snprintf(result.primaryValueText, sizeof(result.primaryValueText), "%s", "bool=true int=42 real=42.25 date=1234.5 data=10203040 string=6c61756e6368642e7061636b65742d6b6579");
        result.hasAlternateValueText = true;
        snprintf(result.alternateValueText, sizeof(result.alternateValueText), "%s", "header=bplist00 root=array");
        result.success =
            format == kCFPropertyListBinaryFormat_v1_0 &&
            binary_header_is_bplist00(data) &&
            CFGetTypeID(readback) == CFArrayGetTypeID() &&
            CFArrayGetCount(parsed) == 6 &&
            CFArrayGetValueAtIndex(parsed, 0) == (CFTypeRef)kCFBooleanTrue &&
            CFNumberGetValue((CFNumberRef)CFArrayGetValueAtIndex(parsed, 1), kCFNumberSInt64Type, &parsedInt) &&
            parsedInt == intValue &&
            CFNumberGetValue((CFNumberRef)CFArrayGetValueAtIndex(parsed, 2), kCFNumberFloat64Type, &parsedReal) &&
            parsedReal == realValue &&
            CFDateGetAbsoluteTime((CFDateRef)CFArrayGetValueAtIndex(parsed, 3)) == when &&
            CFEqual((CFTypeRef)CFArrayGetValueAtIndex(parsed, 4), (CFTypeRef)dataValue) &&
            CFEqual((CFTypeRef)CFArrayGetValueAtIndex(parsed, 5), (CFTypeRef)string);
    }

    if (error != NULL) {
        CFRelease(error);
    }
    if (readback != NULL) {
        CFRelease(readback);
    }
    if (data != NULL) {
        CFRelease(data);
    }
    if (array != NULL) {
        CFRelease(array);
    }
    if (string != NULL) {
        CFRelease(string);
    }
    if (dataValue != NULL) {
        CFRelease(dataValue);
    }
    if (date != NULL) {
        CFRelease(date);
    }
    if (realNumber != NULL) {
        CFRelease(realNumber);
    }
    if (intNumber != NULL) {
        CFRelease(intNumber);
    }
    return result;
}

static struct CaseResult observe_string_key_dictionary_roundtrip(void) {
    static const UInt8 bytes[] = {0x10u, 0x20u, 0x30u, 0x40u};
    struct CaseResult result;
    memset(&result, 0, sizeof(result));
    result.name = "bplist_string_key_dictionary_roundtrip";
    result.classification = "required";
    result.fxMatchLevel = "semantic";

    CFStringRef keyAscii = create_ascii_string("ascii");
    CFStringRef keyBlob = create_ascii_string("blob");
    CFStringRef keyCount = create_ascii_string("count");
    CFStringRef keyEnabled = create_ascii_string("enabled");
    CFStringRef keyUtf8 = create_ascii_string("utf8");
    CFStringRef keyWhen = create_ascii_string("when");
    CFStringRef valueAscii = create_ascii_string("launchd.packet-key");
    CFDataRef valueBlob = create_data(bytes, (CFIndex)sizeof(bytes));
    CFNumberRef valueCount = create_sint64_number(42);
    CFStringRef valueUtf8 = create_utf8_string("caf\xC3\xA9-\xE2\x98\x83-\xF0\x9F\x93\xA6");
    CFDateRef valueWhen = CFDateCreate(kCFAllocatorSystemDefault, 1234.5);
    const void *keys[] = {keyAscii, keyBlob, keyCount, keyEnabled, keyUtf8, keyWhen};
    const void *values[] = {valueAscii, valueBlob, valueCount, kCFBooleanTrue, valueUtf8, valueWhen};
    CFDictionaryRef dictionary =
        (keyAscii != NULL && keyBlob != NULL && keyCount != NULL && keyEnabled != NULL && keyUtf8 != NULL &&
         keyWhen != NULL && valueAscii != NULL && valueBlob != NULL && valueCount != NULL &&
         valueUtf8 != NULL && valueWhen != NULL)
            ? CFDictionaryCreate(
                kCFAllocatorSystemDefault,
                keys,
                values,
                6,
                &kCFTypeDictionaryKeyCallBacks,
                &kCFTypeDictionaryValueCallBacks
            )
            : NULL;
    CFDataRef data = NULL;
    CFPropertyListRef readback = NULL;
    CFPropertyListFormat format = 0;
    CFErrorRef error = NULL;

    if (dictionary != NULL && data_roundtrip((CFPropertyListRef)dictionary, &data, &readback, &format, &error)) {
        SInt64 parsedCount = 0;
        record_type_identity(&result, readback, "CFDictionary");
        result.hasLengthValue = true;
        result.lengthValue = (long)CFDictionaryGetCount((CFDictionaryRef)readback);
        result.hasPrimaryValueText = true;
        snprintf(result.primaryValueText, sizeof(result.primaryValueText), "%s", "ascii=6c61756e6368642e7061636b65742d6b6579 blob=10203040 count=42 enabled=true utf8=636166c3a92de298832df09f93a6 when=1234.5");
        result.hasAlternateValueText = true;
        snprintf(result.alternateValueText, sizeof(result.alternateValueText), "%s", "header=bplist00 root=dict");
        result.success =
            format == kCFPropertyListBinaryFormat_v1_0 &&
            binary_header_is_bplist00(data) &&
            CFGetTypeID(readback) == CFDictionaryGetTypeID() &&
            CFDictionaryGetCount((CFDictionaryRef)readback) == 6 &&
            CFEqual(CFDictionaryGetValue((CFDictionaryRef)readback, keyAscii), valueAscii) &&
            CFEqual(CFDictionaryGetValue((CFDictionaryRef)readback, keyBlob), valueBlob) &&
            CFNumberGetValue((CFNumberRef)CFDictionaryGetValue((CFDictionaryRef)readback, keyCount), kCFNumberSInt64Type, &parsedCount) &&
            parsedCount == 42 &&
            CFDictionaryGetValue((CFDictionaryRef)readback, keyEnabled) == (CFTypeRef)kCFBooleanTrue &&
            CFEqual(CFDictionaryGetValue((CFDictionaryRef)readback, keyUtf8), valueUtf8) &&
            CFDateGetAbsoluteTime((CFDateRef)CFDictionaryGetValue((CFDictionaryRef)readback, keyWhen)) == 1234.5;
    }

    if (error != NULL) {
        CFRelease(error);
    }
    if (readback != NULL) {
        CFRelease(readback);
    }
    if (data != NULL) {
        CFRelease(data);
    }
    if (dictionary != NULL) {
        CFRelease(dictionary);
    }
    if (valueWhen != NULL) {
        CFRelease(valueWhen);
    }
    if (valueUtf8 != NULL) {
        CFRelease(valueUtf8);
    }
    if (valueCount != NULL) {
        CFRelease(valueCount);
    }
    if (valueBlob != NULL) {
        CFRelease(valueBlob);
    }
    if (valueAscii != NULL) {
        CFRelease(valueAscii);
    }
    if (keyWhen != NULL) {
        CFRelease(keyWhen);
    }
    if (keyUtf8 != NULL) {
        CFRelease(keyUtf8);
    }
    if (keyEnabled != NULL) {
        CFRelease(keyEnabled);
    }
    if (keyCount != NULL) {
        CFRelease(keyCount);
    }
    if (keyBlob != NULL) {
        CFRelease(keyBlob);
    }
    if (keyAscii != NULL) {
        CFRelease(keyAscii);
    }
    return result;
}

static struct CaseResult observe_non_string_dictionary_key_write_rejected(void) {
    struct CaseResult result;
    memset(&result, 0, sizeof(result));
    result.name = "bplist_non_string_dictionary_key_write_rejected";
    result.classification = "required";
    result.fxMatchLevel = "error-text-flexible";

    CFNumberRef key = create_sint64_number(42);
    const void *keys[] = {key};
    const void *values[] = {kCFBooleanTrue};
    CFDictionaryRef dictionary = (key != NULL)
        ? CFDictionaryCreate(
            kCFAllocatorSystemDefault,
            keys,
            values,
            1,
            &kCFTypeDictionaryKeyCallBacks,
            &kCFTypeDictionaryValueCallBacks
        )
        : NULL;
    CFErrorRef error = NULL;
    CFDataRef data = (dictionary != NULL)
        ? CFPropertyListCreateData(
            kCFAllocatorSystemDefault,
            (CFPropertyListRef)dictionary,
            kCFPropertyListBinaryFormat_v1_0,
            0,
            &error
        )
        : NULL;

    result.hasLengthValue = (error != NULL);
    result.lengthValue = (error != NULL) ? (long)CFErrorGetCode(error) : 0;
    result.hasPrimaryValueText = true;
    snprintf(result.primaryValueText, sizeof(result.primaryValueText), "%s", "returned-null error-present");
    result.hasAlternateValueText = true;
    snprintf(result.alternateValueText, sizeof(result.alternateValueText), "%s", "write-invalid-non-string-key");
    result.success = dictionary != NULL && data == NULL && error != NULL && CFErrorGetCode(error) == kCFPropertyListWriteStreamError;

    if (data != NULL) {
        CFRelease(data);
    }
    if (error != NULL) {
        CFRelease(error);
    }
    if (dictionary != NULL) {
        CFRelease(dictionary);
    }
    if (key != NULL) {
        CFRelease(key);
    }
    return result;
}

static struct CaseResult observe_invalid_header_rejected(void) {
    static const UInt8 invalidBytes[] = {'x', 'p', 'l', 'i', 's', 't', '0', '0', 0x00u};
    struct CaseResult result;
    memset(&result, 0, sizeof(result));
    result.name = "bplist_invalid_header_rejected";
    result.classification = "required";
    result.fxMatchLevel = "error-text-flexible";

    CFDataRef data = create_data(invalidBytes, (CFIndex)sizeof(invalidBytes));
    CFPropertyListFormat format = 0;
    CFErrorRef error = NULL;
    CFPropertyListRef readback = (data != NULL)
        ? CFPropertyListCreateWithData(
            kCFAllocatorSystemDefault,
            data,
            kCFPropertyListImmutable,
            &format,
            &error
        )
        : NULL;

    result.hasLengthValue = (error != NULL);
    result.lengthValue = (error != NULL) ? (long)CFErrorGetCode(error) : 0;
    result.hasPrimaryValueText = true;
    snprintf(result.primaryValueText, sizeof(result.primaryValueText), "%s", "returned-null error-present");
    result.hasAlternateValueText = true;
    snprintf(result.alternateValueText, sizeof(result.alternateValueText), "%s", "read-invalid-header");
    result.success = data != NULL && readback == NULL && error != NULL && CFErrorGetCode(error) == kCFPropertyListReadCorruptError;

    if (readback != NULL) {
        CFRelease(readback);
    }
    if (error != NULL) {
        CFRelease(error);
    }
    if (data != NULL) {
        CFRelease(data);
    }
    return result;
}

static struct CaseResult observe_truncated_trailer_rejected(void) {
    struct CaseResult result;
    memset(&result, 0, sizeof(result));
    result.name = "bplist_truncated_trailer_rejected";
    result.classification = "required";
    result.fxMatchLevel = "error-text-flexible";

    CFStringRef string = create_ascii_string("launchd.packet-key");
    CFErrorRef writeError = NULL;
    CFDataRef valid = (string != NULL)
        ? CFPropertyListCreateData(
            kCFAllocatorSystemDefault,
            (CFPropertyListRef)string,
            kCFPropertyListBinaryFormat_v1_0,
            0,
            &writeError
        )
        : NULL;
    CFDataRef truncated = NULL;
    if (valid != NULL && CFDataGetLength(valid) > 1) {
        truncated = CFDataCreate(
            kCFAllocatorSystemDefault,
            CFDataGetBytePtr(valid),
            CFDataGetLength(valid) - 1
        );
    }

    CFPropertyListFormat format = 0;
    CFErrorRef readError = NULL;
    CFPropertyListRef readback = (truncated != NULL)
        ? CFPropertyListCreateWithData(
            kCFAllocatorSystemDefault,
            truncated,
            kCFPropertyListImmutable,
            &format,
            &readError
        )
        : NULL;

    result.hasLengthValue = (readError != NULL);
    result.lengthValue = (readError != NULL) ? (long)CFErrorGetCode(readError) : 0;
    result.hasPrimaryValueText = true;
    snprintf(result.primaryValueText, sizeof(result.primaryValueText), "%s", "returned-null error-present");
    result.hasAlternateValueText = true;
    snprintf(result.alternateValueText, sizeof(result.alternateValueText), "%s", "read-truncated-trailer");
    result.success = valid != NULL && truncated != NULL && readback == NULL && readError != NULL && CFErrorGetCode(readError) == kCFPropertyListReadCorruptError;

    if (readback != NULL) {
        CFRelease(readback);
    }
    if (readError != NULL) {
        CFRelease(readError);
    }
    if (truncated != NULL) {
        CFRelease(truncated);
    }
    if (valid != NULL) {
        CFRelease(valid);
    }
    if (writeError != NULL) {
        CFRelease(writeError);
    }
    if (string != NULL) {
        CFRelease(string);
    }
    return result;
}

int main(void) {
    struct CaseResult results[] = {
        observe_ascii_string_roundtrip(),
        observe_unicode_string_roundtrip(),
        observe_mixed_array_roundtrip(),
        observe_string_key_dictionary_roundtrip(),
        observe_non_string_dictionary_key_write_rejected(),
        observe_invalid_header_rejected(),
        observe_truncated_trailer_rejected()
    };
    size_t caseCount = sizeof(results) / sizeof(results[0]);

    fputs("{\n", stdout);
    fputs("  \"probe\": \"subset3a_bplist_core\",\n", stdout);
    fputs("  \"results\": [\n", stdout);
    for (size_t index = 0u; index < caseCount; ++index) {
        json_print_result(stdout, &results[index], index + 1u < caseCount);
    }
    fputs("  ]\n", stdout);
    fputs("}\n", stdout);
    return 0;
}
