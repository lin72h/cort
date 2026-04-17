#include <CoreFoundation/CoreFoundation.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    kMaxText = 1024
};

struct CaseResult {
    const char *name;
    const char *classification;
    const char *fxMatchLevel;
    const char *semanticNote;
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
    fputs(",\n      \"ownership_note\": ", stream);
    json_print_string(stream, result->semanticNote);
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

static bool set_utf8_hex_for_string(char *buffer, size_t bufferSize, CFStringRef string) {
    CFIndex length = CFStringGetLength(string);
    CFIndex maxBytes = length * 4 + 1;
    char *utf8 = (char *)malloc((size_t)maxBytes);
    if (utf8 == NULL) {
        return false;
    }
    bool ok = CFStringGetCString(string, utf8, maxBytes, kCFStringEncodingUTF8);
    if (ok) {
        set_hex_bytes(buffer, bufferSize, (const UInt8 *)utf8, strlen(utf8));
    }
    free(utf8);
    return ok;
}

static bool write_fixture(CFStringRef fixturesDir, const char *name, CFDataRef data) {
    char pathBuffer[4096];
    if (!CFStringGetCString(fixturesDir, pathBuffer, (CFIndex)sizeof(pathBuffer), kCFStringEncodingUTF8)) {
        return false;
    }
    size_t used = strlen(pathBuffer);
    if (used + 1 >= sizeof(pathBuffer)) {
        return false;
    }
    if (used > 0 && pathBuffer[used - 1] != '/') {
        pathBuffer[used++] = '/';
        pathBuffer[used] = '\0';
    }
    if (snprintf(pathBuffer + used, sizeof(pathBuffer) - used, "%s", name) >= (int)(sizeof(pathBuffer) - used)) {
        return false;
    }

    FILE *stream = fopen(pathBuffer, "wb");
    if (stream == NULL) {
        return false;
    }
    CFIndex length = CFDataGetLength(data);
    const UInt8 *bytes = CFDataGetBytePtr(data);
    size_t written = fwrite(bytes, 1, (size_t)length, stream);
    fclose(stream);
    return written == (size_t)length;
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

    *dataOut = data;
    *readbackOut = readback;
    *formatOut = format;
    if (readError != NULL) {
        CFRelease(readError);
    }
    return true;
}

static struct CaseResult observe_ascii_string_roundtrip(CFStringRef fixturesDir) {
    struct CaseResult result;
    memset(&result, 0, sizeof(result));
    result.name = "bplist_ascii_string_roundtrip";
    result.classification = "required";
    result.fxMatchLevel = "semantic";
    result.semanticNote = "Binary create/read in immutable mode must preserve an ASCII root string and report binary format.";

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
            CFEqual((CFTypeRef)string, readback) &&
            write_fixture(fixturesDir, "ascii_string_root.bplist", data);
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

static struct CaseResult observe_unicode_string_roundtrip(CFStringRef fixturesDir) {
    struct CaseResult result;
    memset(&result, 0, sizeof(result));
    result.name = "bplist_unicode_string_roundtrip";
    result.classification = "required";
    result.fxMatchLevel = "semantic";
    result.semanticNote = "Binary create/read in immutable mode must preserve a non-ASCII root string and use the UTF-16 string path for the tested case.";

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
            CFEqual((CFTypeRef)string, readback) &&
            write_fixture(fixturesDir, "unicode_string_root.bplist", data);
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

static struct CaseResult observe_mixed_array_roundtrip(CFStringRef fixturesDir) {
    static const UInt8 bytes[] = {0x10u, 0x20u, 0x30u, 0x40u};

