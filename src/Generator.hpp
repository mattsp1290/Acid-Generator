#pragma once

#include <cstdint>
#include <cmath>
#include <algorithm>

namespace AcidGenerator {

//-----------------------------------------------------------------------------
// Constants
//-----------------------------------------------------------------------------

constexpr int MAX_STEPS = 64;
constexpr int BAR_LEN = 16;
constexpr int SCALE_SIZE = 7;  // Standard scale size for weighting logic

//-----------------------------------------------------------------------------
// SFC32 - Small Fast Chaotic 32-bit PRNG
//-----------------------------------------------------------------------------
// Ported from JavaScript. Produces identical sequences given the same seed.
//
// JavaScript semantics:
//   >>> is unsigned right shift (use uint32_t in C++)
//   | 0 truncates to signed 32-bit (use explicit uint32_t cast)
//   / 4294967296 converts to float in [0, 1)
//
struct SFC32 {
    uint32_t a, b, c, d;

    SFC32(uint32_t seed) {
        // Match JS: sfc32(seed, seed, seed, seed)
        a = seed;
        b = seed;
        c = seed;
        d = seed;
    }

    SFC32(uint32_t _a, uint32_t _b, uint32_t _c, uint32_t _d)
        : a(_a), b(_b), c(_c), d(_d) {}

    // Returns a float in [0, 1) - matches JS output exactly
    float next() {
        // a >>>= 0; b >>>= 0; c >>>= 0; d >>>= 0; (no-op for uint32_t)

        // let t = (a + b) | 0;
        uint32_t t = a + b;

        // a = b ^ (b >>> 9);
        a = b ^ (b >> 9);

        // b = (c + (c << 3)) | 0;
        b = c + (c << 3);

        // c = (c << 21) | (c >>> 11);
        c = (c << 21) | (c >> 11);

        // d = (d + 1) | 0;
        d = d + 1;

        // t = (t + d) | 0;
        t = t + d;

        // c = (c + t) | 0;
        c = c + t;

        // return (t >>> 0) / 4294967296;
        return static_cast<float>(t) / 4294967296.0f;
    }

