#include "daisy_patch.h"
#include "daisysp.h"
#include "daisy_logic.hpp"
#include <cstring>
#include <cstdio>

using namespace daisy;
using namespace AcidGenerator;

// Hardware
static DaisyPatch patch;

// Generator state
static MasterPattern masterPattern;
static uint32_t currentSeed = 12345;

// Sequencer state
static int currentStep = -1;
static int patternLength = 16;
static Scale currentScale = Scale::MINOR;
static int rootNote = 0;       // 0=C, 1=C#, ... 11=B
static int baseOctave = 0;     // -2 to +2

// Knob-mapped parameters (read from hardware)
static float density = 50.0f;
static float spread = 50.0f;
static float accentDensity = 25.0f;
static float slideDensity = 15.0f;

// Clock / gate state
static bool lastGate1High = false;  // Clock input (GATE_IN_1)
static bool lastGate2High = false;  // Reset / Generate (GATE_IN_2)

// Output state
static float currentPitch = 0.0f;
static float slideTargetPitch = 0.0f;
static bool currentSlideActive = false;
static float slideRate = 0.0f;
static bool gateHigh = false;
static bool accentHigh = false;
static float gateTimer = 0.0f;
static float accentTimer = 0.0f;
static float measuredClockPeriod = 0.125f; // ~120 BPM 16ths
static float lastClockTime = 0.0f;
static float elapsedTime = 0.0f;

// Display state
static int displayPage = 0;
static bool displayDirty = true;

// Menu state (encoder navigation)
enum class MenuParam {
    SCALE,
    ROOT,
    OCTAVE,
    LENGTH,
    NUM_PARAMS
};
static MenuParam currentMenuParam = MenuParam::SCALE;

// MIDI state
static MidiUartHandler midi;
static uint8_t midiChannel = 0; // 0-indexed (channel 1)
static int lastMidiNote = -1;
static bool lastNoteHadSlide = false;

// Timing constants (GATE_DURATION_S and SLIDE_TIME_S are in daisy_logic.hpp)
static constexpr float RETRIGGER_GAP = 0.001f;    // 1ms retrigger gap
static constexpr float DISPLAY_UPDATE_RATE = 0.016f; // ~60fps

// Note names for display
static const char* NOTE_NAMES[] = {
    "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
};

// Scale abbreviations for display
static const char* SCALE_ABBR[] = {
    "MAJ", "MIN", "DOR", "MIX", "LYD", "PHR", "LOC",
    "H-m", "H-M", "D#4", "PhD", "Mm", "L+", "LD",
    "HUN", "SuL", "SPA", "BHV", "Pm", "PM", "BLU",
    "WHL", "CHR", "INS"
};

//-----------------------------------------------------------------------------
// MIDI Output (Avalon Bassline compatible)
//-----------------------------------------------------------------------------

static void sendMidiNoteOn(uint8_t note, bool accent) {
    MidiMsg msg = makeNoteOn(note, accent, midiChannel);
    midi.SendMessage(msg.data, msg.length);
}

static void sendMidiNoteOff(uint8_t note) {
    MidiMsg msg = makeNoteOff(note, midiChannel);
    midi.SendMessage(msg.data, msg.length);
}

//-----------------------------------------------------------------------------
// Pattern Generation
//-----------------------------------------------------------------------------

static void generateNewPattern() {
    // Derive new seed from current time
    currentSeed = static_cast<uint32_t>(System::GetNow()) ^ (currentSeed * 1664525 + 1013904223);
    generateMaster(currentSeed, masterPattern);
    masterPattern.clearMutes();
    currentStep = -1;
    displayDirty = true;
}

//-----------------------------------------------------------------------------
// Step Processing (called on clock rising edge)
//-----------------------------------------------------------------------------

