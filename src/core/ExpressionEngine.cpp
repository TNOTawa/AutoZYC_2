// AutoZYC — Simplified expression engine (variable substitution only)
#include "ExpressionEngine.h"
#include <cstdio>
#include <cstdlib>
#include <cctype>

// 基于单词边界替换（与 .mod2 的 replace_var() 一致）
// 前后非字母数字/下划线才能匹配
std::string ExpressionEngine::replaceVariable(const std::string& s,
                                              const std::string& from,
                                              const std::string& to)
{
    if (from.empty()) return s;
    std::string result = s;
    size_t pos = 0;
    while ((pos = result.find(from, pos)) != std::string::npos) {
        bool leftOk = (pos == 0) ||
            (!std::isalnum((unsigned char)result[pos - 1]) && result[pos - 1] != '_');
        bool rightOk = (pos + from.size() >= result.size()) ||
            (!std::isalnum((unsigned char)result[pos + from.size()]) && result[pos + from.size()] != '_');
        if (leftOk && rightOk) {
            result.replace(pos, from.size(), to);
            pos += to.size();
        } else {
            pos += from.size();
        }
    }
    return result;
}

// 预处理: 依次替换所有变量（使用 snprintf("%.15g") 格式化）
std::string ExpressionEngine::preprocess(const std::string& expr,
                                         double pitch, double velocity,
                                         double time, double item_idx,
                                         double track)
{
    std::string s = expr;
    char buf[64];

    snprintf(buf, sizeof(buf), "%.15g", pitch);
    s = replaceVariable(s, "pitch", buf);

    snprintf(buf, sizeof(buf), "%.15g", velocity);
    s = replaceVariable(s, "velocity", buf);

    snprintf(buf, sizeof(buf), "%.15g", time);
    s = replaceVariable(s, "time", buf);

    snprintf(buf, sizeof(buf), "%.15g", item_idx);
    s = replaceVariable(s, "item_idx", buf);

    snprintf(buf, sizeof(buf), "%.15g", track);
    s = replaceVariable(s, "track", buf);

    return s;
}

// 主入口: 空表达式返回 0.0，否则 preprocess + atof
double ExpressionEngine::evaluate(const std::string& expr,
                                  double pitch, double velocity,
                                  double time, double item_idx,
                                  double track)
{
    if (expr.empty()) return 0.0;
    std::string processed = preprocess(expr, pitch, velocity, time, item_idx, track);
    return std::atof(processed.c_str());
}