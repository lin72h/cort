CC ?= cc
AR ?= ar
RANLIB ?= ranlib
INSTALL ?= install

STRICT_CFLAGS := -std=c11 -Wall -Wextra -Werror -pedantic -pthread -fvisibility=hidden
CPPFLAGS += -Iinclude
LDFLAGS +=
PREFIX ?= /usr/local
DESTDIR ?=
REPO_ROOT != cd .. && pwd
REPO_PARENT != cd ../.. && pwd
REPO_NAME != basename "${REPO_ROOT}"
ARTIFACTS_ROOT != if [ -n "${CORT_ARTIFACTS_ROOT}" ]; then printf '%s\n' "${CORT_ARTIFACTS_ROOT}"; else printf '%s/%s-artifacts\n' "${REPO_PARENT}" "${REPO_NAME}"; fi

BUILD_DIR ?= $(ARTIFACTS_ROOT)/cort-fx/build
LIB_DIR := $(BUILD_DIR)/lib
OBJ_DIR := $(BUILD_DIR)/obj
BIN_DIR := $(BUILD_DIR)/bin
STAGE_DIR ?= $(BUILD_DIR)/stage
EXPORTS_DIR := exports
RUN_DIR ?= $(ARTIFACTS_ROOT)/cort-fx/runs/subset0-fx-local
ELIXIR_RUN := $(REPO_ROOT)/tools/run_elixir.sh
COMPARE_TOOL := $(REPO_ROOT)/tools/compare_subset0_json.exs
RUN_ARTIFACT_SCRIPT := scripts/run_subset0_fx_artifacts.sh

LIB := $(LIB_DIR)/libcortfx.a
LIB_OBJECTS := \
	$(OBJ_DIR)/src/CFBase.o \
	$(OBJ_DIR)/src/CFRuntime.o
TEST_BINARY := $(BIN_DIR)/runtime_ownership_tests
ABORT_TEST_BINARY := $(BIN_DIR)/runtime_abort_tests
SMOKE_BINARY := $(BIN_DIR)/c_consumer_smoke
COMPARE_BINARY := $(BIN_DIR)/subset0_public_compare_fx
COMPARE_OUTPUT := $(BUILD_DIR)/out/subset0_public_compare_fx.json
FX_JSON ?= $(COMPARE_OUTPUT)
COMPARE_REPORT ?= $(BUILD_DIR)/out/subset0_fx_vs_mx_report.md
EXPORTS_EXPECTED := $(EXPORTS_DIR)/subset0-exported-symbols.txt
EXPORTS_ACTUAL := $(BUILD_DIR)/subset0-exported-symbols.txt
INSTALL_INCLUDE_DIR := $(DESTDIR)$(PREFIX)/include/CoreFoundation
INSTALL_LIB_DIR := $(DESTDIR)$(PREFIX)/lib
INSTALL_DOC_DIR := $(DESTDIR)$(PREFIX)/share/doc/cort-fx

.PHONY: all clean check-deps check-exports compare-fx compare-with-mx artifact-run install install-stage test test-installed

all: $(LIB) $(TEST_BINARY) $(ABORT_TEST_BINARY) $(SMOKE_BINARY) $(COMPARE_BINARY)

check-deps: $(TEST_BINARY) $(ABORT_TEST_BINARY) $(SMOKE_BINARY) $(COMPARE_BINARY) $(LIB)
	@if ldd $(TEST_BINARY) | rg -q 'dispatch|swift|objc|icu|CoreFoundation'; then \
		echo "forbidden dynamic dependency detected"; \
		exit 1; \
	fi
	@if ldd $(ABORT_TEST_BINARY) | rg -q 'dispatch|swift|objc|icu|CoreFoundation'; then \
		echo "forbidden dynamic dependency detected"; \
		exit 1; \
	fi
	@if ldd $(SMOKE_BINARY) | rg -q 'dispatch|swift|objc|icu|CoreFoundation'; then \
		echo "forbidden dynamic dependency detected"; \
		exit 1; \
	fi
	@if ldd $(COMPARE_BINARY) | rg -q 'dispatch|swift|objc|icu|CoreFoundation'; then \
		echo "forbidden dynamic dependency detected"; \
		exit 1; \
	fi
	@if nm -a $(TEST_BINARY) $(ABORT_TEST_BINARY) $(SMOKE_BINARY) $(COMPARE_BINARY) $(LIB) | rg -q 'swift|dispatch|objc|icu|CoreFoundation'; then \
		echo "forbidden symbol detected"; \
		exit 1; \
	fi

$(EXPORTS_ACTUAL): $(LIB)
	mkdir -p $(BUILD_DIR)
	nm -g $(LIB) | awk '$$2 ~ /^[TDRS]$$/ { print $$3 }' | sort -u > $(EXPORTS_ACTUAL)

check-exports: $(EXPORTS_ACTUAL) $(EXPORTS_EXPECTED)
	diff -u $(EXPORTS_EXPECTED) $(EXPORTS_ACTUAL)

