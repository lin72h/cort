#include "subset7e_control_notify_outcome_support.h"

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

static void verify_error_invalid_token(void) {
    struct _FXCFControlNotifyOutcome outcome = {0};
    struct _FXCFControlPacketError error = {0};
    CFDataRef payload = payload_for_case("response.notify.invalid_token.failure");
    char *summary = NULL;

    if (!_FXCFControlNotifyOutcomeInit(kCFAllocatorSystemDefault, payload, &outcome, &error)) {
        fprintf(stderr, "FAIL: notify error decode failed kind=%d text=%s\n", (int)error.kind, error.text);
        exit(1);
    }

    if (outcome.kind != _FXCFControlNotifyOutcomeKindError ||
        outcome.statusFamily != _FXCFControlNotifyStatusFamilyInvalidToken) {
        fprintf(stderr, "FAIL: invalid_token error classification mismatch\n");
        exit(1);
    }

    summary = _FXCFControlNotifyOutcomeCopySummary(&outcome);
    if (summary == NULL || strcmp(summary, "notify error invalid_token code=notify_failed") != 0) {
        fprintf(stderr, "FAIL: invalid_token error summary mismatch actual=%s\n", summary != NULL ? summary : "null");
        free(summary);
        exit(1);
    }

    free(summary);
    _FXCFControlNotifyOutcomeClear(&outcome);
    CFRelease((CFTypeRef)payload);
}

static void verify_error_invalid_reuse(void) {
    struct _FXCFControlNotifyOutcome outcome = {0};
    struct _FXCFControlPacketError error = {0};
    CFDataRef payload = payload_for_case("response.notify.invalid_reuse.failure");

    if (!_FXCFControlNotifyOutcomeInit(kCFAllocatorSystemDefault, payload, &outcome, &error)) {
        fprintf(stderr, "FAIL: invalid_reuse error decode failed kind=%d text=%s\n", (int)error.kind, error.text);
        exit(1);
    }

    if (outcome.kind != _FXCFControlNotifyOutcomeKindError ||
        outcome.statusFamily != _FXCFControlNotifyStatusFamilyInvalidRequest) {
        fprintf(stderr, "FAIL: invalid_reuse status family mismatch\n");
        exit(1);
    }

    _FXCFControlNotifyOutcomeClear(&outcome);
    CFRelease((CFTypeRef)payload);
}

static void verify_registration_outcome(void) {
    struct _FXCFControlNotifyOutcome outcome = {0};
    struct _FXCFControlPacketError error = {0};
    CFDataRef payload = payload_for_case("response.notify.registration.file_descriptor.success");
    char *summary = NULL;

    if (!_FXCFControlNotifyOutcomeInit(kCFAllocatorSystemDefault, payload, &outcome, &error)) {
        fprintf(stderr, "FAIL: registration outcome decode failed kind=%d text=%s\n", (int)error.kind, error.text);
        exit(1);
    }

    if (outcome.kind != _FXCFControlNotifyOutcomeKindRegistration ||
        outcome.statusFamily != _FXCFControlNotifyStatusFamilyOK ||
        !outcome.hasRegistration ||
        outcome.registration.token != 77 ||
        outcome.registration.lastSeenGeneration != 0 ||
        !outcome.registration.firstCheckPending) {
        fprintf(stderr, "FAIL: registration outcome fields mismatch\n");
        exit(1);
    }

    require_text(outcome.registration.sessionID, "session-fd", "registration.session_id");
    require_text(outcome.registration.scope, "local", "registration.scope");
    require_text(outcome.registration.name, "com.launchx.fixture.fd.new", "registration.name");
    require_text(outcome.registration.bindingID, "binding-alpha", "registration.binding_id");
    require_text(outcome.registration.bindingHandle, "/tmp/launchx-notify-fixture.sock", "registration.binding_handle");

    summary = _FXCFControlNotifyOutcomeCopySummary(&outcome);
    if (summary == NULL || strcmp(summary, "notify ok registration token=77 session=session-fd") != 0) {
        fprintf(stderr, "FAIL: registration summary mismatch actual=%s\n", summary != NULL ? summary : "null");
        free(summary);
        exit(1);
    }

    free(summary);
    _FXCFControlNotifyOutcomeClear(&outcome);
    CFRelease((CFTypeRef)payload);
}

