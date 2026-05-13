// AutoZYC — Cumulative transform unit tests
// Included by main.cpp (catch.hpp already available)
#include "src/generate/CumulativeTransform.h"
#include <cmath>

TEST_CASE("CumulativeTransform itemIndex=0 returns identity values", "[cumulative]") {
    GenerationConfig config;
    // 使用默认值：step_scale=100, step_rotation=0, step_offset=0

    TransformValues v = CumulativeTransform::compute(0, config);

    REQUIRE(v.scale == Approx(100.0));
    REQUIRE(v.rotation == Approx(0.0));
    REQUIRE(v.offsetX == Approx(0.0));
    REQUIRE(v.offsetY == Approx(0.0));
}

TEST_CASE("CumulativeTransform step_scale=110 itemIndex=3 yields 133.1%", "[cumulative]") {
    GenerationConfig config;
    config.step_scale = 110.0;

    TransformValues v = CumulativeTransform::compute(3, config);

    // 1.10^3 = 1.331 → 133.1%
    REQUIRE(v.scale == Approx(133.1));
    REQUIRE(v.rotation == Approx(0.0));
    REQUIRE(v.offsetX == Approx(0.0));
    REQUIRE(v.offsetY == Approx(0.0));
}

TEST_CASE("CumulativeTransform step_rotation=15 itemIndex=4 yields 60 degrees", "[cumulative]") {
    GenerationConfig config;
    config.step_rotation = 15.0;

    TransformValues v = CumulativeTransform::compute(4, config);

    REQUIRE(v.scale == Approx(100.0));
    REQUIRE(v.rotation == Approx(60.0));
    REQUIRE(v.offsetX == Approx(0.0));
    REQUIRE(v.offsetY == Approx(0.0));
}

TEST_CASE("CumulativeTransform step_offset=10 itemIndex=5 yields 50", "[cumulative]") {
    GenerationConfig config;
    config.step_offset_x = 10.0;
    config.step_offset_y = 10.0;

    TransformValues v = CumulativeTransform::compute(5, config);

    REQUIRE(v.scale == Approx(100.0));
    REQUIRE(v.rotation == Approx(0.0));
    REQUIRE(v.offsetX == Approx(50.0));
    REQUIRE(v.offsetY == Approx(50.0));
}

TEST_CASE("CumulativeTransform all transforms combined itemIndex=2", "[cumulative]") {
    GenerationConfig config;
    config.step_scale = 150.0;
    config.step_rotation = 10.0;
    config.step_offset_x = 20.0;
    config.step_offset_y = -5.0;

    TransformValues v = CumulativeTransform::compute(2, config);

    // 1.50^2 = 2.25 → 225.0%
    REQUIRE(v.scale == Approx(225.0));
    // 10 * 2 = 20
    REQUIRE(v.rotation == Approx(20.0));
    // 20 * 2 = 40
    REQUIRE(v.offsetX == Approx(40.0));
    // -5 * 2 = -10
    REQUIRE(v.offsetY == Approx(-10.0));
}

TEST_CASE("CumulativeTransform itemIndex=1 equals step values", "[cumulative]") {
    GenerationConfig config;
    config.step_scale = 120.0;
    config.step_rotation = 30.0;
    config.step_offset_x = 15.0;
    config.step_offset_y = 25.0;

    TransformValues v = CumulativeTransform::compute(1, config);

    // itemIndex=1 → values should equal step values directly
    REQUIRE(v.scale == Approx(120.0));
    REQUIRE(v.rotation == Approx(30.0));
    REQUIRE(v.offsetX == Approx(15.0));
    REQUIRE(v.offsetY == Approx(25.0));
}

TEST_CASE("CumulativeTransform negative itemIndex should work mathematically", "[cumulative]") {
    GenerationConfig config;
    config.step_scale = 200.0;
    config.step_rotation = 90.0;
    config.step_offset_x = 100.0;
    config.step_offset_y = 50.0;

    // 当 index 为负时，pow 和乘法应该仍然有效
    TransformValues v = CumulativeTransform::compute(-1, config);

    // 2.0^(-1) = 0.5 → 50%
    REQUIRE(v.scale == Approx(50.0));
    // 90 * (-1) = -90
    REQUIRE(v.rotation == Approx(-90.0));
    // 100 * (-1) = -100
    REQUIRE(v.offsetX == Approx(-100.0));
    // 50 * (-1) = -50
    REQUIRE(v.offsetY == Approx(-50.0));
}
