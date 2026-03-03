#pragma once

#include <cstdint>
#include "Generator.hpp"

namespace AcidGenerator {

//-----------------------------------------------------------------------------
// Pure functions extracted from Daisy main.cpp for testability
//-----------------------------------------------------------------------------

// MIDI message container (no Daisy SDK dependency)
struct MidiMsg {
    uint8_t data[3];
    int length;
};

// Timing constants
static constexpr float GATE_DURATION_S = 0.020f;  // 20ms normal gate
static constexpr float SLIDE_TIME_S = 0.050f;     // 50ms portamento

// Build a NoteOn MIDI message (Avalon Bassline compatible)
// accent: true -> velocity 127, false -> velocity 60
inline MidiMsg makeNoteOn(uint8_t note, bool accent, uint8_t channel) {
    MidiMsg msg;
    msg.data[0] = static_cast<uint8_t>(0x90 | (channel & 0x0F));
    msg.data[1] = static_cast<uint8_t>(note & 0x7F);
    msg.data[2] = accent ? 127 : 60;
    msg.length = 3;
    return msg;
}

// Build a NoteOff MIDI message
inline MidiMsg makeNoteOff(uint8_t note, uint8_t channel) {
    MidiMsg msg;
    msg.data[0] = static_cast<uint8_t>(0x80 | (channel & 0x0F));
    msg.data[1] = static_cast<uint8_t>(note & 0x7F);
    msg.data[2] = 0;
    msg.length = 3;
    return msg;
}

// Calculate MIDI note from sequence step + scale/root/octave
// Returns clamped 0-127
inline int calculateMidiNote(const SequenceStep& step, Scale scale,
                             int rootNote, int baseOctave) {
    int midiNote = getNoteInScale(step.note, scale, rootNote,
                                  step.octave + baseOctave);
    midiNote += 48;  // Shift to reasonable MIDI range
    if (midiNote < 0) midiNote = 0;
    if (midiNote > 127) midiNote = 127;
    return midiNote;
}

// Calculate pitch voltage (1V/oct, 0V = C2 = MIDI 36)
inline float calculatePitchVoltage(int midiNote) {
    return static_cast<float>(midiNote - 36) / 12.0f;
}

// Calculate gate duration in seconds
inline float calculateGateDuration(bool slide, float measuredClockPeriod) {
    return slide ? measuredClockPeriod * 1.1f : GATE_DURATION_S;
}

// Calculate slide rate (voltage per second)
inline float calculateSlideRate(float currentPitch, float targetPitch) {
    return (targetPitch - currentPitch) / SLIDE_TIME_S;
}

// Convert pitch voltage to 12-bit DAC value (0-4095)
// 0V -> 0, 5V -> 4095
inline uint16_t pitchToDac(float pitchVoltage) {
    if (pitchVoltage < 0.0f) pitchVoltage = 0.0f;
    if (pitchVoltage > 5.0f) pitchVoltage = 5.0f;
    return static_cast<uint16_t>(pitchVoltage / 5.0f * 4095.0f);
}

} // namespace AcidGenerator
