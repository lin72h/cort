#include <CoreFoundation/CoreFoundation.h>

#include <stdio.h>
#include <stdlib.h>

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
        CFDataRef data = CFDataCreate(kCFAllocatorSystemDefault, bytes, (CFIndex)sizeof(bytes));
        CFNumberRef number = CFNumberCreate(kCFAllocatorSystemDefault, kCFNumberSInt32Type, &value);
        CFDateRef date = CFDateCreate(kCFAllocatorSystemDefault, when);

        if (data == NULL || number == NULL || date == NULL) {
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

        CFRelease((CFTypeRef)date);
        CFRelease((CFTypeRef)number);
        CFRelease((CFTypeRef)data);
    }

    CFRelease((CFTypeRef)object);
    CFRelease((CFTypeRef)object);

    puts("PASS c_consumer_smoke");
    return 0;
}
