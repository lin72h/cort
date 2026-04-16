#include <CoreFoundation/CoreFoundation.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    kMaxAPICalls = 6,
    kMaxText = 256
};

struct CaseResult {
    const char *name;
    bool success;
    const char *classification;
    const char *ownershipNote;
    const char *apiCalls[kMaxAPICalls];
    size_t apiCallCount;
    bool pointerIdentitySame;
    bool hasPointerIdentity;
    long typeID;
    bool hasTypeID;
    long retainCountBefore;
    long retainCountAfterRetain;
    long retainCountAfterRelease;
    bool hasRetainDiagnostics;
    long retainCallbackCount;
    bool hasRetainCallbackCount;
    long releaseCallbackCount;
    bool hasReleaseCallbackCount;
    char note[kMaxText];
};

struct CallbackStats {
    long retainCount;
    long releaseCount;
    long allocCount;
    long reallocCount;
    long deallocCount;
};

static void json_print_string(FILE *stream, const char *value) {
    fputc('"', stream);
    if (value != NULL) {
        for (const unsigned char *cursor = (const unsigned char *)value; *cursor != '\0'; ++cursor) {
            switch (*cursor) {
                case '\\':
                    fputs("\\\\", stream);
                    break;
                case '"':
                    fputs("\\\"", stream);
                    break;
                case '\n':
                    fputs("\\n", stream);
                    break;
                case '\r':
                    fputs("\\r", stream);
                    break;
                case '\t':
                    fputs("\\t", stream);
                    break;
                default:
                    if (*cursor < 0x20u) {
                        fprintf(stream, "\\u%04x", (unsigned int)*cursor);
                    } else {
                        fputc((int)*cursor, stream);
                    }
                    break;
            }
        }
    }
    fputc('"', stream);
}

static void json_print_long_or_null(FILE *stream, bool hasValue, long value) {
    if (!hasValue) {
        fputs("null", stream);
        return;
    }
    fprintf(stream, "%ld", value);
}

static void json_print_bool_or_null(FILE *stream, bool hasValue, bool value) {
    if (!hasValue) {
        fputs("null", stream);
        return;
    }
    fputs(value ? "true" : "false", stream);
}

static void json_print_api_calls(FILE *stream, const struct CaseResult *result) {
    fputc('[', stream);
    for (size_t i = 0; i < result->apiCallCount; ++i) {
        if (i != 0) {
            fputs(", ", stream);
        }
        json_print_string(stream, result->apiCalls[i]);
    }
    fputc(']', stream);
}

static void print_result(FILE *stream, const struct CaseResult *result, bool trailingComma) {
    fputs("    {\n", stream);
    fputs("      \"name\": ", stream);
    json_print_string(stream, result->name);
    fputs(",\n      \"success\": ", stream);
    fputs(result->success ? "true" : "false", stream);
    fputs(",\n      \"classification\": ", stream);
    json_print_string(stream, result->classification);
    fputs(",\n      \"api_calls\": ", stream);
    json_print_api_calls(stream, result);
    fputs(",\n      \"pointer_identity_same\": ", stream);
    json_print_bool_or_null(stream, result->hasPointerIdentity, result->pointerIdentitySame);
    fputs(",\n      \"type_id\": ", stream);
    json_print_long_or_null(stream, result->hasTypeID, result->typeID);
    fputs(",\n      \"retain_count_before\": ", stream);
    json_print_long_or_null(stream, result->hasRetainDiagnostics, result->retainCountBefore);
    fputs(",\n      \"retain_count_after_retain\": ", stream);
    json_print_long_or_null(stream, result->hasRetainDiagnostics, result->retainCountAfterRetain);
    fputs(",\n      \"retain_count_after_release\": ", stream);
    json_print_long_or_null(stream, result->hasRetainDiagnostics, result->retainCountAfterRelease);
    fputs(",\n      \"retain_callback_count\": ", stream);
    json_print_long_or_null(stream, result->hasRetainCallbackCount, result->retainCallbackCount);
    fputs(",\n      \"release_callback_count\": ", stream);
    json_print_long_or_null(stream, result->hasReleaseCallbackCount, result->releaseCallbackCount);
    fputs(",\n      \"ownership_note\": ", stream);
    json_print_string(stream, result->ownershipNote);
    fputs(",\n      \"note\": ", stream);
    json_print_string(stream, result->note);
    fputs("\n    }", stream);
    if (trailingComma) {
        fputc(',', stream);
    }
    fputc('\n', stream);
}

