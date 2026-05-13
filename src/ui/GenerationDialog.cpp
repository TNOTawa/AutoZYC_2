// AutoZYC.aux2 — 序列物件生成对话框（Win32 实现）
#include "src/ui/GenerationDialog.h"
#include <commctrl.h>
#include <cstdio>
#include <cstdlib>
#include <cwchar>
#include <string>

// ---------------------------------------------------------------------------
// 静态成员初始化
// ---------------------------------------------------------------------------
GenerationConfig* GenerationDialog::s_config = nullptr;
bool  GenerationDialog::s_confirmed = false;
int   GenerationDialog::s_selectedCount = 0;
HWND  GenerationDialog::s_hwndSelf = nullptr;
bool  GenerationDialog::s_advancedExpanded = false;

// 窗口类名
static const wchar_t* CLASS_NAME = L"AutoZYC_GenerationDialog";

// ---------------------------------------------------------------------------
// 辅助函数：创建带字体的控件
// ---------------------------------------------------------------------------
static HWND createLabel(HWND parent, const wchar_t* text, int x, int y, int w, int h, HINSTANCE hInst, HFONT hFont)
{
    HWND ctrl = CreateWindowExW(0, L"STATIC", text,
        WS_CHILD | WS_VISIBLE, x, y, w, h,
        parent, nullptr, hInst, nullptr);
    SendMessageW(ctrl, WM_SETFONT, reinterpret_cast<WPARAM>(hFont), TRUE);
    return ctrl;
}

static HWND createEdit(HWND parent, int id, int x, int y, int w, int h, HINSTANCE hInst, HFONT hFont, bool readOnly = false)
{
    DWORD style = WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL | WS_TABSTOP;
    if (readOnly) style |= ES_READONLY;
    HWND ctrl = CreateWindowExW(0, L"EDIT", L"",
        style, x, y, w, h,
        parent, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), hInst, nullptr);
    SendMessageW(ctrl, WM_SETFONT, reinterpret_cast<WPARAM>(hFont), TRUE);
    return ctrl;
}

static HWND createButton(HWND parent, const wchar_t* text, int id, int x, int y, int w, int h,
                         HINSTANCE hInst, HFONT hFont, DWORD extraStyle = BS_PUSHBUTTON)
{
    DWORD style = WS_CHILD | WS_VISIBLE | extraStyle | WS_TABSTOP;
    HWND ctrl = CreateWindowExW(0, L"BUTTON", text,
        style, x, y, w, h,
        parent, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), hInst, nullptr);
    SendMessageW(ctrl, WM_SETFONT, reinterpret_cast<WPARAM>(hFont), TRUE);
    return ctrl;
}

static HWND createCombo(HWND parent, int id, int x, int y, int w, int h,
                        HINSTANCE hInst, HFONT hFont)
{
    DWORD style = WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP;
    HWND ctrl = CreateWindowExW(0, L"COMBOBOX", L"",
        style, x, y, w, h + 100,  // +100 for dropdown height
        parent, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), hInst, nullptr);
    SendMessageW(ctrl, WM_SETFONT, reinterpret_cast<WPARAM>(hFont), TRUE);
    return ctrl;
}

static HWND createCheckbox(HWND parent, const wchar_t* text, int id, int x, int y, int w, int h,
                           HINSTANCE hInst, HFONT hFont)
{
    return createButton(parent, text, id, x, y, w, h, hInst, hFont, BS_AUTOCHECKBOX);
}

