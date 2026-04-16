#include <CoreFoundation/CoreFoundation.h>

#include <limits.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct TestObject {
    CFRuntimeBase base;
    uint64_t payload;
};

struct FinalizingObject {
    CFRuntimeBase base;
    _Atomic(int) *finalizeCount;
};

struct AllocatorStats {
    _Atomic(int) allocCalls;
    _Atomic(int) reallocCalls;
    _Atomic(int) deallocCalls;
    _Atomic(int) retainCalls;
    _Atomic(int) releaseCalls;
};

struct ThreadArgs {
    CFTypeRef object;
    int iterations;
};

static _Atomic(int) gFinalizeCount = 0;

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

static void expect_ptr(const void *actual, const void *expected, const char *message) {
    if (actual != expected) {
        fprintf(stderr, "FAIL: %s (actual=%p expected=%p)\n", message, actual, expected);
        exit(1);
    }
}

static void test_object_init(CFTypeRef cf) {
    struct TestObject *object = (struct TestObject *)cf;
    object->payload = 0x1122334455667788ULL;
}

static void finalizing_object_init(CFTypeRef cf) {
    struct FinalizingObject *object = (struct FinalizingObject *)cf;
    object->finalizeCount = &gFinalizeCount;
}

static void finalizing_object_finalize(CFTypeRef cf) {
    struct FinalizingObject *object = (struct FinalizingObject *)cf;
    atomic_fetch_add_explicit(object->finalizeCount, 1, memory_order_relaxed);
}

static const void *stats_retain(const void *info) {
    struct AllocatorStats *stats = (struct AllocatorStats *)info;
    atomic_fetch_add_explicit(&stats->retainCalls, 1, memory_order_relaxed);
    return info;
}

static void stats_release(const void *info) {
    struct AllocatorStats *stats = (struct AllocatorStats *)info;
    atomic_fetch_add_explicit(&stats->releaseCalls, 1, memory_order_relaxed);
}

static void *stats_allocate(CFIndex allocSize, CFOptionFlags hint, void *info) {
    struct AllocatorStats *stats = (struct AllocatorStats *)info;
    atomic_fetch_add_explicit(&stats->allocCalls, 1, memory_order_relaxed);
    if (allocSize <= 0) {
        return NULL;
    }
    if (hint == 0x10000000UL) {
        return calloc(1, (size_t)allocSize);
    }
    return malloc((size_t)allocSize);
}

static void *stats_reallocate(void *ptr, CFIndex newsize, CFOptionFlags hint, void *info) {
    (void)hint;
    struct AllocatorStats *stats = (struct AllocatorStats *)info;
    atomic_fetch_add_explicit(&stats->reallocCalls, 1, memory_order_relaxed);
    if (newsize <= 0) {
        free(ptr);
        return NULL;
    }
    return realloc(ptr, (size_t)newsize);
}

static void stats_deallocate(void *ptr, void *info) {
    struct AllocatorStats *stats = (struct AllocatorStats *)info;
    atomic_fetch_add_explicit(&stats->deallocCalls, 1, memory_order_relaxed);
    free(ptr);
}

static void *retain_release_worker(void *arg) {
    struct ThreadArgs *args = (struct ThreadArgs *)arg;
    for (int i = 0; i < args->iterations; ++i) {
        expect_ptr(CFRetain(args->object), args->object, "CFRetain must return the same pointer");
        CFRelease(args->object);
    }
    return NULL;
}

