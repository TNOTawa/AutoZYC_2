// AutoZYC — ObjectGenerator 单元测试
// 使用 Mock EDIT_SECTION 模拟 Aviutl2 运行时，无需实际插件环境
#include "src/core/EventTypes.h"
#include "src/generate/ObjectGenerator.h"
#include "src/generate/CumulativeTransform.h"
#include <vector>
#include <string>
#include <cmath>

// ============================================================
// Mock EDIT_SECTION — 记录所有 API 调用，无需 Aviutl2 运行时
// ============================================================

struct MockObjectRecord
{
    std::string alias;
    int layer;
    int frame;
    int length;
    std::wstring name;
    void* handle;       // 返回的伪造句柄
};

struct MockEditSection
{
    // 真实的 EDIT_SECTION 是纯函数指针表，这里直接模拟
    std::vector<MockObjectRecord> objects;
    int nextHandle = 1;
    bool simulateFail = false;   // 设为 true 模拟创建失败

    // 静态回调：create_object_from_alias
    static void* createObjectCb(LPCSTR alias, int layer, int frame, int length)
    {
        // 通过全局指针访问 mock 实例（单线程测试安全）
        auto* self = getInstance();
        if (!self || self->simulateFail) return nullptr;

        void* handle = (void*)(intptr_t)(self->nextHandle++);
        MockObjectRecord rec;
        rec.alias = alias ? alias : "";
        rec.layer = layer;
        rec.frame = frame;
        rec.length = length;
        rec.handle = handle;
        self->objects.push_back(rec);
        return handle;
    }

    // 静态回调：set_object_name
    static void setNameCb(void* object, LPCWSTR name)
    {
        auto* self = getInstance();
        if (!self || !object) return;

        // 找到最后一个匹配 handle 的记录
        for (auto& rec : self->objects)
        {
            if (rec.handle == object)
            {
                rec.name = name ? name : L"";
                return;
            }
        }
    }

    static MockEditSection* getInstance()
    {
        static MockEditSection inst;
        return &inst;
    }

    void reset()
    {
        objects.clear();
        nextHandle = 1;
        simulateFail = false;
    }
};

// 构建一个可以传给 generateObjects 的 EDIT_SECTION 指针
// 实际只需要 create_object_from_alias 和 set_object_name 两个函数指针
static EDIT_SECTION* makeMockEditSection()
{
    static EDIT_SECTION mock;
    mock.create_object_from_alias = MockEditSection::createObjectCb;
    mock.set_object_name      = MockEditSection::setNameCb;
    mock.info                 = nullptr;
    return &mock;
}

// ============================================================
// 辅助：构造事件列表
// ============================================================

static UnifiedEvent makeEvent(double pos, double len, double pitch, double vel, int track)
{
    UnifiedEvent e;
    e.position_sec = pos;
    e.length_sec = len;
    e.pitch_shift = pitch;
    e.velocity = vel;
    e.track = track;
    return e;
}

// ============================================================
// Section A: 空事件和无效输入
// ============================================================

TEST_CASE("ObjectGenerator: empty events → 0 created, 0 skipped", "[object_generator]")
{
    MockEditSection::getInstance()->reset();
    ObjectGenerator gen;
    GenerationConfig cfg;
    std::vector<UnifiedEvent> empty;

    auto result = gen.generateObjects(makeMockEditSection(), empty, cfg, 30.0);

    REQUIRE(result.created == 0);
    REQUIRE(result.skipped == 0);
    REQUIRE(MockEditSection::getInstance()->objects.empty());
}

TEST_CASE("ObjectGenerator: nullptr edit → 0 created", "[object_generator]")
{
    ObjectGenerator gen;
    GenerationConfig cfg;
    std::vector<UnifiedEvent> events = { makeEvent(0.0, 1.0, 0.0, 0.8, 1) };

    auto result = gen.generateObjects(nullptr, events, cfg, 30.0);

    REQUIRE(result.created == 0);
}

TEST_CASE("ObjectGenerator: fps <= 0 → 0 created", "[object_generator]")
{
    MockEditSection::getInstance()->reset();
    ObjectGenerator gen;
    GenerationConfig cfg;
    std::vector<UnifiedEvent> events = { makeEvent(0.0, 1.0, 0.0, 0.8, 1) };

    auto result = gen.generateObjects(makeMockEditSection(), events, cfg, 0.0);

    REQUIRE(result.created == 0);
}

// ============================================================
// Section B: 秒 → 帧号转换
// ============================================================

TEST_CASE("ObjectGenerator: position_sec * fps → frame (floor)", "[object_generator]")
{
    MockEditSection::getInstance()->reset();
    ObjectGenerator gen;
    GenerationConfig cfg;
    std::vector<UnifiedEvent> events = {
        makeEvent(0.0, 1.0, 0.0, 0.8, 1),   // frame 0,  length 30
        makeEvent(2.5, 0.5, 2.0, 0.6, 2),   // frame 75, length 15
    };

    auto result = gen.generateObjects(makeMockEditSection(), events, cfg, 30.0);

    REQUIRE(result.created == 2);
    auto& objs = MockEditSection::getInstance()->objects;
    REQUIRE(objs.size() == 2);

    // 0.0 * 30 = 0;  ceil(1.0 * 30) = 30
    REQUIRE(objs[0].frame == 0);
    REQUIRE(objs[0].length == 30);

    // floor(2.5 * 30) = 75;  ceil(0.5 * 30) = 15
    REQUIRE(objs[1].frame == 75);
    REQUIRE(objs[1].length == 15);
}