// ---------------------------------------------------------------------------
// show() — 创建模态对话框并进入消息循环
// ---------------------------------------------------------------------------
bool GenerationDialog::show(HWND parent, GenerationConfig& config, int selectedObjCount)
{
    s_config = &config;
    s_confirmed = false;
    s_selectedCount = selectedObjCount;
    s_hwndSelf = nullptr;
    s_advancedExpanded = false;

    // 注册窗口类（仅首次）
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.lpfnWndProc = wndProc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);
    wc.lpszClassName = CLASS_NAME;
    wc.style = CS_HREDRAW | CS_VREDRAW;

    if (!RegisterClassExW(&wc)) {
        if (GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
            s_config = nullptr;
            return false;
        }
    }

    // 创建窗口（初始为折叠高度）
    HWND hwnd = CreateWindowExW(
        WS_EX_DLGMODALFRAME | WS_EX_CONTROLPARENT,
        CLASS_NAME,
        L"AutoZYC - \u751F\u6210\u5E8F\u5217\u7269\u4EF6",
        WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT,
        DIALOG_W, DIALOG_H_COLLAPSED,
        parent,
        nullptr,
        GetModuleHandleW(nullptr),
        nullptr
    );

    if (!hwnd) {
        s_config = nullptr;
        return false;
    }

    // 居中窗口
    if (parent) {
        RECT rcParent, rcSelf;
        GetWindowRect(parent, &rcParent);
        GetWindowRect(hwnd, &rcSelf);
        int x = rcParent.left + ((rcParent.right - rcParent.left) - (rcSelf.right - rcSelf.left)) / 2;
        int y = rcParent.top + ((rcParent.bottom - rcParent.top) - (rcSelf.bottom - rcSelf.top)) / 2;
        SetWindowPos(hwnd, nullptr, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
    }

    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    // 消息循环
    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);

        if (!s_hwndSelf)
            break;
    }

    s_config = nullptr;
    return s_confirmed;
}

// ---------------------------------------------------------------------------
// wndProc — 窗口过程
// ---------------------------------------------------------------------------
LRESULT CALLBACK GenerationDialog::wndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_CREATE:
        s_hwndSelf = hwnd;
        onCreate(hwnd);
        loadConfig(hwnd, *s_config);
        return 0;

    case WM_COMMAND: {
        WORD cmd = LOWORD(wp);
        WORD notify = HIWORD(wp);

        switch (cmd) {
        case IDC_GEN_TOGGLE_ADV:
            toggleAdvanced(hwnd);
            return 0;

        case IDC_GEN_LAYER_STRAT:
            if (notify == CBN_SELCHANGE)
                onStrategyChange(hwnd);
            return 0;

        case IDOK:
            saveConfig(hwnd, *s_config);
            s_confirmed = true;
            DestroyWindow(hwnd);
            return 0;

        case IDCANCEL:
            s_confirmed = false;
            DestroyWindow(hwnd);
            return 0;
        }
        break;
    }

    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;

    case WM_DESTROY:
        s_hwndSelf = nullptr;
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProcW(hwnd, msg, wp, lp);
}

