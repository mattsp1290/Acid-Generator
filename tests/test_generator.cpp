/**
 * Unit tests for Generator.hpp
 *
 * Compile:
 *   g++ -std=c++14 -I../src -o test_generator test_generator.cpp
 *   g++ -std=c++17 -I../src -o test_generator test_generator.cpp
 */

#include <cstdio>
#include <cstdlib>
#include <cmath>
#include "Generator.hpp"

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
// SFC32 RNG Tests (6)
//-----------------------------------------------------------------------------

void test_sfc32() {
    TEST_GROUP("SFC32 RNG");

    // 1. Known sequence for seed 123
    {
        SFC32 rng(123);
        float vals[5];
        for (int i = 0; i < 5; i++) vals[i] = rng.next();

        TEST_ASSERT_NEAR(vals[0], 0.0f, 0.001f, "seed 123 val[0] ~ 0.0");
        TEST_ASSERT_NEAR(vals[1], 0.0f, 0.001f, "seed 123 val[1] ~ 0.0");
        TEST_ASSERT_NEAR(vals[2], 0.540528f, 0.001f, "seed 123 val[2] ~ 0.540528");
        TEST_ASSERT_NEAR(vals[3], 0.165874f, 0.001f, "seed 123 val[3] ~ 0.165874");
        TEST_ASSERT_NEAR(vals[4], 0.945160f, 0.001f, "seed 123 val[4] ~ 0.945160");
    }

    // 2. Determinism: two instances with same seed produce same sequence
    {
        SFC32 rng1(42);
        SFC32 rng2(42);
        bool match = true;
        for (int i = 0; i < 100; i++) {
            if (rng1.next() != rng2.next()) { match = false; break; }
        }
        TEST_ASSERT(match, "determinism: same seed = same sequence");
    }

    // 3. Different seeds diverge
    {
        SFC32 rng1(100);
        SFC32 rng2(200);
        bool allSame = true;
        for (int i = 0; i < 10; i++) {
            if (rng1.next() != rng2.next()) { allSame = false; break; }
        }
        TEST_ASSERT(!allSame, "different seeds diverge");
    }

    // 4. Range [0, 1)
    {
        SFC32 rng(999);
        bool inRange = true;
        for (int i = 0; i < 10000; i++) {
            float v = rng.next();
            if (v < 0.0f || v >= 1.0f) { inRange = false; break; }
        }
        TEST_ASSERT(inRange, "all values in [0, 1)");
    }

    // 5. 4-arg constructor
    {
        SFC32 rng(1, 2, 3, 4);
        TEST_ASSERT_NEAR(rng.next(), 0.0f, 0.001f, "4-arg ctor val[0] ~ 0.0");
        rng.next(); // skip val[1]
        TEST_ASSERT_NEAR(rng.next(), 0.013184f, 0.001f, "4-arg ctor val[2] ~ 0.013184");
    }

    // 6. randomInt bounds
    {
        SFC32 rng(42);
        bool inBounds = true;
        for (int i = 0; i < 1000; i++) {
            int v = rng.randomInt(3, 7);
            if (v < 3 || v > 7) { inBounds = false; break; }
        }
        TEST_ASSERT(inBounds, "randomInt(3,7) all in [3,7]");
    }
}

//-----------------------------------------------------------------------------
// Scale Data Tests (4)
//-----------------------------------------------------------------------------

void test_scale_data() {
    TEST_GROUP("Scale Data");

    // 1. Scale count
    TEST_ASSERT_EQ(static_cast<int>(Scale::NUM_SCALES), 24, "NUM_SCALES == 24");

    // 2. All scales start at 0
    {
        bool allStartZero = true;
        for (int s = 0; s < static_cast<int>(Scale::NUM_SCALES); s++) {
            if (SCALES[s].intervals[0] != 0) { allStartZero = false; break; }
        }
        TEST_ASSERT(allStartZero, "all scales start at interval 0");
    }

    // 3. Intervals ascending within each scale
    {
        bool ascending = true;
        for (int s = 0; s < static_cast<int>(Scale::NUM_SCALES); s++) {
            for (int i = 1; i < SCALES[s].length; i++) {
                if (SCALES[s].intervals[i] <= SCALES[s].intervals[i - 1]) {
                    ascending = false;
                    break;
                }
            }
            if (!ascending) break;
        }
        TEST_ASSERT(ascending, "all scale intervals strictly ascending");
    }

    // 4. Spot-check lengths
    TEST_ASSERT_EQ(SCALES[static_cast<int>(Scale::PENTATONIC_MINOR)].length, 5,
                   "pentatonic minor length == 5");
    TEST_ASSERT_EQ(SCALES[static_cast<int>(Scale::CHROMATIC)].length, 12,
                   "chromatic length == 12");
    TEST_ASSERT_EQ(SCALES[static_cast<int>(Scale::MAJOR)].length, 7,
                   "major length == 7");
}