TEST_CASE("ObjectGenerator: sub-frame length → clamped to 1 frame", "[object_generator]")
{
    MockEditSection::getInstance()->reset();
    ObjectGenerator gen;
    GenerationConfig cfg;

    // 0.01 sec * 30 fps = 0.3 frame → ceil = 1  (但会 clamp to 1 anyway)
    // 0 sec * 30 fps = 0 → clamp to 1
    std::vector<UnifiedEvent> events = {
        makeEvent(0.0, 0.001, 0.0, 0.5, 1),   // floor(0.03) = 0 → 1
        makeEvent(1.0, 0.0, 1.0, 0.5, 2),     // ceil(0) = 0 → 1
    };

    auto result = gen.generateObjects(makeMockEditSection(), events, cfg, 30.0);

    REQUIRE(result.created == 2);
    auto& objs = MockEditSection::getInstance()->objects;
    REQUIRE(objs[0].length == 1);
    REQUIRE(objs[1].length == 1);
}

// ============================================================
// Section C: 贪心图层分配
// ============================================================

TEST_CASE("ObjectGenerator: greedy layer allocation — non-overlapping", "[object_generator]")
{
    MockEditSection::getInstance()->reset();
    ObjectGenerator gen;
    GenerationConfig cfg;
    cfg.start_layer = 1;

    // 3 个不重叠的事件 → 全部分配到同一图层
    std::vector<UnifiedEvent> events = {
        makeEvent(0.0, 1.0, 0.0, 0.8, 1),    // [0, 30)
        makeEvent(1.0, 1.0, 2.0, 0.6, 1),    // [30, 60)
        makeEvent(2.0, 1.0, 4.0, 0.5, 1),    // [60, 90)
    };

    auto result = gen.generateObjects(makeMockEditSection(), events, cfg, 30.0);

    REQUIRE(result.created == 3);
    auto& objs = MockEditSection::getInstance()->objects;
    REQUIRE(objs.size() == 3);

    // 全部分配到同一个图层（0-based layer 0）
    REQUIRE(objs[0].layer == 0);
    REQUIRE(objs[1].layer == 0);
    REQUIRE(objs[2].layer == 0);
}

TEST_CASE("ObjectGenerator: greedy layer allocation — overlapping → different layers", "[object_generator]")
{
    MockEditSection::getInstance()->reset();
    ObjectGenerator gen;
    GenerationConfig cfg;
    cfg.start_layer = 1;

    // 3 个同时开始的事件 → 各占不同图层
    std::vector<UnifiedEvent> events = {
        makeEvent(0.0, 2.0, 0.0, 0.8, 1),    // [0, 60)
        makeEvent(0.0, 2.0, 2.0, 0.6, 2),    // [0, 60) — 与上重叠，分配到新图层
        makeEvent(0.0, 2.0, 4.0, 0.5, 3),    // [0, 60) — 再分配新图层
    };

    auto result = gen.generateObjects(makeMockEditSection(), events, cfg, 30.0);

    REQUIRE(result.created == 3);
    auto& objs = MockEditSection::getInstance()->objects;

    REQUIRE(objs[0].layer == 0);
    REQUIRE(objs[1].layer == 1);
    REQUIRE(objs[2].layer == 2);
}

TEST_CASE("ObjectGenerator: greedy layer — mixed overlap/non-overlap", "[object_generator]")
{
    MockEditSection::getInstance()->reset();
    ObjectGenerator gen;
    GenerationConfig cfg;
    cfg.start_layer = 1;

    // A: [0, 60)  B: [30, 60) — B与A重叠 → B去 layer 1
    // C: [60, 90) — C不与A重叠 → C回 layer 0
    std::vector<UnifiedEvent> events = {
        makeEvent(0.0, 2.0, 0.0, 0.8, 1),    // A
        makeEvent(1.0, 1.0, 2.0, 0.6, 2),    // B
        makeEvent(2.0, 1.0, 4.0, 0.5, 1),    // C
    };

    auto result = gen.generateObjects(makeMockEditSection(), events, cfg, 30.0);

    REQUIRE(result.created == 3);
    auto& objs = MockEditSection::getInstance()->objects;

    REQUIRE(objs[0].layer == 0);  // A
    REQUIRE(objs[1].layer == 1);  // B (与A重叠)
    REQUIRE(objs[2].layer == 0);  // C (A已结束，回收layer 0)
}

TEST_CASE("ObjectGenerator: start_layer > 1 — skip lower layers", "[object_generator]")
{
    MockEditSection::getInstance()->reset();
    ObjectGenerator gen;
    GenerationConfig cfg;
    cfg.start_layer = 5;   // 从图层 5 开始（1-based → 0-based layer 4）

    std::vector<UnifiedEvent> events = {
        makeEvent(0.0, 1.0, 0.0, 0.8, 1),
    };

    auto result = gen.generateObjects(makeMockEditSection(), events, cfg, 30.0);

    REQUIRE(result.created == 1);
    auto& objs = MockEditSection::getInstance()->objects;
    REQUIRE(objs[0].layer == 4);   // 0-based layer 4 = UI layer 5
}