// ---------------------------------------------------------------------------
// onCreate — 创建所有子控件（包含基础和高级配置）
// ---------------------------------------------------------------------------
void GenerationDialog::onCreate(HWND hwnd)
{
    HINSTANCE hInst = GetModuleHandleW(nullptr);
    HFONT hFont = reinterpret_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));

    int y = MARGIN;

    // ================================================================
    // 状态栏：已选物件数量
    // ================================================================
    wchar_t statusBuf[128];
    swprintf(statusBuf, 128, L"\u5DF2\u9009\u7269\u4EF6: %d \u4E2A", s_selectedCount);
    createLabel(hwnd, statusBuf, MARGIN, y + 2, 300, ROW_H, hInst, hFont);

    y += ROW_H + 10;

    // ================================================================
    // 基础配置区域 (y=36 ~ 150)
    // ================================================================

    // --- 映射区间 ---
    createLabel(hwnd, L"\u6620\u5C04\u533A\u95F4 (\u79D2):", MARGIN, y + 2, LABEL_W, ROW_H, hInst, hFont);
    createEdit(hwnd, IDC_GEN_RANGE_START, MARGIN + LABEL_W + 6, y, EDIT_W_SM, ROW_H, hInst, hFont);
    createLabel(hwnd, L"~", MARGIN + LABEL_W + 6 + EDIT_W_SM + 4, y + 2, 16, ROW_H, hInst, hFont);
    createEdit(hwnd, IDC_GEN_RANGE_END, MARGIN + LABEL_W + 6 + EDIT_W_SM + 22, y, EDIT_W_SM, ROW_H, hInst, hFont);

    y += ROW_H + 10;

    // --- 图层策略 ---
    createLabel(hwnd, L"\u56FE\u5C42\u7B56\u7565:", MARGIN, y + 2, LABEL_W, ROW_H, hInst, hFont);
    HWND hLayerStrat = createCombo(hwnd, IDC_GEN_LAYER_STRAT,
        MARGIN + LABEL_W + 6, y, 160, ROW_H, hInst, hFont);
    SendMessageW(hLayerStrat, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"\u8D2A\u5FC3\u81EA\u52A8"));
    SendMessageW(hLayerStrat, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"\u6307\u5B9A\u8D77\u59CB\u56FE\u5C42"));

    y += ROW_H + 10;

    // --- 起始图层 ---
    createLabel(hwnd, L"\u8D77\u59CB\u56FE\u5C42:", MARGIN, y + 2, LABEL_W, ROW_H, hInst, hFont);
    createEdit(hwnd, IDC_GEN_START_LAYER, MARGIN + LABEL_W + 6, y, EDIT_W_SM, ROW_H, hInst, hFont);

    y += ROW_H + 10;

    // --- 翻转模式 ---
    createLabel(hwnd, L"\u7FFB\u8F6C\u6A21\u5F0F:", MARGIN, y + 2, LABEL_W, ROW_H, hInst, hFont);
    HWND hFlipMode = createCombo(hwnd, IDC_GEN_FLIP_MODE,
        MARGIN + LABEL_W + 6, y, 160, ROW_H, hInst, hFont);
    SendMessageW(hFlipMode, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"\u65E0\u7FFB\u8F6C"));
    SendMessageW(hFlipMode, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"\u5DE6\u53F3\u4EA4\u66FF"));
    SendMessageW(hFlipMode, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"\u4E0A\u4E0B\u4EA4\u66FF"));
    SendMessageW(hFlipMode, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"\u987A\u65F6\u9488\u56DB\u6B65"));
    SendMessageW(hFlipMode, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"\u9006\u65F6\u9488\u56DB\u6B65"));

    y += ROW_H + 10;

    // --- 间隔偏移 ---
    createLabel(hwnd, L"\u95F4\u9694\u504F\u79FB (\u5E27):", MARGIN, y + 2, LABEL_W, ROW_H, hInst, hFont);
    createEdit(hwnd, IDC_GEN_INTERVAL, MARGIN + LABEL_W + 6, y, EDIT_W_SM, ROW_H, hInst, hFont);

    y += ROW_H + 10;

    // ================================================================
    // 高级配置分隔线 + 展开/折叠按钮
    // ================================================================
    createButton(hwnd, L"\u9AD8\u7EA7 >>", IDC_GEN_TOGGLE_ADV,
        MARGIN, y, 100, BTN_H, hInst, hFont);

    y += BTN_H + 12;

    // ================================================================
    // 高级配置区域 (初始隐藏，折叠态)
    // ================================================================
    int advY = y;

    // --- 累计缩放 ---
    createLabel(hwnd, L"\u7D2F\u8BA1\u7F29\u653E(%):", MARGIN, advY + 2, LABEL_W, ROW_H, hInst, hFont);
    HWND hAdvanced = createEdit(hwnd, IDC_GEN_STEP_SCALE, MARGIN + LABEL_W + 6, advY, EDIT_W_SM, ROW_H, hInst, hFont);
    ShowWindow(GetDlgItem(hwnd, IDC_GEN_STEP_SCALE), SW_HIDE);
    // Hide the label too — we need to track handles, so hide all by ID
    ShowWindow(GetDlgItem(hwnd, IDC_GEN_STEP_SCALE), SW_HIDE);
    advY += ROW_H + 8;

    // --- 累计旋转 ---
    createLabel(hwnd, L"\u7D2F\u8BA1\u65CB\u8F6C(\u00B0):", MARGIN, advY + 2, LABEL_W, ROW_H, hInst, hFont);
    createEdit(hwnd, IDC_GEN_STEP_ROT, MARGIN + LABEL_W + 6, advY, EDIT_W_SM, ROW_H, hInst, hFont);
    advY += ROW_H + 8;

    // --- 累计偏移X ---
    createLabel(hwnd, L"\u7D2F\u8BA1\u504F\u79FB X:", MARGIN, advY + 2, LABEL_W, ROW_H, hInst, hFont);
    createEdit(hwnd, IDC_GEN_STEP_OFFX, MARGIN + LABEL_W + 6, advY, EDIT_W_SM, ROW_H, hInst, hFont);
    advY += ROW_H + 8;

    // --- 累计偏移Y ---
    createLabel(hwnd, L"\u7D2F\u8BA1\u504F\u79FB Y:", MARGIN, advY + 2, LABEL_W, ROW_H, hInst, hFont);
    createEdit(hwnd, IDC_GEN_STEP_OFFY, MARGIN + LABEL_W + 6, advY, EDIT_W_SM, ROW_H, hInst, hFont);
    advY += ROW_H + 12;

    // --- 最大物件数 ---
    createLabel(hwnd, L"\u6700\u5927\u7269\u4EF6\u6570 (0=\u65E0\u9650):", MARGIN, advY + 2, 160, ROW_H, hInst, hFont);
    createEdit(hwnd, IDC_GEN_MAX_COUNT, MARGIN + 166, advY, EDIT_W_SM, ROW_H, hInst, hFont);
    advY += ROW_H + 8;

    // --- 生存时间 ---
    createLabel(hwnd, L"\u751F\u5B58\u65F6\u95F4 (\u79D2, 0=\u65E0\u9650):", MARGIN, advY + 2, 160, ROW_H, hInst, hFont);
    createEdit(hwnd, IDC_GEN_MAX_LIFE, MARGIN + 166, advY, EDIT_W_SM, ROW_H, hInst, hFont);
    advY += ROW_H + 12;

    // --- 冻结状态 ---
    createCheckbox(hwnd, L"\u51BB\u7ED3\u72B6\u6001", IDC_GEN_FREEZE,
        MARGIN, advY + 2, 120, ROW_H, hInst, hFont);
    advY += ROW_H + 8;

    // --- BPM对齐 ---
    createCheckbox(hwnd, L"BPM\u5BF9\u9F50", IDC_GEN_BPM_ALIGN,
        MARGIN, advY + 2, 120, ROW_H, hInst, hFont);

    // 初始状态：隐藏所有高级控件
    // Advanced control IDs: 2009-2016
    // Advanced labels are at specific positions, we hide by ID range
    // We hide ALL controls whose ID is >= IDC_GEN_TOGGLE_ADV+1 (i.e. all advanced)
    // But IDC_GEN_TOGGLE_ADV is the toggle button itself, keep it visible
    
    // ================================================================
    // 底部按钮 (y = DIALOG_H_COLLAPSED - BTN_H - MARGIN - 8)
    // ================================================================
    int bottomY = DIALOG_H_COLLAPSED - BTN_H - MARGIN - 8;

    createButton(hwnd, L"\u786E\u5B9A", IDOK,
        DIALOG_W - BTN_W * 2 - MARGIN - 12, bottomY, BTN_W, BTN_H, hInst, hFont);
    createButton(hwnd, L"\u53D6\u6D88", IDCANCEL,
        DIALOG_W - BTN_W - MARGIN, bottomY, BTN_W, BTN_H, hInst, hFont);
}

