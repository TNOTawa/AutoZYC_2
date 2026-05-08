# PROJECT KNOWLEDGE BASE

**Branch:** master

## OVERVIEW
AutoZYC is a hybrid LuaJIT + C++ plugin for Aviutl2 that automates otomad video effects. Parses REAPER .rpp and Standard MIDI Files. Architecture: Lua scripts (.obj2/.tra2/.anm2) + C++ .mod2 (parsing + expression) + C++ .aux2 (object gen + menus + config).

## STRUCTURE
```
.
├── script/
│   ├── @AutoZYC.obj2              # data import, counter, generator, ease settings
│   ├── @AutoZYC.tra2              # 10 easing functions + 6 easing sections
│   ├── @AutoZYC.anm2              # 4-mode flip + 2 pixel shaders
│   └── test/@AutoZYC_test.obj2    # E2E tests
├── native/
│   ├── AutoZYCParse/              # .mod2: RPP/MIDI parser + expression evaluator
│   └── AutoZYC/                   # .aux2: object batch gen + menus + config
├── dist/                          # .au2pkg.zip distribution package
├── aviutl2_sdk/                   # SDK (read-only)
└── references/                    # Reference projects (read-only)
```

## CORE ARCHITECTURE

**Hybrid: LuaJIT + .mod2 (REQUIRED) + .aux2 (optional)**

```
Script layer (.obj2/.tra2/.anm2): UI params, effect orchestration, PI communication
Module layer (.mod2 C++):         obj.module("AutoZYCParse") — RPP/MIDI parsing, expression eval
Plugin layer (.aux2 C++):         Independent DLL — object generation, menus, config persistence
```

## WHERE TO LOOK

| Task | File | Section |
|------|------|---------|
| Data import | script/@AutoZYC.obj2 | @数据导入 |
| Object counter | script/@AutoZYC.obj2 | @物件计数器 |
| Object generator | script/@AutoZYC.obj2 | @物件生成器 |
| Ease settings | script/@AutoZYC.obj2 | @缓动设置 |
| Easing library | script/@AutoZYC.tra2 | preamble (Ease table) |
| Linear/video/pitch/velocity/BPM/expression | script/@AutoZYC.tra2 | @线性/@视频/@音高/@力度/@BPM/@表达式 |
| Mirror flip + alpha | script/@AutoZYC.anm2 | @常用效果 |
| Pixel shaders | script/@AutoZYC.anm2 | @色相映射, @亮度映射 |
| .mod2 source | native/AutoZYCParse/AutoZYCParse.cpp | C++17 |
| .aux2 source | native/AutoZYC/AutoZYC.cpp | C++17 |
| Aviutl2 Lua API | aviutl2_sdk/docs/lua.txt | obj.* functions |

## .mod2 API (REQUIRED DEPENDENCY)

```lua
local mod = obj.module("AutoZYCParse")
mod.parse_file(path)                                    -- parse .rpp or .mid (auto-detect)
mod.get_track_count()                                   -- -> int
mod.get_event_count(track)                              -- -> int
mod.get_event_data(track)                               -- -> {pos,len,pitch,vel,...} flat array
mod.get_track_meta(track)                               -- -> {min_pitch, max_pitch, item_count}
mod.eval_expression(expr, pitch, vel, time, idx, track) -- -> double
```

## GLOBAL STATE

| Global Variable | Source | Consumers |
|-----------------|--------|-----------|
| AutoZYC_State | obj2 (@数据导入) | All sections |
| AutoZYC_Item_count | obj2 (@物件计数器) | tra2, anm2 |
| AutoZYC_EaseSettings | obj2 (@缓动设置) | tra2 |
| AutoZYC_PitchRange | obj2 (@数据导入) | tra2 (@音高驱动位置) |
| AutoZYC_GlobalAudioTime | obj2 (@物件计数器) | tra2 |
| Ease | tra2 (preamble) | All easing sections |

## CRITICAL CONVENTIONS

1. **UTF-8 WITHOUT BOM** — ALL .obj2/.tra2/.anm2 files. BOM breaks Aviutl2 @section parser.
2. **Multi-script preamble**: COMMENTS ONLY. NO executable code before first @section. Each @section is self-contained.
3. **NO @ in comment lines** — Aviutl2 section parser scans ALL lines for @name, including Lua comments.
4. **Cross-section data**: Global variables ONLY. Locals in preamble do NOT transfer between sections.
5. **Available Lua libs**: table, string, math (os, debug, ffi.C disabled).
6. **Parameter syntax**: --track@var:name,min,max,default --check@... --value@... --select@... --file@...
7. **Each @section**: Start with `local obj, math = obj, math;` then your code.

## BUILD

```bash
# .mod2
cd native/AutoZYCParse
g++ -std=c++17 -O2 -m64 -shared -static -o build/AutoZYCParse.mod2 AutoZYCParse.cpp

# .aux2
cd native/AutoZYC
g++ -std=c++17 -O2 -m64 -shared -static -o build/AutoZYC.aux2 AutoZYC.cpp -lcomdlg32

# Package
cd dist
Compress-Archive -Path (Script/*, package.*) -DestinationPath AutoZYC-v2.0.0.au2pkg.zip
```

## NOTES
- Aviutl2: LuaJIT 2.1, beta 44 (build 2004400)
- io.open: verified available
- obj.getinfo("bpm"): returns tempo, beat, offset
- .mod2: REQUIRED. .aux2: optional but recommended
- RPP MIDI: text-hex format only (HASDATA + e offset flags note velocity)
- No multi-language files, no v1.1 backward compat required