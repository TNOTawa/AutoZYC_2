// AutoZYC — DataManager unit tests
// Included by main.cpp (catch.hpp already available)
#include "src/core/DataManager.h"
#include <fstream>
#include <cstdio>
#include <cmath>

// ============================================================
// Helper: write RPP content to a temp file
// ============================================================

static std::string writeTempRpp(const std::string& content, const std::string& suffix = "")
{
    std::string path = "test_dm_temp" + suffix + ".rpp";
    std::ofstream file(path);
    if (!file) return "";
    file << content;
    file.close();
    return path;
}

static void removeTempFile(const std::string& path)
{
    if (!path.empty()) std::remove(path.c_str());
}

// ============================================================
// Basic parse tests
// ============================================================

TEST_CASE("DataManager: non-existent file returns false", "[data_manager]")
{
    DataManager dm;
    REQUIRE_FALSE(dm.parseFile("nonexistent_file_xyz.rpp"));
    REQUIRE(dm.getTrackCount() == 0);
}

TEST_CASE("DataManager: unsupported extension returns false", "[data_manager]")
{
    DataManager dm;
    REQUIRE_FALSE(dm.parseFile("test.txt"));
    REQUIRE(dm.getTrackCount() == 0);
}

TEST_CASE("DataManager: empty RPP file returns false", "[data_manager]")
{
    // RppParser treats empty content as invalid (file not found or empty)
    std::string path = writeTempRpp("");
    REQUIRE_FALSE(path.empty());

    DataManager dm;
    bool result = dm.parseFile(path);
    removeTempFile(path);

    REQUIRE_FALSE(result);
    REQUIRE(dm.getTrackCount() == 0);
}

// ============================================================
// RPP file parsing
// ============================================================

TEST_CASE("DataManager: RPP single track with one item", "[data_manager]")
{
    const char* rpp =
        "<TRACK {AAAA-AAAA-AAAA-AAAA}\n"
        "<ITEM\n"
        "POSITION 1.5\n"
        "LENGTH 0.75\n"
        ">\n"
        ">\n";

    std::string path = writeTempRpp(rpp);
    REQUIRE_FALSE(path.empty());

    DataManager dm;
    REQUIRE(dm.parseFile(path));
    removeTempFile(path);

    REQUIRE(dm.getTrackCount() == 1);
    REQUIRE(dm.getEventCount(1) == 1);

    const auto& events = dm.getEvents(1);
    REQUIRE(events.size() == 1);
    REQUIRE(events[0].track == 1);
    REQUIRE(events[0].position_sec == 1.5);
    REQUIRE(events[0].length_sec == 0.75);
}

TEST_CASE("DataManager: RPP multiple tracks", "[data_manager]")
{
    const char* rpp =
        "<TRACK {AAA}\n"
        "<ITEM\n"
        "POSITION 0.0\n"
        "LENGTH 1.0\n"
        ">\n"
        ">\n"
        "<TRACK {BBB}\n"
        "<ITEM\n"
        "POSITION 2.0\n"
        "LENGTH 3.0\n"
        ">\n"
        ">\n";

    std::string path = writeTempRpp(rpp);
    REQUIRE_FALSE(path.empty());

    DataManager dm;
    REQUIRE(dm.parseFile(path));
    removeTempFile(path);

    REQUIRE(dm.getTrackCount() == 2);
    REQUIRE(dm.getEventCount(1) == 1);
    REQUIRE(dm.getEventCount(2) == 1);

    REQUIRE(dm.getEvents(1)[0].track == 1);
    REQUIRE(dm.getEvents(2)[0].track == 2);
}

TEST_CASE("DataManager: RPP getEvents for unknown track returns empty", "[data_manager]")
{
    const char* rpp = "<TRACK {AAA}\n<ITEM\nPOSITION 0.0\nLENGTH 1.0\n>\n>\n";
    std::string path = writeTempRpp(rpp);
    REQUIRE_FALSE(path.empty());

    DataManager dm;
    REQUIRE(dm.parseFile(path));
    removeTempFile(path);

    // Track 99 does not exist
    const auto& events = dm.getEvents(99);
    REQUIRE(events.empty());
}

// ============================================================
// MIDI file parsing
// ============================================================

