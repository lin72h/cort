#include <CoreFoundation/CoreFoundation.h>

#include <stdio.h>
#include <stdlib.h>

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

static CFDataRef create_data(const UInt8 *bytes, CFIndex length, const char *message) {
    CFDataRef data = CFDataCreate(kCFAllocatorSystemDefault, bytes, length);
    if (data == NULL) {
        fail(message);
    }
    return data;
}

static CFStringRef create_ascii_string(const char *text, const char *message) {
    CFStringRef string = CFStringCreateWithCString(kCFAllocatorSystemDefault, text, kCFStringEncodingASCII);
    if (string == NULL) {
        fail(message);
    }
    return string;
}

static void test_array_empty_immutable(void) {
    CFArrayRef array = CFArrayCreate(kCFAllocatorSystemDefault, NULL, 0, &kCFTypeArrayCallBacks);

    expect_true(array != NULL, "CFArrayCreate empty failed");
    expect_long((CFIndex)CFGetTypeID((CFTypeRef)array), (CFIndex)CFArrayGetTypeID(), "array type ID mismatch");
    expect_long(CFArrayGetCount(array), 0, "empty array count mismatch");

    CFRelease((CFTypeRef)array);
}

static void test_array_immutable_borrowed_get(void) {
    static const UInt8 firstBytes[] = {
        0x61u, 0x6cu, 0x70u, 0x68u, 0x61u, 0x2du, 0x6bu, 0x65u, 0x79u
    };
    static const UInt8 secondBytes[] = {0x00u, 0x41u, 0x7fu, 0xffu};

    CFDataRef first = create_data(firstBytes, (CFIndex)sizeof(firstBytes), "first array data create failed");
    CFDataRef second = create_data(secondBytes, (CFIndex)sizeof(secondBytes), "second array data create failed");
    const void *values[] = {first, second};
    CFIndex firstBase = CFGetRetainCount((CFTypeRef)first);
    CFIndex secondBase = CFGetRetainCount((CFTypeRef)second);

    CFArrayRef array = CFArrayCreate(kCFAllocatorSystemDefault, values, 2, &kCFTypeArrayCallBacks);
    expect_true(array != NULL, "CFArrayCreate immutable failed");
    expect_long(CFGetRetainCount((CFTypeRef)first), firstBase + 1, "immutable array must retain first child");
    expect_long(CFGetRetainCount((CFTypeRef)second), secondBase + 1, "immutable array must retain second child");
    expect_long(CFArrayGetCount(array), 2, "immutable array count mismatch");
    expect_true(CFArrayGetValueAtIndex(array, 0) == (const void *)first, "immutable array borrowed first pointer mismatch");
    expect_true(CFArrayGetValueAtIndex(array, 1) == (const void *)second, "immutable array borrowed second pointer mismatch");
    expect_long(CFGetRetainCount((CFTypeRef)first), firstBase + 1, "immutable array get must not retain first child");
    expect_long(CFGetRetainCount((CFTypeRef)second), secondBase + 1, "immutable array get must not retain second child");

    CFRelease((CFTypeRef)array);
    expect_long(CFGetRetainCount((CFTypeRef)first), firstBase, "immutable array release must release first child");
    expect_long(CFGetRetainCount((CFTypeRef)second), secondBase, "immutable array release must release second child");

    CFRelease((CFTypeRef)second);
    CFRelease((CFTypeRef)first);
}

static void test_array_mutable_append_remove_retains(void) {
    static const UInt8 bytes[] = {0xaau, 0xbbu, 0xccu};

    CFDataRef data = create_data(bytes, (CFIndex)sizeof(bytes), "mutable array data create failed");
    CFMutableArrayRef array = CFArrayCreateMutable(kCFAllocatorSystemDefault, 0, &kCFTypeArrayCallBacks);
    CFIndex base = CFGetRetainCount((CFTypeRef)data);

    expect_true(array != NULL, "CFArrayCreateMutable failed");

    CFArrayAppendValue(array, data);
    expect_long(CFGetRetainCount((CFTypeRef)data), base + 1, "mutable array append must retain");
    expect_true(CFArrayGetValueAtIndex(array, 0) == (const void *)data, "mutable array borrowed pointer mismatch");
    expect_long(CFGetRetainCount((CFTypeRef)data), base + 1, "mutable array get must not retain");
    CFArrayRemoveValueAtIndex(array, 0);
    expect_long(CFArrayGetCount(array), 0, "mutable array remove count mismatch");
    expect_long(CFGetRetainCount((CFTypeRef)data), base, "mutable array remove must release");

    CFArrayAppendValue(array, data);
    expect_long(CFGetRetainCount((CFTypeRef)data), base + 1, "mutable array reappend must retain");
    CFRelease((CFTypeRef)array);
    expect_long(CFGetRetainCount((CFTypeRef)data), base, "mutable array dealloc must release retained child");

    CFRelease((CFTypeRef)data);
}

