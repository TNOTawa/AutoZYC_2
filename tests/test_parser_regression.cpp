// AutoZYC — Parser regression test suite
// Included by main.cpp (catch.hpp already available)
//
// This file provides comprehensive regression tests for both RPP and MIDI parsers.
// It reuses static helper functions from test_midi_parser.cpp (buildMinimalMidi,
// vlq, evt, writeTempMidi, removeTempMidi) which are visible since both files
// are #included into main.cpp as a single translation unit.
#include "src/core/RppParser.h"
#include "src/core/MidiParser.h"
#include "src/core/EventTypes.h"
#include <fstream>
#include <cstring>
#include <cstdio>
#include <sstream>
#include <cmath>

// ============================================================
// Helper: write text content to a temp file
// ============================================================
static std::string writeTempTextFile(const std::string& filename, const std::string& content)
{
    std::ofstream f(filename, std::ios::binary);
    if (!f) return "";
    f.write(content.data(), content.size());
    f.close();
    return filename;
}

// ============================================================
// Section A: RPP 解析回归测试
// ============================================================

TEST_CASE("RPP regression: standard single track with ITEM", "[rpp_regression]")
{
    RppParser parser;
    const char* rpp =
        "<TRACK {REGR-TEST-A001}\n"
        "<ITEM\n"
        "POSITION 3.5\n"
        "LENGTH 1.25\n"
        "PLAYRATE 1.0 1 2.0\n"
        ">\n"
        ">\n";

    REQUIRE(parser.parseContent(rpp) == true);
    REQUIRE(parser.getTrackCount() == 1);
    REQUIRE(parser.getEvents().size() == 1);

    auto& evt = parser.getEvents()[0];
    REQUIRE(evt.track == 1);
    REQUIRE(evt.position_sec == Approx(3.5).margin(0.001));
    REQUIRE(evt.length_sec == Approx(1.25).margin(0.001));
    REQUIRE(evt.pitch_shift == Approx(2.0).margin(0.001));
    REQUIRE(evt.velocity == 0.0);
}

TEST_CASE("RPP regression: multi-track with items on each track", "[rpp_regression]")
{
    RppParser parser;
    const char* rpp =
        "<TRACK {TRK-001}\n"
        "<ITEM\nPOSITION 0.0\nLENGTH 1.0\nPLAYRATE 1.0 1 0.0\n>\n"
        ">\n"
        "<TRACK {TRK-002}\n"
        "<ITEM\nPOSITION 1.0\nLENGTH 2.0\nPLAYRATE 1.0 1 5.0\n>\n"
        ">\n"
        "<TRACK {TRK-003}\n"
        "<ITEM\nPOSITION 3.0\nLENGTH 3.0\nPLAYRATE 1.0 1 -2.5\n>\n"
        ">\n";

    REQUIRE(parser.parseContent(rpp) == true);
    REQUIRE(parser.getTrackCount() == 3);
    REQUIRE(parser.getEvents().size() == 3);

    REQUIRE(parser.getEvents()[0].track == 1);
    REQUIRE(parser.getEvents()[0].position_sec == 0.0);
    REQUIRE(parser.getEvents()[0].pitch_shift == 0.0);

    REQUIRE(parser.getEvents()[1].track == 2);
    REQUIRE(parser.getEvents()[1].position_sec == 1.0);
    REQUIRE(parser.getEvents()[1].pitch_shift == 5.0);

    REQUIRE(parser.getEvents()[2].track == 3);
    REQUIRE(parser.getEvents()[2].position_sec == 3.0);
    REQUIRE(parser.getEvents()[2].pitch_shift == -2.5);
}

TEST_CASE("RPP regression: track with no ITEM returns 0 events", "[rpp_regression]")
{
    RppParser parser;
    const char* rpp =
        "<TRACK {EMPTY-EMPTY-EMPTY}\n"
        ">\n";

    REQUIRE(parser.parseContent(rpp) == true);
    REQUIRE(parser.getTrackCount() == 1);
    REQUIRE(parser.getEvents().empty());
}

