// AutoZYC — Event types default value tests
// Included by main.cpp (catch.hpp already available)
#include "src/core/EventTypes.h"

TEST_CASE("MidiNote default values", "[event_types]") {
    MidiNote note{};

    REQUIRE(note.positionSec == 0.0);
    REQUIRE(note.lengthSec == 0.0);
    REQUIRE(note.pitch == 0);
    REQUIRE(note.velocity == 0);
}

TEST_CASE("TrackInfo default values", "[event_types]") {
    TrackInfo info{};

    REQUIRE(info.index == 0);
    REQUIRE(info.name.empty());
    REQUIRE(info.minPitch == 0);
    REQUIRE(info.maxPitch == 0);
    REQUIRE(info.eventCount == 0);
    REQUIRE(info.notes.empty());
}

TEST_CASE("ProjectData default values", "[event_types]") {
    ProjectData data{};

    REQUIRE(data.bpm == 0.0);
    REQUIRE(data.tracks.empty());
}