    struct CaseResult result;
    memset(&result, 0, sizeof(result));
    result.name = "bplist_mixed_array_roundtrip";
    result.classification = "required";
    result.fxMatchLevel = "semantic";
    result.semanticNote = "Binary create/read in immutable mode must preserve the supported mixed scalar array surface.";

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
        CFTypeRef element0 = CFArrayGetValueAtIndex(parsed, 0);
        CFNumberRef element1 = (CFNumberRef)CFArrayGetValueAtIndex(parsed, 1);
        CFNumberRef element2 = (CFNumberRef)CFArrayGetValueAtIndex(parsed, 2);
        CFDateRef element3 = (CFDateRef)CFArrayGetValueAtIndex(parsed, 3);
        CFDataRef element4 = (CFDataRef)CFArrayGetValueAtIndex(parsed, 4);
        CFStringRef element5 = (CFStringRef)CFArrayGetValueAtIndex(parsed, 5);
        SInt64 parsedInt = 0;
        double parsedReal = 0.0;
        record_type_identity(&result, readback, "CFArray");
        result.hasLengthValue = true;
        result.lengthValue = (long)CFArrayGetCount(parsed);
        result.hasPrimaryValueText = true;
        snprintf(
            result.primaryValueText,
            sizeof(result.primaryValueText),
            "%s",
            "bool=true int=42 real=42.25 date=1234.5 data=10203040 string=6c61756e6368642e7061636b65742d6b6579"
        );
        result.hasAlternateValueText = true;
        snprintf(result.alternateValueText, sizeof(result.alternateValueText), "%s", "header=bplist00 root=array");
        result.success =
            format == kCFPropertyListBinaryFormat_v1_0 &&
            binary_header_is_bplist00(data) &&
            CFGetTypeID(readback) == CFArrayGetTypeID() &&
            CFArrayGetCount(parsed) == 6 &&
            element0 == (CFTypeRef)kCFBooleanTrue &&
            CFNumberGetValue(element1, kCFNumberSInt64Type, &parsedInt) &&
            parsedInt == intValue &&
            CFNumberGetValue(element2, kCFNumberFloat64Type, &parsedReal) &&
            parsedReal == realValue &&
            CFDateGetAbsoluteTime(element3) == when &&
            CFDataGetLength(element4) == (CFIndex)sizeof(bytes) &&
            memcmp(CFDataGetBytePtr(element4), bytes, sizeof(bytes)) == 0 &&
            CFEqual((CFTypeRef)element5, (CFTypeRef)string) &&
            write_fixture(fixturesDir, "mixed_array_root.bplist", data);
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

static struct CaseResult observe_string_key_dictionary_roundtrip(CFStringRef fixturesDir) {
    static const UInt8 bytes[] = {0x10u, 0x20u, 0x30u, 0x40u};

    struct CaseResult result;
    memset(&result, 0, sizeof(result));
    result.name = "bplist_string_key_dictionary_roundtrip";
    result.classification = "required";
    result.fxMatchLevel = "semantic";
    result.semanticNote = "Binary create/read in immutable mode must preserve the supported string-key dictionary surface.";

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
        CFDictionaryRef parsed = (CFDictionaryRef)readback;
        CFNumberRef parsedCount = (CFNumberRef)CFDictionaryGetValue(parsed, keyCount);
        CFDateRef parsedWhen = (CFDateRef)CFDictionaryGetValue(parsed, keyWhen);
        SInt64 parsedCountValue = 0;
        record_type_identity(&result, readback, "CFDictionary");
        result.hasLengthValue = true;
        result.lengthValue = (long)CFDictionaryGetCount(parsed);
        result.hasPrimaryValueText = true;
        snprintf(
            result.primaryValueText,
            sizeof(result.primaryValueText),
            "%s",
            "ascii=6c61756e6368642e7061636b65742d6b6579 blob=10203040 count=42 enabled=true utf8=636166c3a92de298832df09f93a6 when=1234.5"
        );
        result.hasAlternateValueText = true;
        snprintf(result.alternateValueText, sizeof(result.alternateValueText), "%s", "header=bplist00 root=dict");
        result.success =
            format == kCFPropertyListBinaryFormat_v1_0 &&
            binary_header_is_bplist00(data) &&
            CFGetTypeID(readback) == CFDictionaryGetTypeID() &&
            CFDictionaryGetCount(parsed) == 6 &&
            CFEqual(CFDictionaryGetValue(parsed, keyAscii), valueAscii) &&
            CFEqual(CFDictionaryGetValue(parsed, keyBlob), valueBlob) &&
            parsedCount != NULL &&
            CFNumberGetValue(parsedCount, kCFNumberSInt64Type, &parsedCountValue) &&
            parsedCountValue == 42 &&
            CFDictionaryGetValue(parsed, keyEnabled) == (CFTypeRef)kCFBooleanTrue &&
            CFEqual(CFDictionaryGetValue(parsed, keyUtf8), valueUtf8) &&
            parsedWhen != NULL &&
            CFDateGetAbsoluteTime(parsedWhen) == 1234.5 &&
            write_fixture(fixturesDir, "string_key_dictionary_root.bplist", data);
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
    result.semanticNote = "Binary-plist writing must reject dictionaries with non-string keys safely.";

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

static struct CaseResult observe_invalid_header_rejected(CFStringRef fixturesDir) {
    static const UInt8 invalidBytes[] = {'x', 'p', 'l', 'i', 's', 't', '0', '0', 0x00u};

