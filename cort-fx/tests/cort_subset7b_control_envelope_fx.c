#include "subset7b_control_envelope_support.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

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

static void json_print_text_or_null(FILE *stream, const char *value) {
    if (value == NULL) {
        fputs("null", stream);
        return;
    }
    json_print_string(stream, value);
}

static const char *validation_mode_text(enum FXSubset7AValidationMode mode) {
    return mode == FXSubset7AValidationAccept ? "accept" : "reject";
}

static const char *kind_text(enum FXSubset7APacketKind kind) {
    return kind == FXSubset7ARequest ? "request" : "response";
}

static void json_print_case(FILE *stream, const struct FXSubset7ACase *case_data, const struct FXSubset7BEnvelopeResult *result, bool trailing_comma) {
    fputs("    {\n", stream);
    fputs("      \"name\": ", stream);
    json_print_string(stream, case_data->name);
    fputs(",\n      \"classification\": \"required\"", stream);
    fputs(",\n      \"validation_mode\": ", stream);
    json_print_string(stream, validation_mode_text(case_data->validation_mode));
    fputs(",\n      \"kind\": ", stream);
    json_print_string(stream, kind_text(case_data->kind));
    fputs(",\n      \"success\": ", stream);
    fputs(result->success ? "true" : "false", stream);
    fputs(",\n      \"primary_value_text\": ", stream);
    json_print_text_or_null(stream, result->primary_value_text);
    fputs(",\n      \"alternate_value_text\": ", stream);
    json_print_text_or_null(stream, result->alternate_value_text);
    fputs("\n    }", stream);
    if (trailing_comma) {
        fputc(',', stream);
    }
    fputc('\n', stream);
}

int main(void) {
    fputs("{\n", stdout);
    fputs("  \"probe\": \"subset7b_control_envelope\",\n", stdout);
    fputs("  \"format\": \"CORT FX Subset 7B control envelope probe\",\n", stdout);
    fputs("  \"results\": [\n", stdout);

    for (size_t index = 0u; index < kFXSubset7ACaseCount; ++index) {
        const struct FXSubset7ACase *case_data = &kFXSubset7ACases[index];
        struct FXSubset7BEnvelopeResult result = fx_subset7b_run_case(case_data);

        json_print_case(stdout, case_data, &result, index + 1u < kFXSubset7ACaseCount);
        fx_subset7b_free_result(&result);
    }

    fputs("  ]\n", stdout);
    fputs("}\n", stdout);
    return 0;
}
