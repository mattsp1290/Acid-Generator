#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

echo "=== Acid Generator Test Suite ==="
echo ""

PASS=0
FAIL=0

# Test 1: Compile test vectors with C++14 (Daisy compatibility)
echo "[TEST 1] Compile Generator.hpp with C++14 (Daisy compatibility)..."
if g++ -std=c++14 -I"$ROOT_DIR/src" -o "$SCRIPT_DIR/verify_vectors_c14" "$SCRIPT_DIR/verify_vectors.cpp" 2>&1; then
    echo "  PASS: Generator.hpp compiles with C++14"
    PASS=$((PASS + 1))
    rm -f "$SCRIPT_DIR/verify_vectors_c14"
else
    echo "  FAIL: Generator.hpp does not compile with C++14"
    FAIL=$((FAIL + 1))
fi

# Test 2: Compile test vectors with C++17 (VCV Rack compatibility)
echo "[TEST 2] Compile Generator.hpp with C++17 (VCV Rack compatibility)..."
if g++ -std=c++17 -I"$ROOT_DIR/src" -o "$SCRIPT_DIR/verify_vectors_c17" "$SCRIPT_DIR/verify_vectors.cpp" 2>&1; then
    echo "  PASS: Generator.hpp compiles with C++17"
    PASS=$((PASS + 1))
else
    echo "  FAIL: Generator.hpp does not compile with C++17"
    FAIL=$((FAIL + 1))
fi

# Test 3: Run test vectors and verify output
echo "[TEST 3] Run test vector verification..."
if [ -f "$SCRIPT_DIR/verify_vectors_c17" ]; then
    OUTPUT=$("$SCRIPT_DIR/verify_vectors_c17" 2>&1)
    # Check that output contains expected metadata
    if echo "$OUTPUT" | grep -q '"generator": "Acid Pattern Generator C++ Port"'; then
        echo "  PASS: Test vectors generate valid output"
        PASS=$((PASS + 1))
    else
        echo "  FAIL: Test vector output invalid"
        FAIL=$((FAIL + 1))
    fi
    rm -f "$SCRIPT_DIR/verify_vectors_c17"
else
    echo "  SKIP: No binary to run (previous compile failed)"
    FAIL=$((FAIL + 1))
fi

# Test 4: Verify Daisy source compiles (syntax check only, no linker)
echo "[TEST 4] Syntax-check Daisy main.cpp..."
if [ -f "$ROOT_DIR/daisy/src/main.cpp" ]; then
    # We can't link against libDaisy without ARM toolchain, but we can check
    # that our code parses and non-Daisy portions are valid C++14
    # Create a stub header for syntax checking
    STUB_DIR=$(mktemp -d)
    mkdir -p "$STUB_DIR/daisy_patch_stub"

    cat > "$STUB_DIR/daisy_patch.h" << 'STUB'
#pragma once
#include <cstdint>
#include <cstddef>
namespace daisy {
    struct SaiHandle { struct Config { enum class SampleRate { SAI_48KHZ }; }; };
    struct DacHandle { enum class Channel { ONE, TWO }; };
    struct AudioHandle {
        typedef float** InputBuffer;
        typedef float** OutputBuffer;
    };
    struct MidiUartHandler {
        struct Config {};
        void Init(Config) {}
        void StartReceive() {}
        void SendMessage(uint8_t*, size_t) {}
    };
    struct System {
        static uint32_t GetNow() { return 0; }
        static void Delay(uint32_t) {}
    };
    namespace FontDef { struct FontDef { int w; int h; }; }
    static FontDef::FontDef Font_6x8 = {6, 8};
    static FontDef::FontDef Font_7x10 = {7, 10};
    struct OledDisplay {
        void Fill(bool) {}
        void SetCursor(int, int) {}
        template<typename T> void WriteString(const char*, T, bool) {}
        void DrawLine(int, int, int, int, bool) {}
        void DrawRect(int, int, int, int, bool, bool=false) {}
        void DrawCircle(int, int, int, bool) {}
        void Update() {}
    };
    struct Encoder {
        bool RisingEdge() { return false; }
        bool FallingEdge() { return false; }
        float TimeHeldMs() { return 0; }
        int Increment() { return 0; }
    };
    struct AnalogControl { float Value() { return 0; } };
    struct GateIn { bool State() { return false; } };
    struct DaisyPatch {
        enum { CTRL_1, CTRL_2, CTRL_3, CTRL_4 };
        enum { GATE_IN_1, GATE_IN_2 };
        struct { DacHandle dac_placeholder; struct { void WriteValue(DacHandle::Channel, uint16_t) {} } dac; } seed;
        GateIn gate_input[2];
        struct {} gate_output;
        OledDisplay display;
        Encoder encoder;
        void Init() {}
        void SetAudioBlockSize(size_t) {}
        void SetAudioSampleRate(SaiHandle::Config::SampleRate) {}
        void StartAdc() {}
        void StartAudio(void(*)(AudioHandle::InputBuffer, AudioHandle::OutputBuffer, size_t)) {}
        void ProcessAllControls() {}
        float GetKnobValue(int) { return 0; }
        float AudioSampleRate() { return 48000; }
    };
}
inline void dsy_gpio_write(void*, bool) {}
STUB
    cat > "$STUB_DIR/daisysp.h" << 'STUB2'
