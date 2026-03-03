/**
 * Unit tests for daisy_logic.hpp
 *
 * Compile:
 *   g++ -std=c++14 -I../src -I../daisy/src -o test_daisy_logic test_daisy_logic.cpp
 */

#include <cstdio>
#include <cstdlib>
#include <cmath>
#include "daisy_logic.hpp"

using namespace AcidGenerator;

//-----------------------------------------------------------------------------
// Minimal test harness
//-----------------------------------------------------------------------------

static int g_pass = 0;
static int g_fail = 0;

#define TEST_ASSERT(cond, msg) do { \
    if (!(cond)) { \
        printf("  FAIL: %s (line %d)\n", msg, __LINE__); \
        g_fail++; \
    } else { \
        g_pass++; \
    } \
} while(0)

#define TEST_ASSERT_EQ(a, b, msg) do { \
    if ((a) != (b)) { \
        printf("  FAIL: %s — expected %d, got %d (line %d)\n", msg, (int)(b), (int)(a), __LINE__); \
        g_fail++; \
    } else { \
        g_pass++; \
    } \
} while(0)

#define TEST_ASSERT_NEAR(a, b, eps, msg) do { \
    if (std::fabs((a) - (b)) > (eps)) { \
        printf("  FAIL: %s — expected %.6f, got %.6f (line %d)\n", msg, (double)(b), (double)(a), __LINE__); \
        g_fail++; \
    } else { \
        g_pass++; \
    } \
} while(0)

#define TEST_GROUP(name) printf("\n--- %s ---\n", name)

//-----------------------------------------------------------------------------
// MIDI Message Tests (5)
//-----------------------------------------------------------------------------

void test_midi_messages() {
    TEST_GROUP("MIDI Messages");

    // 1. NoteOn normal: velocity 60
    {
        MidiMsg msg = makeNoteOn(60, false, 0);
        TEST_ASSERT_EQ(msg.data[0], 0x90, "NoteOn status byte ch 0");
        TEST_ASSERT_EQ(msg.data[1], 60, "NoteOn note 60");
        TEST_ASSERT_EQ(msg.data[2], 60, "NoteOn normal velocity 60");
    }

    // 2. NoteOn accent: velocity 127
    {
        MidiMsg msg = makeNoteOn(60, true, 0);
        TEST_ASSERT_EQ(msg.data[2], 127, "NoteOn accent velocity 127");
    }

    // 3. Channel encoding
    {
        MidiMsg msg = makeNoteOn(60, false, 5);
        TEST_ASSERT_EQ(msg.data[0], 0x95, "NoteOn channel 5 = 0x95");
    }

    // 4. NoteOff
    {
        MidiMsg msg = makeNoteOff(60, 0);
        TEST_ASSERT_EQ(msg.data[0], 0x80, "NoteOff status byte ch 0");
        TEST_ASSERT_EQ(msg.data[1], 60, "NoteOff note 60");
        TEST_ASSERT_EQ(msg.data[2], 0, "NoteOff velocity 0");
    }

    // 5. Note clamping to 7 bits (0x7F = 127)
    {
        MidiMsg msg = makeNoteOn(200, false, 0);
        TEST_ASSERT_EQ(msg.data[1], 200 & 0x7F, "note clamped to 7 bits");
        TEST_ASSERT(msg.data[1] <= 127, "note <= 127");
    }
}

//-----------------------------------------------------------------------------
// Pitch Calculation Tests (3)
//-----------------------------------------------------------------------------