static void verify_registration_update_outcome(void) {
    struct _FXCFControlNotifyOutcome outcome = {0};
    struct _FXCFControlPacketError error = {0};
    CFDataRef payload = payload_for_case("response.notify.resume.success");

    if (!_FXCFControlNotifyOutcomeInit(kCFAllocatorSystemDefault, payload, &outcome, &error)) {
        fprintf(stderr, "FAIL: registration update decode failed kind=%d text=%s\n", (int)error.kind, error.text);
        exit(1);
    }

    if (outcome.kind != _FXCFControlNotifyOutcomeKindRegistration ||
        !outcome.hasRegistration ||
        outcome.registration.token != 51) {
        fprintf(stderr, "FAIL: registration update fields mismatch\n");
        exit(1);
    }

    require_text(outcome.registration.sessionID, "session-signal", "registration_update.session_id");
    _FXCFControlNotifyOutcomeClear(&outcome);
    CFRelease((CFTypeRef)payload);
}

static void verify_check_outcome(void) {
    struct _FXCFControlNotifyOutcome outcome = {0};
    struct _FXCFControlPacketError error = {0};
    CFDataRef payload = payload_for_case("response.notify.check.changed.success");
    char *summary = NULL;

    if (!_FXCFControlNotifyOutcomeInit(kCFAllocatorSystemDefault, payload, &outcome, &error)) {
        fprintf(stderr, "FAIL: notify check decode failed kind=%d text=%s\n", (int)error.kind, error.text);
        exit(1);
    }

    if (outcome.kind != _FXCFControlNotifyOutcomeKindCheck ||
        !outcome.hasRegistration ||
        !outcome.hasChanged || !outcome.changed ||
        !outcome.hasGeneration || outcome.generation != 4 ||
        !outcome.hasCurrentState || outcome.currentState != 7 ||
        outcome.registration.token != 42) {
        fprintf(stderr, "FAIL: notify check fields mismatch\n");
        exit(1);
    }

    summary = _FXCFControlNotifyOutcomeCopySummary(&outcome);
    if (summary == NULL || strcmp(summary, "notify ok check token=42 changed=true generation=4") != 0) {
        fprintf(stderr, "FAIL: notify check summary mismatch actual=%s\n", summary != NULL ? summary : "null");
        free(summary);
        exit(1);
    }

    free(summary);
    _FXCFControlNotifyOutcomeClear(&outcome);
    CFRelease((CFTypeRef)payload);
}

static void verify_post_outcome(void) {
    struct _FXCFControlNotifyOutcome outcome = {0};
    struct _FXCFControlPacketError error = {0};
    CFDataRef payload = payload_for_case("response.notify.post.success");
    char *summary = NULL;

    if (!_FXCFControlNotifyOutcomeInit(kCFAllocatorSystemDefault, payload, &outcome, &error)) {
        fprintf(stderr, "FAIL: notify post decode failed kind=%d text=%s\n", (int)error.kind, error.text);
        exit(1);
    }

    if (outcome.kind != _FXCFControlNotifyOutcomeKindPost ||
        !outcome.hasDeliveredTokensCount || outcome.deliveredTokensCount != 1 ||
        !outcome.hasGeneration || outcome.generation != 5 ||
        !outcome.hasState || outcome.state != 7) {
        fprintf(stderr, "FAIL: notify post fields mismatch\n");
        exit(1);
    }

    require_text(outcome.name, "com.launchx.fixture.check", "post.name");
    require_text(outcome.scope, "local", "post.scope");

    summary = _FXCFControlNotifyOutcomeCopySummary(&outcome);
    if (summary == NULL || strcmp(summary, "notify ok post generation=5 delivered=1") != 0) {
        fprintf(stderr, "FAIL: notify post summary mismatch actual=%s\n", summary != NULL ? summary : "null");
        free(summary);
        exit(1);
    }

    free(summary);
    _FXCFControlNotifyOutcomeClear(&outcome);
    CFRelease((CFTypeRef)payload);
}

static void verify_cancel_outcome(void) {
    struct _FXCFControlNotifyOutcome outcome = {0};
    struct _FXCFControlPacketError error = {0};
    CFDataRef payload = payload_for_case("response.notify.cancel.success");

    if (!_FXCFControlNotifyOutcomeInit(kCFAllocatorSystemDefault, payload, &outcome, &error)) {
        fprintf(stderr, "FAIL: notify cancel decode failed kind=%d text=%s\n", (int)error.kind, error.text);
        exit(1);
    }

    if (outcome.kind != _FXCFControlNotifyOutcomeKindCancel ||
        !outcome.hasCanceled || !outcome.canceled) {
        fprintf(stderr, "FAIL: notify cancel fields mismatch\n");
        exit(1);
    }

    _FXCFControlNotifyOutcomeClear(&outcome);
    CFRelease((CFTypeRef)payload);
}

