#include "subset7a_control_packet_support.h"

#include <stdio.h>
#include <stdlib.h>

static const char *validation_mode_text(enum FXSubset7AValidationMode mode) {
    return mode == FXSubset7AValidationAccept ? "accept" : "reject";
}

static const char *kind_text(enum FXSubset7APacketKind kind) {
    return kind == FXSubset7ARequest ? "request" : "response";
}

static void fail_case(const struct FXSubset7ACase *case_data, const struct FXSubset7AResult *result) {
    fprintf(stderr, "FAIL: %s (%s %s)\n", case_data->name, validation_mode_text(case_data->validation_mode), kind_text(case_data->kind));
    fprintf(stderr, "  primary: %s\n", result->primary_value_text != NULL ? result->primary_value_text : "null");
    fprintf(stderr, "  alternate: %s\n", result->alternate_value_text != NULL ? result->alternate_value_text : "null");
    exit(1);
}

int main(void) {
    for (size_t index = 0u; index < kFXSubset7ACaseCount; ++index) {
        const struct FXSubset7ACase *case_data = &kFXSubset7ACases[index];
        struct FXSubset7AResult result = fx_subset7a_run_case(case_data);

        if (!result.success) {
            fail_case(case_data, &result);
        }

        fx_subset7a_free_result(&result);
    }

    puts("PASS control_packet_tests");
    return 0;
}
