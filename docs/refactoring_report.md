# AutoZYC v2.0 重构建议报告

> 分析日期: 2026-05-08 | 分析对象: AutoZYC v1.1 + 5个参考项目 + Aviutl2 SDK | 目标: 重写为Aviutl2插件，保留脚本执行，拓展功能，优化UX

---

## 一、当前项目现状

### 代码规模与分布

| 文件 | 行数 | 功能 |
|------|------|------|
| @AutoZuoYouChou_parser.obj2 | 199 | RPP解析 + 全局状态管理 |
| @AutoZuoYouChou_easing.tra2 | 217 | 3个缓动脚本(线性运动/视频播放控制/音高驱动) |
| @AutoZuoYouChou_effects.anm2 | 108 | 2个效果(镜像反转/物件间Alpha) |
| @AutoZuoYouChou_easing.obj2 | 19 | 缓动设置存储 |
| **总计** | **~540** | |

### 核心问题诊断

| 问题 | 严重度 | 详情 |
|------|--------|------|
| 全局变量污染 | 🔴严重 | AutoZYC_RPPData, AutoZYC_Item_count, AutoZYC_EaseSettings, AutoZYC_PitchRange, AutoZYC_GlobalAudioTime 全部裸露全局 |
| 零模块化 | 🔴严重 | 全部代码在4个文件中内联，无函数/模块复用 |
| RPP解析脆弱 | 🟡中等 | 逐行正则匹配，无法处理嵌套、错误格式 |
| 无MIDI支持 | 🟡中等 | 仅支持.rpp，缺失音MAD核心的MIDI工作流 |
| 硬编码缓动 | 🟡中等 | easeIn/easeOut/easeInOut在每个脚本中重复定义 |
| 无错误处理 | 🟡中等 | 仅靠 or {} 防御 |
| 工作流繁琐 | 🟡中等 | 用户需手动添加缓动设置物件并配置编号 |
| 无配置持久化 | 🟢低 | 每次需重新选RPP路径 |

---

## 二、参考项目分析摘要

### 2.1 aviutl2_script_Basic_S (sigma-axis, LuaJIT, ~2800行) ⭐⭐⭐⭐⭐

**这是Aviutl2 LuaJIT脚本开发的黄金标准。** 关键可移植模式：

