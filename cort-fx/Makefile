CC ?= cc
AR ?= ar
RANLIB ?= ranlib
INSTALL ?= install

STRICT_CFLAGS := -std=c11 -Wall -Wextra -Werror -pedantic -pthread -fvisibility=hidden
CPPFLAGS += -Iinclude
LDFLAGS +=
PREFIX ?= /usr/local
DESTDIR ?=
CORT_ARTIFACTS_ROOT ?= ../../wip-cort-gpt-artifacts
REPO_ROOT := ..
ARTIFACTS_ROOT := $(CORT_ARTIFACTS_ROOT)

BUILD_DIR ?= $(ARTIFACTS_ROOT)/cort-fx/build
LIB_DIR := $(BUILD_DIR)/lib
OBJ_DIR := $(BUILD_DIR)/obj
BIN_DIR := $(BUILD_DIR)/bin
STAGE_DIR ?= $(BUILD_DIR)/stage
EXPORTS_DIR := exports
RUN_DIR ?= $(ARTIFACTS_ROOT)/cort-fx/runs/subset0-fx-local
ELIXIR_RUN := $(REPO_ROOT)/tools/run_elixir.sh
COMPARE_TOOL := $(REPO_ROOT)/tools/compare_subset0_json.exs
SCALAR_COMPARE_TOOL := $(REPO_ROOT)/tools/compare_subset1_scalar_core_json.exs
STRING_COMPARE_TOOL := $(REPO_ROOT)/tools/compare_subset1b_cfstring_json.exs
CONTAINER_COMPARE_TOOL := $(REPO_ROOT)/tools/compare_subset2a_container_json.exs
BPLIST_COMPARE_TOOL := $(REPO_ROOT)/tools/compare_subset3a_bplist_json.exs
PACKET_COMPARE_TOOL := $(REPO_ROOT)/tools/compare_subset7a_control_packet_json.exs
ENVELOPE_COMPARE_TOOL := $(REPO_ROOT)/tools/compare_subset7b_control_envelope_json.exs
RUN_ARTIFACT_SCRIPT := scripts/run_subset0_fx_artifacts.sh
SUBSET1A_COMPARE_ARTIFACT_SCRIPT := scripts/run_subset1a_compare_artifact.sh
SUBSET2A_COMPARE_ARTIFACT_SCRIPT := scripts/run_subset2a_compare_artifact.sh
SUBSET3A_COMPARE_ARTIFACT_SCRIPT := scripts/run_subset3a_compare_artifact.sh
PUBLISH_SHARED_JSON_SCRIPT := scripts/publish_shared_json.sh
SUBSET7A_CASES_HEADER_BUILDER := $(REPO_ROOT)/tools/build_subset7a_control_packet_cases_header.exs
SUBSET7A_CASES_HEADER := tests/generated/subset7a_control_packet_cases.h

LIB := $(LIB_DIR)/libcortfx.a
LIB_OBJECTS := \
	$(OBJ_DIR)/src/CFBase.o \
	$(OBJ_DIR)/src/CFArray.o \
	$(OBJ_DIR)/src/CFBinaryPList.o \
	$(OBJ_DIR)/src/CFData.o \
	$(OBJ_DIR)/src/CFDate.o \
	$(OBJ_DIR)/src/CFDictionary.o \
	$(OBJ_DIR)/src/CFError.o \
	$(OBJ_DIR)/src/FXCFControlPacket.o \
	$(OBJ_DIR)/src/CFNumber.o \
	$(OBJ_DIR)/src/CFPropertyList.o \
	$(OBJ_DIR)/src/CFString.o \
	$(OBJ_DIR)/src/CFRuntime.o
