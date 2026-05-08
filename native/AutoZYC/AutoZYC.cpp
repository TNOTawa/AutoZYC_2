//----------------------------------------------------------------------------------
//  AutoZYC.aux2 — 共用插件: 物件批量生成 + 菜单注册 + 配置持久化
//  For AviUtl ExEdit2 (x64)
//  Build: g++ -std=c++17 -O2 -Wall -m64 -shared -static -I../../ -lcomdlg32
//----------------------------------------------------------------------------------

#include <windows.h>
#include <commdlg.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <cmath>

#include "aviutl2_sdk/plugin2.h"

// ============================================================
// 事件数据结构
// ============================================================
struct RppEvent {
    double position_sec;
    double length_sec;
    double pitch_shift;
    int track;
};

// ============================================================
// 全局状态
// ============================================================
static HOST_APP_TABLE* g_host = nullptr;
static EDIT_HANDLE*    g_edit = nullptr;
static char            g_last_rpp_path[1024] = "";
static char            g_last_midi_path[1024] = "";

// ============================================================
// 辅助函数: 跳过空白字符
// ============================================================
static const char* skip_ws(const char* s) {
    while (*s == ' ' || *s == '\t') s++;
    return s;
}

// ============================================================
// 辅助函数: 安全字符串复制
// ============================================================
static void safe_strcpy(char* dst, size_t dst_size, const char* src) {
    if (!dst || dst_size == 0) return;
    if (!src) { dst[0] = '\0'; return; }
    size_t i = 0;
    while (i < dst_size - 1 && src[i] != '\0') {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

// ============================================================
// 配置持久化辅助
// ============================================================
static void save_config_str(PROJECT_FILE* pf, LPCSTR key, LPCSTR value) {
    if (pf && key && value) pf->set_param_string(key, value);
}

static LPCSTR load_config_str(PROJECT_FILE* pf, LPCSTR key, LPCSTR default_val) {
    if (!pf || !key) return default_val;
    LPCSTR val = pf->get_param_string(key);
    return val ? val : default_val;
}

// ============================================================
// RPP 文件解析 (内联C++解析器, 不依赖.mod2)
// ============================================================
static void parse_rpp(const char* path, std::vector<RppEvent>& out_events) {
    out_events.clear();

    FILE* f = fopen(path, "r");
    if (!f) {
        char msg[512];
        snprintf(msg, sizeof(msg), "无法打开文件: %s", path);
        MessageBoxA(nullptr, msg, "AutoZYC 错误", MB_OK | MB_ICONERROR);
        return;
    }

    char line[4096];
    int current_track = 0;

    while (fgets(line, sizeof(line), f)) {
        const char* s = skip_ws(line);

        if (strncmp(s, "<TRACK", 6) == 0) {
            current_track++;
            continue;
        }

        if (strncmp(s, "<ITEM", 5) == 0 && current_track > 0) {
            RppEvent ev = {};
            ev.track = current_track;
            ev.pitch_shift = 0.0;

            while (fgets(line, sizeof(line), f)) {
                s = skip_ws(line);

                if (*s == '>') break;

                if (strncmp(s, "POSITION", 8) == 0) {
                    ev.position_sec = atof(s + 9);
                } else if (strncmp(s, "LENGTH", 6) == 0) {
                    ev.length_sec = atof(s + 7);
                } else if (strncmp(s, "PLAYRATE", 8) == 0) {
                    const char* p = s + 9;
                    int part = 0;
                    char token[64];
                    int ti = 0;

                    while (*p && part < 4) {
                        if (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') {
                            if (ti > 0) {
                                token[ti] = '\0';
                                if (part == 2) {
                                    ev.pitch_shift = atof(token);
                                    break;
                                }
                                part++;
                                ti = 0;
                            }
                            p++;
                        } else {
                            if (ti < 63) token[ti++] = *p;
                            p++;
                        }
                    }
                    if (ti > 0 && part == 2) {
                        token[ti] = '\0';
                        ev.pitch_shift = atof(token);
                    }
                }

                if (strncmp(s, "<SOURCE", 7) == 0) {
                    int depth = 1;
                    while (depth > 0 && fgets(line, sizeof(line), f)) {
                        s = skip_ws(line);
                        if (*s == '<' && s[1] != '/') {
                            if (strncmp(s, "<SOURCE", 7) != 0) depth++;
                        } else if (strncmp(s, ">", 1) == 0) {
                            depth--;
                        }
                    }
                }
            }

            if (ev.length_sec > 0.001) {
                out_events.push_back(ev);
            }
        }
    }

    fclose(f);
}

// ============================================================
// 物件批量生成
// ============================================================
static void generate_objects_from_path(EDIT_SECTION* edit, const char* path) {
    std::vector<RppEvent> events;
    parse_rpp(path, events);

    if (events.empty()) {
        MessageBoxA(nullptr, "RPP解析完成但未发现有效事件。", "AutoZYC", MB_OK);
        return;
    }

    double fps = 60.0;
    if (edit->info) {
        if (edit->info->scale != 0)
            fps = (double)edit->info->rate / (double)edit->info->scale;
        else if (edit->info->rate > 0)
            fps = (double)edit->info->rate;
    }

    int created_count = 0;
    int skipped_count = 0;

    for (size_t i = 0; i < events.size(); i++) {
        const RppEvent& ev = events[i];

        int frame = (int)floor(ev.position_sec * fps);
        int length_frames = (int)ceil(ev.length_sec * fps);
        if (length_frames < 1) length_frames = 1;

        OBJECT_HANDLE obj = edit->create_object(
            L"図形",
            ev.track,
            frame,
            length_frames
        );

        if (obj) {
            wchar_t obj_name[256];
            if (ev.pitch_shift != 0.0) {
                swprintf(obj_name, 256,
                    L"AutoZYC:%d (p=%.1f)", ev.track, ev.pitch_shift);
            } else {
                swprintf(obj_name, 256,
                    L"AutoZYC:%d", ev.track);
            }
            edit->set_object_name(obj, obj_name);
            created_count++;
        } else {
            skipped_count++;
        }
    }

    wchar_t msg[512];
    swprintf(msg, 512,
        L"物件生成完成!\n\n"
        L"已创建: %d 个物件\n"
        L"失败: %d 个\n"
        L"总事件: %zu 个\n\n"
        L"提示: 可用\"动画效果\"→"
        L"\"AutoZYC\\常用效果\"来驱动这些物件。",
        created_count, skipped_count, events.size());
    MessageBoxW(nullptr, msg, L"AutoZYC", MB_OK);
}

// ============================================================
// 菜单回调: 物件右键 → "从RPP生成物件"
// ============================================================
static void on_object_menu(EDIT_SECTION* edit) {
    LPCSTR saved_path = nullptr;
    if (g_edit) {
        PROJECT_FILE* pf = edit->get_project_file(g_edit);
        saved_path = load_config_str(pf, "autozic_rpp_path", "");
    }
    if (!saved_path || !*saved_path) {
        saved_path = g_last_rpp_path;
    }

    if (!saved_path || !*saved_path) {
        MessageBoxA(nullptr,
            "未设置RPP文件路径。\n\n"
            "请通过\"文件\"→\"导入\"→"
            "\"AutoZYC RPP文件\" 选择RPP文件。",
            "AutoZYC", MB_OK | MB_ICONINFORMATION);
        return;
    }

    generate_objects_from_path(edit, saved_path);
}

// ============================================================
// 菜单回调: 轨道右键 → "从RPP生成物件(当前轨)"
// ============================================================
static void on_layer_menu(EDIT_SECTION* edit) {
    on_object_menu(edit);
}

// ============================================================
// 菜单回调: 文件 → 导入 → "AutoZYC RPP文件"
// ============================================================
static void on_import_menu(EDIT_SECTION* edit) {
    OPENFILENAMEA ofn = {};
    char file[1024] = "";

    if (g_last_rpp_path[0]) {
        safe_strcpy(file, sizeof(file), g_last_rpp_path);
    }

    ofn.lStructSize = sizeof(OPENFILENAMEA);
    ofn.hwndOwner = g_edit ? g_edit->get_host_app_window() : nullptr;
    ofn.lpstrFilter =
        "REAPER项目文件 (*.rpp)\0*.rpp\0"
        "MIDI文件 (*.mid;*.midi;*.smf)\0*.mid;*.midi;*.smf\0"
        "所有文件 (*.*)\0*.*\0";
    ofn.lpstrFile = file;
    ofn.nMaxFile = sizeof(file);
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_HIDEREADONLY | OFN_PATHMUSTEXIST;
    ofn.lpstrTitle = "选择 RPP / MIDI 文件";

    if (GetOpenFileNameA(&ofn)) {
        safe_strcpy(g_last_rpp_path, sizeof(g_last_rpp_path), file);
        generate_objects_from_path(edit, file);
    }
}

// ============================================================
// 菜单回调: 设置 → "AutoZYC设置"
// ============================================================
static void on_config_menu(HWND hwnd, HINSTANCE dll_hinst) {
    char msg[1024];
    snprintf(msg, sizeof(msg),
        "=== AutoZYC v2.0 ===\n\n"
        "当前RPP路径:\n  %s\n\n"
        "使用方法:\n"
        "  1. 通过\"文件\"→\"导入\"→\"AutoZYC RPP文件\"\n"
        "     选择你的REAPER项目文件(.rpp)\n"
        "  2. 右键时间轴上的物件或轨道空白处\n"
        "     选择\"AutoZYC\"→\"从RPP生成物件\"\n"
        "  3. 物件将根据RPP中的音频事件\n"
        "     自动生成在对应轨道位置\n\n"
        "路径会随项目文件(.aup)一同保存。\n\n"
        "提示: 生成物件后, 可使用\n"
        "  \"动画效果\"→\"AutoZYC\\常用效果\"\n"
        "  来驱动物件动画。",
        (g_last_rpp_path[0] ? g_last_rpp_path : "(未设置)")
    );

    MessageBoxA(hwnd, msg, "AutoZYC 设置", MB_OK | MB_ICONINFORMATION);
}

// ============================================================
// 项目文件加载处理 (读取持久化配置)
// ============================================================
static void on_project_load(PROJECT_FILE* project) {
    LPCSTR path = load_config_str(project, "autozic_rpp_path", "");
    if (path && *path) {
        safe_strcpy(g_last_rpp_path, sizeof(g_last_rpp_path), path);
    }

    LPCSTR midi_path = load_config_str(project, "autozic_midi_path", "");
    if (midi_path && *midi_path) {
        safe_strcpy(g_last_midi_path, sizeof(g_last_midi_path), midi_path);
    }
}

// ============================================================
// 项目文件保存处理 (写入持久化配置)
// ============================================================
static void on_project_save(PROJECT_FILE* project) {
    if (g_last_rpp_path[0]) {
        save_config_str(project, "autozic_rpp_path", g_last_rpp_path);
    }
    if (g_last_midi_path[0]) {
        save_config_str(project, "autozic_midi_path", g_last_midi_path);
    }
}

// ============================================================
// 插件注册 (TYPE_PLUGIN_COMMON = 9)
// ============================================================
extern "C" __declspec(dllexport) void RegisterPlugin(HOST_APP_TABLE* host) {
    g_host = host;

    host->set_plugin_information(
        L"AutoZYC v2.0 - 音MAD自动物件批量生成 + 项目配置持久化"
    );

    host->register_object_menu(
        L"AutoZYC\\从RPP生成物件",
        on_object_menu
    );

    host->register_layer_menu(
        L"AutoZYC\\从RPP生成物件(当前轨)",
        on_layer_menu
    );

    host->register_config_menu(
        L"AutoZYC设置",
        on_config_menu
    );

    host->register_import_menu(
        L"AutoZYC RPP文件...",
        on_import_menu
    );

    host->register_project_load_handler(on_project_load);
    host->register_project_save_handler(on_project_save);

    g_edit = host->create_edit_handle();
}

// ============================================================
// 插件初始化
// ============================================================
extern "C" __declspec(dllexport) bool InitializePlugin(DWORD version) {
    return true;
}

// ============================================================
// 插件卸载
// ============================================================
extern "C" __declspec(dllexport) void UninitializePlugin() {
    g_host = nullptr;
    g_edit = nullptr;
}

// ============================================================
// 可选导出: GetCommonPluginTable
// ============================================================
extern "C" __declspec(dllexport) COMMON_PLUGIN_TABLE* GetCommonPluginTable() {
    static COMMON_PLUGIN_TABLE table = {
        L"AutoZYC v2.0",
        L"音MAD自动物件生成 - RPP/MIDI解析驱动的批量物件生成与配置持久化"
    };
    return &table;
}