    struct CaseResult result;
    memset(&result, 0, sizeof(result));
    result.name = "bplist_invalid_header_rejected";
    result.classification = "required";
    result.fxMatchLevel = "error-text-flexible";
    result.semanticNote = "Binary-plist reading must reject an invalid header safely.";

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
    result.success =
        data != NULL &&
        readback == NULL &&
        error != NULL &&
        CFErrorGetCode(error) == kCFPropertyListReadCorruptError &&
        write_fixture(fixturesDir, "invalid_header.bplist", data);

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

static struct CaseResult observe_truncated_trailer_rejected(CFStringRef fixturesDir) {
    struct CaseResult result;
    memset(&result, 0, sizeof(result));
    result.name = "bplist_truncated_trailer_rejected";
    result.classification = "required";
    result.fxMatchLevel = "error-text-flexible";
    result.semanticNote = "Binary-plist reading must reject truncated trailer data safely.";

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
    result.success =
        valid != NULL &&
        truncated != NULL &&
        readback == NULL &&
        readError != NULL &&
        CFErrorGetCode(readError) == kCFPropertyListReadCorruptError &&
        write_fixture(fixturesDir, "truncated_trailer.bplist", truncated);

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

int main(int argc, const char *argv[]) {
    CFStringRef fixturesDir = NULL;
    if (argc > 1) {
        fixturesDir = CFStringCreateWithCString(kCFAllocatorSystemDefault, argv[1], kCFStringEncodingUTF8);
    }
    if (fixturesDir == NULL) {
        fixturesDir = CFStringCreateWithCString(kCFAllocatorSystemDefault, ".", kCFStringEncodingUTF8);
    }

    struct CaseResult results[] = {
        observe_ascii_string_roundtrip(fixturesDir),
        observe_unicode_string_roundtrip(fixturesDir),
        observe_mixed_array_roundtrip(fixturesDir),
        observe_string_key_dictionary_roundtrip(fixturesDir),
        observe_non_string_dictionary_key_write_rejected(),
        observe_invalid_header_rejected(fixturesDir),
        observe_truncated_trailer_rejected(fixturesDir)
    };
    size_t caseCount = sizeof(results) / sizeof(results[0]);

    fputs("{\n", stdout);
    fputs("  \"probe\": \"subset3a_bplist_core\",\n", stdout);
    fputs("  \"results\": [\n", stdout);
    for (size_t index = 0; index < caseCount; ++index) {
        json_print_result(stdout, &results[index], index + 1 < caseCount);
    }
    fputs("  ]\n", stdout);
    fputs("}\n", stdout);

    CFRelease(fixturesDir);
    return 0;
}