// ---------------------------------------------------------------------------
// loadConfig — 从配置对象填充控件值
// ---------------------------------------------------------------------------
void GenerationDialog::loadConfig(HWND hwnd, const GenerationConfig& cfg)
{
    wchar_t buf[128];

    // 映射区间-起始
    swprintf(buf, 128, L"%.3f", cfg.range_start_sec);
    SetDlgItemTextW(hwnd, IDC_GEN_RANGE_START, buf);

    // 映射区间-结束
    swprintf(buf, 128, L"%.3f", cfg.range_end_sec);
    SetDlgItemTextW(hwnd, IDC_GEN_RANGE_END, buf);

    // 图层策略
    SendDlgItemMessageW(hwnd, IDC_GEN_LAYER_STRAT, CB_SETCURSEL, cfg.layer_strategy, 0);

    // 起始图层
    swprintf(buf, 128, L"%d", cfg.start_layer);
    SetDlgItemTextW(hwnd, IDC_GEN_START_LAYER, buf);

    // 根据策略启用/禁用起始图层编辑框
    EnableWindow(GetDlgItem(hwnd, IDC_GEN_START_LAYER), cfg.layer_strategy == 1);

    // 翻转模式
    int flipIdx = cfg.flip_mode;
    if (flipIdx < 0) flipIdx = 0;
    if (flipIdx > 4) flipIdx = 4;
    SendDlgItemMessageW(hwnd, IDC_GEN_FLIP_MODE, CB_SETCURSEL, flipIdx, 0);

    // 间隔偏移
    swprintf(buf, 128, L"%d", cfg.interval_frames);
    SetDlgItemTextW(hwnd, IDC_GEN_INTERVAL, buf);

    // 累计缩放
    swprintf(buf, 128, L"%.2f", cfg.step_scale);
    SetDlgItemTextW(hwnd, IDC_GEN_STEP_SCALE, buf);

    // 累计旋转
    swprintf(buf, 128, L"%.2f", cfg.step_rotation);
    SetDlgItemTextW(hwnd, IDC_GEN_STEP_ROT, buf);

    // 累计偏移 X
    swprintf(buf, 128, L"%.2f", cfg.step_offset_x);
    SetDlgItemTextW(hwnd, IDC_GEN_STEP_OFFX, buf);

    // 累计偏移 Y
    swprintf(buf, 128, L"%.2f", cfg.step_offset_y);
    SetDlgItemTextW(hwnd, IDC_GEN_STEP_OFFY, buf);

    // 最大物件数
    swprintf(buf, 128, L"%d", cfg.max_visible_count);
    SetDlgItemTextW(hwnd, IDC_GEN_MAX_COUNT, buf);

    // 生存时间
    swprintf(buf, 128, L"%.3f", cfg.max_lifetime_sec);
    SetDlgItemTextW(hwnd, IDC_GEN_MAX_LIFE, buf);

    // 冻结状态
    SendDlgItemMessageW(hwnd, IDC_GEN_FREEZE, BM_SETCHECK,
        cfg.freeze_state ? BST_CHECKED : BST_UNCHECKED, 0);

    // BPM对齐
    SendDlgItemMessageW(hwnd, IDC_GEN_BPM_ALIGN, BM_SETCHECK,
        cfg.bpm_align ? BST_CHECKED : BST_UNCHECKED, 0);
}

