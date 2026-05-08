# PROJECT KNOWLEDGE BASE

**Generated:** 2026-05-08
**Commit:** de39551
**Branch:** master

## OVERVIEW
AutoZYC is a LuaJIT script plugin for Aviutl2 that automates otomad (音MAD) video effects. It parses REAPER .rpp project files to synchronize video objects with audio tracks — enabling left/right flipping, easing animations, and pitch-driven positioning.

## STRUCTURE
```
.
├── @AutoZuoYouChou_parser.obj2    # RPP解析 + 全局状态管理 (199行)
├── @AutoZuoYouChou_easing.tra2    # 3个缓动脚本(线性运动/视频控制/音高驱动) (217行)
├── @AutoZuoYouChou_effects.anm2   # 2个效果(镜像反转/物件间Alpha) (108行)
├── @AutoZuoYouChou_easing.obj2    # 缓动设置存储 (19行)
├── aviutl2_sdk/                   # Aviutl2 SDK (C++ samples, docs, headers)
│   └── docs/                      # lua.txt=完整API, aviutl2_plugin_sdk.txt=SDK概览
├── docs/
│   └── refactoring_report.md      # 重构建议报告
├── references/                    # 参考项目(不修改)
│   ├── aviutl2_script_Basic_S/    # ⭐ LuaJIT工程化黄金标准
│   ├── RPPtoEXO-ver2.0/           # ⭐ 统一数据模型参考
│   ├── om_midi/                   # MIDI可视化(TypeScript)
│   ├── Keyframe-Trigger/          # MIDI触发(C++/AE插件)
│   └── OtomadHelper/              # 音MAD辅助(C#)
├── Readme!.txt
└── How2use.txt
```

## WHERE TO LOOK

| 任务 | 位置 | 说明 |
|------|------|------|
| RPP解析/全局状态 | `@AutoZuoYouChou_parser.obj2` | parse_rpp() + init() 更新全局表 |
| 缓动函数/动画 | `@AutoZuoYouChou_easing.tra2` | @线性运动 @视频播放控制 @音高驱动位置 |
| 视觉特效 | `@AutoZuoYouChou_effects.anm2` | @常用效果(镜像+Alpha) |
| 缓动配置存储 | `@AutoZuoYouChou_easing.obj2` | @缓动设置(编号→轨道/时长) |
| Aviutl2 Lua API | `aviutl2_sdk/docs/lua.txt` | obj.* 全部函数 + 参数声明语法 |
| Aviutl2 SDK概览 | `aviutl2_sdk/docs/aviutl2_plugin_sdk.txt` | 插件类型(.aui2/.auo2/.auf2/.mod2/.aux2) |
| 重构计划 | `docs/refactoring_report.md` | 详细架构建议 + 实施路线图 |
| LuaJIT工程化参考 | `references/aviutl2_script_Basic_S/` | 模式: @multi-script, PI传递, 着色器, 缓存缓冲 |
| 统一数据模型参考 | `references/RPPtoEXO-ver2.0/` | Parser→objDict→Generator 架构 |

## GLOBAL STATE (现状)

项目使用以下全局变量通信(重构后应封装到 data_model 模块):

| 变量 | 来源 | 消费者 |
|------|------|--------|
| `AutoZYC_RPPData` | parser.obj2:parse_rpp() | parser.obj2:init() |
| `AutoZYC_Item_count` | parser.obj2:init() | easing.tra2, effects.anm2 |
| `AutoZYC_EaseSettings` | easing.obj2 | easing.tra2 |
| `AutoZYC_PitchRange` | parser.obj2:init() | easing.tra2:@音高驱动位置 |
| `AutoZYC_GlobalAudioTime` | parser.obj2:init() | easing.tra2 |

## CONVENTIONS

- **文件类型**: `.obj2`=物件脚本, `.anm2`=动画/滤镜脚本, `.tra2`=缓动脚本
- **多脚本注册**: 文件名以`@`开头，内部用`@名字`分割各脚本
- **UTF-8编码**: 所有.obj2/.anm2/.tra2文件必须UTF-8 (旧格式.anm/.obj/.tra使用SJIS)
- **参数声明**: --track@变量:名称,min,max,default --check@... --value@... --select@...
- **仅可用库**: table, string, math (os, debug, ffi.C 被禁用)

## ANTI-PATTERNS (本项目)

- **全局变量裸露**: 所有模块通过全局表隐式通信，应封装
- **缓动函数重复**: easeIn/easeOut/easeInOut 在多个section中重复定义
- **无参数规范化**: 用户输入未进行 clamp/normalize
- **无版本声明**: 缺少 --require 声明最低Aviutl2版本

## BASIC_S PATTERNS TO ADOPT (重构核心参考)

- `local obj, math = obj, math;` — 每个@section开头局部化
- `--label:AutoZYC\分类` — 菜单层级组织
- `--group:组名,false` / `--group` — 参数折叠分组
- `--value@PI:PI,{}` — 接收结构化数据
- `--[[pixelshader@name:` — 内嵌HLSL着色器
- `"cache:autozic/xxx"` — 跨效果临时缓冲
- `obj.effect("效果@AutoZYC", ...)` — 跨脚本引用
- `--require:version` — 版本依赖声明
- `--separator:名称` — 参数UI分隔线
- 参数规范化: 所有输入 clamp + round + normalize

## COMMANDS

```bash
# 此项目为Aviutl2脚本插件，无build步骤
# 部署: 复制到 C:\ProgramData\aviutl2\Script\AutoZYC\
# 或打包为 .au2pkg.zip 拖放到Aviutl2窗口安装
```

## NOTES

- Aviutl2使用LuaJIT 2.1，非标准Lua 5.1
- 脚本仅能使用 table, string, math 库
- 文件IO用 io.open (需确认Aviutl2是否开放此权限)
- 脚本间数据共享只能用全局变量或缓存缓冲区
- 项目当前仅支持REAPER .rpp格式，不支持MIDI
- 目录路径中不能包含中文字符(RPP文件路径限制)

## V2 ARCHITECTURE (重构目标)

### 核心创新
- **表达式映射引擎**: 解析数据作为变量(pitch/velocity/time/phase)代入用户自定义Lua表达式，驱动物件参数
- **RPP内MIDI物件**: RPP中的<SOURCE MIDI块自动检测解析，产出与.mid文件相同的数据格式
- **混合架构**: 脚本层(LuaJIT编排) + .mod2(C++性能路径) + .aux2(C++系统能力)

### 混合架构分层
```
脚本层 (LuaJIT)       .anm2/.obj2/.tra2   UI参数、效果编排、PI通信
模块层 (.mod2 C++)    obj.module()调用     解析加速、表达式求值、数据存储
插件层 (.aux2 C++)    独立DLL              物件批量生成、配置持久化、菜单注册
```

### .mod2 API (module2.h)
- get_param_int/double/string/data(index) — 类型化参数获取
- get_param_table_int/double/string(index,key) — 表参数获取
- push_result_int/double/string/boolean(data) — 返回值压栈
- push_result_table_* / push_result_array_* — 结构化返回
- set_error(message) — 错误报告
- 在Lua中: local mod = obj.module("AutoZYCParse"); mod.parse_rpp(path)

### .aux2 关键API (aviutl2_plugin_sdk.txt)
- create_object() / create_object_from_alias() / create_object_from_media_file()
- register_object_menu() / register_config_menu()
- get_project_file() → PROJECT_FILE 配置持久化
- enum_effect_name() / enum_effect_item()
- call_read_section() / call_edit_section()