TEST_BINARY := $(BIN_DIR)/runtime_ownership_tests
ABORT_TEST_BINARY := $(BIN_DIR)/runtime_abort_tests
SCALAR_TEST_BINARY := $(BIN_DIR)/scalar_core_tests
STRING_TEST_BINARY := $(BIN_DIR)/string_core_tests
CONTAINER_TEST_BINARY := $(BIN_DIR)/container_core_tests
BPLIST_TEST_BINARY := $(BIN_DIR)/bplist_core_tests
PACKET_TEST_BINARY := $(BIN_DIR)/control_packet_tests
ENVELOPE_TEST_BINARY := $(BIN_DIR)/control_envelope_tests
SMOKE_BINARY := $(BIN_DIR)/c_consumer_smoke
COMPARE_BINARY := $(BIN_DIR)/subset0_public_compare_fx
SCALAR_COMPARE_BINARY := $(BIN_DIR)/subset1_scalar_core_fx
STRING_COMPARE_BINARY := $(BIN_DIR)/subset1b_cfstring_fx
CONTAINER_COMPARE_BINARY := $(BIN_DIR)/subset2a_container_fx
BPLIST_COMPARE_BINARY := $(BIN_DIR)/subset3a_bplist_fx
PACKET_COMPARE_BINARY := $(BIN_DIR)/subset7a_control_packet_fx
ENVELOPE_COMPARE_BINARY := $(BIN_DIR)/subset7b_control_envelope_fx
COMPARE_OUTPUT := $(BUILD_DIR)/out/subset0_public_compare_fx.json
SCALAR_COMPARE_OUTPUT := $(BUILD_DIR)/out/subset1_scalar_core_fx.json
STRING_COMPARE_OUTPUT := $(BUILD_DIR)/out/subset1b_cfstring_fx.json
CONTAINER_COMPARE_OUTPUT := $(BUILD_DIR)/out/subset2a_container_fx.json
BPLIST_COMPARE_OUTPUT := $(BUILD_DIR)/out/subset3a_bplist_fx.json
SHARED_SUBSET0_JSON := $(REPO_ROOT)/subset0_public_compare_fx.json
SHARED_SUBSET1A_JSON := $(REPO_ROOT)/subset1_scalar_core_fx.json
SHARED_SUBSET1B_JSON := $(REPO_ROOT)/subset1b_cfstring_fx.json
SHARED_SUBSET2A_JSON := $(REPO_ROOT)/subset2a_container_fx.json
SHARED_SUBSET3A_JSON := $(REPO_ROOT)/subset3a_bplist_fx.json
PACKET_COMPARE_OUTPUT := $(BUILD_DIR)/out/subset7a_control_packet_fx.json
ENVELOPE_COMPARE_OUTPUT := $(BUILD_DIR)/out/subset7b_control_envelope_fx.json
SHARED_SUBSET7A_JSON := $(REPO_ROOT)/subset7a_control_packet_fx.json
SHARED_SUBSET7B_JSON := $(REPO_ROOT)/subset7b_control_envelope_fx.json
FX_JSON ?= $(COMPARE_OUTPUT)
FX_SCALAR_JSON ?= $(SCALAR_COMPARE_OUTPUT)
FX_STRING_JSON ?= $(STRING_COMPARE_OUTPUT)
FX_CONTAINER_JSON ?= $(CONTAINER_COMPARE_OUTPUT)
FX_BPLIST_JSON ?= $(BPLIST_COMPARE_OUTPUT)
FX_PACKET_JSON ?= $(PACKET_COMPARE_OUTPUT)
FX_ENVELOPE_JSON ?= $(ENVELOPE_COMPARE_OUTPUT)
COMPARE_REPORT ?= $(BUILD_DIR)/out/subset0_fx_vs_mx_report.md
SCALAR_COMPARE_REPORT ?= $(BUILD_DIR)/out/subset1_scalar_core_fx_vs_mx_report.md
STRING_COMPARE_REPORT ?= $(BUILD_DIR)/out/subset1b_cfstring_fx_vs_mx_report.md
CONTAINER_COMPARE_REPORT ?= $(BUILD_DIR)/out/subset2a_container_fx_vs_mx_report.md
BPLIST_COMPARE_REPORT ?= $(BUILD_DIR)/out/subset3a_bplist_fx_vs_mx_report.md
PACKET_COMPARE_REPORT ?= $(BUILD_DIR)/out/subset7a_control_packet_fx_vs_expected_report.md
ENVELOPE_COMPARE_REPORT ?= $(BUILD_DIR)/out/subset7b_control_envelope_fx_vs_expected_report.md
EXPECTED_JSON ?= $(REPO_ROOT)/fixtures/control/subset7a_control_packet_expected_v1.json
ENVELOPE_EXPECTED_JSON ?= $(REPO_ROOT)/fixtures/control/subset7b_control_envelope_expected_v1.json
EXPORTS_EXPECTED := $(EXPORTS_DIR)/subset3a-exported-symbols.txt
EXPORTS_ACTUAL := $(BUILD_DIR)/subset3a-exported-symbols.txt
SCALAR_COMPARE_RUN_DIR ?= $(ARTIFACTS_ROOT)/cort-fx/runs/subset1a-fx-vs-mx
CONTAINER_COMPARE_RUN_DIR ?= $(ARTIFACTS_ROOT)/cort-fx/runs/subset2a-fx-vs-mx
BPLIST_COMPARE_RUN_DIR ?= $(ARTIFACTS_ROOT)/cort-fx/runs/subset3a-fx-vs-mx
INSTALL_INCLUDE_DIR := $(DESTDIR)$(PREFIX)/include/CoreFoundation
INSTALL_LIB_DIR := $(DESTDIR)$(PREFIX)/lib
INSTALL_DOC_DIR := $(DESTDIR)$(PREFIX)/share/doc/cort-fx

.PHONY: all clean check-deps check-exports compare-fx compare-subset1a-fx compare-subset1b-fx compare-subset2a-fx compare-subset3a-fx compare-subset7a-fx compare-subset7b-fx compare-with-mx compare-subset1a-with-mx compare-subset1b-with-mx compare-subset2a-with-mx compare-subset3a-with-mx compare-subset7a-with-expected compare-subset7b-with-expected publish-subset0-artifact publish-subset1a-artifact publish-subset1b-artifact publish-subset2a-artifact publish-subset3a-artifact publish-subset7a-artifact publish-subset7b-artifact publish-shared-artifacts artifact-run artifact-subset1a-compare artifact-subset2a-compare artifact-subset3a-compare install install-stage test test-installed

all: $(LIB) $(TEST_BINARY) $(ABORT_TEST_BINARY) $(SCALAR_TEST_BINARY) $(STRING_TEST_BINARY) $(CONTAINER_TEST_BINARY) $(BPLIST_TEST_BINARY) $(PACKET_TEST_BINARY) $(ENVELOPE_TEST_BINARY) $(SMOKE_BINARY) $(COMPARE_BINARY) $(SCALAR_COMPARE_BINARY) $(STRING_COMPARE_BINARY) $(CONTAINER_COMPARE_BINARY) $(BPLIST_COMPARE_BINARY) $(PACKET_COMPARE_BINARY) $(ENVELOPE_COMPARE_BINARY)

check-deps: $(TEST_BINARY) $(ABORT_TEST_BINARY) $(SCALAR_TEST_BINARY) $(STRING_TEST_BINARY) $(CONTAINER_TEST_BINARY) $(BPLIST_TEST_BINARY) $(PACKET_TEST_BINARY) $(ENVELOPE_TEST_BINARY) $(SMOKE_BINARY) $(COMPARE_BINARY) $(SCALAR_COMPARE_BINARY) $(STRING_COMPARE_BINARY) $(CONTAINER_COMPARE_BINARY) $(BPLIST_COMPARE_BINARY) $(PACKET_COMPARE_BINARY) $(ENVELOPE_COMPARE_BINARY) $(LIB)
	@set -eu; \
	host_uname_s=$$(uname -s 2>/dev/null || printf Unknown); \
	for file in $(TEST_BINARY) $(ABORT_TEST_BINARY) $(SCALAR_TEST_BINARY) $(STRING_TEST_BINARY) $(CONTAINER_TEST_BINARY) $(BPLIST_TEST_BINARY) $(PACKET_TEST_BINARY) $(ENVELOPE_TEST_BINARY) $(SMOKE_BINARY) $(COMPARE_BINARY) $(SCALAR_COMPARE_BINARY) $(STRING_COMPARE_BINARY) $(CONTAINER_COMPARE_BINARY) $(BPLIST_COMPARE_BINARY) $(PACKET_COMPARE_BINARY) $(ENVELOPE_COMPARE_BINARY); do \
		if [ "$$host_uname_s" = "Darwin" ]; then \
			if otool -L "$$file" | sed '1d' | rg -q 'dispatch|swift|objc|icu|CoreFoundation'; then \
				echo "forbidden dynamic dependency detected in $$file"; \
				exit 1; \
			fi; \
		elif command -v ldd >/dev/null 2>&1; then \
			if ldd "$$file" | rg -q 'dispatch|swift|objc|icu|CoreFoundation'; then \
				echo "forbidden dynamic dependency detected in $$file"; \
				exit 1; \
			fi; \
		else \
			echo "no supported dynamic dependency tool for $$host_uname_s"; \
			exit 1; \
		fi; \
	done
	@if nm -a $(TEST_BINARY) $(ABORT_TEST_BINARY) $(SCALAR_TEST_BINARY) $(STRING_TEST_BINARY) $(CONTAINER_TEST_BINARY) $(BPLIST_TEST_BINARY) $(PACKET_TEST_BINARY) $(ENVELOPE_TEST_BINARY) $(SMOKE_BINARY) $(COMPARE_BINARY) $(SCALAR_COMPARE_BINARY) $(STRING_COMPARE_BINARY) $(CONTAINER_COMPARE_BINARY) $(BPLIST_COMPARE_BINARY) $(PACKET_COMPARE_BINARY) $(ENVELOPE_COMPARE_BINARY) $(LIB) | rg -q 'swift|dispatch|objc|icu|CoreFoundation'; then \
		echo "forbidden symbol detected"; \
		exit 1; \
	fi