static void advanceStep() {
    currentStep++;
    if (currentStep >= patternLength) {
        currentStep = 0;
    }

    // Get step with current real-time parameters applied
    SequenceStep step = masterPattern.getStep(
        currentStep, density, spread, accentDensity, slideDensity
    );

    if (step.isRest()) {
        // Rest - close gate
        gateHigh = false;
        accentHigh = false;
        gateTimer = 0.0f;
        accentTimer = 0.0f;

        // Send MIDI note off for previous note
        if (lastMidiNote >= 0) {
            sendMidiNoteOff(static_cast<uint8_t>(lastMidiNote));
            lastMidiNote = -1;
        }
        lastNoteHadSlide = false;
    } else {
        // Calculate MIDI note and pitch voltage
        int midiNote = calculateMidiNote(step, currentScale, rootNote, baseOctave);
        float targetPitch = calculatePitchVoltage(midiNote);

        if (currentSlideActive) {
            // Slide from current pitch to new pitch
            slideTargetPitch = targetPitch;
            slideRate = calculateSlideRate(currentPitch, slideTargetPitch);
            // No gate retrigger (legato)
        } else {
            // Normal note - retrigger gate
            currentPitch = targetPitch;
            slideTargetPitch = targetPitch;
            gateHigh = true;
            gateTimer = calculateGateDuration(step.slide, measuredClockPeriod);
        }

        // Accent
        accentHigh = step.accent;
        accentTimer = gateTimer;

        // MIDI output
        // For Avalon slide: send new note BEFORE releasing old (overlapping notes)
        if (step.slide && lastMidiNote >= 0 && lastNoteHadSlide) {
            // Overlapping: send new note on first, then old note off
            sendMidiNoteOn(static_cast<uint8_t>(midiNote), step.accent);
            sendMidiNoteOff(static_cast<uint8_t>(lastMidiNote));
        } else {
            // Normal: off first, then on
            if (lastMidiNote >= 0) {
                sendMidiNoteOff(static_cast<uint8_t>(lastMidiNote));
            }
            sendMidiNoteOn(static_cast<uint8_t>(midiNote), step.accent);
        }

        lastMidiNote = midiNote;
        lastNoteHadSlide = step.slide;
        currentSlideActive = step.slide;
    }

    // Update display page to follow playhead
    int newPage = currentStep / 16;
    if (newPage != displayPage) {
        displayPage = newPage;
    }
    displayDirty = true;
}

//-----------------------------------------------------------------------------
// Audio Callback
//-----------------------------------------------------------------------------

static void AudioCallback(AudioHandle::InputBuffer in,
                          AudioHandle::OutputBuffer out,
                          size_t size)
{
    float sampleRate = patch.AudioSampleRate();
    float dt = 1.0f / sampleRate;

    patch.ProcessAllControls();

    // Read knobs (0.0 - 1.0 → 0 - 100)
    density = patch.GetKnobValue(DaisyPatch::CTRL_1) * 100.0f;
    spread = patch.GetKnobValue(DaisyPatch::CTRL_2) * 100.0f;
    accentDensity = patch.GetKnobValue(DaisyPatch::CTRL_3) * 100.0f;
    slideDensity = patch.GetKnobValue(DaisyPatch::CTRL_4) * 100.0f;

    // Check gate inputs (clock = GATE_IN_1)
    bool gate1High = patch.gate_input[DaisyPatch::GATE_IN_1].State();
    bool gate2High = patch.gate_input[DaisyPatch::GATE_IN_2].State();

    // Clock rising edge
    if (gate1High && !lastGate1High) {
        // Measure clock period
        float now = elapsedTime;
        float period = now - lastClockTime;
        if (period > 0.010f && period < 2.0f) {
            measuredClockPeriod = period;
        }
        lastClockTime = now;

        advanceStep();
    }
    lastGate1High = gate1High;

    // Gate 2: reset (rising edge)
    if (gate2High && !lastGate2High) {
        currentStep = -1;
        currentSlideActive = false;
        gateHigh = false;
        accentHigh = false;
        if (lastMidiNote >= 0) {
            sendMidiNoteOff(static_cast<uint8_t>(lastMidiNote));
            lastMidiNote = -1;
        }
        displayDirty = true;
    }
    lastGate2High = gate2High;

    // Process each sample
    for (size_t i = 0; i < size; i++) {
        elapsedTime += dt;

        // Slide portamento
        if (currentPitch != slideTargetPitch) {
            float diff = slideTargetPitch - currentPitch;
            float step = slideRate * dt;
            if (std::fabs(step) >= std::fabs(diff)) {
                currentPitch = slideTargetPitch;
            } else {
                currentPitch += step;
            }
        }

        // Gate timer
        if (gateTimer > 0.0f) {
            gateTimer -= dt;
            if (gateTimer <= 0.0f) {
                gateHigh = false;
                gateTimer = 0.0f;
            }
        }

        // Accent timer
        if (accentTimer > 0.0f) {
            accentTimer -= dt;
            if (accentTimer <= 0.0f) {
                accentHigh = false;
                accentTimer = 0.0f;
            }
        }

        // Output CV 1: Pitch (0-5V range via DAC)
        uint16_t pitchDac = pitchToDac(currentPitch);

        // Output CV 2: Accent (0V or 5V)
        float accentVoltage = accentHigh ? 1.0f : 0.0f;

        // Write to DAC
        patch.seed.dac.WriteValue(DacHandle::Channel::ONE, pitchDac);
        patch.seed.dac.WriteValue(DacHandle::Channel::TWO,
            static_cast<uint16_t>(accentVoltage * 4095.0f));

        // Gate output (digital GPIO)
        dsy_gpio_write(&patch.gate_output, gateHigh);

        // Audio passthrough (silence - this is a sequencer, not a synth)
        for (size_t ch = 0; ch < 4; ch++) {
            out[ch][i] = 0.0f;
        }
    }
}

