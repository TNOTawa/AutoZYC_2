// AutoZYC — Data manager: unified RPP/MIDI file parser interface
#include "src/core/DataManager.h"
#include "src/core/RppParser.h"
#include "src/core/MidiParser.h"
#include <algorithm>
#include <cctype>

// ============================================================
// parseFile() — auto-detect by extension
// ============================================================

bool DataManager::parseFile(const std::string& filepath)
{
    // Clear any previous state
    clear();

    // Extract extension (lowercase)
    size_t dot = filepath.find_last_of('.');
    if (dot == std::string::npos) return false;

    std::string ext;
    for (size_t i = dot + 1; i < filepath.size(); i++)
        ext.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(filepath[i]))));

    if (ext == "rpp")
        return parseRpp(filepath);
    else if (ext == "mid" || ext == "midi" || ext == "smf")
        return parseMidi(filepath);

    return false;
}

// ============================================================
// parseRpp() — delegate to RppParser
// ============================================================

bool DataManager::parseRpp(const std::string& filepath)
{
    RppParser parser;
    if (!parser.parse(filepath)) return false;

    m_trackCount = parser.getTrackCount();

    // Group events by track
    for (const auto& evt : parser.getEvents())
    {
        m_events[evt.track].push_back(evt);
    }

    // Copy track metadata from RppParser (already computed with G5 guardrail)
    m_trackMeta = parser.getTrackMeta();

    return true;
}

// ============================================================
// parseMidi() — delegate to MidiParser, convert to UnifiedEvent
// ============================================================

bool DataManager::parseMidi(const std::string& filepath)
{
    MidiParser parser;
    ProjectData data;
    if (!parser.parse(filepath.c_str(), data)) return false;

    m_trackCount = static_cast<int>(data.tracks.size());
    convertMidiToUnified(data);
    computeTrackMeta();
    return true;
}

// ============================================================
// convertMidiToUnified() — MidiNote -> UnifiedEvent
// ============================================================

void DataManager::convertMidiToUnified(const ProjectData& data)
{
    for (const auto& track : data.tracks)
    {
        std::vector<UnifiedEvent> events;
        events.reserve(track.notes.size());

        for (const auto& note : track.notes)
        {
            UnifiedEvent evt;
            evt.position_sec = note.positionSec;
            evt.length_sec   = note.lengthSec;
            evt.pitch_shift  = static_cast<double>(note.pitch) - 60.0;  // C4=0, matches old .mod2
            evt.velocity     = static_cast<double>(note.velocity);
            evt.track        = track.index;  // already has +100 offset from MidiParser (G2)
            events.push_back(evt);
        }

        m_events[track.index] = std::move(events);
    }
}

// ============================================================
// computeTrackMeta() — compute per-track metadata with G5 guardrail
// ============================================================

void DataManager::computeTrackMeta()
{
    m_trackMeta.clear();

    for (auto& kv : m_events)
    {
        int track = kv.first;
        auto& events = kv.second;
        if (events.empty()) continue;

        TrackMeta meta;
        meta.item_count = static_cast<int>(events.size());
        meta.min_pitch  = events[0].pitch_shift;
        meta.max_pitch  = events[0].pitch_shift;

        for (auto& e : events)
        {
            if (e.pitch_shift < meta.min_pitch) meta.min_pitch = e.pitch_shift;
            if (e.pitch_shift > meta.max_pitch) meta.max_pitch = e.pitch_shift;
        }

        // G5 guardrail: min == max -> expand +/-1.0
        if (meta.min_pitch == meta.max_pitch)
        {
            meta.min_pitch -= 1.0;
            meta.max_pitch += 1.0;
        }

        m_trackMeta[track] = meta;
    }
}

// ============================================================
// Accessors
// ============================================================

int DataManager::getTrackCount() const
{
    return m_trackCount;
}

int DataManager::getEventCount(int track) const
{
    auto it = m_events.find(track);
    if (it == m_events.end()) return 0;
    return static_cast<int>(it->second.size());
}

const std::vector<UnifiedEvent>& DataManager::getEvents(int track) const
{
    static const std::vector<UnifiedEvent> s_empty;
    auto it = m_events.find(track);
    if (it == m_events.end()) return s_empty;
    return it->second;
}

TrackMeta DataManager::getTrackMeta(int track) const
{
    auto it = m_trackMeta.find(track);
    if (it != m_trackMeta.end()) return it->second;
    // Default: no track found -> return safe defaults
    return {-12.0, 12.0, 0};
}

// ============================================================
// getCurrentItem() — binary search O(log n)
// Matching old .mod2 get_current_item() behavior
// ============================================================

CounterState DataManager::getCurrentItem(int track, double timeSec) const
{
    auto it = m_events.find(track);
    if (it == m_events.end() || it->second.empty())
    {
        return CounterState{};
    }

    const auto& events = it->second;

    // Binary search: find the first event with position_sec > timeSec
    // Using <= comparator matches old .mod2 lower_bound behavior
    auto bound = std::lower_bound(events.begin(), events.end(), timeSec,
        [](const UnifiedEvent& e, double t) { return e.position_sec <= t; });

    int count = static_cast<int>(bound - events.begin());

    CounterState state;
    state.count = count;

    if (count > 0)
    {
        const auto& evt = events[count - 1];
        state.position_sec = evt.position_sec;
        state.length_sec   = evt.length_sec;
        state.pitch_shift  = evt.pitch_shift;
        state.velocity     = evt.velocity;
        state.is_playing   = (timeSec >= evt.position_sec &&
                              timeSec < evt.position_sec + evt.length_sec);
    }

    return state;
}

// ============================================================
// clear() — reset all state
// ============================================================

void DataManager::clear()
{
    m_events.clear();
    m_trackMeta.clear();
    m_trackCount = 0;
}