TEST_CASE("RPP regression: SOURCE MIDI with multiple overlapping notes", "[rpp_regression]")
{
    RppParser parser;
    // Three overlapping MIDI notes with different timing in one SOURCE MIDI block
    const char* rpp =
        "<TRACK {RPP-MULTI-MIDI}\n"
        "<ITEM\n"
        "POSITION 0.0\n"
        "LENGTH 4.0\n"
        "<SOURCE MIDI\n"
        "HASDATA 960\n"
        "e 0 90 3c 64\n"
        "e 0 90 40 50\n"
        "e 1E0 80 3c 00\n"
        "e 3C0 80 40 00\n"
        "e 1E0 90 48 7f\n"
        "e 3C0 80 48 00\n"
        ">\n"
        ">\n"
        ">\n";

    REQUIRE(parser.parseContent(rpp) == true);
    // 3 MIDI notes + 1 ITEM = 4 events total
    REQUIRE(parser.getEvents().size() == 4);

    // Note C4 (60): offset 0 → 480 ticks (hex 1E0 = 480 dec)
    // At 960 ticks/qn, 120 BPM: 480 * 500000/1e6/960 = 0.25 sec
    auto& n1 = parser.getEvents()[0];
    REQUIRE(n1.pitch_shift == Approx(60.0 - 69.0).margin(0.001));  // -9
    REQUIRE(n1.velocity == Approx(100.0).margin(0.001));
    REQUIRE(n1.position_sec == Approx(0.0).margin(0.001));
    REQUIRE(n1.length_sec == Approx(0.25).margin(0.001));
    REQUIRE(n1.track == 1);

    // Note E4 (64): offset 0 → 960 ticks (hex 3C0 = 960 dec)
    // 960 * 500000/1e6/960 = 0.5 sec
    auto& n2 = parser.getEvents()[1];
    REQUIRE(n2.pitch_shift == Approx(64.0 - 69.0).margin(0.001));  // -5
    REQUIRE(n2.position_sec == Approx(0.0).margin(0.001));
    REQUIRE(n2.length_sec == Approx(0.5).margin(0.001));

    // Note G5 (72): offset 480 (hex 1E0) → 960 (hex 3C0)
    // position = 480 * 500000/1e6/960 = 0.25 sec, length = 0.25 sec
    auto& n3 = parser.getEvents()[2];
    REQUIRE(n3.pitch_shift == Approx(72.0 - 69.0).margin(0.001));  // +3
    REQUIRE(n3.position_sec == Approx(0.25).margin(0.001));
    REQUIRE(n3.length_sec == Approx(0.25).margin(0.001));

    // ITEM event (last)
    auto& item = parser.getEvents()[3];
    REQUIRE(item.track == 1);
    REQUIRE(item.position_sec == 0.0);
    REQUIRE(item.length_sec == 4.0);
    REQUIRE(item.pitch_shift == 0.0);
}

// ============================================================
// Section B: MIDI 解析回归测试
// ============================================================

