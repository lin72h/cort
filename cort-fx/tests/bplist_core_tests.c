#include <CoreFoundation/CoreFoundation.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void fail(const char *message) {
    fprintf(stderr, "FAIL: %s\n", message);
    exit(1);
}

static void expect_true(Boolean condition, const char *message) {
    if (!condition) {
        fail(message);
    }
}

static void expect_long(CFIndex actual, CFIndex expected, const char *message) {
    if (actual != expected) {
        fprintf(stderr, "FAIL: %s (actual=%ld expected=%ld)\n", message, (long)actual, (long)expected);
        exit(1);
    }
}

static CFStringRef create_ascii_string(const char *text, const char *message) {
    CFStringRef string = CFStringCreateWithCString(kCFAllocatorSystemDefault, text, kCFStringEncodingASCII);
    if (string == NULL) {
        fail(message);
    }
    return string;
}

static CFStringRef create_utf8_string(const char *text, const char *message) {
    CFStringRef string = CFStringCreateWithCString(kCFAllocatorSystemDefault, text, kCFStringEncodingUTF8);
    if (string == NULL) {
        fail(message);
    }
    return string;
}

static CFDataRef create_data(const UInt8 *bytes, CFIndex length, const char *message) {
    CFDataRef data = CFDataCreate(kCFAllocatorSystemDefault, bytes, length);
    if (data == NULL) {
        fail(message);
    }
    return data;
}

static CFNumberRef create_sint64(SInt64 value, const char *message) {
    CFNumberRef number = CFNumberCreate(kCFAllocatorSystemDefault, kCFNumberSInt64Type, &value);
    if (number == NULL) {
        fail(message);
    }
    return number;
}

static CFNumberRef create_float64(double value, const char *message) {
    CFNumberRef number = CFNumberCreate(kCFAllocatorSystemDefault, kCFNumberFloat64Type, &value);
    if (number == NULL) {
        fail(message);
    }
    return number;
}

static void expect_bplist_header(CFDataRef data, const char *message) {
    static const UInt8 header[] = {'b', 'p', 'l', 'i', 's', 't', '0', '0'};
    expect_true(CFDataGetLength(data) >= (CFIndex)sizeof(header), message);
    expect_true(memcmp(CFDataGetBytePtr(data), header, sizeof(header)) == 0, message);
}

static void test_ascii_string_roundtrip(void) {
    CFStringRef string = create_ascii_string("launchd.packet-key", "ascii string create failed");
    CFErrorRef writeError = NULL;
    CFDataRef data = CFPropertyListCreateData(
        kCFAllocatorSystemDefault,
        (CFPropertyListRef)string,
        kCFPropertyListBinaryFormat_v1_0,
        0,
        &writeError
    );
    CFPropertyListFormat format = 0;
    CFErrorRef readError = NULL;
    CFPropertyListRef readback = NULL;

    expect_true(data != NULL, "ascii string plist write failed");
    expect_true(writeError == NULL, "ascii string plist write should not set error");
    expect_bplist_header(data, "ascii string plist header mismatch");

    readback = CFPropertyListCreateWithData(
        kCFAllocatorSystemDefault,
        data,
        kCFPropertyListImmutable,
        &format,
        &readError
    );

    expect_true(readback != NULL, "ascii string plist read failed");
    expect_true(readError == NULL, "ascii string plist read should not set error");
    expect_long(format, kCFPropertyListBinaryFormat_v1_0, "ascii string plist format mismatch");
    expect_long((CFIndex)CFGetTypeID(readback), (CFIndex)CFStringGetTypeID(), "ascii string readback type mismatch");
    expect_true(CFEqual((CFTypeRef)string, readback), "ascii string readback semantic mismatch");

    CFRelease(readback);
    CFRelease(data);
    CFRelease(string);
}