static void test_array_create_copy_borrowed_get(void) {
    static const UInt8 bytes[] = {0x11u, 0x22u};

    CFStringRef key = create_ascii_string("copy-array-key", "copy array key create failed");
    CFDataRef data = create_data(bytes, (CFIndex)sizeof(bytes), "copy array data create failed");
    const void *values[] = {key, data};
    CFIndex keyBase = CFGetRetainCount((CFTypeRef)key);
    CFIndex dataBase = CFGetRetainCount((CFTypeRef)data);

    CFArrayRef original = CFArrayCreate(kCFAllocatorSystemDefault, values, 2, &kCFTypeArrayCallBacks);
    expect_true(original != NULL, "array original create failed");
    expect_long(CFGetRetainCount((CFTypeRef)key), keyBase + 1, "array original key retain mismatch");
    expect_long(CFGetRetainCount((CFTypeRef)data), dataBase + 1, "array original data retain mismatch");

    CFArrayRef copy = CFArrayCreateCopy(kCFAllocatorSystemDefault, original);
    expect_true(copy != NULL, "array copy create failed");
    expect_long(CFGetRetainCount((CFTypeRef)key), keyBase + 2, "array copy must retain key child");
    expect_long(CFGetRetainCount((CFTypeRef)data), dataBase + 2, "array copy must retain data child");

    CFRelease((CFTypeRef)original);
    expect_long(CFGetRetainCount((CFTypeRef)key), keyBase + 1, "array copy must keep key child alive");
    expect_long(CFGetRetainCount((CFTypeRef)data), dataBase + 1, "array copy must keep data child alive");
    expect_long(CFArrayGetCount(copy), 2, "array copy count mismatch");
    expect_true(CFArrayGetValueAtIndex(copy, 0) == (const void *)key, "array copy borrowed key pointer mismatch");
    expect_true(CFArrayGetValueAtIndex(copy, 1) == (const void *)data, "array copy borrowed data pointer mismatch");

    CFRelease((CFTypeRef)copy);
    expect_long(CFGetRetainCount((CFTypeRef)key), keyBase, "array copy release must release key child");
    expect_long(CFGetRetainCount((CFTypeRef)data), dataBase, "array copy release must release data child");

    CFRelease((CFTypeRef)data);
    CFRelease((CFTypeRef)key);
}

static void test_dictionary_empty_immutable(void) {
    CFDictionaryRef dictionary = CFDictionaryCreate(
        kCFAllocatorSystemDefault,
        NULL,
        NULL,
        0,
        &kCFTypeDictionaryKeyCallBacks,
        &kCFTypeDictionaryValueCallBacks
    );

    expect_true(dictionary != NULL, "CFDictionaryCreate empty failed");
    expect_long((CFIndex)CFGetTypeID((CFTypeRef)dictionary), (CFIndex)CFDictionaryGetTypeID(), "dictionary type ID mismatch");
    expect_long(CFDictionaryGetCount(dictionary), 0, "empty dictionary count mismatch");

    CFRelease((CFTypeRef)dictionary);
}

