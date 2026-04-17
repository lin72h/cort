#include "subset7b_control_envelope_support.h"

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

static void verify_request_case(void) {
    const struct FXSubset7ACase *case_data = find_case("request.notify.register_signal.targeted");
    struct _FXCFControlRequest request = {0};
    struct _FXCFControlPacketError error = {0};
    CFDataRef payload = NULL;
    char *summary = NULL;

    if (case_data == NULL) {
        fprintf(stderr, "FAIL: missing targeted request case\n");
        exit(1);
    }

    payload = CFDataCreate(kCFAllocatorSystemDefault, case_data->payload, (CFIndex)case_data->payload_length);
    if (payload == NULL) {
        fprintf(stderr, "FAIL: request payload allocation failed\n");
        exit(1);
    }

    if (!_FXCFControlPacketIsBinaryPlist(payload)) {
        fprintf(stderr, "FAIL: request payload did not advertise binary plist header\n");
        exit(1);
    }
    if (!_FXCFControlRequestInit(kCFAllocatorSystemDefault, payload, &request, &error)) {
        fprintf(stderr, "FAIL: request decode failed kind=%d text=%s\n", (int)error.kind, error.text);
        exit(1);
    }

    if (request.protocolVersion != 1 || CFDictionaryGetCount(request.params) != 4) {
        fprintf(stderr, "FAIL: request envelope counts were wrong\n");
        exit(1);
    }

    require_text(request.service, "notify", "request.service");
    require_text(request.method, "register_signal", "request.method");

    summary = _FXCFControlRequestCopyEnvelopeSummary(&request);
    if (summary == NULL || strcmp(summary, "request notify register_signal params=4") != 0) {
        fprintf(stderr, "FAIL: request summary mismatch actual=%s\n", summary != NULL ? summary : "null");
        free(summary);
        exit(1);
    }

    free(summary);
    _FXCFControlRequestClear(&request);
    CFRelease((CFTypeRef)payload);
}

static void verify_success_response_case(void) {
    const struct FXSubset7ACase *case_data = find_case("response.scalar_coverage.success");
    struct _FXCFControlResponse response = {0};
    struct _FXCFControlPacketError error = {0};
    CFDataRef payload = NULL;
    char *summary = NULL;

    if (case_data == NULL) {
        fprintf(stderr, "FAIL: missing scalar coverage response case\n");
        exit(1);
    }

    payload = CFDataCreate(kCFAllocatorSystemDefault, case_data->payload, (CFIndex)case_data->payload_length);
    if (payload == NULL) {
        fprintf(stderr, "FAIL: response payload allocation failed\n");
        exit(1);
    }

    if (!_FXCFControlResponseInit(kCFAllocatorSystemDefault, payload, &response, &error)) {
        fprintf(stderr, "FAIL: success response decode failed kind=%d text=%s\n", (int)error.kind, error.text);
        exit(1);
    }

    if (response.protocolVersion != 1 ||
        response.status != _FXCFControlResponseStatusOK ||
        response.result == NULL ||
        response.errorPayload != NULL ||
        response.errorCode != NULL ||
        response.errorMessage != NULL ||
        _FXCFControlPacketValueKind(response.result) != _FXCFControlValueKindObject) {
        fprintf(stderr, "FAIL: success response envelope shape mismatch\n");
        exit(1);
    }

    summary = _FXCFControlResponseCopyEnvelopeSummary(&response);
    if (summary == NULL || strcmp(summary, "response ok result=object count=9") != 0) {
        fprintf(stderr, "FAIL: success response summary mismatch actual=%s\n", summary != NULL ? summary : "null");
        free(summary);
        exit(1);
    }

    free(summary);
    _FXCFControlResponseClear(&response);
    CFRelease((CFTypeRef)payload);
}