static void verify_name_state_outcome(void) {
    struct _FXCFControlNotifyOutcome outcome = {0};
    struct _FXCFControlPacketError error = {0};
    CFDataRef payload = payload_for_case("response.notify.get_state.success");
    char *summary = NULL;

    if (!_FXCFControlNotifyOutcomeInit(kCFAllocatorSystemDefault, payload, &outcome, &error)) {
        fprintf(stderr, "FAIL: notify name_state decode failed kind=%d text=%s\n", (int)error.kind, error.text);
        exit(1);
    }

    if (outcome.kind != _FXCFControlNotifyOutcomeKindNameState ||
        !outcome.hasGeneration || outcome.generation != 4 ||
        !outcome.hasState || outcome.state != -1) {
        fprintf(stderr, "FAIL: notify name_state fields mismatch\n");
        exit(1);
    }

    require_text(outcome.name, "com.launchx.fixture.check", "name_state.name");
    require_text(outcome.scope, "local", "name_state.scope");

    summary = _FXCFControlNotifyOutcomeCopySummary(&outcome);
    if (summary == NULL || strcmp(summary, "notify ok name_state state=-1 generation=4") != 0) {
        fprintf(stderr, "FAIL: notify name_state summary mismatch actual=%s\n", summary != NULL ? summary : "null");
        free(summary);
        exit(1);
    }

    free(summary);
    _FXCFControlNotifyOutcomeClear(&outcome);
    CFRelease((CFTypeRef)payload);
}

static void verify_validity_outcome(void) {
    struct _FXCFControlNotifyOutcome outcome = {0};
    struct _FXCFControlPacketError error = {0};
    CFDataRef payload = payload_for_case("response.notify.is_valid_token.false");
    char *summary = NULL;

    if (!_FXCFControlNotifyOutcomeInit(kCFAllocatorSystemDefault, payload, &outcome, &error)) {
        fprintf(stderr, "FAIL: notify validity decode failed kind=%d text=%s\n", (int)error.kind, error.text);
        exit(1);
    }

    if (outcome.kind != _FXCFControlNotifyOutcomeKindValidity ||
        !outcome.hasValid || outcome.valid) {
        fprintf(stderr, "FAIL: notify validity fields mismatch\n");
        exit(1);
    }

    summary = _FXCFControlNotifyOutcomeCopySummary(&outcome);
    if (summary == NULL || strcmp(summary, "notify ok validity valid=false") != 0) {
        fprintf(stderr, "FAIL: notify validity summary mismatch actual=%s\n", summary != NULL ? summary : "null");
        free(summary);
        exit(1);
    }

    free(summary);
    _FXCFControlNotifyOutcomeClear(&outcome);
    CFRelease((CFTypeRef)payload);
}

static void verify_list_names_rejected(void) {
    struct _FXCFControlNotifyOutcome outcome = {0};
    struct _FXCFControlPacketError error = {0};
    CFDataRef payload = payload_for_case("response.notify.list_names.scoped");

    if (_FXCFControlNotifyOutcomeInit(kCFAllocatorSystemDefault, payload, &outcome, &error)) {
        fprintf(stderr, "FAIL: list_names should be out of scope for 7E\n");
        _FXCFControlNotifyOutcomeClear(&outcome);
        exit(1);
    }

    if (error.kind != _FXCFControlPacketErrorInvalidPacket ||
        strcmp(error.text, "unsupported notify outcome profile") != 0) {
        fprintf(stderr, "FAIL: list_names rejection mismatch kind=%d text=%s\n", (int)error.kind, error.text);
        exit(1);
    }

    CFRelease((CFTypeRef)payload);
}

static void verify_corpus_loop(void) {
    for (size_t index = 0u; index < kFXSubset7ACaseCount; ++index) {
        const struct FXSubset7ACase *case_data = &kFXSubset7ACases[index];
        struct FXSubset7EResult result;

        if (!fx_subset7e_is_supported_case(case_data)) {
            continue;
        }

        result = fx_subset7e_run_case(case_data);
        if (!result.success) {
            fprintf(stderr, "FAIL: %s primary=%s alternate=%s\n",
                case_data->name,
                result.primary_value_text != NULL ? result.primary_value_text : "null",
                result.alternate_value_text != NULL ? result.alternate_value_text : "null");
            fx_subset7e_free_result(&result);
            exit(1);
        }
        fx_subset7e_free_result(&result);
    }
}

int main(void) {
    verify_error_invalid_token();
    verify_error_invalid_reuse();
    verify_registration_outcome();
    verify_registration_update_outcome();
    verify_check_outcome();
    verify_post_outcome();
    verify_cancel_outcome();
    verify_name_state_outcome();
    verify_validity_outcome();
    verify_list_names_rejected();
    verify_corpus_loop();
    puts("PASS control_notify_outcome_tests");
    return 0;
}
