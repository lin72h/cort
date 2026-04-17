#include "subset7f_control_notify_transition_support.h"

#include <stdbool.h>
#include <stdio.h>

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

static void json_print_case(FILE *stream, const struct FXSubset7FTransitionCaseMeta *case_data, const struct FXSubset7FResult *result, bool trailing_comma) {
    fputs("    {\n", stream);
    fputs("      \"name\": ", stream);
    json_print_string(stream, case_data->name);
    fputs(",\n      \"classification\": \"required\"", stream);
    fputs(",\n      \"validation_mode\": \"accept\"", stream);
    fputs(",\n      \"kind\": \"notify_transition\"", stream);
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
    size_t remaining = kFXSubset7FTransitionCaseCount;

    fputs("{\n", stdout);
    fputs("  \"probe\": \"subset7f_notify_state_transition\",\n", stdout);
    fputs("  \"format\": \"CORT FX Subset 7F notify state transition probe\",\n", stdout);
    fputs("  \"results\": [\n", stdout);

    for (size_t index = 0u; index < kFXSubset7FTransitionCaseCount; ++index) {
        const struct FXSubset7FTransitionCaseMeta *case_data = &kFXSubset7FTransitionCases[index];
        struct FXSubset7FResult result = fx_subset7f_run_case(case_data);

        remaining -= 1u;
        json_print_case(stdout, case_data, &result, remaining > 0u);
        fx_subset7f_free_result(&result);
    }

    fputs("  ]\n", stdout);
    fputs("}\n", stdout);
    return 0;
}
