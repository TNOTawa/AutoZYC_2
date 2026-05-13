// AutoZYC — Cumulative transform implementation
// 累计变换系统：纯数学计算，不依赖 AviUtl2 SDK
#include "src/generate/CumulativeTransform.h"
#include <cmath>

TransformValues CumulativeTransform::compute(int itemIndex, const GenerationConfig& config)
{
    TransformValues result;

    // 累计缩放：step_scale^itemIndex
    // 例如 step_scale=110, itemIndex=3 → 1.10^3 = 1.331 → 133.1%
    result.scale = std::pow(config.step_scale / 100.0, itemIndex) * 100.0;

    // 累计旋转：step_rotation * itemIndex
    result.rotation = config.step_rotation * static_cast<double>(itemIndex);

    // 累计偏移：step_offset * itemIndex
    result.offsetX = config.step_offset_x * static_cast<double>(itemIndex);
    result.offsetY = config.step_offset_y * static_cast<double>(itemIndex);

    return result;
}
