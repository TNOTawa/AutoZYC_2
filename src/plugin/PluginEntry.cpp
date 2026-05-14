// AutoZYC.aux2 — Plugin entry point (DLL exports + menu registration)
#include <windows.h>
#include <cstdio>
#include "aviutl2_sdk/plugin2.h"
#include "src/plugin/ProjectConfig.h"
#include "src/ui/SettingsDialog.h"
#include "src/ui/GenerationDialog.h"
#include "src/generate/ObjectGenerator.h"
#include "src/core/DataManager.h"

// ---------------------------------------------------------------------------
// Global state (persisted for plugin lifetime)
// ---------------------------------------------------------------------------
static HOST_APP_TABLE* g_host = nullptr;
static EDIT_HANDLE*    g_editHandle = nullptr;
static ProjectConfig   g_config;
static DataManager     g_dataManager;
static GenerationConfig g_genConfig;

// ---------------------------------------------------------------------------
// Plugin identification table — returned by GetCommonPluginTable()
// ---------------------------------------------------------------------------
static COMMON_PLUGIN_TABLE g_pluginTable = {
    L"AutoZYC v2.0",
    L"\x97F3MAD\x81EA\x52A8\x7269\x4EF6\x751F\x6210\x63D2\x4EF6",
};

// ---------------------------------------------------------------------------
// Menu callbacks
// ---------------------------------------------------------------------------

// AutoZYC\工程设置 — right-click layer menu
static void on_layer_menu(EDIT_SECTION* edit)
{
    (void)edit;

    // 获取父窗口句柄（优先使用宿主窗口，回退到前台窗口）
    HWND hwndParent = nullptr;
    if (g_editHandle)
        hwndParent = g_editHandle->get_host_app_window();
    if (!hwndParent)
        hwndParent = GetForegroundWindow();

    SettingsDialog::show(hwndParent, g_config);
}

// AutoZYC\生成序列物件 — right-click object menu
static void on_object_menu(EDIT_SECTION* edit)
{
    // 获取父窗口句柄
    HWND hwndParent = nullptr;
    if (g_editHandle)
        hwndParent = g_editHandle->get_host_app_window();
    if (!hwndParent)
        hwndParent = GetForegroundWindow();

    // 获取已选物件数量
    int selectedCount = edit->get_selected_object_num();

    // 显示生成配置对话框
    GenerationConfig cfg = g_genConfig;  // 拷贝默认配置
    if (GenerationDialog::show(hwndParent, cfg, selectedCount))
    {
        // 用户按了确定 — 执行物件生成
        // 收集所有轨道的事件
        std::vector<UnifiedEvent> allEvents;
        int trackCount = g_dataManager.getTrackCount();
        for (int t = 1; t <= trackCount; t++)
        {
            const auto& trackEvents = g_dataManager.getEvents(t);
            allEvents.insert(allEvents.end(), trackEvents.begin(), trackEvents.end());
        }

        if (allEvents.empty())
        {
            MessageBoxW(hwndParent,
                L"\u6CA1\u6709\u53EF\u7528\u7684\u4E8B\u4EF6\u6570\u636E\u3002\u8BF7\u5148\u5BFC\u5165 RPP/MIDI \u6587\u4EF6\u3002",
                L"AutoZYC", MB_OK | MB_ICONWARNING);
            return;
        }

        // 计算帧率
        double fps = 0.0;
        if (edit->info && edit->info->scale > 0)
            fps = static_cast<double>(edit->info->rate) / static_cast<double>(edit->info->scale);
        if (fps <= 0.0)
            fps = 30.0;  // 默认 30fps

        // 保存配置供下次使用
        g_genConfig = cfg;

        // 执行生成
        ObjectGenerator generator;
        ObjectGenerator::GenerationResult result = generator.generateObjects(
            edit, allEvents, cfg, fps);

        // 显示结果
        wchar_t resultMsg[256];
        swprintf(resultMsg, 256,
            L"\u751F\u6210\u5B8C\u6210\uFF1A\u521B\u5EFA %d \u4E2A\u7269\u4EF6\uFF0C\u8DF3\u8FC7 %d \u4E2A\u3002",
            result.created, result.skipped);
        MessageBoxW(hwndParent, resultMsg, L"AutoZYC", MB_OK | MB_ICONINFORMATION);
    }
}

// AutoZYC设置 — system settings menu (different signature: HWND + HINSTANCE)
static void on_config_menu(HWND hwnd, HINSTANCE dll_hinst)
{
    (void)hwnd;
    (void)dll_hinst;
    MessageBoxW(nullptr, L"AutoZYC\u8BBE\u7F6E \u2014 WIP", L"AutoZYC", MB_OK);
}