void test_pitch_calculation() {
    TEST_GROUP("Pitch Calculation");

    // 1. MIDI note from scale+root
    {
        SequenceStep step = {0, 0, false, false};
        int midiNote = calculateMidiNote(step, Scale::MINOR, 0, 0);
        // getNoteInScale(0, MINOR, 0, 0) = 0, +48 = 48
        TEST_ASSERT_EQ(midiNote, 48, "C Minor root at octave 0 = MIDI 48");
    }

    // 2. Clamping 0-127
    {
        // Very high: octave +10 should clamp to 127
        SequenceStep high = {6, 0, false, false};
        int midiHigh = calculateMidiNote(high, Scale::MAJOR, 0, 10);
        TEST_ASSERT(midiHigh <= 127, "high note clamped to 127");

        // Very low: octave -10 should clamp to 0
        SequenceStep low = {0, -1, false, false};
        int midiLow = calculateMidiNote(low, Scale::MAJOR, 0, -5);
        TEST_ASSERT(midiLow >= 0, "low note clamped to 0");
    }

    // 3. Pitch voltage: MIDI 36→0V, 48→1V, 60→2V
    {
        TEST_ASSERT_NEAR(calculatePitchVoltage(36), 0.0f, 0.001f, "MIDI 36 = 0V");
        TEST_ASSERT_NEAR(calculatePitchVoltage(48), 1.0f, 0.001f, "MIDI 48 = 1V");
        TEST_ASSERT_NEAR(calculatePitchVoltage(60), 2.0f, 0.001f, "MIDI 60 = 2V");
    }
}

//-----------------------------------------------------------------------------
// DAC Conversion Tests (3)
//-----------------------------------------------------------------------------

void test_dac_conversion() {
    TEST_GROUP("DAC Conversion");

    // 1. 0V → 0
    TEST_ASSERT_EQ(pitchToDac(0.0f), 0, "0V = DAC 0");

    // 2. 5V → 4095
    TEST_ASSERT_EQ(pitchToDac(5.0f), 4095, "5V = DAC 4095");

    // 3. 2.5V → ~2048
    {
        uint16_t dac = pitchToDac(2.5f);
        TEST_ASSERT(dac >= 2046 && dac <= 2048, "2.5V ~ DAC 2048");
    }

    // 4. Negative clamps to 0
    TEST_ASSERT_EQ(pitchToDac(-1.0f), 0, "negative voltage clamps to 0");
}

//-----------------------------------------------------------------------------
// Gate / Slide Timing Tests (3)
//-----------------------------------------------------------------------------

void test_gate_slide_timing() {
    TEST_GROUP("Gate / Slide Timing");

    // 1. Normal gate = 20ms
    {
        float dur = calculateGateDuration(false, 0.125f);
        TEST_ASSERT_NEAR(dur, 0.020f, 0.0001f, "normal gate = 20ms");
    }

    // 2. Slide gate = 110% of clock period
    {
        float clockPeriod = 0.125f; // 120 BPM 16ths
        float dur = calculateGateDuration(true, clockPeriod);
        TEST_ASSERT_NEAR(dur, 0.1375f, 0.0001f, "slide gate = 110% of clock");
    }

    // 3. Slide rate calculation
    {
        float rate = calculateSlideRate(1.0f, 2.0f);
        // (2.0 - 1.0) / 0.050 = 20.0 V/s
        TEST_ASSERT_NEAR(rate, 20.0f, 0.001f, "slide rate = 20 V/s");

        float rateDown = calculateSlideRate(3.0f, 1.0f);
        // (1.0 - 3.0) / 0.050 = -40.0 V/s
        TEST_ASSERT_NEAR(rateDown, -40.0f, 0.001f, "downward slide rate = -40 V/s");
    }
}

//-----------------------------------------------------------------------------
// Main
//-----------------------------------------------------------------------------

int main() {
    printf("=== Daisy Logic Unit Tests ===\n");

    test_midi_messages();
    test_pitch_calculation();
    test_dac_conversion();
    test_gate_slide_timing();

    printf("\n=== Results: %d passed, %d failed ===\n", g_pass, g_fail);

    if (g_fail > 0) {
        printf("FAILED\n");
        return 1;
    }
    printf("ALL PASSED\n");
    return 0;
}