static const void *stats_retain(const void *info) {
    struct CallbackStats *stats = (struct CallbackStats *)info;
    stats->retainCount += 1;
    return info;
}

static void stats_release(const void *info) {
    struct CallbackStats *stats = (struct CallbackStats *)info;
    stats->releaseCount += 1;
}

static void *stats_allocate(CFIndex allocSize, CFOptionFlags hint, void *info) {
    struct CallbackStats *stats = (struct CallbackStats *)info;
    stats->allocCount += 1;
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
    struct CallbackStats *stats = (struct CallbackStats *)info;
    stats->reallocCount += 1;
    if (newsize <= 0) {
        free(ptr);
        return NULL;
    }
    return realloc(ptr, (size_t)newsize);
}

static void stats_deallocate(void *ptr, void *info) {
    struct CallbackStats *stats = (struct CallbackStats *)info;
    stats->deallocCount += 1;
    free(ptr);
}

static void set_api_calls(struct CaseResult *result, const char **apiCalls, size_t apiCallCount) {
    result->apiCallCount = apiCallCount;
    for (size_t i = 0; i < apiCallCount && i < kMaxAPICalls; ++i) {
        result->apiCalls[i] = apiCalls[i];
    }
}

static struct CaseResult make_default_allocator_case(void) {
    static const char *apiCalls[] = { "CFAllocatorGetDefault" };
    struct CaseResult result;
    memset(&result, 0, sizeof(result));
    result.name = "allocator_default_identity";
    result.classification = "required";
    result.ownershipNote = "Initial default allocator identity is public allocator behavior relevant to Subset 0.";
    set_api_calls(&result, apiCalls, sizeof(apiCalls) / sizeof(apiCalls[0]));
    result.hasPointerIdentity = true;
    result.pointerIdentitySame = CFAllocatorGetDefault() == kCFAllocatorSystemDefault;
    result.success = result.pointerIdentitySame;
    snprintf(result.note, sizeof(result.note), "%s", "Checks whether the initial default allocator is the system default singleton.");
    return result;
}

static struct CaseResult make_allocator_type_case(void) {
    static const char *apiCalls[] = { "CFAllocatorGetTypeID", "CFGetTypeID" };
    struct CaseResult result;
    memset(&result, 0, sizeof(result));
    result.name = "allocator_type_identity";
    result.classification = "required";
    result.ownershipNote = "Allocator singleton should report the allocator type ID.";
    set_api_calls(&result, apiCalls, sizeof(apiCalls) / sizeof(apiCalls[0]));
    result.hasTypeID = true;
    result.typeID = (long)CFGetTypeID((CFTypeRef)kCFAllocatorSystemDefault);
    result.success = result.typeID == (long)CFAllocatorGetTypeID();
    snprintf(result.note, sizeof(result.note), "%s", "Numeric type IDs may differ across implementations; equality within the process is what matters.");
    return result;
}

static struct CaseResult make_allocator_singleton_retain_case(void) {
    static const char *apiCalls[] = { "CFRetain", "CFRelease", "CFGetRetainCount" };
    struct CaseResult result;
    memset(&result, 0, sizeof(result));
    result.name = "allocator_singleton_retain_identity";
    result.classification = "required";
    result.ownershipNote = "Static allocator singletons should retain by pointer identity and release safely.";
    set_api_calls(&result, apiCalls, sizeof(apiCalls) / sizeof(apiCalls[0]));
    result.hasPointerIdentity = true;
    result.hasRetainDiagnostics = true;
    result.retainCountBefore = (long)CFGetRetainCount((CFTypeRef)kCFAllocatorSystemDefault);
    result.pointerIdentitySame = CFRetain((CFTypeRef)kCFAllocatorSystemDefault) == (CFTypeRef)kCFAllocatorSystemDefault;
    result.retainCountAfterRetain = (long)CFGetRetainCount((CFTypeRef)kCFAllocatorSystemDefault);
    CFRelease((CFTypeRef)kCFAllocatorSystemDefault);
    result.retainCountAfterRelease = (long)CFGetRetainCount((CFTypeRef)kCFAllocatorSystemDefault);
    result.success = result.pointerIdentitySame;
    snprintf(result.note, sizeof(result.note), "%s", "Retain-count numerics are diagnostic only.");
    return result;
}

