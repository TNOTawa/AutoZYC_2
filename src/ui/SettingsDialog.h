// AutoZYC.aux2 — 工程设置对话框头文件
#pragma once
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#include <windows.h>
#include "src/plugin/ProjectConfig.h"

#define IDC_EDIT_PATH   1001
#define IDC_BROWSE      1002
#define IDC_EDIT_OFFSET 1003
#define IDC_EDIT_BPM    1004

class SettingsDialog
{
public:
    static bool show(HWND hwndParent, ProjectConfig& config);
private:
    static LRESULT CALLBACK wndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
    static void onCreate(HWND hwnd);
    static void loadFromConfig(HWND hwnd);
    static void saveToConfig(HWND hwnd);
    static void onBrowseFile(HWND hwnd);
    static constexpr int DIALOG_W = 520;
    static constexpr int DIALOG_H = 280;
    static constexpr int MARGIN = 14;
    static constexpr int ROW_H = 24;
    static constexpr int LABEL_W = 120;
    static constexpr int R1_EDIT_W = 260;
    static constexpr int BTN_W = 80;
    static constexpr int BTN_H = 28;
    static ProjectConfig* s_config;
    static HWND s_hwndSelf;
};