static void test_unicode_string_roundtrip(void) {
    CFStringRef string = create_utf8_string("caf\xC3\xA9-\xE2\x98\x83-\xF0\x9F\x93\xA6", "unicode string create failed");
    CFErrorRef writeError = NULL;
    CFDataRef data = CFPropertyListCreateData(
        kCFAllocatorSystemDefault,
        (CFPropertyListRef)string,
        kCFPropertyListBinaryFormat_v1_0,
        0,
        &writeError
    );
    CFPropertyListFormat format = 0;
    CFErrorRef readError = NULL;
    CFPropertyListRef readback = NULL;

    expect_true(data != NULL, "unicode string plist write failed");
    expect_true(writeError == NULL, "unicode string plist write should not set error");
    expect_bplist_header(data, "unicode string plist header mismatch");

    readback = CFPropertyListCreateWithData(
        kCFAllocatorSystemDefault,
        data,
        kCFPropertyListImmutable,
        &format,
        &readError
    );

    expect_true(readback != NULL, "unicode string plist read failed");
    expect_true(readError == NULL, "unicode string plist read should not set error");
    expect_long(format, kCFPropertyListBinaryFormat_v1_0, "unicode string plist format mismatch");
    expect_true(CFEqual((CFTypeRef)string, readback), "unicode string readback semantic mismatch");

    CFRelease(readback);
    CFRelease(data);
    CFRelease(string);
}

static void test_mixed_array_roundtrip(void) {
    static const UInt8 bytes[] = {0x10u, 0x20u, 0x30u, 0x40u};
    SInt64 intValue = 42;
    double realValue = 42.25;
    CFNumberRef intNumber = create_sint64(intValue, "array int create failed");
    CFNumberRef realNumber = create_float64(realValue, "array real create failed");
    CFDateRef date = CFDateCreate(kCFAllocatorSystemDefault, 1234.5);
    CFDataRef dataValue = create_data(bytes, (CFIndex)sizeof(bytes), "array data create failed");
    CFStringRef string = create_ascii_string("launchd.packet-key", "array string create failed");
    const void *values[] = {kCFBooleanTrue, intNumber, realNumber, date, dataValue, string};
    CFArrayRef array = CFArrayCreate(kCFAllocatorSystemDefault, values, 6, &kCFTypeArrayCallBacks);
    CFErrorRef writeError = NULL;
    CFDataRef data = NULL;
    CFPropertyListRef readback = NULL;
    CFPropertyListFormat format = 0;
    CFErrorRef readError = NULL;
    SInt64 parsedInt = 0;
    double parsedReal = 0.0;

    expect_true(date != NULL, "array date create failed");
    expect_true(array != NULL, "array create failed");

    data = CFPropertyListCreateData(
        kCFAllocatorSystemDefault,
        (CFPropertyListRef)array,
        kCFPropertyListBinaryFormat_v1_0,
        0,
        &writeError
    );
    expect_true(data != NULL, "array plist write failed");
    expect_true(writeError == NULL, "array plist write should not set error");
    expect_bplist_header(data, "array plist header mismatch");

    readback = CFPropertyListCreateWithData(
        kCFAllocatorSystemDefault,
        data,
        kCFPropertyListImmutable,
        &format,
        &readError
    );
    expect_true(readback != NULL, "array plist read failed");
    expect_true(readError == NULL, "array plist read should not set error");
    expect_long(format, kCFPropertyListBinaryFormat_v1_0, "array plist format mismatch");
    expect_long(CFArrayGetCount((CFArrayRef)readback), 6, "array readback count mismatch");
    expect_true(CFArrayGetValueAtIndex((CFArrayRef)readback, 0) == (CFTypeRef)kCFBooleanTrue, "array bool readback mismatch");
    expect_true(CFNumberGetValue((CFNumberRef)CFArrayGetValueAtIndex((CFArrayRef)readback, 1), kCFNumberSInt64Type, &parsedInt), "array int readback convert failed");
    expect_long(parsedInt, intValue, "array int readback mismatch");
    expect_true(CFNumberGetValue((CFNumberRef)CFArrayGetValueAtIndex((CFArrayRef)readback, 2), kCFNumberFloat64Type, &parsedReal), "array real readback convert failed");
    expect_true(parsedReal == realValue, "array real readback mismatch");
    expect_true(CFDateGetAbsoluteTime((CFDateRef)CFArrayGetValueAtIndex((CFArrayRef)readback, 3)) == 1234.5, "array date readback mismatch");
    expect_true(CFEqual((CFTypeRef)CFArrayGetValueAtIndex((CFArrayRef)readback, 4), (CFTypeRef)dataValue), "array data readback mismatch");
    expect_true(CFEqual((CFTypeRef)CFArrayGetValueAtIndex((CFArrayRef)readback, 5), (CFTypeRef)string), "array string readback mismatch");

    CFRelease(readback);
    CFRelease(data);
    CFRelease(array);
    CFRelease(string);
    CFRelease(dataValue);
    CFRelease(date);
    CFRelease(realNumber);
    CFRelease(intNumber);
}