static void test_dictionary_immutable_string_key_lookup(void) {
    static const UInt8 bytes[] = {0x10u, 0x20u, 0x30u, 0x40u};

    CFStringRef key = create_ascii_string("subset2a-key", "dictionary key create failed");
    CFStringRef missingKey = create_ascii_string("missing-key", "dictionary missing key create failed");
    CFDataRef value = create_data(bytes, (CFIndex)sizeof(bytes), "dictionary value create failed");
    const void *keys[] = {key};
    const void *values[] = {value};
    const void *presentValue = NULL;
    const void *missingValue = (const void *)0x1;
    CFIndex keyBase = CFGetRetainCount((CFTypeRef)key);
    CFIndex valueBase = CFGetRetainCount((CFTypeRef)value);

    CFDictionaryRef dictionary = CFDictionaryCreate(
        kCFAllocatorSystemDefault,
        keys,
        values,
        1,
        &kCFTypeDictionaryKeyCallBacks,
        &kCFTypeDictionaryValueCallBacks
    );

    expect_true(dictionary != NULL, "CFDictionaryCreate immutable failed");
    expect_long(CFDictionaryGetCount(dictionary), 1, "immutable dictionary count mismatch");
    expect_long(CFGetRetainCount((CFTypeRef)key), keyBase + 1, "immutable dictionary must retain key");
    expect_long(CFGetRetainCount((CFTypeRef)value), valueBase + 1, "immutable dictionary must retain value");
    expect_true(CFDictionaryGetValue(dictionary, key) == (const void *)value, "immutable dictionary get pointer mismatch");
    expect_true(CFDictionaryGetValueIfPresent(dictionary, key, &presentValue), "dictionary present lookup failed");
    expect_true(presentValue == (const void *)value, "dictionary present lookup pointer mismatch");
    expect_true(!CFDictionaryGetValueIfPresent(dictionary, missingKey, &missingValue), "dictionary missing lookup must fail");
    expect_true(missingValue == NULL, "dictionary missing lookup should null out value");
    expect_long(CFGetRetainCount((CFTypeRef)value), valueBase + 1, "dictionary borrowed getters must not retain");

    CFRelease((CFTypeRef)dictionary);
    expect_long(CFGetRetainCount((CFTypeRef)key), keyBase, "dictionary release must release key");
    expect_long(CFGetRetainCount((CFTypeRef)value), valueBase, "dictionary release must release value");

    CFRelease((CFTypeRef)value);
    CFRelease((CFTypeRef)missingKey);
    CFRelease((CFTypeRef)key);
}

static void test_dictionary_mutable_set_replace_remove_retains(void) {
    static const UInt8 value1Bytes[] = {0x01u, 0x02u};
    static const UInt8 value2Bytes[] = {0x03u, 0x04u};

    CFStringRef key = create_ascii_string("subset2a-key", "mutable dictionary key create failed");
    CFDataRef value1 = create_data(value1Bytes, (CFIndex)sizeof(value1Bytes), "mutable dictionary value1 create failed");
    CFDataRef value2 = create_data(value2Bytes, (CFIndex)sizeof(value2Bytes), "mutable dictionary value2 create failed");
    CFMutableDictionaryRef dictionary = CFDictionaryCreateMutable(
        kCFAllocatorSystemDefault,
        0,
        &kCFTypeDictionaryKeyCallBacks,
        &kCFTypeDictionaryValueCallBacks
    );
    CFIndex keyBase = CFGetRetainCount((CFTypeRef)key);
    CFIndex value1Base = CFGetRetainCount((CFTypeRef)value1);
    CFIndex value2Base = CFGetRetainCount((CFTypeRef)value2);

    expect_true(dictionary != NULL, "CFDictionaryCreateMutable failed");

    CFDictionarySetValue(dictionary, key, value1);
    expect_long(CFGetRetainCount((CFTypeRef)key), keyBase + 1, "mutable dictionary set must retain key");
    expect_long(CFGetRetainCount((CFTypeRef)value1), value1Base + 1, "mutable dictionary set must retain value1");
    expect_true(CFDictionaryGetValue(dictionary, key) == (const void *)value1, "mutable dictionary first get mismatch");
    expect_long(CFGetRetainCount((CFTypeRef)value1), value1Base + 1, "mutable dictionary get must not retain value1");

    CFDictionarySetValue(dictionary, key, value2);
    expect_long(CFGetRetainCount((CFTypeRef)key), keyBase + 1, "mutable dictionary replace must preserve single key retain");
    expect_long(CFGetRetainCount((CFTypeRef)value1), value1Base, "mutable dictionary replace must release old value");
    expect_long(CFGetRetainCount((CFTypeRef)value2), value2Base + 1, "mutable dictionary replace must retain new value");
    expect_true(CFDictionaryGetValue(dictionary, key) == (const void *)value2, "mutable dictionary replace get mismatch");

    CFDictionaryRemoveValue(dictionary, key);
    expect_long(CFDictionaryGetCount(dictionary), 0, "mutable dictionary remove count mismatch");
    expect_long(CFGetRetainCount((CFTypeRef)key), keyBase, "mutable dictionary remove must release key");
    expect_long(CFGetRetainCount((CFTypeRef)value2), value2Base, "mutable dictionary remove must release value2");

    CFDictionarySetValue(dictionary, key, value1);
    expect_long(CFGetRetainCount((CFTypeRef)key), keyBase + 1, "mutable dictionary reseed must retain key");
    expect_long(CFGetRetainCount((CFTypeRef)value1), value1Base + 1, "mutable dictionary reseed must retain value1");
    CFRelease((CFTypeRef)dictionary);
    expect_long(CFGetRetainCount((CFTypeRef)key), keyBase, "mutable dictionary dealloc must release key");
    expect_long(CFGetRetainCount((CFTypeRef)value1), value1Base, "mutable dictionary dealloc must release value1");

    CFRelease((CFTypeRef)value2);
    CFRelease((CFTypeRef)value1);
    CFRelease((CFTypeRef)key);
}

