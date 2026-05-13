// AutoZYC — MIDI parser unit tests
// Included by main.cpp (catch.hpp already available)
#include "src/core/MidiParser.h"
#include "src/core/EventTypes.h"
#include <fstream>
#include <cstring>
#include <cstdio>

// Helper: write binary data to a temp file, return path or empty on failure
static std::string writeTempMidi(const std::vector<uint8_t>& data) {
    std::string path = "test_midi_temp.mid";
    std::ofstream file(path, std::ios::binary);
    if (!file) return "";
    file.write(reinterpret_cast<const char*>(data.data()), data.size());
    file.close();
    return path;
}

// Helper: remove temp file
static void removeTempMidi(const std::string& path) {
    if (!path.empty()) std::remove(path.c_str());
}

TEST_CASE("MidiParser: invalid/empty file returns false", "[midi_parser]") {
    MidiParser parser;
    ProjectData out;

    // Non-existent file
    REQUIRE_FALSE(parser.parse("nonexistent_file_xyz.mid", out));
    REQUIRE(out.tracks.empty());

    // Empty file (write empty file to test)
    {
        std::ofstream f("test_midi_empty.mid", std::ios::binary);
        f.close();
    }
    REQUIRE_FALSE(parser.parse("test_midi_empty.mid", out));
    std::remove("test_midi_empty.mid");

    // Invalid magic bytes
    std::vector<uint8_t> bad = {'B','A','D','!', 0,0,0,0, 0,0,0,0, 0,0};
    std::string path = writeTempMidi(bad);
    REQUIRE_FALSE(parser.parse(path.c_str(), out));
    removeTempMidi(path);
}

static std::vector<uint8_t> buildMinimalMidi(uint16_t format, uint16_t ntrks,
                                              int16_t division, uint32_t tempo,
                                              const std::vector<std::vector<uint8_t>>& trackEvents) {
    // MThd header
    std::vector<uint8_t> data;
    data.push_back('M'); data.push_back('T'); data.push_back('h'); data.push_back('d');
    // header length = 6
    data.push_back(0); data.push_back(0); data.push_back(0); data.push_back(6);
    // format
    data.push_back(static_cast<uint8_t>(format >> 8));
    data.push_back(static_cast<uint8_t>(format & 0xFF));
    // ntrks
    data.push_back(static_cast<uint8_t>(ntrks >> 8));
    data.push_back(static_cast<uint8_t>(ntrks & 0xFF));
    // division (signed int16, big-endian)
    data.push_back(static_cast<uint8_t>((division >> 8) & 0xFF));
    data.push_back(static_cast<uint8_t>(division & 0xFF));

    for (const auto& events : trackEvents) {
        // MTrk header
        data.push_back('M'); data.push_back('T'); data.push_back('r'); data.push_back('k');
        uint32_t trackLen = static_cast<uint32_t>(events.size());
        data.push_back(static_cast<uint8_t>((trackLen >> 24) & 0xFF));
        data.push_back(static_cast<uint8_t>((trackLen >> 16) & 0xFF));
        data.push_back(static_cast<uint8_t>((trackLen >> 8) & 0xFF));
        data.push_back(static_cast<uint8_t>(trackLen & 0xFF));
        // track event data
        data.insert(data.end(), events.begin(), events.end());
    }

    return data;
}

// Build a variable-length value (MIDI VLQ)
static std::vector<uint8_t> vlq(uint32_t value) {
    std::vector<uint8_t> result;
    uint8_t buf[4];
    int count = 0;
    buf[count++] = static_cast<uint8_t>(value & 0x7F);
    value >>= 7;
    while (value > 0) {
        buf[count++] = static_cast<uint8_t>((value & 0x7F) | 0x80);
        value >>= 7;
    }
    for (int i = count - 1; i >= 0; i--)
        result.push_back(buf[i]);
    return result;
}

// Build delta-time + event combined
static std::vector<uint8_t> evt(uint32_t delta, const std::vector<uint8_t>& eventData) {
    auto d = vlq(delta);
    d.insert(d.end(), eventData.begin(), eventData.end());
    return d;
}

