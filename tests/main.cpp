// AutoZYC — Catch2 test runner (single translation unit)
#define CATCH_CONFIG_MAIN
#include <windows.h>
#include "catch.hpp"

// Include test sources directly to avoid separate TU linking issues
// with MinGW + Catch2 v2
#include "test_event_types.cpp"
#include "test_expression.cpp"
#include "test_midi_parser.cpp"
#include "test_rpp_parser.cpp"
#include "test_parser_regression.cpp"
#include "test_data_manager.cpp"
#include "test_object_generator.cpp"
#include "test_cumulative.cpp"
#include "test_e2e_pipeline.cpp"