// ---------------------------------------------------------------------------
// saveConfig — 从控件读取值保存到配置对象
// ---------------------------------------------------------------------------
void GenerationDialog::saveConfig(HWND hwnd, GenerationConfig& cfg)
{
    wchar_t buf[128];

    // 映射区间-起始
    if (GetDlgItemTextW(hwnd, IDC_GEN_RANGE_START, buf, 128) > 0)
        cfg.range_start_sec = _wtof(buf);

    // 映射区间-结束
    if (GetDlgItemTextW(hwnd, IDC_GEN_RANGE_END, buf, 128) > 0)
        cfg.range_end_sec = _wtof(buf);

    // 图层策略
    LRESULT stratSel = SendDlgItemMessageW(hwnd, IDC_GEN_LAYER_STRAT, CB_GETCURSEL, 0, 0);
    if (stratSel != CB_ERR)
        cfg.layer_strategy = static_cast<int>(stratSel);

    // 起始图层
    if (GetDlgItemTextW(hwnd, IDC_GEN_START_LAYER, buf, 128) > 0)
        cfg.start_layer = _wtoi(buf);
    if (cfg.start_layer < 1) cfg.start_layer = 1;

    // 翻转模式
    LRESULT flipSel = SendDlgItemMessageW(hwnd, IDC_GEN_FLIP_MODE, CB_GETCURSEL, 0, 0);
    if (flipSel != CB_ERR)
        cfg.flip_mode = static_cast<int>(flipSel);

    // 间隔偏移
    if (GetDlgItemTextW(hwnd, IDC_GEN_INTERVAL, buf, 128) > 0)
        cfg.interval_frames = _wtoi(buf);
    if (cfg.interval_frames < 0) cfg.interval_frames = 0;

    // 累计缩放
    if (GetDlgItemTextW(hwnd, IDC_GEN_STEP_SCALE, buf, 128) > 0)
        cfg.step_scale = _wtof(buf);
    if (cfg.step_scale <= 0.0) cfg.step_scale = 100.0;

    // 累计旋转
    if (GetDlgItemTextW(hwnd, IDC_GEN_STEP_ROT, buf, 128) > 0)
        cfg.step_rotation = _wtof(buf);

    // 累计偏移 X
    if (GetDlgItemTextW(hwnd, IDC_GEN_STEP_OFFX, buf, 128) > 0)
        cfg.step_offset_x = _wtof(buf);

    // 累计偏移 Y
    if (GetDlgItemTextW(hwnd, IDC_GEN_STEP_OFFY, buf, 128) > 0)
        cfg.step_offset_y = _wtof(buf);

    // 最大物件数
    if (GetDlgItemTextW(hwnd, IDC_GEN_MAX_COUNT, buf, 128) > 0)
        cfg.max_visible_count = _wtoi(buf);
    if (cfg.max_visible_count < 0) cfg.max_visible_count = 0;

    // 生存时间
    if (GetDlgItemTextW(hwnd, IDC_GEN_MAX_LIFE, buf, 128) > 0)
        cfg.max_lifetime_sec = _wtof(buf);
    if (cfg.max_lifetime_sec < 0.0) cfg.max_lifetime_sec = 0.0;

    // 冻结状态
    cfg.freeze_state = (SendDlgItemMessageW(hwnd, IDC_GEN_FREEZE, BM_GETCHECK, 0, 0) == BST_CHECKED);

    // BPM对齐
    cfg.bpm_align = (SendDlgItemMessageW(hwnd, IDC_GEN_BPM_ALIGN, BM_GETCHECK, 0, 0) == BST_CHECKED);
}