$(EXPORTS_ACTUAL): $(LIB)
	@mkdir -p $(BUILD_DIR)
	@set -eu; \
	host_uname_s=$$(uname -s 2>/dev/null || printf Unknown); \
	if [ "$$host_uname_s" = "Darwin" ]; then \
		nm -gUj $(LIB) | sed 's/^_//' | rg -v '(^$$|:$$|^_FXCF)' | sort -u > $(EXPORTS_ACTUAL); \
	else \
		nm -g $(LIB) | awk '$$2 ~ /^[TDRS]$$/ { print $$3 }' | rg -v '^_FXCF' | sort -u > $(EXPORTS_ACTUAL); \
	fi

check-exports: $(EXPORTS_ACTUAL) $(EXPORTS_EXPECTED)
	diff -u $(EXPORTS_EXPECTED) $(EXPORTS_ACTUAL)

compare-fx: $(COMPARE_BINARY)
	mkdir -p $(BUILD_DIR)/out
	$(COMPARE_BINARY) > $(COMPARE_OUTPUT)

compare-subset1a-fx: $(SCALAR_COMPARE_BINARY)
	mkdir -p $(BUILD_DIR)/out
	$(SCALAR_COMPARE_BINARY) > $(SCALAR_COMPARE_OUTPUT)

compare-subset1b-fx: $(STRING_COMPARE_BINARY)
	mkdir -p $(BUILD_DIR)/out
	$(STRING_COMPARE_BINARY) > $(STRING_COMPARE_OUTPUT)

compare-subset2a-fx: $(CONTAINER_COMPARE_BINARY)
	mkdir -p $(BUILD_DIR)/out
	$(CONTAINER_COMPARE_BINARY) > $(CONTAINER_COMPARE_OUTPUT)

compare-subset3a-fx: $(BPLIST_COMPARE_BINARY)
	mkdir -p $(BUILD_DIR)/out
	$(BPLIST_COMPARE_BINARY) > $(BPLIST_COMPARE_OUTPUT)

compare-subset7a-fx: $(PACKET_COMPARE_BINARY)
	mkdir -p $(BUILD_DIR)/out
	$(PACKET_COMPARE_BINARY) > $(PACKET_COMPARE_OUTPUT)

compare-subset7b-fx: $(ENVELOPE_COMPARE_BINARY)
	mkdir -p $(BUILD_DIR)/out
	$(ENVELOPE_COMPARE_BINARY) > $(ENVELOPE_COMPARE_OUTPUT)

compare-with-mx: compare-fx $(COMPARE_TOOL)
	@if [ -z "$(MX_JSON)" ]; then \
		echo "set MX_JSON=/path/to/subset0_public_compare_mx.json"; \
		exit 1; \
	fi
	$(ELIXIR_RUN) $(COMPARE_TOOL) --fx-json $(FX_JSON) --mx-json $(MX_JSON) --output $(COMPARE_REPORT)
	cat $(COMPARE_REPORT)

compare-subset1a-with-mx: compare-subset1a-fx $(SCALAR_COMPARE_TOOL)
	@if [ -z "$(MX_JSON)" ]; then \
		echo "set MX_JSON=/path/to/subset1_scalar_core.json"; \
		exit 1; \
	fi
	$(ELIXIR_RUN) $(SCALAR_COMPARE_TOOL) --fx-json $(FX_SCALAR_JSON) --mx-json $(MX_JSON) --output $(SCALAR_COMPARE_REPORT)
	cat $(SCALAR_COMPARE_REPORT)

compare-subset1b-with-mx: compare-subset1b-fx $(STRING_COMPARE_TOOL)
	@if [ -z "$(MX_JSON)" ]; then \
		echo "set MX_JSON=/path/to/subset1b_cfstring_core.json"; \
		exit 1; \
	fi
	$(ELIXIR_RUN) $(STRING_COMPARE_TOOL) --fx-json $(FX_STRING_JSON) --mx-json $(MX_JSON) --output $(STRING_COMPARE_REPORT)
	cat $(STRING_COMPARE_REPORT)

compare-subset2a-with-mx: compare-subset2a-fx $(CONTAINER_COMPARE_TOOL)
	@if [ ! -f "$(FX_CONTAINER_JSON)" ]; then \
		echo "set FX_CONTAINER_JSON=/path/to/subset2a_container_fx.json"; \
		exit 1; \
	fi
	@if [ -z "$(MX_JSON)" ]; then \
		echo "set MX_JSON=/path/to/subset2a_container_core.json"; \
		exit 1; \
	fi
	$(ELIXIR_RUN) $(CONTAINER_COMPARE_TOOL) --fx-json $(FX_CONTAINER_JSON) --mx-json $(MX_JSON) --output $(CONTAINER_COMPARE_REPORT)
	cat $(CONTAINER_COMPARE_REPORT)

compare-subset3a-with-mx: compare-subset3a-fx $(BPLIST_COMPARE_TOOL)
	@if [ ! -f "$(FX_BPLIST_JSON)" ]; then \
		echo "set FX_BPLIST_JSON=/path/to/subset3a_bplist_fx.json"; \
		exit 1; \
	fi
	@if [ -z "$(MX_JSON)" ]; then \
		echo "set MX_JSON=/path/to/subset3a_bplist_core.json"; \
		exit 1; \
	fi
	$(ELIXIR_RUN) $(BPLIST_COMPARE_TOOL) --fx-json $(FX_BPLIST_JSON) --mx-json $(MX_JSON) --output $(BPLIST_COMPARE_REPORT)
	cat $(BPLIST_COMPARE_REPORT)

