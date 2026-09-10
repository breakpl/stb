#include <catch2/catch_test_macros.hpp>
#include "JsonFormatterUtils.h"

// ── Format ────────────────────────────────────────────────────────────────────

TEST_CASE("Format empty string is valid and returns empty", "[json]") {
    bool valid = false;
    REQUIRE(JsonFormatterUtils::Format("", &valid) == "");
    REQUIRE(valid);
}

TEST_CASE("Format simple object", "[json]") {
    bool valid = false;
    std::string result = JsonFormatterUtils::Format(R"({"a":1})", &valid);
    REQUIRE(valid);
    REQUIRE(result == "{\n  \"a\": 1\n}");
}

TEST_CASE("Format simple array", "[json]") {
    bool valid = false;
    std::string result = JsonFormatterUtils::Format("[1,2,3]", &valid);
    REQUIRE(valid);
    REQUIRE(result == "[\n  1,\n  2,\n  3\n]");
}

TEST_CASE("Format nested object", "[json]") {
    bool valid = false;
    std::string result = JsonFormatterUtils::Format(R"({"a":{"b":1}})", &valid);
    REQUIRE(valid);
    REQUIRE(result == "{\n  \"a\": {\n    \"b\": 1\n  }\n}");
}

TEST_CASE("Format strips whitespace outside strings", "[json]") {
    bool valid1 = false, valid2 = false;
    std::string compact = JsonFormatterUtils::Format(R"({"a":1})", &valid1);
    std::string spaced  = JsonFormatterUtils::Format(R"({ "a" : 1 })", &valid2);
    REQUIRE(valid1); REQUIRE(valid2);
    REQUIRE(compact == spaced);
}

TEST_CASE("Format preserves commas and colons inside string values", "[json]") {
    bool valid = false;
    std::string result = JsonFormatterUtils::Format(R"({"k":"v with , : { }"})", &valid);
    REQUIRE(valid);
    REQUIRE(result.find("v with , : { }") != std::string::npos);
}

TEST_CASE("Format handles escaped quote inside string", "[json]") {
    bool valid = false;
    std::string result = JsonFormatterUtils::Format(R"({"k":"say \"hi\""})", &valid);
    REQUIRE(valid);
    REQUIRE(result.find(R"(say \"hi\")") != std::string::npos);
}

TEST_CASE("Format unclosed string marks result invalid", "[json]") {
    bool valid = true;
    JsonFormatterUtils::Format(R"({"key":"unclosed)", &valid);
    REQUIRE_FALSE(valid);
}

TEST_CASE("Format extra closing brace marks result invalid", "[json]") {
    bool valid = true;
    JsonFormatterUtils::Format("{}}", &valid);
    REQUIRE_FALSE(valid);
}

TEST_CASE("Format unclosed object marks result invalid", "[json]") {
    bool valid = true;
    JsonFormatterUtils::Format("{", &valid);
    REQUIRE_FALSE(valid);
}

TEST_CASE("Format works without valid output parameter", "[json]") {
    REQUIRE_NOTHROW(JsonFormatterUtils::Format(R"({"a":1})"));
}

// ── Minify ────────────────────────────────────────────────────────────────────

TEST_CASE("Minify empty string returns empty", "[json]") {
    REQUIRE(JsonFormatterUtils::Minify("") == "");
}

TEST_CASE("Minify removes whitespace outside strings", "[json]") {
    REQUIRE(JsonFormatterUtils::Minify("{\n  \"a\": 1\n}") == "{\"a\":1}");
}

TEST_CASE("Minify preserves whitespace inside string values", "[json]") {
    REQUIRE(JsonFormatterUtils::Minify(R"({ "key" : "val with spaces" })") ==
            R"({"key":"val with spaces"})");
}

TEST_CASE("Minify preserves escaped backslash in string", "[json]") {
    REQUIRE(JsonFormatterUtils::Minify(R"({ "k" : "a\\b" })") == R"({"k":"a\\b"})");
}

// ── Roundtrip ─────────────────────────────────────────────────────────────────

TEST_CASE("Minify(Format(s)) == Minify(s) for valid JSON", "[json]") {
    std::string original = R"({"a":1,"b":[2,3],"c":{"d":"e"}})";
    bool valid = false;
    std::string formatted = JsonFormatterUtils::Format(original, &valid);
    REQUIRE(valid);
    REQUIRE(JsonFormatterUtils::Minify(formatted) == original);
}