//-----------------------------------------------------------------------------
// getNoteInScale Tests (7)
//-----------------------------------------------------------------------------

void test_get_note_in_scale() {
    TEST_GROUP("getNoteInScale");

    // 1. Root C Major: degree 0 = 0 (C)
    TEST_ASSERT_EQ(getNoteInScale(0, Scale::MAJOR, 0, 0), 0,
                   "C Major degree 0 = 0");

    // 2. Octave wrapping: degree 7 in Major wraps to next octave
    TEST_ASSERT_EQ(getNoteInScale(7, Scale::MAJOR, 0, 0), 12,
                   "Major degree 7 wraps to 12");

    // 3. Root offset: root=2 (D), degree 0
    TEST_ASSERT_EQ(getNoteInScale(0, Scale::MAJOR, 2, 0), 2,
                   "D Major degree 0 = 2");

    // 4. Octave offset
    TEST_ASSERT_EQ(getNoteInScale(0, Scale::MAJOR, 0, 1), 12,
                   "C Major degree 0, octave 1 = 12");

    // 5. Rest input
    TEST_ASSERT_EQ(getNoteInScale(-1, Scale::MAJOR, 0, 0), -1,
                   "rest input returns -1");

    // 6. Pentatonic minor wrap: degree 5 (len=5) wraps to octave
    TEST_ASSERT_EQ(getNoteInScale(5, Scale::PENTATONIC_MINOR, 0, 0), 12,
                   "pentatonic minor degree 5 = 12");

    // 7. Chromatic wrap: degree 12 (len=12) wraps to octave
    TEST_ASSERT_EQ(getNoteInScale(12, Scale::CHROMATIC, 0, 0), 12,
                   "chromatic degree 12 = 12");
}

//-----------------------------------------------------------------------------
// stepToVoltage Tests (4)
//-----------------------------------------------------------------------------

void test_step_to_voltage() {
    TEST_GROUP("stepToVoltage");

    // 1. Rest returns 0
    {
        SequenceStep rest = {-1, 0, false, false};
        TEST_ASSERT_NEAR(stepToVoltage(rest, Scale::MAJOR, 0, 0), 0.0f, 0.0001f,
                         "rest returns 0V");
    }

    // 2. Middle C (MIDI 60) at baseOctave=5: degree 0, C Major, octave=5 → MIDI 60
    //    voltage = (60-60)/12 = 0.0
    {
        SequenceStep s = {0, 0, false, false};
        TEST_ASSERT_NEAR(stepToVoltage(s, Scale::MAJOR, 0, 5), 0.0f, 0.0001f,
                         "middle C = 0V");
    }

    // 3. One octave up: degree 0, octave offset +1 from middle C → MIDI 72
    //    voltage = (72-60)/12 = 1.0
    {
        SequenceStep s = {0, 1, false, false};
        TEST_ASSERT_NEAR(stepToVoltage(s, Scale::MAJOR, 0, 5), 1.0f, 0.0001f,
                         "one octave up = 1V");
    }

    // 4. Semitone resolution: degree 1 in C Major = interval 2 → MIDI 62
    //    voltage = (62-60)/12 = 0.16667
    {
        SequenceStep s = {1, 0, false, false};
        TEST_ASSERT_NEAR(stepToVoltage(s, Scale::MAJOR, 0, 5), 2.0f / 12.0f, 0.0001f,
                         "semitone resolution");
    }
}

//-----------------------------------------------------------------------------
// MasterPattern Method Tests (11)
//-----------------------------------------------------------------------------