static struct CaseResult make_allocator_null_case(void) {
    static const char *apiCalls[] = { "CFAllocatorAllocate" };
    struct CaseResult result;
    memset(&result, 0, sizeof(result));
    result.name = "allocator_null_allocate";
    result.classification = "required";
    result.ownershipNote = "Null allocator should refuse allocation.";
    set_api_calls(&result, apiCalls, sizeof(apiCalls) / sizeof(apiCalls[0]));
    result.success = CFAllocatorAllocate(kCFAllocatorNull, 64, 0) == NULL;
    snprintf(result.note, sizeof(result.note), "%s", "Expected semantic result is NULL, not a crash.");
    return result;
}

static struct CaseResult make_allocator_context_roundtrip_case(void) {
    static const char *apiCalls[] = { "CFAllocatorCreate", "CFAllocatorGetContext", "CFRelease" };
    struct CaseResult result;
    struct CallbackStats stats;
    memset(&result, 0, sizeof(result));
    memset(&stats, 0, sizeof(stats));

    result.name = "allocator_context_roundtrip";
    result.classification = "required";
    result.ownershipNote = "Allocator context retain/release and get-context roundtrip are public allocator semantics.";
    set_api_calls(&result, apiCalls, sizeof(apiCalls) / sizeof(apiCalls[0]));

    CFAllocatorContext input;
    memset(&input, 0, sizeof(input));
    input.version = 0;
    input.info = &stats;
    input.retain = stats_retain;
    input.release = stats_release;
    input.allocate = stats_allocate;
    input.reallocate = stats_reallocate;
    input.deallocate = stats_deallocate;

    CFAllocatorRef allocator = CFAllocatorCreate(kCFAllocatorSystemDefault, &input);
    CFAllocatorContext output;
    memset(&output, 0, sizeof(output));
    CFAllocatorGetContext(allocator, &output);

    result.hasRetainCallbackCount = true;
    result.retainCallbackCount = stats.retainCount;
    CFRelease((CFTypeRef)allocator);
    result.hasReleaseCallbackCount = true;
    result.releaseCallbackCount = stats.releaseCount;
    result.success = allocator != NULL &&
        output.info == &stats &&
        output.allocate == stats_allocate &&
        stats.retainCount >= 1 &&
        stats.releaseCount >= 1;
    snprintf(result.note, sizeof(result.note), "%s", "Compares allocator context storage and callback ownership behavior.");
    return result;
}

static struct CaseResult make_allocator_default_switch_case(void) {
    static const char *apiCalls[] = { "CFAllocatorCreate", "CFAllocatorSetDefault", "CFAllocatorGetDefault", "CFRelease" };
    struct CaseResult result;
    struct CallbackStats stats;
    memset(&result, 0, sizeof(result));
    memset(&stats, 0, sizeof(stats));

    result.name = "allocator_default_switch";
    result.classification = "required";
    result.ownershipNote = "Default allocator switching is public behavior relevant to runtime allocation bootstrap.";
    set_api_calls(&result, apiCalls, sizeof(apiCalls) / sizeof(apiCalls[0]));

    CFAllocatorContext context;
    memset(&context, 0, sizeof(context));
    context.version = 0;
    context.info = &stats;
    context.allocate = stats_allocate;
    context.reallocate = stats_reallocate;
    context.deallocate = stats_deallocate;

    CFAllocatorRef allocator = CFAllocatorCreate(kCFAllocatorSystemDefault, &context);
    CFAllocatorRef originalDefault = CFAllocatorGetDefault();
    CFAllocatorSetDefault(allocator);
    result.hasPointerIdentity = true;
    result.pointerIdentitySame = CFAllocatorGetDefault() == allocator;
    CFAllocatorSetDefault(originalDefault);
    CFRelease((CFTypeRef)allocator);
    result.success = result.pointerIdentitySame && CFAllocatorGetDefault() == originalDefault;
    snprintf(result.note, sizeof(result.note), "%s", "On MX this may be thread-scoped; comparison should stay within one thread.");
    return result;
}

