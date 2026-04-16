#include <CoreFoundation/CoreFoundation.h>

#include <signal.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <unistd.h>

struct TestObject {
    CFRuntimeBase base;
};

struct FinalizeRetainObject {
    CFRuntimeBase base;
};

struct NoFreeAllocatorStats {
    _Atomic(int) allocCalls;
    _Atomic(int) deallocCalls;
};

static void fail(const char *message) {
    fprintf(stderr, "FAIL: %s\n", message);
    exit(1);
}

static void expect_abort(void (*fn)(void), const char *name) {
    pid_t pid = fork();
    if (pid < 0) {
        fail("fork failed");
    }

    if (pid == 0) {
        struct rlimit coreLimit;
        coreLimit.rlim_cur = 0;
        coreLimit.rlim_max = 0;
        (void)setrlimit(RLIMIT_CORE, &coreLimit);
        fn();
        _exit(0);
    }

    int status = 0;
    if (waitpid(pid, &status, 0) < 0) {
        fail("waitpid failed");
    }
    if (!WIFSIGNALED(status) || WTERMSIG(status) != SIGABRT) {
        fprintf(stderr, "FAIL: %s did not abort with SIGABRT (status=%d)\n", name, status);
        exit(1);
    }
}

static CFTypeID register_test_type(const char *name, void (*finalize)(CFTypeRef)) {
    static CFRuntimeClass clsStorage[16];
    static int used = 0;

    if (used >= 16) {
        fail("too many abort test runtime classes");
    }

    clsStorage[used].version = 0;
    clsStorage[used].className = name;
    clsStorage[used].init = NULL;
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

static void *no_free_allocate(CFIndex allocSize, CFOptionFlags hint, void *info) {
    struct NoFreeAllocatorStats *stats = (struct NoFreeAllocatorStats *)info;
    atomic_fetch_add_explicit(&stats->allocCalls, 1, memory_order_relaxed);
    if (allocSize <= 0) {
        return NULL;
    }
    if (hint == 0x10000000UL) {
        return calloc(1, (size_t)allocSize);
    }
    return malloc((size_t)allocSize);
}

static void no_free_deallocate(void *ptr, void *info) {
    struct NoFreeAllocatorStats *stats = (struct NoFreeAllocatorStats *)info;
    atomic_fetch_add_explicit(&stats->deallocCalls, 1, memory_order_relaxed);
    (void)ptr;
}

static void finalize_and_retain(CFTypeRef cf) {
    (void)CFRetain(cf);
}

static void case_cfrelease_null(void) {
    CFRelease(NULL);
}

static void case_cfretain_null(void) {
    (void)CFRetain(NULL);
}

static void case_allocator_get_context_non_allocator(void) {
    CFTypeID typeID = register_test_type("AbortNonAllocator", NULL);
    struct TestObject *object = (struct TestObject *)_CFRuntimeCreateInstance(
        kCFAllocatorSystemDefault,
        typeID,
        (CFIndex)(sizeof(struct TestObject) - sizeof(CFRuntimeBase)),
        NULL
    );
    if (object == NULL) {
        fail("abort test object allocation failed");
    }

    CFAllocatorContext context;
    memset(&context, 0, sizeof(context));
    CFAllocatorGetContext((CFAllocatorRef)object, &context);
}

static struct TestObject *create_abort_test_object(const char *name) {
    CFTypeID typeID = register_test_type(name, NULL);
    struct TestObject *object = (struct TestObject *)_CFRuntimeCreateInstance(
        kCFAllocatorSystemDefault,
        typeID,
        (CFIndex)(sizeof(struct TestObject) - sizeof(CFRuntimeBase)),
        NULL
    );
    if (object == NULL) {
        fail("abort test object allocation failed");
    }
    return object;
}

static void case_boolean_get_value_non_boolean(void) {
    struct TestObject *object = create_abort_test_object("AbortNonBoolean");
    (void)CFBooleanGetValue((CFBooleanRef)object);
}

static void case_data_get_length_non_data(void) {
    struct TestObject *object = create_abort_test_object("AbortNonData");
    (void)CFDataGetLength((CFDataRef)object);
}

static void case_data_create_copy_non_data(void) {
    struct TestObject *object = create_abort_test_object("AbortNonDataCopy");
    (void)CFDataCreateCopy(kCFAllocatorSystemDefault, (CFDataRef)object);
}

static void case_data_get_byte_ptr_non_data(void) {
    struct TestObject *object = create_abort_test_object("AbortNonDataBytePtr");
    (void)CFDataGetBytePtr((CFDataRef)object);
}

static void case_number_get_type_non_number(void) {
    struct TestObject *object = create_abort_test_object("AbortNonNumberType");
    (void)CFNumberGetType((CFNumberRef)object);
}

static void case_number_get_value_non_number(void) {
    struct TestObject *object = create_abort_test_object("AbortNonNumber");
    SInt32 value = 0;
    (void)CFNumberGetValue((CFNumberRef)object, kCFNumberSInt32Type, &value);
}

static void case_date_get_absolute_time_non_date(void) {
    struct TestObject *object = create_abort_test_object("AbortNonDate");
    (void)CFDateGetAbsoluteTime((CFDateRef)object);
}

static void case_cfequal_null(void) {
    (void)CFEqual(NULL, (CFTypeRef)kCFBooleanTrue);
}

static void case_cfhash_null(void) {
    (void)CFHash(NULL);
}

static void case_double_release_detected(void) {
    struct NoFreeAllocatorStats stats;
    memset(&stats, 0, sizeof(stats));

    CFAllocatorContext context;
    memset(&context, 0, sizeof(context));
    context.version = 0;
    context.info = &stats;
    context.allocate = no_free_allocate;
    context.deallocate = no_free_deallocate;

    CFAllocatorRef allocator = CFAllocatorCreate(kCFAllocatorSystemDefault, &context);
    if (allocator == NULL) {
        fail("abort allocator creation failed");
    }

    CFTypeID typeID = register_test_type("AbortDoubleRelease", NULL);
    struct TestObject *object = (struct TestObject *)_CFRuntimeCreateInstance(
        allocator,
        typeID,
        (CFIndex)(sizeof(struct TestObject) - sizeof(CFRuntimeBase)),
        NULL
    );
    if (object == NULL) {
        fail("abort double-release object allocation failed");
    }

    CFRelease((CFTypeRef)object);
    CFRelease((CFTypeRef)object);
}

static void case_retain_in_finalize_aborts(void) {
    CFTypeID typeID = register_test_type("AbortRetainInFinalize", finalize_and_retain);
    struct FinalizeRetainObject *object = (struct FinalizeRetainObject *)_CFRuntimeCreateInstance(
        kCFAllocatorSystemDefault,
        typeID,
        (CFIndex)(sizeof(struct FinalizeRetainObject) - sizeof(CFRuntimeBase)),
        NULL
    );
    if (object == NULL) {
        fail("abort finalize-retain object allocation failed");
    }

    CFRelease((CFTypeRef)object);
}

int main(void) {
    expect_abort(case_cfrelease_null, "CFRelease(NULL)");
    expect_abort(case_cfretain_null, "CFRetain(NULL)");
    expect_abort(case_allocator_get_context_non_allocator, "CFAllocatorGetContext(non-allocator)");
    expect_abort(case_boolean_get_value_non_boolean, "CFBooleanGetValue(non-boolean)");
    expect_abort(case_data_get_length_non_data, "CFDataGetLength(non-data)");
    expect_abort(case_data_create_copy_non_data, "CFDataCreateCopy(non-data)");
    expect_abort(case_data_get_byte_ptr_non_data, "CFDataGetBytePtr(non-data)");
    expect_abort(case_number_get_type_non_number, "CFNumberGetType(non-number)");
    expect_abort(case_number_get_value_non_number, "CFNumberGetValue(non-number)");
    expect_abort(case_date_get_absolute_time_non_date, "CFDateGetAbsoluteTime(non-date)");
    expect_abort(case_cfequal_null, "CFEqual(NULL, object)");
    expect_abort(case_cfhash_null, "CFHash(NULL)");
    expect_abort(case_double_release_detected, "double release");
    expect_abort(case_retain_in_finalize_aborts, "CFRetain during finalization");

    puts("PASS runtime_abort_tests");
    return 0;
}