publish-subset0-artifact: compare-fx $(PUBLISH_SHARED_JSON_SCRIPT)
	$(PUBLISH_SHARED_JSON_SCRIPT) $(COMPARE_OUTPUT) $(SHARED_SUBSET0_JSON)

publish-subset1a-artifact: compare-subset1a-fx $(PUBLISH_SHARED_JSON_SCRIPT)
	$(PUBLISH_SHARED_JSON_SCRIPT) $(SCALAR_COMPARE_OUTPUT) $(SHARED_SUBSET1A_JSON)

publish-subset1b-artifact: compare-subset1b-fx $(PUBLISH_SHARED_JSON_SCRIPT)
	$(PUBLISH_SHARED_JSON_SCRIPT) $(STRING_COMPARE_OUTPUT) $(SHARED_SUBSET1B_JSON)

publish-subset2a-artifact: compare-subset2a-fx $(PUBLISH_SHARED_JSON_SCRIPT)
	$(PUBLISH_SHARED_JSON_SCRIPT) $(CONTAINER_COMPARE_OUTPUT) $(SHARED_SUBSET2A_JSON)

publish-subset3a-artifact: compare-subset3a-fx $(PUBLISH_SHARED_JSON_SCRIPT)
	$(PUBLISH_SHARED_JSON_SCRIPT) $(BPLIST_COMPARE_OUTPUT) $(SHARED_SUBSET3A_JSON)

compare-subset7a-with-expected: compare-subset7a-fx $(PACKET_COMPARE_TOOL)
	@if [ ! -f "$(FX_PACKET_JSON)" ]; then \
		echo "set FX_PACKET_JSON=/path/to/subset7a_control_packet_fx.json"; \
		exit 1; \
	fi
	@if [ ! -f "$(EXPECTED_JSON)" ]; then \
		echo "expected corpus not found at $(EXPECTED_JSON)"; \
		exit 1; \
	fi
	$(ELIXIR_RUN) $(PACKET_COMPARE_TOOL) --fx-json $(FX_PACKET_JSON) --expected-json $(EXPECTED_JSON) --output $(PACKET_COMPARE_REPORT)
	cat $(PACKET_COMPARE_REPORT)

compare-subset7b-with-expected: compare-subset7b-fx $(ENVELOPE_COMPARE_TOOL)
	@if [ ! -f "$(FX_ENVELOPE_JSON)" ]; then \
		echo "set FX_ENVELOPE_JSON=/path/to/subset7b_control_envelope_fx.json"; \
		exit 1; \
	fi
	@if [ ! -f "$(ENVELOPE_EXPECTED_JSON)" ]; then \
		echo "expected envelope corpus not found at $(ENVELOPE_EXPECTED_JSON)"; \
		exit 1; \
	fi
	$(ELIXIR_RUN) $(ENVELOPE_COMPARE_TOOL) --fx-json $(FX_ENVELOPE_JSON) --expected-json $(ENVELOPE_EXPECTED_JSON) --output $(ENVELOPE_COMPARE_REPORT)
	cat $(ENVELOPE_COMPARE_REPORT)

publish-subset7a-artifact: compare-subset7a-fx $(PUBLISH_SHARED_JSON_SCRIPT)
	$(PUBLISH_SHARED_JSON_SCRIPT) $(PACKET_COMPARE_OUTPUT) $(SHARED_SUBSET7A_JSON)

publish-subset7b-artifact: compare-subset7b-fx $(PUBLISH_SHARED_JSON_SCRIPT)
	$(PUBLISH_SHARED_JSON_SCRIPT) $(ENVELOPE_COMPARE_OUTPUT) $(SHARED_SUBSET7B_JSON)

publish-shared-artifacts: publish-subset0-artifact publish-subset1a-artifact publish-subset1b-artifact publish-subset2a-artifact publish-subset3a-artifact publish-subset7a-artifact publish-subset7b-artifact

artifact-run: $(RUN_ARTIFACT_SCRIPT)
	CORT_ARTIFACTS_ROOT="$(ARTIFACTS_ROOT)" BUILD_DIR="$(BUILD_DIR)" STAGE_DIR="$(STAGE_DIR)" $(RUN_ARTIFACT_SCRIPT) $(RUN_DIR)

artifact-subset1a-compare: $(SUBSET1A_COMPARE_ARTIFACT_SCRIPT)
	@if [ ! -f "$(FX_SCALAR_JSON)" ]; then \
		echo "set FX_SCALAR_JSON=/path/to/subset1_scalar_core_fx.json"; \
		exit 1; \
	fi
	@if [ -z "$(MX_JSON)" ]; then \
		echo "set MX_JSON=/path/to/subset1_scalar_core.json"; \
		exit 1; \
	fi
	CORT_ARTIFACTS_ROOT="$(ARTIFACTS_ROOT)" BUILD_DIR="$(BUILD_DIR)" \
		FX_SCALAR_JSON="$(FX_SCALAR_JSON)" MX_JSON="$(MX_JSON)" \
		$(SUBSET1A_COMPARE_ARTIFACT_SCRIPT) $(SCALAR_COMPARE_RUN_DIR)

artifact-subset2a-compare: $(SUBSET2A_COMPARE_ARTIFACT_SCRIPT)
	@if [ ! -f "$(FX_CONTAINER_JSON)" ]; then \
		echo "set FX_CONTAINER_JSON=/path/to/subset2a_container_fx.json"; \
		exit 1; \
	fi
	@if [ -z "$(MX_JSON)" ]; then \
		echo "set MX_JSON=/path/to/subset2a_container_core.json"; \
		exit 1; \
	fi
	CORT_ARTIFACTS_ROOT="$(ARTIFACTS_ROOT)" BUILD_DIR="$(BUILD_DIR)" \
		FX_CONTAINER_JSON="$(FX_CONTAINER_JSON)" MX_JSON="$(MX_JSON)" \
		$(SUBSET2A_COMPARE_ARTIFACT_SCRIPT) $(CONTAINER_COMPARE_RUN_DIR)

artifact-subset3a-compare: $(SUBSET3A_COMPARE_ARTIFACT_SCRIPT)
	@if [ ! -f "$(FX_BPLIST_JSON)" ]; then \
		echo "set FX_BPLIST_JSON=/path/to/subset3a_bplist_fx.json"; \
		exit 1; \
	fi
	@if [ -z "$(MX_JSON)" ]; then \
		echo "set MX_JSON=/path/to/subset3a_bplist_core.json"; \
		exit 1; \
	fi
	CORT_ARTIFACTS_ROOT="$(ARTIFACTS_ROOT)" BUILD_DIR="$(BUILD_DIR)" \
		FX_BPLIST_JSON="$(FX_BPLIST_JSON)" MX_JSON="$(MX_JSON)" \
		$(SUBSET3A_COMPARE_ARTIFACT_SCRIPT) $(BPLIST_COMPARE_RUN_DIR)