    // Convenience: random int in [min, max] inclusive
    int randomInt(int min, int max) {
        return static_cast<int>(std::floor(next() * (max - min + 1))) + min;
    }
};

//-----------------------------------------------------------------------------
// Scale Definitions
//-----------------------------------------------------------------------------

enum class Scale {
    MAJOR,
    MINOR,
    DORIAN,
    MIXOLYDIAN,
    LYDIAN,
    PHRYGIAN,
    LOCRIAN,
    HARMONIC_MINOR,
    HARMONIC_MAJOR,
    DORIAN_NR_4,
    PHRYGIAN_DOMINANT,
    MELODIC_MINOR,
    LYDIAN_AUGMENTED,
    LYDIAN_DOMINANT,
    HUNGARIAN_MINOR,
    SUPER_LOCRIAN,
    SPANISH,
    BHAIRAV,
    PENTATONIC_MINOR,
    PENTATONIC_MAJOR,
    BLUES_MINOR,
    WHOLE_TONE,
    CHROMATIC,
    JAPANESE_IN_SEN,
    NUM_SCALES
};

// Scale intervals stored as fixed arrays
// Max scale length is 12 (chromatic), use -1 as terminator
struct ScaleData {
    int intervals[12];
    int length;
};

// Compile-time scale definitions - no heap allocation
// Use static constexpr for C++14 compatibility (inline variables are C++17)
static constexpr ScaleData SCALES[] = {
    // MAJOR
    {{0, 2, 4, 5, 7, 9, 11, -1, -1, -1, -1, -1}, 7},
    // MINOR
    {{0, 2, 3, 5, 7, 8, 10, -1, -1, -1, -1, -1}, 7},
    // DORIAN
    {{0, 2, 3, 5, 7, 9, 10, -1, -1, -1, -1, -1}, 7},
    // MIXOLYDIAN
    {{0, 2, 4, 5, 7, 9, 10, -1, -1, -1, -1, -1}, 7},
    // LYDIAN
    {{0, 2, 4, 6, 7, 9, 11, -1, -1, -1, -1, -1}, 7},
    // PHRYGIAN
    {{0, 1, 3, 5, 7, 8, 10, -1, -1, -1, -1, -1}, 7},
    // LOCRIAN
    {{0, 1, 3, 5, 6, 8, 10, -1, -1, -1, -1, -1}, 7},
    // HARMONIC_MINOR
    {{0, 2, 3, 5, 7, 8, 11, -1, -1, -1, -1, -1}, 7},
    // HARMONIC_MAJOR
    {{0, 2, 4, 5, 7, 8, 11, -1, -1, -1, -1, -1}, 7},
    // DORIAN_NR_4
    {{0, 2, 3, 6, 7, 9, 10, -1, -1, -1, -1, -1}, 7},
    // PHRYGIAN_DOMINANT
    {{0, 1, 4, 5, 7, 8, 10, -1, -1, -1, -1, -1}, 7},
    // MELODIC_MINOR
    {{0, 2, 3, 5, 7, 9, 11, -1, -1, -1, -1, -1}, 7},
    // LYDIAN_AUGMENTED
    {{0, 2, 4, 6, 8, 9, 11, -1, -1, -1, -1, -1}, 7},
    // LYDIAN_DOMINANT
    {{0, 2, 4, 6, 7, 9, 10, -1, -1, -1, -1, -1}, 7},
    // HUNGARIAN_MINOR
    {{0, 2, 3, 6, 7, 8, 11, -1, -1, -1, -1, -1}, 7},
    // SUPER_LOCRIAN
    {{0, 1, 3, 4, 6, 8, 10, -1, -1, -1, -1, -1}, 7},
    // SPANISH
    {{0, 1, 4, 5, 7, 9, 10, -1, -1, -1, -1, -1}, 7},
    // BHAIRAV
    {{0, 1, 4, 5, 7, 8, 11, -1, -1, -1, -1, -1}, 7},
    // PENTATONIC_MINOR
    {{0, 3, 5, 7, 10, -1, -1, -1, -1, -1, -1, -1}, 5},
    // PENTATONIC_MAJOR
    {{0, 2, 4, 7, 9, -1, -1, -1, -1, -1, -1, -1}, 5},
    // BLUES_MINOR
    {{0, 3, 5, 6, 7, 10, -1, -1, -1, -1, -1, -1}, 6},
    // WHOLE_TONE
    {{0, 2, 4, 6, 8, 10, -1, -1, -1, -1, -1, -1}, 6},
    // CHROMATIC
    {{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11}, 12},
    // JAPANESE_IN_SEN
    {{0, 1, 5, 7, 10, -1, -1, -1, -1, -1, -1, -1}, 5},
};

//-----------------------------------------------------------------------------
// Scale name lookup (for UI display)
//-----------------------------------------------------------------------------

inline const char* getScaleName(Scale scale) {
    static const char* names[] = {
        "Major", "Minor", "Dorian", "Mixolydian", "Lydian",
        "Phrygian", "Locrian", "Harmonic Minor", "Harmonic Major",
        "Dorian #4", "Phrygian Dominant", "Melodic Minor",
        "Lydian Augmented", "Lydian Dominant", "Hungarian Minor",
        "Super Locrian", "Spanish", "Bhairav",
        "Pentatonic Minor", "Pentatonic Major", "Blues Minor",
        "Whole Tone", "Chromatic", "Japanese In-Sen"
    };
    return names[static_cast<int>(scale)];
}

//-----------------------------------------------------------------------------
// SequenceStep - Output data for each step
//-----------------------------------------------------------------------------

struct SequenceStep {
    int note;       // Scale degree index (0-6 typically), or -1 for rest
    int octave;     // -1, 0, or 1
    bool accent;    // TB-303 accent
    bool slide;     // TB-303 slide/glide