static void test_string_key_dictionary_roundtrip(void) {
    static const UInt8 bytes[] = {0x10u, 0x20u, 0x30u, 0x40u};
    CFStringRef keyAscii = create_ascii_string("ascii", "dict key ascii create failed");
    CFStringRef keyBlob = create_ascii_string("blob", "dict key blob create failed");
    CFStringRef keyCount = create_ascii_string("count", "dict key count create failed");
    CFStringRef keyEnabled = create_ascii_string("enabled", "dict key enabled create failed");
    CFStringRef keyUtf8 = create_ascii_string("utf8", "dict key utf8 create failed");
    CFStringRef keyWhen = create_ascii_string("when", "dict key when create failed");
    CFStringRef valueAscii = create_ascii_string("launchd.packet-key", "dict value ascii create failed");
    CFDataRef valueBlob = create_data(bytes, (CFIndex)sizeof(bytes), "dict value blob create failed");
    CFNumberRef valueCount = create_sint64(42, "dict value count create failed");
    CFStringRef valueUtf8 = create_utf8_string("caf\xC3\xA9-\xE2\x98\x83-\xF0\x9F\x93\xA6", "dict value utf8 create failed");
    CFDateRef valueWhen = CFDateCreate(kCFAllocatorSystemDefault, 1234.5);
    const void *keys[] = {keyAscii, keyBlob, keyCount, keyEnabled, keyUtf8, keyWhen};
    const void *values[] = {valueAscii, valueBlob, valueCount, kCFBooleanTrue, valueUtf8, valueWhen};
    CFDictionaryRef dictionary = CFDictionaryCreate(
        kCFAllocatorSystemDefault,
        keys,
        values,
        6,
        &kCFTypeDictionaryKeyCallBacks,
        &kCFTypeDictionaryValueCallBacks
    );
    CFErrorRef writeError = NULL;
    CFDataRef data = NULL;
    CFPropertyListRef readback = NULL;
    CFPropertyListFormat format = 0;
    CFErrorRef readError = NULL;
    SInt64 parsedCount = 0;

    expect_true(valueWhen != NULL, "dict value when create failed");
    expect_true(dictionary != NULL, "dict create failed");

    data = CFPropertyListCreateData(
        kCFAllocatorSystemDefault,
        (CFPropertyListRef)dictionary,
        kCFPropertyListBinaryFormat_v1_0,
        0,
        &writeError
    );
    expect_true(data != NULL, "dict plist write failed");
    expect_true(writeError == NULL, "dict plist write should not set error");
    expect_bplist_header(data, "dict plist header mismatch");

    readback = CFPropertyListCreateWithData(
        kCFAllocatorSystemDefault,
        data,
        kCFPropertyListImmutable,
        &format,
        &readError
    );
    expect_true(readback != NULL, "dict plist read failed");
    expect_true(readError == NULL, "dict plist read should not set error");
    expect_long(format, kCFPropertyListBinaryFormat_v1_0, "dict plist format mismatch");
    expect_long(CFDictionaryGetCount((CFDictionaryRef)readback), 6, "dict readback count mismatch");
    expect_true(CFEqual(CFDictionaryGetValue((CFDictionaryRef)readback, keyAscii), valueAscii), "dict ascii readback mismatch");
    expect_true(CFEqual(CFDictionaryGetValue((CFDictionaryRef)readback, keyBlob), valueBlob), "dict blob readback mismatch");
    expect_true(CFNumberGetValue((CFNumberRef)CFDictionaryGetValue((CFDictionaryRef)readback, keyCount), kCFNumberSInt64Type, &parsedCount), "dict count convert failed");
    expect_long(parsedCount, 42, "dict count readback mismatch");
    expect_true(CFDictionaryGetValue((CFDictionaryRef)readback, keyEnabled) == (CFTypeRef)kCFBooleanTrue, "dict enabled readback mismatch");
    expect_true(CFEqual(CFDictionaryGetValue((CFDictionaryRef)readback, keyUtf8), valueUtf8), "dict utf8 readback mismatch");
    expect_true(CFDateGetAbsoluteTime((CFDateRef)CFDictionaryGetValue((CFDictionaryRef)readback, keyWhen)) == 1234.5, "dict when readback mismatch");

    CFRelease(readback);
    CFRelease(data);
    CFRelease(dictionary);
    CFRelease(valueWhen);
    CFRelease(valueUtf8);
    CFRelease(valueCount);
    CFRelease(valueBlob);
    CFRelease(valueAscii);
    CFRelease(keyWhen);
    CFRelease(keyUtf8);
    CFRelease(keyEnabled);
    CFRelease(keyCount);
    CFRelease(keyBlob);
    CFRelease(keyAscii);
}

