// AutoZYC — RPP 解析器测试
// Included by main.cpp (catch.hpp already available)
#include "src/core/RppParser.h"
#include <cmath>

// ============================================================
// 基础解析测试
// ============================================================

TEST_CASE("RPP parser: empty content returns no events", "[rpp_parser]")
{
    RppParser parser;
    REQUIRE(parser.parseContent("") == true);
    REQUIRE(parser.getEvents().empty());
    REQUIRE(parser.getTrackMeta().empty());
    REQUIRE(parser.getTrackCount() == 0);
}

TEST_CASE("RPP parser: whitespace-only content", "[rpp_parser]")
{
    RppParser parser;
    REQUIRE(parser.parseContent("  \n\t\n  ") == true);
    REQUIRE(parser.getEvents().empty());
}

TEST_CASE("RPP parser: single track with no items", "[rpp_parser]")
{
    RppParser parser;
    const char* rpp = "<TRACK {1234-5678-90AB-CDEF}\n>\n";
    REQUIRE(parser.parseContent(rpp) == true);
    REQUIRE(parser.getTrackCount() == 1);
    REQUIRE(parser.getEvents().empty());
}

// ============================================================
// ITEM 解析测试
// ============================================================

TEST_CASE("RPP parser: simple item with POSITION and LENGTH", "[rpp_parser]")
{
    RppParser parser;
    const char* rpp =
        "<TRACK {AAAA-AAAA-AAAA-AAAA}\n"
        "<ITEM\n"
        "POSITION 1.5\n"
        "LENGTH 0.75\n"
        ">\n"
        ">\n";

    REQUIRE(parser.parseContent(rpp) == true);
    REQUIRE(parser.getTrackCount() == 1);
    REQUIRE(parser.getEvents().size() == 1);

    auto& evt = parser.getEvents()[0];
    REQUIRE(evt.track == 1);
    REQUIRE(evt.position_sec == 1.5);
    REQUIRE(evt.length_sec == 0.75);
    REQUIRE(evt.pitch_shift == 0.0);
    REQUIRE(evt.velocity == 0.0);
}

TEST_CASE("RPP parser: item with zero-length LENGTH", "[rpp_parser]")
{
    RppParser parser;
    const char* rpp =
        "<TRACK {BBBB-BBBB-BBBB-BBBB}\n"
        "<ITEM\n"
        "POSITION 2.0\n"
        "LENGTH 0.001\n"
        ">\n"
        ">\n";

    REQUIRE(parser.parseContent(rpp) == true);
    // 不跳过 length_sec <= 0.001 的事件（MUST NOT）
    REQUIRE(parser.getEvents().size() == 1);
    REQUIRE(parser.getEvents()[0].length_sec == 0.001);
}

// ============================================================
// PLAYRATE 解析测试
// ============================================================

TEST_CASE("RPP parser: PLAYRATE with pitch_shift in 3rd token", "[rpp_parser]")
{
    RppParser parser;
    const char* rpp =
        "<TRACK {CCCC-CCCC-CCCC-CCCC}\n"
        "<ITEM\n"
        "POSITION 0.0\n"
        "LENGTH 1.0\n"
        "PLAYRATE 1.0 1 2.5\n"
        ">\n"
        ">\n";

    REQUIRE(parser.parseContent(rpp) == true);
    REQUIRE(parser.getEvents().size() == 1);

    auto& evt = parser.getEvents()[0];
    // 第 3 个 token (0-indexed) = 2.5 为 pitch_shift
    REQUIRE(evt.pitch_shift == 2.5);
}

TEST_CASE("RPP parser: PLAYRATE with negative pitch_shift", "[rpp_parser]")
{
    RppParser parser;
    const char* rpp =
        "<TRACK {DDDD-DDDD-DDDD-DDDD}\n"
        "<ITEM\n"
        "POSITION 0.0\n"
        "LENGTH 1.0\n"
        "PLAYRATE 1.0 0 -3.0\n"
        ">\n"
        ">\n";

    REQUIRE(parser.parseContent(rpp) == true);
    REQUIRE(parser.getEvents().size() == 1);
    REQUIRE(parser.getEvents()[0].pitch_shift == -3.0);
}