test: $(TEST_BINARY) $(ABORT_TEST_BINARY) $(SCALAR_TEST_BINARY) $(STRING_TEST_BINARY) $(CONTAINER_TEST_BINARY) $(BPLIST_TEST_BINARY) $(PACKET_TEST_BINARY) $(ENVELOPE_TEST_BINARY) $(SMOKE_BINARY) check-deps check-exports compare-fx compare-subset1a-fx compare-subset1b-fx compare-subset2a-fx compare-subset3a-fx compare-subset7a-with-expected compare-subset7b-with-expected
	$(TEST_BINARY)
	$(ABORT_TEST_BINARY)
	$(SCALAR_TEST_BINARY)
	$(STRING_TEST_BINARY)
	$(CONTAINER_TEST_BINARY)
	$(BPLIST_TEST_BINARY)
	$(PACKET_TEST_BINARY)
	$(ENVELOPE_TEST_BINARY)
	$(SMOKE_BINARY)

test-installed: install-stage
	mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) $(STRICT_CFLAGS) -I$(STAGE_DIR)$(PREFIX)/include $(LDFLAGS) \
		tests/c_consumer_smoke.c $(STAGE_DIR)$(PREFIX)/lib/libcortfx.a -pthread \
		-o $(BIN_DIR)/c_consumer_smoke_installed
	$(BIN_DIR)/c_consumer_smoke_installed

install: $(LIB) README.md
	mkdir -p $(INSTALL_INCLUDE_DIR)
	mkdir -p $(INSTALL_LIB_DIR)
	mkdir -p $(INSTALL_DOC_DIR)
	$(INSTALL) -m 644 include/CoreFoundation/CFArray.h $(INSTALL_INCLUDE_DIR)/CFArray.h
	$(INSTALL) -m 644 include/CoreFoundation/CFBase.h $(INSTALL_INCLUDE_DIR)/CFBase.h
	$(INSTALL) -m 644 include/CoreFoundation/CFData.h $(INSTALL_INCLUDE_DIR)/CFData.h
	$(INSTALL) -m 644 include/CoreFoundation/CFDate.h $(INSTALL_INCLUDE_DIR)/CFDate.h
	$(INSTALL) -m 644 include/CoreFoundation/CFDictionary.h $(INSTALL_INCLUDE_DIR)/CFDictionary.h
	$(INSTALL) -m 644 include/CoreFoundation/CFError.h $(INSTALL_INCLUDE_DIR)/CFError.h
	$(INSTALL) -m 644 include/CoreFoundation/CFNumber.h $(INSTALL_INCLUDE_DIR)/CFNumber.h
	$(INSTALL) -m 644 include/CoreFoundation/CFPropertyList.h $(INSTALL_INCLUDE_DIR)/CFPropertyList.h
	$(INSTALL) -m 644 include/CoreFoundation/CFString.h $(INSTALL_INCLUDE_DIR)/CFString.h
	$(INSTALL) -m 644 include/CoreFoundation/CFRuntime.h $(INSTALL_INCLUDE_DIR)/CFRuntime.h
	$(INSTALL) -m 644 include/CoreFoundation/CoreFoundation.h $(INSTALL_INCLUDE_DIR)/CoreFoundation.h
	$(INSTALL) -m 644 $(LIB) $(INSTALL_LIB_DIR)/libcortfx.a
	$(INSTALL) -m 644 README.md $(INSTALL_DOC_DIR)/README.md

install-stage: $(LIB) README.md
	$(MAKE) install DESTDIR=$(STAGE_DIR) PREFIX=$(PREFIX)

$(LIB): $(LIB_OBJECTS)
	mkdir -p $(LIB_DIR)
	$(AR) rcs $(LIB) $(LIB_OBJECTS)
	$(RANLIB) $(LIB)

$(TEST_BINARY): $(OBJ_DIR)/tests/runtime_ownership_tests.o $(LIB)
	mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) $(STRICT_CFLAGS) $(LDFLAGS) -o $(TEST_BINARY) $(OBJ_DIR)/tests/runtime_ownership_tests.o $(LIB) -pthread

$(ABORT_TEST_BINARY): $(OBJ_DIR)/tests/runtime_abort_tests.o $(LIB)
	mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) $(STRICT_CFLAGS) $(LDFLAGS) -o $(ABORT_TEST_BINARY) $(OBJ_DIR)/tests/runtime_abort_tests.o $(LIB) -pthread

$(SCALAR_TEST_BINARY): $(OBJ_DIR)/tests/scalar_core_tests.o $(LIB)
	mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) $(STRICT_CFLAGS) $(LDFLAGS) -o $(SCALAR_TEST_BINARY) $(OBJ_DIR)/tests/scalar_core_tests.o $(LIB) -pthread

$(STRING_TEST_BINARY): $(OBJ_DIR)/tests/string_core_tests.o $(LIB)
	mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) $(STRICT_CFLAGS) $(LDFLAGS) -o $(STRING_TEST_BINARY) $(OBJ_DIR)/tests/string_core_tests.o $(LIB) -pthread

$(CONTAINER_TEST_BINARY): $(OBJ_DIR)/tests/container_core_tests.o $(LIB)
	mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) $(STRICT_CFLAGS) $(LDFLAGS) -o $(CONTAINER_TEST_BINARY) $(OBJ_DIR)/tests/container_core_tests.o $(LIB) -pthread

$(BPLIST_TEST_BINARY): $(OBJ_DIR)/tests/bplist_core_tests.o $(LIB)
	mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) $(STRICT_CFLAGS) $(LDFLAGS) -o $(BPLIST_TEST_BINARY) $(OBJ_DIR)/tests/bplist_core_tests.o $(LIB) -pthread

$(PACKET_TEST_BINARY): $(OBJ_DIR)/tests/control_packet_tests.o $(LIB)
	mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) $(STRICT_CFLAGS) $(LDFLAGS) -o $(PACKET_TEST_BINARY) $(OBJ_DIR)/tests/control_packet_tests.o $(LIB) -pthread

