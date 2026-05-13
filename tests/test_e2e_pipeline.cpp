// AutoZYC �� E2E Integration Tests
// Full pipeline: RPP/MIDI file �� DataManager �� ObjectGenerator
// Verifies end-to-end data flow and config propagation
//
// All helpers are from sibling test files in the same TU:
//   writeTempRpp / removeTempFile   from test_data_manager.cpp
//   writeTempMidi / removeTempMidi  from test_midi_parser.cpp
//   buildMinimalMidi / vlq / evt    from test_midi_parser.cpp
//   MockEditSection / makeMockEditSection / makeEvent from test_object_generator.cpp

#include "src/core/DataManager.h"
#include "src/generate/ObjectGenerator.h"
#include "src/generate/CumulativeTransform.h"
#include <fstream>
#include <cstdio>
#include <cmath>
#include <algorithm>

// ============================================================
// Helper: collect all events from DataManager sorted by position
// ============================================================
static std::vector<UnifiedEvent> collectAllEvents(DataManager& dm)
{
    std::vector<UnifiedEvent> all;
    for (int t = 1; t <= 200; t++)
    {
        const auto& events = dm.getEvents(t);
        if (!events.empty())
            all.insert(all.end(), events.begin(), events.end());
    }
    std::sort(all.begin(), all.end(), [](const UnifiedEvent& a, const UnifiedEvent& b) {
        return a.position_sec < b.position_sec;
    });
    return all;
}

// ============================================================
// Section A: RPP �� DataManager �� ObjectGenerator complete pipeline
// ============================================================

TEST_CASE("E2E RPP: 2 tracks 3 events -> 3 objects created with correct positions/pitch", "[e2e][rpp]")
{
    const char* rpp =
        "<TRACK {AAA}\n"
        "<ITEM\n"
        "POSITION 0.0\n"
        "LENGTH 1.0\n"
        "PLAYRATE 1.0 1 0.0\n"
        ">\n"
        "<ITEM\n"
        "POSITION 2.0\n"
        "LENGTH 0.5\n"
        "PLAYRATE 1.0 1 -5.0\n"
        ">\n"
        ">\n"
        "<TRACK {BBB}\n"
        "<ITEM\n"
        "POSITION 1.0\n"
        "LENGTH 0.75\n"
        "PLAYRATE 1.0 1 7.0\n"
        ">\n"
        ">\n";

    std::string path = writeTempRpp(rpp, "_e2e_rpp_2t3e");
    REQUIRE_FALSE(path.empty());

    DataManager dm;
    REQUIRE(dm.parseFile(path));
    removeTempFile(path);

    REQUIRE(dm.getTrackCount() == 2);
    REQUIRE(dm.getEventCount(1) == 2);
    REQUIRE(dm.getEventCount(2) == 1);

    // Collect and verify sort order
    auto allEvents = collectAllEvents(dm);
    REQUIRE(allEvents.size() == 3);
    REQUIRE(allEvents[0].position_sec == 0.0);
    REQUIRE(allEvents[1].position_sec == 1.0);
    REQUIRE(allEvents[2].position_sec == 2.0);

    // Generate objects
    MockEditSection::getInstance()->reset();
    ObjectGenerator gen;
    GenerationConfig cfg;
    auto result = gen.generateObjects(makeMockEditSection(), allEvents, cfg, 30.0);

    REQUIRE(result.created == 3);
    REQUIRE(result.skipped == 0);

    auto& objs = MockEditSection::getInstance()->objects;
    REQUIRE(objs.size() == 3);

    // Event 0: pos=0.0 -> floor(0) = 0, length=1.0 -> ceil(30) = 30
    REQUIRE(objs[0].frame == 0);
    REQUIRE(objs[0].length == 30);
    REQUIRE(objs[0].name == L"Trk1 P69");

    // Event 1: pos=1.0 -> floor(30) = 30, length=0.75 -> ceil(22.5) = 23
    REQUIRE(objs[1].frame == 30);
    REQUIRE(objs[1].length == 23);
    REQUIRE(objs[1].name == L"Trk2 P76");

    // Event 2: pos=2.0 -> floor(60) = 60, length=0.5 -> ceil(15) = 15
    REQUIRE(objs[2].frame == 60);
    REQUIRE(objs[2].length == 15);
    REQUIRE(objs[2].name == L"Trk1 P64");
}

