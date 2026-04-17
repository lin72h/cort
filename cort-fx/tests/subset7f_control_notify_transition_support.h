#ifndef FX_SUBSET7F_CONTROL_NOTIFY_TRANSITION_SUPPORT_H
#define FX_SUBSET7F_CONTROL_NOTIFY_TRANSITION_SUPPORT_H

#include <CoreFoundation/CoreFoundation.h>

#include "../src/FXCFControlPacket.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum FXSubset7AValidationMode {
    FXSubset7AValidationAccept = 1,
    FXSubset7AValidationReject = 2
};

enum FXSubset7APacketKind {
    FXSubset7ARequest = 1,
    FXSubset7AResponse = 2
};

enum FXSubset7ATemplateKind {
    FXSubset7ATemplateNil = 0,
    FXSubset7ATemplateString = 1,
    FXSubset7ATemplateInteger = 2,
    FXSubset7ATemplateDouble = 3,
    FXSubset7ATemplateBool = 4,
    FXSubset7ATemplateDate = 5,
    FXSubset7ATemplateData = 6,
    FXSubset7ATemplateObject = 7,
    FXSubset7ATemplateArray = 8
};

enum FXSubset7ARejectKind {
    FXSubset7ARejectMissingKey = 1,
    FXSubset7ARejectUnsupportedVersion = 2,
    FXSubset7ARejectInvalidPacket = 3
};

struct FXSubset7AObjectEntry {
    const char *key;
    size_t node_index;
};

struct FXSubset7ATemplateNode {
    enum FXSubset7ATemplateKind kind;
    const char *text;
    long long integer_value;
    double double_value;
    Boolean bool_value;
    const struct FXSubset7AObjectEntry *entries;
    const size_t *items;
    size_t child_count;
    const UInt8 *bytes;
    size_t byte_count;
};

struct FXSubset7ACase {
    const char *name;
    enum FXSubset7AValidationMode validation_mode;
    enum FXSubset7APacketKind kind;
    const UInt8 *payload;
    size_t payload_length;
    const char *expected_primary_value_text;
    const char *expected_alternate_value_text;
    const struct FXSubset7ATemplateNode *accept_nodes;
    size_t accept_root_index;
    enum FXSubset7ARejectKind reject_kind;
    const char *reject_text;
    long long reject_version;
};

#include "generated/subset7a_control_packet_cases.h"

struct FXSubset7FTransitionCaseMeta {
    const char *name;
    const char *outcome_case_name;
    enum _FXCFControlNotifyTransitionOperation operation;
    Boolean hasCachedRegistration;
    const char *cachedSessionID;
    const char *cachedScope;
    const char *cachedName;
    const char *cachedBindingID;
    long long cachedToken;
    long long cachedLastSeenGeneration;
    Boolean cachedFirstCheckPending;
};

#include "generated/subset7f_notify_transition_cases.h"

struct FXSubset7FResult {
    Boolean success;
    char *primary_value_text;
    char *alternate_value_text;
};

static char *fx_subset7f_strdup(const char *text) {
    size_t length = text != NULL ? strlen(text) : 0u;
    char *copy = (char *)calloc(length + 1u, sizeof(char));

    if (copy == NULL) {
        return NULL;
    }
    if (length > 0u) {
        memcpy(copy, text, length);
    }
    return copy;
}

static const struct FXSubset7ACase *fx_subset7f_find_packet_case(const char *name) {
    for (size_t index = 0u; index < kFXSubset7ACaseCount; ++index) {
        if (strcmp(kFXSubset7ACases[index].name, name) == 0) {
            return &kFXSubset7ACases[index];
        }
    }
    return NULL;
}

static const struct FXSubset7FTransitionCaseMeta * __attribute__((unused)) fx_subset7f_find_transition_case(const char *name) {
    for (size_t index = 0u; index < kFXSubset7FTransitionCaseCount; ++index) {
        if (strcmp(kFXSubset7FTransitionCases[index].name, name) == 0) {
            return &kFXSubset7FTransitionCases[index];
        }
    }
    return NULL;
}

