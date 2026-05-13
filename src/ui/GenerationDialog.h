// AutoZYC.aux2 — 序列物件生成对话框头文件
#pragma once

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#include <windows.h>
#include "src/core/EventTypes.h"

// 控件 ID
#define IDC_GEN_STATUS      2001
#define IDC_GEN_RANGE_START 2002
#define IDC_GEN_RANGE_END   2003
#define IDC_GEN_LAYER_STRAT 2004
#define IDC_GEN_START_LAYER 2005
#define IDC_GEN_FLIP_MODE   2006
#define IDC_GEN_INTERVAL    2007
#define IDC_GEN_TOGGLE_ADV  2008
#define IDC_GEN_STEP_SCALE  2009
#define IDC_GEN_STEP_ROT    2010
#define IDC_GEN_STEP_OFFX   2011
#define IDC_GEN_STEP_OFFY   2012
#define IDC_GEN_MAX_COUNT   2013
#define IDC_GEN_MAX_LIFE    2014
#define IDC_GEN_FREEZE      2015
#define IDC_GEN_BPM_ALIGN   2016

class GenerationDialog
{
public:
    // 显示模态生成配置对话框
    // parent:            父窗口句柄（可为 nullptr）
    // config:            配置对象引用，确定时保存，取消时丢弃
    // selectedObjCount:  已选物件数量（显示在状态栏）
    // 返回: true=用户按了确定, false=取消或出错
    static bool show(HWND parent, GenerationConfig& config, int selectedObjCount);

private:
    // 窗口过程（静态，供 RegisterClass 使用）
    static LRESULT CALLBACK wndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);

    // WM_CREATE 处理：创建所有子控件
    static void onCreate(HWND hwnd);

    // 从 config 加载值填充到控件
    static void loadConfig(HWND hwnd, const GenerationConfig& cfg);

    // 从控件读取值保存到 config
    static void saveConfig(HWND hwnd, GenerationConfig& cfg);

    // 切换高级配置展开/折叠
    static void toggleAdvanced(HWND hwnd);

    // 图层策略变更时启用/禁用起始图层编辑框
    static void onStrategyChange(HWND hwnd);

    // 对话框布局常量
    static constexpr int DIALOG_W = 520;
    static constexpr int DIALOG_H_COLLAPSED = 260;
    static constexpr int DIALOG_H_EXPANDED = 440;
    static constexpr int MARGIN = 12;
    static constexpr int ROW_H = 22;
    static constexpr int LABEL_W = 120;
    static constexpr int EDIT_W_SM = 80;
    static constexpr int EDIT_W_MD = 140;
    static constexpr int BTN_W = 80;
    static constexpr int BTN_H = 26;

    // 运行时状态（在 show() 中设置，在 wndProc 中使用）
    static GenerationConfig* s_config;
    static bool s_confirmed;
    static int  s_selectedCount;
    static HWND s_hwndSelf;
    static bool s_advancedExpanded;
};