static void verify_error_response_case(void) {
    const struct FXSubset7ACase *case_data = find_case("response.notify.invalid_token.failure");
    struct _FXCFControlResponse response = {0};
    struct _FXCFControlPacketError error = {0};
    CFDataRef payload = NULL;
    char *summary = NULL;

    if (case_data == NULL) {
        fprintf(stderr, "FAIL: missing error response case\n");
        exit(1);
    }

    payload = CFDataCreate(kCFAllocatorSystemDefault, case_data->payload, (CFIndex)case_data->payload_length);
    if (payload == NULL) {
        fprintf(stderr, "FAIL: error response payload allocation failed\n");
        exit(1);
    }

    if (!_FXCFControlResponseInit(kCFAllocatorSystemDefault, payload, &response, &error)) {
        fprintf(stderr, "FAIL: error response decode failed kind=%d text=%s\n", (int)error.kind, error.text);
        exit(1);
    }

    if (response.status != _FXCFControlResponseStatusError ||
        response.result != NULL ||
        response.errorPayload == NULL ||
        response.errorCode == NULL ||
        response.errorMessage == NULL) {
        fprintf(stderr, "FAIL: error response envelope shape mismatch\n");
        exit(1);
    }

    require_text(response.errorCode, "notify_failed", "response.error.code");
    require_text(response.errorMessage, "notify_failed(invalid_token(42))", "response.error.message");

    summary = _FXCFControlResponseCopyEnvelopeSummary(&response);
    if (summary == NULL || strcmp(summary, "response error code=notify_failed") != 0) {
        fprintf(stderr, "FAIL: error response summary mismatch actual=%s\n", summary != NULL ? summary : "null");
        free(summary);
        exit(1);
    }

    free(summary);
    _FXCFControlResponseClear(&response);
    CFRelease((CFTypeRef)payload);
}

static void verify_rejection_case(void) {
    const struct FXSubset7ACase *case_data = find_case("request.missing_protocol_version");
    struct _FXCFControlRequest request = {0};
    struct _FXCFControlPacketError error = {0};
    CFDataRef payload = NULL;

    if (case_data == NULL) {
        fprintf(stderr, "FAIL: missing rejection request case\n");
        exit(1);
    }

    payload = CFDataCreate(kCFAllocatorSystemDefault, case_data->payload, (CFIndex)case_data->payload_length);
    if (payload == NULL) {
        fprintf(stderr, "FAIL: rejection payload allocation failed\n");
        exit(1);
    }

    if (_FXCFControlRequestInit(kCFAllocatorSystemDefault, payload, &request, &error)) {
        fprintf(stderr, "FAIL: rejection request unexpectedly decoded successfully\n");
        _FXCFControlRequestClear(&request);
        CFRelease((CFTypeRef)payload);
        exit(1);
    }

    if (error.kind != _FXCFControlPacketErrorMissingKey || strcmp(error.text, "protocol_version") != 0) {
        fprintf(stderr, "FAIL: rejection error mismatch kind=%d text=%s\n", (int)error.kind, error.text);
        CFRelease((CFTypeRef)payload);
        exit(1);
    }

    CFRelease((CFTypeRef)payload);
}

static void verify_corpus_loop(void) {
    for (size_t index = 0u; index < kFXSubset7ACaseCount; ++index) {
        const struct FXSubset7ACase *case_data = &kFXSubset7ACases[index];
        struct FXSubset7BEnvelopeResult result = fx_subset7b_run_case(case_data);

        if (!result.success) {
            fprintf(stderr, "FAIL: %s primary=%s alternate=%s\n",
                case_data->name,
                result.primary_value_text != NULL ? result.primary_value_text : "null",
                result.alternate_value_text != NULL ? result.alternate_value_text : "null");
            fx_subset7b_free_result(&result);
            exit(1);
        }

        fx_subset7b_free_result(&result);
    }
}

int main(void) {
    verify_request_case();
    verify_success_response_case();
    verify_error_response_case();
    verify_rejection_case();
    verify_corpus_loop();
    puts("PASS control_envelope_tests");
    return 0;
}
