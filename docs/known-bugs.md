# AutoZYC SettingsDialog Crash — Known Bug

## Symptoms
Opening "AutoZYC/工程设置" → clicking "Browse..." → selecting a file → application crashes (silent exit, no error dialog).

## Current Investigation

### What was tried (did NOT fix):
1. Added `OFN_NOCHANGEDIR` — prevents GetOpenFileNameW from changing cwd
2. Added `OFN_EXPLORER` — forces Explorer-style dialog
3. Removed `PostQuitMessage(0)` from WM_DESTROY — but WM_QUIT is correctly consumed by GetMessageW returning 0
4. Added `IsDialogMessageW()` to message loop — proper dialog keyboard handling
5. Increased window size — cosmetic fix only

### Root cause hypothesis:
GetOpenFileNameW interacts badly with Aviutl2's internal message pump. The file dialog's OWN modal loop runs inside our dialog's message loop, creating nested message pumping that corrupts Aviutl2's state.

### Likely fix (not yet attempted):
Replace GetOpenFileNameW with the modern IFileOpenDialog (COM-based shell dialog):

```cpp
#include <shobjidl_core.h>  // needs MinGW with proper headers

HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
IFileOpenDialog* pfd = nullptr;
hr = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pfd));
if (SUCCEEDED(hr)) {
    COMDLG_FILTERSPEC ft[] = {{L"Audio Project", L"*.rpp;*.mid;*.midi"},{L"All Files", L"*.*"}};
    pfd->SetFileTypes(2, ft);
    if (SUCCEEDED(pfd->Show(hwnd))) {
        IShellItem* item = nullptr;
        if (SUCCEEDED(pfd->GetResult(&item))) {
            PWSTR path = nullptr;
            item->GetDisplayName(SIGDN_FILESYSPATH, &path);
            // use path
            CoTaskMemFree(path); item->Release();
        }
    }
    pfd->Release();
}
CoUninitialize();
```

Requires MinGW-w64 with uuid_shobjidl.h or linking -luuid -lole32.

### Alternative fix:
Use the Aviutl2 SDK's own file dialog mechanism if available, or bypass file dialog entirely by having user type/paste the path manually.

### Source file:
src/ui/SettingsDialog.cpp — function onBrowseFile()

### Status:
Deferred — crash does not affect other plugin functionality (import/generation/settings persistence work via manual path entry).
