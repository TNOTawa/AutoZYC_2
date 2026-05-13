// AutoZYC — Cumulative transform system
// 累计变换系统：计算逐物件的累计缩放、旋转、偏移
#pragma once

#include "src/core/EventTypes.h"

// 变换值结果
struct TransformValues {
    double scale = 100.0;   // 缩放百分比 (100 = 100%)
    double rotation = 0.0;  // 旋转角度（度）
    double offsetX = 0.0;   // X 偏移（像素）
    double offsetY = 0.0;   // Y 偏移（像素）
};

class CumulativeTransform
{
public:
    // 计算第 itemIndex 个物件的累计变换值（从 0 开始）
    // 公式：
    //   scale   = pow(config.step_scale / 100.0, itemIndex) * 100.0
    //   rotation = config.step_rotation * itemIndex
    //   offsetX = config.step_offset_x * itemIndex
    //   offsetY = config.step_offset_y * itemIndex
    static TransformValues compute(int itemIndex, const GenerationConfig& config);
};