    bool isRest() const { return note < 0; }
};

//-----------------------------------------------------------------------------
// GeneratorParams - Input parameters for pattern generation
//-----------------------------------------------------------------------------

struct GeneratorParams {
    int patternLength;      // Not used in generate(), but stored for reference
    float density;          // 0-100 (percentage)
    float spread;           // 0-100 (percentage)
    float accentsDensity;   // 0-100 (percentage)
    float slidesDensity;    // 0-100 (percentage)
    uint32_t seed;          // RNG seed
};

//-----------------------------------------------------------------------------
// getNoteInScale - Convert scale degree to MIDI note
//-----------------------------------------------------------------------------
// note: scale degree index (can exceed scale length, will wrap with octave)
// scale: which scale to use
// root: root note offset (0 = C, 1 = C#, etc.)
// octave: base octave offset
// Returns: MIDI note number relative to middle C (60)

inline int getNoteInScale(int note, Scale scale, int root = 0, int octave = 0) {
    if (note < 0) return -1;  // Rest

    const ScaleData& scaleData = SCALES[static_cast<int>(scale)];
    int len = scaleData.length;

    // Wrap the index to fit the scale length
    int wrappedIndex = note % len;

    // Calculate extra octaves if the index exceeded the scale length
    int octaveOffset = note / len;

    // Formula: Note in Scale + Root + (User Octave * 12) + (Wrapped Octave * 12)
    return scaleData.intervals[wrappedIndex] + root + 12 * (octave + octaveOffset);
}

//-----------------------------------------------------------------------------
// Pattern - Fixed-size container for generated sequence
//-----------------------------------------------------------------------------

struct Pattern {
    SequenceStep steps[MAX_STEPS];
    int length;  // Active length (always <= MAX_STEPS)

    Pattern() : length(MAX_STEPS) {
        for (int i = 0; i < MAX_STEPS; i++) {
            steps[i] = {-1, 0, false, false};
        }
    }
};

//-----------------------------------------------------------------------------
// MasterPattern - Stores full pattern data for real-time density/spread control
//-----------------------------------------------------------------------------
// The master pattern contains ALL note data without density/spread masks applied.
// Density and spread are applied in real-time during playback.
//
// - barActivationOrder: Which bar positions activate first as density increases
//   (index 0 = first to activate, typically the "One")
// - scalePriorityOrder: Which scale degrees are added first as spread increases
//   (index 0 = root, typically; index 1 often = 5th)
// - steps[].notePoolIndex: Index into scalePriorityOrder (0 = highest priority note)

struct MasterStep {
    int notePoolIndex;  // 0-6, index into scalePriorityOrder (NOT the scale degree itself)
    int octave;         // -1, 0, or 1
    float accentProb;   // Random value 0-1, compared against accentsDensity
    float slideProb;    // Random value 0-1, compared against slidesDensity
};

struct MasterPattern {
    // Activation order for bar positions (density mask)
    // barActivationOrder[0] = first position to activate (usually beat 1)
    // barActivationOrder[15] = last position to activate
    int barActivationOrder[BAR_LEN];

    // Priority order for scale degrees (spread control)
    // scalePriorityOrder[0] = highest priority note (root)
    // scalePriorityOrder[6] = lowest priority note
    int scalePriorityOrder[SCALE_SIZE];

    // Step data for all 64 steps
    MasterStep steps[MAX_STEPS];

    // Per-step mute mask (for user-created rests)
    // When true, step is forced to rest regardless of density
    bool muted[MAX_STEPS];

    MasterPattern() {
        for (int i = 0; i < BAR_LEN; i++) {
            barActivationOrder[i] = i;
        }
        for (int i = 0; i < SCALE_SIZE; i++) {
            scalePriorityOrder[i] = i;
        }
        for (int i = 0; i < MAX_STEPS; i++) {
            steps[i] = {0, 0, 0.5f, 0.5f};
            muted[i] = false;
        }
    }

