#ifndef FX_SUBSET7B_CONTROL_ENVELOPE_SUPPORT_H
#define FX_SUBSET7B_CONTROL_ENVELOPE_SUPPORT_H

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

struct FXSubset7BEnvelopeResult {
    Boolean success;
    char *primary_value_text;
    char *alternate_value_text;
};

static char *fx_subset7b_strdup(const char *text) {
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

static enum FXSubset7ARejectKind fx_subset7b_internal_reject_kind(enum _FXCFControlPacketErrorKind kind) {
    switch (kind) {
        case _FXCFControlPacketErrorMissingKey:
            return FXSubset7ARejectMissingKey;
        case _FXCFControlPacketErrorUnsupportedVersion:
            return FXSubset7ARejectUnsupportedVersion;
        case _FXCFControlPacketErrorInvalidPacket:
            return FXSubset7ARejectInvalidPacket;
        default:
            return 0;
    }
}

static struct FXSubset7BEnvelopeResult fx_subset7b_run_case(const struct FXSubset7ACase *case_data) {
    struct FXSubset7BEnvelopeResult result;
    CFDataRef payload = NULL;
    struct _FXCFControlPacketError error = {0};

    memset(&result, 0, sizeof(result));

    payload = CFDataCreate(kCFAllocatorSystemDefault, case_data->payload, (CFIndex)case_data->payload_length);
    if (payload == NULL) {
        result.primary_value_text = fx_subset7b_strdup("{\"detail\":\"payload allocation failed\",\"type\":\"invalid_packet\"}");
        result.alternate_value_text = fx_subset7b_strdup("internal payload allocation failure");
        return result;
    }

    if (case_data->validation_mode == FXSubset7AValidationAccept) {
        if (case_data->kind == FXSubset7ARequest) {
            struct _FXCFControlRequest request = {0};

            if (_FXCFControlRequestInit(kCFAllocatorSystemDefault, payload, &request, &error)) {
                result.primary_value_text = _FXCFControlRequestCopyEnvelopeJSON(&request);
                result.alternate_value_text = _FXCFControlRequestCopyEnvelopeSummary(&request);
                result.success = result.primary_value_text != NULL && result.alternate_value_text != NULL;
            } else {
                result.primary_value_text = _FXCFControlPacketCopyErrorJSON(&error);
                result.alternate_value_text = _FXCFControlPacketCopyErrorSummary(_FXCFControlPacketKindRequest, &error);
            }

            _FXCFControlRequestClear(&request);
        } else {
            struct _FXCFControlResponse response = {0};

            if (_FXCFControlResponseInit(kCFAllocatorSystemDefault, payload, &response, &error)) {
                result.primary_value_text = _FXCFControlResponseCopyEnvelopeJSON(&response);
                result.alternate_value_text = _FXCFControlResponseCopyEnvelopeSummary(&response);
                result.success = result.primary_value_text != NULL && result.alternate_value_text != NULL;
            } else {
                result.primary_value_text = _FXCFControlPacketCopyErrorJSON(&error);
                result.alternate_value_text = _FXCFControlPacketCopyErrorSummary(_FXCFControlPacketKindResponse, &error);
            }

            _FXCFControlResponseClear(&response);
        }
    } else {
        Boolean matched = false;

        if (case_data->kind == FXSubset7ARequest) {
            struct _FXCFControlRequest request = {0};

            if (_FXCFControlRequestInit(kCFAllocatorSystemDefault, payload, &request, &error)) {
                result.primary_value_text = _FXCFControlRequestCopyEnvelopeJSON(&request);
                result.alternate_value_text = _FXCFControlRequestCopyEnvelopeSummary(&request);
                _FXCFControlRequestClear(&request);
            } else {
                result.primary_value_text = _FXCFControlPacketCopyErrorJSON(&error);
                result.alternate_value_text = _FXCFControlPacketCopyErrorSummary(_FXCFControlPacketKindRequest, &error);
            }
        } else {
            struct _FXCFControlResponse response = {0};

            if (_FXCFControlResponseInit(kCFAllocatorSystemDefault, payload, &response, &error)) {
                result.primary_value_text = _FXCFControlResponseCopyEnvelopeJSON(&response);
                result.alternate_value_text = _FXCFControlResponseCopyEnvelopeSummary(&response);
                _FXCFControlResponseClear(&response);
            } else {
                result.primary_value_text = _FXCFControlPacketCopyErrorJSON(&error);
                result.alternate_value_text = _FXCFControlPacketCopyErrorSummary(_FXCFControlPacketKindResponse, &error);
            }
        }

        matched = fx_subset7b_internal_reject_kind(error.kind) == case_data->reject_kind;
        if (matched) {
            switch (case_data->reject_kind) {
                case FXSubset7ARejectMissingKey:
                case FXSubset7ARejectInvalidPacket:
                    matched = strcmp(error.text, case_data->reject_text) == 0;
                    break;
                case FXSubset7ARejectUnsupportedVersion:
                    matched = error.version == (CFIndex)case_data->reject_version;
                    break;
                default:
                    matched = false;
                    break;
            }
        }
        result.success = matched;
    }

    CFRelease((CFTypeRef)payload);
    return result;
}

static void fx_subset7b_free_result(struct FXSubset7BEnvelopeResult *result) {
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