$(ENVELOPE_TEST_BINARY): $(OBJ_DIR)/tests/control_envelope_tests.o $(LIB)
	mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) $(STRICT_CFLAGS) $(LDFLAGS) -o $(ENVELOPE_TEST_BINARY) $(OBJ_DIR)/tests/control_envelope_tests.o $(LIB) -pthread

$(SMOKE_BINARY): $(OBJ_DIR)/tests/c_consumer_smoke.o $(LIB)
	mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) $(STRICT_CFLAGS) $(LDFLAGS) -o $(SMOKE_BINARY) $(OBJ_DIR)/tests/c_consumer_smoke.o $(LIB) -pthread

$(COMPARE_BINARY): $(OBJ_DIR)/tests/cort_subset0_public_allocator_compare.o $(LIB)
	mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) $(STRICT_CFLAGS) $(LDFLAGS) -o $(COMPARE_BINARY) $(OBJ_DIR)/tests/cort_subset0_public_allocator_compare.o $(LIB) -pthread

$(SCALAR_COMPARE_BINARY): $(OBJ_DIR)/tests/cort_subset1_scalar_core_fx.o $(LIB)
	mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) $(STRICT_CFLAGS) $(LDFLAGS) -o $(SCALAR_COMPARE_BINARY) $(OBJ_DIR)/tests/cort_subset1_scalar_core_fx.o $(LIB) -pthread

$(STRING_COMPARE_BINARY): $(OBJ_DIR)/tests/cort_subset1b_cfstring_fx.o $(LIB)
	mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) $(STRICT_CFLAGS) $(LDFLAGS) -o $(STRING_COMPARE_BINARY) $(OBJ_DIR)/tests/cort_subset1b_cfstring_fx.o $(LIB) -pthread

$(CONTAINER_COMPARE_BINARY): $(OBJ_DIR)/tests/cort_subset2a_container_fx.o $(LIB)
	mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) $(STRICT_CFLAGS) $(LDFLAGS) -o $(CONTAINER_COMPARE_BINARY) $(OBJ_DIR)/tests/cort_subset2a_container_fx.o $(LIB) -pthread

$(BPLIST_COMPARE_BINARY): $(OBJ_DIR)/tests/cort_subset3a_bplist_fx.o $(LIB)
	mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) $(STRICT_CFLAGS) $(LDFLAGS) -o $(BPLIST_COMPARE_BINARY) $(OBJ_DIR)/tests/cort_subset3a_bplist_fx.o $(LIB) -pthread

$(PACKET_COMPARE_BINARY): $(OBJ_DIR)/tests/cort_subset7a_control_packet_fx.o $(LIB)
	mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) $(STRICT_CFLAGS) $(LDFLAGS) -o $(PACKET_COMPARE_BINARY) $(OBJ_DIR)/tests/cort_subset7a_control_packet_fx.o $(LIB) -pthread

$(ENVELOPE_COMPARE_BINARY): $(OBJ_DIR)/tests/cort_subset7b_control_envelope_fx.o $(LIB)
	mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) $(STRICT_CFLAGS) $(LDFLAGS) -o $(ENVELOPE_COMPARE_BINARY) $(OBJ_DIR)/tests/cort_subset7b_control_envelope_fx.o $(LIB) -pthread

$(OBJ_DIR)/src/CFBase.o: src/CFBase.c src/FXCFInternal.h include/CoreFoundation/CFBase.h include/CoreFoundation/CFRuntime.h
	mkdir -p $(OBJ_DIR)/src
	$(CC) $(CPPFLAGS) $(CFLAGS) $(STRICT_CFLAGS) -c src/CFBase.c -o $(OBJ_DIR)/src/CFBase.o

$(OBJ_DIR)/src/CFArray.o: src/CFArray.c src/FXCFInternal.h include/CoreFoundation/CFArray.h include/CoreFoundation/CFBase.h include/CoreFoundation/CFRuntime.h
	mkdir -p $(OBJ_DIR)/src
	$(CC) $(CPPFLAGS) $(CFLAGS) $(STRICT_CFLAGS) -c src/CFArray.c -o $(OBJ_DIR)/src/CFArray.o

$(OBJ_DIR)/src/CFBinaryPList.o: src/CFBinaryPList.c src/FXCFInternal.h include/CoreFoundation/CFArray.h include/CoreFoundation/CFBase.h include/CoreFoundation/CFData.h include/CoreFoundation/CFDate.h include/CoreFoundation/CFDictionary.h include/CoreFoundation/CFError.h include/CoreFoundation/CFNumber.h include/CoreFoundation/CFPropertyList.h include/CoreFoundation/CFString.h include/CoreFoundation/CFRuntime.h
	mkdir -p $(OBJ_DIR)/src
	$(CC) $(CPPFLAGS) $(CFLAGS) $(STRICT_CFLAGS) -c src/CFBinaryPList.c -o $(OBJ_DIR)/src/CFBinaryPList.o

$(OBJ_DIR)/src/CFData.o: src/CFData.c src/FXCFInternal.h include/CoreFoundation/CFBase.h include/CoreFoundation/CFData.h include/CoreFoundation/CFRuntime.h
	mkdir -p $(OBJ_DIR)/src
	$(CC) $(CPPFLAGS) $(CFLAGS) $(STRICT_CFLAGS) -c src/CFData.c -o $(OBJ_DIR)/src/CFData.o

$(OBJ_DIR)/src/CFDate.o: src/CFDate.c src/FXCFInternal.h include/CoreFoundation/CFBase.h include/CoreFoundation/CFDate.h include/CoreFoundation/CFRuntime.h
	mkdir -p $(OBJ_DIR)/src
	$(CC) $(CPPFLAGS) $(CFLAGS) $(STRICT_CFLAGS) -c src/CFDate.c -o $(OBJ_DIR)/src/CFDate.o

$(OBJ_DIR)/src/CFDictionary.o: src/CFDictionary.c src/FXCFInternal.h include/CoreFoundation/CFBase.h include/CoreFoundation/CFDictionary.h include/CoreFoundation/CFRuntime.h
	mkdir -p $(OBJ_DIR)/src
	$(CC) $(CPPFLAGS) $(CFLAGS) $(STRICT_CFLAGS) -c src/CFDictionary.c -o $(OBJ_DIR)/src/CFDictionary.o

$(OBJ_DIR)/src/CFError.o: src/CFError.c src/FXCFInternal.h include/CoreFoundation/CFBase.h include/CoreFoundation/CFError.h include/CoreFoundation/CFRuntime.h
	mkdir -p $(OBJ_DIR)/src
	$(CC) $(CPPFLAGS) $(CFLAGS) $(STRICT_CFLAGS) -c src/CFError.c -o $(OBJ_DIR)/src/CFError.o

