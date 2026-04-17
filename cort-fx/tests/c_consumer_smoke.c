#include <CoreFoundation/CoreFoundation.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct SmokeObject {
    CFRuntimeBase base;
    int value;
};

static void smoke_init(CFTypeRef cf) {
    struct SmokeObject *object = (struct SmokeObject *)cf;
    object->value = 1234;
}

static void fail(const char *message) {
    fprintf(stderr, "FAIL: %s\n", message);
    exit(1);
}

int main(void) {
    static const CFRuntimeClass smokeClass = {
        0,
        "SmokeObject",
        smoke_init,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        0
    };

    if (CFAllocatorGetDefault() != kCFAllocatorSystemDefault) {
        fail("unexpected default allocator");
    }

    CFTypeID typeID = _CFRuntimeRegisterClass(&smokeClass);
    if (typeID == _kCFRuntimeNotATypeID) {
        fail("runtime class registration failed");
    }

    struct SmokeObject *object = (struct SmokeObject *)_CFRuntimeCreateInstance(
        kCFAllocatorSystemDefault,
        typeID,
        (CFIndex)(sizeof(struct SmokeObject) - sizeof(CFRuntimeBase)),
        NULL
    );
    if (object == NULL) {
        fail("runtime object allocation failed");
    }
    if (CFGetTypeID((CFTypeRef)object) != typeID) {
        fail("type ID mismatch");
    }
    if (object->value != 1234) {
        fail("init callback did not run");
    }
    if (CFRetain((CFTypeRef)object) != (CFTypeRef)object) {
        fail("retain must return the same pointer");
    }

    {
        static const UInt8 bytes[] = {0x10u, 0x20u, 0x30u};
        SInt32 value = 42;
        double when = 1234.5;
        char textBuffer[32];
        CFDataRef data = CFDataCreate(kCFAllocatorSystemDefault, bytes, (CFIndex)sizeof(bytes));
        CFNumberRef number = CFNumberCreate(kCFAllocatorSystemDefault, kCFNumberSInt32Type, &value);
        CFDateRef date = CFDateCreate(kCFAllocatorSystemDefault, when);
        CFStringRef string = CFStringCreateWithCString(kCFAllocatorSystemDefault, "launchd.packet-key", kCFStringEncodingASCII);
        CFErrorRef error = NULL;
        CFPropertyListFormat format = 0;
        const void *arrayValues[] = {string, data};
        CFArrayRef array = NULL;
        CFMutableDictionaryRef dictionary = NULL;
        CFDataRef plistBytes = NULL;
        CFPropertyListRef decoded = NULL;
        const void *decodedValue = NULL;

        if (data == NULL || number == NULL || date == NULL || string == NULL) {
            fail("scalar create failed");
        }
        if (!CFBooleanGetValue(kCFBooleanTrue)) {
            fail("boolean getter failed");
        }
        if (CFDataGetLength(data) != (CFIndex)sizeof(bytes)) {
            fail("data length mismatch");
        }
        if (CFNumberGetType(number) != kCFNumberSInt32Type) {
            fail("number type mismatch");
        }
        if (CFDateGetAbsoluteTime(date) != when) {
            fail("date roundtrip mismatch");
        }
        if (CFStringGetLength(string) != 18) {
            fail("string length mismatch");
        }
        if (!CFStringGetCString(string, textBuffer, (CFIndex)sizeof(textBuffer), kCFStringEncodingUTF8)) {
            fail("string UTF-8 export failed");
        }
        if (strcmp(textBuffer, "launchd.packet-key") != 0) {
            fail("string UTF-8 export mismatch");
        }

        array = CFArrayCreate(kCFAllocatorSystemDefault, arrayValues, 2, &kCFTypeArrayCallBacks);
        dictionary = CFDictionaryCreateMutable(
            kCFAllocatorSystemDefault,
            0,
            &kCFTypeDictionaryKeyCallBacks,
            &kCFTypeDictionaryValueCallBacks
        );
        if (array == NULL || dictionary == NULL) {
            fail("container create failed");
        }
        if (CFArrayGetCount(array) != 2) {
            fail("array count mismatch");
        }
        if (CFArrayGetValueAtIndex(array, 0) != (const void *)string) {
            fail("array borrowed get mismatch");
        }
        CFDictionarySetValue(dictionary, string, data);
        if (CFDictionaryGetCount(dictionary) != 1) {
            fail("dictionary count mismatch");
        }
        if (CFDictionaryGetValue(dictionary, string) != (const void *)data) {
            fail("dictionary borrowed get mismatch");
        }

        plistBytes = CFPropertyListCreateData(
            kCFAllocatorSystemDefault,
            (CFPropertyListRef)dictionary,
            kCFPropertyListBinaryFormat_v1_0,
            0,
            &error
        );
        if (plistBytes == NULL || error != NULL) {
            fail("binary plist write failed");
        }
        decoded = CFPropertyListCreateWithData(
            kCFAllocatorSystemDefault,
            plistBytes,
            kCFPropertyListImmutable,
            &format,
            &error
        );
        if (decoded == NULL || error != NULL) {
            fail("binary plist read failed");
        }
        if (format != kCFPropertyListBinaryFormat_v1_0) {
            fail("binary plist format mismatch");
        }
        if (CFGetTypeID(decoded) != CFDictionaryGetTypeID()) {
            fail("decoded plist type mismatch");
        }
        decodedValue = CFDictionaryGetValue((CFDictionaryRef)decoded, string);
        if (decodedValue == NULL || CFGetTypeID((CFTypeRef)decodedValue) != CFDataGetTypeID()) {
            fail("decoded plist value type mismatch");
        }
        if (CFDataGetLength((CFDataRef)decodedValue) != (CFIndex)sizeof(bytes)) {
            fail("decoded plist data length mismatch");
        }

        CFRelease(decoded);
        CFRelease(plistBytes);
        CFRelease((CFTypeRef)dictionary);
        CFRelease((CFTypeRef)array);
        CFRelease((CFTypeRef)string);
        CFRelease((CFTypeRef)date);
        CFRelease((CFTypeRef)number);
        CFRelease((CFTypeRef)data);
    }

    CFRelease((CFTypeRef)object);
    CFRelease((CFTypeRef)object);

    puts("PASS c_consumer_smoke");
    return 0;
}