TEST_CASE("RPP parser: PLAYRATE with only 2 tokens (no pitch)", "[rpp_parser]")
{
    RppParser parser;
    const char* rpp =
        "<TRACK {EEEE-EEEE-EEEE-EEEE}\n"
        "<ITEM\n"
        "POSITION 0.0\n"
        "LENGTH 1.0\n"
        "PLAYRATE 1.0 0\n"
        ">\n"
        ">\n";

    REQUIRE(parser.parseContent(rpp) == true);
    REQUIRE(parser.getEvents().size() == 1);
    // 第 3 个 token 不存在，pitch_shift 保持默认 0.0
    REQUIRE(parser.getEvents()[0].pitch_shift == 0.0);
}

TEST_CASE("RPP parser: PLAYRATE with zero pitch_shift", "[rpp_parser]")
{
    RppParser parser;
    const char* rpp =
        "<TRACK {FFFF-FFFF-FFFF-FFFF}\n"
        "<ITEM\n"
        "POSITION 5.0\n"
        "LENGTH 0.5\n"
        "PLAYRATE 1.0 1 0.0\n"
        ">\n"
        ">\n";

    REQUIRE(parser.parseContent(rpp) == true);
    REQUIRE(parser.getEvents().size() == 1);
    REQUIRE(parser.getEvents()[0].pitch_shift == 0.0);
}

// ============================================================
// SOURCE MIDI 解析测试
// ============================================================

TEST_CASE("RPP parser: SOURCE MIDI with note events", "[rpp_parser]")
{
    RppParser parser;
    // RPP text-hex MIDI: e offset flags note vel (所有字段均为十六进制)
    // Note On:  fl=0x90, vel>0
    // Note Off: fl=0x80
    // offset 0x240 = 576 ticks, length = 576 * 500000/1e6/960 = 0.3s
    const char* rpp =
        "<TRACK {1111-1111-1111-1111}\n"
        "<ITEM\n"
        "POSITION 2.0\n"
        "LENGTH 4.0\n"
        "<SOURCE MIDI\n"
        "HASDATA 960\n"
        "e 0 90 3c 64\n"
        "e 240 80 3c 00\n"
        ">\n"
        ">\n"
        ">\n";

    REQUIRE(parser.parseContent(rpp) == true);
    // 1 MIDI 音符 + 1 ITEM = 2 events
    // 注意：SOURCE MIDI 中的事件先于 ITEM 事件被推入
    REQUIRE(parser.getEvents().size() == 2);

    // MIDI note event (最先推入，note 0x3c=60, pitch_shift=60-69=-9)
    auto& midiEvt = parser.getEvents()[0];
    REQUIRE(midiEvt.track == 1);
    REQUIRE(midiEvt.pitch_shift == Approx(60.0 - 69.0));
    REQUIRE(midiEvt.velocity == Approx(100.0));

    // 位置: baseOffset(2.0) + offset(0) * tickDur ≈ 2.0
    REQUIRE(midiEvt.position_sec > 1.9);
    REQUIRE(midiEvt.position_sec < 2.1);

    // 长度: offset 0x240=576 ticks, length = 576 * 500000/1e6/960 = 0.3
    REQUIRE(midiEvt.length_sec == Approx(0.3));

    // ITEM event（在 SOURCE MIDI 解析完成后推入）
    auto& itemEvt = parser.getEvents()[1];
    REQUIRE(itemEvt.track == 1);
    REQUIRE(itemEvt.position_sec == 2.0);
    REQUIRE(itemEvt.length_sec == 4.0);
    REQUIRE(itemEvt.pitch_shift == 0.0);
    REQUIRE(itemEvt.velocity == 0.0);
}

TEST_CASE("RPP parser: SOURCE MIDI with pending notes get default length", "[rpp_parser]")
{
    RppParser parser;
    // 只有 Note On，没有 Note Off → 默认 0.5 秒长度
    const char* rpp =
        "<TRACK {2222-2222-2222-2222}\n"
        "<ITEM\n"
        "POSITION 0.0\n"
        "LENGTH 10.0\n"
        "<SOURCE MIDI\n"
        "HASDATA 960\n"
        "e 0 90 45 7f\n"
        ">\n"
        ">\n"
        ">\n";

    REQUIRE(parser.parseContent(rpp) == true);
    // 1 个未结束 MIDI 音符 + 1 个 ITEM = 2 events
    REQUIRE(parser.getEvents().size() == 2);

    // MIDI 暂挂音符（最先推入，note 0x45=69, pitch=0, 未结束→默认 0.5s）
    auto& midiEvt = parser.getEvents()[0];
    REQUIRE(midiEvt.length_sec == 0.5);
    REQUIRE(midiEvt.pitch_shift == Approx(69.0 - 69.0));  // note 69
    REQUIRE(midiEvt.velocity == Approx(127.0));

    // ITEM event（第二个推入）
    auto& itemEvt = parser.getEvents()[1];
    REQUIRE(itemEvt.track == 1);
    REQUIRE(itemEvt.position_sec == 0.0);
    REQUIRE(itemEvt.length_sec == 10.0);
}

