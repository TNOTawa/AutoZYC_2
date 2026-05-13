// AutoZYC — RPP 文件解析器实现（统一版，含 SOURCE MIDI 支持）
// 基于 .mod2 的 parse_rpp() 和 parse_rpp_midi() 逻辑

#include "src/core/RppParser.h"
#include <fstream>
#include <sstream>
#include <cstdlib>
#include <cstring>
#include <map>
#include <algorithm>

// ============================================================
// 辅助函数
// ============================================================

std::string RppParser::trim(const std::string& s)
{
    size_t start = 0;
    while (start < s.size() && (unsigned char)s[start] <= 0x20) start++;
    size_t end = s.size();
    while (end > start && (unsigned char)s[end - 1] <= 0x20) end--;
    return s.substr(start, end - start);
}

bool RppParser::startsWith(const std::string& s, const char* prefix)
{
    size_t len = strlen(prefix);
    return s.size() >= len && s.compare(0, len, prefix) == 0;
}

std::string RppParser::readFileContent(const std::string& filepath)
{
    std::ifstream file(filepath, std::ios::in | std::ios::binary);
    if (!file.is_open()) return "";

    // 跳过 UTF-8 BOM (EF BB BF)
    std::string content;
    file.seekg(0, std::ios::end);
    content.resize(static_cast<size_t>(file.tellg()));
    file.seekg(0, std::ios::beg);
    file.read(&content[0], content.size());
    file.close();

    // 去掉 BOM
    if (content.size() >= 3 &&
        (unsigned char)content[0] == 0xEF &&
        (unsigned char)content[1] == 0xBB &&
        (unsigned char)content[2] == 0xBF)
    {
        content.erase(0, 3);
    }

    return content;
}

// ============================================================
// parse() / parseContent()
// ============================================================

bool RppParser::parse(const std::string& filepath)
{
    m_events.clear();
    m_trackMeta.clear();
    m_trackCount = 0;

    std::string content = readFileContent(filepath);
    if (content.empty()) return false;

    parseRpp(content);
    computeTrackMeta();
    return true;
}

bool RppParser::parseContent(const std::string& content)
{
    m_events.clear();
    m_trackMeta.clear();
    m_trackCount = 0;

    if (content.empty())
    {
        // 空文件返回空事件列表，不崩溃
        return true;
    }

    parseRpp(content);
    computeTrackMeta();
    return true;
}

// ============================================================
// parseRpp() — 核心 RPP 解析逻辑
// 基于 .mod2 的 parse_rpp()，逐字符解析嵌套结构
// ============================================================

void RppParser::parseRpp(const std::string& content)
{
    m_trackCount = 0;
    int currentTrack = 0;
    size_t pos = 0;

    while (pos < content.size())
    {
        // 找行尾
        size_t lineEnd = content.find('\n', pos);
        if (lineEnd == std::string::npos) lineEnd = content.size();
        std::string line = content.substr(pos, lineEnd - pos);
        pos = lineEnd + 1;

        // 去掉 \r
        if (!line.empty() && line.back() == '\r') line.pop_back();
        line = trim(line);
        if (line.empty()) continue;

        // <TRACK → 轨道计数
        if (startsWith(line, "<TRACK"))
        {
            currentTrack++;
            if (currentTrack > m_trackCount) m_trackCount = currentTrack;
        }
        // <ITEM → 解析物件
        else if (startsWith(line, "<ITEM") && currentTrack >= 1)
        {
            UnifiedEvent evt;
            evt.track = currentTrack;
            evt.position_sec = 0.0;
            evt.length_sec = 0.0;
            evt.pitch_shift = 0.0;
            evt.velocity = 0.0;

            int depth = 1;
            while (pos < content.size())
            {
                size_t nextEnd = content.find('\n', pos);
                if (nextEnd == std::string::npos) nextEnd = content.size();
                std::string inner = content.substr(pos, nextEnd - pos);
                pos = nextEnd + 1;

                if (!inner.empty() && inner.back() == '\r') inner.pop_back();
                inner = trim(inner);

                // <SOURCE MIDI → 嵌套解析内嵌 MIDI
                if (startsWith(inner, "<SOURCE MIDI"))
                {
                    std::string midiContent;
                    int midiNest = 1;
                    while (pos < content.size())
                    {
                        size_t mend = content.find('\n', pos);
                        if (mend == std::string::npos) mend = content.size();
                        std::string mline = content.substr(pos, mend - pos);
                        pos = mend + 1;
                        if (!mline.empty() && mline.back() == '\r') mline.pop_back();
                        midiContent += mline + "\n";

                        std::string mt = trim(mline);
                        if (startsWith(mt, "<")) midiNest++;
                        if (mt == ">") { midiNest--; if (midiNest <= 0) break; }
                    }
                    parseSourceMidi(midiContent, currentTrack, evt.position_sec);
                    continue;
                }

                // 嵌套深度跟踪
                if (startsWith(inner, "<")) { depth++; }
                if (inner == ">") { depth--; if (depth <= 0) break; continue; }

                // POSITION 字段
                if (startsWith(inner, "POSITION"))
                {
                    const char* p = inner.c_str() + 8;
                    while (*p == ' ' || *p == '\t') p++;
                    if (*p) evt.position_sec = atof(p);
                }
                // LENGTH 字段
                else if (startsWith(inner, "LENGTH"))
                {
                    const char* p = inner.c_str() + 6;
                    while (*p == ' ' || *p == '\t') p++;
                    if (*p) evt.length_sec = atof(p);
                }
                // PLAYRATE 字段 — 第 3 个 token 为 pitch_shift
                else if (startsWith(inner, "PLAYRATE"))
                {
                    // "PLAYRATE 1.0 1 0.0"
                    // token 0: PLAYRATE, 1: 速率, 2: 步进, 3: 音高偏移
                    const char* p = inner.c_str() + 8;
                    while (*p == ' ' || *p == '\t') p++;
                    std::string remaining(p);

                    size_t sp1 = remaining.find(' ');
                    if (sp1 == std::string::npos) continue;

                    std::string token2 = trim(remaining.substr(sp1 + 1));
                    size_t sp2 = token2.find(' ');
                    if (sp2 == std::string::npos) continue;

                    std::string token3 = trim(token2.substr(sp2 + 1));
                    if (!token3.empty())
                        evt.pitch_shift = atof(token3.c_str());
                }
            }

            m_events.push_back(evt);
        }
    }
}

