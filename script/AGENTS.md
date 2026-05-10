# script/ — LuaJIT 脚本层

## OVERVIEW
LuaJIT 2.1 scripts for Aviutl2. Three @multi-script files + 1 test file. All UI params, effect orchestration, and PI communication live here.

## STRUCTURE
```
script/
├── @AutoZYC.obj2          # 核心: 数据导入 (@数据导入 + 2 section)
├── @AutoZYC.tra2          # 缓动: 10 Ease函数 + 6 easing section
├── @AutoZYC.anm2          # 效果: 4模式翻转 + 2个HLSL着色器
└── test/
    └── @AutoZYC_test.obj2 # E2E测试套件 (6 section)
```

## WHERE TO LOOK

| File | Section | Purpose |
|------|---------|---------|
| @AutoZYC.obj2 | @数据导入 | RPP/MIDI→State; Ease table; State methods (get_active_info, find_active_track, get_audio_time) |
| @AutoZYC.obj2 | @物件生成器 | Event-aligned object positioning |
| @AutoZYC.tra2 | @视频播放控制 | Elapsed-time video frame offset |
| @AutoZYC.tra2 | @音高驱动位置 | Pitch → position via pitch range |
| @AutoZYC.tra2 | @节奏时间控制 | Note-based time-controlled repeat |
| @AutoZYC.tra2 | @节奏时间控制往复 | Note-based round-trip time-controlled repeat |
| @AutoZYC.tra2 | @节奏时间控制镜像往复 | Mirrored round-trip time-controlled repeat |
| @AutoZYC.tra2 | @音高驱动位置时间控制 | Pitch-driven time-controlled effect |
| @AutoZYC.anm2 | @常用效果 | 4-mode flip + alpha hide on silence |
| @AutoZYC.anm2 | @色相映射 | HLSL: pitch_normalized → HSV hue rotation |
| @AutoZYC.anm2 | @亮度映射 | HLSL: velocity → RGB multiplier |

## GLOBAL STATE

| Global Variable | Source | Consumers |
|-----------------|--------|-----------|
| AutoZYC_State | obj2 (@数据导入) | All sections |
| AutoZYC_PitchRange | obj2 (@数据导入) | tra2 (@音高驱动位置) |
| Ease | obj2 (@数据导入) | All easing sections |

## CONVENTIONS (this directory)

- **UTF-8 WITHOUT BOM**: Critical. BOM breaks Aviutl2 `@section` parser.
- **@multi-script preamble**: Comments ONLY before first `@section`. Each section is self-contained with its own `local obj, math = obj, math;`.
- **NO `@` in comments**: Aviutl2 scans ALL lines for `@name` patterns, including Lua comments. Only use `@` on valid parameter directives (`--file@`, `--value@`, etc.) and pixel shader embeds (`--[[pixelshader@name:`).
- **Section structure**: `@name` → parameter declarations → `local obj, math = obj, math;` → code.
- **Cross-section data**: Global variables ONLY. Locals defined in one section are invisible to others.
- **State pattern**: Each section accesses global `AutoZYC_State` via `local State = AutoZYC_State or {}`. Guard with `if type(State.method) ~= "function" then return end`.
- **State methods**: `State.get_active_info(track, time)` returns {count, is_playing, position_sec, length_sec, pitch_shift, velocity} via .mod2 binary search. `State.find_active_track(counter_data, track_param)` resolves the active track. `State.get_audio_time()` returns current global audio playback time.
- **Parameter syntax**: `--track@var:name,min,max,default`, `--check@`, `--value@`, `--select@`, `--file@`, `--param:N`
- **Available libs**: `table`, `string`, `math` only. `os`, `debug`, `ffi.C` disabled.
- **`info` directive**: Use `--information:description` at top of each @section for metadata.

## ANTI-PATTERNS (this directory)
- **Preamble executable code**: Causes `State` nil at runtime. All code goes in @sections.
- **`@name` in comments**: Confuses Aviutl2 section parser, truncates preamble.
- **Bare global reads without nil guard**: `AutoZYC_Item_count[track].exists` crashes if track missing.
- **Hardcoded easing functions**: Use `Ease.select(acc, dec)` from preamble, not inline `easeIn/easeOut`.

## TESTING
- `script/test/@AutoZYC_test.obj2` — 6 test sections. Load in Aviutl2 to verify.
- Key E2E tests: `@e2e_mod2` (.mod2 load), `@e2e_pipeline` (full RPP parse).