void test_master_pattern_methods() {
    TEST_GROUP("MasterPattern Methods");

    // Setup: use generateMaster with seed 1001 for known values
    MasterPattern mp;
    generateMaster(1001, mp);

    // --- isStepActive ---

    // 1. density=0 → no steps active
    {
        bool anyActive = false;
        for (int i = 0; i < BAR_LEN; i++) {
            if (mp.isStepActive(i, 0.0f)) { anyActive = true; break; }
        }
        TEST_ASSERT(!anyActive, "density=0: no steps active");
    }

    // 2. density=100 → all steps active
    {
        bool allActive = true;
        for (int i = 0; i < BAR_LEN; i++) {
            if (!mp.isStepActive(i, 100.0f)) { allActive = false; break; }
        }
        TEST_ASSERT(allActive, "density=100: all steps active");
    }

    // 3. Partial density: first in activation order should be active
    {
        // At ~6% density, round(16*6/100) = round(0.96) = 1, so only 1 step active
        int firstStep = mp.barActivationOrder[0];
        TEST_ASSERT(mp.isStepActive(firstStep, 6.25f),
                    "partial density: first activated step is active");
    }

    // 4. Bar wrapping: step 16 wraps to barPos 0
    {
        bool step0Active = mp.isStepActive(0, 100.0f);
        bool step16Active = mp.isStepActive(16, 100.0f);
        TEST_ASSERT(step0Active == step16Active,
                    "step 16 wraps same as step 0");
    }

    // --- getScaleDegree ---

    // 5. spread=min (~14% → 1 note): always returns root
    {
        int degree = mp.getScaleDegree(0, 14.0f, true);
        TEST_ASSERT_EQ(degree, mp.scalePriorityOrder[0],
                       "spread min: returns root");
    }

    // 6. spread=100 (all 7 notes): returns the step's note from full pool
    {
        int degree = mp.getScaleDegree(0, 100.0f, true);
        TEST_ASSERT_EQ(degree, mp.scalePriorityOrder[mp.steps[0].notePoolIndex],
                       "spread max: returns step's pool note");
    }

    // 7. Quantize vs rest: step with high notePoolIndex at low spread
    {
        // Find a step with notePoolIndex > 0
        int testStep = -1;
        for (int i = 0; i < MAX_STEPS; i++) {
            if (mp.steps[i].notePoolIndex >= 2) { testStep = i; break; }
        }
        if (testStep >= 0) {
            // quantizeToPool=true: should return root
            int qDegree = mp.getScaleDegree(testStep, 14.0f, true);
            TEST_ASSERT_EQ(qDegree, mp.scalePriorityOrder[0],
                           "quantize to pool returns root");

            // quantizeToPool=false: should return -1 (rest)
            int rDegree = mp.getScaleDegree(testStep, 14.0f, false);
            TEST_ASSERT_EQ(rDegree, -1,
                           "no quantize returns -1");
        } else {
            // Unlikely but handle gracefully
            TEST_ASSERT(false, "no step with notePoolIndex >= 2 found");
            TEST_ASSERT(false, "skipped no-quantize test");
        }
    }

    // --- getStep ---

    // 8. Muted step → rest
    {
        mp.muted[0] = true;
        SequenceStep s = mp.getStep(0, 100.0f, 100.0f, 100.0f, 100.0f);
        TEST_ASSERT(s.isRest(), "muted step is rest");
        mp.muted[0] = false;  // restore
    }

    // 9. Accent threshold: accentProb < accentsDensity/100 → accent true
    {
        // Step 0 has accentProb ≈ 0.339109
        // With accentsDensity=100 (threshold 1.0), should be accent
        SequenceStep s = mp.getStep(0, 100.0f, 100.0f, 100.0f, 0.0f);
        TEST_ASSERT(s.accent, "accent when accentsDensity=100");

        // With accentsDensity=0 (threshold 0.0), should NOT be accent
        SequenceStep s2 = mp.getStep(0, 100.0f, 100.0f, 0.0f, 0.0f);
        TEST_ASSERT(!s2.accent, "no accent when accentsDensity=0");
    }

    // 10. Slide threshold: similar logic
    {
        // Step 0 has slideProb ≈ 0.554531
        SequenceStep s = mp.getStep(0, 100.0f, 100.0f, 0.0f, 100.0f);
        TEST_ASSERT(s.slide, "slide when slidesDensity=100");

        SequenceStep s2 = mp.getStep(0, 100.0f, 100.0f, 0.0f, 0.0f);
        TEST_ASSERT(!s2.slide, "no slide when slidesDensity=0");
    }

    // --- findNotePoolIndex ---

    // 11. Find known degree and unknown degree
    {
        // scalePriorityOrder[0] is always 0 (root) for seed 1001
        int idx = mp.findNotePoolIndex(0);
        TEST_ASSERT_EQ(idx, 0, "findNotePoolIndex(root) == 0");

        // Find a degree that's in position 1
        int deg1 = mp.scalePriorityOrder[1];
        int idx1 = mp.findNotePoolIndex(deg1);
        TEST_ASSERT_EQ(idx1, 1, "findNotePoolIndex returns correct index");

        // Value not in pool (99) → returns 0
        int missing = mp.findNotePoolIndex(99);
        TEST_ASSERT_EQ(missing, 0, "findNotePoolIndex(99) defaults to 0");
    }
}

