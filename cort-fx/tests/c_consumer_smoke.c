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

    CFRelease((CFTypeRef)object);
    CFRelease((CFTypeRef)object);

    puts("PASS c_consumer_smoke");
    return 0;
}
