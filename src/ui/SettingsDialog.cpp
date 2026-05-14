// AutoZYC.aux2 — 工程设置对话框（Win32 实现）
#include "src/ui/SettingsDialog.h"
#include "src/core/Logger.h"
#include <commctrl.h>
#include <commdlg.h>
#include <cstdio>
#include <cstdlib>
#include <string>

// ---------------------------------------------------------------------------
// 静态成员初始化
// ---------------------------------------------------------------------------
ProjectConfig* SettingsDialog::s_config = nullptr;
HWND SettingsDialog::s_hwndSelf = nullptr;

static const wchar_t* CLASS_NAME = L"AutoZYC_SettingsDialog";

// ---------------------------------------------------------------------------
// show() — 创建模态对话框并进入消息循环
// ---------------------------------------------------------------------------
bool SettingsDialog::show(HWND hwndParent, ProjectConfig& config)
{
    AUTOZYC_LOG_INFO("SettingsDialog::show() called, parent=0x%p", hwndParent);

    s_config = &config;
    s_hwndSelf = nullptr;

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
            AUTOZYC_LOG_ERROR("RegisterClassExW failed, error=%lu", GetLastError());
            s_config = nullptr;
            return false;
        }
    }

    HWND hwnd = CreateWindowExW(
        WS_EX_DLGMODALFRAME | WS_EX_CONTROLPARENT,
        CLASS_NAME,
        L"AutoZYC 工程设置",
        WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT,
        DIALOG_W, DIALOG_H,
        hwndParent,
        nullptr,
        GetModuleHandleW(nullptr),
        nullptr
    );

    if (!hwnd) {
        AUTOZYC_LOG_ERROR("CreateWindowExW failed, error=%lu", GetLastError());
        s_config = nullptr;
        return false;
    }

    // 居中窗口
    if (hwndParent) {
        RECT rcParent, rcSelf;
        GetWindowRect(hwndParent, &rcParent);
        GetWindowRect(hwnd, &rcSelf);
        int x = rcParent.left + ((rcParent.right - rcParent.left) - (rcSelf.right - rcSelf.left)) / 2;
        int y = rcParent.top + ((rcParent.bottom - rcParent.top) - (rcSelf.bottom - rcSelf.top)) / 2;
        SetWindowPos(hwnd, nullptr, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
    }

    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    bool result = false;
    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
        if (!s_hwndSelf)
            break;
    }

    AUTOZYC_LOG_INFO("SettingsDialog closed, result=%d", result);
    s_config = nullptr;
    return result;
}

// ---------------------------------------------------------------------------
// wndProc — 窗口过程
// ---------------------------------------------------------------------------
LRESULT CALLBACK SettingsDialog::wndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_CREATE:
        AUTOZYC_LOG_DEBUG("SettingsDialog WM_CREATE");
        s_hwndSelf = hwnd;
        onCreate(hwnd);
        loadFromConfig(hwnd);
        return 0;

    case WM_COMMAND: {
        WORD cmd = LOWORD(wp);
        AUTOZYC_LOG_DEBUG("SettingsDialog WM_COMMAND cmd=%d", cmd);
        switch (cmd) {
        case IDC_BROWSE:
            AUTOZYC_LOG_INFO("SettingsDialog: browse button clicked");
            onBrowseFile(hwnd);
            return 0;

        case IDOK:
            AUTOZYC_LOG_INFO("SettingsDialog: OK clicked");
            saveToConfig(hwnd);
            DestroyWindow(hwnd);
            return 0;

        case IDCANCEL:
            AUTOZYC_LOG_INFO("SettingsDialog: Cancel clicked");
            DestroyWindow(hwnd);
            return 0;
        }
        break;
    }

    case WM_CLOSE:
        AUTOZYC_LOG_DEBUG("SettingsDialog WM_CLOSE");
        DestroyWindow(hwnd);
        return 0;

    case WM_DESTROY:
        AUTOZYC_LOG_DEBUG("SettingsDialog WM_DESTROY");
        s_hwndSelf = nullptr;
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProcW(hwnd, msg, wp, lp);
}

