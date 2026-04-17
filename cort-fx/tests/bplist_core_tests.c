#include <CoreFoundation/CoreFoundation.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct UnsupportedPropertyListObject {
    CFRuntimeBase base;
};

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

static CFNumberRef create_sint64_number(SInt64 value, const char *message) {
    CFNumberRef number = CFNumberCreate(kCFAllocatorSystemDefault, kCFNumberSInt64Type, &value);
    if (number == NULL) {
        fail(message);
    }
    return number;
}

static CFNumberRef create_float64_number(double value, const char *message) {
    CFNumberRef number = CFNumberCreate(kCFAllocatorSystemDefault, kCFNumberFloat64Type, &value);
    if (number == NULL) {
        fail(message);
    }
    return number;
}

static Boolean binary_header_is_bplist00(CFDataRef data) {
    static const UInt8 header[] = {'b', 'p', 'l', 'i', 's', 't', '0', '0'};
    return CFDataGetLength(data) >= 8 && memcmp(CFDataGetBytePtr(data), header, sizeof(header)) == 0;
}

static void expect_error_code(CFErrorRef error, CFIndex code, const char *message) {
    expect_true(error != NULL, message);
    expect_long((CFIndex)CFGetTypeID((CFTypeRef)error), (CFIndex)CFErrorGetTypeID(), "CFError type ID mismatch");
    expect_long(CFErrorGetCode(error), code, message);
}

static CFTypeID register_test_type(const char *name) {
    static CFRuntimeClass clsStorage[8];
    static int used = 0;

    if (used >= 8) {
        fail("too many bplist runtime classes");
    }

    clsStorage[used].version = 0;
    clsStorage[used].className = name;
    clsStorage[used].init = NULL;
    clsStorage[used].copy = NULL;
    clsStorage[used].finalize = NULL;
    clsStorage[used].equal = NULL;
    clsStorage[used].hash = NULL;
    clsStorage[used].copyFormattingDesc = NULL;
    clsStorage[used].copyDebugDesc = NULL;
    clsStorage[used].reclaim = NULL;
    clsStorage[used].refcount = NULL;
    clsStorage[used].requiredAlignment = 0;

    CFTypeID typeID = _CFRuntimeRegisterClass(&clsStorage[used]);
    if (typeID == _kCFRuntimeNotATypeID) {
        fail("runtime class registration failed");
    }
    used += 1;
    return typeID;
}

static struct UnsupportedPropertyListObject *create_unsupported_property_list_object(const char *name) {
    CFTypeID typeID = register_test_type(name);
    struct UnsupportedPropertyListObject *object =
        (struct UnsupportedPropertyListObject *)_CFRuntimeCreateInstance(
            kCFAllocatorSystemDefault,
            typeID,
            (CFIndex)(sizeof(struct UnsupportedPropertyListObject) - sizeof(CFRuntimeBase)),
            NULL
        );
    if (object == NULL) {
        fail("unsupported property-list object allocation failed");
    }
    return object;
}

static void test_ascii_string_roundtrip(void) {
    CFStringRef string = create_ascii_string("launchd.packet-key", "ASCII string create failed");
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
    CFPropertyListRef readback = CFPropertyListCreateWithData(
        kCFAllocatorSystemDefault,
        data,
        kCFPropertyListImmutable,
        &format,
        &readError
    );

    expect_true(data != NULL, "ASCII write failed");
    expect_true(writeError == NULL, "ASCII write error must be NULL");
    expect_true(binary_header_is_bplist00(data), "ASCII binary header mismatch");
    expect_true(readback != NULL, "ASCII readback failed");
    expect_true(readError == NULL, "ASCII read error must be NULL");
    expect_long((CFIndex)format, (CFIndex)kCFPropertyListBinaryFormat_v1_0, "ASCII format mismatch");
    expect_long((CFIndex)CFGetTypeID(readback), (CFIndex)CFStringGetTypeID(), "ASCII readback type mismatch");
    expect_long(CFStringGetLength((CFStringRef)readback), 18, "ASCII readback length mismatch");
    expect_true(CFEqual((CFTypeRef)string, readback), "ASCII roundtrip value mismatch");

    CFRelease(readback);
    CFRelease(data);
    CFRelease((CFTypeRef)string);
}