$(OBJ_DIR)/src/FXCFControlPacket.o: src/FXCFControlPacket.c src/FXCFControlPacket.h src/FXCFInternal.h include/CoreFoundation/CoreFoundation.h
	mkdir -p $(OBJ_DIR)/src
	$(CC) $(CPPFLAGS) $(CFLAGS) $(STRICT_CFLAGS) -c src/FXCFControlPacket.c -o $(OBJ_DIR)/src/FXCFControlPacket.o

$(OBJ_DIR)/src/CFNumber.o: src/CFNumber.c src/FXCFInternal.h include/CoreFoundation/CFBase.h include/CoreFoundation/CFNumber.h include/CoreFoundation/CFRuntime.h
	mkdir -p $(OBJ_DIR)/src
	$(CC) $(CPPFLAGS) $(CFLAGS) $(STRICT_CFLAGS) -c src/CFNumber.c -o $(OBJ_DIR)/src/CFNumber.o

$(OBJ_DIR)/src/CFPropertyList.o: src/CFPropertyList.c src/FXCFInternal.h include/CoreFoundation/CFBase.h include/CoreFoundation/CFData.h include/CoreFoundation/CFError.h include/CoreFoundation/CFPropertyList.h include/CoreFoundation/CFRuntime.h
	mkdir -p $(OBJ_DIR)/src
	$(CC) $(CPPFLAGS) $(CFLAGS) $(STRICT_CFLAGS) -c src/CFPropertyList.c -o $(OBJ_DIR)/src/CFPropertyList.o

$(OBJ_DIR)/src/CFString.o: src/CFString.c src/FXCFInternal.h include/CoreFoundation/CFBase.h include/CoreFoundation/CFString.h include/CoreFoundation/CFRuntime.h
	mkdir -p $(OBJ_DIR)/src
	$(CC) $(CPPFLAGS) $(CFLAGS) $(STRICT_CFLAGS) -c src/CFString.c -o $(OBJ_DIR)/src/CFString.o

$(OBJ_DIR)/src/CFRuntime.o: src/CFRuntime.c src/FXCFInternal.h include/CoreFoundation/CFBase.h include/CoreFoundation/CFRuntime.h
	mkdir -p $(OBJ_DIR)/src
	$(CC) $(CPPFLAGS) $(CFLAGS) $(STRICT_CFLAGS) -c src/CFRuntime.c -o $(OBJ_DIR)/src/CFRuntime.o

$(OBJ_DIR)/tests/runtime_ownership_tests.o: tests/runtime_ownership_tests.c include/CoreFoundation/CoreFoundation.h include/CoreFoundation/CFBase.h include/CoreFoundation/CFRuntime.h
	mkdir -p $(OBJ_DIR)/tests
	$(CC) $(CPPFLAGS) $(CFLAGS) $(STRICT_CFLAGS) -c tests/runtime_ownership_tests.c -o $(OBJ_DIR)/tests/runtime_ownership_tests.o

$(OBJ_DIR)/tests/runtime_abort_tests.o: tests/runtime_abort_tests.c include/CoreFoundation/CoreFoundation.h include/CoreFoundation/CFBase.h include/CoreFoundation/CFRuntime.h
	mkdir -p $(OBJ_DIR)/tests
	$(CC) $(CPPFLAGS) $(CFLAGS) $(STRICT_CFLAGS) -c tests/runtime_abort_tests.c -o $(OBJ_DIR)/tests/runtime_abort_tests.o

$(OBJ_DIR)/tests/scalar_core_tests.o: tests/scalar_core_tests.c include/CoreFoundation/CoreFoundation.h include/CoreFoundation/CFBase.h include/CoreFoundation/CFData.h include/CoreFoundation/CFDate.h include/CoreFoundation/CFNumber.h include/CoreFoundation/CFRuntime.h
	mkdir -p $(OBJ_DIR)/tests
	$(CC) $(CPPFLAGS) $(CFLAGS) $(STRICT_CFLAGS) -c tests/scalar_core_tests.c -o $(OBJ_DIR)/tests/scalar_core_tests.o

$(OBJ_DIR)/tests/string_core_tests.o: tests/string_core_tests.c include/CoreFoundation/CoreFoundation.h include/CoreFoundation/CFBase.h include/CoreFoundation/CFString.h include/CoreFoundation/CFRuntime.h
	mkdir -p $(OBJ_DIR)/tests
	$(CC) $(CPPFLAGS) $(CFLAGS) $(STRICT_CFLAGS) -c tests/string_core_tests.c -o $(OBJ_DIR)/tests/string_core_tests.o

$(OBJ_DIR)/tests/container_core_tests.o: tests/container_core_tests.c include/CoreFoundation/CoreFoundation.h include/CoreFoundation/CFArray.h include/CoreFoundation/CFBase.h include/CoreFoundation/CFData.h include/CoreFoundation/CFDictionary.h include/CoreFoundation/CFString.h include/CoreFoundation/CFRuntime.h
	mkdir -p $(OBJ_DIR)/tests
	$(CC) $(CPPFLAGS) $(CFLAGS) $(STRICT_CFLAGS) -c tests/container_core_tests.c -o $(OBJ_DIR)/tests/container_core_tests.o

$(OBJ_DIR)/tests/bplist_core_tests.o: tests/bplist_core_tests.c include/CoreFoundation/CoreFoundation.h include/CoreFoundation/CFArray.h include/CoreFoundation/CFBase.h include/CoreFoundation/CFData.h include/CoreFoundation/CFDate.h include/CoreFoundation/CFDictionary.h include/CoreFoundation/CFError.h include/CoreFoundation/CFNumber.h include/CoreFoundation/CFPropertyList.h include/CoreFoundation/CFString.h include/CoreFoundation/CFRuntime.h
	mkdir -p $(OBJ_DIR)/tests
	$(CC) $(CPPFLAGS) $(CFLAGS) $(STRICT_CFLAGS) -c tests/bplist_core_tests.c -o $(OBJ_DIR)/tests/bplist_core_tests.o

$(SUBSET7A_CASES_HEADER): $(SUBSET7A_CASES_HEADER_BUILDER) ../fixtures/control/bplist_packet_corpus_v1.json ../fixtures/control/bplist_packet_corpus_v1.source.json ../fixtures/control/bplist_packet_rejection_corpus_v1.json ../fixtures/control/subset7a_control_packet_expected_v1.json
	mkdir -p tests/generated
	$(ELIXIR_RUN) $(SUBSET7A_CASES_HEADER_BUILDER)