// ============================================================
// Section D: 物件名称
// ============================================================

TEST_CASE("ObjectGenerator: object name contains track and pitch", "[object_generator]")
{
    MockEditSection::getInstance()->reset();
    ObjectGenerator gen;
    GenerationConfig cfg;

    // pitch_shift = 0 (A4 = MIDI 69)
    std::vector<UnifiedEvent> events = {
        makeEvent(0.0, 1.0, 0.0, 0.8, 1),      // A4, track 1
        makeEvent(1.0, 1.0, -9.0, 0.6, 3),     // C4 (MIDI 60), track 3
        makeEvent(2.0, 1.0, 12.0, 0.5, 2),     // A5 (MIDI 81), track 2
    };

    auto result = gen.generateObjects(makeMockEditSection(), events, cfg, 30.0);

    REQUIRE(result.created == 3);
    auto& objs = MockEditSection::getInstance()->objects;

    REQUIRE(objs[0].name == L"Trk1 P69");   // pitch_shift 0 + 69 = 69
    REQUIRE(objs[1].name == L"Trk3 P60");   // pitch_shift -9 + 69 = 60
    REQUIRE(objs[2].name == L"Trk2 P81");   // pitch_shift 12 + 69 = 81
}

// ============================================================
// Section E: 时间范围筛选
// ============================================================

TEST_CASE("ObjectGenerator: range filter — events outside range skipped", "[object_generator]")
{
    MockEditSection::getInstance()->reset();
    ObjectGenerator gen;
    GenerationConfig cfg;
    cfg.range_start_sec = 1.0;
    cfg.range_end_sec = 3.0;

    std::vector<UnifiedEvent> events = {
        makeEvent(0.0, 1.0, 0.0, 0.8, 1),    // before range → skip
        makeEvent(1.5, 0.5, 2.0, 0.6, 2),    // inside range → create
        makeEvent(2.0, 0.5, 4.0, 0.5, 3),    // inside range → create
        makeEvent(4.0, 1.0, 0.0, 0.8, 4),    // after range → skip
    };

    auto result = gen.generateObjects(makeMockEditSection(), events, cfg, 30.0);

    REQUIRE(result.created == 2);
    REQUIRE(result.skipped == 2);
}

// ============================================================
// Section F: 物件别名
// ============================================================

TEST_CASE("ObjectGenerator: uses configured alias", "[object_generator]")
{
    MockEditSection::getInstance()->reset();
    ObjectGenerator gen;
    GenerationConfig cfg;
    cfg.alias = "MyCustomObject.object";

    std::vector<UnifiedEvent> events = {
        makeEvent(0.0, 1.0, 0.0, 0.8, 1),
    };

    auto result = gen.generateObjects(makeMockEditSection(), events, cfg, 30.0);

    REQUIRE(result.created == 1);
    auto& objs = MockEditSection::getInstance()->objects;
    REQUIRE(objs[0].alias == "MyCustomObject.object");
}

// ============================================================
// Section G: 物件创建失败
// ============================================================

TEST_CASE("ObjectGenerator: create_object_from_alias failure → counted as skipped", "[object_generator]")
{
    MockEditSection::getInstance()->reset();
    MockEditSection::getInstance()->simulateFail = true;   // 模拟所有创建失败

    ObjectGenerator gen;
    GenerationConfig cfg;
    std::vector<UnifiedEvent> events = {
        makeEvent(0.0, 1.0, 0.0, 0.8, 1),
        makeEvent(1.0, 1.0, 2.0, 0.6, 2),
    };

    auto result = gen.generateObjects(makeMockEditSection(), events, cfg, 30.0);

    REQUIRE(result.created == 0);
    REQUIRE(result.skipped == 2);
}

// ============================================================
// Section H: 不同帧率
// ============================================================

TEST_CASE("ObjectGenerator: different fps yields correct frame numbers", "[object_generator]")
{
    MockEditSection::getInstance()->reset();
    ObjectGenerator gen;
    GenerationConfig cfg;

    // 24 fps: floor(1.0 * 24) = 24, ceil(0.5 * 24) = 12
    std::vector<UnifiedEvent> events = {
        makeEvent(1.0, 0.5, 0.0, 0.8, 1),
    };

    auto result = gen.generateObjects(makeMockEditSection(), events, cfg, 24.0);

    REQUIRE(result.created == 1);
    auto& objs = MockEditSection::getInstance()->objects;
    REQUIRE(objs[0].frame == 24);
    REQUIRE(objs[0].length == 12);
}

// ============================================================
// Section I: 最大可见物件数限制
// ============================================================

TEST_CASE("ObjectGenerator: max_visible_count=0 → no limit (default)", "[object_generator]")
{
    MockEditSection::getInstance()->reset();
    ObjectGenerator gen;
    GenerationConfig cfg;
    cfg.max_visible_count = 0;  // 无限制

    std::vector<UnifiedEvent> events = {
        makeEvent(0.0, 1.0, 0.0, 0.8, 1),
        makeEvent(1.0, 1.0, 2.0, 0.6, 2),
        makeEvent(2.0, 1.0, 4.0, 0.5, 3),
    };

    auto result = gen.generateObjects(makeMockEditSection(), events, cfg, 30.0);

    // 所有事件都被创建
    REQUIRE(result.created == 3);
    REQUIRE(result.skipped == 0);
}