TEST_CASE("E2E RPP: empty RPP file -> pipeline yields 0 events, 0 objects", "[e2e][rpp]")
{
    std::string path = writeTempRpp("", "_e2e_rpp_empty");
    REQUIRE_FALSE(path.empty());

    DataManager dm;
    REQUIRE_FALSE(dm.parseFile(path));
    removeTempFile(path);

    auto allEvents = collectAllEvents(dm);
    REQUIRE(allEvents.empty());

    MockEditSection::getInstance()->reset();
    ObjectGenerator gen;
    GenerationConfig cfg;
    auto result = gen.generateObjects(makeMockEditSection(), allEvents, cfg, 30.0);

    REQUIRE(result.created == 0);
    REQUIRE(result.skipped == 0);
}

TEST_CASE("E2E RPP: range filter in config filters parsed events", "[e2e][rpp]")
{
    const char* rpp =
        "<TRACK {AAA}\n"
        "<ITEM\n"
        "POSITION 0.0\n"
        "LENGTH 1.0\n"
        ">\n"
        "<ITEM\n"
        "POSITION 2.0\n"
        "LENGTH 1.0\n"
        ">\n"
        "<ITEM\n"
        "POSITION 4.0\n"
        "LENGTH 1.0\n"
        ">\n"
        ">\n";

    std::string path = writeTempRpp(rpp, "_e2e_rpp_range");
    REQUIRE_FALSE(path.empty());

    DataManager dm;
    REQUIRE(dm.parseFile(path));
    removeTempFile(path);

    auto allEvents = collectAllEvents(dm);
    REQUIRE(allEvents.size() == 3);

    // Range filter 1.0-3.5 -> only event at 2.0 passes
    MockEditSection::getInstance()->reset();
    ObjectGenerator gen;
    GenerationConfig cfg;
    cfg.range_start_sec = 1.0;
    cfg.range_end_sec = 3.5;

    auto result = gen.generateObjects(makeMockEditSection(), allEvents, cfg, 30.0);

    REQUIRE(result.created == 1);
    REQUIRE(result.skipped == 2);

    auto& objs = MockEditSection::getInstance()->objects;
    REQUIRE(objs[0].frame == 60); // floor(2.0 * 30) = 60
}

// ============================================================
// Section B: MIDI -> DataManager -> ObjectGenerator complete pipeline
// ============================================================

TEST_CASE("E2E MIDI: single track 2 notes -> 2 objects created with correct pitch/vel", "[e2e][midi]")
{
    // Build MIDI: Tempo 120 BPM (500000 usec), 480 ticks/beat
    // Note C4 (60) vel=100 at delta=0, length=480 ticks (quarter = 0.5s)
    // Note D4 (62) vel=80 at delta=480, length=240 ticks (eighth = 0.25s)
    std::vector<uint8_t> track;
    auto tempo = evt(0, {0xFF, 0x51, 0x03, 0x07, 0xA1, 0x20}); // 500000
    track.insert(track.end(), tempo.begin(), tempo.end());
    auto n1on = evt(0, {0x90, 60, 100});
    track.insert(track.end(), n1on.begin(), n1on.end());
    auto n1off = evt(480, {0x80, 60, 0});
    track.insert(track.end(), n1off.begin(), n1off.end());
    auto n2on = evt(0, {0x90, 62, 80});
    track.insert(track.end(), n2on.begin(), n2on.end());
    auto n2off = evt(240, {0x80, 62, 0});
    track.insert(track.end(), n2off.begin(), n2off.end());
    auto eot = evt(0, {0xFF, 0x2F, 0x00});
    track.insert(track.end(), eot.begin(), eot.end());

    auto data = buildMinimalMidi(0, 1, 480, 500000, {track});
    std::string path = writeTempMidi(data);
    REQUIRE_FALSE(path.empty());

    DataManager dm;
    REQUIRE(dm.parseFile(path));
    removeTempMidi(path);

    // MIDI track index 0 -> stored as 100 (G2 offset)
    REQUIRE(dm.getTrackCount() == 1);
    REQUIRE(dm.getEventCount(100) == 2);

    auto allEvents = collectAllEvents(dm);
    REQUIRE(allEvents.size() == 2);

    // Verify parsed data before generation
    REQUIRE(allEvents[0].pitch_shift == Approx(0.0).margin(0.001));
    REQUIRE(allEvents[0].velocity == Approx(100.0).margin(0.001));
    REQUIRE(allEvents[0].position_sec == Approx(0.0).margin(0.001));
    REQUIRE(allEvents[0].length_sec   == Approx(0.5).margin(0.001));

    REQUIRE(allEvents[1].pitch_shift == Approx(2.0).margin(0.001));
    REQUIRE(allEvents[1].velocity == Approx(80.0).margin(0.001));
    REQUIRE(allEvents[1].position_sec == Approx(0.5).margin(0.001));
    REQUIRE(allEvents[1].length_sec   == Approx(0.25).margin(0.001));

    // Generate objects
    MockEditSection::getInstance()->reset();
    ObjectGenerator gen;
    GenerationConfig cfg;
    auto result = gen.generateObjects(makeMockEditSection(), allEvents, cfg, 30.0);

    REQUIRE(result.created == 2);
    REQUIRE(result.skipped == 0);

    auto& objs = MockEditSection::getInstance()->objects;
    REQUIRE(objs.size() == 2);

    // C4: pitch_shift=0 -> 0+69 = P69
    REQUIRE(objs[0].name == L"Trk100 P69");
    REQUIRE(objs[0].frame == 0);            // floor(0.0 * 30)
    // Float precision in MIDI parser may give 15 or 16 frames; we accept either
    REQUIRE((objs[0].length == 15 || objs[0].length == 16));
    // D4: pitch_shift=2 -> 2+69 = P71
    REQUIRE(objs[1].name == L"Trk100 P71");
    REQUIRE(objs[1].frame == 15);            // floor(0.5 * 30)
    REQUIRE(objs[1].length == 8);            // ceil(0.25 * 30) = 7.5 -> 8
}

