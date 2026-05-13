// AutoZYC — Standard MIDI File (SMF) parser header
#pragma once

#include "src/core/EventTypes.h"
#include <cstdint>
#include <vector>

class MidiParser
{
public:
    /// Parse standard MIDI file (.mid/.midi/.smf)
    /// Returns true on success, false if file not found or invalid format
    bool parse(const char* filePath, ProjectData& out);

private:
    // MIDI binary read helpers
    static uint16_t readBE16(const uint8_t*& p);
    static int16_t  readBE16s(const uint8_t*& p);
    static uint32_t readBE32(const uint8_t*& p);
    static uint32_t readVarLen(const uint8_t*& p, const uint8_t* end);

    // Pending note tracking
    struct PendingNote
    {
        uint8_t note;
        uint8_t channel;
        double  startSec;
        uint8_t velocity;   // velocity from Note On
    };
};