static void test_unicode_string_roundtrip(void) {
    CFStringRef string = create_utf8_string("caf\xC3\xA9-\xE2\x98\x83-\xF0\x9F\x93\xA6", "UTF-8 string create failed");
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
    CFPropertyListRef readback = CFPropertyListCreateWithData(
        kCFAllocatorSystemDefault,
        data,
        kCFPropertyListImmutable,
        &format,
        &readError
    );

    expect_true(data != NULL, "UTF-8 write failed");
    expect_true(writeError == NULL, "UTF-8 write error must be NULL");
    expect_true(binary_header_is_bplist00(data), "UTF-8 binary header mismatch");
    expect_true(readback != NULL, "UTF-8 readback failed");
    expect_true(readError == NULL, "UTF-8 read error must be NULL");
    expect_long((CFIndex)format, (CFIndex)kCFPropertyListBinaryFormat_v1_0, "UTF-8 format mismatch");
    expect_long((CFIndex)CFGetTypeID(readback), (CFIndex)CFStringGetTypeID(), "UTF-8 readback type mismatch");
    expect_long(CFStringGetLength((CFStringRef)readback), 9, "UTF-8 readback length mismatch");
    expect_true(CFEqual((CFTypeRef)string, readback), "UTF-8 roundtrip value mismatch");

    CFRelease(readback);
    CFRelease(data);
    CFRelease((CFTypeRef)string);
}