TEST_CASE("E2E MIDI: format 1 multi-track -> tracks separated correctly", "[e2e][midi]")
{
    // Track 0: tempo only (120 BPM)
    std::vector<uint8_t> track0;
    auto tempo = evt(0, {0xFF, 0x51, 0x03, 0x07, 0xA1, 0x20});
    track0.insert(track0.end(), tempo.begin(), tempo.end());
    auto eot0 = evt(0, {0xFF, 0x2F, 0x00});
    track0.insert(track0.end(), eot0.begin(), eot0.end());

    // Track 1: note E4 (64) vel=90
    std::vector<uint8_t> track1;
    auto n1on = evt(0, {0x90, 64, 90});
    track1.insert(track1.end(), n1on.begin(), n1on.end());
    auto n1off = evt(480, {0x80, 64, 0});
    track1.insert(track1.end(), n1off.begin(), n1off.end());
    auto eot1 = evt(0, {0xFF, 0x2F, 0x00});
    track1.insert(track1.end(), eot1.begin(), eot1.end());

    auto data = buildMinimalMidi(1, 2, 480, 500000, {track0, track1});
    std::string path = writeTempMidi(data);
    REQUIRE_FALSE(path.empty());

    DataManager dm;
    REQUIRE(dm.parseFile(path));
    removeTempMidi(path);

    // Track 0 (index 100) is empty (tempo only), Track 1 (index 101) has the note
    REQUIRE(dm.getTrackCount() == 1);       // only non-empty tracks counted
    REQUIRE(dm.getEventCount(100) == 0);    // tempo track -> empty
    REQUIRE(dm.getEventCount(101) == 1);    // note track

    auto allEvents = collectAllEvents(dm);
    REQUIRE(allEvents.size() == 1);
    REQUIRE(allEvents[0].track == 101);
    REQUIRE(allEvents[0].pitch_shift == Approx(4.0).margin(0.001)); // 64-60=4

    // Generate
    MockEditSection::getInstance()->reset();
    ObjectGenerator gen;
    GenerationConfig cfg;
    auto result = gen.generateObjects(makeMockEditSection(), allEvents, cfg, 30.0);

    REQUIRE(result.created == 1);
    REQUIRE(result.skipped == 0);

    auto& objs = MockEditSection::getInstance()->objects;
    // pitch_shift=4 -> 4+69 = P73
    REQUIRE(objs[0].name == L"Trk101 P73");
}

// ============================================================
// Section C: Config propagation through pipeline (config persistence round-trip)
// ============================================================