TEST_CASE("MIDI regression: Format 0 with multiple sequential notes", "[midi_regression]")
{
    // Three sequential notes: C4(60), E4(64), G4(67)
    std::vector<uint8_t> track;
    auto n1on = evt(0, {0x90, 60, 100});
    track.insert(track.end(), n1on.begin(), n1on.end());
    auto n1off = evt(480, {0x80, 60, 0});
    track.insert(track.end(), n1off.begin(), n1off.end());

    auto n2on = evt(0, {0x90, 64, 80});
    track.insert(track.end(), n2on.begin(), n2on.end());
    auto n2off = evt(480, {0x80, 64, 0});
    track.insert(track.end(), n2off.begin(), n2off.end());

    auto n3on = evt(0, {0x90, 67, 90});
    track.insert(track.end(), n3on.begin(), n3on.end());
    auto n3off = evt(960, {0x80, 67, 0});
    track.insert(track.end(), n3off.begin(), n3off.end());

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
    REQUIRE(out.tracks[0].notes.size() == 3);

    // Note C4: quarter note at 120 BPM = 0.5 sec
    REQUIRE(out.tracks[0].notes[0].pitch == 60);
    REQUIRE(out.tracks[0].notes[0].positionSec == Approx(0.0).margin(0.001));
    REQUIRE(out.tracks[0].notes[0].lengthSec == Approx(0.5).margin(0.001));

    // Note E4: quarter note at 120 BPM = 0.5 sec
    REQUIRE(out.tracks[0].notes[1].pitch == 64);
    REQUIRE(out.tracks[0].notes[1].positionSec == Approx(0.5).margin(0.001));
    REQUIRE(out.tracks[0].notes[1].lengthSec == Approx(0.5).margin(0.001));

    // Note G4: half note at 120 BPM = 1.0 sec
    REQUIRE(out.tracks[0].notes[2].pitch == 67);
    REQUIRE(out.tracks[0].notes[2].positionSec == Approx(1.0).margin(0.001));
    REQUIRE(out.tracks[0].notes[2].lengthSec == Approx(1.0).margin(0.001));

    // Default BPM = 120
    REQUIRE(out.bpm == Approx(120.0).margin(0.01));
}

TEST_CASE("MIDI regression: Format 1 with tempo-only track and note track", "[midi_regression]")
{
    // Track 0: tempo meta events only (no notes)
    std::vector<uint8_t> track0;
    auto t0 = evt(0, {0xFF, 0x51, 0x03, 0x07, 0xA1, 0x20});  // 500000 = 120 BPM
    track0.insert(track0.end(), t0.begin(), t0.end());
    auto eot0 = evt(0, {0xFF, 0x2F, 0x00});
    track0.insert(track0.end(), eot0.begin(), eot0.end());

    // Track 1: one note
    std::vector<uint8_t> track1;
    auto nOn = evt(0, {0x90, 60, 100});
    track1.insert(track1.end(), nOn.begin(), nOn.end());
    auto nOff = evt(480, {0x80, 60, 0});
    track1.insert(track1.end(), nOff.begin(), nOff.end());
    auto eot1 = evt(0, {0xFF, 0x2F, 0x00});
    track1.insert(track1.end(), eot1.begin(), eot1.end());

    auto data = buildMinimalMidi(1, 2, 480, 500000, {track0, track1});
    std::string path = writeTempMidi(data);
    REQUIRE_FALSE(path.empty());

    MidiParser parser;
    ProjectData out;
    REQUIRE(parser.parse(path.c_str(), out));
    removeTempMidi(path);

    // Track 0 (tempo-only) should be skipped; only Track 1 with notes survives
    REQUIRE(out.tracks.size() == 1);
    REQUIRE(out.tracks[0].index == 101);  // 100 + trackIdx(1)
    REQUIRE(out.tracks[0].notes.size() == 1);
    REQUIRE(out.tracks[0].notes[0].pitch == 60);
}

TEST_CASE("MIDI regression: dynamic Set Tempo changes note timing", "[midi_regression]")
{
    // Note starting at tempo 60 BPM, then tempo changes to 120 BPM mid-note
    std::vector<uint8_t> track;
    // Initial tempo: 60 BPM (1000000 usec/beat)
    auto t1 = evt(0, {0xFF, 0x51, 0x03, 0x0F, 0x42, 0x40});  // 1000000
    track.insert(track.end(), t1.begin(), t1.end());
    auto nOn = evt(0, {0x90, 60, 100});
    track.insert(track.end(), nOn.begin(), nOn.end());
    // Tempo change after 240 ticks at 60 BPM
    auto t2 = evt(240, {0xFF, 0x51, 0x03, 0x07, 0xA1, 0x20});  // 500000
    track.insert(track.end(), t2.begin(), t2.end());
    // Note off after 720 more ticks at 120 BPM
    auto nOff = evt(720, {0x80, 60, 0});
    track.insert(track.end(), nOff.begin(), nOff.end());
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

    auto& note = out.tracks[0].notes[0];
    REQUIRE(note.pitch == 60);
    REQUIRE(note.positionSec == Approx(0.0).margin(0.001));

    // 240 ticks at 60 BPM: 240 * (1000000/1e6/480) = 240 * 0.00208333 = 0.5 sec
    // 720 ticks at 120 BPM: 720 * (500000/1e6/480) = 720 * 0.00104167 = 0.75 sec
    // Total note length = 0.5 + 0.75 = 1.25 sec
    REQUIRE(note.lengthSec == Approx(1.25).margin(0.002));
}