static Boolean fx_subset7f_init_cached_registration(
    const struct FXSubset7FTransitionCaseMeta *case_data,
    struct _FXCFControlNotifyCachedRegistration *registrationOut
) {
    struct _FXCFControlPacketError error = {0};
    CFStringRef sessionID = NULL;
    CFStringRef scope = NULL;
    CFStringRef name = NULL;
    CFStringRef bindingID = NULL;
    Boolean ok = false;

    memset(registrationOut, 0, sizeof(*registrationOut));
    if (case_data == NULL || !case_data->hasCachedRegistration) {
        return true;
    }

    sessionID = CFStringCreateWithCString(kCFAllocatorSystemDefault, case_data->cachedSessionID, kCFStringEncodingUTF8);
    scope = CFStringCreateWithCString(kCFAllocatorSystemDefault, case_data->cachedScope, kCFStringEncodingUTF8);
    name = CFStringCreateWithCString(kCFAllocatorSystemDefault, case_data->cachedName, kCFStringEncodingUTF8);
    if (case_data->cachedBindingID != NULL) {
        bindingID = CFStringCreateWithCString(kCFAllocatorSystemDefault, case_data->cachedBindingID, kCFStringEncodingUTF8);
    }

    if (sessionID == NULL || scope == NULL || name == NULL ||
        !_FXCFControlNotifyCachedRegistrationInitCopy(
            kCFAllocatorSystemDefault,
            &(struct _FXCFControlNotifyCachedRegistration) {
                .sessionID = sessionID,
                .scope = scope,
                .name = name,
                .bindingID = bindingID,
                .token = (CFIndex)case_data->cachedToken,
                .lastSeenGeneration = (CFIndex)case_data->cachedLastSeenGeneration,
                .firstCheckPending = case_data->cachedFirstCheckPending
            },
            registrationOut,
            &error
        )) {
        fprintf(stderr, "FAIL: cached registration init failed for %s: %s\n",
            case_data->name,
            error.text);
        ok = false;
    } else {
        ok = true;
    }

    if (sessionID != NULL) {
        CFRelease((CFTypeRef)sessionID);
    }
    if (scope != NULL) {
        CFRelease((CFTypeRef)scope);
    }
    if (name != NULL) {
        CFRelease((CFTypeRef)name);
    }
    if (bindingID != NULL) {
        CFRelease((CFTypeRef)bindingID);
    }

    return ok;
}

static struct FXSubset7FResult fx_subset7f_run_case(const struct FXSubset7FTransitionCaseMeta *case_data) {
    struct FXSubset7FResult result;
    const struct FXSubset7ACase *packet_case = NULL;
    CFDataRef payload = NULL;
    struct _FXCFControlPacketError error = {0};
    struct _FXCFControlNotifyOutcome outcome = {0};
    struct _FXCFControlNotifyCachedRegistration cached = {0};
    struct _FXCFControlNotifyTransition transition = {0};

    memset(&result, 0, sizeof(result));

    if (case_data == NULL) {
        result.primary_value_text = fx_subset7f_strdup("{\"detail\":\"missing transition case\",\"type\":\"invalid_packet\"}");
        result.alternate_value_text = fx_subset7f_strdup("internal notify transition fixture mismatch");
        return result;
    }

    packet_case = fx_subset7f_find_packet_case(case_data->outcome_case_name);
    if (packet_case == NULL) {
        result.primary_value_text = fx_subset7f_strdup("{\"detail\":\"missing packet case\",\"type\":\"invalid_packet\"}");
        result.alternate_value_text = fx_subset7f_strdup("internal notify transition packet mismatch");
        return result;
    }

    payload = CFDataCreate(kCFAllocatorSystemDefault, packet_case->payload, (CFIndex)packet_case->payload_length);
    if (payload == NULL) {
        result.primary_value_text = fx_subset7f_strdup("{\"detail\":\"payload allocation failed\",\"type\":\"invalid_packet\"}");
        result.alternate_value_text = fx_subset7f_strdup("internal payload allocation failure");
        return result;
    }

    if (!_FXCFControlNotifyOutcomeInit(kCFAllocatorSystemDefault, payload, &outcome, &error)) {
        result.primary_value_text = _FXCFControlPacketCopyErrorJSON(&error);
        result.alternate_value_text = _FXCFControlPacketCopyErrorSummary(_FXCFControlPacketKindResponse, &error);
        goto cleanup;
    }

    if (!fx_subset7f_init_cached_registration(case_data, &cached)) {
        result.primary_value_text = fx_subset7f_strdup("{\"detail\":\"cached registration init failed\",\"type\":\"invalid_packet\"}");
        result.alternate_value_text = fx_subset7f_strdup("internal cached registration init failure");
        goto cleanup;
    }

    if (_FXCFControlNotifyTransitionInit(
            kCFAllocatorSystemDefault,
            case_data->operation,
            &outcome,
            case_data->hasCachedRegistration ? &cached : NULL,
            case_data->hasCachedRegistration,
            &transition,
            &error
        )) {
        result.primary_value_text = _FXCFControlNotifyTransitionCopyCanonicalJSON(&transition);
        result.alternate_value_text = _FXCFControlNotifyTransitionCopySummary(&transition);
        result.success = result.primary_value_text != NULL && result.alternate_value_text != NULL;
    } else {
        result.primary_value_text = _FXCFControlPacketCopyErrorJSON(&error);
        result.alternate_value_text = _FXCFControlPacketCopyErrorSummary(_FXCFControlPacketKindResponse, &error);
    }

cleanup:
    _FXCFControlNotifyTransitionClear(&transition);
    _FXCFControlNotifyCachedRegistrationClear(&cached);
    _FXCFControlNotifyOutcomeClear(&outcome);
    if (payload != NULL) {
        CFRelease((CFTypeRef)payload);
    }
    return result;
}

static void fx_subset7f_free_result(struct FXSubset7FResult *result) {
    if (result == NULL) {
        return;
    }

    free(result->primary_value_text);
    free(result->alternate_value_text);
    result->primary_value_text = NULL;
    result->alternate_value_text = NULL;
    result->success = false;
}

#endif