TEST_CASE("MidiParser: Format 0 single track with one note", "[midi_parser]") {
    // Build a Format 0 MIDI with: tempo, one C4 note (60), end-of-track
    std::vector<uint8_t> track;
    // delta=0: Set Tempo 500000 usec (120 BPM)
    auto tempoEvent = evt(0, {0xFF, 0x51, 0x03, 0x07, 0xA1, 0x20}); // 500000
    track.insert(track.end(), tempoEvent.begin(), tempoEvent.end());
    // delta=0: Note On ch0, note=60, vel=100
    auto noteOn = evt(0, {0x90, 60, 100});
    track.insert(track.end(), noteOn.begin(), noteOn.end());
    // delta=480 (quarter note): Note On ch0, note=60, vel=0 (note off)
    auto noteOff = evt(480, {0x90, 60, 0});
    track.insert(track.end(), noteOff.begin(), noteOff.end());
    // delta=0: End of Track
    auto eot = evt(0, {0xFF, 0x2F, 0x00});
    track.insert(track.end(), eot.begin(), eot.end());

    auto data = buildMinimalMidi(0, 1, 480, 500000, {track});
    std::string path = writeTempMidi(data);
    REQUIRE_FALSE(path.empty());

    MidiParser parser;
    ProjectData out;
    REQUIRE(parser.parse(path.c_str(), out));
    removeTempMidi(path);

    // Verify: one track at index 100
    REQUIRE(out.tracks.size() == 1);
    REQUIRE(out.tracks[0].index == 100);
    REQUIRE(out.tracks[0].eventCount == 1);
    REQUIRE(out.tracks[0].notes.size() == 1);

    auto& note = out.tracks[0].notes[0];
    REQUIRE(note.pitch == 60);
    REQUIRE(note.velocity == 100);
    // At 480 ticks/beat, 120 BPM: quarter note = 500000 usec = 0.5 sec
    REQUIRE(note.positionSec == Approx(0.0).margin(0.001));
    REQUIRE(note.lengthSec == Approx(0.5).margin(0.001));
}

TEST_CASE("MidiParser: Format 1 multi-track", "[midi_parser]") {
    // Track 0: tempo only
    std::vector<uint8_t> track0;
    auto tempoEvent = evt(0, {0xFF, 0x51, 0x03, 0x07, 0xA1, 0x20}); // 500000
    track0.insert(track0.end(), tempoEvent.begin(), tempoEvent.end());
    auto eot0 = evt(0, {0xFF, 0x2F, 0x00});
    track0.insert(track0.end(), eot0.begin(), eot0.end());

    // Track 1: note D4 (62) at delta=0, ends at delta=240 (eighth note)
    std::vector<uint8_t> track1;
    auto noteOn1 = evt(0, {0x90, 62, 80});
    track1.insert(track1.end(), noteOn1.begin(), noteOn1.end());
    auto noteOff1 = evt(240, {0x80, 62, 64});
    track1.insert(track1.end(), noteOff1.begin(), noteOff1.end());
    auto eot1 = evt(0, {0xFF, 0x2F, 0x00});
    track1.insert(track1.end(), eot1.begin(), eot1.end());

    auto data = buildMinimalMidi(1, 2, 480, 500000, {track0, track1});
    std::string path = writeTempMidi(data);
    REQUIRE_FALSE(path.empty());

    MidiParser parser;
    ProjectData out;
    REQUIRE(parser.parse(path.c_str(), out));
    removeTempMidi(path);

    // Track 0 is empty (tempo only), Track 1 has the note
    REQUIRE(out.tracks.size() == 1);
    REQUIRE(out.tracks[0].index == 101);    // 100 + trackIdx(1)
    REQUIRE(out.tracks[0].notes.size() == 1);

    auto& note = out.tracks[0].notes[0];
    REQUIRE(note.pitch == 62);
    REQUIRE(note.velocity == 80);
    // Eighth note at 120 BPM = 0.25 sec
    REQUIRE(note.positionSec == Approx(0.0).margin(0.001));
    REQUIRE(note.lengthSec == Approx(0.25).margin(0.001));
}