TEST_CASE("MIDI regression: no Note On events returns no tracks", "[midi_regression]")
{
    // Track with only meta events (no Note On) — should produce 0 tracks
    std::vector<uint8_t> track;
    auto text = evt(0, {0xFF, 0x01, 0x03, 'F', 'o', 'o'});  // Text meta event
    track.insert(track.end(), text.begin(), text.end());
    auto eot = evt(0, {0xFF, 0x2F, 0x00});
    track.insert(track.end(), eot.begin(), eot.end());

    auto data = buildMinimalMidi(0, 1, 480, 500000, {track});
    std::string path = writeTempMidi(data);
    REQUIRE_FALSE(path.empty());

    MidiParser parser;
    ProjectData out;
    REQUIRE(parser.parse(path.c_str(), out));
    removeTempMidi(path);

    // No notes → no tracks (tempo-only tracks are skipped)
    REQUIRE(out.tracks.empty());
}

TEST_CASE("MIDI regression: empty file returns false", "[midi_regression]")
{
    {
        std::ofstream f("test_midi_empty_reg.mid", std::ios::binary);
        f.close();
    }

    MidiParser parser;
    ProjectData out;
    REQUIRE_FALSE(parser.parse("test_midi_empty_reg.mid", out));
    std::remove("test_midi_empty_reg.mid");
}

// ============================================================
// Section C: 边界情况测试
// ============================================================

TEST_CASE("Boundary: UTF-8 BOM prefix in RPP file (via parse())", "[boundary]")
{
    // Write RPP content with BOM prefix to temp file
    std::string content;
    content += static_cast<char>(0xEF);
    content += static_cast<char>(0xBB);
    content += static_cast<char>(0xBF);
    content += "<TRACK {BOM-REGR-TEST}\n";
    content += "<ITEM\nPOSITION 2.5\nLENGTH 1.5\nPLAYRATE 1.0 1 3.0\n>\n>\n";

    std::string path = writeTempTextFile("test_rpp_bom_reg.rpp", content);
    REQUIRE_FALSE(path.empty());

    RppParser parser;
    // parse() calls readFileContent() which strips UTF-8 BOM
    REQUIRE(parser.parse(path) == true);
    std::remove(path.c_str());

    REQUIRE(parser.getTrackCount() == 1);
    REQUIRE(parser.getEvents().size() == 1);
    REQUIRE(parser.getEvents()[0].position_sec == 2.5);
    REQUIRE(parser.getEvents()[0].length_sec == 1.5);
    REQUIRE(parser.getEvents()[0].pitch_shift == 3.0);
    REQUIRE(parser.getEvents()[0].track == 1);
}

TEST_CASE("Boundary: minimal valid MIDI file with no notes", "[boundary]")
{
    // Minimal MIDI: MThd header + single MTrk with only End-of-Track
    std::vector<uint8_t> track;
    auto eot = evt(0, {0xFF, 0x2F, 0x00});
    track.insert(track.end(), eot.begin(), eot.end());

    auto data = buildMinimalMidi(0, 1, 480, 500000, {track});
    std::string path = writeTempMidi(data);
    REQUIRE_FALSE(path.empty());

    MidiParser parser;
    ProjectData out;
    REQUIRE(parser.parse(path.c_str(), out));
    removeTempMidi(path);

    // Parses successfully, but no notes → no tracks
    REQUIRE(out.tracks.empty());
    // Default 120 BPM should still be set
    REQUIRE(out.bpm == Approx(120.0).margin(0.01));
}