static void test_mixed_array_roundtrip(void) {
    static const UInt8 bytes[] = {0x10u, 0x20u, 0x30u, 0x40u};

    CFNumberRef intNumber = create_sint64_number(42, "int number create failed");
    CFNumberRef realNumber = create_float64_number(42.25, "real number create failed");
    CFDateRef date = CFDateCreate(kCFAllocatorSystemDefault, 1234.5);
    CFDataRef dataValue = create_data(bytes, (CFIndex)sizeof(bytes), "array data create failed");
    CFStringRef string = create_ascii_string("launchd.packet-key", "array string create failed");
    const void *values[] = {kCFBooleanTrue, intNumber, realNumber, date, dataValue, string};
    CFArrayRef array = CFArrayCreate(kCFAllocatorSystemDefault, values, 6, &kCFTypeArrayCallBacks);

    expect_true(date != NULL, "date create failed");
    expect_true(array != NULL, "array create failed");

    CFErrorRef writeError = NULL;
    CFDataRef data = CFPropertyListCreateData(
        kCFAllocatorSystemDefault,
        (CFPropertyListRef)array,
        kCFPropertyListBinaryFormat_v1_0,
        0,
        &writeError
    );
    CFPropertyListFormat format = 0;
    CFErrorRef readError = NULL;
    CFPropertyListRef readback = CFPropertyListCreateWithData(
        kCFAllocatorSystemDefault,
        data,
        kCFPropertyListImmutable,
        &format,
        &readError
    );

    expect_true(data != NULL, "mixed array write failed");
    expect_true(writeError == NULL, "mixed array write error must be NULL");
    expect_true(readback != NULL, "mixed array readback failed");
    expect_true(readError == NULL, "mixed array read error must be NULL");
    expect_true(binary_header_is_bplist00(data), "mixed array binary header mismatch");
    expect_long((CFIndex)format, (CFIndex)kCFPropertyListBinaryFormat_v1_0, "mixed array format mismatch");
    expect_long((CFIndex)CFGetTypeID(readback), (CFIndex)CFArrayGetTypeID(), "mixed array type mismatch");
    expect_long(CFArrayGetCount((CFArrayRef)readback), 6, "mixed array count mismatch");
    expect_true(CFArrayGetValueAtIndex((CFArrayRef)readback, 0) == (CFTypeRef)kCFBooleanTrue, "mixed array boolean mismatch");

    SInt64 parsedInt = 0;
    double parsedReal = 0.0;
    expect_true(
        CFNumberGetValue((CFNumberRef)CFArrayGetValueAtIndex((CFArrayRef)readback, 1), kCFNumberSInt64Type, &parsedInt),
        "mixed array integer conversion failed"
    );
    expect_long(parsedInt, 42, "mixed array integer mismatch");
    expect_true(
        CFNumberGetValue((CFNumberRef)CFArrayGetValueAtIndex((CFArrayRef)readback, 2), kCFNumberFloat64Type, &parsedReal),
        "mixed array real conversion failed"
    );
    expect_true(parsedReal == 42.25, "mixed array real mismatch");
    expect_true(
        CFDateGetAbsoluteTime((CFDateRef)CFArrayGetValueAtIndex((CFArrayRef)readback, 3)) == 1234.5,
        "mixed array date mismatch"
    );
    expect_long(
        CFDataGetLength((CFDataRef)CFArrayGetValueAtIndex((CFArrayRef)readback, 4)),
        (CFIndex)sizeof(bytes),
        "mixed array data length mismatch"
    );
    expect_true(
        memcmp(CFDataGetBytePtr((CFDataRef)CFArrayGetValueAtIndex((CFArrayRef)readback, 4)), bytes, sizeof(bytes)) == 0,
        "mixed array data payload mismatch"
    );
    expect_true(
        CFEqual(CFArrayGetValueAtIndex((CFArrayRef)readback, 5), (CFTypeRef)string),
        "mixed array string mismatch"
    );

    CFRelease(readback);
    CFRelease(data);
    CFRelease((CFTypeRef)array);
    CFRelease((CFTypeRef)string);
    CFRelease((CFTypeRef)dataValue);
    CFRelease((CFTypeRef)date);
    CFRelease((CFTypeRef)realNumber);
    CFRelease((CFTypeRef)intNumber);
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
    CFNumberRef valueCount = create_sint64_number(42, "dict value count create failed");
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

    expect_true(valueWhen != NULL, "dict value when create failed");
    expect_true(dictionary != NULL, "dictionary create failed");

    CFErrorRef writeError = NULL;
    CFDataRef data = CFPropertyListCreateData(
        kCFAllocatorSystemDefault,
        (CFPropertyListRef)dictionary,
        kCFPropertyListBinaryFormat_v1_0,
        0,
        &writeError
    );
    CFPropertyListFormat format = 0;
    CFErrorRef readError = NULL;
    CFPropertyListRef readback = CFPropertyListCreateWithData(
        kCFAllocatorSystemDefault,
        data,
        kCFPropertyListImmutable,
        &format,
        &readError
    );

    expect_true(data != NULL, "dictionary write failed");
    expect_true(writeError == NULL, "dictionary write error must be NULL");
    expect_true(readback != NULL, "dictionary readback failed");
    expect_true(readError == NULL, "dictionary read error must be NULL");
    expect_true(binary_header_is_bplist00(data), "dictionary binary header mismatch");
    expect_long((CFIndex)format, (CFIndex)kCFPropertyListBinaryFormat_v1_0, "dictionary format mismatch");
    expect_long((CFIndex)CFGetTypeID(readback), (CFIndex)CFDictionaryGetTypeID(), "dictionary type mismatch");
    expect_long(CFDictionaryGetCount((CFDictionaryRef)readback), 6, "dictionary count mismatch");
    expect_true(CFEqual(CFDictionaryGetValue((CFDictionaryRef)readback, keyAscii), (CFTypeRef)valueAscii), "dictionary ascii mismatch");
    expect_true(CFEqual(CFDictionaryGetValue((CFDictionaryRef)readback, keyBlob), (CFTypeRef)valueBlob), "dictionary blob mismatch");

    SInt64 parsedCount = 0;
    expect_true(
        CFNumberGetValue((CFNumberRef)CFDictionaryGetValue((CFDictionaryRef)readback, keyCount), kCFNumberSInt64Type, &parsedCount),
        "dictionary count conversion failed"
    );
    expect_long(parsedCount, 42, "dictionary count mismatch");
    expect_true(
        CFDictionaryGetValue((CFDictionaryRef)readback, keyEnabled) == (CFTypeRef)kCFBooleanTrue,
        "dictionary enabled mismatch"
    );
    expect_true(CFEqual(CFDictionaryGetValue((CFDictionaryRef)readback, keyUtf8), (CFTypeRef)valueUtf8), "dictionary utf8 mismatch");
    expect_true(
        CFDateGetAbsoluteTime((CFDateRef)CFDictionaryGetValue((CFDictionaryRef)readback, keyWhen)) == 1234.5,
        "dictionary when mismatch"
    );

    CFRelease(readback);
    CFRelease(data);
    CFRelease((CFTypeRef)dictionary);
    CFRelease((CFTypeRef)valueWhen);
    CFRelease((CFTypeRef)valueUtf8);
    CFRelease((CFTypeRef)valueCount);
    CFRelease((CFTypeRef)valueBlob);
    CFRelease((CFTypeRef)valueAscii);
    CFRelease((CFTypeRef)keyWhen);
    CFRelease((CFTypeRef)keyUtf8);
    CFRelease((CFTypeRef)keyEnabled);
    CFRelease((CFTypeRef)keyCount);
    CFRelease((CFTypeRef)keyBlob);
    CFRelease((CFTypeRef)keyAscii);
}