TEST_CASE("MidiParser: dynamic tempo change", "[midi_parser]") {
    // Build a track with tempo change then a note
    std::vector<uint8_t> track;
    // Initial tempo: 60 BPM (1000000 usec/beat)
    auto t1 = evt(0, {0xFF, 0x51, 0x03, 0x0F, 0x42, 0x40}); // 1000000
    track.insert(track.end(), t1.begin(), t1.end());
    // Note starts at delta=0
    auto noteOn = evt(0, {0x90, 64, 90});
    track.insert(track.end(), noteOn.begin(), noteOn.end());
    // After 240 ticks: tempo change to 120 BPM (500000 usec/beat)
    auto t2 = evt(240, {0xFF, 0x51, 0x03, 0x07, 0xA1, 0x20}); // 500000
    track.insert(track.end(), t2.begin(), t2.end());
    // Note ends at delta=240 more (from tempo change): total 480 ticks
    auto noteOff = evt(240, {0x80, 64, 0});
    track.insert(track.end(), noteOff.begin(), noteOff.end());
    auto eot = evt(0, {0xFF, 0x2F, 0x00});
    track.insert(track.end(), eot.begin(), eot.end());

    auto data = buildMinimalMidi(0, 1, 480, 500000, {track});
    std::string path = writeTempMidi(data);
    REQUIRE_FALSE(path.empty());

    MidiParser parser;
    ProjectData out;
    REQUIRE(parser.parse(path.c_str(), out));
    removeTempMidi(path);

    REQUIRE(out.tracks.size() == 1);
    auto& note = out.tracks[0].notes[0];
    REQUIRE(note.pitch == 64);

    // At 60 BPM: 240 ticks = 0.5 sec (first segment: 1000000/480 = 2083.33 usec/tick)
    // At 120 BPM: 240 ticks = 0.25 sec (second segment: 500000/480 = 1041.67 usec/tick)
    // Total note length = 0.75 sec
    REQUIRE(note.positionSec == Approx(0.0).margin(0.001));
    REQUIRE(note.lengthSec == Approx(0.75).margin(0.002));
}

TEST_CASE("MidiParser: pending notes closed at track end", "[midi_parser]") {
    // Note On without matching Note Off — should be closed at end of track
    std::vector<uint8_t> track;
    auto noteOn = evt(0, {0x90, 72, 100});
    track.insert(track.end(), noteOn.begin(), noteOn.end());
    // Some other event to advance time
    auto noteOn2 = evt(480, {0x90, 74, 80});
    track.insert(track.end(), noteOn2.begin(), noteOn2.end());
    auto noteOff2 = evt(240, {0x80, 74, 0});
    track.insert(track.end(), noteOff2.begin(), noteOff2.end());
    auto eot = evt(0, {0xFF, 0x2F, 0x00});
    track.insert(track.end(), eot.begin(), eot.end());

    auto data = buildMinimalMidi(0, 1, 480, 500000, {track});
    std::string path = writeTempMidi(data);
    REQUIRE_FALSE(path.empty());

    MidiParser parser;
    ProjectData out;
    REQUIRE(parser.parse(path.c_str(), out));
    removeTempMidi(path);

    REQUIRE(out.tracks.size() == 1);
    REQUIRE(out.tracks[0].notes.size() == 2);

    // Notes are resolved in Note-Off encounter order (not start-time order).
    // Find each note by pitch to avoid depending on internal ordering.
    const MidiNote* n72 = nullptr;
    const MidiNote* n74 = nullptr;
    for (auto& n : out.tracks[0].notes) {
        if (n.pitch == 72) n72 = &n;
        if (n.pitch == 74) n74 = &n;
    }
    REQUIRE(n72 != nullptr);
    REQUIRE(n74 != nullptr);

    // Note 72: started at 0, no Note Off -> length = absTimeSec (end) - 0
    // Track ends at 480+240 = 720 ticks, at 120 BPM that's 0.75 sec
    REQUIRE(n72->positionSec == Approx(0.0).margin(0.001));
    REQUIRE(n72->lengthSec == Approx(0.75).margin(0.001));

    // Note 74: started at 480 ticks (0.5 sec), ended at 720 ticks (0.75 sec)
    REQUIRE(n74->positionSec == Approx(0.5).margin(0.001));
    REQUIRE(n74->lengthSec == Approx(0.25).margin(0.001));
}

TEST_CASE("MidiParser: SMPTE division returns false", "[midi_parser]") {
    // Build a MIDI with negative division (SMPTE)
    std::vector<uint8_t> track;
    auto eot = evt(0, {0xFF, 0x2F, 0x00});
    track.insert(track.end(), eot.begin(), eot.end());

    auto data = buildMinimalMidi(0, 1, -24, 500000, {track});  // -24 = SMPTE 24fps
    std::string path = writeTempMidi(data);
    REQUIRE_FALSE(path.empty());

    MidiParser parser;
    ProjectData out;
    REQUIRE_FALSE(parser.parse(path.c_str(), out));
    removeTempMidi(path);
}

