// AutoZYC.aux2 — Project configuration persistence header
#pragma once
#include <string>

struct PROJECT_FILE;

class ProjectConfig {
public:
    // Persisted keys
    std::string audio_project_path;      // autozic_audio_path
    double      preview_offset_sec = 0.0; // autozic_preview_offset
    double      bpm_override = 0.0;       // autozic_bpm_override

    // Backward-compatible legacy key
    std::string last_rpp_path;            // autozic_rpp_path

    // Load / save via PROJECT_FILE key-value store
    void loadConfig(PROJECT_FILE* pf);
    void saveConfig(PROJECT_FILE* pf);

    // Convenience accessors
    void setAudioPath(const std::string& path) { audio_project_path = path; last_rpp_path = path; }
    const std::string& getAudioPath() const { return audio_project_path; }
};