//-----------------------------------------------------------------------------
// generate() End-to-End Tests (5)
//-----------------------------------------------------------------------------

void test_generate_e2e() {
    TEST_GROUP("generate() End-to-End");

    // 1. Deterministic output
    {
        GeneratorParams p;
        p.seed = 1001; p.density = 100; p.spread = 100;
        p.accentsDensity = 50; p.slidesDensity = 50; p.patternLength = 16;

        Pattern pat1, pat2;
        generate(p, pat1);
        generate(p, pat2);

        bool match = true;
        for (int i = 0; i < MAX_STEPS; i++) {
            if (pat1.steps[i].note != pat2.steps[i].note ||
                pat1.steps[i].octave != pat2.steps[i].octave ||
                pat1.steps[i].accent != pat2.steps[i].accent ||
                pat1.steps[i].slide != pat2.steps[i].slide) {
                match = false; break;
            }
        }
        TEST_ASSERT(match, "deterministic: same params = same pattern");
    }

    // 2. Seed sensitivity
    {
        GeneratorParams p1, p2;
        p1.seed = 1001; p1.density = 100; p1.spread = 100;
        p1.accentsDensity = 50; p1.slidesDensity = 50; p1.patternLength = 16;
        p2 = p1; p2.seed = 1002;

        Pattern pat1, pat2;
        generate(p1, pat1);
        generate(p2, pat2);

        bool differ = false;
        for (int i = 0; i < MAX_STEPS; i++) {
            if (pat1.steps[i].note != pat2.steps[i].note) { differ = true; break; }
        }
        TEST_ASSERT(differ, "different seeds produce different patterns");
    }

    // 3. Density=0 → all rests
    {
        GeneratorParams p;
        p.seed = 1001; p.density = 0; p.spread = 100;
        p.accentsDensity = 50; p.slidesDensity = 50; p.patternLength = 16;

        Pattern pat;
        generate(p, pat);

        bool allRests = true;
        for (int i = 0; i < MAX_STEPS; i++) {
            if (!pat.steps[i].isRest()) { allRests = false; break; }
        }
        TEST_ASSERT(allRests, "density=0: all steps are rests");
    }

    // 4. Density=100 → no rests
    {
        GeneratorParams p;
        p.seed = 1001; p.density = 100; p.spread = 100;
        p.accentsDensity = 50; p.slidesDensity = 50; p.patternLength = 16;

        Pattern pat;
        generate(p, pat);

        bool noRests = true;
        for (int i = 0; i < MAX_STEPS; i++) {
            if (pat.steps[i].isRest()) { noRests = false; break; }
        }
        TEST_ASSERT(noRests, "density=100: no rests");
    }

    // 5. Known vector: scenario 1 first 8 steps
    {
        GeneratorParams p;
        p.seed = 1001; p.density = 100; p.spread = 100;
        p.accentsDensity = 50; p.slidesDensity = 50; p.patternLength = 16;

        Pattern pat;
        generate(p, pat);

        // Expected: from reference binary output
        struct { int n; int o; bool a; bool s; } expected[8] = {
            {0,  0, true,  false},
            {3,  0, true,  false},
            {1, -1, true,  false},
            {4,  0, false, true },
            {0,  1, false, false},
            {2, -1, false, false},
            {3,  1, true,  true },
            {2,  1, true,  false},
        };

        bool match = true;
        for (int i = 0; i < 8; i++) {
            const auto& s = pat.steps[i];
            const auto& e = expected[i];
            if (s.note != e.n || s.octave != e.o || s.accent != e.a || s.slide != e.s) {
                printf("    step %d mismatch: got n=%d o=%d a=%d s=%d, expected n=%d o=%d a=%d s=%d\n",
                       i, s.note, s.octave, s.accent, s.slide, e.n, e.o, e.a, e.s);
                match = false;
            }
        }
        TEST_ASSERT(match, "scenario 1 first 8 steps match reference");
    }
}