static CFTypeID register_test_type(const char *name, void (*init)(CFTypeRef), void (*finalize)(CFTypeRef)) {
    static CFRuntimeClass clsStorage[8];
    static int used = 0;

    if (used >= 8) {
        fail("too many test runtime classes");
    }

    clsStorage[used].version = 0;
    clsStorage[used].className = name;
    clsStorage[used].init = init;
    clsStorage[used].copy = NULL;
    clsStorage[used].finalize = finalize;
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

static void test_allocator_singletons(void) {
    expect_long((CFIndex)CFAllocatorGetTypeID(), 2, "CFAllocator type ID must be builtin allocator type");
    expect_ptr(CFRetain((CFTypeRef)kCFAllocatorSystemDefault), kCFAllocatorSystemDefault, "system allocator retain must be identity");
    expect_ptr(CFRetain((CFTypeRef)kCFAllocatorMalloc), kCFAllocatorMalloc, "malloc allocator retain must be identity");
    expect_ptr(CFRetain((CFTypeRef)kCFAllocatorNull), kCFAllocatorNull, "null allocator retain must be identity");
    expect_long(CFGetRetainCount((CFTypeRef)kCFAllocatorSystemDefault), LONG_MAX, "system allocator must be immortal");
    expect_long(CFGetRetainCount((CFTypeRef)kCFAllocatorMalloc), LONG_MAX, "malloc allocator must be immortal");
    expect_long(CFGetRetainCount((CFTypeRef)kCFAllocatorNull), LONG_MAX, "null allocator must be immortal");
    expect_long((CFIndex)CFGetTypeID((CFTypeRef)kCFAllocatorSystemDefault), 2, "system allocator type ID must be allocator");
    CFRelease((CFTypeRef)kCFAllocatorSystemDefault);
    CFRelease((CFTypeRef)kCFAllocatorMalloc);
    CFRelease((CFTypeRef)kCFAllocatorNull);
}

static void test_runtime_registration_and_init(void) {
    CFTypeID typeID = register_test_type("TestObject", test_object_init, NULL);
    const CFRuntimeClass *cls = _CFRuntimeGetClassWithTypeID(typeID);
    expect_true(cls != NULL, "registered class must be discoverable");
    expect_true(strcmp(cls->className, "TestObject") == 0, "registered class name mismatch");

    struct TestObject *object = (struct TestObject *)_CFRuntimeCreateInstance(kCFAllocatorSystemDefault, typeID, (CFIndex)(sizeof(struct TestObject) - sizeof(CFRuntimeBase)), NULL);
    expect_true(object != NULL, "runtime object allocation failed");
    expect_long((CFIndex)CFGetTypeID((CFTypeRef)object), (CFIndex)typeID, "runtime object type ID mismatch");
    expect_long(CFGetRetainCount((CFTypeRef)object), 1, "new runtime object must start retained once");
    expect_true(object->payload == 0x1122334455667788ULL, "runtime init callback did not run");
    expect_ptr(CFGetAllocator((CFTypeRef)object), kCFAllocatorSystemDefault, "default-allocated object must report system allocator");
    CFRelease((CFTypeRef)object);
}

static void test_static_instance_immortality(void) {
    static struct TestObject staticObject;
    CFTypeID typeID = register_test_type("StaticTestObject", NULL, NULL);

    memset(&staticObject, 0, sizeof(staticObject));
    _CFRuntimeInitStaticInstance(&staticObject, typeID);

    expect_long((CFIndex)CFGetTypeID((CFTypeRef)&staticObject), (CFIndex)typeID, "static instance type ID mismatch");
    expect_long(CFGetRetainCount((CFTypeRef)&staticObject), LONG_MAX, "static instance must be immortal");
    expect_ptr(CFRetain((CFTypeRef)&staticObject), &staticObject, "static retain must be identity");
    CFRelease((CFTypeRef)&staticObject);
}

static void test_unknown_type_and_null_allocator(void) {
    expect_true(_CFRuntimeCreateInstance(kCFAllocatorSystemDefault, 9999, 0, NULL) == NULL, "unknown type must fail allocation");
    expect_true(CFAllocatorAllocate(kCFAllocatorNull, 64, 0) == NULL, "null allocator must refuse allocation");

    CFTypeID typeID = register_test_type("NullAllocatorObject", NULL, NULL);
    expect_true(_CFRuntimeCreateInstance(kCFAllocatorNull, typeID, 0, NULL) == NULL, "runtime create must fail under null allocator");
}

static void test_retain_release_and_finalize(void) {
    atomic_store_explicit(&gFinalizeCount, 0, memory_order_relaxed);
    CFTypeID typeID = register_test_type("FinalizingObject", finalizing_object_init, finalizing_object_finalize);
    struct FinalizingObject *object = (struct FinalizingObject *)_CFRuntimeCreateInstance(kCFAllocatorSystemDefault, typeID, (CFIndex)(sizeof(struct FinalizingObject) - sizeof(CFRuntimeBase)), NULL);
    expect_true(object != NULL, "finalizing object allocation failed");
    expect_long(CFGetRetainCount((CFTypeRef)object), 1, "finalizing object must start at retain count 1");

    expect_ptr(CFRetain((CFTypeRef)object), object, "retain must return object");
    expect_long(CFGetRetainCount((CFTypeRef)object), 2, "retain must increment count");
    CFRelease((CFTypeRef)object);
    expect_long(CFGetRetainCount((CFTypeRef)object), 1, "release must decrement count");
    expect_long((CFIndex)atomic_load_explicit(&gFinalizeCount, memory_order_relaxed), 0, "finalize must not run before last release");
    CFRelease((CFTypeRef)object);
    expect_long((CFIndex)atomic_load_explicit(&gFinalizeCount, memory_order_relaxed), 1, "finalize must run exactly once");
}

static void test_custom_allocator_and_default_switch(void) {
    struct AllocatorStats stats;
    memset(&stats, 0, sizeof(stats));

    CFAllocatorContext context;
    memset(&context, 0, sizeof(context));
    context.version = 0;
    context.info = &stats;
    context.retain = stats_retain;
    context.release = stats_release;
    context.allocate = stats_allocate;
    context.reallocate = stats_reallocate;
    context.deallocate = stats_deallocate;

    CFAllocatorRef allocator = CFAllocatorCreate(kCFAllocatorSystemDefault, &context);
    expect_true(allocator != NULL, "custom allocator creation failed");
    expect_long((CFIndex)atomic_load_explicit(&stats.retainCalls, memory_order_relaxed), 1, "allocator context retain must run once");

    CFAllocatorRef originalDefault = CFAllocatorGetDefault();
    CFAllocatorSetDefault(allocator);
    expect_ptr(CFAllocatorGetDefault(), allocator, "default allocator switch failed");

    CFTypeID typeID = register_test_type("CustomAllocatedObject", NULL, NULL);
    struct TestObject *object = (struct TestObject *)_CFRuntimeCreateInstance(NULL, typeID, (CFIndex)(sizeof(struct TestObject) - sizeof(CFRuntimeBase)), NULL);
    expect_true(object != NULL, "allocation through default custom allocator failed");
    expect_ptr(CFGetAllocator((CFTypeRef)object), allocator, "object must report custom allocator");
    expect_true(atomic_load_explicit(&stats.allocCalls, memory_order_relaxed) > 0, "custom allocator allocate callback must run");

    CFRelease((CFTypeRef)object);
    expect_true(atomic_load_explicit(&stats.deallocCalls, memory_order_relaxed) > 0, "custom allocator deallocate callback must run");

    CFAllocatorSetDefault(originalDefault);
    expect_ptr(CFAllocatorGetDefault(), originalDefault, "default allocator restore failed");
    CFRelease((CFTypeRef)allocator);
    expect_long((CFIndex)atomic_load_explicit(&stats.releaseCalls, memory_order_relaxed), 1, "allocator context release must run once");
}

static void test_reallocate_requires_callback(void) {
    struct AllocatorStats stats;
    memset(&stats, 0, sizeof(stats));

    CFAllocatorContext context;
    memset(&context, 0, sizeof(context));
    context.version = 0;
    context.info = &stats;
    context.allocate = stats_allocate;
    context.deallocate = stats_deallocate;

    CFAllocatorRef allocator = CFAllocatorCreate(kCFAllocatorSystemDefault, &context);
    expect_true(allocator != NULL, "allocator without reallocate callback must still be creatable");

    void *ptr = CFAllocatorAllocate(allocator, 32, 0);
    expect_true(ptr != NULL, "allocator allocate must succeed");
    expect_true(CFAllocatorReallocate(allocator, ptr, 64, 0) == NULL, "reallocate without callback must fail");
    expect_long((CFIndex)atomic_load_explicit(&stats.deallocCalls, memory_order_relaxed), 0, "failed reallocate must not free the original pointer");

    CFAllocatorDeallocate(allocator, ptr);
    expect_long((CFIndex)atomic_load_explicit(&stats.deallocCalls, memory_order_relaxed), 1, "explicit deallocate must free the original pointer");
    CFRelease((CFTypeRef)allocator);
}

static void test_concurrent_retain_release(void) {
    enum { threadCount = 6, iterations = 50000 };
    pthread_t threads[threadCount];
    struct ThreadArgs args;

    atomic_store_explicit(&gFinalizeCount, 0, memory_order_relaxed);
    CFTypeID typeID = register_test_type("ConcurrentObject", finalizing_object_init, finalizing_object_finalize);
    struct FinalizingObject *object = (struct FinalizingObject *)_CFRuntimeCreateInstance(kCFAllocatorSystemDefault, typeID, (CFIndex)(sizeof(struct FinalizingObject) - sizeof(CFRuntimeBase)), NULL);
    expect_true(object != NULL, "concurrent test object allocation failed");

    args.object = (CFTypeRef)object;
    args.iterations = iterations;
    for (int i = 0; i < threadCount; ++i) {
        if (pthread_create(&threads[i], NULL, retain_release_worker, &args) != 0) {
            fail("pthread_create failed");
        }
    }
    for (int i = 0; i < threadCount; ++i) {
        if (pthread_join(threads[i], NULL) != 0) {
            fail("pthread_join failed");
        }
    }

    expect_long(CFGetRetainCount((CFTypeRef)object), 1, "balanced concurrent retain/release must preserve count");
    expect_long((CFIndex)atomic_load_explicit(&gFinalizeCount, memory_order_relaxed), 0, "concurrent test must not finalize early");
    CFRelease((CFTypeRef)object);
    expect_long((CFIndex)atomic_load_explicit(&gFinalizeCount, memory_order_relaxed), 1, "concurrent test must finalize once");
}

int main(void) {
    test_allocator_singletons();
    test_runtime_registration_and_init();
    test_static_instance_immortality();
    test_unknown_type_and_null_allocator();
    test_retain_release_and_finalize();
    test_custom_allocator_and_default_switch();
    test_reallocate_requires_callback();
    test_concurrent_retain_release();

    puts("PASS runtime_ownership_tests");
    return 0;
}