//-----------------------------------------------------------------------------
// OLED Display
//-----------------------------------------------------------------------------

static void DrawPianoRoll() {
    auto& display = patch.display;

    // Header: "AcidGen" + scale info
    char header[32];
    snprintf(header, sizeof(header), "%s %s", NOTE_NAMES[rootNote], SCALE_ABBR[static_cast<int>(currentScale)]);
    display.SetCursor(0, 0);
    display.WriteString(header, Font_6x8, true);

    // Page indicator
    int totalPages = (patternLength + 15) / 16;
    if (totalPages > 1) {
        char pageStr[8];
        snprintf(pageStr, sizeof(pageStr), "%d/%d", displayPage + 1, totalPages);
        display.SetCursor(104, 0);
        display.WriteString(pageStr, Font_6x8, true);
    }

    // Piano roll area: y=10 to y=56 (46 pixels), x=0 to x=127
    // 16 steps visible, each 8px wide = 128px
    // 2 octaves = 24 semitones, ~2px per semitone
    int rollTop = 10;
    int rollBottom = 56;
    int rollHeight = rollBottom - rollTop;
    int stepWidth = 8; // 128 / 16 = 8px per step

    int pageOffset = displayPage * 16;
    int stepsOnPage = patternLength - pageOffset;
    if (stepsOnPage > 16) stepsOnPage = 16;

    // Draw grid lines (every 4 steps)
    for (int i = 4; i < 16; i += 4) {
        int x = i * stepWidth;
        display.DrawLine(x, rollTop, x, rollBottom, true);
    }

    // Draw horizontal line at middle (octave boundary)
    display.DrawLine(0, rollTop + rollHeight / 2, 127, rollTop + rollHeight / 2, true);

    // Draw steps
    for (int i = 0; i < stepsOnPage; i++) {
        int stepIdx = pageOffset + i;
        SequenceStep step = masterPattern.getStep(
            stepIdx, density, spread, accentDensity, slideDensity
        );

        int x = i * stepWidth;

        if (!step.isRest()) {
            // Get MIDI note for display
            int midiNote = getNoteInScale(step.note, currentScale, rootNote, step.octave + baseOctave);
            midiNote += 48; // Shift to reasonable range

            // Map MIDI note to Y position (higher notes = higher on screen)
            // Show range of ~2 octaves centered around middle C area
            int centerNote = 48 + rootNote + baseOctave * 12;
            int noteOffset = midiNote - (centerNote - 12); // -12 to +12 range
            if (noteOffset < 0) noteOffset = 0;
            if (noteOffset > 24) noteOffset = 24;

            int y = rollBottom - (noteOffset * rollHeight / 24);
            int barHeight = 3;

            // Draw note bar
            display.DrawRect(x + 1, y - barHeight / 2, x + stepWidth - 1, y + barHeight / 2, true, true);

            // Accent indicator: dot above
            if (step.accent) {
                display.DrawCircle(x + stepWidth / 2, rollTop + 2, 1, true);
            }

            // Slide indicator: small line below note
            if (step.slide) {
                display.DrawLine(x + stepWidth - 2, y + 2, x + stepWidth + 1, y + 2, true);
            }
        }

        // Current step indicator
        if (stepIdx == currentStep) {
            // Invert column or draw bright line at bottom
            display.DrawLine(x, rollBottom + 1, x + stepWidth - 1, rollBottom + 1, true);
            display.DrawLine(x, rollBottom + 2, x + stepWidth - 1, rollBottom + 2, true);
        }
    }

    // Bottom bar: menu parameter
    char menuStr[32];
    switch (currentMenuParam) {
        case MenuParam::SCALE:
            snprintf(menuStr, sizeof(menuStr), "SCL:%s", getScaleName(currentScale));
            break;
        case MenuParam::ROOT:
            snprintf(menuStr, sizeof(menuStr), "ROOT:%s", NOTE_NAMES[rootNote]);
            break;
        case MenuParam::OCTAVE:
            snprintf(menuStr, sizeof(menuStr), "OCT:%+d", baseOctave);
            break;
        case MenuParam::LENGTH:
            snprintf(menuStr, sizeof(menuStr), "LEN:%d", patternLength);
            break;
        default:
            menuStr[0] = '\0';
            break;
    }
    display.SetCursor(0, 57);
    display.WriteString(menuStr, Font_6x8, true);
}

