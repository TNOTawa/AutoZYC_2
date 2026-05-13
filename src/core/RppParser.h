// AutoZYC — RPP 文件解析器（统一版，含 SOURCE MIDI 支持）
#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include "src/core/EventTypes.h"

class RppParser
{
public:
    // 解析文件路径
    bool parse(const std::string& filepath);

    // 解析 RPP 内容字符串（用于测试）
    bool parseContent(const std::string& content);

    // 获取解析结果
    const std::vector<UnifiedEvent>& getEvents() const { return m_events; }
    const std::unordered_map<int, TrackMeta>& getTrackMeta() const { return m_trackMeta; }
    int getTrackCount() const { return m_trackCount; }

private:
    // 解析 RPP 文本内容
    void parseRpp(const std::string& content);

    // 解析内嵌 SOURCE MIDI（RPP 文本-hex 格式）
    void parseSourceMidi(const std::string& midiContent, int trackNum, double baseOffset);

    // 计算轨道元数据
    void computeTrackMeta();

    // 辅助函数
    static std::string trim(const std::string& s);
    static bool startsWith(const std::string& s, const char* prefix);
    static std::string readFileContent(const std::string& filepath);

    std::vector<UnifiedEvent> m_events;
    std::unordered_map<int, TrackMeta> m_trackMeta;
    int m_trackCount = 0;
};