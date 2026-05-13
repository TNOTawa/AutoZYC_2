// AutoZYC — Core event data structures
#pragma once

#include <cstdint>
#include <vector>
#include <string>

struct MidiNote
{
    double  positionSec;   // position in seconds
    double  lengthSec;     // note length in seconds
    int     pitch;         // MIDI pitch (0-127)
    int     velocity;      // velocity (0-127)
};

struct TrackInfo
{
    int             index;
    std::string     name;
    int             minPitch;
    int             maxPitch;
    int             eventCount;
    std::vector<MidiNote> notes;
};

struct ProjectData
{
    double                  bpm;
    std::vector<TrackInfo>  tracks;
};

// ============================================================
// 统一事件类型 — 同时适配 RPP 物件和 MIDI 音符
// ============================================================
struct UnifiedEvent
{
    double  position_sec;   // 起始位置（秒）
    double  length_sec;     // 持续时间（秒）
    double  pitch_shift;    // 音高偏移（半音），MIDI 音符编号 - 69（A4=0）
    double  velocity;       // 力度 0.0-1.0（RPP 物件为 0.0）
    int     track;          // 轨道编号（从 1 开始）
};

// ============================================================
// 轨道元数据
// ============================================================
struct TrackMeta
{
    double  min_pitch;      // 最小音高偏移
    double  max_pitch;      // 最大音高偏移
    int     item_count;     // 事件数量
};

// ============================================================
// 物件生成配置 — 传递给 ObjectGenerator 的参数集合
// ============================================================
struct GenerationConfig
{
    double      range_start_sec = 0.0;        // 时间范围筛选-起始（秒）
    double      range_end_sec = 99999.0;      // 时间范围筛选-结束（秒）
    int         layer_strategy = 0;           // 图层分配策略：0=贪心自动, 1=指定起始图层
    int         start_layer = 1;              // 起始图层编号（1-based，UI 习惯）
    int         interval_frames = 0;          // 每物件间隔帧数（0=无间隔）
    int         max_visible_count = 0;        // 最大可见物件数（0=无限制），超出后跳过剩余事件
    double      max_lifetime_sec = 0.0;       // 物件最大生存时间（秒，0=无限制），裁剪物件长度
    bool        freeze_state = false;         // 冻结状态：true=累积变换烘焙到物件名称/数据
    int         flip_mode = 0;                // 翻转模式：0=无,1=左右交替,2=上下交替,3=顺时针四步,4=逆时针四步
    bool        bpm_align = false;            // BPM对齐：true=物件对齐到节拍网格（TODO: 后端未实现）
    std::string alias = "AutoZYC.object";     // 物件别名（.object 文件名的 name 字段）

    // 累计变换参数
    double      step_scale = 100.0;           // 每物件累计缩放步进（百分比）
    double      step_rotation = 0.0;          // 每物件累计旋转步进（角度）
    double      step_offset_x = 0.0;          // 每物件累计 X 偏移步进（像素）
    double      step_offset_y = 0.0;          // 每物件累计 Y 偏移步进（像素）
};