TEST_CASE("E2E: config values propagate through RPP pipeline - alias + start_layer + freeze", "[e2e][config]")
{
    const char* rpp =
        "<TRACK {AAA}\n"
        "<ITEM\n"
        "POSITION 0.0\n"
        "LENGTH 1.0\n"
        ">\n"
        ">\n";

    std::string path = writeTempRpp(rpp, "_e2e_cfg1");
    REQUIRE_FALSE(path.empty());

    DataManager dm;
    REQUIRE(dm.parseFile(path));
    removeTempFile(path);

    auto allEvents = collectAllEvents(dm);
    REQUIRE(allEvents.size() == 1);

    MockEditSection::getInstance()->reset();
    ObjectGenerator gen;
    GenerationConfig cfg;
    cfg.alias = "CustomBox.object";
    cfg.start_layer = 5;
    cfg.freeze_state = true;
    cfg.step_scale = 110.0;
    cfg.step_rotation = 2.0;
    cfg.step_offset_x = 3.0;
    cfg.step_offset_y = -1.0;

    auto result = gen.generateObjects(makeMockEditSection(), allEvents, cfg, 30.0);

    REQUIRE(result.created == 1);
    REQUIRE(result.skipped == 0);

    auto& objs = MockEditSection::getInstance()->objects;
    REQUIRE(objs.size() == 1);
    REQUIRE(objs[0].alias == "CustomBox.object");
    REQUIRE(objs[0].layer == 4); // 0-based = start_layer - 1
    REQUIRE(objs[0].name == L"Trk1 P69 [F:0.0,-0.0,0.0,100.0]");
}

TEST_CASE("E2E: freeze_state + step transforms applied through parsed RPP data", "[e2e][config]")
{
    const char* rpp =
        "<TRACK {AAA}\n"
        "<ITEM\n"
        "POSITION 0.0\n"
        "LENGTH 1.0\n"
        ">\n"
        "<ITEM\n"
        "POSITION 1.0\n"
        "LENGTH 1.0\n"
        ">\n"
        "<ITEM\n"
        "POSITION 2.0\n"
        "LENGTH 1.0\n"
        ">\n"
        ">\n";

    std::string path = writeTempRpp(rpp, "_e2e_cfg2");
    REQUIRE_FALSE(path.empty());

    DataManager dm;
    REQUIRE(dm.parseFile(path));
    removeTempFile(path);

    auto allEvents = collectAllEvents(dm);
    REQUIRE(allEvents.size() == 3);

    MockEditSection::getInstance()->reset();
    ObjectGenerator gen;
    GenerationConfig cfg;
    cfg.freeze_state = true;
    cfg.step_scale = 90.0;
    cfg.step_rotation = 5.0;
    cfg.step_offset_x = 10.0;
    cfg.step_offset_y = -2.0;

    auto result = gen.generateObjects(makeMockEditSection(), allEvents, cfg, 30.0);

    REQUIRE(result.created == 3);

    auto& objs = MockEditSection::getInstance()->objects;

    // itemIndex=0: identity -> scale=100, rot=0, dx=0, dy=0
    REQUIRE(objs[0].name == L"Trk1 P69 [F:0.0,-0.0,0.0,100.0]");
    // itemIndex=1: step applied once -> scale=90, rot=5, dx=10, dy=-2
    REQUIRE(objs[1].name == L"Trk1 P69 [F:10.0,-2.0,5.0,90.0]");
    // itemIndex=2: step applied twice -> scale=81, rot=10, dx=20, dy=-4
    REQUIRE(objs[2].name == L"Trk1 P69 [F:20.0,-4.0,10.0,81.0]");
}

TEST_CASE("E2E: RPP with different fps -> correct frame conversion", "[e2e][config]")
{
    const char* rpp =
        "<TRACK {AAA}\n"
        "<ITEM\n"
        "POSITION 1.0\n"
        "LENGTH 0.5\n"
        ">\n"
        ">\n";

    std::string path = writeTempRpp(rpp, "_e2e_fps24");
    REQUIRE_FALSE(path.empty());

    DataManager dm;
    REQUIRE(dm.parseFile(path));
    removeTempFile(path);

    auto allEvents = collectAllEvents(dm);

    // 24 fps
    MockEditSection::getInstance()->reset();
    ObjectGenerator gen;
    GenerationConfig cfg;
    auto result = gen.generateObjects(makeMockEditSection(), allEvents, cfg, 24.0);

    REQUIRE(result.created == 1);
    auto& objs = MockEditSection::getInstance()->objects;
    REQUIRE(objs[0].frame == 24);  // floor(1.0 * 24)
    REQUIRE(objs[0].length == 12); // ceil(0.5 * 24)
}

// ============================================================
// Section D: Cumulative transform + max object count combination
// ============================================================

