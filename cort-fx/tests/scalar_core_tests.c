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

static void expect_bytes(const UInt8 *actual, const UInt8 *expected, CFIndex length, const char *message) {
    if (length > 0 && memcmp(actual, expected, (size_t)length) != 0) {
        fail(message);
    }
}

static void test_booleans(void) {
    expect_long((CFIndex)CFBooleanGetTypeID(), 3, "boolean type ID must be builtin");
    expect_long((CFIndex)CFGetTypeID((CFTypeRef)kCFBooleanTrue), (CFIndex)CFBooleanGetTypeID(), "true type ID mismatch");
    expect_long((CFIndex)CFGetTypeID((CFTypeRef)kCFBooleanFalse), (CFIndex)CFBooleanGetTypeID(), "false type ID mismatch");
    expect_true(CFBooleanGetValue(kCFBooleanTrue), "true singleton must report true");
    expect_true(!CFBooleanGetValue(kCFBooleanFalse), "false singleton must report false");
    expect_true(CFRetain((CFTypeRef)kCFBooleanTrue) == (CFTypeRef)kCFBooleanTrue, "true retain must be identity");
    expect_true(CFRetain((CFTypeRef)kCFBooleanFalse) == (CFTypeRef)kCFBooleanFalse, "false retain must be identity");
    CFRelease((CFTypeRef)kCFBooleanTrue);
    CFRelease((CFTypeRef)kCFBooleanFalse);
    expect_true(CFEqual((CFTypeRef)kCFBooleanTrue, (CFTypeRef)kCFBooleanTrue), "true must equal itself");
    expect_true(!CFEqual((CFTypeRef)kCFBooleanTrue, (CFTypeRef)kCFBooleanFalse), "true and false must differ");
    expect_true(CFHash((CFTypeRef)kCFBooleanTrue) == CFHash((CFTypeRef)kCFBooleanTrue), "true hash must be stable");
}

static void test_data(void) {
    static const UInt8 bytes[] = {0x00u, 0x41u, 0x7fu, 0xffu};
    static const UInt8 differentBytes[] = {0x00u, 0x41u, 0x7fu, 0x00u};

    CFDataRef data = CFDataCreate(kCFAllocatorSystemDefault, bytes, (CFIndex)sizeof(bytes));
    CFDataRef copy = CFDataCreateCopy(kCFAllocatorSystemDefault, data);
    CFDataRef different = CFDataCreate(kCFAllocatorSystemDefault, differentBytes, (CFIndex)sizeof(differentBytes));

    expect_true(data != NULL, "CFDataCreate failed");
    expect_true(copy != NULL, "CFDataCreateCopy failed");
    expect_true(different != NULL, "CFDataCreate for different bytes failed");
    expect_long((CFIndex)CFGetTypeID((CFTypeRef)data), (CFIndex)CFDataGetTypeID(), "data type ID mismatch");
    expect_long(CFDataGetLength(data), (CFIndex)sizeof(bytes), "data length mismatch");
    expect_true(CFDataGetBytePtr(data) != NULL, "data byte pointer must be non-null");
    expect_bytes(CFDataGetBytePtr(data), bytes, (CFIndex)sizeof(bytes), "data bytes mismatch");
    expect_true(CFEqual((CFTypeRef)data, (CFTypeRef)copy), "equal data must compare equal");
    expect_true(!CFEqual((CFTypeRef)data, (CFTypeRef)different), "different data must not compare equal");
    expect_true(CFHash((CFTypeRef)data) == CFHash((CFTypeRef)copy), "equal data must hash equally");

    CFRelease((CFTypeRef)different);
    CFRelease((CFTypeRef)copy);
    CFRelease((CFTypeRef)data);
}