| # | 模式 | 可移植性 | 说明 |
|---|------|----------|------|
| 1 | 多脚本单文件(@前缀) | ✅直接复用 | 一个.anm2文件包含多个效果 |
| 2 | 跨脚本引用(@脚本名) | ✅核心机制 | obj.effect("效果@Script", ...) |
| 3 | PI参数传递(--value@PI) | ✅推荐 | 模块间传递结构化配置 |
| 4 | 局部变量快照 | ✅必须 | local obj,math=obj,math; 提升LuaJIT性能 |
| 5 | 像素着色器内嵌(--[[pixelshader@) | ✅推荐 | 高性能视觉效果 |
| 6 | 缓存缓冲区(cache:xxx) | ✅标准做法 | copybuffer实现多步骤复合效果 |
| 7 | 参数规范化 | ✅必须 | 所有输入都clamp+round+normalize |
| 8 | 菜单层级(--label:XX\YY) | ✅组织菜单 | 反斜杠分隔层级 |
| 9 | 参数分组(--group/--separator) | ✅改善UI | 折叠分组提升可用性 |
| 10 | 版本要求(--require:version) | ✅声明依赖 | 最低Aviutl2版本号 |
| 11 | 包分发(.au2pkg.zip) | ✅部署方式 | package.ini+Script+Language |

### 2.2 RPPtoEXO-ver2.0 (Python, ~3000行) ⭐⭐⭐⭐

与AutoZYC理念最接近的项目。关键模式：

| # | 模式 | 可移植性 |
|---|------|----------|
| 1 | 统一数据模型(objDict) | ✅核心架构 - RPP和MIDI解析后统一格式 |
| 2 | 解析器/生成器分离 | ✅推荐 - Parser→Data→Generator |
| 3 | 对象翻转模式系统(4种模式+同音检测) | ✅当前简化版可升级 |
| 4 | i18n(.po/.mo→.aul2) | ✅Aviutl2语言文件格式 |
| 5 | 错误收集+统一报告 | ✅防御性编程模式 |

### 2.3 om_midi + Keyframe-Trigger ⭐⭐⭐

MIDI处理架构参考点：
- MIDI Note Number → 音高偏移(半音)
- Velocity → 强度映射
- 每个MIDI Track → 独立物件轨道
- Lyric事件 → 文本物件
- "关键帧触发→克隆图层" → 可映射为"音符开始→生成物件"

---

## 三、重构架构建议

### 3.1 推荐文件结构

```
AutoZYC.au2pkg.zip
├── Script/AutoZYC/
│   ├── @AutoZYC.anm2          # 滤镜效果
│   ├── @AutoZYC.obj2          # 自定义物件(数据源+生成)
│   ├── @AutoZYC.tra2          # 缓动/移动脚本
│   └── lib/
│       ├── data_model.lua     # 统一数据模型+全局状态管理
│       ├── rpp_parser.lua     # RPP解析器
│       ├── midi_parser.lua    # MIDI解析器
│       ├── easing.lua         # 缓动函数库
│       └── utils.lua          # 通用工具
├── Language/ (.aul2文件)
├── assets/package.txt
└── package.ini
```

### 3.2 统一数据模型 (data_model.lua)

借鉴RPPtoEXO的objDict设计，RPP和MIDI解析后输出统一格式：

```lua
-- 全局事件数据(只通过函数访问)
local events = {}  -- { [track] = { EventData, ... } }
local track_meta = {}  -- { [track] = TrackMeta }
local play_state = {}  -- 当前播放状态

-- EventData = {
--   track, position_sec, length_sec, pitch_shift,
--   velocity, file_path?, event_type
-- }

-- 公开API
function DataModel.reset() end
function DataModel.add_event(track, event) end
function DataModel.get_events(track) end
function DataModel.update_play_state(global_time) end
function DataModel.get_play_state(track) end
```

### 3.3 模块职责

```
解析层                        效果层(消费统一数据)
┌──────────────┐    ┌──────────────────────────────────┐
│rpp_parser.lua│    │@AutoZYC.tra2                     │
│midi_parser   │    │  @线性运动 (时间驱动+缓动选择)     │
└──┬───────────┘    │  @视频播放控制 (时间驱动)          │
   │ 写入           │  @音高驱动位置 (pitch→pos)        │
   ▼                │  @速度驱动缩放 (playrate→scale)   │
┌──────────────┐    │  @力度驱动透明度 (velocity→alpha)  │
│ data_model   │◄───│  @BPM同步缓动 (BPM grid驱动)      │
│  (SSOT)      │    ├──────────────────────────────────┤
└──────────────┘    │@AutoZYC.anm2 (滤镜)               │
   │ 读取           │  @常用效果 (镜像+Alpha控制)        │
   ▼                │  @MIDI力度映射 (velocity→color)   │
┌──────────────┐    │  @色相偏移 (pitch→hue via shader) │
│@AutoZYC.obj2 │    ├──────────────────────────────────┤
│ @物件自动生成 │    │@AutoZYC.obj2                     │
│ (批量创建物件)│    │ @物件计数器 (轨道状态追踪)         │
└──────────────┘    └──────────────────────────────────┘
```

### 3.4 缓动函数统一库 (easing.lua)

消除重复定义，从Basic_S的.tra2中提取完整缓动系统：

```lua
-- 标准缓动(已有): linear, easeInQuad, easeOutQuad, easeInOutQuad
-- 借鉴Basic_S新增:
--   立方/四次/N次缓动(easeInCubic/Quart/Quint)
--   正弦缓动(easeInSine/Out/InOut)
--   指数缓动(easeInExpo/Out/InOut)
--   圆形缓动(easeInCirc/Out/InOut)
--   弹性缓动(easeOutElastic)
--   回退缓动(easeOutBack)
--   弹跳缓动(easeOutBounce)
-- 自动选择: Easing.select(acc, dec) → 根据加速/减速自动选取
```

### 3.5 像素着色器应用

借鉴Basic_S的着色器模式，将计算密集型效果用HLSL实现：

```hlsl
--[[pixelshader@pitch_to_color:
float4 psmain(float4 pos : SV_Position) : SV_Target {
    // 音高→色相映射，力度→亮度映射 (Vocaloid风格)
    ...
}
]]
```

可对着色器化的效果: 音高→色相, 力度→亮度, 频谱可视化, 批量对象变换

---

## 四、新增功能规划

### 优先级 🔴 最高

| 功能 | 描述 | 参考 |
|------|------|------|
| MIDI解析 | SMF格式，Note On/Off, Velocity, Lyric事件 | om_midi, RPPtoEXO |
| 音符→物件映射 | 每个Note On→一个物件 | RPPtoEXO |
| 物件自动生成 | 一键根据数据源批量生成物件 | 原创(需确认API能力) |
| 力度→透明度/缩放 | Velocity映射到视觉参数 | 原创 |

### 优先级 🟡 中

| 功能 | 描述 |
|------|------|
| 速度驱动缩放 | playrate映射到缩放 |
| BPM同步缓动 | 使用obj.getinfo("bpm")对齐工程网格 |
| 增强翻转系统 | 4种模式(无/LR/UD/旋转) + 同音检测 |
| 时间控制缓动 | --timecontrol图形化缓动曲线 |
| 对数/逆数补间 | 指数/倒数运动曲线 |

---

## 五、代码质量规范

```lua
-- ✅ 每个@section开头局部化
local obj, math, tonumber = obj, math, tonumber;

-- ✅ 参数规范化
x = math.min(math.max(math.floor(0.5 + x), min), max);

-- ✅ 提前返回
if trivial_case then return end

-- ✅ 声明依赖
--require:2004300

-- ✅ 菜单组织
--label:AutoZYC\缓动

-- ✅ PI类型文档
--[==[ PI = { track: number?, easing: string? } ]==]

-- ❌ 避免裸全局变量 — 始终通过DataModel函数访问
```

---

## 六、实施路线图

### Phase 1: 基础设施 (基础)
- 建立文件结构和lib/模块
- 实现data_model + easing + utils模块
- 重写rpp_parser(保持v1.x兼容)

### Phase 2: MIDI支持 (核心扩展)
- midi_parser.lua SMF解析
- MIDI物件 + 力度驱动效果

### Phase 3: 自动化 (UX核心)
- 物件自动生成(批量创建)
- 增强的翻转模式系统

### Phase 4: 视觉特效
- 像素着色器效果
- 扩展的缓动类型(BPM同步,弹跳,弹性)

### Phase 5: 交付
- i18n(.aul2语言文件)
- .au2pkg.zip打包
- 文档

---

## 七、关键风险

| 风险 | 缓解 |
|------|------|
| Aviutl2脚本环境限制 | 确认io.open可用 + require可用；备选: obj.module()开发.mod2模块 |
| 跨脚本数据共享 | 全局表封装在DataModel中，通过函数访问 |
| 物件自动生成API | 确认脚本权限；可能需要aux2插件辅助 |
| MIDI解析复杂度 | SMF相对简单，但需处理变长编码和running status |

---

## 八、总结

AutoZYC v1.x是正确的概念验证，但存在全局变量污染、零模块化、功能单一等架构问题。

| 借鉴来源 | 核心收获 |
|----------|----------|
| **Basic_S** | LuaJIT工程化标准、着色器集成、参数系统、包分发 |
| **RPPtoEXO** | 统一数据模型、解析/生成分离、完整翻转系统 |
| **om_midi** | MIDI→视觉映射概念 |
| **Keyframe-Trigger** | 触发→克隆机制 |

重构的AutoZYC v2.0将实现: **RPP+MIDI双格式、统一数据模型、物件自动生成、增强缓动系统、着色器特效、国际化、标准包分发**。

## 八、补充：新增原创功能设计

### 8.1 RPP内MIDI物件解析（RPP解析增强）

REAPER的 `.rpp` 文件中可以包含 MIDI 物件（`<SOURCE MIDI` 块），当前解析器完全忽略此类型。

**RPP MIDI 物件格式示例**：
```
<ITEM
  POSITION 3.5
  LENGTH 2.0
  <SOURCE MIDI
    HASDATA 1 960 QN     ; 960 ticks per quarter note
    e 0 90 3C 60          ; note on: offset 0, note 0x3C (C4), velocity 0x60
    e 480 80 3C 00        ; note off: offset 480 ticks
    e 480 90 40 50        ; next note on
    E 1920 b0 7b 00       ; all notes off
  >
>
```

**解析策略**：
```
RPP文件 -> 检测SOURCE类型 -+- SOURCE WAVE/VIDEO/MP3 -> 作为音频/视频物件
                            |                           (提取 POSITION, LENGTH, PLAYRATE)
                            |
                            +- SOURCE MIDI -> 作为MIDI物件
                                              (提取 POSITION, LENGTH,
                                               HASDATA 解析tick转换,
                                               逐行解析 e/E 事件: 音高/力度/时间)
```

**统一数据模型扩展**：MIDI物件产出与 `.mid` 文件解析完全相同的 EventData 格式：
```lua
EventData = {
  track, position_sec, length_sec,
  pitch_shift,        -- 来自NOTE的pitch值(0-127半音)
  velocity,            -- 来自NOTE的velocity值(0-127)
  event_type = "note"  -- 标记为音符事件
}
```

这样RPP中的MIDI物件和独立MIDI文件经过解析后，下游效果层全部无差别消费。

---

### 8.2 表达式映射引擎（核心创新功能）

**目标**：将解析数据作为变量，代入用户自定义的表达式，驱动物件参数。实现"数据驱动的参数化控制"。

#### 设计思路

```
 数据变量字典                    用户表达式
+-----------------+          +------------------+
| pitch     = 64   |          |                   |
| velocity  = 100  |  代入    | "pitch / 127 *    |
| time      = 3.2  | ------> |  screen_h +       |
| length    = 0.5  |          |  velocity * 0.2"  |
| track     = 2    |          |                   |
| phase     = 0.7  |          +--------+---------+
| playrate  = 1.5  |                   |
| ...              |          +--------v---------+
+-----------------+          |   计算结果: 420.5  |
                             | -> 映射到 obj.oy   |
                             +------------------+
```

#### 表达式语法设计

```lua
-- 支持的变量(每帧由DataModel自动注入)
--   pitch      : 当前音符的音高偏移(半音)
--   velocity   : 当前音符的力度(0~127)
--   time       : 当前帧在物件内的相对时间(秒)
--   length     : 当前音符长度(秒)
--   phase      : time / length (0~1归一化进度)
--   track      : 轨道编号
--   item_idx   : 当前轨道内物件序号(从1开始)
--   playrate   : 播放速率
--   GlobalTime : 全局音频时间(秒)

-- 示例表达式:
--   "pitch / 12 * 100"                     -- 音高驱动Y位置
--   "velocity / 127"                       -- 力度驱动透明度
--   "math.sin(phase * math.pi * 2) * 50"   -- 相位正弦振荡
--   "playrate > 1.0 and 2.0 or 0.5"       -- 速率→缩放切换
--   "(item_idx % 2 == 0) and 1 or 0"       -- 奇偶→翻转
```

#### 安全实现方案

```lua
-- expression_engine.lua (白名单沙箱求值)
local ExpressionEngine = {}

function ExpressionEngine.compile(expr_str, var_table)
  -- 构建受限环境: 仅暴露数据变量 + math函数
  local env = {}
  for k, v in pairs(var_table) do env[k] = v end
  for k, v in pairs(math) do
    if type(v) == "function" then env[k] = v end
  end
  -- load()在"t"模式下仅允许表达式(无io/os/debug)
  local fn, err = load("return " .. expr_str, "=expr", "t", env)
  if not fn then return nil, err end
  return function() return fn() end
end

function ExpressionEngine.evaluate(expr_str, var_table)
  local fn = ExpressionEngine.compile(expr_str, var_table)
  if fn then
    local ok, result = pcall(fn)
    if ok then return tonumber(result) or result end
  end
  return nil
end
```

#### 作为其他脚本的前置数据源

表达式映射作为 `.tra2` 缓动脚本，通过 `PI` 机制对外暴露：

```lua
-- @AutoZYC.tra2 中的 "表达式映射"
@表达式映射
--twopoint
--param:轨道编号,1
--value@expression:表达式,"pitch / 12 * 50"
-- 每帧: DataModel获取数据 -> 注入变量 -> 求值 -> return结果

-- 其他脚本通过PI机制消费:
-- obj.effect("表达式映射@AutoZYC", "PI", "expression=pitch/127*screen_h")
```

---

### 8.3 混合架构：脚本 + .mod2 + .aux2

.md2和.aux2可作为插件核心组件：

#### .mod2 模块用途（C++ DLL, obj.module()调用）

| 模块 | 用途 | 理由 |
|------|------|------|
| **AutoZYCCore.mod2** | 共享数据模型的内存管理 | 全局表在脚本间共享但无生命周期管理 |
| **AutoZYCParse.mod2** | RPP/MIDI解析加速 | 大文件C++解析比Lua快10-100x |
| **AutoZYCExpr.mod2** | 表达式编译与批量求值 | 安全沙箱 + 表达式预编译缓存 |

.mod2 开发模式（参考SDK ScriptModule.cpp）：
```cpp
void parse_rpp(SCRIPT_MODULE_PARAM* param) {
  auto path = param->get_param_string(0);  // 文件路径
  // C++ 解析...
  // 通过 push_result_table_* 返回结构化数据
}
// Lua: local result = obj.module("AutoZYCParse").parse_rpp(path)
```

#### .aux2 插件用途（通用插件，操作项目数据）

| 功能 | 对应API |
|------|---------|
| 物件批量生成 | create_object() / create_object_from_alias() |
| 轨道管理 | set_object_name() |
| 配置持久化 | PROJECT_FILE API |
| 菜单注册 | register_object_menu() / register_config_menu() |

#### 推荐分层架构

```
+------------------------------------------------------+
|                    脚本层 (LuaJIT)                     |
|  @AutoZYC.anm2  @AutoZYC.obj2  @AutoZYC.tra2         |
|  用户UI参数、效果编排、缓动计算、着色器、PI通信          |
+------------------------------------------------------+
|                   模块层 (.mod2 C++)                    |
|  解析引擎、表达式求值、数据存储(性能关键路径)              |
|  Lua中通过 obj.module("AutoZYCParse") 调用              |
+------------------------------------------------------+
|                   插件层 (.aux2 C++)                    |
|  物件批量生成、项目配置、菜单注册(系统能力)                |
+------------------------------------------------------+
```

---

## 九、总结（更新）

| 借鉴来源 | 核心收获 |
|----------|----------|
| **Basic_S** | LuaJIT工程化标准、着色器集成、PI通信、包分发 |
| **RPPtoEXO** | 统一数据模型、解析/生成分离、翻转系统、RPP MIDI物件解析 |
| **om_midi** | MIDI映射概念、歌词->文本 |
| **Keyframe-Trigger** | 触发机制、矩阵变换 |
| **SDK (module2.h)** | .mod2开发模板、SCRIPT_MODULE_PARAM完整接口 |

重构后AutoZYC v2.0的**9大核心能力**：

1. **RPP+MIDI双格式** + **RPP内MIDI物件检测解析**
2. **统一数据模型** — Parser->DataModel->Effects三明治
3. **表达式映射引擎** — Lua白名单沙箱求值
4. **物件批量自动生成** — .aux2插件
5. **混合架构** — 脚本(编排) + .mod2(性能) + .aux2(系统能力)
6. **增强缓动系统** — 统一库 + BPM同步 + 高级缓动
7. **着色器特效** — GPU加速音高/力度视觉映射
8. **国际化** — .aul2多语言
9. **标准包分发** — .au2pkg.zip