// ============================================================
// 多轨道测试
// ============================================================

TEST_CASE("RPP parser: multiple tracks", "[rpp_parser]")
{
    RppParser parser;
    const char* rpp =
        "<TRACK {AAA}\n"
        "<ITEM\n"
        "POSITION 0.0\n"
        "LENGTH 1.0\n"
        ">\n"
        ">\n"
        "<TRACK {BBB}\n"
        "<ITEM\n"
        "POSITION 2.0\n"
        "LENGTH 3.0\n"
        ">\n"
        ">\n";

    REQUIRE(parser.parseContent(rpp) == true);
    REQUIRE(parser.getTrackCount() == 2);
    REQUIRE(parser.getEvents().size() == 2);

    REQUIRE(parser.getEvents()[0].track == 1);
    REQUIRE(parser.getEvents()[1].track == 2);
}

// ============================================================
// TrackMeta 计算测试
// ============================================================

TEST_CASE("RPP parser: TrackMeta with single pitch (G5 guardrail)", "[rpp_parser]")
{
    RppParser parser;
    const char* rpp =
        "<TRACK {XXX-XXX-XXX-XXX}\n"
        "<ITEM\n"
        "POSITION 0.0\n"
        "LENGTH 1.0\n"
        "PLAYRATE 1.0 1 0.0\n"
        ">\n"
        ">\n";

    REQUIRE(parser.parseContent(rpp) == true);

    auto& meta = parser.getTrackMeta();
    REQUIRE(meta.count(1) == 1);

    TrackMeta tm = meta.at(1);
    REQUIRE(tm.item_count == 1);
    // G5: min==max → 扩展 ±1.0
    REQUIRE(tm.min_pitch == -1.0);
    REQUIRE(tm.max_pitch == 1.0);
}

TEST_CASE("RPP parser: TrackMeta with multiple pitches", "[rpp_parser]")
{
    RppParser parser;
    const char* rpp =
        "<TRACK {YYY-YYY-YYY-YYY}\n"
        "<ITEM\n"
        "POSITION 0.0\n"
        "LENGTH 1.0\n"
        "PLAYRATE 1.0 1 -5.0\n"
        ">\n"
        "<ITEM\n"
        "POSITION 2.0\n"
        "LENGTH 1.0\n"
        "PLAYRATE 1.0 1 7.0\n"
        ">\n"
        ">\n";

    REQUIRE(parser.parseContent(rpp) == true);

    auto& meta = parser.getTrackMeta();
    REQUIRE(meta.count(1) == 1);

    TrackMeta tm = meta.at(1);
    REQUIRE(tm.item_count == 2);
    REQUIRE(tm.min_pitch == -5.0);
    REQUIRE(tm.max_pitch == 7.0);
}

// ============================================================
// BOM 处理测试
// ============================================================

TEST_CASE("RPP parser: content with UTF-8 BOM", "[rpp_parser]")
{
    RppParser parser;
    // UTF-8 BOM: EF BB BF
    std::string content;
    content += static_cast<char>(0xEF);
    content += static_cast<char>(0xBB);
    content += static_cast<char>(0xBF);
    content += "<TRACK {BOM-BOM-BOM-BOM}\n";
    content += "<ITEM\nPOSITION 1.0\nLENGTH 2.0\n>\n>\n";

    // parseContent receives raw content; BOM skipping happens in readFileContent/parse
    // For parseContent test, we test without BOM since parseContent doesn't strip BOM
    // (BOM stripping is handled by readFileContent when parsing from file)
    // This test verifies the parser handles content with BOM prefix
    bool result = parser.parseContent(content);

    // If parseContent doesn't strip BOM, the first line will be "ï»¿<TRACK..."
    // which starts with BOM chars and won't match "<TRACK"
    // The result should still be valid (no crash), just 0 tracks
    REQUIRE(result == true);
    // No crash is the main assertion
}