TEST_CASE("Boundary: large RPP content with 100 items (structural)", "[boundary]")
{
    // Build RPP content with 100 items to stress-test parser structure
    std::ostringstream ss;
    ss << "<TRACK {LARGE-STRUCTURAL-TEST}\n";
    for (int i = 0; i < 100; i++) {
        ss << "<ITEM\n";
        ss << "POSITION " << (i * 0.5) << "\n";
        ss << "LENGTH 0.25\n";
        ss << ">\n";
    }
    ss << ">\n";

    RppParser parser;
    REQUIRE(parser.parseContent(ss.str()) == true);
    REQUIRE(parser.getTrackCount() == 1);
    REQUIRE(parser.getEvents().size() == 100);

    // Verify first item
    REQUIRE(parser.getEvents()[0].position_sec == 0.0);
    REQUIRE(parser.getEvents()[0].length_sec == 0.25);

    // Verify last item
    REQUIRE(parser.getEvents()[99].position_sec == Approx(49.5).margin(0.001));
    REQUIRE(parser.getEvents()[99].length_sec == 0.25);

    // Verify TrackMeta
    auto& meta = parser.getTrackMeta();
    REQUIRE(meta.count(1) == 1);
    REQUIRE(meta.at(1).item_count == 100);
    // All items have pitch_shift=0.0 (no PLAYRATE), so G5 guardrail triggers
    REQUIRE(meta.at(1).min_pitch == -1.0);
    REQUIRE(meta.at(1).max_pitch == 1.0);
}

// ============================================================
// Section D: 新旧解析器一致性测试
//
// These tests verify cross-parser consistency and deterministic
// re-parse behavior. Since the old .mod2 parser has been replaced
// by the new C++ parser, we verify:
//   1. RPP SOURCE MIDI and standalone MidiParser produce equivalent
//      timing for identical musical data (position tolerance 0.001s)
//   2. Parsing the same input twice produces identical results
//   3. Event counts are consistent across equivalent representations
// ============================================================

TEST_CASE("Consistency: RPP SOURCE MIDI vs MidiParser equivalent note timing", "[consistency]")
{
    // Create equivalent musical data in both parsers:
    // C4 (MIDI 60), quarter note at 120 BPM, 960 ticks/qn

    // RPP with SOURCE MIDI
    const char* rppContent =
        "<TRACK {CONSISTENCY-TEST}\n"
        "<ITEM\n"
        "POSITION 0.0\n"
        "LENGTH 2.0\n"
        "<SOURCE MIDI\n"
        "HASDATA 960\n"
        "e 0 90 3c 64\n"
        "e 3C0 80 3c 00\n"
        ">\n"
        ">\n"
        ">\n";

    RppParser rppParser;
    REQUIRE(rppParser.parseContent(rppContent) == true);
    REQUIRE(rppParser.getEvents().size() >= 2);  // MIDI note + ITEM

    // First event = MIDI note from SOURCE MIDI
    auto& rppNote = rppParser.getEvents()[0];

    // Equivalent MIDI file: Format 0, 960 ticks/qn, default 120 BPM, C4 quarter note
    std::vector<uint8_t> track;
    auto nOn = evt(0, {0x90, 60, 100});
    track.insert(track.end(), nOn.begin(), nOn.end());
    auto nOff = evt(960, {0x80, 60, 0});  // 960 ticks = quarter note at 960 ticks/qn
    track.insert(track.end(), nOff.begin(), nOff.end());
    auto eot = evt(0, {0xFF, 0x2F, 0x00});
    track.insert(track.end(), eot.begin(), eot.end());

    auto midiData = buildMinimalMidi(0, 1, 960, 500000, {track});
    std::string midiPath = writeTempMidi(midiData);
    REQUIRE_FALSE(midiPath.empty());

    MidiParser midiParser;
    ProjectData midiOut;
    REQUIRE(midiParser.parse(midiPath.c_str(), midiOut));
    removeTempMidi(midiPath);

    REQUIRE(midiOut.tracks.size() == 1);
    REQUIRE(midiOut.tracks[0].notes.size() == 1);
    auto& midiNote = midiOut.tracks[0].notes[0];

    // === Cross-parser consistency checks ===
    // 1. position_sec: both = 0.0 (note starts at beginning)
    REQUIRE(rppNote.position_sec == Approx(midiNote.positionSec).margin(0.001));

    // 2. length_sec: quarter note at 120 BPM = 0.5 sec
    REQUIRE(rppNote.length_sec == Approx(midiNote.lengthSec).margin(0.001));
    REQUIRE(rppNote.length_sec == Approx(0.5).margin(0.001));

    // 3. pitch equivalence:
    //    RPP pitch_shift = note - 69 (C4 = 60 → -9)
    //    MIDI pitch = raw MIDI note number (C4 = 60)
    //    So: rppNote.pitch_shift + 69.0 == midiNote.pitch
    REQUIRE((rppNote.pitch_shift + 69.0) == Approx(static_cast<double>(midiNote.pitch)).margin(0.001));
    REQUIRE(rppNote.pitch_shift == Approx(-9.0).margin(0.001));

    // 4. velocity: both store Note On velocity
    REQUIRE(rppNote.velocity == Approx(100.0).margin(0.001));
    REQUIRE(midiNote.velocity == 100);
}