// ---------------------------------------------------------------------------
// toggleAdvanced — 展开/折叠高级配置
// ---------------------------------------------------------------------------
void GenerationDialog::toggleAdvanced(HWND hwnd)
{
    s_advancedExpanded = !s_advancedExpanded;

    // 更新按钮文字
    SetDlgItemTextW(hwnd, IDC_GEN_TOGGLE_ADV,
        s_advancedExpanded ? L"\u9AD8\u7EA7 <<" : L"\u9AD8\u7EA7 >>");

    // 高级控件 ID 范围: 2009 ~ 2016
    static const int advancedIDs[] = {
        IDC_GEN_STEP_SCALE, IDC_GEN_STEP_ROT,
        IDC_GEN_STEP_OFFX, IDC_GEN_STEP_OFFY,
        IDC_GEN_MAX_COUNT, IDC_GEN_MAX_LIFE,
        IDC_GEN_FREEZE, IDC_GEN_BPM_ALIGN
    };
    static const int advancedCount = 8;

    int show = s_advancedExpanded ? SW_SHOW : SW_HIDE;

    // 显示/隐藏所有高级控件
    for (int i = 0; i < advancedCount; i++) {
        HWND ctrl = GetDlgItem(hwnd, advancedIDs[i]);
        if (ctrl) ShowWindow(ctrl, show);

        // 同时查找对应标签（标签在高级控件的左边）
        // 标签没有 ID，我们通过遍历子窗口来处理
    }

    // 同时需要隐藏标签。由于标签是 STATIC 控件没有 ID，
    // 我们需要遍历所有子控件，对位于高级区域的标签进行显示/隐藏
    HWND child = GetWindow(hwnd, GW_CHILD);
    while (child) {
        wchar_t className[32];
        GetClassNameW(child, className, 32);
        if (wcscmp(className, L"Static") == 0) {
            RECT rc;
            GetWindowRect(child, &rc);
            POINT pt = { rc.left, rc.top };
            ScreenToClient(hwnd, &pt);
            // 高级区域 y 范围: 约 160 ~ 380 (相对于折叠窗口)
            // 标签在高级控件的同一行
            int ctrlId = GetDlgCtrlID(child);
            bool isAdvanced = false;
            for (int i = 0; i < advancedCount; i++) {
                if (ctrlId == advancedIDs[i]) { isAdvanced = true; break; }
            }
            // 对于没有 ID 的 STATIC 控件，根据 Y 坐标判断
            if (!isAdvanced && ctrlId == 0) {
                // STATIC labels without ID that are in the advanced section
                if (pt.y >= 160 && pt.y <= 380) {
                    ShowWindow(child, show);
                }
            }
        }
        child = GetWindow(child, GW_HWNDNEXT);
    }

    // 调整对话框尺寸
    int newHeight = s_advancedExpanded ? DIALOG_H_EXPANDED : DIALOG_H_COLLAPSED;

    RECT rc;
    GetWindowRect(hwnd, &rc);
    SetWindowPos(hwnd, nullptr, 0, 0,
        rc.right - rc.left, newHeight,
        SWP_NOMOVE | SWP_NOZORDER);

    // 底部按钮需要在展开/折叠时重新定位
    int bottomY = newHeight - BTN_H - MARGIN - 8;
    HWND hOk = GetDlgItem(hwnd, IDOK);
    HWND hCancel = GetDlgItem(hwnd, IDCANCEL);
    if (hOk) SetWindowPos(hOk, nullptr,
        DIALOG_W - BTN_W * 2 - MARGIN - 12, bottomY, 0, 0,
        SWP_NOSIZE | SWP_NOZORDER);
    if (hCancel) SetWindowPos(hCancel, nullptr,
        DIALOG_W - BTN_W - MARGIN, bottomY, 0, 0,
        SWP_NOSIZE | SWP_NOZORDER);

    // 无效化窗口以触发重绘
    InvalidateRect(hwnd, nullptr, TRUE);
}

// ---------------------------------------------------------------------------
// onStrategyChange — 图层策略变更处理
// ---------------------------------------------------------------------------
void GenerationDialog::onStrategyChange(HWND hwnd)
{
    LRESULT sel = SendDlgItemMessageW(hwnd, IDC_GEN_LAYER_STRAT, CB_GETCURSEL, 0, 0);
    if (sel != CB_ERR) {
        // strategy=1（指定起始图层）时启用起始图层编辑框
        BOOL enable = (sel == 1) ? TRUE : FALSE;
        EnableWindow(GetDlgItem(hwnd, IDC_GEN_START_LAYER), enable);
    }
}
