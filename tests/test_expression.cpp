// AutoZYC — Expression engine tests
// Included by main.cpp (catch.hpp already available)
#include "src/core/ExpressionEngine.h"
#include <cstring>

TEST_CASE("Variable substitution: pitch", "[expression]") {
    ExpressionEngine engine;

    std::string result = engine.preprocess("pitch * 2", 12.0, 0.0, 0.0, 0.0, 0.0);
    REQUIRE(result.find("pitch") == std::string::npos);
    REQUIRE(result.find("12") != std::string::npos);
    REQUIRE(result.find(" * 2") != std::string::npos);
}

TEST_CASE("Word boundary check", "[expression]") {
    ExpressionEngine engine;

    std::string result1 = engine.preprocess("pitch * 2", 7.0, 0.0, 0.0, 0.0, 0.0);
    REQUIRE(result1.find("pitch") == std::string::npos);

    std::string result2 = engine.preprocess("pitchfork", 7.0, 0.0, 0.0, 0.0, 0.0);
    REQUIRE(result2 == "pitchfork");
}

TEST_CASE("Empty expression", "[expression]") {
    ExpressionEngine engine;

    double result = engine.evaluate("", 0.0, 0.0, 0.0, 0.0, 0.0);
    REQUIRE(result == 0.0);
}

TEST_CASE("Numeric precision", "[expression]") {
    ExpressionEngine engine;

    std::string result = engine.preprocess("time", 0.0, 0.0, 12345.6789012345, 0.0, 0.0);
    REQUIRE(result.find("12345") != std::string::npos);
    REQUIRE(result.find(".") != std::string::npos);
    REQUIRE_FALSE(result.empty());

    result = engine.preprocess("time", 0.0, 0.0, 0.001, 0.0, 0.0);
    REQUIRE(result.find("0.001") != std::string::npos);
}