TEST_CASE("ObjectGenerator: max_visible_count limits created objects", "[object_generator]")
{
    MockEditSection::getInstance()->reset();
    ObjectGenerator gen;
    GenerationConfig cfg;
    cfg.max_visible_count = 2;  // 最多 2 个物件

    std::vector<UnifiedEvent> events = {
        makeEvent(0.0, 1.0, 0.0, 0.8, 1),
        makeEvent(1.0, 1.0, 2.0, 0.6, 2),
        makeEvent(2.0, 1.0, 4.0, 0.5, 3),
        makeEvent(3.0, 1.0, 5.0, 0.4, 4),
    };

    auto result = gen.generateObjects(makeMockEditSection(), events, cfg, 30.0);

    // 只有前 2 个被创建，剩余 2 个被跳过
    REQUIRE(result.created == 2);
    REQUIRE(result.skipped == 2);

    auto& objs = MockEditSection::getInstance()->objects;
    REQUIRE(objs.size() == 2);
    REQUIRE(objs[0].frame == 0);
    REQUIRE(objs[1].frame == 30);
}

TEST_CASE("ObjectGenerator: max_visible_count=1 → only first event created", "[object_generator]")
{
    MockEditSection::getInstance()->reset();
    ObjectGenerator gen;
    GenerationConfig cfg;
    cfg.max_visible_count = 1;

    std::vector<UnifiedEvent> events = {
        makeEvent(0.0, 1.0, 0.0, 0.8, 1),
        makeEvent(1.0, 1.0, 2.0, 0.6, 2),
        makeEvent(2.0, 1.0, 4.0, 0.5, 3),
    };

    auto result = gen.generateObjects(makeMockEditSection(), events, cfg, 30.0);

    REQUIRE(result.created == 1);
    REQUIRE(result.skipped == 2);

    auto& objs = MockEditSection::getInstance()->objects;
    REQUIRE(objs.size() == 1);
    REQUIRE(objs[0].frame == 0);
}

// ============================================================
// Section J: 最大生存时间限制
// ============================================================

TEST_CASE("ObjectGenerator: max_lifetime_sec=0 → no limit (default)", "[object_generator]")
{
    MockEditSection::getInstance()->reset();
    ObjectGenerator gen;
    GenerationConfig cfg;
    cfg.max_lifetime_sec = 0.0;  // 无限制

    std::vector<UnifiedEvent> events = {
        makeEvent(0.0, 2.0, 0.0, 0.8, 1),   // length_sec=2.0, 30fps → length=60
    };

    auto result = gen.generateObjects(makeMockEditSection(), events, cfg, 30.0);

    REQUIRE(result.created == 1);
    auto& objs = MockEditSection::getInstance()->objects;
    REQUIRE(objs[0].length == 60);  // ceil(2.0 * 30) = 60
}

TEST_CASE("ObjectGenerator: max_lifetime_sec clamps object length", "[object_generator]")
{
    MockEditSection::getInstance()->reset();
    ObjectGenerator gen;
    GenerationConfig cfg;
    cfg.max_lifetime_sec = 0.5;  // 最多存活 0.5 秒

    std::vector<UnifiedEvent> events = {
        makeEvent(0.0, 2.0, 0.0, 0.8, 1),   // length_sec=2.0 → clamp to 0.5 → 15 frames
    };

    auto result = gen.generateObjects(makeMockEditSection(), events, cfg, 30.0);

    REQUIRE(result.created == 1);
    auto& objs = MockEditSection::getInstance()->objects;
    REQUIRE(objs[0].length == 15);  // ceil(0.5 * 30) = 15
}

TEST_CASE("ObjectGenerator: max_lifetime_sec larger than event length → no change", "[object_generator]")
{
    MockEditSection::getInstance()->reset();
    ObjectGenerator gen;
    GenerationConfig cfg;
    cfg.max_lifetime_sec = 5.0;  // 限制比事件本身长，不影响

    std::vector<UnifiedEvent> events = {
        makeEvent(0.0, 1.0, 0.0, 0.8, 1),   // length_sec=1.0 < 5.0 → untouched
    };

    auto result = gen.generateObjects(makeMockEditSection(), events, cfg, 30.0);

    REQUIRE(result.created == 1);
    auto& objs = MockEditSection::getInstance()->objects;
    REQUIRE(objs[0].length == 30);  // ceil(1.0 * 30) = 30
}

TEST_CASE("ObjectGenerator: max_lifetime_sec with multiple events", "[object_generator]")
{
    MockEditSection::getInstance()->reset();
    ObjectGenerator gen;
    GenerationConfig cfg;
    cfg.max_lifetime_sec = 0.5;

    std::vector<UnifiedEvent> events = {
        makeEvent(0.0, 2.0, 0.0, 0.8, 1),   // clamped to 0.5s → 15 frames
        makeEvent(1.0, 0.3, 2.0, 0.6, 2),   // 0.3s < 0.5s → 9 frames (unchanged)
    };

    auto result = gen.generateObjects(makeMockEditSection(), events, cfg, 30.0);

    REQUIRE(result.created == 2);
    auto& objs = MockEditSection::getInstance()->objects;
    REQUIRE(objs[0].length == 15);  // clamped
    REQUIRE(objs[1].length == 9);   // ceil(0.3 * 30) = 9
}

