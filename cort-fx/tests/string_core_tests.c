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

static void expect_bytes(const UInt8 *actual, const UInt8 *expected, size_t length, const char *message) {
    if (length > 0 && memcmp(actual, expected, length) != 0) {
        fail(message);
    }
}

static void expect_unichars(const UniChar *actual, const UniChar *expected, size_t length, const char *message) {
    if (length > 0 && memcmp(actual, expected, length * sizeof(UniChar)) != 0) {
        fail(message);
    }
}

static void expect_cstring(const char *actual, const char *expected, const char *message) {
    if (strcmp(actual, expected) != 0) {
        fail(message);
    }
}

static void test_ascii_cstring_roundtrip(void) {
    static const char text[] = "launchd.packet-key";
    static const UniChar expectedChars[] = {
        'l', 'a', 'u', 'n', 'c', 'h', 'd', '.', 'p', 'a', 'c', 'k', 'e', 't', '-', 'k', 'e', 'y'
    };

    char utf8Buffer[64];
    UniChar characterBuffer[18];

    CFStringRef primary = CFStringCreateWithCString(kCFAllocatorSystemDefault, text, kCFStringEncodingASCII);
    CFStringRef same = CFStringCreateWithCString(kCFAllocatorSystemDefault, text, kCFStringEncodingASCII);
    CFStringRef different = CFStringCreateWithCString(kCFAllocatorSystemDefault, "launchd.packet-value", kCFStringEncodingASCII);

    expect_true(primary != NULL, "ASCII CFStringCreateWithCString failed");
    expect_true(same != NULL, "ASCII duplicate CFStringCreateWithCString failed");
    expect_true(different != NULL, "ASCII different CFStringCreateWithCString failed");
    expect_long((CFIndex)CFGetTypeID((CFTypeRef)primary), (CFIndex)CFStringGetTypeID(), "string type ID mismatch");
    expect_long(CFStringGetLength(primary), 18, "ASCII string length mismatch");
    expect_true(CFStringGetCString(primary, utf8Buffer, (CFIndex)sizeof(utf8Buffer), kCFStringEncodingUTF8), "ASCII UTF-8 export failed");
    expect_cstring(utf8Buffer, text, "ASCII UTF-8 export mismatch");
    CFStringGetCharacters(primary, CFRangeMake(0, CFStringGetLength(primary)), characterBuffer);
    expect_unichars(characterBuffer, expectedChars, 18, "ASCII UTF-16 characters mismatch");
    expect_true(CFEqual((CFTypeRef)primary, (CFTypeRef)same), "equal ASCII strings must compare equal");
    expect_true(!CFEqual((CFTypeRef)primary, (CFTypeRef)different), "different ASCII strings must not compare equal");
    expect_true(CFHash((CFTypeRef)primary) == CFHash((CFTypeRef)same), "equal ASCII strings must hash equally");

    CFRelease((CFTypeRef)different);
    CFRelease((CFTypeRef)same);
    CFRelease((CFTypeRef)primary);
}

static void test_empty_roundtrip(void) {
    char buffer[1];
    CFIndex used = -1;
    CFStringRef fromCString = CFStringCreateWithCString(kCFAllocatorSystemDefault, "", kCFStringEncodingASCII);
    CFStringRef fromBytes = CFStringCreateWithBytes(kCFAllocatorSystemDefault, NULL, 0, kCFStringEncodingASCII, false);
    CFStringRef fromChars = CFStringCreateWithCharacters(kCFAllocatorSystemDefault, NULL, 0);
    CFStringRef different = CFStringCreateWithCString(kCFAllocatorSystemDefault, "x", kCFStringEncodingASCII);

    expect_true(fromCString != NULL, "empty CFStringCreateWithCString failed");
    expect_true(fromBytes != NULL, "empty CFStringCreateWithBytes failed");
    expect_true(fromChars != NULL, "empty CFStringCreateWithCharacters failed");
    expect_true(different != NULL, "different empty comparison string failed");
    expect_long(CFStringGetLength(fromCString), 0, "empty length mismatch");
    expect_true(CFEqual((CFTypeRef)fromCString, (CFTypeRef)fromBytes), "empty cstring and bytes strings must compare equal");
    expect_true(CFEqual((CFTypeRef)fromCString, (CFTypeRef)fromChars), "empty cstring and characters strings must compare equal");
    expect_true(!CFEqual((CFTypeRef)fromCString, (CFTypeRef)different), "empty string must not equal non-empty string");
    expect_true(CFHash((CFTypeRef)fromCString) == CFHash((CFTypeRef)fromBytes), "empty string hash mismatch");
    expect_true(CFStringGetCString(fromCString, buffer, 1, kCFStringEncodingUTF8), "empty exact-fit export failed");
    expect_cstring(buffer, "", "empty export mismatch");
    expect_long(CFStringGetBytes(fromCString, CFRangeMake(0, 0), kCFStringEncodingASCII, 0, false, NULL, 0, &used), 0, "empty bytes converted mismatch");
    expect_long(used, 0, "empty bytes used mismatch");

    CFRelease((CFTypeRef)different);
    CFRelease((CFTypeRef)fromChars);
    CFRelease((CFTypeRef)fromBytes);
    CFRelease((CFTypeRef)fromCString);
}