static void test_non_string_dictionary_key_rejected(void) {
    SInt64 value = 42;
    CFNumberRef key = create_sint64_number(value, "non-string key create failed");
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
    CFDataRef data = CFPropertyListCreateData(
        kCFAllocatorSystemDefault,
        (CFPropertyListRef)dictionary,
        kCFPropertyListBinaryFormat_v1_0,
        0,
        &error
    );

    expect_true(dictionary != NULL, "non-string-key dictionary create failed");
    expect_true(data == NULL, "non-string-key write should fail");
    expect_error_code(error, kCFPropertyListWriteStreamError, "non-string-key error code mismatch");

    CFRelease(error);
    CFRelease((CFTypeRef)dictionary);
    CFRelease((CFTypeRef)key);
}

static void test_property_list_create_data_null_rejected(void) {
    CFErrorRef error = NULL;
    CFDataRef data = CFPropertyListCreateData(
        kCFAllocatorSystemDefault,
        NULL,
        kCFPropertyListBinaryFormat_v1_0,
        0,
        &error
    );

    expect_true(data == NULL, "property-list write with NULL input should fail");
    expect_error_code(error, kCFPropertyListWriteStreamError, "property-list write NULL error code mismatch");
    CFRelease(error);
}

static void test_property_list_create_data_unsupported_format_rejected(void) {
    CFStringRef string = create_ascii_string("launchd.packet-key", "unsupported-format string create failed");
    CFErrorRef error = NULL;
    CFDataRef data = CFPropertyListCreateData(
        kCFAllocatorSystemDefault,
        (CFPropertyListRef)string,
        kCFPropertyListXMLFormat_v1_0,
        0,
        &error
    );

    expect_true(data == NULL, "property-list write with unsupported format should fail");
    expect_error_code(error, kCFPropertyListWriteStreamError, "property-list unsupported-format error code mismatch");
    CFRelease(error);
    CFRelease((CFTypeRef)string);
}

static void test_property_list_create_data_mutable_options_rejected(void) {
    CFStringRef string = create_ascii_string("launchd.packet-key", "mutable-options string create failed");
    CFErrorRef error = NULL;
    CFDataRef data = CFPropertyListCreateData(
        kCFAllocatorSystemDefault,
        (CFPropertyListRef)string,
        kCFPropertyListBinaryFormat_v1_0,
        kCFPropertyListMutableContainers,
        &error
    );

    expect_true(data == NULL, "property-list write with mutable options should fail");
    expect_error_code(error, kCFPropertyListWriteStreamError, "property-list mutable-options error code mismatch");
    CFRelease(error);
    CFRelease((CFTypeRef)string);
}

static void test_property_list_create_data_unsupported_object_rejected(void) {
    struct UnsupportedPropertyListObject *object = create_unsupported_property_list_object("BplistUnsupportedRoot");
    CFErrorRef error = NULL;
    CFDataRef data = CFPropertyListCreateData(
        kCFAllocatorSystemDefault,
        (CFPropertyListRef)object,
        kCFPropertyListBinaryFormat_v1_0,
        0,
        &error
    );

    expect_true(data == NULL, "property-list write with unsupported object should fail");
    expect_error_code(error, kCFPropertyListWriteStreamError, "property-list unsupported-object error code mismatch");
    CFRelease(error);
    CFRelease((CFTypeRef)object);
}

static void test_property_list_create_with_data_null_rejected(void) {
    CFPropertyListFormat format = 99;
    CFErrorRef error = NULL;
    CFPropertyListRef plist = CFPropertyListCreateWithData(
        kCFAllocatorSystemDefault,
        NULL,
        kCFPropertyListImmutable,
        &format,
        &error
    );

    expect_true(plist == NULL, "property-list read with NULL data should fail");
    expect_error_code(error, kCFPropertyListReadCorruptError, "property-list read NULL error code mismatch");
    expect_long((CFIndex)format, 0, "property-list read NULL should zero format");
    CFRelease(error);
}