static void UpdateDisplay() {
    auto& display = patch.display;
    display.Fill(false);
    DrawPianoRoll();
    display.Update();
}

//-----------------------------------------------------------------------------
// Encoder Handling
//-----------------------------------------------------------------------------

static void ProcessEncoder() {
    // Encoder press: cycle through menu parameters
    if (patch.encoder.RisingEdge()) {
        int next = static_cast<int>(currentMenuParam) + 1;
        if (next >= static_cast<int>(MenuParam::NUM_PARAMS)) next = 0;
        currentMenuParam = static_cast<MenuParam>(next);
        displayDirty = true;
    }

    // Encoder long press: generate new pattern
    if (patch.encoder.TimeHeldMs() > 1000.0f && patch.encoder.FallingEdge()) {
        generateNewPattern();
    }

    // Encoder turn: adjust current parameter
    int inc = patch.encoder.Increment();
    if (inc != 0) {
        switch (currentMenuParam) {
            case MenuParam::SCALE: {
                int s = static_cast<int>(currentScale) + inc;
                if (s < 0) s = static_cast<int>(Scale::NUM_SCALES) - 1;
                if (s >= static_cast<int>(Scale::NUM_SCALES)) s = 0;
                currentScale = static_cast<Scale>(s);
                break;
            }
            case MenuParam::ROOT: {
                rootNote += inc;
                if (rootNote < 0) rootNote = 11;
                if (rootNote > 11) rootNote = 0;
                break;
            }
            case MenuParam::OCTAVE: {
                baseOctave += inc;
                if (baseOctave < -2) baseOctave = -2;
                if (baseOctave > 2) baseOctave = 2;
                break;
            }
            case MenuParam::LENGTH: {
                patternLength += inc;
                if (patternLength < 1) patternLength = 1;
                if (patternLength > 64) patternLength = 64;
                break;
            }
            default:
                break;
        }
        displayDirty = true;
    }
}

//-----------------------------------------------------------------------------
// Main
//-----------------------------------------------------------------------------

int main(void)
{
    // Initialize hardware
    patch.Init();
    patch.SetAudioBlockSize(48);
    patch.SetAudioSampleRate(SaiHandle::Config::SampleRate::SAI_48KHZ);

    // Initialize MIDI
    MidiUartHandler::Config midi_cfg;
    midi.Init(midi_cfg);
    midi.StartReceive();

    // Generate initial pattern
    currentSeed = System::GetNow();
    generateMaster(currentSeed, masterPattern);
    masterPattern.clearMutes();

    // Start audio processing
    patch.StartAdc();
    patch.StartAudio(AudioCallback);

    // Main loop (display + encoder, runs at ~60fps)
    float displayTimer = 0.0f;
    uint32_t lastTick = System::GetNow();

    while (1) {
        // Calculate delta time
        uint32_t now = System::GetNow();
        float dt = static_cast<float>(now - lastTick) / 1000.0f;
        lastTick = now;
        displayTimer += dt;

        // Process encoder
        patch.ProcessAllControls();
        ProcessEncoder();

        // Update display at ~60fps
        if (displayTimer >= DISPLAY_UPDATE_RATE || displayDirty) {
            UpdateDisplay();
            displayTimer = 0.0f;
            displayDirty = false;
        }

        // Small delay to prevent busy-waiting
        System::Delay(1);
    }
}