#pragma once
namespace daisysp {}
STUB2

    if g++ -std=c++14 -fsyntax-only -I"$ROOT_DIR/src" -I"$STUB_DIR" "$ROOT_DIR/daisy/src/main.cpp" 2>&1; then
        echo "  PASS: Daisy main.cpp syntax check passed"
        PASS=$((PASS + 1))
    else
        echo "  FAIL: Daisy main.cpp has syntax errors"
        FAIL=$((FAIL + 1))
    fi
    rm -rf "$STUB_DIR"
else
    echo "  SKIP: daisy/src/main.cpp not found"
    FAIL=$((FAIL + 1))
fi

# Test 5: Verify Daisy build system files exist
echo "[TEST 5] Verify Daisy project structure..."
MISSING=""
[ ! -f "$ROOT_DIR/daisy/Makefile" ] && MISSING="$MISSING daisy/Makefile"
[ ! -f "$ROOT_DIR/daisy/src/main.cpp" ] && MISSING="$MISSING daisy/src/main.cpp"
[ ! -d "$ROOT_DIR/daisy/lib/libDaisy" ] && MISSING="$MISSING daisy/lib/libDaisy"
[ ! -d "$ROOT_DIR/daisy/lib/DaisySP" ] && MISSING="$MISSING daisy/lib/DaisySP"

if [ -z "$MISSING" ]; then
    echo "  PASS: All Daisy project files present"
    PASS=$((PASS + 1))
else
    echo "  FAIL: Missing files:$MISSING"
    FAIL=$((FAIL + 1))
fi

# Test 6: Compile + run Generator.hpp unit tests with C++14
echo "[TEST 6] Compile + run test_generator.cpp with C++14..."
if g++ -std=c++14 -I"$ROOT_DIR/src" -o "$SCRIPT_DIR/test_generator_c14" "$SCRIPT_DIR/test_generator.cpp" 2>&1; then
    OUTPUT=$("$SCRIPT_DIR/test_generator_c14" 2>&1)
    EXIT_CODE=$?
    echo "$OUTPUT" | tail -3
    if [ $EXIT_CODE -eq 0 ]; then
        echo "  PASS: test_generator (C++14) all assertions passed"
        PASS=$((PASS + 1))
    else
        echo "  FAIL: test_generator (C++14) had failures"
        FAIL=$((FAIL + 1))
    fi
    rm -f "$SCRIPT_DIR/test_generator_c14"
else
    echo "  FAIL: test_generator.cpp does not compile with C++14"
    FAIL=$((FAIL + 1))
fi

# Test 7: Compile + run Generator.hpp unit tests with C++17
echo "[TEST 7] Compile + run test_generator.cpp with C++17..."
if g++ -std=c++17 -I"$ROOT_DIR/src" -o "$SCRIPT_DIR/test_generator_c17" "$SCRIPT_DIR/test_generator.cpp" 2>&1; then
    OUTPUT=$("$SCRIPT_DIR/test_generator_c17" 2>&1)
    EXIT_CODE=$?
    echo "$OUTPUT" | tail -3
    if [ $EXIT_CODE -eq 0 ]; then
        echo "  PASS: test_generator (C++17) all assertions passed"
        PASS=$((PASS + 1))
    else
        echo "  FAIL: test_generator (C++17) had failures"
        FAIL=$((FAIL + 1))
    fi
    rm -f "$SCRIPT_DIR/test_generator_c17"
else
    echo "  FAIL: test_generator.cpp does not compile with C++17"
    FAIL=$((FAIL + 1))
fi

# Test 8: Compile + run Daisy logic unit tests with C++14
echo "[TEST 8] Compile + run test_daisy_logic.cpp with C++14..."
if g++ -std=c++14 -I"$ROOT_DIR/src" -I"$ROOT_DIR/daisy/src" -o "$SCRIPT_DIR/test_daisy_logic" "$SCRIPT_DIR/test_daisy_logic.cpp" 2>&1; then
    OUTPUT=$("$SCRIPT_DIR/test_daisy_logic" 2>&1)
    EXIT_CODE=$?
    echo "$OUTPUT" | tail -3
    if [ $EXIT_CODE -eq 0 ]; then
        echo "  PASS: test_daisy_logic all assertions passed"
        PASS=$((PASS + 1))
    else
        echo "  FAIL: test_daisy_logic had failures"
        FAIL=$((FAIL + 1))
    fi
    rm -f "$SCRIPT_DIR/test_daisy_logic"
else
    echo "  FAIL: test_daisy_logic.cpp does not compile with C++14"
    FAIL=$((FAIL + 1))
fi

echo ""
echo "=== Results: $PASS passed, $FAIL failed ==="

if [ $FAIL -gt 0 ]; then
    exit 1
fi

echo "All tests passed!"
exit 0
