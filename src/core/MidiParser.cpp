// AutoZYC — Standard MIDI File (SMF) parser
#include "src/core/MidiParser.h"
#include <fstream>
#include <cstring>
#include <cmath>

// --- Binary read helpers (pointer-advancing, big-endian) ---

uint16_t MidiParser::readBE16(const uint8_t*& p)
{
    uint16_t v = ((uint16_t)p[0] << 8) | p[1];
    p += 2;
    return v;
}

int16_t MidiParser::readBE16s(const uint8_t*& p)
{
    int16_t v = (int16_t)(((uint16_t)p[0] << 8) | p[1]);
    p += 2;
    return v;
}

uint32_t MidiParser::readBE32(const uint8_t*& p)
{
    uint32_t v = ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16)
               | ((uint32_t)p[2] << 8)  | p[3];
    p += 4;
    return v;
}

uint32_t MidiParser::readVarLen(const uint8_t*& p, const uint8_t* end)
{
    uint32_t v = 0;
    while (p < end) {
        uint8_t b = *p++;
        v = (v << 7) | (b & 0x7F);
        if ((b & 0x80) == 0) break;
    }
    return v;
}

// --- Main parser ---

bool MidiParser::parse(const char* filePath, ProjectData& out)
{
    // Reset output
    out = ProjectData{};

    // Read entire file into memory (binary)
    std::ifstream file(filePath, std::ios::binary | std::ios::ate);
    if (!file.is_open()) return false;

    std::streamsize size = file.tellg();
    if (size < 14) return false;  // minimum valid MIDI file size

    file.seekg(0, std::ios::beg);
    std::vector<uint8_t> data(static_cast<size_t>(size));
    if (!file.read(reinterpret_cast<char*>(data.data()), size)) return false;

    const uint8_t* p = data.data();
    const uint8_t* end = p + data.size();

    // --- Parse MThd header ---
    if (end - p < 8 || std::memcmp(p, "MThd", 4) != 0) return false;
    p += 4;
    uint32_t headerLen = readBE32(p);
    if (headerLen < 6) return false;

    /*uint16_t format =*/ readBE16(p);   // SMF format 0/1/2
    uint16_t ntrks    = readBE16(p);     // number of tracks
    int16_t  division = readBE16s(p);    // ticks per quarter note (signed!)
    p += headerLen - 6;

    // SMPTE timecode (division < 0) is not supported
    if (division < 0) return false;

    double ticksPerBeat = static_cast<double>(division);
    double usecPerBeat  = 500000.0;       // default: 120 BPM
    double usecPerTick  = usecPerBeat / ticksPerBeat;

    // --- Parse MTrk chunks ---
    // Collect notes per MIDI track index for TrackInfo building
    struct RawNote {
        double positionSec;
        double lengthSec;
        int    pitch;
        int    velocity;
    };
    std::vector<std::vector<RawNote>> trackNotes(ntrks);

    for (int trackIdx = 0; trackIdx < static_cast<int>(ntrks); trackIdx++) {
        // Verify MTrk magic
        if (end - p < 8) break;
        if (std::memcmp(p, "MTrk", 4) != 0) break;
        p += 4;
        uint32_t trackLen = readBE32(p);
        const uint8_t* trackStart = p;
        const uint8_t* trackEnd   = p + trackLen;

        if (trackEnd > end) break;  // truncated file

        std::vector<PendingNote> pending;
        double absTimeSec = 0.0;
        uint8_t runningStatus = 0;

        while (p < trackEnd) {
            uint32_t delta = readVarLen(p, trackEnd);
            if (p >= trackEnd) break;
            absTimeSec += delta * usecPerTick / 1000000.0;

            uint8_t byte = *p;
            uint8_t status;

            // Running status: if MSB is 0, reuse previous status byte
            if (byte < 0x80) {
                if (runningStatus == 0) {
                    p++;  // skip orphan data byte
                    continue;
                }
                status = runningStatus;
            } else {
                status = *p++;
                if (status < 0xF0)
                    runningStatus = status;
                else
                    runningStatus = 0;  // system messages reset running status
            }

            // --- Meta Event (0xFF) ---
            if (status == 0xFF) {
                if (p >= trackEnd) break;
                uint8_t metaType = *p++;
                uint32_t metaLen = readVarLen(p, trackEnd);
                const uint8_t* metaData = p;
                p += metaLen;

                // Set Tempo: FF 51 03 tt tt tt (microseconds per quarter note)
                if (metaType == 0x51 && metaLen == 3 && metaData + 3 <= trackEnd) {
                    usecPerBeat = static_cast<double>(
                        ((uint32_t)metaData[0] << 16) |
                        ((uint32_t)metaData[1] << 8)  |
                         (uint32_t)metaData[2]
                    );
                    usecPerTick = usecPerBeat / ticksPerBeat;
                }

                // End of Track: FF 2F 00
                if (metaType == 0x2F) break;

                continue;
            }

            // --- SysEx (0xF0 / 0xF7): skip ---
            if (status == 0xF0 || status == 0xF7) {
                uint32_t sysexLen = readVarLen(p, trackEnd);
                p += sysexLen;
                continue;
            }

            // --- Channel Voice Messages ---
            if (status < 0xF0) {
                uint8_t msgType = (status >> 4) & 0x0F;
                uint8_t channel = status & 0x0F;

                // Note Off (0x8n) or Note On (0x9n)
                if (msgType == 0x8 || msgType == 0x9) {
                    if (p + 2 > trackEnd) break;
                    uint8_t note     = *p++;
                    uint8_t velocity = *p++;

                    if (msgType == 0x9 && velocity > 0) {
                        // Note On — store in pending list
                        PendingNote pn;
                        pn.note     = note;
                        pn.channel  = channel;
                        pn.startSec = absTimeSec;
                        pn.velocity = velocity;
                        pending.push_back(pn);
                    } else {
                        // Note Off (or Note On with vel=0)
                        // Match against pending notes (same note + channel)
                        for (size_t i = 0; i < pending.size(); i++) {
                            if (pending[i].note == note && pending[i].channel == channel) {
                                RawNote rn;
                                rn.positionSec = pending[i].startSec;
                                rn.lengthSec   = absTimeSec - pending[i].startSec;
                                rn.pitch       = static_cast<int>(note);
                                rn.velocity    = static_cast<int>(pending[i].velocity);
                                trackNotes[trackIdx].push_back(rn);
                                pending.erase(pending.begin() + i);
                                break;
                            }
                        }
                    }
                }
                // Polyphonic Key Pressure (0xAn): 2 data bytes
                else if (msgType == 0xA) {
                    if (p + 2 <= trackEnd) p += 2; else break;
                }
                // Control Change (0xBn): 2 data bytes
                else if (msgType == 0xB) {
                    if (p + 2 <= trackEnd) p += 2; else break;
                }
                // Program Change (0xCn): 1 data byte
                else if (msgType == 0xC) {
                    if (p + 1 <= trackEnd) p += 1; else break;
                }
                // Channel Pressure (0xDn): 1 data byte
                else if (msgType == 0xD) {
                    if (p + 1 <= trackEnd) p += 1; else break;
                }
                // Pitch Bend (0xEn): 2 data bytes
                else if (msgType == 0xE) {
                    if (p + 2 <= trackEnd) p += 2; else break;
                }
            }
        }

        // --- Close any remaining pending notes ---
        for (auto& pn : pending) {
            RawNote rn;
            rn.positionSec = pn.startSec;
            rn.lengthSec   = absTimeSec - pn.startSec;
            rn.pitch       = static_cast<int>(pn.note);
            rn.velocity    = static_cast<int>(pn.velocity);
            trackNotes[trackIdx].push_back(rn);
        }

        // Advance to end of this track chunk
        p = trackStart + trackLen;
    }

    // --- Build ProjectData output ---
    // Apply G2: MIDI track index 0 -> stored track = 100
    for (int trackIdx = 0; trackIdx < static_cast<int>(ntrks); trackIdx++) {
        auto& notes = trackNotes[trackIdx];
        if (notes.empty()) continue;  // skip empty tracks (e.g. tempo-only)

        TrackInfo info;
        info.index      = 100 + trackIdx;   // G2: +100 offset
        info.name       = "MIDI Track " + std::to_string(trackIdx + 1);
        info.eventCount = static_cast<int>(notes.size());

        // Compute min/max pitch
        info.minPitch = notes[0].pitch;
        info.maxPitch = notes[0].pitch;

        for (auto& rn : notes) {
            MidiNote mn;
            mn.positionSec = rn.positionSec;
            mn.lengthSec   = rn.lengthSec;
            mn.pitch       = rn.pitch;
            mn.velocity    = rn.velocity;
            info.notes.push_back(mn);

            if (rn.pitch < info.minPitch) info.minPitch = rn.pitch;
            if (rn.pitch > info.maxPitch) info.maxPitch = rn.pitch;
        }

        // G5: if min == max, expand by ±1
        if (info.minPitch == info.maxPitch) {
            info.minPitch -= 1;
            info.maxPitch += 1;
        }

        out.tracks.push_back(std::move(info));
    }

    // Set BPM from initial/default tempo
    out.bpm = 60000000.0 / usecPerBeat;

    return true;
}