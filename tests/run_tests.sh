#!/bin/bash
# CrashLang test runner — validates all examples produce expected output.
# Usage: ./tests/run_tests.sh [path-to-crashlang-binary]

set -euo pipefail

BINARY="${1:-./build/crashlang}"
PASS=0
FAIL=0
ERRORS=""

red()   { printf "\033[31m%s\033[0m" "$1"; }
green() { printf "\033[32m%s\033[0m" "$1"; }
bold()  { printf "\033[1m%s\033[0m" "$1"; }

# Test helper: expect success (exit 0) and check that output contains a string.
expect_success() {
    local name="$1"
    local file="$2"
    local expected="$3"
    local flags="${4:-}"

    local output
    if output=$($BINARY $flags "$file" 2>&1); then
        if echo "$output" | grep -qF "$expected"; then
            PASS=$((PASS + 1))
            printf "  $(green "PASS")  %s\n" "$name"
        else
            FAIL=$((FAIL + 1))
            ERRORS+="  FAIL: $name — expected output to contain: '$expected'\n"
            printf "  $(red "FAIL")  %s — missing: '%s'\n" "$name" "$expected"
        fi
    else
        FAIL=$((FAIL + 1))
        ERRORS+="  FAIL: $name — expected exit 0 but got non-zero\n"
        printf "  $(red "FAIL")  %s — unexpected failure\n" "$name"
    fi
}

# Test helper: expect crash (exit 1) and check the error kind.
expect_crash() {
    local name="$1"
    local file="$2"
    local error_kind="$3"

    local output
    if output=$($BINARY --no-color "$file" 2>&1); then
        FAIL=$((FAIL + 1))
        ERRORS+="  FAIL: $name — expected crash but program succeeded\n"
        printf "  $(red "FAIL")  %s — expected crash\n" "$name"
    else
        if echo "$output" | grep -qF "$error_kind"; then
            PASS=$((PASS + 1))
            printf "  $(green "PASS")  %s\n" "$name"
        else
            FAIL=$((FAIL + 1))
            ERRORS+="  FAIL: $name — expected '$error_kind' in output\n"
            printf "  $(red "FAIL")  %s — wrong error kind\n" "$name"
        fi
    fi
}

# Test helper: expect leak warning.
expect_leaks() {
    local name="$1"
    local file="$2"
    local leak_count="$3"

    local output
    output=$($BINARY --no-color --check-leaks "$file" 2>&1) || true
    if echo "$output" | grep -qF "$leak_count object(s) were never freed"; then
        PASS=$((PASS + 1))
        printf "  $(green "PASS")  %s\n" "$name"
    else
        FAIL=$((FAIL + 1))
        ERRORS+="  FAIL: $name — expected $leak_count leak(s)\n"
        printf "  $(red "FAIL")  %s — wrong leak count\n" "$name"
    fi
}

echo ""
bold "CrashLang Test Suite"
echo ""
echo "Binary: $BINARY"
echo ""

# ── Phase 1-2: Core language ───────────────────────────────────────────────────

bold "Phase 1-2: Core language"
echo ""

expect_success "Hello world + fibonacci" \
    "examples/01_hello_world.cl" \
    "All assertions passed."

expect_crash "Error test (deliberate type mismatch)" \
    "examples/03_error_test.cl" \
    "TypeMismatch"

# ── Phase 3: Memory model ──────────────────────────────────────────────────────

echo ""
bold "Phase 3: Memory model"
echo ""

expect_success "Heap basics (alloc, fields, ref, deref, free)" \
    "examples/04_heap_basic.cl" \
    "Freed successfully."

expect_crash "Use-after-free detection" \
    "examples/05_use_after_free.cl" \
    "UseAfterFree"

expect_crash "Double-free detection" \
    "examples/06_double_free.cl" \
    "DoubleFree"

expect_crash "Ownership violation (use after move)" \
    "examples/07_ownership.cl" \
    "TypeMismatch"

expect_crash "Null dereference" \
    "examples/08_null_deref.cl" \
    "NullDereference"

expect_crash "Cross-function use-after-free" \
    "examples/09_flagship.cl" \
    "UseAfterFree"

# ── Phase 4: Diagnostics ───────────────────────────────────────────────────────

echo ""
bold "Phase 4: Diagnostics engine"
echo ""

# Check that crash reports contain the structured sections.
expect_crash "Crash report has source context" \
    "examples/05_use_after_free.cl" \
    "What happened:"

expect_crash "Crash report has timeline" \
    "examples/05_use_after_free.cl" \
    "Object lifetime"

expect_crash "Crash report has suggestion" \
    "examples/05_use_after_free.cl" \
    "Suggested fix:"

expect_crash "Crash report has explanation" \
    "examples/06_double_free.cl" \
    "Why this is a problem:"

expect_leaks "Memory leak detection" \
    "examples/10_memory_leak.cl" \
    "2"

# ── Phase 5: Advanced features ─────────────────────────────────────────────────

echo ""
bold "Phase 5: Advanced features"
echo ""

expect_success "String indexing + iteration + lambdas + builtins" \
    "examples/11_advanced.cl" \
    "All Phase 5 tests passed."

# ── CLI ─────────────────────────────────────────────────────────────────────────

echo ""
bold "CLI"
echo ""

# Version flag.
output=$($BINARY --version 2>&1)
if echo "$output" | grep -qF "CrashLang"; then
    PASS=$((PASS + 1))
    printf "  $(green "PASS")  --version\n"
else
    FAIL=$((FAIL + 1))
    printf "  $(red "FAIL")  --version\n"
fi

# Help flag.
output=$($BINARY --help 2>&1)
if echo "$output" | grep -qF "Usage:"; then
    PASS=$((PASS + 1))
    printf "  $(green "PASS")  --help\n"
else
    FAIL=$((FAIL + 1))
    printf "  $(red "FAIL")  --help\n"
fi

# ── Summary ─────────────────────────────────────────────────────────────────────

echo ""
echo "────────────────────────────────────────────────────"
TOTAL=$((PASS + FAIL))
if [ "$FAIL" -eq 0 ]; then
    green "All $TOTAL tests passed."
    echo ""
else
    printf "%s passed, %s failed (out of %s)\n" \
        "$(green "$PASS")" "$(red "$FAIL")" "$TOTAL"
    if [ -n "$ERRORS" ]; then
        echo ""
        printf "$ERRORS"
    fi
fi
echo ""
