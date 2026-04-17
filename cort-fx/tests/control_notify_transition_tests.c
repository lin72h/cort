#include "subset7f_control_notify_transition_support.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void require_contains(const char *actual, const char *needle, const char *label) {
    if (actual == NULL || needle == NULL || strstr(actual, needle) == NULL) {
        fprintf(stderr, "FAIL: %s missing substring %s in %s\n",
            label,
            needle != NULL ? needle : "null",
            actual != NULL ? actual : "null");
        exit(1);
    }
}

static struct FXSubset7FResult run_named_case(const char *name) {
    const struct FXSubset7FTransitionCaseMeta *case_data = fx_subset7f_find_transition_case(name);

    if (case_data == NULL) {
        fprintf(stderr, "FAIL: missing 7F transition case %s\n", name);
        exit(1);
    }

    return fx_subset7f_run_case(case_data);
}

static void verify_registration_store_fd(void) {
    struct FXSubset7FResult result = run_named_case("notify.transition.registration.store.file_descriptor");

    if (!result.success) {
        fprintf(stderr, "FAIL: registration store fd case did not succeed\n");
        exit(1);
    }

    require_contains(result.primary_value_text, "\"action\":\"store\"", "registration_store.action");
    require_contains(result.primary_value_text, "\"operation\":\"registration_store\"", "registration_store.operation");
    require_contains(result.primary_value_text, "\"binding_id\":\"binding-alpha\"", "registration_store.binding_id");
    require_contains(result.primary_value_text, "\"token\":77", "registration_store.token");
    require_contains(result.alternate_value_text, "token=77", "registration_store.summary");

    fx_subset7f_free_result(&result);
}

static void verify_check_merge_binding_preserved(void) {
    struct FXSubset7FResult result = run_named_case("notify.transition.check.merge.binding_preserved");

    if (!result.success) {
        fprintf(stderr, "FAIL: check merge binding-preserved case did not succeed\n");
        exit(1);
    }

    require_contains(result.primary_value_text, "\"action\":\"merge\"", "check_merge.action");
    require_contains(result.primary_value_text, "\"binding_id\":\"binding-alpha\"", "check_merge.binding_id");
    require_contains(result.primary_value_text, "\"changed_result\":true", "check_merge.changed");
    require_contains(result.primary_value_text, "\"last_seen_generation\":4", "check_merge.last_seen_generation");
    require_contains(result.alternate_value_text, "changed=true", "check_merge.summary");

    fx_subset7f_free_result(&result);
}

static void verify_check_merge_stale_generation(void) {
    struct FXSubset7FResult result = run_named_case("notify.transition.check.merge.stale_generation");

    if (!result.success) {
        fprintf(stderr, "FAIL: stale generation check case did not succeed\n");
        exit(1);
    }

    require_contains(result.primary_value_text, "\"changed_result\":false", "stale_generation.changed");
    require_contains(result.primary_value_text, "\"last_seen_generation\":2", "stale_generation.last_seen_generation");
    require_contains(result.alternate_value_text, "changed=false", "stale_generation.summary");

    fx_subset7f_free_result(&result);
}

static void verify_invalid_token_discard_update(void) {
    struct FXSubset7FResult result = run_named_case("notify.transition.error.invalid_token.discard_update");

    if (!result.success) {
        fprintf(stderr, "FAIL: invalid token discard update case did not succeed\n");
        exit(1);
    }

    require_contains(result.primary_value_text, "\"action\":\"discard\"", "invalid_token_discard.action");
    require_contains(result.primary_value_text, "\"released_binding_id\":\"binding-alpha\"", "invalid_token_discard.released_binding_id");
    require_contains(result.primary_value_text, "\"status_family\":\"invalid_token\"", "invalid_token_discard.status");
    require_contains(result.alternate_value_text, "release=binding-alpha", "invalid_token_discard.summary");

    fx_subset7f_free_result(&result);
}

static void verify_invalid_request_keep_validity(void) {
    struct FXSubset7FResult result = run_named_case("notify.transition.error.invalid_request.keep_validity");

    if (!result.success) {
        fprintf(stderr, "FAIL: invalid request keep validity case did not succeed\n");
        exit(1);
    }

    require_contains(result.primary_value_text, "\"action\":\"keep\"", "invalid_request_keep.action");
    require_contains(result.primary_value_text, "\"valid_result\":true", "invalid_request_keep.valid");
    require_contains(result.primary_value_text, "\"status_family\":\"invalid_request\"", "invalid_request_keep.status");
    require_contains(result.alternate_value_text, "valid=true", "invalid_request_keep.summary");

    fx_subset7f_free_result(&result);
}

static void verify_validity_false_discard(void) {
    struct FXSubset7FResult result = run_named_case("notify.transition.validity.false.discard");

    if (!result.success) {
        fprintf(stderr, "FAIL: validity false discard case did not succeed\n");
        exit(1);
    }

    require_contains(result.primary_value_text, "\"action\":\"discard\"", "validity_false.action");
    require_contains(result.primary_value_text, "\"valid_result\":false", "validity_false.valid");
    require_contains(result.primary_value_text, "\"released_binding_id\":\"binding-alpha\"", "validity_false.released_binding_id");
    require_contains(result.alternate_value_text, "valid=false", "validity_false.summary");

    fx_subset7f_free_result(&result);
}

int main(void) {
    verify_registration_store_fd();
    verify_check_merge_binding_preserved();
    verify_check_merge_stale_generation();
    verify_invalid_token_discard_update();
    verify_invalid_request_keep_validity();
    verify_validity_false_discard();
    puts("PASS control_notify_transition_tests");
    return 0;
}
