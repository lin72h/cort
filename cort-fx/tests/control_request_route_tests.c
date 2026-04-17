#include "subset7c_control_request_route_support.h"

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

static CFDataRef build_request_payload(CFStringRef service, CFStringRef method, CFDictionaryRef params) {
    CFStringRef keys[4];
    CFTypeRef values[4];
    CFNumberRef version = NULL;
    CFDictionaryRef packet = NULL;
    CFDataRef payload = NULL;
    CFErrorRef error = NULL;
    SInt32 versionValue = 1;

    version = CFNumberCreate(kCFAllocatorSystemDefault, kCFNumberSInt32Type, &versionValue);
    if (version == NULL) {
        return NULL;
    }

    keys[0] = CFStringCreateWithCString(kCFAllocatorSystemDefault, "protocol_version", kCFStringEncodingASCII);
    values[0] = (CFTypeRef)version;
    keys[1] = CFStringCreateWithCString(kCFAllocatorSystemDefault, "service", kCFStringEncodingASCII);
    values[1] = (CFTypeRef)service;
    keys[2] = CFStringCreateWithCString(kCFAllocatorSystemDefault, "method", kCFStringEncodingASCII);
    values[2] = (CFTypeRef)method;
    keys[3] = CFStringCreateWithCString(kCFAllocatorSystemDefault, "params", kCFStringEncodingASCII);
    values[3] = (CFTypeRef)params;

    if (keys[0] == NULL || keys[1] == NULL || keys[2] == NULL || keys[3] == NULL) {
        for (size_t index = 0u; index < 4u; ++index) {
            if (keys[index] != NULL) {
                CFRelease((CFTypeRef)keys[index]);
            }
        }
        CFRelease((CFTypeRef)version);
        return NULL;
    }

    packet = CFDictionaryCreate(kCFAllocatorSystemDefault, (const void **)keys, (const void **)values, 4, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    for (size_t index = 0u; index < 4u; ++index) {
        CFRelease((CFTypeRef)keys[index]);
    }
    CFRelease((CFTypeRef)version);
    if (packet == NULL) {
        return NULL;
    }

    payload = CFPropertyListCreateData(kCFAllocatorSystemDefault, packet, kCFPropertyListBinaryFormat_v1_0, 0, &error);
    CFRelease((CFTypeRef)packet);
    if (error != NULL) {
        CFRelease((CFTypeRef)error);
    }
    return payload;
}

static void verify_health_route(void) {
    const struct FXSubset7ACase *case_data = find_case("request.control.health");
    struct _FXCFControlRequestRoute route = {0};
    struct _FXCFControlPacketError error = {0};
    CFDataRef payload = NULL;
    char *summary = NULL;

    if (case_data == NULL) {
        fprintf(stderr, "FAIL: missing health request case\n");
        exit(1);
    }

    payload = CFDataCreate(kCFAllocatorSystemDefault, case_data->payload, (CFIndex)case_data->payload_length);
    if (payload == NULL) {
        fprintf(stderr, "FAIL: health request payload allocation failed\n");
        exit(1);
    }

    if (!_FXCFControlRequestRouteInit(kCFAllocatorSystemDefault, payload, &route, &error)) {
        fprintf(stderr, "FAIL: health route decode failed kind=%d text=%s\n", (int)error.kind, error.text);
        exit(1);
    }

    if (route.kind != _FXCFControlRequestRouteKindControlHealth ||
        route.name != NULL ||
        route.scope != NULL ||
        route.hasToken ||
        CFDictionaryGetCount(route.request.params) != 0) {
        fprintf(stderr, "FAIL: health route shape mismatch\n");
        exit(1);
    }

    summary = _FXCFControlRequestRouteCopySummary(&route);
    if (summary == NULL || strcmp(summary, "route control.health params=none") != 0) {
        fprintf(stderr, "FAIL: health route summary mismatch actual=%s\n", summary != NULL ? summary : "null");
        free(summary);
        exit(1);
    }

    free(summary);
    _FXCFControlRequestRouteClear(&route);
    CFRelease((CFTypeRef)payload);
}

static void verify_signal_route(void) {
    const struct FXSubset7ACase *case_data = find_case("request.notify.register_signal.targeted");
    struct _FXCFControlRequestRoute route = {0};
    struct _FXCFControlPacketError error = {0};
    CFDataRef payload = NULL;
    char *summary = NULL;

    if (case_data == NULL) {
        fprintf(stderr, "FAIL: missing signal request case\n");
        exit(1);
    }

    payload = CFDataCreate(kCFAllocatorSystemDefault, case_data->payload, (CFIndex)case_data->payload_length);
    if (payload == NULL) {
        fprintf(stderr, "FAIL: signal request payload allocation failed\n");
        exit(1);
    }

    if (!_FXCFControlRequestRouteInit(kCFAllocatorSystemDefault, payload, &route, &error)) {
        fprintf(stderr, "FAIL: signal route decode failed kind=%d text=%s\n", (int)error.kind, error.text);
        exit(1);
    }

    if (route.kind != _FXCFControlRequestRouteKindNotifyRegisterSignal ||
        !route.hasSignal || route.signal != 30 ||
        !route.hasTargetPID || route.targetPID != 4321 ||
        route.hasToken || route.hasState) {
        fprintf(stderr, "FAIL: signal route numeric fields mismatch\n");
        exit(1);
    }

    require_text(route.name, "com.launchx.fixture.signal", "route.name");
    require_text(route.scope, "local", "route.scope");

    summary = _FXCFControlRequestRouteCopySummary(&route);
    if (summary == NULL || strcmp(summary, "route notify.register_signal params=name,scope,signal,target_pid") != 0) {
        fprintf(stderr, "FAIL: signal route summary mismatch actual=%s\n", summary != NULL ? summary : "null");
        free(summary);
        exit(1);
    }

    free(summary);
    _FXCFControlRequestRouteClear(&route);
    CFRelease((CFTypeRef)payload);
}

static void verify_resume_route(void) {
    const struct FXSubset7ACase *case_data = find_case("request.notify.resume.expected_session");
    struct _FXCFControlRequestRoute route = {0};
    struct _FXCFControlPacketError error = {0};
    CFDataRef payload = NULL;

    if (case_data == NULL) {
        fprintf(stderr, "FAIL: missing resume request case\n");
        exit(1);
    }

    payload = CFDataCreate(kCFAllocatorSystemDefault, case_data->payload, (CFIndex)case_data->payload_length);
    if (payload == NULL) {
        fprintf(stderr, "FAIL: resume request payload allocation failed\n");
        exit(1);
    }

    if (!_FXCFControlRequestRouteInit(kCFAllocatorSystemDefault, payload, &route, &error)) {
        fprintf(stderr, "FAIL: resume route decode failed kind=%d text=%s\n", (int)error.kind, error.text);
        exit(1);
    }

    if (route.kind != _FXCFControlRequestRouteKindNotifyResume || !route.hasToken || route.token != 77) {
        fprintf(stderr, "FAIL: resume route token mismatch\n");
        exit(1);
    }

    require_text(route.expectedSessionID, "session-fd", "route.expectedSessionID");

    _FXCFControlRequestRouteClear(&route);
    CFRelease((CFTypeRef)payload);
}

static void verify_rejection_case(void) {
    const struct FXSubset7ACase *case_data = find_case("request.missing_protocol_version");
    struct _FXCFControlRequestRoute route = {0};
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

    if (_FXCFControlRequestRouteInit(kCFAllocatorSystemDefault, payload, &route, &error)) {
        fprintf(stderr, "FAIL: rejection request unexpectedly decoded successfully\n");
        _FXCFControlRequestRouteClear(&route);
        CFRelease((CFTypeRef)payload);
        exit(1);
    }

    if (error.kind != _FXCFControlPacketErrorMissingKey || strcmp(error.text, "protocol_version") != 0) {
        fprintf(stderr, "FAIL: route rejection mismatch kind=%d text=%s\n", (int)error.kind, error.text);
        CFRelease((CFTypeRef)payload);
        exit(1);
    }

    CFRelease((CFTypeRef)payload);
}

static void verify_unknown_route_rejected(void) {
    CFMutableDictionaryRef params = CFDictionaryCreateMutable(kCFAllocatorSystemDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    CFStringRef service = CFStringCreateWithCString(kCFAllocatorSystemDefault, "notify", kCFStringEncodingASCII);
    CFStringRef method = CFStringCreateWithCString(kCFAllocatorSystemDefault, "unknown_method", kCFStringEncodingASCII);
    CFDataRef payload = NULL;
    struct _FXCFControlRequestRoute route = {0};
    struct _FXCFControlPacketError error = {0};

    if (params == NULL || service == NULL || method == NULL) {
        fprintf(stderr, "FAIL: unknown route fixture allocation failed\n");
        exit(1);
    }

    payload = build_request_payload(service, method, params);
    CFRelease((CFTypeRef)params);
    CFRelease((CFTypeRef)service);
    CFRelease((CFTypeRef)method);
    if (payload == NULL) {
        fprintf(stderr, "FAIL: unknown route payload build failed\n");
        exit(1);
    }

    if (_FXCFControlRequestRouteInit(kCFAllocatorSystemDefault, payload, &route, &error)) {
        fprintf(stderr, "FAIL: unknown route unexpectedly decoded successfully\n");
        _FXCFControlRequestRouteClear(&route);
        CFRelease((CFTypeRef)payload);
        exit(1);
    }

    if (error.kind != _FXCFControlPacketErrorInvalidPacket || strcmp(error.text, "unsupported request route") != 0) {
        fprintf(stderr, "FAIL: unknown route rejection mismatch kind=%d text=%s\n", (int)error.kind, error.text);
        CFRelease((CFTypeRef)payload);
        exit(1);
    }

    CFRelease((CFTypeRef)payload);
}

static void verify_wrong_type_param_rejected(void) {
    CFStringRef tokenKey = NULL;
    CFStringRef sessionKey = NULL;
    CFTypeRef values[2];
    CFMutableDictionaryRef params = CFDictionaryCreateMutable(kCFAllocatorSystemDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    CFStringRef service = CFStringCreateWithCString(kCFAllocatorSystemDefault, "notify", kCFStringEncodingASCII);
    CFStringRef method = CFStringCreateWithCString(kCFAllocatorSystemDefault, "check", kCFStringEncodingASCII);
    CFStringRef tokenText = CFStringCreateWithCString(kCFAllocatorSystemDefault, "forty-two", kCFStringEncodingASCII);
    CFStringRef session = CFStringCreateWithCString(kCFAllocatorSystemDefault, "session-alpha", kCFStringEncodingASCII);
    CFDataRef payload = NULL;
    struct _FXCFControlRequestRoute route = {0};
    struct _FXCFControlPacketError error = {0};

    tokenKey = CFStringCreateWithCString(kCFAllocatorSystemDefault, "token", kCFStringEncodingASCII);
    sessionKey = CFStringCreateWithCString(kCFAllocatorSystemDefault, "expected_session_id", kCFStringEncodingASCII);

    if (params == NULL || service == NULL || method == NULL || tokenText == NULL || session == NULL || tokenKey == NULL || sessionKey == NULL) {
        fprintf(stderr, "FAIL: wrong-type fixture allocation failed\n");
        exit(1);
    }

    values[0] = (CFTypeRef)tokenText;
    values[1] = (CFTypeRef)session;
    CFDictionarySetValue(params, tokenKey, values[0]);
    CFDictionarySetValue(params, sessionKey, values[1]);

    payload = build_request_payload(service, method, params);
    CFRelease((CFTypeRef)params);
    CFRelease((CFTypeRef)service);
    CFRelease((CFTypeRef)method);
    CFRelease((CFTypeRef)tokenText);
    CFRelease((CFTypeRef)session);
    CFRelease((CFTypeRef)tokenKey);
    CFRelease((CFTypeRef)sessionKey);
    if (payload == NULL) {
        fprintf(stderr, "FAIL: wrong-type payload build failed\n");
        exit(1);
    }

    if (_FXCFControlRequestRouteInit(kCFAllocatorSystemDefault, payload, &route, &error)) {
        fprintf(stderr, "FAIL: wrong-type param unexpectedly decoded successfully\n");
        _FXCFControlRequestRouteClear(&route);
        CFRelease((CFTypeRef)payload);
        exit(1);
    }

    if (error.kind != _FXCFControlPacketErrorInvalidPacket || strcmp(error.text, "token") != 0) {
        fprintf(stderr, "FAIL: wrong-type token rejection mismatch kind=%d text=%s\n", (int)error.kind, error.text);
        CFRelease((CFTypeRef)payload);
        exit(1);
    }

    CFRelease((CFTypeRef)payload);
}

static void verify_request_corpus_loop(void) {
    for (size_t index = 0u; index < kFXSubset7ACaseCount; ++index) {
        const struct FXSubset7ACase *case_data = &kFXSubset7ACases[index];
        struct FXSubset7CRequestRouteResult result;

        if (case_data->kind != FXSubset7ARequest) {
            continue;
        }

        result = fx_subset7c_run_case(case_data);
        if (!result.success) {
            fprintf(stderr, "FAIL: %s primary=%s alternate=%s\n",
                case_data->name,
                result.primary_value_text != NULL ? result.primary_value_text : "null",
                result.alternate_value_text != NULL ? result.alternate_value_text : "null");
            fx_subset7c_free_result(&result);
            exit(1);
        }

        fx_subset7c_free_result(&result);
    }
}

int main(void) {
    verify_health_route();
    verify_signal_route();
    verify_resume_route();
    verify_rejection_case();
    verify_unknown_route_rejected();
    verify_wrong_type_param_rejected();
    verify_request_corpus_loop();
    puts("PASS control_request_route_tests");
    return 0;
}