TEST_CASE("MidiParser: running status", "[midi_parser]") {
    // Two Note On events using running status (omit status byte for second note)
    std::vector<uint8_t> track;
    // Note On ch0, note=60, vel=100 (status=0x90)
    auto noteOn1 = evt(0, {0x90, 60, 100});
    track.insert(track.end(), noteOn1.begin(), noteOn1.end());
    // delta=0, running status: just data bytes
    auto delta2 = vlq(0);
    track.insert(track.end(), delta2.begin(), delta2.end());
    track.push_back(62);  // note (no status byte — running status)
    track.push_back(90);  // velocity
    // Note Off for both at delta=480
    auto noteOff1 = evt(480, {0x80, 60, 0});
    track.insert(track.end(), noteOff1.begin(), noteOff1.end());
    auto noteOff2 = evt(0, {0x80, 62, 0});
    track.insert(track.end(), noteOff2.begin(), noteOff2.end());
    auto eot = evt(0, {0xFF, 0x2F, 0x00});
    track.insert(track.end(), eot.begin(), eot.end());

    auto data = buildMinimalMidi(0, 1, 480, 500000, {track});
    std::string path = writeTempMidi(data);
    REQUIRE_FALSE(path.empty());

    MidiParser parser;
    ProjectData out;
    REQUIRE(parser.parse(path.c_str(), out));
    removeTempMidi(path);

    REQUIRE(out.tracks.size() == 1);
    REQUIRE(out.tracks[0].notes.size() == 2);
    REQUIRE(out.tracks[0].notes[0].pitch == 60);
    REQUIRE(out.tracks[0].notes[1].pitch == 62);
}

TEST_CASE("MidiParser: Note On with velocity 0 treated as Note Off", "[midi_parser]") {
    // Note On vel=0 should act as Note Off
    std::vector<uint8_t> track;
    auto noteOn = evt(0, {0x90, 66, 100});
    track.insert(track.end(), noteOn.begin(), noteOn.end());
    auto noteOff = evt(240, {0x90, 66, 0});  // Note On with vel=0
    track.insert(track.end(), noteOff.begin(), noteOff.end());
    auto eot = evt(0, {0xFF, 0x2F, 0x00});
    track.insert(track.end(), eot.begin(), eot.end());

    auto data = buildMinimalMidi(0, 1, 480, 500000, {track});
    std::string path = writeTempMidi(data);
    REQUIRE_FALSE(path.empty());

    MidiParser parser;
    ProjectData out;
    REQUIRE(parser.parse(path.c_str(), out));
    removeTempMidi(path);

    REQUIRE(out.tracks.size() == 1);
    REQUIRE(out.tracks[0].notes.size() == 1);
    REQUIRE(out.tracks[0].notes[0].pitch == 66);
}

TEST_CASE("MidiParser: BPM from default tempo", "[midi_parser]") {
    // No Set Tempo event — should default to 120 BPM
    std::vector<uint8_t> track;
    auto noteOn = evt(0, {0x90, 60, 100});
    track.insert(track.end(), noteOn.begin(), noteOn.end());
    auto noteOff = evt(480, {0x80, 60, 0});
    track.insert(track.end(), noteOff.begin(), noteOff.end());
    auto eot = evt(0, {0xFF, 0x2F, 0x00});
    track.insert(track.end(), eot.begin(), eot.end());

    auto data = buildMinimalMidi(0, 1, 480, 500000, {track});
    std::string path = writeTempMidi(data);
    REQUIRE_FALSE(path.empty());

    MidiParser parser;
    ProjectData out;
    REQUIRE(parser.parse(path.c_str(), out));
    removeTempMidi(path);

    REQUIRE(out.bpm == Approx(120.0).margin(0.01));
}

TEST_CASE("MidiParser: min/max pitch expands when equal", "[midi_parser]") {
    // Single note should result in min = pitch-1, max = pitch+1 (G5 rule)
    std::vector<uint8_t> track;
    auto noteOn = evt(0, {0x90, 60, 100});
    track.insert(track.end(), noteOn.begin(), noteOn.end());
    auto noteOff = evt(480, {0x80, 60, 0});
    track.insert(track.end(), noteOff.begin(), noteOff.end());
    auto eot = evt(0, {0xFF, 0x2F, 0x00});
    track.insert(track.end(), eot.begin(), eot.end());

    auto data = buildMinimalMidi(0, 1, 480, 500000, {track});
    std::string path = writeTempMidi(data);
    REQUIRE_FALSE(path.empty());

    MidiParser parser;
    ProjectData out;
    REQUIRE(parser.parse(path.c_str(), out));
    removeTempMidi(path);

    REQUIRE(out.tracks.size() == 1);
    REQUIRE(out.tracks[0].minPitch == 59);
    REQUIRE(out.tracks[0].maxPitch == 61);
}