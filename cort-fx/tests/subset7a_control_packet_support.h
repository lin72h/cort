#ifndef FX_SUBSET7A_CONTROL_PACKET_SUPPORT_H
#define FX_SUBSET7A_CONTROL_PACKET_SUPPORT_H

#include <CoreFoundation/CoreFoundation.h>

#include "../src/FXCFControlPacket.h"

#include <math.h>
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

struct FXSubset7AResult {
    Boolean success;
    char *primary_value_text;
    char *alternate_value_text;
};

static char *fx_subset7a_strdup(const char *text) {
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

static enum _FXCFControlPacketKind fx_subset7a_internal_kind(enum FXSubset7APacketKind kind) {
    return kind == FXSubset7ARequest ? _FXCFControlPacketKindRequest : _FXCFControlPacketKindResponse;
}

static enum FXSubset7ARejectKind fx_subset7a_internal_reject_kind(enum _FXCFControlPacketErrorKind kind) {
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

static Boolean fx_subset7a_compare_date(CFDateRef actual, const char *expectedText) {
    char *json = _FXCFControlPacketCopyCanonicalJSON((CFTypeRef)actual);
    Boolean ok = false;
    char expected[128];

    if (json == NULL || expectedText == NULL) {
        free(json);
        return false;
    }

    snprintf(expected, sizeof(expected), "{\"$date\":\"%s\"}", expectedText);
    ok = strcmp(json, expected) == 0;
    free(json);
    return ok;
}

static Boolean fx_subset7a_match_node(
    const struct FXSubset7ATemplateNode *nodes,
    size_t node_index,
    CFTypeRef actual,
    char *mismatch,
    size_t mismatch_size
);

static Boolean fx_subset7a_match_object(
    const struct FXSubset7ATemplateNode *nodes,
    const struct FXSubset7ATemplateNode *node,
    CFDictionaryRef actual,
    char *mismatch,
    size_t mismatch_size
) {
    size_t required_count = 0u;

    for (size_t index = 0u; index < node->child_count; ++index) {
        const struct FXSubset7ATemplateNode *child = &nodes[node->entries[index].node_index];
        if (child->kind != FXSubset7ATemplateNil) {
            required_count += 1u;
        }
    }

    if ((size_t)CFDictionaryGetCount(actual) != required_count) {
        snprintf(
            mismatch,
            mismatch_size,
            "dictionary count mismatch actual=%ld expected=%lu",
            (long)CFDictionaryGetCount(actual),
            (unsigned long)required_count
        );
        return false;
    }

    for (size_t index = 0u; index < node->child_count; ++index) {
        const struct FXSubset7AObjectEntry *entry = &node->entries[index];
        const struct FXSubset7ATemplateNode *child = &nodes[entry->node_index];
        CFStringRef key = CFStringCreateWithCString(kCFAllocatorSystemDefault, entry->key, kCFStringEncodingUTF8);
        const void *actualValue = NULL;
        Boolean present = false;

        if (key == NULL) {
            snprintf(mismatch, mismatch_size, "failed to create lookup key %s", entry->key);
            return false;
        }

        present = CFDictionaryGetValueIfPresent(actual, key, &actualValue);
        CFRelease((CFTypeRef)key);

        if (child->kind == FXSubset7ATemplateNil) {
            if (present) {
                snprintf(mismatch, mismatch_size, "unexpected key present %s", entry->key);
                return false;
            }
            continue;
        }

        if (!present || actualValue == NULL) {
            snprintf(mismatch, mismatch_size, "missing expected key %s", entry->key);
            return false;
        }

        if (!fx_subset7a_match_node(nodes, entry->node_index, (CFTypeRef)actualValue, mismatch, mismatch_size)) {
            return false;
        }
    }

    return true;
}

static Boolean fx_subset7a_match_array(
    const struct FXSubset7ATemplateNode *nodes,
    const struct FXSubset7ATemplateNode *node,
    CFArrayRef actual,
    char *mismatch,
    size_t mismatch_size
) {
    if ((size_t)CFArrayGetCount(actual) != node->child_count) {
        snprintf(
            mismatch,
            mismatch_size,
            "array count mismatch actual=%ld expected=%lu",
            (long)CFArrayGetCount(actual),
            (unsigned long)node->child_count
        );
        return false;
    }

    for (size_t index = 0u; index < node->child_count; ++index) {
        if (!fx_subset7a_match_node(
                nodes,
                node->items[index],
                (CFTypeRef)CFArrayGetValueAtIndex(actual, (CFIndex)index),
                mismatch,
                mismatch_size
            )) {
            return false;
        }
    }

    return true;
}

static Boolean fx_subset7a_match_node(
    const struct FXSubset7ATemplateNode *nodes,
    size_t node_index,
    CFTypeRef actual,
    char *mismatch,
    size_t mismatch_size
) {
    const struct FXSubset7ATemplateNode *node = &nodes[node_index];

    if (actual == NULL) {
        snprintf(mismatch, mismatch_size, "unexpected null actual value");
        return false;
    }

    switch (node->kind) {
        case FXSubset7ATemplateNil:
            return false;
        case FXSubset7ATemplateString: {
            char *text = NULL;
            Boolean ok = false;
            if (CFGetTypeID(actual) != CFStringGetTypeID()) {
                snprintf(mismatch, mismatch_size, "expected string");
                return false;
            }
            text = _FXCFControlPacketCopyCanonicalJSON(actual);
            if (text == NULL) {
                snprintf(mismatch, mismatch_size, "failed to stringify string");
                return false;
            }
            char *expected = fx_subset7a_strdup(node->text);
            if (expected == NULL) {
                free(text);
                snprintf(mismatch, mismatch_size, "failed to allocate expected string");
                return false;
            }
            char *quoted = (char *)calloc(strlen(expected) + 3u, sizeof(char));
            if (quoted == NULL) {
                free(expected);
                free(text);
                snprintf(mismatch, mismatch_size, "failed to allocate quoted string");
                return false;
            }
            snprintf(quoted, strlen(expected) + 3u, "\"%s\"", expected);
            ok = strcmp(text, quoted) == 0;
            if (!ok) {
                snprintf(mismatch, mismatch_size, "string mismatch actual=%s expected=%s", text, quoted);
            }
            free(quoted);
            free(expected);
            free(text);
            return ok;
        }
        case FXSubset7ATemplateInteger: {
            SInt64 value = 0;
            if (CFGetTypeID(actual) != CFNumberGetTypeID() || CFNumberGetType((CFNumberRef)actual) == kCFNumberFloat64Type) {
                snprintf(mismatch, mismatch_size, "expected integer");
                return false;
            }
            if (!CFNumberGetValue((CFNumberRef)actual, kCFNumberSInt64Type, &value) || value != (SInt64)node->integer_value) {
                snprintf(mismatch, mismatch_size, "integer mismatch actual=%lld expected=%lld", (long long)value, node->integer_value);
                return false;
            }
            return true;
        }
        case FXSubset7ATemplateDouble: {
            double value = 0.0;
            if (CFGetTypeID(actual) != CFNumberGetTypeID() || CFNumberGetType((CFNumberRef)actual) != kCFNumberFloat64Type) {
                snprintf(mismatch, mismatch_size, "expected double");
                return false;
            }
            if (!CFNumberGetValue((CFNumberRef)actual, kCFNumberFloat64Type, &value) || fabs(value - node->double_value) > 1e-12) {
                snprintf(mismatch, mismatch_size, "double mismatch actual=%.17g expected=%.17g", value, node->double_value);
                return false;
            }
            return true;
        }
        case FXSubset7ATemplateBool:
            if (CFGetTypeID(actual) != CFBooleanGetTypeID()) {
                snprintf(mismatch, mismatch_size, "expected boolean");
                return false;
            }
            if (CFBooleanGetValue((CFBooleanRef)actual) != node->bool_value) {
                snprintf(mismatch, mismatch_size, "boolean mismatch");
                return false;
            }
            return true;
        case FXSubset7ATemplateDate:
            if (CFGetTypeID(actual) != CFDateGetTypeID()) {
                snprintf(mismatch, mismatch_size, "expected date");
                return false;
            }
            if (!fx_subset7a_compare_date((CFDateRef)actual, node->text)) {
                snprintf(mismatch, mismatch_size, "date mismatch expected=%s", node->text);
                return false;
            }
            return true;
        case FXSubset7ATemplateData:
            if (CFGetTypeID(actual) != CFDataGetTypeID()) {
                snprintf(mismatch, mismatch_size, "expected data");
                return false;
            }
            if ((size_t)CFDataGetLength((CFDataRef)actual) != node->byte_count ||
                memcmp(CFDataGetBytePtr((CFDataRef)actual), node->bytes, node->byte_count) != 0) {
                snprintf(mismatch, mismatch_size, "data mismatch");
                return false;
            }
            return true;
        case FXSubset7ATemplateObject:
            if (CFGetTypeID(actual) != CFDictionaryGetTypeID()) {
                snprintf(mismatch, mismatch_size, "expected dictionary");
                return false;
            }
            return fx_subset7a_match_object(nodes, node, (CFDictionaryRef)actual, mismatch, mismatch_size);
        case FXSubset7ATemplateArray:
            if (CFGetTypeID(actual) != CFArrayGetTypeID()) {
                snprintf(mismatch, mismatch_size, "expected array");
                return false;
            }
            return fx_subset7a_match_array(nodes, node, (CFArrayRef)actual, mismatch, mismatch_size);
        default:
            snprintf(mismatch, mismatch_size, "unsupported template kind %d", (int)node->kind);
            return false;
    }
}

static struct FXSubset7AResult fx_subset7a_run_case(const struct FXSubset7ACase *case_data) {
    struct FXSubset7AResult result;
    CFDataRef payload = NULL;
    CFDictionaryRef packet = NULL;
    struct _FXCFControlPacketError error = {0};
    char mismatch[256];

    memset(&result, 0, sizeof(result));
    memset(mismatch, 0, sizeof(mismatch));

    payload = CFDataCreate(kCFAllocatorSystemDefault, case_data->payload, (CFIndex)case_data->payload_length);
    if (payload == NULL) {
        result.primary_value_text = fx_subset7a_strdup("{\"type\":\"invalid_packet\",\"detail\":\"payload allocation failed\"}");
        result.alternate_value_text = fx_subset7a_strdup("internal payload allocation failure");
        return result;
    }

    if (case_data->validation_mode == FXSubset7AValidationAccept) {
        if (_FXCFControlPacketDecodeEnvelope(
                kCFAllocatorSystemDefault,
                payload,
                fx_subset7a_internal_kind(case_data->kind),
                &packet,
                &error
            )) {
            if (fx_subset7a_match_node(case_data->accept_nodes, case_data->accept_root_index, (CFTypeRef)packet, mismatch, sizeof(mismatch))) {
                result.success = true;
                result.primary_value_text = fx_subset7a_strdup(case_data->expected_primary_value_text);
                result.alternate_value_text = fx_subset7a_strdup(case_data->expected_alternate_value_text);
            } else {
                result.primary_value_text = _FXCFControlPacketCopyCanonicalJSON((CFTypeRef)packet);
                result.alternate_value_text = fx_subset7a_strdup(mismatch);
            }
        } else {
            result.primary_value_text = _FXCFControlPacketCopyErrorJSON(&error);
            result.alternate_value_text = _FXCFControlPacketCopyErrorSummary(fx_subset7a_internal_kind(case_data->kind), &error);
        }
    } else {
        if (_FXCFControlPacketDecodeEnvelope(
                kCFAllocatorSystemDefault,
                payload,
                fx_subset7a_internal_kind(case_data->kind),
                &packet,
                &error
            )) {
            result.primary_value_text = _FXCFControlPacketCopyCanonicalJSON((CFTypeRef)packet);
            result.alternate_value_text = _FXCFControlPacketCopyAcceptedSummary(packet, fx_subset7a_internal_kind(case_data->kind));
        } else {
            Boolean matched = fx_subset7a_internal_reject_kind(error.kind) == case_data->reject_kind;
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

            if (matched) {
                result.success = true;
                result.primary_value_text = fx_subset7a_strdup(case_data->expected_primary_value_text);
                result.alternate_value_text = fx_subset7a_strdup(case_data->expected_alternate_value_text);
            } else {
                result.primary_value_text = _FXCFControlPacketCopyErrorJSON(&error);
                result.alternate_value_text = _FXCFControlPacketCopyErrorSummary(fx_subset7a_internal_kind(case_data->kind), &error);
            }
        }
    }

    if (packet != NULL) {
        CFRelease((CFTypeRef)packet);
    }
    CFRelease((CFTypeRef)payload);
    return result;
}

static void fx_subset7a_free_result(struct FXSubset7AResult *result) {
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