// ---------------------------------------------------------------------------
// onCreate — 创建所有子控件
// ---------------------------------------------------------------------------
void SettingsDialog::onCreate(HWND hwnd)
{
    AUTOZYC_LOG_DEBUG("SettingsDialog::onCreate");
    HINSTANCE hInst = GetModuleHandleW(nullptr);
    HFONT hFont = reinterpret_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));

    int y = MARGIN;

    // --- 第 1 行：音频工程文件 ---
    CreateWindowExW(0, L"STATIC", L"音频工程文件:",
        WS_CHILD | WS_VISIBLE, MARGIN, y + 2, LABEL_W, ROW_H,
        hwnd, nullptr, hInst, nullptr);

    HWND hEditPath = CreateWindowExW(0, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL | WS_TABSTOP,
        MARGIN + LABEL_W + 6, y, EDIT_W, 22,
        hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_EDIT_PATH)), hInst, nullptr);
    SendMessageW(hEditPath, WM_SETFONT, reinterpret_cast<WPARAM>(hFont), TRUE);

    CreateWindowExW(0, L"BUTTON", L"浏览...",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP,
        MARGIN + LABEL_W + 6 + EDIT_W + 8, y, 60, 22,
        hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_BROWSE)), hInst, nullptr);

    y += ROW_H + 10;

    // --- 第 2 行：预览偏移 ---
    CreateWindowExW(0, L"STATIC", L"预览偏移 (秒):",
        WS_CHILD | WS_VISIBLE, MARGIN, y + 2, LABEL_W, ROW_H,
        hwnd, nullptr, hInst, nullptr);

    HWND hEditOffset = CreateWindowExW(0, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL | WS_TABSTOP,
        MARGIN + LABEL_W + 6, y, 100, 22,
        hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_EDIT_OFFSET)), hInst, nullptr);
    SendMessageW(hEditOffset, WM_SETFONT, reinterpret_cast<WPARAM>(hFont), TRUE);

    y += ROW_H + 10;

    // --- 第 3 行：BPM 覆写 ---
    CreateWindowExW(0, L"STATIC", L"BPM 覆写 (0=自动):",
        WS_CHILD | WS_VISIBLE, MARGIN, y + 2, LABEL_W + 30, ROW_H,
        hwnd, nullptr, hInst, nullptr);

    HWND hEditBpm = CreateWindowExW(0, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL | WS_TABSTOP,
        MARGIN + LABEL_W + 30 + 6, y, 80, 22,
        hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_EDIT_BPM)), hInst, nullptr);
    SendMessageW(hEditBpm, WM_SETFONT, reinterpret_cast<WPARAM>(hFont), TRUE);

    y = DIALOG_H - BTN_H - MARGIN - 8;

    CreateWindowExW(0, L"BUTTON", L"确定",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP,
        DIALOG_W - BTN_W * 2 - MARGIN - 12, y, BTN_W, BTN_H,
        hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDOK)), hInst, nullptr);

    CreateWindowExW(0, L"BUTTON", L"取消",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP,
        DIALOG_W - BTN_W - MARGIN, y, BTN_W, BTN_H,
        hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDCANCEL)), hInst, nullptr);

    AUTOZYC_LOG_DEBUG("SettingsDialog::onCreate done, %d controls created", 8);
}

// ---------------------------------------------------------------------------
// loadFromConfig — 从配置对象填充控件值
// ---------------------------------------------------------------------------
void SettingsDialog::loadFromConfig(HWND hwnd)
{
    if (!s_config) return;

    AUTOZYC_LOG_DEBUG("SettingsDialog::loadFromConfig path='%s' offset=%.3f bpm=%.1f",
        s_config->audio_project_path.c_str(),
        s_config->preview_offset_sec,
        s_config->bpm_override);

    SetDlgItemTextW(hwnd, IDC_EDIT_PATH,
        std::wstring(s_config->audio_project_path.begin(),
                     s_config->audio_project_path.end()).c_str());

    wchar_t buf[64];
    swprintf(buf, 64, L"%.6f", s_config->preview_offset_sec);
    SetDlgItemTextW(hwnd, IDC_EDIT_OFFSET, buf);

    swprintf(buf, 64, L"%.6f", s_config->bpm_override);
    SetDlgItemTextW(hwnd, IDC_EDIT_BPM, buf);
}

// ---------------------------------------------------------------------------
// saveToConfig — 从控件读取值保存到配置对象
// ---------------------------------------------------------------------------
void SettingsDialog::saveToConfig(HWND hwnd)
{
    if (!s_config) return;

    wchar_t buf[512];

    if (GetDlgItemTextW(hwnd, IDC_EDIT_PATH, buf, 512) > 0) {
        int len = WideCharToMultiByte(CP_UTF8, 0, buf, -1, nullptr, 0, nullptr, nullptr);
        std::string path(len - 1, '\0');
        WideCharToMultiByte(CP_UTF8, 0, buf, -1, &path[0], len, nullptr, nullptr);
        s_config->audio_project_path = path;
        s_config->last_rpp_path = path;
        AUTOZYC_LOG_INFO("SettingsDialog: path saved = '%s'", path.c_str());
    }

    if (GetDlgItemTextW(hwnd, IDC_EDIT_OFFSET, buf, 64) > 0) {
        s_config->preview_offset_sec = _wtof(buf);
        AUTOZYC_LOG_DEBUG("SettingsDialog: offset saved = %.3f", s_config->preview_offset_sec);
    }

    if (GetDlgItemTextW(hwnd, IDC_EDIT_BPM, buf, 64) > 0) {
        s_config->bpm_override = _wtof(buf);
        AUTOZYC_LOG_DEBUG("SettingsDialog: bpm saved = %.1f", s_config->bpm_override);
    }
}

// ---------------------------------------------------------------------------
// onBrowseFile — 打开文件选择对话框
// ---------------------------------------------------------------------------
void SettingsDialog::onBrowseFile(HWND hwnd)
{
    wchar_t filePath[MAX_PATH] = { 0 };

    OPENFILENAMEW ofn = {};
    ofn.lStructSize = sizeof(OPENFILENAMEW);
    ofn.hwndOwner = hwnd;
    ofn.lpstrFilter = L"RPP/MIDI 文件(*.rpp;*.mid;*.midi)\0*.rpp;*.mid;*.midi\0所有文件(*.*)\0*.*\0";
    ofn.lpstrFile = filePath;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_HIDEREADONLY | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    ofn.lpstrTitle = L"选择音频工程文件";

    AUTOZYC_LOG_INFO("SettingsDialog: opening file dialog...");
    if (GetOpenFileNameW(&ofn)) {
        AUTOZYC_LOG_INFO("SettingsDialog: file selected = '%ls'", filePath);
        SetDlgItemTextW(hwnd, IDC_EDIT_PATH, filePath);
    } else {
        DWORD err = CommDlgExtendedError();
        AUTOZYC_LOG_WARN("SettingsDialog: GetOpenFileNameW cancelled or failed, err=%lu", err);
    }
    AUTOZYC_LOG_DEBUG("SettingsDialog: file dialog closed");
}
