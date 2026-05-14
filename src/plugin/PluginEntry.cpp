// AutoZYC.aux2 — Plugin entry point (DLL exports + menu registration)
#include <windows.h>
#include <cstdio>
#include "aviutl2_sdk/plugin2.h"
#include "src/plugin/ProjectConfig.h"
#include "src/ui/SettingsDialog.h"
#include "src/ui/GenerationDialog.h"
#include "src/generate/ObjectGenerator.h"
#include "src/core/DataManager.h"
#include "src/core/Logger.h"

static HOST_APP_TABLE* g_host = nullptr;
static EDIT_HANDLE*    g_editHandle = nullptr;
static ProjectConfig   g_config;
static DataManager     g_dataManager;
static GenerationConfig g_genConfig;

static COMMON_PLUGIN_TABLE g_pluginTable = {
    L"AutoZYC v2.0",
    L"AutoZYC v2.0 - Audio/MIDI-driven animation effects",
};

static void on_layer_menu(EDIT_SECTION* edit)
{
    (void)edit;
    AUTOZYC_LOG_INFO("Menu: right-click layer (工程设置)");

    HWND hwndParent = nullptr;
    if (g_editHandle) hwndParent = g_editHandle->get_host_app_window();
    if (!hwndParent) hwndParent = GetForegroundWindow();

    AUTOZYC_LOG_DEBUG("Parent HWND = 0x%p", hwndParent);
    SettingsDialog::show(hwndParent, g_config);
    AUTOZYC_LOG_INFO("Settings path = '%s'", g_config.audio_project_path.c_str());
}

static void on_object_menu(EDIT_SECTION* edit)
{
    AUTOZYC_LOG_INFO("Menu: right-click object (生成序列物件)");

    HWND hwndParent = nullptr;
    if (g_editHandle) hwndParent = g_editHandle->get_host_app_window();
    if (!hwndParent) hwndParent = GetForegroundWindow();

    int selectedCount = edit->get_selected_object_num();
    AUTOZYC_LOG_INFO("Selected objects = %d", selectedCount);

    GenerationConfig cfg = g_genConfig;
    if (GenerationDialog::show(hwndParent, cfg, selectedCount))
    {
        std::vector<UnifiedEvent> allEvents;
        int trackCount = g_dataManager.getTrackCount();
        AUTOZYC_LOG_INFO("Generating from %d tracks", trackCount);

        for (int t = 1; t <= trackCount; t++) {
            const auto& trackEvents = g_dataManager.getEvents(t);
            allEvents.insert(allEvents.end(), trackEvents.begin(), trackEvents.end());
        }

        if (allEvents.empty()) {
            AUTOZYC_LOG_WARN("No events available");
            MessageBoxW(hwndParent, L"No events. Import RPP/MIDI first.", L"AutoZYC", MB_OK | MB_ICONWARNING);
            return;
        }

        double fps = 0.0;
        if (edit->info && edit->info->scale > 0)
            fps = (double)edit->info->rate / (double)edit->info->scale;
        if (fps <= 0.0) fps = 30.0;

        g_genConfig = cfg;

        ObjectGenerator generator;
        ObjectGenerator::GenerationResult result = generator.generateObjects(edit, allEvents, cfg, fps);
        AUTOZYC_LOG_INFO("Generation: %d created, %d skipped", result.created, result.skipped);

        wchar_t msg[256];
        swprintf(msg, 256, L"Generated: %d objects, %d skipped.", result.created, result.skipped);
        MessageBoxW(hwndParent, msg, L"AutoZYC", MB_OK | MB_ICONINFORMATION);
    }
}

static void on_config_menu(HWND hwnd, HINSTANCE dll_hinst)
{
    (void)hwnd; (void)dll_hinst;
    AUTOZYC_LOG_INFO("Menu: system config");
    MessageBoxW(nullptr, L"AutoZYC Settings - WIP", L"AutoZYC", MB_OK);
}

static void on_import_menu(EDIT_SECTION* edit)
{
    (void)edit;
    AUTOZYC_LOG_INFO("Menu: import audio project");

    HWND hwndParent = nullptr;
    if (g_editHandle) hwndParent = g_editHandle->get_host_app_window();
    if (!hwndParent) hwndParent = GetForegroundWindow();

    if (g_config.audio_project_path.empty()) {
        AUTOZYC_LOG_WARN("Import: no path configured");
        MessageBoxW(hwndParent, L"Set audio project path in Settings first.", L"AutoZYC", MB_OK | MB_ICONINFORMATION);
        return;
    }

    AUTOZYC_LOG_INFO("Parsing: '%s'", g_config.audio_project_path.c_str());

    if (g_dataManager.parseFile(g_config.audio_project_path)) {
        int tc = g_dataManager.getTrackCount();
        int total = 0;
        for (int t = 1; t <= tc; t++) total += g_dataManager.getEventCount(t);
        AUTOZYC_LOG_INFO("Import OK: %d tracks, %d events", tc, total);

        wchar_t msg[256];
        swprintf(msg, 256, L"Imported: %d tracks, %d events.", tc, total);
        MessageBoxW(hwndParent, msg, L"AutoZYC", MB_OK | MB_ICONINFORMATION);
    } else {
        AUTOZYC_LOG_ERROR("Import failed: '%s'", g_config.audio_project_path.c_str());
        MessageBoxW(hwndParent, L"Import failed. Check file path and format.", L"AutoZYC", MB_OK | MB_ICONERROR);
    }
}

static void on_project_load(PROJECT_FILE* project)
{
    AUTOZYC_LOG_INFO("Project load: reading config");
    g_config.loadConfig(project);
    if (!g_config.audio_project_path.empty()) {
        AUTOZYC_LOG_INFO("Auto-loading: '%s'", g_config.audio_project_path.c_str());
        g_dataManager.parseFile(g_config.audio_project_path);
    }
}

static void on_project_save(PROJECT_FILE* project)
{
    AUTOZYC_LOG_INFO("Project save: writing config");
    g_config.saveConfig(project);
}

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
    (void)hinstDLL; (void)lpvReserved;
    return TRUE;
}

extern "C" __declspec(dllexport) DWORD RequiredVersion() { return 2004400; }

extern "C" __declspec(dllexport) bool InitializePlugin(DWORD version)
{
    (void)version;
    AUTOZYC_LOG_INFO("Plugin init, host ver=%lu", version);
    return true;
}

extern "C" __declspec(dllexport) void UninitializePlugin()
{
    AUTOZYC_LOG_INFO("Plugin uninit");
}

extern "C" __declspec(dllexport) COMMON_PLUGIN_TABLE* GetCommonPluginTable()
{
    return &g_pluginTable;
}

extern "C" __declspec(dllexport) void RegisterPlugin(HOST_APP_TABLE* host)
{
    AUTOZYC_LOG_INFO("RegisterPlugin called");
    g_host = host;
    host->set_plugin_information(L"AutoZYC v2.0 - Audio/MIDI-driven animation effects");
    host->register_layer_menu(L"AutoZYC\\工程设置", on_layer_menu);
    host->register_object_menu(L"AutoZYC\\生成序列物件", on_object_menu);
    host->register_config_menu(L"AutoZYC设置", on_config_menu);
    host->register_import_menu(L"AutoZYC 导入音频工程...", on_import_menu);
    host->register_project_load_handler(on_project_load);
    host->register_project_save_handler(on_project_save);
    g_editHandle = host->create_edit_handle();
    AUTOZYC_LOG_INFO("RegisterPlugin done: 4 menus + 2 handlers");
}