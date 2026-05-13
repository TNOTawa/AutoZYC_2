// AutoZYC.aux2 — Project configuration persistence
#include "src/plugin/ProjectConfig.h"
#include <windows.h>
#include "aviutl2_sdk/plugin2.h"
#include <cstdio>
#include <cstdlib>

void ProjectConfig::loadConfig(PROJECT_FILE* pf)
{
    if (!pf)
        return;

    // New keys
    {
        const char* val = pf->get_param_string("autozic_audio_path");
        if (val)
            audio_project_path = val;
    }

    {
        const char* val = pf->get_param_string("autozic_preview_offset");
        if (val)
            preview_offset_sec = std::atof(val);
    }

    {
        const char* val = pf->get_param_string("autozic_bpm_override");
        if (val)
            bpm_override = std::atof(val);
    }

    // Legacy key — backward compatibility
    {
        const char* val = pf->get_param_string("autozic_rpp_path");
        if (val)
            last_rpp_path = val;
    }

    // If new key is empty but legacy key exists, adopt legacy value
    if (audio_project_path.empty() && !last_rpp_path.empty())
    {
        audio_project_path = last_rpp_path;
    }
}

void ProjectConfig::saveConfig(PROJECT_FILE* pf)
{
    if (!pf)
        return;

    char buf[32];

    // New keys
    pf->set_param_string("autozic_audio_path", audio_project_path.c_str());

    std::snprintf(buf, sizeof(buf), "%.6f", preview_offset_sec);
    pf->set_param_string("autozic_preview_offset", buf);

    std::snprintf(buf, sizeof(buf), "%.6f", bpm_override);
    pf->set_param_string("autozic_bpm_override", buf);

    // Legacy key — keep writing for older project files
    pf->set_param_string("autozic_rpp_path", audio_project_path.c_str());
}