$(OBJ_DIR)/tests/control_packet_tests.o: tests/control_packet_tests.c tests/subset7a_control_packet_support.h $(SUBSET7A_CASES_HEADER) include/CoreFoundation/CoreFoundation.h src/FXCFControlPacket.h
	mkdir -p $(OBJ_DIR)/tests
	$(CC) $(CPPFLAGS) $(CFLAGS) $(STRICT_CFLAGS) -c tests/control_packet_tests.c -o $(OBJ_DIR)/tests/control_packet_tests.o

$(OBJ_DIR)/tests/control_envelope_tests.o: tests/control_envelope_tests.c tests/subset7b_control_envelope_support.h $(SUBSET7A_CASES_HEADER) include/CoreFoundation/CoreFoundation.h src/FXCFControlPacket.h
	mkdir -p $(OBJ_DIR)/tests
	$(CC) $(CPPFLAGS) $(CFLAGS) $(STRICT_CFLAGS) -c tests/control_envelope_tests.c -o $(OBJ_DIR)/tests/control_envelope_tests.o

$(OBJ_DIR)/tests/c_consumer_smoke.o: tests/c_consumer_smoke.c include/CoreFoundation/CoreFoundation.h include/CoreFoundation/CFBase.h include/CoreFoundation/CFRuntime.h
	mkdir -p $(OBJ_DIR)/tests
	$(CC) $(CPPFLAGS) $(CFLAGS) $(STRICT_CFLAGS) -c tests/c_consumer_smoke.c -o $(OBJ_DIR)/tests/c_consumer_smoke.o

$(OBJ_DIR)/tests/cort_subset0_public_allocator_compare.o: ../cort-mx/src/cort_subset0_public_allocator_compare.c include/CoreFoundation/CoreFoundation.h include/CoreFoundation/CFBase.h include/CoreFoundation/CFRuntime.h
	mkdir -p $(OBJ_DIR)/tests
	$(CC) $(CPPFLAGS) $(CFLAGS) $(STRICT_CFLAGS) -c ../cort-mx/src/cort_subset0_public_allocator_compare.c -o $(OBJ_DIR)/tests/cort_subset0_public_allocator_compare.o

$(OBJ_DIR)/tests/cort_subset1_scalar_core_fx.o: tests/cort_subset1_scalar_core_fx.c include/CoreFoundation/CoreFoundation.h include/CoreFoundation/CFBase.h include/CoreFoundation/CFData.h include/CoreFoundation/CFDate.h include/CoreFoundation/CFNumber.h include/CoreFoundation/CFRuntime.h
	mkdir -p $(OBJ_DIR)/tests
	$(CC) $(CPPFLAGS) $(CFLAGS) $(STRICT_CFLAGS) -c tests/cort_subset1_scalar_core_fx.c -o $(OBJ_DIR)/tests/cort_subset1_scalar_core_fx.o

$(OBJ_DIR)/tests/cort_subset1b_cfstring_fx.o: tests/cort_subset1b_cfstring_fx.c include/CoreFoundation/CoreFoundation.h include/CoreFoundation/CFBase.h include/CoreFoundation/CFString.h include/CoreFoundation/CFRuntime.h
	mkdir -p $(OBJ_DIR)/tests
	$(CC) $(CPPFLAGS) $(CFLAGS) $(STRICT_CFLAGS) -c tests/cort_subset1b_cfstring_fx.c -o $(OBJ_DIR)/tests/cort_subset1b_cfstring_fx.o

$(OBJ_DIR)/tests/cort_subset2a_container_fx.o: tests/cort_subset2a_container_fx.c include/CoreFoundation/CoreFoundation.h include/CoreFoundation/CFArray.h include/CoreFoundation/CFBase.h include/CoreFoundation/CFData.h include/CoreFoundation/CFDictionary.h include/CoreFoundation/CFString.h include/CoreFoundation/CFRuntime.h
	mkdir -p $(OBJ_DIR)/tests
	$(CC) $(CPPFLAGS) $(CFLAGS) $(STRICT_CFLAGS) -c tests/cort_subset2a_container_fx.c -o $(OBJ_DIR)/tests/cort_subset2a_container_fx.o

$(OBJ_DIR)/tests/cort_subset3a_bplist_fx.o: tests/cort_subset3a_bplist_fx.c include/CoreFoundation/CoreFoundation.h include/CoreFoundation/CFArray.h include/CoreFoundation/CFBase.h include/CoreFoundation/CFData.h include/CoreFoundation/CFDate.h include/CoreFoundation/CFDictionary.h include/CoreFoundation/CFError.h include/CoreFoundation/CFNumber.h include/CoreFoundation/CFPropertyList.h include/CoreFoundation/CFString.h include/CoreFoundation/CFRuntime.h
	mkdir -p $(OBJ_DIR)/tests
	$(CC) $(CPPFLAGS) $(CFLAGS) $(STRICT_CFLAGS) -c tests/cort_subset3a_bplist_fx.c -o $(OBJ_DIR)/tests/cort_subset3a_bplist_fx.o

$(OBJ_DIR)/tests/cort_subset7a_control_packet_fx.o: tests/cort_subset7a_control_packet_fx.c tests/subset7a_control_packet_support.h $(SUBSET7A_CASES_HEADER) include/CoreFoundation/CoreFoundation.h src/FXCFControlPacket.h
	mkdir -p $(OBJ_DIR)/tests
	$(CC) $(CPPFLAGS) $(CFLAGS) $(STRICT_CFLAGS) -c tests/cort_subset7a_control_packet_fx.c -o $(OBJ_DIR)/tests/cort_subset7a_control_packet_fx.o

$(OBJ_DIR)/tests/cort_subset7b_control_envelope_fx.o: tests/cort_subset7b_control_envelope_fx.c tests/subset7b_control_envelope_support.h $(SUBSET7A_CASES_HEADER) include/CoreFoundation/CoreFoundation.h src/FXCFControlPacket.h
	mkdir -p $(OBJ_DIR)/tests
	$(CC) $(CPPFLAGS) $(CFLAGS) $(STRICT_CFLAGS) -c tests/cort_subset7b_control_envelope_fx.c -o $(OBJ_DIR)/tests/cort_subset7b_control_envelope_fx.o

clean:
	rm -rf $(BUILD_DIR) $(STAGE_DIR)