static struct CaseResult make_allocator_reallocate_with_callback_case(void) {
    static const char *apiCalls[] = { "CFAllocatorCreate", "CFAllocatorAllocate", "CFAllocatorReallocate", "CFAllocatorDeallocate", "CFRelease" };
    struct CaseResult result;
    struct CallbackStats stats;
    memset(&result, 0, sizeof(result));
    memset(&stats, 0, sizeof(stats));

    result.name = "allocator_reallocate_with_callback";
    result.classification = "required";
    result.ownershipNote = "Allocator reallocate should use the provided callback when present.";
    set_api_calls(&result, apiCalls, sizeof(apiCalls) / sizeof(apiCalls[0]));

    CFAllocatorContext context;
    memset(&context, 0, sizeof(context));
    context.version = 0;
    context.info = &stats;
    context.allocate = stats_allocate;
    context.reallocate = stats_reallocate;
    context.deallocate = stats_deallocate;

    CFAllocatorRef allocator = CFAllocatorCreate(kCFAllocatorSystemDefault, &context);
    void *ptr = CFAllocatorAllocate(allocator, 32, 0);
    void *resized = CFAllocatorReallocate(allocator, ptr, 64, 0);
    if (resized != NULL) {
        CFAllocatorDeallocate(allocator, resized);
    }
    CFRelease((CFTypeRef)allocator);

    result.hasRetainCallbackCount = true;
    result.retainCallbackCount = stats.reallocCount;
    result.hasReleaseCallbackCount = true;
    result.releaseCallbackCount = stats.deallocCount;
    result.success = ptr != NULL && resized != NULL && stats.reallocCount >= 1 && stats.deallocCount >= 1;
    snprintf(result.note, sizeof(result.note), "%s", "Tracks callback usage rather than byte-for-byte allocation strategy.");
    return result;
}

static struct CaseResult make_allocator_reallocate_without_callback_case(void) {
    static const char *apiCalls[] = { "CFAllocatorCreate", "CFAllocatorAllocate", "CFAllocatorReallocate", "CFAllocatorDeallocate", "CFRelease" };
    struct CaseResult result;
    struct CallbackStats stats;
    memset(&result, 0, sizeof(result));
    memset(&stats, 0, sizeof(stats));

    result.name = "allocator_reallocate_without_callback";
    result.classification = "semantic-match-only";
    result.ownershipNote = "Used to detect variance when no explicit reallocate callback exists.";
    set_api_calls(&result, apiCalls, sizeof(apiCalls) / sizeof(apiCalls[0]));

    CFAllocatorContext context;
    memset(&context, 0, sizeof(context));
    context.version = 0;
    context.info = &stats;
    context.allocate = stats_allocate;
    context.deallocate = stats_deallocate;

    CFAllocatorRef allocator = CFAllocatorCreate(kCFAllocatorSystemDefault, &context);
    void *ptr = CFAllocatorAllocate(allocator, 32, 0);
    void *resized = CFAllocatorReallocate(allocator, ptr, 64, 0);
    if (resized != NULL) {
        CFAllocatorDeallocate(allocator, resized);
    } else if (ptr != NULL) {
        CFAllocatorDeallocate(allocator, ptr);
    }
    CFRelease((CFTypeRef)allocator);

    result.success = true;
    snprintf(result.note, sizeof(result.note), "Observed result pointer was %s. FX intentionally returns NULL here when no callback is provided.", resized != NULL ? "non-NULL" : "NULL");
    return result;
}

int main(void) {
    struct CaseResult results[8];
    size_t resultCount = 0;

    results[resultCount++] = make_default_allocator_case();
    results[resultCount++] = make_allocator_type_case();
    results[resultCount++] = make_allocator_singleton_retain_case();
    results[resultCount++] = make_allocator_null_case();
    results[resultCount++] = make_allocator_context_roundtrip_case();
    results[resultCount++] = make_allocator_default_switch_case();
    results[resultCount++] = make_allocator_reallocate_with_callback_case();
    results[resultCount++] = make_allocator_reallocate_without_callback_case();

    fputs("{\n", stdout);
    fputs("  \"suite\": ", stdout);
    json_print_string(stdout, "subset0_public_allocator_compare");
    fputs(",\n  \"results\": [\n", stdout);
    for (size_t i = 0; i < resultCount; ++i) {
        print_result(stdout, &results[i], i + 1u < resultCount);
    }
    fputs("  ]\n}\n", stdout);
    return 0;
}
