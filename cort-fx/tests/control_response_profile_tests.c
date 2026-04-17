#include "subset7d_control_response_profile_support.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const struct FXSubset7ACase *find_case(const char *name) {
    for (size_t index = 0u; index < kFXSubset7ACaseCount; ++index) {
        if (strcmp(kFXSubset7ACases[index].name, name) == 0) {
            return &kFXSubset7ACases[index];
        }
    }
    return NULL;
}

static void require_text(CFStringRef string, const char *expected, const char *label) {
    char buffer[256];

    if (string == NULL || expected == NULL || !CFStringGetCString(string, buffer, sizeof(buffer), kCFStringEncodingUTF8)) {
        fprintf(stderr, "FAIL: %s missing or undecodable\n", label);
        exit(1);
    }
    if (strcmp(buffer, expected) != 0) {
        fprintf(stderr, "FAIL: %s expected=%s actual=%s\n", label, expected, buffer);
        exit(1);
    }
}

static CFDataRef payload_for_case(const char *name) {
    const struct FXSubset7ACase *case_data = find_case(name);
    if (case_data == NULL) {
        fprintf(stderr, "FAIL: missing case %s\n", name);
        exit(1);
    }
    CFDataRef payload = CFDataCreate(kCFAllocatorSystemDefault, case_data->payload, (CFIndex)case_data->payload_length);
    if (payload == NULL) {
        fprintf(stderr, "FAIL: payload allocation failed for %s\n", name);
        exit(1);
    }
    return payload;
}

static void verify_error_profile(void) {
    struct _FXCFControlResponseProfile profile = {0};
    struct _FXCFControlPacketError error = {0};
    CFDataRef payload = payload_for_case("response.notify.invalid_token.failure");
    char *summary = NULL;

    if (!_FXCFControlResponseProfileInit(kCFAllocatorSystemDefault, payload, &profile, &error)) {
        fprintf(stderr, "FAIL: error profile decode failed kind=%d text=%s\n", (int)error.kind, error.text);
        exit(1);
    }

    if (profile.kind != _FXCFControlResponseProfileKindError || profile.response.status != _FXCFControlResponseStatusError) {
        fprintf(stderr, "FAIL: error profile classification mismatch\n");
        exit(1);
    }

    require_text(profile.response.errorCode, "notify_failed", "profile.error.code");
    summary = _FXCFControlResponseProfileCopySummary(&profile);
    if (summary == NULL || strcmp(summary, "profile error code=notify_failed") != 0) {
        fprintf(stderr, "FAIL: error profile summary mismatch actual=%s\n", summary != NULL ? summary : "null");
        free(summary);
        exit(1);
    }

    free(summary);
    _FXCFControlResponseProfileClear(&profile);
    CFRelease((CFTypeRef)payload);
}

static void verify_notify_post_profile(void) {
    struct _FXCFControlResponseProfile profile = {0};
    struct _FXCFControlPacketError error = {0};
    CFDataRef payload = payload_for_case("response.notify.post.success");
    char *summary = NULL;

    if (!_FXCFControlResponseProfileInit(kCFAllocatorSystemDefault, payload, &profile, &error)) {
        fprintf(stderr, "FAIL: notify post profile decode failed kind=%d text=%s\n", (int)error.kind, error.text);
        exit(1);
    }

    if (profile.kind != _FXCFControlResponseProfileKindNotifyPost ||
        !profile.hasGeneration || profile.generation != 5 ||
        !profile.hasState || profile.state != 7 ||
        profile.deliveredTokens == NULL || CFArrayGetCount(profile.deliveredTokens) != 1) {
        fprintf(stderr, "FAIL: notify post profile fields mismatch\n");
        exit(1);
    }

    summary = _FXCFControlResponseProfileCopySummary(&profile);
    if (summary == NULL || strcmp(summary, "profile notify.post delivered=1 generation=5") != 0) {
        fprintf(stderr, "FAIL: notify post summary mismatch actual=%s\n", summary != NULL ? summary : "null");
        free(summary);
        exit(1);
    }

    free(summary);
    _FXCFControlResponseProfileClear(&profile);
    CFRelease((CFTypeRef)payload);
}

static void verify_registration_profile(void) {
    struct _FXCFControlResponseProfile profile = {0};
    struct _FXCFControlPacketError error = {0};
    CFDataRef payload = payload_for_case("response.notify.registration.file_descriptor.success");

    if (!_FXCFControlResponseProfileInit(kCFAllocatorSystemDefault, payload, &profile, &error)) {
        fprintf(stderr, "FAIL: registration profile decode failed kind=%d text=%s\n", (int)error.kind, error.text);
        exit(1);
    }

    if (profile.kind != _FXCFControlResponseProfileKindNotifyRegistrationWrapper ||
        !profile.hasToken || profile.token != 77 ||
        !profile.hasValid || !profile.valid) {
        fprintf(stderr, "FAIL: registration wrapper fields mismatch\n");
        exit(1);
    }

    _FXCFControlResponseProfileClear(&profile);
    CFRelease((CFTypeRef)payload);
}