// ============================================================
// Section K: 组合约束
// ============================================================

TEST_CASE("ObjectGenerator: max_visible_count + max_lifetime_sec together", "[object_generator]")
{
    MockEditSection::getInstance()->reset();
    ObjectGenerator gen;
    GenerationConfig cfg;
    cfg.max_visible_count = 2;
    cfg.max_lifetime_sec = 0.5;

    std::vector<UnifiedEvent> events = {
        makeEvent(0.0, 2.0, 0.0, 0.8, 1),   // created, clamped to 0.5s → 15 frames
        makeEvent(1.0, 2.0, 2.0, 0.6, 2),   // created, clamped to 0.5s → 15 frames
        makeEvent(2.0, 2.0, 4.0, 0.5, 3),   // skipped (max_visible_count reached)
        makeEvent(3.0, 2.0, 5.0, 0.4, 4),   // skipped (max_visible_count reached)
    };

    auto result = gen.generateObjects(makeMockEditSection(), events, cfg, 30.0);

    REQUIRE(result.created == 2);
    REQUIRE(result.skipped == 2);

    auto& objs = MockEditSection::getInstance()->objects;
    REQUIRE(objs.size() == 2);
    REQUIRE(objs[0].length == 15);
    REQUIRE(objs[1].length == 15);
}

// ============================================================


// ============================================================
// Section M: 翻转模式 (flip_mode)
// ============================================================

TEST_CASE("ObjectGenerator: computeFlipState mode 0 — no flip", "[object_generator][flip]")
{
    // Mode 0: no flip for any index
    for (int i = 0; i < 8; i++)
    {
        auto state = ObjectGenerator::computeFlipState(i, 0);
        REQUIRE(state.horizontal == false);
        REQUIRE(state.vertical == false);
    }
}

TEST_CASE("ObjectGenerator: computeFlipState mode 1 — left-right alternate", "[object_generator][flip]")
{
    // Mode 1: odd index -> horizontal
    REQUIRE(ObjectGenerator::computeFlipState(0, 1).horizontal == false);
    REQUIRE(ObjectGenerator::computeFlipState(1, 1).horizontal == true);
    REQUIRE(ObjectGenerator::computeFlipState(2, 1).horizontal == false);
    REQUIRE(ObjectGenerator::computeFlipState(3, 1).horizontal == true);
    // vertical always false
    REQUIRE(ObjectGenerator::computeFlipState(0, 1).vertical == false);
    REQUIRE(ObjectGenerator::computeFlipState(1, 1).vertical == false);
}

TEST_CASE("ObjectGenerator: computeFlipState mode 2 — up-down alternate", "[object_generator][flip]")
{
    // Mode 2: even index -> vertical
    REQUIRE(ObjectGenerator::computeFlipState(0, 2).vertical == true);
    REQUIRE(ObjectGenerator::computeFlipState(1, 2).vertical == false);
    REQUIRE(ObjectGenerator::computeFlipState(2, 2).vertical == true);
    REQUIRE(ObjectGenerator::computeFlipState(3, 2).vertical == false);
    // horizontal always false
    REQUIRE(ObjectGenerator::computeFlipState(0, 2).horizontal == false);
    REQUIRE(ObjectGenerator::computeFlipState(1, 2).horizontal == false);
}

TEST_CASE("ObjectGenerator: computeFlipState mode 3 — clockwise 4-step", "[object_generator][flip]")
{
    // Cycle: [0, H, H+V, V]
    REQUIRE(ObjectGenerator::computeFlipState(0, 3).horizontal == false);
    REQUIRE(ObjectGenerator::computeFlipState(0, 3).vertical == false);
    REQUIRE(ObjectGenerator::computeFlipState(1, 3).horizontal == true);
    REQUIRE(ObjectGenerator::computeFlipState(1, 3).vertical == false);
    REQUIRE(ObjectGenerator::computeFlipState(2, 3).horizontal == true);
    REQUIRE(ObjectGenerator::computeFlipState(2, 3).vertical == true);
    REQUIRE(ObjectGenerator::computeFlipState(3, 3).horizontal == false);
    REQUIRE(ObjectGenerator::computeFlipState(3, 3).vertical == true);
    REQUIRE(ObjectGenerator::computeFlipState(4, 3).horizontal == false);
    REQUIRE(ObjectGenerator::computeFlipState(4, 3).vertical == false);
    REQUIRE(ObjectGenerator::computeFlipState(5, 3).horizontal == true);
    REQUIRE(ObjectGenerator::computeFlipState(5, 3).vertical == false);
}