static void test_property_list_create_with_data_non_data_rejected(void) {
    CFStringRef string = create_ascii_string("launchd.packet-key", "non-data string create failed");
    CFPropertyListFormat format = 99;
    CFErrorRef error = NULL;
    CFPropertyListRef plist = CFPropertyListCreateWithData(
        kCFAllocatorSystemDefault,
        (CFDataRef)string,
        kCFPropertyListImmutable,
        &format,
        &error
    );

    expect_true(plist == NULL, "property-list read with non-data input should fail");
    expect_error_code(error, kCFPropertyListReadCorruptError, "property-list non-data error code mismatch");
    expect_long((CFIndex)format, 0, "property-list read non-data should zero format");
    CFRelease(error);
    CFRelease((CFTypeRef)string);
}

static void test_property_list_create_with_data_mutable_options_rejected(void) {
    CFStringRef string = create_ascii_string("launchd.packet-key", "mutable-read string create failed");
    CFErrorRef writeError = NULL;
    CFDataRef data = CFPropertyListCreateData(
        kCFAllocatorSystemDefault,
        (CFPropertyListRef)string,
        kCFPropertyListBinaryFormat_v1_0,
        0,
        &writeError
    );
    CFPropertyListFormat format = 99;
    CFErrorRef readError = NULL;
    CFPropertyListRef plist = CFPropertyListCreateWithData(
        kCFAllocatorSystemDefault,
        data,
        kCFPropertyListMutableContainers,
        &format,
        &readError
    );

    expect_true(data != NULL, "mutable-read setup write failed");
    expect_true(writeError == NULL, "mutable-read setup write error must be NULL");
    expect_true(plist == NULL, "property-list read with mutable options should fail");
    expect_error_code(readError, kCFPropertyListReadCorruptError, "property-list mutable-read error code mismatch");
    expect_long((CFIndex)format, 0, "property-list mutable-read should zero format");
    CFRelease(readError);
    CFRelease(data);
    CFRelease((CFTypeRef)string);
}

static void test_invalid_header_rejected(void) {
    static const UInt8 invalidBytes[] = {'x', 'p', 'l', 'i', 's', 't', '0', '0', 0x00u};

    CFDataRef data = create_data(invalidBytes, (CFIndex)sizeof(invalidBytes), "invalid-header data create failed");
    CFPropertyListFormat format = 0;
    CFErrorRef error = NULL;
    CFPropertyListRef readback = CFPropertyListCreateWithData(
        kCFAllocatorSystemDefault,
        data,
        kCFPropertyListImmutable,
        &format,
        &error
    );

    expect_true(readback == NULL, "invalid-header read should fail");
    expect_error_code(error, kCFPropertyListReadCorruptError, "invalid-header error code mismatch");
    expect_long((CFIndex)format, 0, "invalid-header format should remain zero");

    CFRelease(error);
    CFRelease(data);
}

static void test_truncated_trailer_rejected(void) {
    CFStringRef string = create_ascii_string("launchd.packet-key", "truncated-trailer string create failed");
    CFErrorRef writeError = NULL;
    CFDataRef valid = CFPropertyListCreateData(
        kCFAllocatorSystemDefault,
        (CFPropertyListRef)string,
        kCFPropertyListBinaryFormat_v1_0,
        0,
        &writeError
    );
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

    expect_true(valid != NULL, "valid data for truncated-trailer test failed");
    expect_true(writeError == NULL, "truncated-trailer write error must be NULL");
    expect_true(truncated != NULL, "truncated-trailer truncated data create failed");
    expect_true(readback == NULL, "truncated-trailer read should fail");
    expect_error_code(readError, kCFPropertyListReadCorruptError, "truncated-trailer error code mismatch");
    expect_long((CFIndex)format, 0, "truncated-trailer format should remain zero");

    CFRelease(readError);
    CFRelease(truncated);
    CFRelease(valid);
    CFRelease((CFTypeRef)string);
}

int main(void) {
    test_ascii_string_roundtrip();
    test_unicode_string_roundtrip();
    test_mixed_array_roundtrip();
    test_string_key_dictionary_roundtrip();
    test_non_string_dictionary_key_rejected();
    test_property_list_create_data_null_rejected();
    test_property_list_create_data_unsupported_format_rejected();
    test_property_list_create_data_mutable_options_rejected();
    test_property_list_create_data_unsupported_object_rejected();
    test_property_list_create_with_data_null_rejected();
    test_property_list_create_with_data_non_data_rejected();
    test_property_list_create_with_data_mutable_options_rejected();
    test_invalid_header_rejected();
    test_truncated_trailer_rejected();
    puts("PASS bplist_core_tests");
    return 0;
}