compare-fx: $(COMPARE_BINARY)
	mkdir -p $(BUILD_DIR)/out
	$(COMPARE_BINARY) > $(COMPARE_OUTPUT)

compare-with-mx: compare-fx $(COMPARE_TOOL)
	@if [ -z "$(MX_JSON)" ]; then \
		echo "set MX_JSON=/path/to/subset0_public_compare_mx.json"; \
		exit 1; \
	fi
	$(ELIXIR_RUN) $(COMPARE_TOOL) --fx-json $(FX_JSON) --mx-json $(MX_JSON) --output $(COMPARE_REPORT)
	cat $(COMPARE_REPORT)

artifact-run: $(RUN_ARTIFACT_SCRIPT)
	CORT_ARTIFACTS_ROOT="$(ARTIFACTS_ROOT)" BUILD_DIR="$(BUILD_DIR)" STAGE_DIR="$(STAGE_DIR)" $(RUN_ARTIFACT_SCRIPT) $(RUN_DIR)

test: $(TEST_BINARY) $(ABORT_TEST_BINARY) $(SMOKE_BINARY) check-deps check-exports compare-fx
	$(TEST_BINARY)
	$(ABORT_TEST_BINARY)
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
	$(INSTALL) -m 644 include/CoreFoundation/CFBase.h $(INSTALL_INCLUDE_DIR)/CFBase.h
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

$(SMOKE_BINARY): $(OBJ_DIR)/tests/c_consumer_smoke.o $(LIB)
	mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) $(STRICT_CFLAGS) $(LDFLAGS) -o $(SMOKE_BINARY) $(OBJ_DIR)/tests/c_consumer_smoke.o $(LIB) -pthread

$(COMPARE_BINARY): $(OBJ_DIR)/tests/cort_subset0_public_allocator_compare.o $(LIB)
	mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) $(STRICT_CFLAGS) $(LDFLAGS) -o $(COMPARE_BINARY) $(OBJ_DIR)/tests/cort_subset0_public_allocator_compare.o $(LIB) -pthread

$(OBJ_DIR)/src/CFBase.o: src/CFBase.c src/FXCFInternal.h include/CoreFoundation/CFBase.h include/CoreFoundation/CFRuntime.h
	mkdir -p $(OBJ_DIR)/src
	$(CC) $(CPPFLAGS) $(CFLAGS) $(STRICT_CFLAGS) -c src/CFBase.c -o $(OBJ_DIR)/src/CFBase.o

$(OBJ_DIR)/src/CFRuntime.o: src/CFRuntime.c src/FXCFInternal.h include/CoreFoundation/CFBase.h include/CoreFoundation/CFRuntime.h
	mkdir -p $(OBJ_DIR)/src
	$(CC) $(CPPFLAGS) $(CFLAGS) $(STRICT_CFLAGS) -c src/CFRuntime.c -o $(OBJ_DIR)/src/CFRuntime.o

$(OBJ_DIR)/tests/runtime_ownership_tests.o: tests/runtime_ownership_tests.c include/CoreFoundation/CoreFoundation.h include/CoreFoundation/CFBase.h include/CoreFoundation/CFRuntime.h
	mkdir -p $(OBJ_DIR)/tests
	$(CC) $(CPPFLAGS) $(CFLAGS) $(STRICT_CFLAGS) -c tests/runtime_ownership_tests.c -o $(OBJ_DIR)/tests/runtime_ownership_tests.o

$(OBJ_DIR)/tests/runtime_abort_tests.o: tests/runtime_abort_tests.c include/CoreFoundation/CoreFoundation.h include/CoreFoundation/CFBase.h include/CoreFoundation/CFRuntime.h
	mkdir -p $(OBJ_DIR)/tests
	$(CC) $(CPPFLAGS) $(CFLAGS) $(STRICT_CFLAGS) -c tests/runtime_abort_tests.c -o $(OBJ_DIR)/tests/runtime_abort_tests.o

$(OBJ_DIR)/tests/c_consumer_smoke.o: tests/c_consumer_smoke.c include/CoreFoundation/CoreFoundation.h include/CoreFoundation/CFBase.h include/CoreFoundation/CFRuntime.h
	mkdir -p $(OBJ_DIR)/tests
	$(CC) $(CPPFLAGS) $(CFLAGS) $(STRICT_CFLAGS) -c tests/c_consumer_smoke.c -o $(OBJ_DIR)/tests/c_consumer_smoke.o

$(OBJ_DIR)/tests/cort_subset0_public_allocator_compare.o: ../cort-mx/src/cort_subset0_public_allocator_compare.c include/CoreFoundation/CoreFoundation.h include/CoreFoundation/CFBase.h include/CoreFoundation/CFRuntime.h
	mkdir -p $(OBJ_DIR)/tests
	$(CC) $(CPPFLAGS) $(CFLAGS) $(STRICT_CFLAGS) -c ../cort-mx/src/cort_subset0_public_allocator_compare.c -o $(OBJ_DIR)/tests/cort_subset0_public_allocator_compare.o

clean:
	rm -rf $(BUILD_DIR) $(STAGE_DIR)