TEST_CASE("ObjectGenerator: computeFlipState mode 4 — counter-clockwise 4-step", "[object_generator][flip]")
{
    // Cycle: [0, V, H+V, H]
    REQUIRE(ObjectGenerator::computeFlipState(0, 4).horizontal == false);
    REQUIRE(ObjectGenerator::computeFlipState(0, 4).vertical == false);
    REQUIRE(ObjectGenerator::computeFlipState(1, 4).horizontal == false);
    REQUIRE(ObjectGenerator::computeFlipState(1, 4).vertical == true);
    REQUIRE(ObjectGenerator::computeFlipState(2, 4).horizontal == true);
    REQUIRE(ObjectGenerator::computeFlipState(2, 4).vertical == true);
    REQUIRE(ObjectGenerator::computeFlipState(3, 4).horizontal == true);
    REQUIRE(ObjectGenerator::computeFlipState(3, 4).vertical == false);
    REQUIRE(ObjectGenerator::computeFlipState(4, 4).horizontal == false);
    REQUIRE(ObjectGenerator::computeFlipState(4, 4).vertical == false);
}

TEST_CASE("ObjectGenerator: flip_mode=0 — no suffix in name", "[object_generator][flip]")
{
    MockEditSection::getInstance()->reset();
    ObjectGenerator gen;
    GenerationConfig cfg;
    cfg.flip_mode = 0;

    std::vector<UnifiedEvent> events = {
        makeEvent(0.0, 1.0, 0.0, 0.8, 1),
        makeEvent(1.0, 1.0, 2.0, 0.6, 2),
    };

    auto result = gen.generateObjects(makeMockEditSection(), events, cfg, 30.0);

    REQUIRE(result.created == 2);
    auto& objs = MockEditSection::getInstance()->objects;
    REQUIRE(objs[0].name == L"Trk1 P69");
    REQUIRE(objs[1].name == L"Trk2 P71");
}

TEST_CASE("ObjectGenerator: flip_mode=1 — odd index gets _H suffix", "[object_generator][flip]")
{
    MockEditSection::getInstance()->reset();
    ObjectGenerator gen;
    GenerationConfig cfg;
    cfg.flip_mode = 1;

    std::vector<UnifiedEvent> events = {
        makeEvent(0.0, 1.0, 0.0, 0.8, 1),
        makeEvent(1.0, 1.0, 2.0, 0.6, 1),
        makeEvent(2.0, 1.0, 4.0, 0.5, 1),
    };

    auto result = gen.generateObjects(makeMockEditSection(), events, cfg, 30.0);

    REQUIRE(result.created == 3);
    auto& objs = MockEditSection::getInstance()->objects;
    REQUIRE(objs[0].name == L"Trk1 P69");
    REQUIRE(objs[1].name == L"Trk1 P71_H");
    REQUIRE(objs[2].name == L"Trk1 P73");
}

TEST_CASE("ObjectGenerator: flip_mode=2 — even index gets _V suffix", "[object_generator][flip]")
{
    MockEditSection::getInstance()->reset();
    ObjectGenerator gen;
    GenerationConfig cfg;
    cfg.flip_mode = 2;

    std::vector<UnifiedEvent> events = {
        makeEvent(0.0, 1.0, 0.0, 0.8, 1),
        makeEvent(1.0, 1.0, 2.0, 0.6, 1),
        makeEvent(2.0, 1.0, 4.0, 0.5, 1),
    };

    auto result = gen.generateObjects(makeMockEditSection(), events, cfg, 30.0);

    REQUIRE(result.created == 3);
    auto& objs = MockEditSection::getInstance()->objects;
    REQUIRE(objs[0].name == L"Trk1 P69_V");
    REQUIRE(objs[1].name == L"Trk1 P71");
    REQUIRE(objs[2].name == L"Trk1 P73_V");
}

TEST_CASE("ObjectGenerator: flip_mode=3 — clockwise 4-step suffix", "[object_generator][flip]")
{
    MockEditSection::getInstance()->reset();
    ObjectGenerator gen;
    GenerationConfig cfg;
    cfg.flip_mode = 3;

    std::vector<UnifiedEvent> events = {
        makeEvent(0.0, 1.0, 0.0, 0.8, 1),
        makeEvent(1.0, 1.0, 2.0, 0.6, 1),
        makeEvent(2.0, 1.0, 4.0, 0.5, 1),
        makeEvent(3.0, 1.0, 5.0, 0.4, 1),
        makeEvent(4.0, 1.0, 7.0, 0.3, 1),
    };

    auto result = gen.generateObjects(makeMockEditSection(), events, cfg, 30.0);

    REQUIRE(result.created == 5);
    auto& objs = MockEditSection::getInstance()->objects;
    REQUIRE(objs[0].name == L"Trk1 P69");
    REQUIRE(objs[1].name == L"Trk1 P71_H");
    REQUIRE(objs[2].name == L"Trk1 P73_HV");
    REQUIRE(objs[3].name == L"Trk1 P74_V");
    REQUIRE(objs[4].name == L"Trk1 P76");
}