    // Check if a bar position is active given current density (0-100)
    bool isStepActive(int step, float density) const {
        int barPos = step % BAR_LEN;
        int activeCount = std::max(0, static_cast<int>(std::round(BAR_LEN * density / 100.0f)));

        // Check if this bar position is within the first 'activeCount' positions
        for (int i = 0; i < activeCount && i < BAR_LEN; i++) {
            if (barActivationOrder[i] == barPos) {
                return true;
            }
        }
        return false;
    }

    // Get the scale degree for a step, constrained by current spread (0-100)
    // Returns -1 if the note is outside the spread pool (treat as rest or quantize)
    int getScaleDegree(int step, float spread, bool quantizeToPool = true) const {
        const MasterStep& ms = steps[step];
        int spreadCount = std::max(1, static_cast<int>(std::round(SCALE_SIZE * spread / 100.0f)));

        if (ms.notePoolIndex < spreadCount) {
            // Note is within the spread pool
            return scalePriorityOrder[ms.notePoolIndex];
        } else if (quantizeToPool) {
            // Note is outside pool - quantize to root (highest priority)
            return scalePriorityOrder[0];
        } else {
            // Note is outside pool - treat as rest
            return -1;
        }
    }

    // Find the notePoolIndex for a given scale degree
    // Returns the index into scalePriorityOrder that contains the given degree
    // Returns 0 (root) if not found
    int findNotePoolIndex(int scaleDegree) const {
        for (int i = 0; i < SCALE_SIZE; i++) {
            if (scalePriorityOrder[i] == scaleDegree) {
                return i;
            }
        }
        return 0;  // Default to root if not found
    }

    // Clear all mutes (called when generating new pattern)
    void clearMutes() {
        for (int i = 0; i < MAX_STEPS; i++) {
            muted[i] = false;
        }
    }