// ============================================================
// parseSourceMidi() — 解析内嵌 MIDI（RPP 文本-hex 格式）
// 基于 .mod2 的 parse_rpp_midi()
// ============================================================

// RPP MIDI 暂挂音符结构（在函数外定义以便 map 使用）
struct RppMidiPending
{
    unsigned int offset;
    double posSec;
    unsigned int vel;
};

void RppParser::parseSourceMidi(const std::string& midiContent, int trackNum, double baseOffset)
{
    int ticksPerQn = 960;
    double tempoUs = 500000.0;  // 默认 120 BPM

    std::istringstream ss(midiContent);
    std::string line;
    std::map<int, RppMidiPending> pending;

    while (std::getline(ss, line))
    {
        line = trim(line);

        // HASDATA ticks → ticks_per_qn
        if (line.find("HASDATA ") == 0)
        {
            ticksPerQn = atoi(line.c_str() + 8);
            if (ticksPerQn <= 0) ticksPerQn = 960;
        }
        // e offset flags note vel → 音符事件
        else if (!line.empty() && line[0] == 'e' && line[1] == ' ')
        {
            unsigned int off = 0, fl = 0, n = 0, v = 0;
            sscanf(line.c_str(), "e %x %x %x %x", &off, &fl, &n, &v);

            double tickDur = tempoUs / 1000000.0 / ticksPerQn;
            double pos = baseOffset + off * tickDur;

            // Note On: 0x90 + vel > 0
            if (fl == 0x90 && v > 0)
            {
                pending[(int)n] = {off, pos, v};
            }
            // Note Off: 0x80 或 0x90 + vel=0
            else if (fl == 0x80 || (fl == 0x90 && v == 0))
            {
                auto it = pending.find((int)n);
                if (it != pending.end())
                {
                    double len = (off - it->second.offset) * tickDur;

                    UnifiedEvent evt;
                    evt.position_sec = it->second.posSec;
                    evt.length_sec = len;
                    evt.pitch_shift = (double)n - 69.0;  // A4=440Hz 为 0
                    evt.velocity = (double)it->second.vel;
                    evt.track = trackNum;

                    m_events.push_back(evt);
                    pending.erase(it);
                }
            }
        }
    }

    // 剩余未结束的音符使用默认 0.5 秒长度
    double tickDur = tempoUs / 1000000.0 / ticksPerQn;
    for (auto& p : pending)
    {
        UnifiedEvent evt;
        evt.position_sec = p.second.posSec;
        evt.length_sec = 0.5;
        evt.pitch_shift = (double)p.first - 69.0;
        evt.velocity = (double)p.second.vel;
        evt.track = trackNum;

        m_events.push_back(evt);
    }
}

// ============================================================
// computeTrackMeta() — 计算轨道元数据
// 基于 .mod2 的 compute_track_meta()
// ============================================================

void RppParser::computeTrackMeta()
{
    m_trackMeta.clear();

    // 按轨道分组事件
    std::unordered_map<int, std::vector<UnifiedEvent>> trackEvents;
    for (auto& evt : m_events)
    {
        trackEvents[evt.track].push_back(evt);
    }

    for (auto& kv : trackEvents)
    {
        int track = kv.first;
        auto& events = kv.second;
        if (events.empty()) continue;

        TrackMeta meta;
        meta.item_count = (int)events.size();
        meta.min_pitch = events[0].pitch_shift;
        meta.max_pitch = events[0].pitch_shift;

        for (auto& e : events)
        {
            if (e.pitch_shift < meta.min_pitch) meta.min_pitch = e.pitch_shift;
            if (e.pitch_shift > meta.max_pitch) meta.max_pitch = e.pitch_shift;
        }

        // G5 guardrail: min==max → 扩展 ±1.0
        if (meta.min_pitch == meta.max_pitch)
        {
            meta.min_pitch -= 1.0;
            meta.max_pitch += 1.0;
        }

        m_trackMeta[track] = meta;
    }
}