TEST_CASE("Consistency: deterministic re-parse of RPP content", "[consistency]")
{
    const char* rpp =
        "<TRACK {DET-REPARSE-001}\n"
        "<ITEM\nPOSITION 1.0\nLENGTH 2.0\nPLAYRATE 1.0 1 4.0\n>\n"
        ">\n"
        "<TRACK {DET-REPARSE-002}\n"
        "<ITEM\nPOSITION 3.0\nLENGTH 1.5\nPLAYRATE 1.0 1 -1.0\n>\n"
        ">\n";

    // First parse
    RppParser p1;
    p1.parseContent(rpp);

    // Second parse (fresh parser, same input)
    RppParser p2;
    p2.parseContent(rpp);

    // Identical event counts
    REQUIRE(p1.getEvents().size() == p2.getEvents().size());
    REQUIRE(p1.getTrackCount() == p2.getTrackCount());

    // Each event is identical within tolerance
    for (size_t i = 0; i < p1.getEvents().size(); i++) {
        auto& e1 = p1.getEvents()[i];
        auto& e2 = p2.getEvents()[i];
        REQUIRE(e1.track == e2.track);
        REQUIRE(e1.position_sec == Approx(e2.position_sec).margin(0.001));
        REQUIRE(e1.length_sec == Approx(e2.length_sec).margin(0.001));
        REQUIRE(e1.pitch_shift == Approx(e2.pitch_shift).margin(0.001));
        REQUIRE(e1.velocity == Approx(e2.velocity).margin(0.001));
    }

    // TrackMeta is identical
    auto& meta1 = p1.getTrackMeta();
    auto& meta2 = p2.getTrackMeta();
    REQUIRE(meta1.size() == meta2.size());
    for (auto& kv : meta1) {
        REQUIRE(meta2.count(kv.first) == 1);
        auto& tm1 = kv.second;
        auto& tm2 = meta2.at(kv.first);
        REQUIRE(tm1.min_pitch == Approx(tm2.min_pitch).margin(0.001));
        REQUIRE(tm1.max_pitch == Approx(tm2.max_pitch).margin(0.001));
        REQUIRE(tm1.item_count == tm2.item_count);
    }
}