TEST_CASE("ObjectGenerator: flip_mode=4 — counter-clockwise 4-step suffix", "[object_generator][flip]")
{
    MockEditSection::getInstance()->reset();
    ObjectGenerator gen;
    GenerationConfig cfg;
    cfg.flip_mode = 4;

    std::vector<UnifiedEvent> events = {
        makeEvent(0.0, 1.0, 0.0, 0.8, 1),
        makeEvent(1.0, 1.0, 2.0, 0.6, 1),
        makeEvent(2.0, 1.0, 4.0, 0.5, 1),
        makeEvent(3.0, 1.0, 5.0, 0.4, 1),
        makeEvent(4.0, 1.0, 7.0, 0.3, 1),
    };

    auto result = gen.generateObjects(makeMockEditSection(), events, cfg, 30.0);

    REQUIRE(result.created == 5);
    auto& objs = MockEditSection::getInstance()->objects;
    REQUIRE(objs[0].name == L"Trk1 P69");
    REQUIRE(objs[1].name == L"Trk1 P71_V");
    REQUIRE(objs[2].name == L"Trk1 P73_HV");
    REQUIRE(objs[3].name == L"Trk1 P74_H");
    REQUIRE(objs[4].name == L"Trk1 P76");
}

TEST_CASE("ObjectGenerator: flip_mode with skipped events — itemIndex advances correctly", "[object_generator][flip]")
{
    MockEditSection::getInstance()->reset();
    ObjectGenerator gen;
    GenerationConfig cfg;
    cfg.flip_mode = 1;
    cfg.range_start_sec = 0.5;
    cfg.range_end_sec = 100.0;

    std::vector<UnifiedEvent> events = {
        makeEvent(0.0, 1.0, 0.0, 0.8, 1),
        makeEvent(1.0, 1.0, 2.0, 0.6, 1),
        makeEvent(2.0, 1.0, 4.0, 0.5, 1),
    };

    auto result = gen.generateObjects(makeMockEditSection(), events, cfg, 30.0);

    REQUIRE(result.created == 2);
    auto& objs = MockEditSection::getInstance()->objects;
    REQUIRE(objs[0].name == L"Trk1 P71");
    REQUIRE(objs[1].name == L"Trk1 P73_H");
}

TEST_CASE("ObjectGenerator: flip_mode with freeze_state — both in name", "[object_generator][flip]")
{
    MockEditSection::getInstance()->reset();
    ObjectGenerator gen;
    GenerationConfig cfg;
    cfg.flip_mode = 1;
    cfg.freeze_state = true;

    std::vector<UnifiedEvent> events = {
        makeEvent(0.0, 1.0, 0.0, 0.8, 1),
        makeEvent(1.0, 1.0, 2.0, 0.6, 1),
    };

    auto result = gen.generateObjects(makeMockEditSection(), events, cfg, 30.0);

    REQUIRE(result.created == 2);
    auto& objs = MockEditSection::getInstance()->objects;
    REQUIRE(objs[0].name == L"Trk1 P69 [F:0.0,0.0,0.0,100.0]");
    REQUIRE(objs[1].name == L"Trk1 P71_H [F:0.0,0.0,0.0,100.0]");
}

// Section L: 冻结状态 (freeze_state)
// ============================================================

TEST_CASE("ObjectGenerator: freeze_state=false → no freeze data in name (default)", "[object_generator]")
{
    MockEditSection::getInstance()->reset();
    ObjectGenerator gen;
    GenerationConfig cfg;
    cfg.freeze_state = false;  // 默认值

    std::vector<UnifiedEvent> events = {
        makeEvent(0.0, 1.0, 0.0, 0.8, 1),
    };

    auto result = gen.generateObjects(makeMockEditSection(), events, cfg, 30.0);

    REQUIRE(result.created == 1);
    auto& objs = MockEditSection::getInstance()->objects;
    REQUIRE(objs[0].name == L"Trk1 P69");  // 无冻结数据
}

TEST_CASE("ObjectGenerator: freeze_state=true → freeze data appended to name", "[object_generator]")
{
    MockEditSection::getInstance()->reset();
    ObjectGenerator gen;
    GenerationConfig cfg;
    cfg.freeze_state = true;

    std::vector<UnifiedEvent> events = {
        makeEvent(0.0, 1.0, 0.0, 0.8, 1),
    };

    auto result = gen.generateObjects(makeMockEditSection(), events, cfg, 30.0);

    REQUIRE(result.created == 1);
    auto& objs = MockEditSection::getInstance()->objects;
    // itemIndex=0: scale=pOw(100/100,0)*100=100.0, rotation=0, offsetX=0, offsetY=0
    REQUIRE(objs[0].name == L"Trk1 P69 [F:0.0,0.0,0.0,100.0]");
}

TEST_CASE("ObjectGenerator: freeze_state with non-default step values", "[object_generator]")
{
    MockEditSection::getInstance()->reset();
    ObjectGenerator gen;
    GenerationConfig cfg;
    cfg.freeze_state = true;
    cfg.step_scale = 80.0;       // 每次缩放 80%
    cfg.step_rotation = 5.0;     // 每次旋转 5度
    cfg.step_offset_x = 10.0;    // 每次 X 偏移 10px
    cfg.step_offset_y = -3.0;    // 每次 Y 偏移 -3px

    std::vector<UnifiedEvent> events = {
        makeEvent(0.0, 1.0, 0.0, 0.8, 1),   // itemIndex=0
        makeEvent(1.0, 1.0, 2.0, 0.6, 1),   // itemIndex=1
        makeEvent(2.0, 1.0, 4.0, 0.5, 1),   // itemIndex=2
    };

    auto result = gen.generateObjects(makeMockEditSection(), events, cfg, 30.0);

    REQUIRE(result.created == 3);
    auto& objs = MockEditSection::getInstance()->objects;

    // itemIndex=0: scale=pow(0.8,0)*100=100.0, rotation=0, offsetX=0, offsetY=0
    REQUIRE(objs[0].name == L"Trk1 P69 [F:0.0,-0.0,0.0,100.0]");

    // itemIndex=1: scale=pow(0.8,1)*100=80.0, rotation=5, offsetX=10, offsetY=-3
    REQUIRE(objs[1].name == L"Trk1 P71 [F:10.0,-3.0,5.0,80.0]");

    // itemIndex=2: scale=pow(0.8,2)*100=64.0, rotation=10, offsetX=20, offsetY=-6
    REQUIRE(objs[2].name == L"Trk1 P73 [F:20.0,-6.0,10.0,64.0]");
}

