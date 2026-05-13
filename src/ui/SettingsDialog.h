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

// 控件 ID
#define IDC_EDIT_PATH   1001
#define IDC_BROWSE      1002
#define IDC_EDIT_OFFSET 1003
#define IDC_EDIT_BPM    1004

class SettingsDialog
{
public:
    // 显示模态工程设置对话框
    // hwndParent: 父窗口句柄（可为 nullptr）
    // config: 配置对象引用，确定时保存，取消时丢弃
    // 返回: true=用户按了确定, false=取消或出错
    static bool show(HWND hwndParent, ProjectConfig& config);

private:
    // 窗口过程（静态，供 RegisterClass 使用）
    static LRESULT CALLBACK wndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);

    // WM_CREATE 处理：创建子控件
    static void onCreate(HWND hwnd);

    // 从 config 加载值填充到控件
    static void loadFromConfig(HWND hwnd);

    // 从控件读取值保存到 config
    static void saveToConfig(HWND hwnd);

    // 浏览按钮处理：打开 GetOpenFileNameW 对话框
    static void onBrowseFile(HWND hwnd);

    // 对话框布局常量
    static constexpr int DIALOG_W = 480;
    static constexpr int DIALOG_H = 220;
    static constexpr int MARGIN = 12;
    static constexpr int ROW_H = 24;
    static constexpr int LABEL_W = 110;
    static constexpr int EDIT_W = 280;
    static constexpr int BTN_W = 80;
    static constexpr int BTN_H = 26;

    // 运行时配置指针（在 show() 中设置，在 wndProc 中使用）
    static ProjectConfig* s_config;
    static HWND s_hwndSelf;       // 自己的窗口句柄（用于 WM_CREATE 时获取）
};