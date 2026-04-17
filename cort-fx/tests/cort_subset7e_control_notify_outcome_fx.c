#include "subset7e_control_notify_outcome_support.h"

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

static void json_print_case(FILE *stream, const struct FXSubset7ACase *case_data, const struct FXSubset7EResult *result, bool trailing_comma) {
    fputs("    {\n", stream);
    fputs("      \"name\": ", stream);
    json_print_string(stream, case_data->name);
    fputs(",\n      \"classification\": \"required\"", stream);
    fputs(",\n      \"validation_mode\": \"accept\"", stream);
    fputs(",\n      \"kind\": \"response\"", stream);
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
    size_t remaining = 0u;

    for (size_t index = 0u; index < kFXSubset7ACaseCount; ++index) {
        if (fx_subset7e_is_supported_case(&kFXSubset7ACases[index])) {
            remaining += 1u;
        }
    }

    fputs("{\n", stdout);
    fputs("  \"probe\": \"subset7e_notify_client_outcome\",\n", stdout);
    fputs("  \"format\": \"CORT FX Subset 7E notify client outcome probe\",\n", stdout);
    fputs("  \"results\": [\n", stdout);

    for (size_t index = 0u; index < kFXSubset7ACaseCount; ++index) {
        const struct FXSubset7ACase *case_data = &kFXSubset7ACases[index];
        struct FXSubset7EResult result;

        if (!fx_subset7e_is_supported_case(case_data)) {
            continue;
        }

        result = fx_subset7e_run_case(case_data);
        remaining -= 1u;
        json_print_case(stdout, case_data, &result, remaining > 0u);
        fx_subset7e_free_result(&result);
    }

    fputs("  ]\n", stdout);
    fputs("}\n", stdout);
    return 0;
}