    // Get full step data with density/spread/accent/slide applied
    SequenceStep getStep(int step, float density, float spread,
                         float accentsDensity, float slidesDensity,
                         bool quantizeToPool = true) const {
        // Check user mute first (takes priority over density)
        if (muted[step]) {
            return {-1, 0, false, false};  // Rest due to user mute
        }

        if (!isStepActive(step, density)) {
            return {-1, 0, false, false};  // Rest due to density
        }

        int scaleDegree = getScaleDegree(step, spread, quantizeToPool);
        if (scaleDegree < 0) {
            return {-1, 0, false, false};  // Rest due to spread (if not quantizing)
        }

        const MasterStep& ms = steps[step];
        return {
            scaleDegree,
            ms.octave,
            ms.accentProb < (accentsDensity / 100.0f),
            ms.slideProb < (slidesDensity / 100.0f)
        };
    }
};

//-----------------------------------------------------------------------------
// generateMaster - Generate a master pattern for real-time control
//-----------------------------------------------------------------------------
// Creates a MasterPattern with full note data. Density and spread are NOT
// baked in - they are applied in real-time during playback via getStep().

inline void generateMaster(uint32_t seed, MasterPattern& output) {
    SFC32 rng(seed);

    // --- 1. MUSICAL SPREAD LOGIC ---
    // Weight scale notes and sort by weight (root and 5th get priority)
    struct WeightedNote {
        int index;
        float weight;
    };

    WeightedNote weightedScale[SCALE_SIZE];
    for (int i = 0; i < SCALE_SIZE; i++) {
        float weight = rng.next();
        if (i == 0) weight += 999.0f;  // Always keep Root first
        if (i == 4) weight += 0.5f;    // Often the 5th
        weightedScale[i] = {i, weight};
    }

    // Sort by weight descending
    for (int i = 0; i < SCALE_SIZE - 1; i++) {
        for (int j = 0; j < SCALE_SIZE - i - 1; j++) {
            if (weightedScale[j].weight < weightedScale[j + 1].weight) {
                WeightedNote temp = weightedScale[j];
                weightedScale[j] = weightedScale[j + 1];
                weightedScale[j + 1] = temp;
            }
        }
    }

    // Store the priority order
    for (int i = 0; i < SCALE_SIZE; i++) {
        output.scalePriorityOrder[i] = weightedScale[i].index;
    }

    // --- 2. DENSITY MASK ORDER ---
    struct WeightedStep {
        int step;
        float weight;
    };

    WeightedStep weightedBarSteps[BAR_LEN];
    for (int i = 0; i < BAR_LEN; i++) {
        float weight = rng.next();
        if (i % 4 == 0) weight += 0.5f;  // Boost downbeats
        if (i == 0) weight += 0.5f;       // Huge boost for the "One"
        weightedBarSteps[i] = {i, weight};
    }

    // Sort by weight descending
    for (int i = 0; i < BAR_LEN - 1; i++) {
        for (int j = 0; j < BAR_LEN - i - 1; j++) {
            if (weightedBarSteps[j].weight < weightedBarSteps[j + 1].weight) {
                WeightedStep temp = weightedBarSteps[j];
                weightedBarSteps[j] = weightedBarSteps[j + 1];
                weightedBarSteps[j + 1] = temp;
            }
        }
    }

    // Store the activation order
    for (int i = 0; i < BAR_LEN; i++) {
        output.barActivationOrder[i] = weightedBarSteps[i].step;
    }

    // --- 3. GENERATE STEP CONTENT ---
    // Generate notes using pool indices (0-6), not constrained by spread
    for (int i = 0; i < MAX_STEPS; i++) {
        bool isDownbeat = (i % 4 == 0);
        int notePoolIndex;

        if (isDownbeat && rng.next() > 0.3f) {
            // Downbeats favor the root (pool index 0)
            notePoolIndex = 0;
        } else {
            // Other steps pick from the full pool
            notePoolIndex = rng.randomInt(0, SCALE_SIZE - 1);
        }

        output.steps[i] = {
            notePoolIndex,
            rng.randomInt(-1, 1),
            rng.next(),  // accentProb
            rng.next()   // slideProb
        };
    }
}

//-----------------------------------------------------------------------------
// generate - Main pattern generation function (legacy, for test compatibility)
//-----------------------------------------------------------------------------
// Ported from TypeScript generator.ts
// Uses fixed-size arrays to avoid heap allocation in audio thread
//
// Algorithm:
// 1. Weight scale notes (favor root and 5th)
// 2. Select subset of notes based on spread parameter
// 3. Generate rhythm mask based on density (favor downbeats)
// 4. Generate note/octave/accent/slide for each step
// 5. Apply rhythm mask to create rests

inline void generate(const GeneratorParams& params, Pattern& output) {
    SFC32 rng(params.seed);

    // --- 1. MUSICAL SPREAD LOGIC ---
    // Weight scale notes and sort by weight
    struct WeightedNote {
        int index;
        float weight;
    };

    WeightedNote weightedScale[SCALE_SIZE];
    for (int i = 0; i < SCALE_SIZE; i++) {
        float weight = rng.next();
        if (i == 0) weight += 999.0f;  // Always keep Root first
        if (i == 4) weight += 0.5f;    // Often the 5th
        weightedScale[i] = {i, weight};
    }

    // Sort by weight descending (simple bubble sort - only 7 elements)
    for (int i = 0; i < SCALE_SIZE - 1; i++) {
        for (int j = 0; j < SCALE_SIZE - i - 1; j++) {
            if (weightedScale[j].weight < weightedScale[j + 1].weight) {
                WeightedNote temp = weightedScale[j];
                weightedScale[j] = weightedScale[j + 1];
                weightedScale[j + 1] = temp;
            }
        }
    }

    // Extract sorted indices
    int sortedScale[SCALE_SIZE];
    for (int i = 0; i < SCALE_SIZE; i++) {
        sortedScale[i] = weightedScale[i].index;
    }

    // Select notes based on spread
    int spreadCount = std::max(1, static_cast<int>(std::round(SCALE_SIZE * (params.spread / 100.0f))));
    int selectedNotes[SCALE_SIZE];
    for (int i = 0; i < spreadCount; i++) {
        selectedNotes[i] = sortedScale[i];
    }

    // --- 2. DENSITY MASK (RHYTHM) ---
    struct WeightedStep {
        int step;
        float weight;
    };

    WeightedStep weightedBarSteps[BAR_LEN];
    for (int i = 0; i < BAR_LEN; i++) {
        float weight = rng.next();
        if (i % 4 == 0) weight += 0.5f;  // Boost downbeats
        if (i == 0) weight += 0.5f;       // Huge boost for the "One"
        weightedBarSteps[i] = {i, weight};
    }

    // Sort by weight descending
    for (int i = 0; i < BAR_LEN - 1; i++) {
        for (int j = 0; j < BAR_LEN - i - 1; j++) {
            if (weightedBarSteps[j].weight < weightedBarSteps[j + 1].weight) {
                WeightedStep temp = weightedBarSteps[j];
                weightedBarSteps[j] = weightedBarSteps[j + 1];
                weightedBarSteps[j + 1] = temp;
            }
        }
    }

    // Extract activation order
    int barActivationOrder[BAR_LEN];
    for (int i = 0; i < BAR_LEN; i++) {
        barActivationOrder[i] = weightedBarSteps[i].step;
    }

    // --- 3. GENERATE STEP CONTENT ---
    struct StepData {
        int noteIndex;
        int octave;
        float accentProb;
        float slideProb;
    };

    StepData stepData[MAX_STEPS];
    for (int i = 0; i < MAX_STEPS; i++) {
        bool isDownbeat = (i % 4 == 0);
        int noteIndex = 0;

        if (isDownbeat && rng.next() > 0.3f) {
            noteIndex = 0;
        } else {
            noteIndex = rng.randomInt(0, spreadCount - 1);
        }

        stepData[i] = {
            noteIndex,
            rng.randomInt(-1, 1),
            rng.next(),
            rng.next()
        };
    }

    // --- 4. APPLY MASKS ---
    int numStepsToGeneratePerBar = static_cast<int>(std::round(BAR_LEN * (params.density / 100.0f)));

    // Create active steps set (using simple array lookup)
    bool activeBarSteps[BAR_LEN] = {false};
    for (int i = 0; i < numStepsToGeneratePerBar && i < BAR_LEN; i++) {
        activeBarSteps[barActivationOrder[i]] = true;
    }

    // Generate final output
    output.length = MAX_STEPS;
    for (int i = 0; i < MAX_STEPS; i++) {
        int barPosition = i % BAR_LEN;

        if (!activeBarSteps[barPosition]) {
            // Rest
            output.steps[i] = {-1, 0, false, false};
        } else {
            const StepData& data = stepData[i];
            output.steps[i] = {
                selectedNotes[data.noteIndex],
                data.octave,
                data.accentProb < (params.accentsDensity / 100.0f),
                data.slideProb < (params.slidesDensity / 100.0f)
            };
        }
    }
}

//-----------------------------------------------------------------------------
// Voltage conversion helpers for VCV Rack
//-----------------------------------------------------------------------------

// Convert scale degree + octave to 1V/oct CV
// baseOctave: the octave for note 0 (e.g., 4 for C4)
inline float stepToVoltage(const SequenceStep& step, Scale scale, int root = 0, int baseOctave = 0) {
    if (step.isRest()) return 0.0f;

    int midiNote = getNoteInScale(step.note, scale, root, step.octave + baseOctave);

    // VCV Rack standard: 0V = C4 (MIDI 60)
    // 1V/octave, so each semitone is 1/12 volt
    return (midiNote - 60) / 12.0f;
}

} // namespace AcidGenerator