TEST_CASE("Consistency: event count and timing across equivalent RPP/MIDI inputs", "[consistency]")
{
    // RPP SOURCE MIDI with 5 short notes (eighth notes at 120 BPM)
    const char* rpp =
        "<TRACK {ECOUNT-ECOUNT-ECOUNT}\n"
        "<ITEM\n"
        "POSITION 0.0\n"
        "LENGTH 4.0\n"
        "<SOURCE MIDI\n"
        "HASDATA 960\n"
        "e 0 90 3c 64\n"
        "e 1E0 80 3c 00\n"
        "e 0 90 40 50\n"
        "e 1E0 80 40 00\n"
        "e 0 90 48 7f\n"
        "e 1E0 80 48 00\n"
        "e 0 90 4c 40\n"
        "e 1E0 80 4c 00\n"
        "e 0 90 50 60\n"
        "e 1E0 80 50 00\n"
        ">\n"
        ">\n"
        ">\n";

    RppParser rppParser;
    REQUIRE(rppParser.parseContent(rpp) == true);

    // RPP: 5 MIDI notes + 1 ITEM = 6 events total
    size_t rppTotalEvents = rppParser.getEvents().size();
    REQUIRE(rppTotalEvents == 6);

    // Equivalent MIDI file: 5 simultaneous notes in Format 0
    // All Note On at delta=0, all Note Off at delta=480 (simultaneous, matching RPP)
    std::vector<uint8_t> track;
    int pitches[] = {60, 64, 72, 76, 80};
    for (int p : pitches) {
        auto on = evt(0, {0x90, static_cast<uint8_t>(p), 100});
        track.insert(track.end(), on.begin(), on.end());
    }
    for (int p : pitches) {
        auto off = evt(0, {0x80, static_cast<uint8_t>(p), 0});
        track.insert(track.end(), off.begin(), off.end());
    }
    // Adjust: first Note Off should use delta=480, rest use delta=0
    // Rebuild: [ON1, ON2, ..., ON5, delta=480 OFF1, OFF2, ..., OFF5]
    std::vector<uint8_t> track2;
    for (int p : pitches) {
        auto on = evt(0, {0x90, static_cast<uint8_t>(p), 100});
        track2.insert(track2.end(), on.begin(), on.end());
    }
    {
        auto off1 = evt(480, {0x80, static_cast<uint8_t>(pitches[0]), 0});
        track2.insert(track2.end(), off1.begin(), off1.end());
    }
    for (int i = 1; i < 5; i++) {
        auto off = evt(0, {0x80, static_cast<uint8_t>(pitches[i]), 0});
        track2.insert(track2.end(), off.begin(), off.end());
    }
    track = track2;
    auto eot = evt(0, {0xFF, 0x2F, 0x00});
    track.insert(track.end(), eot.begin(), eot.end());

    auto midiData = buildMinimalMidi(0, 1, 960, 500000, {track});
    std::string midiPath = writeTempMidi(midiData);
    REQUIRE_FALSE(midiPath.empty());

    MidiParser midiParser;
    ProjectData midiOut;
    REQUIRE(midiParser.parse(midiPath.c_str(), midiOut));
    removeTempMidi(midiPath);

    REQUIRE(midiOut.tracks.size() == 1);
    REQUIRE(midiOut.tracks[0].notes.size() == 5);

    // RPP MIDI note count (= total - 1 for the ITEM event) should equal MidiParser note count
    REQUIRE(rppTotalEvents - 1 == midiOut.tracks[0].notes.size());

    // Verify each note's timing matches between parsers
    for (size_t i = 0; i < midiOut.tracks[0].notes.size(); i++) {
        auto& rppN = rppParser.getEvents()[i];
        auto& midN = midiOut.tracks[0].notes[i];

        REQUIRE(rppN.position_sec == Approx(midN.positionSec).margin(0.001));
        REQUIRE(rppN.length_sec == Approx(midN.lengthSec).margin(0.001));
        REQUIRE((rppN.pitch_shift + 69.0) == Approx(static_cast<double>(midN.pitch)).margin(0.001));
    }
}