// AutoZYC 导入音频工程... — File > Import menu
static void on_import_menu(EDIT_SECTION* edit)
{
    (void)edit;

    // 获取父窗口句柄
    HWND hwndParent = nullptr;
    if (g_editHandle)
        hwndParent = g_editHandle->get_host_app_window();
    if (!hwndParent)
        hwndParent = GetForegroundWindow();

    // 使用配置中的路径，如果没有则提示用户
    if (g_config.audio_project_path.empty())
    {
        MessageBoxW(hwndParent,
            L"\u8BF7\u5148\u5728\u201C\u5DE5\u7A0B\u8BBE\u7F6E\u201D\u4E2D\u6307\u5B9A\u97F3\u9891\u5DE5\u7A0B\u6587\u4EF6\u8DEF\u5F84\u3002",
            L"AutoZYC", MB_OK | MB_ICONINFORMATION);
        return;
    }

    if (g_dataManager.parseFile(g_config.audio_project_path))
    {
        int trackCount = g_dataManager.getTrackCount();
        int totalEvents = 0;
        for (int t = 1; t <= trackCount; t++)
            totalEvents += g_dataManager.getEventCount(t);

        wchar_t msg[256];
        swprintf(msg, 256,
            L"\u5BFC\u5165\u6210\u529F\uFF1A%d \u4E2A\u8F68\u9053\uFF0C%d \u4E2A\u4E8B\u4EF6\u3002",
            trackCount, totalEvents);
        MessageBoxW(hwndParent, msg, L"AutoZYC", MB_OK | MB_ICONINFORMATION);
    }
    else
    {
        MessageBoxW(hwndParent,
            L"\u5BFC\u5165\u5931\u8D25\uFF0C\u8BF7\u68C0\u67E5\u6587\u4EF6\u8DEF\u5F84\u548C\u683C\u5F0F\u3002",
            L"AutoZYC", MB_OK | MB_ICONERROR);
    }
}

// ---------------------------------------------------------------------------
// Project load / save handlers — persist config via PROJECT_FILE
// ---------------------------------------------------------------------------
static void on_project_load(PROJECT_FILE* project)
{
    g_config.loadConfig(project);

    // 项目加载后尝试导入音频工程
    if (!g_config.audio_project_path.empty())
        g_dataManager.parseFile(g_config.audio_project_path);
}

static void on_project_save(PROJECT_FILE* project)
{
    g_config.saveConfig(project);
}

// ---------------------------------------------------------------------------
// DllMain — standard DLL entry point
// ---------------------------------------------------------------------------
BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
    (void)hinstDLL;
    (void)lpvReserved;
    switch (fdwReason)
    {
    case DLL_PROCESS_ATTACH:
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}

// ---------------------------------------------------------------------------
// Aviutl2 required DLL exports
// ---------------------------------------------------------------------------

// Required minimum host version
extern "C" __declspec(dllexport) DWORD RequiredVersion()
{
    return 2004400; // Aviutl2 beta 44, build 2004400
}

// Called after DLL load to pass host version
extern "C" __declspec(dllexport) bool InitializePlugin(DWORD version)
{
    (void)version;
    return true;
}

// Called before DLL unload
extern "C" __declspec(dllexport) void UninitializePlugin()
{
    // Nothing to clean up yet
}

// Returns the plugin identification table
extern "C" __declspec(dllexport) COMMON_PLUGIN_TABLE* GetCommonPluginTable()
{
    return &g_pluginTable;
}

// Main registration entry — called by Aviutl2 host to set up the plugin
extern "C" __declspec(dllexport) void RegisterPlugin(HOST_APP_TABLE* host)
{
    g_host = host;

    // Plugin information shown in host UI
    host->set_plugin_information(
        L"\x97F3MAD\x81EA\x52A8\x7269\x4EF6\x751F\x6210\x63D2\x4EF6 - AutoZYC v2.0"
    );

    // --- Register menus ---

    // Layer context menu: right-click on timeline layer
    host->register_layer_menu(L"AutoZYC\\\u5DE5\u7A0B\u8BBE\u7F6E", on_layer_menu);

    // Object context menu: right-click on an object
    host->register_object_menu(L"AutoZYC\\\u751F\u6210\u5E8F\u5217\u7269\u4EF6", on_object_menu);

    // Settings menu entry (appears in system menu)
    host->register_config_menu(L"AutoZYC\u8BBE\u7F6E", on_config_menu);

    // File > Import menu entry
    host->register_import_menu(L"AutoZYC \u5BFC\u5165\u97F3\u9891\u5DE5\u7A0B...", on_import_menu);

    // --- Register project handlers ---
    host->register_project_load_handler(on_project_load);
    host->register_project_save_handler(on_project_save);

    // Create and store the edit handle for later call_edit_section usage
    g_editHandle = host->create_edit_handle();
}