static void test_utf8_cstring_roundtrip(void) {
    static const char text[] = "caf\xC3\xA9-\xE2\x98\x83-\xF0\x9F\x93\xA6";
    static const UniChar expectedChars[] = {0x0063, 0x0061, 0x0066, 0x00e9, 0x002d, 0x2603, 0x002d, 0xd83d, 0xdce6};

    char utf8Buffer[32];
    UniChar characterBuffer[9];

    CFStringRef primary = CFStringCreateWithCString(kCFAllocatorSystemDefault, text, kCFStringEncodingUTF8);
    CFStringRef same = CFStringCreateWithCString(kCFAllocatorSystemDefault, text, kCFStringEncodingUTF8);
    CFStringRef different = CFStringCreateWithCString(kCFAllocatorSystemDefault, "caf\xC3\xA9-\xE2\x98\x83-\xF0\x9F\x93\xA7", kCFStringEncodingUTF8);

    expect_true(primary != NULL, "UTF-8 CFStringCreateWithCString failed");
    expect_true(same != NULL, "UTF-8 duplicate CFStringCreateWithCString failed");
    expect_true(different != NULL, "UTF-8 different CFStringCreateWithCString failed");
    expect_long(CFStringGetLength(primary), 9, "UTF-8 string length mismatch");
    expect_true(CFStringGetCString(primary, utf8Buffer, (CFIndex)sizeof(utf8Buffer), kCFStringEncodingUTF8), "UTF-8 export failed");
    expect_cstring(utf8Buffer, text, "UTF-8 export mismatch");
    CFStringGetCharacters(primary, CFRangeMake(0, CFStringGetLength(primary)), characterBuffer);
    expect_unichars(characterBuffer, expectedChars, 9, "UTF-8 characters mismatch");
    expect_true(CFEqual((CFTypeRef)primary, (CFTypeRef)same), "equal UTF-8 strings must compare equal");
    expect_true(!CFEqual((CFTypeRef)primary, (CFTypeRef)different), "different UTF-8 strings must not compare equal");
    expect_true(CFHash((CFTypeRef)primary) == CFHash((CFTypeRef)same), "equal UTF-8 strings must hash equally");

    CFRelease((CFTypeRef)different);
    CFRelease((CFTypeRef)same);
    CFRelease((CFTypeRef)primary);
}

static void test_bytes_ascii_roundtrip(void) {
    static const UInt8 bytes[] = "launchd.packet-key";
    static const UniChar expectedChars[] = {
        'l', 'a', 'u', 'n', 'c', 'h', 'd', '.', 'p', 'a', 'c', 'k', 'e', 't', '-', 'k', 'e', 'y'
    };

    UInt8 extracted[32];
    UniChar characters[18];
    CFIndex used = 0;
    CFStringRef primary = CFStringCreateWithBytes(kCFAllocatorSystemDefault, bytes, 18, kCFStringEncodingASCII, false);
    CFStringRef same = CFStringCreateWithCString(kCFAllocatorSystemDefault, "launchd.packet-key", kCFStringEncodingASCII);
    CFStringRef different = CFStringCreateWithBytes(kCFAllocatorSystemDefault, (const UInt8 *)"launchd.packet-value", 20, kCFStringEncodingASCII, false);

    expect_true(primary != NULL, "ASCII bytes create failed");
    expect_true(same != NULL, "ASCII cstring comparison create failed");
    expect_true(different != NULL, "ASCII bytes different create failed");
    expect_long(CFStringGetLength(primary), 18, "ASCII bytes length mismatch");
    expect_long(CFStringGetBytes(primary, CFRangeMake(0, CFStringGetLength(primary)), kCFStringEncodingASCII, 0, false, extracted, (CFIndex)sizeof(extracted), &used), 18, "ASCII bytes converted mismatch");
    expect_long(used, 18, "ASCII bytes used mismatch");
    expect_bytes(extracted, bytes, 18, "ASCII bytes roundtrip mismatch");
    CFStringGetCharacters(primary, CFRangeMake(0, CFStringGetLength(primary)), characters);
    expect_unichars(characters, expectedChars, 18, "ASCII bytes UTF-16 mismatch");
    expect_true(CFEqual((CFTypeRef)primary, (CFTypeRef)same), "bytes and cstring ASCII strings must compare equal");
    expect_true(!CFEqual((CFTypeRef)primary, (CFTypeRef)different), "different ASCII byte strings must not compare equal");
    expect_true(CFHash((CFTypeRef)primary) == CFHash((CFTypeRef)same), "equal ASCII byte strings must hash equally");

    CFRelease((CFTypeRef)different);
    CFRelease((CFTypeRef)same);
    CFRelease((CFTypeRef)primary);
}

