// AutoZYC — 物件批量生成器（完整实现）
// 参考: aviutl2_sdk/plugin2.h EDIT_SECTION API
//       references/Keyframe-Trigger/KTrigger.cpp 生成模式
// 冻结状态: freeze_state=true 时累计变换烘焙到物件名称中

#include "src/generate/ObjectGenerator.h"
#include "src/generate/CumulativeTransform.h"
#include <algorithm>
#include <cmath>
#include <sstream>
#include <iomanip>

// ============================================================
// generateObjects() — 将 UnifiedEvent 列表转换为 Aviutl2 物件
// ============================================================
ObjectGenerator::GenerationResult ObjectGenerator::generateObjects(
    EDIT_SECTION* edit,
    const std::vector<UnifiedEvent>& events,
    const GenerationConfig& config,
    double fps)
{
    GenerationResult result;

    // 空事件列表 → 直接返回 {0, 0}
    if (events.empty())
        return result;

    // 安全校验：没有 EDIT_SECTION 或无效帧率
    if (!edit || fps <= 0.0)
        return result;

    // 图层结束帧追踪（贪心分配用）
    std::vector<int> layerEnds;

    // 起始图层（转为 0-based）
    int startLayer = config.start_layer - 1;
    if (startLayer < 0) startLayer = 0;

    // 物件索引（用于累计变换计算）
    int itemIndex = 0;

    for (const auto& evt : events)
    {
        // ——— 最大可见物件数限制 ———
        // max_visible_count > 0 时，跳过超出上限的事件
        if (config.max_visible_count > 0 && result.created >= config.max_visible_count)
        {
            result.skipped++;
            continue;
        }

        // ——— 时间范围筛选 ———
        if (evt.position_sec < config.range_start_sec ||
            evt.position_sec > config.range_end_sec)
        {
            result.skipped++;
            continue;
        }

        // ——— 秒 → 帧号转换 ———
        int frame = static_cast<int>(std::floor(evt.position_sec * fps));

        // ——— 最大生存时间限制（裁剪物件长度） ———
        double lengthSec = evt.length_sec;
        if (config.max_lifetime_sec > 0.0)
            lengthSec = std::min(lengthSec, config.max_lifetime_sec);

        int length = static_cast<int>(std::ceil(lengthSec * fps));

        // 物件至少 1 帧（Aviutl2 要求）
        if (length < 1) length = 1;

        // ——— 贪心图层分配 ———
        int layer = allocateLayer(layerEnds, frame, length, startLayer);

        // ——— 创建物件（调用 Aviutl2 API） ———
        OBJECT_HANDLE obj = edit->create_object_from_alias(
            config.alias.c_str(),
            layer,
            frame,
            length);

        if (obj)
        {
            // 设置物件名称（freeze_state=true 时烘焙累计变换到名称中）
            std::wstring name = buildObjectName(evt, config, itemIndex);
            edit->set_object_name(obj, name.c_str());
            result.created++;
            itemIndex++;
        }
        else
        {
            // 创建失败（别名无效 / 重叠冲突 / 其他错误）
            result.skipped++;
        }
    }

    return result;
}

// ============================================================
// computeFlipState() — compute flip state from item index and mode
// ============================================================
ObjectGenerator::FlipState ObjectGenerator::computeFlipState(
    int itemIndex, int flipMode)
{
    FlipState state;
    switch (flipMode)
    {
    case 1:
        // Left-right alternate: odd index -> horizontal flip
        if (itemIndex % 2 == 1)
            state.horizontal = true;
        break;
    case 2:
        // Up-down alternate: even index -> vertical flip
        if (itemIndex % 2 == 0)
            state.vertical = true;
        break;
    case 3:
        // Clockwise 4-step cycle [0, H, H+V, V]
        switch (itemIndex % 4)
        {
        case 1: state.horizontal = true; break;
        case 2: state.horizontal = true; state.vertical = true; break;
        case 3: state.vertical = true; break;
        }
        break;
    case 4:
        // Counter-clockwise 4-step cycle [0, V, H+V, H]
        switch (itemIndex % 4)
        {
        case 1: state.vertical = true; break;
        case 2: state.horizontal = true; state.vertical = true; break;
        case 3: state.horizontal = true; break;
        }
        break;
    // case 0: no flip, defaults remain false
    }
    return state;
}

// ============================================================
// allocateLayer() — 贪心算法分配不重叠的图层
// ============================================================
int ObjectGenerator::allocateLayer(
    std::vector<int>& layerEnds,
    int frame,
    int length,
    int startLayer)
{
    int endFrame = frame + length;

    // 确保 layerEnds 至少有 startLayer 个条目
    while (static_cast<int>(layerEnds.size()) <= startLayer)
        layerEnds.push_back(0);

    // 从 startLayer 开始搜索第一个可用图层
    for (int i = startLayer; i < static_cast<int>(layerEnds.size()); ++i)
    {
        // frame >= layerEnds[i] 表示当前物件不与图层上已有物件重叠
        // （上一物件结束帧 = 新物件起始帧 时，不重叠，可以共享图层）
        if (frame >= layerEnds[i])
        {
            layerEnds[i] = endFrame;
            return i;
        }
    }

    // 所有现有图层都冲突，创建新图层
    int newLayer = static_cast<int>(layerEnds.size());
    layerEnds.push_back(endFrame);
    return newLayer;
}

// ============================================================
// buildObjectName() — 构建物件显示名称
// ============================================================
// freeze_state=false: "Trk{轨道号} P{midi音符}"
// freeze_state=true:  "Trk{轨道号} P{midi音符} [F:scale,rot,dx,dy]"
// ============================================================
std::wstring ObjectGenerator::buildObjectName(
    const UnifiedEvent& evt,
    const GenerationConfig& config,
    int itemIndex)
{
    // 基本格式: "Trk{轨道号} P{midi音符}" 例如 "Trk1 P60"
    // pitch_shift + 69.0 = MIDI 音符编号（A4=69=0半音偏移）
    int midiNote = static_cast<int>(std::round(evt.pitch_shift + 69.0));

    std::wostringstream wss;
    wss << L"Trk" << evt.track << L" P" << midiNote;

    // Encode flip state suffix: _H / _V / _HV
    FlipState flip = computeFlipState(itemIndex, config.flip_mode);
    if (flip.horizontal && flip.vertical)
        wss << L"_HV";
    else if (flip.horizontal)
        wss << L"_H";
    else if (flip.vertical)
        wss << L"_V";

    // 冻结状态：烘焙累计变换到物件名称
    if (config.freeze_state)
    {
        TransformValues tv = CumulativeTransform::compute(itemIndex, config);
        wss << L" [F:"
            << std::fixed << std::setprecision(1)
            << tv.offsetX << L","
            << tv.offsetY << L","
            << tv.rotation << L","
            << tv.scale << L"]";
    }

    return wss.str();
}

