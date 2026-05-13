// AutoZYC — Data manager: unified RPP/MIDI file parser interface
// Auto-detects file format by extension and provides a single API
// for both RPP (.rpp) and Standard MIDI File (.mid/.midi/.smf) sources.
#pragma once

#include "src/core/EventTypes.h"
#include <string>
#include <vector>
#include <unordered_map>

// CounterState — returned by getCurrentItem(), matching old .mod2 get_current_item() behavior
struct CounterState
{
    int     count = 0;          // number of events with position_sec <= current time
    bool    is_playing = false; // true if current time falls within the last event's range
    double  position_sec = 0.0; // position of the last event
    double  length_sec = 0.0;   // length of the last event
    double  pitch_shift = 0.0;  // pitch shift of the last event
    double  velocity = 0.0;     // velocity of the last event
};

class DataManager
{
public:
    bool parseFile(const std::string& filepath);

    int  getTrackCount() const;
    int  getEventCount(int track) const;
    const std::vector<UnifiedEvent>& getEvents(int track) const;
    TrackMeta getTrackMeta(int track) const;

    CounterState getCurrentItem(int track, double timeSec) const;

    void clear();

private:
    bool parseRpp(const std::string& filepath);
    bool parseMidi(const std::string& filepath);

    void convertMidiToUnified(const ProjectData& data);

    void computeTrackMeta();

    std::unordered_map<int, std::vector<UnifiedEvent>> m_events;
    std::unordered_map<int, TrackMeta> m_trackMeta;
    int m_trackCount = 0;
};