static void verify_registration_pending_generation_profile(void) {
    struct _FXCFControlResponseProfile profile = {0};
    struct _FXCFControlPacketError error = {0};
    CFDataRef payload = payload_for_case("response.notify.resume.failed_after_pending_suspend");

    if (!_FXCFControlResponseProfileInit(kCFAllocatorSystemDefault, payload, &profile, &error)) {
        fprintf(stderr, "FAIL: pending registration profile decode failed kind=%d text=%s\n", (int)error.kind, error.text);
        exit(1);
    }

    if (profile.kind != _FXCFControlResponseProfileKindNotifyRegistration ||
        !profile.hasToken || profile.token != 77 ||
        !profile.hasPendingGeneration || profile.pendingGeneration != 5) {
        fprintf(stderr, "FAIL: pending registration profile fields mismatch\n");
        exit(1);
    }

    _FXCFControlResponseProfileClear(&profile);
    CFRelease((CFTypeRef)payload);
}

static void verify_health_profile(void) {
    struct _FXCFControlResponseProfile profile = {0};
    struct _FXCFControlPacketError error = {0};
    CFDataRef payload = payload_for_case("response.control.health.notify_persistence.degraded");
    char *summary = NULL;

    if (!_FXCFControlResponseProfileInit(kCFAllocatorSystemDefault, payload, &profile, &error)) {
        fprintf(stderr, "FAIL: health profile decode failed kind=%d text=%s\n", (int)error.kind, error.text);
        exit(1);
    }

    if (profile.kind != _FXCFControlResponseProfileKindControlHealth ||
        profile.reasons == NULL || CFArrayGetCount(profile.reasons) != 1) {
        fprintf(stderr, "FAIL: health profile fields mismatch\n");
        exit(1);
    }

    summary = _FXCFControlResponseProfileCopySummary(&profile);
    if (summary == NULL || strcmp(summary, "profile control.health state=degraded reasons=1") != 0) {
        fprintf(stderr, "FAIL: health summary mismatch actual=%s\n", summary != NULL ? summary : "null");
        free(summary);
        exit(1);
    }

    free(summary);
    _FXCFControlResponseProfileClear(&profile);
    CFRelease((CFTypeRef)payload);
}

static void verify_diagnostics_profile(void) {
    struct _FXCFControlResponseProfile profile = {0};
    struct _FXCFControlPacketError error = {0};
    CFDataRef payload = payload_for_case("response.diagnostics.snapshot.notify_fast_path.degraded");
    char *summary = NULL;

    if (!_FXCFControlResponseProfileInit(kCFAllocatorSystemDefault, payload, &profile, &error)) {
        fprintf(stderr, "FAIL: diagnostics profile decode failed kind=%d text=%s\n", (int)error.kind, error.text);
        exit(1);
    }

    if (profile.kind != _FXCFControlResponseProfileKindDiagnosticsSnapshot ||
        profile.notifyNames == NULL || CFArrayGetCount(profile.notifyNames) != 1) {
        fprintf(stderr, "FAIL: diagnostics profile fields mismatch\n");
        exit(1);
    }

    summary = _FXCFControlResponseProfileCopySummary(&profile);
    if (summary == NULL || strcmp(summary, "profile diagnostics.snapshot jobs=0 notify_names=1") != 0) {
        fprintf(stderr, "FAIL: diagnostics summary mismatch actual=%s\n", summary != NULL ? summary : "null");
        free(summary);
        exit(1);
    }

    free(summary);
    _FXCFControlResponseProfileClear(&profile);
    CFRelease((CFTypeRef)payload);
}

static void verify_generic_object_profile(void) {
    struct _FXCFControlResponseProfile profile = {0};
    struct _FXCFControlPacketError error = {0};
    CFDataRef payload = payload_for_case("response.scalar_coverage.success");

    if (!_FXCFControlResponseProfileInit(kCFAllocatorSystemDefault, payload, &profile, &error)) {
        fprintf(stderr, "FAIL: generic object profile decode failed kind=%d text=%s\n", (int)error.kind, error.text);
        exit(1);
    }

    if (profile.kind != _FXCFControlResponseProfileKindGenericObject || profile.resultObject == NULL) {
        fprintf(stderr, "FAIL: generic object profile mismatch\n");
        exit(1);
    }

    _FXCFControlResponseProfileClear(&profile);
    CFRelease((CFTypeRef)payload);
}

static void verify_corpus_loop(void) {
    for (size_t index = 0u; index < kFXSubset7ACaseCount; ++index) {
        const struct FXSubset7ACase *case_data = &kFXSubset7ACases[index];
        struct FXSubset7DResponseProfileResult result;

        if (case_data->kind != FXSubset7AResponse || case_data->validation_mode != FXSubset7AValidationAccept) {
            continue;
        }

        result = fx_subset7d_run_case(case_data);
        if (!result.success) {
            fprintf(stderr, "FAIL: %s primary=%s alternate=%s\n",
                case_data->name,
                result.primary_value_text != NULL ? result.primary_value_text : "null",
                result.alternate_value_text != NULL ? result.alternate_value_text : "null");
            fx_subset7d_free_result(&result);
            exit(1);
        }
        fx_subset7d_free_result(&result);
    }
}

int main(void) {
    verify_error_profile();
    verify_notify_post_profile();
    verify_registration_profile();
    verify_registration_pending_generation_profile();
    verify_health_profile();
    verify_diagnostics_profile();
    verify_generic_object_profile();
    verify_corpus_loop();
    puts("PASS control_response_profile_tests");
    return 0;
}