TEST_CASE("E2E: cumulative transform + max_visible_count combined", "[e2e][combined]")
{
    MockEditSection::getInstance()->reset();
    ObjectGenerator gen;
    GenerationConfig cfg;
    cfg.max_visible_count = 2;
    cfg.freeze_state = true;
    cfg.step_scale = 80.0;
    cfg.step_rotation = 10.0;
    cfg.step_offset_x = 15.0;
    cfg.step_offset_y = -5.0;

    std::vector<UnifiedEvent> events = {
        makeEvent(0.0, 1.0, 0.0, 0.8, 1),  // created, itemIndex=0
        makeEvent(1.0, 1.0, 2.0, 0.6, 1),  // created, itemIndex=1
        makeEvent(2.0, 1.0, 4.0, 0.5, 1),  // skipped (max_visible_count)
        makeEvent(3.0, 1.0, 5.0, 0.4, 1),  // skipped (max_visible_count)
    };

    auto result = gen.generateObjects(makeMockEditSection(), events, cfg, 30.0);

    REQUIRE(result.created == 2);
    REQUIRE(result.skipped == 2);

    auto& objs = MockEditSection::getInstance()->objects;
    REQUIRE(objs.size() == 2);

    // itemIndex=0: identity transform
    REQUIRE(objs[0].name == L"Trk1 P69 [F:0.0,-0.0,0.0,100.0]");

    // itemIndex=1: step applied once -> scale=80, rot=10, dx=15, dy=-5
    REQUIRE(objs[1].name == L"Trk1 P71 [F:15.0,-5.0,10.0,80.0]");
}

TEST_CASE("E2E: max_visible_count + range filter combined", "[e2e][combined]")
{
    MockEditSection::getInstance()->reset();
    ObjectGenerator gen;
    GenerationConfig cfg;
    cfg.max_visible_count = 1;
    cfg.range_start_sec = 1.0;
    cfg.range_end_sec = 10.0;

    std::vector<UnifiedEvent> events = {
        makeEvent(0.0, 1.0, 0.0, 0.8, 1),  // skipped (out of range)
        makeEvent(1.5, 1.0, 2.0, 0.6, 1),  // created (1st in range)
        makeEvent(2.0, 1.0, 4.0, 0.5, 1),  // skipped (max_visible_count reached)
        makeEvent(3.0, 1.0, 5.0, 0.4, 1),  // skipped (max_visible_count reached)
    };

    auto result = gen.generateObjects(makeMockEditSection(), events, cfg, 30.0);

    REQUIRE(result.created == 1);
    REQUIRE(result.skipped == 3);
    REQUIRE(MockEditSection::getInstance()->objects.size() == 1);
}

TEST_CASE("E2E: max_lifetime_sec + max_visible_count combined", "[e2e][combined]")
{
    MockEditSection::getInstance()->reset();
    ObjectGenerator gen;
    GenerationConfig cfg;
    cfg.max_visible_count = 2;
    cfg.max_lifetime_sec = 0.5;

    std::vector<UnifiedEvent> events = {
        makeEvent(0.0, 2.0, 0.0, 0.8, 1),  // created, length clamped to 0.5s -> 15f
        makeEvent(1.0, 3.0, 2.0, 0.6, 1),  // created, length clamped to 0.5s -> 15f
        makeEvent(2.0, 1.0, 4.0, 0.5, 1),  // skipped (max_visible_count)
    };

    auto result = gen.generateObjects(makeMockEditSection(), events, cfg, 30.0);

    REQUIRE(result.created == 2);
    REQUIRE(result.skipped == 1);

    auto& objs = MockEditSection::getInstance()->objects;
    REQUIRE((objs[0].length == 15));  // ceil(0.5 * 30) = 15 (exact math)
    REQUIRE((objs[1].length == 15));  // ceil(0.5 * 30) = 15 (exact math)
}

TEST_CASE("E2E: range filter + freeze_state combined - skip does not advance itemIndex", "[e2e][combined]")
{
    MockEditSection::getInstance()->reset();
    ObjectGenerator gen;
    GenerationConfig cfg;
    cfg.freeze_state = true;
    cfg.range_start_sec = 0.0;
    cfg.range_end_sec = 3.5;

    std::vector<UnifiedEvent> events = {
        makeEvent(0.0, 1.0, 0.0, 0.8, 1),   // created, itemIndex=0
        makeEvent(5.0, 1.0, 2.0, 0.6, 2),   // skipped (out of range)
        makeEvent(1.0, 1.0, 4.0, 0.5, 1),   // created, itemIndex=1
    };

    auto result = gen.generateObjects(makeMockEditSection(), events, cfg, 30.0);

    REQUIRE(result.created == 2);
    REQUIRE(result.skipped == 1);

    auto& objs = MockEditSection::getInstance()->objects;
    // itemIndex=0: identity
    REQUIRE(objs[0].name == L"Trk1 P69 [F:0.0,0.0,0.0,100.0]");
    // itemIndex=1: identity (default step values)
    REQUIRE(objs[1].name == L"Trk1 P73 [F:0.0,0.0,0.0,100.0]");
}