// Reuse the buildMinimalMidi/vlq/evt/writeTempMidi/removeTempMidi helpers
// from test_midi_parser.cpp (same TU via main.cpp includes)

TEST_CASE("DataManager: MIDI single track with one note", "[data_manager]")
{
    // Build a Format 0 MIDI with note C4 (60), duration quarter note
    std::vector<uint8_t> track;
    // delta=0: Set Tempo 500000 usec (120 BPM)
    auto tempoEvent = evt(0, {0xFF, 0x51, 0x03, 0x07, 0xA1, 0x20});
    track.insert(track.end(), tempoEvent.begin(), tempoEvent.end());
    // delta=0: Note On ch0, note=60, vel=100
    auto noteOn = evt(0, {0x90, 60, 100});
    track.insert(track.end(), noteOn.begin(), noteOn.end());
    // delta=480: Note On ch0, note=60, vel=0 (note off)
    auto noteOff = evt(480, {0x90, 60, 0});
    track.insert(track.end(), noteOff.begin(), noteOff.end());
    // delta=0: End of Track
    auto eot = evt(0, {0xFF, 0x2F, 0x00});
    track.insert(track.end(), eot.begin(), eot.end());

    auto data = buildMinimalMidi(0, 1, 480, 500000, {track});
    std::string midPath = writeTempMidi(data);
    REQUIRE_FALSE(midPath.empty());

    DataManager dm;
    REQUIRE(dm.parseFile(midPath));
    removeTempMidi(midPath);

    // MIDI track index 0 -> stored as 100 (G2 offset)
    REQUIRE(dm.getTrackCount() == 1);
    REQUIRE(dm.getEventCount(100) == 1);

    const auto& events = dm.getEvents(100);
    REQUIRE(events.size() == 1);

    // pitch_shift = note - 60.0 -> C4(60) - 60 = 0.0
    REQUIRE(events[0].pitch_shift == Approx(0.0).margin(0.001));
    // velocity preserved
    REQUIRE(events[0].velocity == Approx(100.0).margin(0.001));
    // position and length at 120 BPM, 480 ticks/beat
    // quarter = 0.5 sec
    REQUIRE(events[0].position_sec == Approx(0.0).margin(0.001));
    REQUIRE(events[0].length_sec == Approx(0.5).margin(0.001));
    // track = 100 (G2 offset)
    REQUIRE(events[0].track == 100);
}

// ============================================================
// getTrackMeta() tests
// ============================================================

TEST_CASE("DataManager: getTrackMeta returns default for unknown track", "[data_manager]")
{
    DataManager dm;
    TrackMeta meta = dm.getTrackMeta(1);
    REQUIRE(meta.min_pitch == -12.0);
    REQUIRE(meta.max_pitch == 12.0);
    REQUIRE(meta.item_count == 0);
}

TEST_CASE("DataManager: getTrackMeta from RPP file", "[data_manager]")
{
    const char* rpp =
        "<TRACK {XXX}\n"
        "<ITEM\n"
        "POSITION 0.0\n"
        "LENGTH 1.0\n"
        "PLAYRATE 1.0 1 -5.0\n"
        ">\n"
        "<ITEM\n"
        "POSITION 2.0\n"
        "LENGTH 1.0\n"
        "PLAYRATE 1.0 1 7.0\n"
        ">\n"
        ">\n";

    std::string path = writeTempRpp(rpp);
    REQUIRE_FALSE(path.empty());

    DataManager dm;
    REQUIRE(dm.parseFile(path));
    removeTempFile(path);

    TrackMeta meta = dm.getTrackMeta(1);
    REQUIRE(meta.item_count == 2);
    REQUIRE(meta.min_pitch == -5.0);
    REQUIRE(meta.max_pitch == 7.0);
}

// ============================================================
// getCurrentItem() binary search tests
// ============================================================

TEST_CASE("DataManager: getCurrentItem on empty track", "[data_manager]")
{
    DataManager dm;
    CounterState cs = dm.getCurrentItem(1, 0.0);
    REQUIRE(cs.count == 0);
    REQUIRE(cs.is_playing == false);
    REQUIRE(cs.position_sec == 0.0);
    REQUIRE(cs.length_sec == 0.0);
}