static void test_utf16_characters_roundtrip(void) {
    static const UniChar characters[] = {0x004f, 0x03a9, 0x002d, 0x65e5, 0xd83d, 0xdce6};
    static const char utf8Text[] = "O\xCE\xA9-\xE6\x97\xA5\xF0\x9F\x93\xA6";
    static const UniChar differentCharacters[] = {0x004f, 0x03a9, 0x002d, 0x65e5, 0xd83d, 0xdce7};

    char utf8Buffer[32];
    UniChar copiedCharacters[6];
    CFStringRef primary = CFStringCreateWithCharacters(kCFAllocatorSystemDefault, characters, 6);
    CFStringRef same = CFStringCreateWithCharacters(kCFAllocatorSystemDefault, characters, 6);
    CFStringRef different = CFStringCreateWithCharacters(kCFAllocatorSystemDefault, differentCharacters, 6);

    expect_true(primary != NULL, "UTF-16 characters create failed");
    expect_true(same != NULL, "UTF-16 same create failed");
    expect_true(different != NULL, "UTF-16 different create failed");
    expect_long(CFStringGetLength(primary), 6, "UTF-16 length mismatch");
    expect_true(CFStringGetCString(primary, utf8Buffer, (CFIndex)sizeof(utf8Buffer), kCFStringEncodingUTF8), "UTF-16 UTF-8 export failed");
    expect_cstring(utf8Buffer, utf8Text, "UTF-16 UTF-8 export mismatch");
    CFStringGetCharacters(primary, CFRangeMake(0, CFStringGetLength(primary)), copiedCharacters);
    expect_unichars(copiedCharacters, characters, 6, "UTF-16 characters copy mismatch");
    expect_true(CFEqual((CFTypeRef)primary, (CFTypeRef)same), "equal UTF-16 strings must compare equal");
    expect_true(!CFEqual((CFTypeRef)primary, (CFTypeRef)different), "different UTF-16 strings must not compare equal");
    expect_true(CFHash((CFTypeRef)primary) == CFHash((CFTypeRef)same), "equal UTF-16 strings must hash equally");

    CFRelease((CFTypeRef)different);
    CFRelease((CFTypeRef)same);
    CFRelease((CFTypeRef)primary);
}

static void test_copy_and_buffer_boundaries(void) {
    static const char text[] = "caf\xC3\xA9-\xE2\x98\x83-\xF0\x9F\x93\xA6";
    char exactBuffer[sizeof(text)];
    char smallBuffer[sizeof(text) - 1];

    CFStringRef primary = CFStringCreateWithCString(kCFAllocatorSystemDefault, text, kCFStringEncodingUTF8);
    CFStringRef copy = CFStringCreateCopy(kCFAllocatorSystemDefault, primary);
    CFStringRef different = CFStringCreateWithCString(kCFAllocatorSystemDefault, "caf\xC3\xA9-\xE2\x98\x83-\xF0\x9F\x93\xA7", kCFStringEncodingUTF8);

    expect_true(primary != NULL, "copy primary create failed");
    expect_true(copy != NULL, "CFStringCreateCopy failed");
    expect_true(different != NULL, "copy different create failed");
    expect_long(CFStringGetLength(primary), 9, "copy primary length mismatch");
    expect_true(CFEqual((CFTypeRef)primary, (CFTypeRef)copy), "copied string must compare equal");
    expect_true(!CFEqual((CFTypeRef)primary, (CFTypeRef)different), "copied string must not equal different string");
    expect_true(CFHash((CFTypeRef)primary) == CFHash((CFTypeRef)copy), "copied string hash mismatch");
    expect_true(CFStringGetCString(primary, exactBuffer, (CFIndex)sizeof(exactBuffer), kCFStringEncodingUTF8), "exact-fit UTF-8 export failed");
    expect_cstring(exactBuffer, text, "exact-fit UTF-8 export mismatch");
    expect_true(!CFStringGetCString(primary, smallBuffer, (CFIndex)sizeof(smallBuffer), kCFStringEncodingUTF8), "undersized UTF-8 export must fail");

    CFRelease((CFTypeRef)different);
    CFRelease((CFTypeRef)copy);
    CFRelease((CFTypeRef)primary);
}

static void test_invalid_utf8_rejected(void) {
    static const UInt8 invalidBytes[] = {0xc3u, 0x28u};
    static const char invalidCString[] = "\xC3(";

    expect_true(CFStringCreateWithBytes(kCFAllocatorSystemDefault, invalidBytes, 2, kCFStringEncodingUTF8, false) == NULL, "invalid UTF-8 bytes must be rejected");
    expect_true(CFStringCreateWithCString(kCFAllocatorSystemDefault, invalidCString, kCFStringEncodingUTF8) == NULL, "invalid UTF-8 C-string must be rejected");
}

int main(void) {
    test_ascii_cstring_roundtrip();
    test_empty_roundtrip();
    test_utf8_cstring_roundtrip();
    test_bytes_ascii_roundtrip();
    test_utf16_characters_roundtrip();
    test_copy_and_buffer_boundaries();
    test_invalid_utf8_rejected();
    puts("PASS string_core_tests");
    return 0;
}