// ============================================================
// Section E: Flip mode combination tests
// ============================================================

TEST_CASE("E2E: flip_mode with RPP parsed events", "[e2e][flip]")
{
    const char* rpp =
        "<TRACK {AAA}\n"
        "<ITEM\n"
        "POSITION 0.0\n"
        "LENGTH 1.0\n"
        ">\n"
        "<ITEM\n"
        "POSITION 1.0\n"
        "LENGTH 1.0\n"
        ">\n"
        "<ITEM\n"
        "POSITION 2.0\n"
        "LENGTH 1.0\n"
        ">\n"
        "<ITEM\n"
        "POSITION 3.0\n"
        "LENGTH 1.0\n"
        ">\n"
        ">\n";

    std::string path = writeTempRpp(rpp, "_e2e_rpp_flip");
    REQUIRE_FALSE(path.empty());

    DataManager dm;
    REQUIRE(dm.parseFile(path));
    removeTempFile(path);

    auto allEvents = collectAllEvents(dm);
    REQUIRE(allEvents.size() == 4);

    // Test flip_mode=3 (clockwise 4-step: [0, H, H+V, V])
    MockEditSection::getInstance()->reset();
    ObjectGenerator gen;
    GenerationConfig cfg;
    cfg.flip_mode = 3;

    auto result = gen.generateObjects(makeMockEditSection(), allEvents, cfg, 30.0);

    REQUIRE(result.created == 4);

    auto& objs = MockEditSection::getInstance()->objects;
    REQUIRE(objs[0].name == L"Trk1 P69");       // index 0: no flip
    REQUIRE(objs[1].name == L"Trk1 P69_H");     // index 1: H
    REQUIRE(objs[2].name == L"Trk1 P69_HV");    // index 2: H+V
    REQUIRE(objs[3].name == L"Trk1 P69_V");     // index 3: V
}

TEST_CASE("E2E: all 5 flip modes with parsed events", "[e2e][flip]")
{
    const char* rpp =
        "<TRACK {AAA}\n"
        "<ITEM\n"
        "POSITION 0.0\n"
        "LENGTH 1.0\n"
        ">\n"
        "<ITEM\n"
        "POSITION 1.0\n"
        "LENGTH 1.0\n"
        ">\n"
        "<ITEM\n"
        "POSITION 2.0\n"
        "LENGTH 1.0\n"
        ">\n"
        "<ITEM\n"
        "POSITION 3.0\n"
        "LENGTH 1.0\n"
        ">\n"
        ">\n";

    std::string path = writeTempRpp(rpp, "_e2e_flip_all");
    REQUIRE_FALSE(path.empty());

    DataManager dm;
    REQUIRE(dm.parseFile(path));
    removeTempFile(path);

    auto allEvents = collectAllEvents(dm);
    REQUIRE(allEvents.size() == 4);

    // Mode 0: no flip
    {
        MockEditSection::getInstance()->reset();
        ObjectGenerator gen;
        GenerationConfig cfg;
        cfg.flip_mode = 0;
        auto result = gen.generateObjects(makeMockEditSection(), allEvents, cfg, 30.0);
        REQUIRE(result.created == 4);
        auto& objs = MockEditSection::getInstance()->objects;
        for (int i = 0; i < 4; i++)
            REQUIRE(objs[i].name.find(L'_') == std::wstring::npos);
    }

    // Mode 1: odd -> H
    {
        MockEditSection::getInstance()->reset();
        ObjectGenerator gen;
        GenerationConfig cfg;
        cfg.flip_mode = 1;
        gen.generateObjects(makeMockEditSection(), allEvents, cfg, 30.0);
        auto& objs = MockEditSection::getInstance()->objects;
        REQUIRE(objs[0].name == L"Trk1 P69");
        REQUIRE(objs[1].name == L"Trk1 P69_H");
        REQUIRE(objs[2].name == L"Trk1 P69");
        REQUIRE(objs[3].name == L"Trk1 P69_H");
    }

    // Mode 2: even -> V
    {
        MockEditSection::getInstance()->reset();
        ObjectGenerator gen;
        GenerationConfig cfg;
        cfg.flip_mode = 2;
        gen.generateObjects(makeMockEditSection(), allEvents, cfg, 30.0);
        auto& objs = MockEditSection::getInstance()->objects;
        REQUIRE(objs[0].name == L"Trk1 P69_V");
        REQUIRE(objs[1].name == L"Trk1 P69");
        REQUIRE(objs[2].name == L"Trk1 P69_V");
        REQUIRE(objs[3].name == L"Trk1 P69");
    }

    // Mode 3: clockwise [0, H, H+V, V]
    {
        MockEditSection::getInstance()->reset();
        ObjectGenerator gen;
        GenerationConfig cfg;
        cfg.flip_mode = 3;
        gen.generateObjects(makeMockEditSection(), allEvents, cfg, 30.0);
        auto& objs = MockEditSection::getInstance()->objects;
        REQUIRE(objs[0].name == L"Trk1 P69");
        REQUIRE(objs[1].name == L"Trk1 P69_H");
        REQUIRE(objs[2].name == L"Trk1 P69_HV");
        REQUIRE(objs[3].name == L"Trk1 P69_V");
    }

    // Mode 4: counter-clockwise [0, V, H+V, H]
    {
        MockEditSection::getInstance()->reset();
        ObjectGenerator gen;
        GenerationConfig cfg;
        cfg.flip_mode = 4;
        gen.generateObjects(makeMockEditSection(), allEvents, cfg, 30.0);
        auto& objs = MockEditSection::getInstance()->objects;
        REQUIRE(objs[0].name == L"Trk1 P69");
        REQUIRE(objs[1].name == L"Trk1 P69_V");
        REQUIRE(objs[2].name == L"Trk1 P69_HV");
        REQUIRE(objs[3].name == L"Trk1 P69_H");
    }
}

