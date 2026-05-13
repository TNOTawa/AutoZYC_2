// AutoZYC — 物件生成器核心逻辑
// 将统一事件列表转换为 Aviutl2 时间线上的物件，
// 使用贪心图层分配避免时间重叠冲突。
#pragma once

#include <windows.h>
#include "src/core/EventTypes.h"
#include "aviutl2_sdk/plugin2.h"
#include <vector>
#include <string>

class ObjectGenerator
{
public:
    // ============================================================
    // Flip state — computed by computeFlipState() from item index and flip mode
    // ============================================================
    struct FlipState
    {
        bool horizontal = false;   // horizontal flip (left-right)
        bool vertical = false;     // vertical flip (up-down)
    };

    // ============================================================
    // 生成结果统计
    // ============================================================
    struct GenerationResult
    {
        int created = 0;    // 成功创建的物件数
        int skipped = 0;    // 跳过的物件数（范围外/创建失败）
    };

    // ============================================================
    // 主入口：将事件列表生成到时间线上
    // ============================================================
    // edit:   EDIT_SECTION 指针（必须在 call_edit_section 回调内调用）
    // events: 统一事件列表
    // config: 生成配置（别名、图层、范围筛选、冻结状态等）
    // fps:    帧率 = edit->info->rate / edit->info->scale
    // ============================================================
    GenerationResult generateObjects(
        EDIT_SECTION* edit,
        const std::vector<UnifiedEvent>& events,
        const GenerationConfig& config,
        double fps
    );

    // ============================================================
    // Compute flip state from item index and flip mode
    // Mode 0: no flip
    // Mode 1: odd index -> horizontal flip
    // Mode 2: even index -> vertical flip
    // Mode 3: clockwise 4-step cycle [0, H, H+V, V]
    // Mode 4: counter-clockwise 4-step cycle [0, V, H+V, H]
    // ============================================================
    static FlipState computeFlipState(int itemIndex, int flipMode);

private:
    // ============================================================
    // 贪心图层分配：找到第一个不与已有物件重叠的图层
    // ============================================================
    // layerEnds:  各图层当前结束帧号（引用，会被修改）
    // frame:      目标起始帧号
    // length:     物件帧长度
    // startLayer: 搜索起始图层（0-based）
    // 返回: 分配到的图层编号（0-based）
    // ============================================================
    int allocateLayer(
        std::vector<int>& layerEnds,
        int frame,
        int length,
        int startLayer
    );

    // ============================================================
    // 构建物件名称（包含轨道和音高信息，可选累计变换烘焙）
    // ============================================================
    // evt:       事件数据
    // config:    生成配置（用于读取 freeze_state 和 step_* 值）
    // itemIndex: 当前物件在生成序列中的序号（从 0 开始，用于累计变换）
    // ============================================================
    std::wstring buildObjectName(
        const UnifiedEvent& evt,
        const GenerationConfig& config,
        int itemIndex
    );
};