static void test_non_string_dictionary_key_write_rejected(void) {
    CFNumberRef key = create_sint64(42, "non-string key create failed");
    const void *keys[] = {key};
    const void *values[] = {kCFBooleanTrue};
    CFDictionaryRef dictionary = CFDictionaryCreate(
        kCFAllocatorSystemDefault,
        keys,
        values,
        1,
        &kCFTypeDictionaryKeyCallBacks,
        &kCFTypeDictionaryValueCallBacks
    );
    CFErrorRef error = NULL;
    CFDataRef data = NULL;

    expect_true(dictionary != NULL, "non-string-key dictionary create failed");

    data = CFPropertyListCreateData(
        kCFAllocatorSystemDefault,
        (CFPropertyListRef)dictionary,
        kCFPropertyListBinaryFormat_v1_0,
        0,
        &error
    );

    expect_true(data == NULL, "non-string-key dictionary write should fail");
    expect_true(error != NULL, "non-string-key dictionary write should set error");
    expect_long(CFErrorGetCode(error), kCFPropertyListWriteStreamError, "non-string-key dictionary error code mismatch");

    CFRelease(error);
    CFRelease(dictionary);
    CFRelease(key);
}

static void test_invalid_header_rejected(void) {
    static const UInt8 invalidBytes[] = {'x', 'p', 'l', 'i', 's', 't', '0', '0', 0x00u};
    CFDataRef data = create_data(invalidBytes, (CFIndex)sizeof(invalidBytes), "invalid header data create failed");
    CFPropertyListFormat format = 0;
    CFErrorRef error = NULL;
    CFPropertyListRef readback = CFPropertyListCreateWithData(
        kCFAllocatorSystemDefault,
        data,
        kCFPropertyListImmutable,
        &format,
        &error
    );

    expect_true(readback == NULL, "invalid header should fail");
    expect_true(error != NULL, "invalid header should set error");
    expect_long(CFErrorGetCode(error), kCFPropertyListReadCorruptError, "invalid header error code mismatch");
    expect_long(format, 0, "invalid header format should stay zero");

    CFRelease(error);
    CFRelease(data);
}

static void test_truncated_trailer_rejected(void) {
    CFStringRef string = create_ascii_string("launchd.packet-key", "truncate source string create failed");
    CFErrorRef writeError = NULL;
    CFDataRef valid = CFPropertyListCreateData(
        kCFAllocatorSystemDefault,
        (CFPropertyListRef)string,
        kCFPropertyListBinaryFormat_v1_0,
        0,
        &writeError
    );
    CFDataRef truncated = NULL;
    CFPropertyListFormat format = 0;
    CFErrorRef readError = NULL;
    CFPropertyListRef readback = NULL;

    expect_true(valid != NULL, "truncate source write failed");
    expect_true(writeError == NULL, "truncate source write should not set error");
    truncated = CFDataCreate(
        kCFAllocatorSystemDefault,
        CFDataGetBytePtr(valid),
        CFDataGetLength(valid) - 1
    );
    expect_true(truncated != NULL, "truncate data create failed");

    readback = CFPropertyListCreateWithData(
        kCFAllocatorSystemDefault,
        truncated,
        kCFPropertyListImmutable,
        &format,
        &readError
    );

    expect_true(readback == NULL, "truncated trailer should fail");
    expect_true(readError != NULL, "truncated trailer should set error");
    expect_long(CFErrorGetCode(readError), kCFPropertyListReadCorruptError, "truncated trailer error code mismatch");

    CFRelease(readError);
    CFRelease(truncated);
    CFRelease(valid);
    CFRelease(string);
}

int main(void) {
    test_ascii_string_roundtrip();
    test_unicode_string_roundtrip();
    test_mixed_array_roundtrip();
    test_string_key_dictionary_roundtrip();
    test_non_string_dictionary_key_write_rejected();
    test_invalid_header_rejected();
    test_truncated_trailer_rejected();
    puts("PASS bplist_core_tests");
    return 0;
}