TEST_CASE("DataManager: getCurrentItem before first event", "[data_manager]")
{
    const char* rpp =
        "<TRACK {AAA}\n"
        "<ITEM\n"
        "POSITION 5.0\n"
        "LENGTH 1.0\n"
        ">\n"
        ">\n";

    std::string path = writeTempRpp(rpp);
    REQUIRE_FALSE(path.empty());

    DataManager dm;
    REQUIRE(dm.parseFile(path));
    removeTempFile(path);

    // timeSec < first event position
    CounterState cs = dm.getCurrentItem(1, 2.0);
    REQUIRE(cs.count == 0);
    REQUIRE(cs.is_playing == false);
}

TEST_CASE("DataManager: getCurrentItem at event position", "[data_manager]")
{
    const char* rpp =
        "<TRACK {BBB}\n"
        "<ITEM\n"
        "POSITION 5.0\n"
        "LENGTH 2.0\n"
        "PLAYRATE 1.0 1 3.0\n"
        ">\n"
        ">\n";

    std::string path = writeTempRpp(rpp);
    REQUIRE_FALSE(path.empty());

    DataManager dm;
    REQUIRE(dm.parseFile(path));
    removeTempFile(path);

    // Exactly at event start
    CounterState cs = dm.getCurrentItem(1, 5.0);
    REQUIRE(cs.count == 1);
    REQUIRE(cs.is_playing == true);
    REQUIRE(cs.position_sec == 5.0);
    REQUIRE(cs.length_sec == 2.0);
    REQUIRE(cs.pitch_shift == 3.0);
}

TEST_CASE("DataManager: getCurrentItem inside event", "[data_manager]")
{
    const char* rpp =
        "<TRACK {CCC}\n"
        "<ITEM\n"
        "POSITION 5.0\n"
        "LENGTH 2.0\n"
        "PLAYRATE 1.0 1 3.0\n"
        ">\n"
        ">\n";

    std::string path = writeTempRpp(rpp);
    REQUIRE_FALSE(path.empty());

    DataManager dm;
    REQUIRE(dm.parseFile(path));
    removeTempFile(path);

    // Inside the event
    CounterState cs = dm.getCurrentItem(1, 6.0);
    REQUIRE(cs.count == 1);
    REQUIRE(cs.is_playing == true);
    REQUIRE(cs.position_sec == 5.0);
    REQUIRE(cs.length_sec == 2.0);
    REQUIRE(cs.pitch_shift == 3.0);
}

TEST_CASE("DataManager: getCurrentItem after event ends", "[data_manager]")
{
    const char* rpp =
        "<TRACK {DDD}\n"
        "<ITEM\n"
        "POSITION 5.0\n"
        "LENGTH 2.0\n"
        ">\n"
        ">\n";

    std::string path = writeTempRpp(rpp);
    REQUIRE_FALSE(path.empty());

    DataManager dm;
    REQUIRE(dm.parseFile(path));
    removeTempFile(path);

    // After event ended
    CounterState cs = dm.getCurrentItem(1, 8.0);
    REQUIRE(cs.count == 1);
    REQUIRE(cs.is_playing == false);  // past the end
    REQUIRE(cs.position_sec == 5.0);
    REQUIRE(cs.length_sec == 2.0);
}

TEST_CASE("DataManager: getCurrentItem with multiple events", "[data_manager]")
{
    const char* rpp =
        "<TRACK {EEE}\n"
        "<ITEM\n"
        "POSITION 1.0\n"
        "LENGTH 1.0\n"
        ">\n"
        "<ITEM\n"
        "POSITION 3.0\n"
        "LENGTH 1.0\n"
        ">\n"
        "<ITEM\n"
        "POSITION 5.0\n"
        "LENGTH 1.0\n"
        ">\n"
        ">\n";

    std::string path = writeTempRpp(rpp);
    REQUIRE_FALSE(path.empty());

    DataManager dm;
    REQUIRE(dm.parseFile(path));
    removeTempFile(path);

    // Between first and second events
    CounterState cs = dm.getCurrentItem(1, 2.0);
    REQUIRE(cs.count == 1);     // only first event counted
    REQUIRE(cs.is_playing == false);  // outside its length
    REQUIRE(cs.position_sec == 1.0);

    // At third event
    cs = dm.getCurrentItem(1, 5.0);
    REQUIRE(cs.count == 3);     // all three events counted (pos <= 5.0)
    REQUIRE(cs.is_playing == true);
    REQUIRE(cs.position_sec == 5.0);

    // Past all events
    cs = dm.getCurrentItem(1, 10.0);
    REQUIRE(cs.count == 3);
    REQUIRE(cs.is_playing == false);  // last event ended at 6.0
}