//-----------------------------------------------------------------------------
// generateMaster() End-to-End Tests (4)
//-----------------------------------------------------------------------------

void test_generate_master_e2e() {
    TEST_GROUP("generateMaster() End-to-End");

    // 1. Deterministic
    {
        MasterPattern mp1, mp2;
        generateMaster(1001, mp1);
        generateMaster(1001, mp2);

        bool match = true;
        for (int i = 0; i < SCALE_SIZE; i++) {
            if (mp1.scalePriorityOrder[i] != mp2.scalePriorityOrder[i]) {
                match = false; break;
            }
        }
        for (int i = 0; i < BAR_LEN && match; i++) {
            if (mp1.barActivationOrder[i] != mp2.barActivationOrder[i]) {
                match = false; break;
            }
        }
        for (int i = 0; i < MAX_STEPS && match; i++) {
            if (mp1.steps[i].notePoolIndex != mp2.steps[i].notePoolIndex ||
                mp1.steps[i].octave != mp2.steps[i].octave) {
                match = false; break;
            }
        }
        TEST_ASSERT(match, "deterministic: same seed = same master pattern");
    }

    // 2. Root always first in priority (check across many seeds)
    {
        bool rootFirst = true;
        for (uint32_t s = 1; s <= 50; s++) {
            MasterPattern mp;
            generateMaster(s, mp);
            if (mp.scalePriorityOrder[0] != 0) { rootFirst = false; break; }
        }
        TEST_ASSERT(rootFirst, "root always first in scalePriorityOrder");
    }

    // 3. Beat-one is among first 4 activated positions (across many seeds)
    {
        bool earlyActivation = true;
        for (uint32_t s = 1; s <= 50; s++) {
            MasterPattern mp;
            generateMaster(s, mp);
            bool found = false;
            for (int i = 0; i < 4; i++) {
                if (mp.barActivationOrder[i] == 0) { found = true; break; }
            }
            if (!found) { earlyActivation = false; break; }
        }
        TEST_ASSERT(earlyActivation, "beat-one activates within first 4 positions");
    }

    // 4. Step data ranges valid
    {
        MasterPattern mp;
        generateMaster(1001, mp);

        bool valid = true;
        for (int i = 0; i < MAX_STEPS; i++) {
            const auto& s = mp.steps[i];
            if (s.notePoolIndex < 0 || s.notePoolIndex >= SCALE_SIZE) { valid = false; break; }
            if (s.octave < -1 || s.octave > 1) { valid = false; break; }
            if (s.accentProb < 0.0f || s.accentProb >= 1.0f) { valid = false; break; }
            if (s.slideProb < 0.0f || s.slideProb >= 1.0f) { valid = false; break; }
        }
        TEST_ASSERT(valid, "all step data in valid ranges");
    }
}

//-----------------------------------------------------------------------------
// Main
//-----------------------------------------------------------------------------

int main() {
    printf("=== Generator.hpp Unit Tests ===\n");

    test_sfc32();
    test_scale_data();
    test_get_note_in_scale();
    test_step_to_voltage();
    test_master_pattern_methods();
    test_generate_e2e();
    test_generate_master_e2e();

    printf("\n=== Results: %d passed, %d failed ===\n", g_pass, g_fail);

    if (g_fail > 0) {
        printf("FAILED\n");
        return 1;
    }
    printf("ALL PASSED\n");
    return 0;
}
