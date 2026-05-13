// AutoZYC — Simplified expression engine (variable substitution only)
#pragma once
#include <string>

class ExpressionEngine {
public:
    // 主入口: 执行变量替换
    // 将表达式中的 pitch/velocity/time/item_idx/track 替换为对应数值
    // 空表达式返回 0.0
    double evaluate(const std::string& expr,
                    double pitch = 0.0,
                    double velocity = 0.0,
                    double time = 0.0,
                    double item_idx = 0.0,
                    double track = 0.0);

    // 变量替换（公开用于测试）
    // 基于单词边界替换（前后非字母数字/下划线）
    // 使用 snprintf("%.15g") 进行双精度格式化
    std::string replaceVariable(const std::string& input,
                                const std::string& var_name,
                                const std::string& value_str);

    // 预处理: 替换所有变量
    std::string preprocess(const std::string& expr,
                           double pitch, double velocity,
                           double time, double item_idx,
                           double track);
};