// ============================================================
// clear() test
// ============================================================

TEST_CASE("DataManager: clear resets state", "[data_manager]")
{
    const char* rpp =
        "<TRACK {FFF}\n"
        "<ITEM\n"
        "POSITION 0.0\n"
        "LENGTH 1.0\n"
        ">\n"
        ">\n";

    std::string path = writeTempRpp(rpp);
    REQUIRE_FALSE(path.empty());

    DataManager dm;
    REQUIRE(dm.parseFile(path));
    removeTempFile(path);

    REQUIRE(dm.getTrackCount() == 1);
    REQUIRE(dm.getEventCount(1) == 1);

    dm.clear();

    REQUIRE(dm.getTrackCount() == 0);
    REQUIRE(dm.getEventCount(1) == 0);
    REQUIRE(dm.getEvents(1).empty());

    // getTrackMeta should return default after clear
    TrackMeta meta = dm.getTrackMeta(1);
    REQUIRE(meta.item_count == 0);
}

// ============================================================
// Re-parse test (uses unique temp files to avoid filename clash)
// ============================================================

TEST_CASE("DataManager: parseFile clears previous state", "[data_manager]")
{
    const char* rpp1 =
        "<TRACK {AAA}\n"
        "<ITEM\n"
        "POSITION 1.0\n"
        "LENGTH 1.0\n"
        ">\n"
        ">\n";

    const char* rpp2 =
        "<TRACK {BBB}\n"
        "<ITEM\n"
        "POSITION 2.0\n"
        "LENGTH 2.0\n"
        ">\n"
        ">\n";

    // Use unique filenames via suffix to avoid overwriting
    std::string path1 = writeTempRpp(rpp1, "_1");
    REQUIRE_FALSE(path1.empty());

    DataManager dm;
    REQUIRE(dm.parseFile(path1));
    removeTempFile(path1);

    REQUIRE(dm.getTrackCount() == 1);
    REQUIRE(dm.getEvents(1)[0].position_sec == 1.0);

    // Re-parse with different file
    std::string path2 = writeTempRpp(rpp2, "_2");
    REQUIRE_FALSE(path2.empty());

    REQUIRE(dm.parseFile(path2));
    removeTempFile(path2);

    REQUIRE(dm.getTrackCount() == 1);
    REQUIRE(dm.getEvents(1)[0].position_sec == 2.0);
}

// ============================================================
// MIDI TrackMeta test
// ============================================================

TEST_CASE("DataManager: MIDI TrackMeta with G5 guardrail", "[data_manager]")
{
    // Single pitch MIDI: G5 guardrail should expand to +/-1
    std::vector<uint8_t> track;
    auto tempoEvent = evt(0, {0xFF, 0x51, 0x03, 0x07, 0xA1, 0x20});
    track.insert(track.end(), tempoEvent.begin(), tempoEvent.end());
    auto noteOn = evt(0, {0x90, 64, 100});   // E4
    track.insert(track.end(), noteOn.begin(), noteOn.end());
    auto noteOff = evt(480, {0x80, 64, 0});
    track.insert(track.end(), noteOff.begin(), noteOff.end());
    auto eot = evt(0, {0xFF, 0x2F, 0x00});
    track.insert(track.end(), eot.begin(), eot.end());

    auto data = buildMinimalMidi(0, 1, 480, 500000, {track});
    std::string path = writeTempMidi(data);
    REQUIRE_FALSE(path.empty());

    DataManager dm;
    REQUIRE(dm.parseFile(path));
    removeTempMidi(path);

    TrackMeta meta = dm.getTrackMeta(100);

    // pitch_shift = 64 - 60 = 4.0. G5: min==max -> expand to [3.0, 5.0]
    REQUIRE(meta.item_count == 1);
    REQUIRE(meta.min_pitch == Approx(3.0).margin(0.001));
    REQUIRE(meta.max_pitch == Approx(5.0).margin(0.001));
}