static void test_dictionary_create_copy_borrowed_lookup(void) {
    static const UInt8 bytes[] = {0x55u, 0x66u, 0x77u, 0x88u};

    CFStringRef key = create_ascii_string("copy-dict-key", "copy dictionary key create failed");
    CFDataRef value = create_data(bytes, (CFIndex)sizeof(bytes), "copy dictionary value create failed");
    const void *keys[] = {key};
    const void *values[] = {value};
    CFIndex keyBase = CFGetRetainCount((CFTypeRef)key);
    CFIndex valueBase = CFGetRetainCount((CFTypeRef)value);

    CFDictionaryRef original = CFDictionaryCreate(
        kCFAllocatorSystemDefault,
        keys,
        values,
        1,
        &kCFTypeDictionaryKeyCallBacks,
        &kCFTypeDictionaryValueCallBacks
    );
    expect_true(original != NULL, "dictionary original create failed");
    expect_long(CFGetRetainCount((CFTypeRef)key), keyBase + 1, "dictionary original retain mismatch");
    expect_long(CFGetRetainCount((CFTypeRef)value), valueBase + 1, "dictionary original retain mismatch");

    CFDictionaryRef copy = CFDictionaryCreateCopy(kCFAllocatorSystemDefault, original);
    expect_true(copy != NULL, "dictionary copy create failed");
    expect_long(CFGetRetainCount((CFTypeRef)key), keyBase + 2, "dictionary copy must retain key");
    expect_long(CFGetRetainCount((CFTypeRef)value), valueBase + 2, "dictionary copy must retain value");

    CFRelease((CFTypeRef)original);
    expect_long(CFGetRetainCount((CFTypeRef)key), keyBase + 1, "dictionary copy must keep key alive");
    expect_long(CFGetRetainCount((CFTypeRef)value), valueBase + 1, "dictionary copy must keep value alive");
    expect_long(CFDictionaryGetCount(copy), 1, "dictionary copy count mismatch");
    expect_true(CFDictionaryGetValue(copy, key) == (const void *)value, "dictionary copy borrowed lookup mismatch");

    CFRelease((CFTypeRef)copy);
    expect_long(CFGetRetainCount((CFTypeRef)key), keyBase, "dictionary copy release must release key");
    expect_long(CFGetRetainCount((CFTypeRef)value), valueBase, "dictionary copy release must release value");

    CFRelease((CFTypeRef)value);
    CFRelease((CFTypeRef)key);
}

int main(void) {
    test_array_empty_immutable();
    test_array_immutable_borrowed_get();
    test_array_mutable_append_remove_retains();
    test_array_create_copy_borrowed_get();
    test_dictionary_empty_immutable();
    test_dictionary_immutable_string_key_lookup();
    test_dictionary_mutable_set_replace_remove_retains();
    test_dictionary_create_copy_borrowed_lookup();
    puts("PASS container_core_tests");
    return 0;
}
