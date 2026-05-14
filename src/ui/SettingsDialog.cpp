#include "src/ui/SettingsDialog.h"
#include "src/core/Logger.h"
#include <cstdio>
#include <cstdlib>
#include <string>

ProjectConfig* SettingsDialog::s_config = nullptr;
HWND SettingsDialog::s_hwndSelf = nullptr;
static const wchar_t* CLASS_NAME = L"AutoZYC_SettingsDlg";

bool SettingsDialog::show(HWND hwndParent, ProjectConfig& config)
{
    AUTOZYC_LOG_INFO("SettingsDialog::show parent=0x%p", hwndParent);
    s_config = &config;
    s_hwndSelf = nullptr;

    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = wndProc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = CLASS_NAME;
    RegisterClassExW(&wc);

    HWND hwnd = CreateWindowExW(WS_EX_DLGMODALFRAME, CLASS_NAME,
        L"AutoZYC Project Settings",
        WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT, DIALOG_W, DIALOG_H,
        hwndParent, nullptr, GetModuleHandleW(nullptr), nullptr);
    if (!hwnd) { s_config = nullptr; return false; }

    if (hwndParent) {
        RECT p, s; GetWindowRect(hwndParent, &p); GetWindowRect(hwnd, &s);
        int w = s.right - s.left, h2 = s.bottom - s.top;
        SetWindowPos(hwnd, nullptr, p.left + (p.right - p.left - w) / 2,
            p.top + (p.bottom - p.top - h2) / 2, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
    }
    ShowWindow(hwnd, SW_SHOW);

    MSG msg;
    while (IsWindow(hwnd) && GetMessageW(&msg, nullptr, 0, 0)) {
        if (!IsDialogMessageW(hwnd, &msg)) {
            TranslateMessage(&msg); DispatchMessageW(&msg);
        }
    }

    // s_hwndSelf == nullptr means dialog was destroyed normally (OK or Cancel)
    // s_config == nullptr means Cancel was pressed (cleared in wndProc)
    bool ok = (s_config != nullptr);
    AUTOZYC_LOG_INFO("SettingsDialog closed ok=%d", ok);
    s_config = nullptr;
    return ok;
}

LRESULT CALLBACK SettingsDialog::wndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_CREATE:
        s_hwndSelf = hwnd; onCreate(hwnd); loadFromConfig(hwnd); return 0;
    case WM_COMMAND: {
        WORD c = LOWORD(wp);
        if (c == IDC_BROWSE) { onBrowseFile(hwnd); return 0; }
        if (c == IDOK) { saveToConfig(hwnd); DestroyWindow(hwnd); return 0; }
        if (c == IDCANCEL) { s_config = nullptr; DestroyWindow(hwnd); return 0; }
        break;
    }
    case WM_CLOSE:
        s_config = nullptr; DestroyWindow(hwnd); return 0;
    case WM_DESTROY:
        s_hwndSelf = nullptr; PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

void SettingsDialog::onCreate(HWND hwnd)
{
    HINSTANCE hI = GetModuleHandleW(nullptr);
    auto ctrl = [&](const wchar_t* cls, const wchar_t* txt, DWORD style, int x, int y, int w, int h, int id) {
        HWND c = CreateWindowExW(0, cls, txt, style | WS_CHILD | WS_VISIBLE, x, y, w, h,
            hwnd, (HMENU)(INT_PTR)id, hI, nullptr);
        SendMessageW(c, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), TRUE);
        return c;
    };
    int y = MARGIN;
    ctrl(L"STATIC", L"Audio Project File:", 0, MARGIN, y + 3, LABEL_W, ROW_H, 0);
    ctrl(L"EDIT", L"", WS_BORDER | ES_AUTOHSCROLL | WS_TABSTOP, MARGIN + LABEL_W + 6, y, 260, 22, IDC_EDIT_PATH);
    ctrl(L"BUTTON", L"Browse...", BS_PUSHBUTTON | WS_TABSTOP, MARGIN + LABEL_W + 272, y, 60, 22, IDC_BROWSE);
    y += ROW_H + 12;
    ctrl(L"STATIC", L"Preview Offset (sec):", 0, MARGIN, y + 3, LABEL_W, ROW_H, 0);
    ctrl(L"EDIT", L"", WS_BORDER | ES_AUTOHSCROLL | WS_TABSTOP, MARGIN + LABEL_W + 6, y, 100, 22, IDC_EDIT_OFFSET);
    y += ROW_H + 12;
    ctrl(L"STATIC", L"BPM Override (0=auto):", 0, MARGIN, y + 3, LABEL_W + 20, ROW_H, 0);
    ctrl(L"EDIT", L"", WS_BORDER | ES_AUTOHSCROLL | WS_TABSTOP, MARGIN + LABEL_W + 26, y, 80, 22, IDC_EDIT_BPM);
    y += ROW_H + 36;
    ctrl(L"BUTTON", L"OK", BS_PUSHBUTTON | WS_TABSTOP, DIALOG_W - 180, y, 80, 28, IDOK);
    ctrl(L"BUTTON", L"Cancel", BS_PUSHBUTTON | WS_TABSTOP, DIALOG_W - 90, y, 80, 28, IDCANCEL);
}

void SettingsDialog::loadFromConfig(HWND hwnd)
{
    if (!s_config) return;
    auto w = [](const std::string& s) { return std::wstring(s.begin(), s.end()); };
    SetDlgItemTextW(hwnd, IDC_EDIT_PATH, w(s_config->audio_project_path).c_str());
    wchar_t b[64];
    swprintf(b, 64, L"%.3f", s_config->preview_offset_sec);
    SetDlgItemTextW(hwnd, IDC_EDIT_OFFSET, b);
    swprintf(b, 64, L"%.1f", s_config->bpm_override);
    SetDlgItemTextW(hwnd, IDC_EDIT_BPM, b);
}

void SettingsDialog::saveToConfig(HWND hwnd)
{
    if (!s_config) return;
    wchar_t buf[512];
    if (GetDlgItemTextW(hwnd, IDC_EDIT_PATH, buf, 512) > 0) {
        int l = WideCharToMultiByte(CP_UTF8, 0, buf, -1, nullptr, 0, nullptr, nullptr);
        std::string p(l - 1, 0);
        WideCharToMultiByte(CP_UTF8, 0, buf, -1, &p[0], l, nullptr, nullptr);
        s_config->audio_project_path = p; s_config->last_rpp_path = p;
        AUTOZYC_LOG_INFO("Path: %s", p.c_str());
    }
    if (GetDlgItemTextW(hwnd, IDC_EDIT_OFFSET, buf, 64) > 0) s_config->preview_offset_sec = _wtof(buf);
    if (GetDlgItemTextW(hwnd, IDC_EDIT_BPM, buf, 64) > 0) s_config->bpm_override = _wtof(buf);
}

void SettingsDialog::onBrowseFile(HWND hwnd)
{
    wchar_t path[MAX_PATH] = {};
    OPENFILENAMEW ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwnd;
    ofn.lpstrFilter = L"RPP/MIDI Files\0*.rpp;*.mid;*.midi\0All Files\0*.*\0";
    ofn.lpstrFile = path;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_HIDEREADONLY | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR | OFN_EXPLORER;
    ofn.lpstrTitle = L"Select Audio Project File";
    AUTOZYC_LOG_INFO("Opening file dialog...");
    if (GetOpenFileNameW(&ofn)) {
        SetDlgItemTextW(hwnd, IDC_EDIT_PATH, path);
        AUTOZYC_LOG_INFO("File: %ls", path);
    }
}