TEST_CASE("E2E: flip_mode=4 with range filter - skips don't advance flip index", "[e2e][flip]")
{
    MockEditSection::getInstance()->reset();
    ObjectGenerator gen;
    GenerationConfig cfg;
    cfg.flip_mode = 4;
    cfg.range_start_sec = 0.0;
    cfg.range_end_sec = 3.5;

    std::vector<UnifiedEvent> events = {
        makeEvent(0.0, 1.0, 0.0, 0.8, 1),   // created, itemIndex=0
        makeEvent(5.0, 1.0, 2.0, 0.6, 1),   // skipped (out of range)
        makeEvent(1.0, 1.0, 4.0, 0.5, 1),   // created, itemIndex=1
        makeEvent(2.0, 1.0, 5.0, 0.4, 1),   // created, itemIndex=2
        makeEvent(3.0, 1.0, 7.0, 0.3, 1),   // created, itemIndex=3
    };

    auto result = gen.generateObjects(makeMockEditSection(), events, cfg, 30.0);

    REQUIRE(result.created == 4);
    REQUIRE(result.skipped == 1);

    auto& objs = MockEditSection::getInstance()->objects;
    // Counter-clockwise 4-step: [0, V, H+V, H]
    REQUIRE(objs[0].name == L"Trk1 P69");       // index 0: no flip
    REQUIRE(objs[1].name == L"Trk1 P73_V");     // index 1: V
    REQUIRE(objs[2].name == L"Trk1 P74_HV");    // index 2: H+V
    REQUIRE(objs[3].name == L"Trk1 P76_H");     // index 3: H
}

TEST_CASE("E2E: flip_mode with freeze_state - both suffixes in name", "[e2e][flip]")
{
    MockEditSection::getInstance()->reset();
    ObjectGenerator gen;
    GenerationConfig cfg;
    cfg.flip_mode = 1;       // odd -> H
    cfg.freeze_state = true;

    std::vector<UnifiedEvent> events = {
        makeEvent(0.0, 1.0, 0.0, 0.8, 1),   // itemIndex=0, no flip
        makeEvent(1.0, 1.0, 2.0, 0.6, 1),   // itemIndex=1, H flip
        makeEvent(2.0, 1.0, 4.0, 0.5, 1),   // itemIndex=2, no flip
    };

    auto result = gen.generateObjects(makeMockEditSection(), events, cfg, 30.0);

    REQUIRE(result.created == 3);

    auto& objs = MockEditSection::getInstance()->objects;
    REQUIRE(objs[0].name == L"Trk1 P69 [F:0.0,0.0,0.0,100.0]");
    REQUIRE(objs[1].name == L"Trk1 P71_H [F:0.0,0.0,0.0,100.0]");
    REQUIRE(objs[2].name == L"Trk1 P73 [F:0.0,0.0,0.0,100.0]");
}