TEST_CASE("ObjectGenerator: freeze_state with range filter — skip does not count as itemIndex", "[object_generator]")
{
    MockEditSection::getInstance()->reset();
    ObjectGenerator gen;
    GenerationConfig cfg;
    cfg.freeze_state = true;
    cfg.range_start_sec = 0.0;
    cfg.range_end_sec = 2.5;

    std::vector<UnifiedEvent> events = {
        makeEvent(0.0, 1.0, 0.0, 0.8, 1),    // created → itemIndex=0
        makeEvent(3.0, 1.0, 2.0, 0.6, 2),    // skipped (out of range)
        makeEvent(1.0, 1.0, 4.0, 0.5, 3),    // created → itemIndex=1
    };

    auto result = gen.generateObjects(makeMockEditSection(), events, cfg, 30.0);

    REQUIRE(result.created == 2);
    REQUIRE(result.skipped == 1);
    auto& objs = MockEditSection::getInstance()->objects;

    // itemIndex=0: default values
    REQUIRE(objs[0].name == L"Trk1 P69 [F:0.0,0.0,0.0,100.0]");

    // itemIndex=1: default step applied once
    REQUIRE(objs[1].name == L"Trk3 P73 [F:0.0,0.0,0.0,100.0]");
}

TEST_CASE("ObjectGenerator: freeze_state does not affect object position or length", "[object_generator]")
{
    MockEditSection::getInstance()->reset();
    ObjectGenerator gen;
    GenerationConfig cfg;
    cfg.freeze_state = true;

    std::vector<UnifiedEvent> events = {
        makeEvent(2.0, 1.5, 0.0, 0.8, 1),
    };

    auto result = gen.generateObjects(makeMockEditSection(), events, cfg, 30.0);

    REQUIRE(result.created == 1);
    auto& objs = MockEditSection::getInstance()->objects;

    // freeze 不影响位置和长度
    REQUIRE(objs[0].frame == 60);   // floor(2.0 * 30)
    REQUIRE(objs[0].length == 45);  // ceil(1.5 * 30)
}


// ============================================================
// Section M: CumulativeTransform compute verification
// ============================================================

TEST_CASE("CumulativeTransform: compute returns correct default values", "[cumulative_transform]")
{
    GenerationConfig cfg;  // defaults: step_scale=100, step_rotation=0, step_offset_x=0, step_offset_y=0

    TransformValues tv0 = CumulativeTransform::compute(0, cfg);
    REQUIRE(tv0.scale == 100.0);
    REQUIRE(tv0.rotation == 0.0);
    REQUIRE(tv0.offsetX == 0.0);
    REQUIRE(tv0.offsetY == 0.0);

    // itemIndex=1 with all defaults
    TransformValues tv1 = CumulativeTransform::compute(1, cfg);
    REQUIRE(tv1.scale == 100.0);    // pow(100/100, 1) * 100 = 100
    REQUIRE(tv1.rotation == 0.0);
    REQUIRE(tv1.offsetX == 0.0);
    REQUIRE(tv1.offsetY == 0.0);
}

TEST_CASE("CumulativeTransform: compute with non-default steps", "[cumulative_transform]")
{
    GenerationConfig cfg;
    cfg.step_scale = 80.0;
    cfg.step_rotation = 5.0;
    cfg.step_offset_x = 10.0;
    cfg.step_offset_y = -3.0;

    // itemIndex=0: all identity
    TransformValues tv0 = CumulativeTransform::compute(0, cfg);
    REQUIRE(tv0.scale == 100.0);
    REQUIRE(tv0.rotation == 0.0);
    REQUIRE(tv0.offsetX == 0.0);
    REQUIRE(tv0.offsetY == 0.0);

    // itemIndex=1: step applied once
    TransformValues tv1 = CumulativeTransform::compute(1, cfg);
    REQUIRE(tv1.scale == 80.0);         // pow(0.8, 1) * 100 = 80
    REQUIRE(tv1.rotation == 5.0);
    REQUIRE(tv1.offsetX == 10.0);
    REQUIRE(tv1.offsetY == -3.0);

    // itemIndex=2: step applied twice
    TransformValues tv2 = CumulativeTransform::compute(2, cfg);
    REQUIRE(tv2.scale == Approx(64.0));         // pow(0.8, 2) * 100 = 64
    REQUIRE(tv2.rotation == 10.0);
    REQUIRE(tv2.offsetX == 20.0);
    REQUIRE(tv2.offsetY == -6.0);
}