static void test_numbers(void) {
    SInt32 int32Value = 42;
    SInt64 int64Value = 42;
    double float64Value = 42.0;
    double float64Different = 1234.5;
    SInt32 out32 = 0;
    SInt64 out64 = 0;
    double outDouble = 0.0;

    CFNumberRef int32Number = CFNumberCreate(kCFAllocatorSystemDefault, kCFNumberSInt32Type, &int32Value);
    CFNumberRef int64Number = CFNumberCreate(kCFAllocatorSystemDefault, kCFNumberSInt64Type, &int64Value);
    CFNumberRef float64Number = CFNumberCreate(kCFAllocatorSystemDefault, kCFNumberFloat64Type, &float64Value);
    CFNumberRef differentNumber = CFNumberCreate(kCFAllocatorSystemDefault, kCFNumberFloat64Type, &float64Different);

    expect_true(int32Number != NULL, "CFNumberCreate SInt32 failed");
    expect_true(int64Number != NULL, "CFNumberCreate SInt64 failed");
    expect_true(float64Number != NULL, "CFNumberCreate Float64 failed");
    expect_true(differentNumber != NULL, "CFNumberCreate different Float64 failed");
    expect_long((CFIndex)CFGetTypeID((CFTypeRef)int32Number), (CFIndex)CFNumberGetTypeID(), "number type ID mismatch");
    expect_long((CFIndex)CFNumberGetType(int32Number), (CFIndex)kCFNumberSInt32Type, "SInt32 stored type mismatch");
    expect_long((CFIndex)CFNumberGetType(int64Number), (CFIndex)kCFNumberSInt64Type, "SInt64 stored type mismatch");
    expect_long((CFIndex)CFNumberGetType(float64Number), (CFIndex)kCFNumberFloat64Type, "Float64 stored type mismatch");
    expect_true(CFNumberGetValue(int32Number, kCFNumberSInt32Type, &out32), "SInt32 roundtrip must be exact");
    expect_true(CFNumberGetValue(int64Number, kCFNumberSInt64Type, &out64), "SInt64 roundtrip must be exact");
    expect_true(CFNumberGetValue(float64Number, kCFNumberFloat64Type, &outDouble), "Float64 roundtrip must be exact");
    expect_long((CFIndex)out32, (CFIndex)int32Value, "SInt32 roundtrip mismatch");
    expect_long((CFIndex)out64, (CFIndex)int64Value, "SInt64 roundtrip mismatch");
    expect_true(outDouble == float64Value, "Float64 roundtrip mismatch");
    expect_true(CFEqual((CFTypeRef)int32Number, (CFTypeRef)int64Number), "SInt32 and SInt64 42 must compare equal");
    expect_true(CFEqual((CFTypeRef)int32Number, (CFTypeRef)float64Number), "SInt32 42 and Float64 42.0 must compare equal");
    expect_true(!CFEqual((CFTypeRef)int32Number, (CFTypeRef)differentNumber), "different numbers must not compare equal");
    expect_true(CFHash((CFTypeRef)int32Number) == CFHash((CFTypeRef)int64Number), "equal integer numbers must hash equally");
    expect_true(CFHash((CFTypeRef)int32Number) == CFHash((CFTypeRef)float64Number), "equal integer/float numbers must hash equally");

    CFRelease((CFTypeRef)differentNumber);
    CFRelease((CFTypeRef)float64Number);
    CFRelease((CFTypeRef)int64Number);
    CFRelease((CFTypeRef)int32Number);
}

static void test_dates(void) {
    CFAbsoluteTime value = 1234.5;
    CFAbsoluteTime differentValue = 1235.5;

    CFDateRef primary = CFDateCreate(kCFAllocatorSystemDefault, value);
    CFDateRef same = CFDateCreate(kCFAllocatorSystemDefault, value);
    CFDateRef different = CFDateCreate(kCFAllocatorSystemDefault, differentValue);

    expect_true(primary != NULL, "CFDateCreate failed");
    expect_true(same != NULL, "CFDateCreate same failed");
    expect_true(different != NULL, "CFDateCreate different failed");
    expect_long((CFIndex)CFGetTypeID((CFTypeRef)primary), (CFIndex)CFDateGetTypeID(), "date type ID mismatch");
    expect_true(CFDateGetAbsoluteTime(primary) == value, "date roundtrip mismatch");
    expect_true(CFEqual((CFTypeRef)primary, (CFTypeRef)same), "equal dates must compare equal");
    expect_true(!CFEqual((CFTypeRef)primary, (CFTypeRef)different), "different dates must not compare equal");
    expect_true(CFHash((CFTypeRef)primary) == CFHash((CFTypeRef)same), "equal dates must hash equally");

    CFRelease((CFTypeRef)different);
    CFRelease((CFTypeRef)same);
    CFRelease((CFTypeRef)primary);
}

int main(void) {
    test_booleans();
    test_data();
    test_numbers();
    test_dates();
    puts("PASS scalar_core_tests");
    return 0;
}