// ============================================================
// Section F: Full featured combination tests
// ============================================================

TEST_CASE("E2E: freeze + flip + max_lifetime + range all together", "[e2e][all]")
{
    MockEditSection::getInstance()->reset();
    ObjectGenerator gen;
    GenerationConfig cfg;
    cfg.freeze_state = true;
    cfg.flip_mode = 1;                  // odd index -> H flip
    cfg.max_lifetime_sec = 0.5;
    cfg.step_scale = 85.0;
    cfg.step_rotation = 3.0;
    cfg.step_offset_x = 5.0;
    cfg.step_offset_y = -2.0;
    cfg.range_start_sec = 0.0;
    cfg.range_end_sec = 4.0;

    std::vector<UnifiedEvent> events = {
        makeEvent(0.0, 2.0, 0.0, 0.8, 1),   // created, itemIndex=0, no flip
        makeEvent(1.0, 2.0, 2.0, 0.6, 1),   // created, itemIndex=1, H flip
        makeEvent(5.0, 1.0, 4.0, 0.5, 1),   // skipped (out of range)
        makeEvent(2.0, 2.0, 5.0, 0.4, 1),   // created, itemIndex=2, no flip
    };

    auto result = gen.generateObjects(makeMockEditSection(), events, cfg, 30.0);

    REQUIRE(result.created == 3);
    REQUIRE(result.skipped == 1);

    auto& objs = MockEditSection::getInstance()->objects;
    REQUIRE(objs.size() == 3);

    // All lengths clamped to ceil(0.5 * 30) = 15
    REQUIRE(objs[0].length == 15);
    REQUIRE(objs[1].length == 15);
    REQUIRE(objs[2].length == 15);

    // itemIndex=0: no flip, identity transform
    REQUIRE(objs[0].name == L"Trk1 P69 [F:0.0,-0.0,0.0,100.0]");

    // itemIndex=1: H flip, step applied once
    REQUIRE(objs[1].name == L"Trk1 P71_H [F:5.0,-2.0,3.0,85.0]");

    // itemIndex=2: no flip, step applied twice
    REQUIRE(objs[2].name == L"Trk1 P74 [F:10.0,-4.0,6.0,72.2]");
}

TEST_CASE("E2E: RPP -> all features combined pipeline test", "[e2e][all]")
{
    const char* rpp =
        "<TRACK {AAA}\n"
        "<ITEM\n"
        "POSITION 0.0\n"
        "LENGTH 2.0\n"
        ">\n"
        "<ITEM\n"
        "POSITION 1.0\n"
        "LENGTH 2.0\n"
        ">\n"
        ">\n";

    std::string path = writeTempRpp(rpp, "_e2e_all_pipe");
    REQUIRE_FALSE(path.empty());

    DataManager dm;
    REQUIRE(dm.parseFile(path));
    removeTempFile(path);

    auto allEvents = collectAllEvents(dm);
    REQUIRE(allEvents.size() == 2);

    MockEditSection::getInstance()->reset();
    ObjectGenerator gen;
    GenerationConfig cfg;
    cfg.freeze_state = true;
    cfg.flip_mode = 3;                    // clockwise 4-step
    cfg.max_lifetime_sec = 0.75;
    cfg.step_scale = 120.0;
    cfg.step_rotation = 15.0;
    cfg.step_offset_x = 8.0;
    cfg.step_offset_y = -3.0;
    cfg.max_visible_count = 2;

    auto result = gen.generateObjects(makeMockEditSection(), allEvents, cfg, 30.0);

    REQUIRE(result.created == 2);
    REQUIRE(result.skipped == 0);

    auto& objs = MockEditSection::getInstance()->objects;
    REQUIRE(objs.size() == 2);

    // Both lengths clamped to ceil(0.75 * 30) = 23
    REQUIRE(objs[0].length == 23);
    REQUIRE(objs[1].length == 23);

    // itemIndex=0: clock step 0 -> no flip, identity transform
    REQUIRE(objs[0].name == L"Trk1 P69 [F:0.0,-0.0,0.0,100.0]");

    // itemIndex=1: clock step 1 -> H flip, step applied once
    REQUIRE(objs[1].name == L"Trk1 P69_H [F:8.0,-3.0,15.0,120.0]");
}