// ============================================================
// 嵌套 SOURCE 处理测试
// ============================================================

TEST_CASE("RPP parser: nested SOURCE (non-MIDI) is skipped", "[rpp_parser]")
{
    RppParser parser;
    // <SOURCE WAVE or other non-MIDI sources should be skipped
    const char* rpp =
        "<TRACK {WAV-WAV-WAV-WAV}\n"
        "<ITEM\n"
        "POSITION 0.0\n"
        "LENGTH 3.0\n"
        "<SOURCE WAVE\n"
        "FILE \"test.wav\"\n"
        ">\n"
        ">\n"
        ">\n";

    REQUIRE(parser.parseContent(rpp) == true);
    REQUIRE(parser.getEvents().size() == 1);
    // Only the ITEM event, no MIDI events from WAVE source
}

// ============================================================
// 边界情况测试
// ============================================================

TEST_CASE("RPP parser: item without POSITION gets default 0.0", "[rpp_parser]")
{
    RppParser parser;
    const char* rpp =
        "<TRACK {NOP-NOP-NOP-NOP}\n"
        "<ITEM\n"
        "LENGTH 2.0\n"
        ">\n"
        ">\n";

    REQUIRE(parser.parseContent(rpp) == true);
    REQUIRE(parser.getEvents().size() == 1);
    REQUIRE(parser.getEvents()[0].position_sec == 0.0);
    REQUIRE(parser.getEvents()[0].length_sec == 2.0);
}

TEST_CASE("RPP parser: item without LENGTH gets default 0.0", "[rpp_parser]")
{
    RppParser parser;
    const char* rpp =
        "<TRACK {NOL-NOL-NOL-NOL}\n"
        "<ITEM\n"
        "POSITION 5.0\n"
        ">\n"
        ">\n";

    REQUIRE(parser.parseContent(rpp) == true);
    REQUIRE(parser.getEvents().size() == 1);
    REQUIRE(parser.getEvents()[0].position_sec == 5.0);
    REQUIRE(parser.getEvents()[0].length_sec == 0.0);
}

TEST_CASE("RPP parser: items before first TRACK are ignored", "[rpp_parser]")
{
    RppParser parser;
    const char* rpp =
        "<ITEM\n"
        "POSITION 0.0\n"
        "LENGTH 1.0\n"
        ">\n"
        "<TRACK {AFT-AFT-AFT-AFT}\n"
        "<ITEM\n"
        "POSITION 2.0\n"
        "LENGTH 1.0\n"
        ">\n"
        ">\n";

    REQUIRE(parser.parseContent(rpp) == true);
    // Only the ITEM after TRACK should be counted
    REQUIRE(parser.getEvents().size() == 1);
    REQUIRE(parser.getEvents()[0].position_sec == 2.0);
}

// ============================================================
// 解析方法切换测试
// ============================================================

TEST_CASE("RPP parser: parseContent clears previous state", "[rpp_parser]")
{
    RppParser parser;

    // 第一次解析
    const char* rpp1 = "<TRACK {AAA}\n<ITEM\nPOSITION 1.0\nLENGTH 1.0\n>\n>\n";
    REQUIRE(parser.parseContent(rpp1) == true);
    REQUIRE(parser.getEvents().size() == 1);

    // 第二次解析（应清除旧状态）
    const char* rpp2 = "<TRACK {BBB}\n<ITEM\nPOSITION 2.0\nLENGTH 2.0\n>\n>\n";
    REQUIRE(parser.parseContent(rpp2) == true);
    REQUIRE(parser.getEvents().size() == 1);
    REQUIRE(parser.getEvents()[0].position_sec == 2.0);
}

TEST_CASE("RPP parser: re-parse with empty content", "[rpp_parser]")
{
    RppParser parser;

    const char* rpp = "<TRACK {AAA}\n<ITEM\nPOSITION 1.0\nLENGTH 1.0\n>\n>\n";
    REQUIRE(parser.parseContent(rpp) == true);
    REQUIRE(parser.getEvents().size() == 1);

    // 空内容解析 — 保持空状态
    REQUIRE(parser.parseContent("") == true);
    REQUIRE(parser.getEvents().empty());
    REQUIRE(parser.getTrackCount() == 0);
}