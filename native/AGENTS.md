# native/ — C++ 原生模块

## OVERVIEW
C++17 native plugins for Aviutl2. Two plugin types: `.mod2` (script module, called via `obj.module()`) + `.aux2` (common plugin, independent DLL with menus). Built with g++ MinGW-W64 x64.

## STRUCTURE
```
native/
├── AutoZYCParse/              # .mod2: parsing + expression evaluation
│   ├── AutoZYCParse.cpp       # 602 lines C++17
│   ├── Makefile               # g++ build config
│   └── build/
│       └── AutoZYCParse.mod2  # Compiled x64 DLL
└── AutoZYC/                   # .aux2: object generation + menus + config
    ├── AutoZYC.cpp            # 398 lines C++17
    ├── Makefile
    └── build/
        └── AutoZYC.aux2       # Compiled x64 DLL
```

## WHERE TO LOOK

| Task | File | Details |
|------|------|---------|
| RPP parsing (C++) | AutoZYCParse.cpp - `parse_rpp()` | ITEM/POSITION/LENGTH/PLAYRATE extraction, SOURCE MIDI detection |
| MIDI SMF parsing | AutoZYCParse.cpp - `parse_midi()` | MThd/MTrk, variable-length delta, running status, Set Tempo |
| RPP MIDI parsing | AutoZYCParse.cpp - `parse_rpp_midi()` | Text-hex `e offset flags note velocity` format |
| Expression evaluator | AutoZYCParse.cpp - `eval_expression()` | Recursive descent parser, 14 math functions |
| Object generation | AutoZYC.cpp - `generate_objects_callback()` | Inline RPP parser, `create_object()`, `set_object_item_value()` |
| Menu registration | AutoZYC.cpp - `RegisterPlugin()` | 4 menus: object/layer/config/import |
| Config persistence | AutoZYC.cpp - `PROJECT_FILE` | `get_param_string`/`set_param_string` key-value store |

## CONVENTIONS (this directory)

### Build
- **g++ 15.2.0 MinGW-W64 x64** (x86_64-ucrt-posix-seh)
- Flags: `-std=c++17 -O2 -m64 -shared -static`
- `.mod2`: No extra link dependencies
- `.aux2`: Link `-lcomdlg32` for file dialogs
- Output extension: `.mod2` or `.aux2` (not `.dll`)

### .mod2 API (module2.h)
- **Export**: `GetScriptModuleTable() → SCRIPT_MODULE_TABLE*`
- **Functions**: `void func(SCRIPT_MODULE_PARAM* param)`
- **Params**: `get_param_int/double/string/data(index)` → read Lua args
- **Results**: `push_result_int/double/string/array/table/boolean(val)` → return to Lua
- **Error**: `set_error(LPCSTR msg)` → Lua receives error string
- **Function list**: `{L"name", func_ptr, ..., nullptr}` NULL-terminated

### .aux2 API (plugin2.h)
- **Export**: `RegisterPlugin(HOST_APP_TABLE* host)`
- **Menus**: `register_object_menu()`, `register_config_menu()`, `register_import_menu()`, `register_layer_menu()`
- **Edit ops**: `EDIT_SECTION::create_object()`, `set_object_item_value()`, `move_object()`, `delete_object()`
- **Config**: `PROJECT_FILE::get_param_string(key)`, `set_param_string(key, value)`
- **Handlers**: `register_project_load_handler()`, `register_project_save_handler()`

### SDK Headers (read-only)
```
aviutl2_sdk/module2.h   — SCRIPT_MODULE_PARAM, SCRIPT_MODULE_TABLE
aviutl2_sdk/plugin2.h   — HOST_APP_TABLE, EDIT_SECTION, PROJECT_FILE
aviutl2_sdk/filter2.h   — PIXEL_RGBA
aviutl2_sdk/config2.h   — CONFIG_HANDLE
```

## ANTI-PATTERNS (this directory)
- **Calling .mod2 from .aux2**: Different plugin types, don't cross-call. Each self-contains its parsing.
- **Dynamic linking**: Use `-static` for all g++ builds. Aviutl2 may not have runtime DLLs.
- **Raw pointers across plugin boundaries**: Use `push_result_data()` only within same plugin lifecycle.
